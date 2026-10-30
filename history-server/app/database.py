from contextlib import contextmanager
import fcntl
from pathlib import Path
import sqlite3

from .models import Reading

SCHEMA_VERSION = 1
COLUMNS = "timestamp_utc, observed_at_utc, t0, t1, t2, s0, s1, s2, result, detail"


@contextmanager
def connect(path: Path, *, readonly=False):
    db = sqlite3.connect(path.resolve().as_uri() + "?mode=ro", uri=True, timeout=30) if readonly else sqlite3.connect(path, timeout=30)
    db.row_factory = sqlite3.Row
    try:
        db.execute("PRAGMA busy_timeout=30000")
        yield db
    finally:
        db.close()


class Database:
    def __init__(self, path: Path):
        self.path = Path(path)

    def initialize(self):
        self.path.parent.mkdir(parents=True, exist_ok=True)
        with connect(self.path) as db:
            version = db.execute("PRAGMA user_version").fetchone()[0]
            if version not in (0, SCHEMA_VERSION):
                raise RuntimeError(f"Unbekannte Datenbankversion: {version}")
            # Rollback-Journal: kurze Transaktionen, kein WAL-Reset bei parallelen Lesern.
            db.execute("PRAGMA journal_mode=DELETE")
            db.execute("PRAGMA synchronous=FULL")
            db.execute("""CREATE TABLE IF NOT EXISTS samples (
                timestamp_utc INTEGER PRIMARY KEY CHECK (timestamp_utc % 900 = 0),
                observed_at_utc INTEGER NOT NULL,
                t0 REAL, t1 REAL, t2 REAL,
                s0 TEXT NOT NULL, s1 TEXT NOT NULL, s2 TEXT NOT NULL,
                result TEXT NOT NULL, detail TEXT NOT NULL DEFAULT ''
            )""")
            db.execute(f"PRAGMA user_version={SCHEMA_VERSION}")
            db.commit()

    def insert(self, timestamp: int, observed_at: int, reading: Reading) -> bool:
        with connect(self.path) as db:
            cursor = db.execute(
                f"INSERT INTO samples ({COLUMNS}) VALUES (?,?,?,?,?,?,?,?,?,?) ON CONFLICT(timestamp_utc) DO NOTHING",
                (timestamp, observed_at, *reading.temperatures, *reading.sensor_status, reading.result, reading.detail),
            )
            db.commit()
            return cursor.rowcount == 1

    def latest(self):
        with connect(self.path, readonly=True) as db:
            row = db.execute("SELECT * FROM samples ORDER BY timestamp_utc DESC LIMIT 1").fetchone()
            return dict(row) if row else None

    def rows(self, start: float, end: float):
        with connect(self.path, readonly=True) as db:
            cursor = db.execute("SELECT * FROM samples WHERE timestamp_utc >= ? AND timestamp_utc < ? ORDER BY timestamp_utc", (start, end))
            while batch := cursor.fetchmany(1024):
                yield from batch


@contextmanager
def collector_lock(path: Path):
    path.parent.mkdir(parents=True, exist_ok=True)
    with path.with_suffix(".lock").open("a") as handle:
        try:
            fcntl.flock(handle, fcntl.LOCK_EX | fcntl.LOCK_NB)
        except BlockingIOError as exc:
            raise RuntimeError("Für diese Datenbank läuft bereits ein Archivdienst. Nur einen Worker starten.") from exc
        try:
            yield
        finally:
            fcntl.flock(handle, fcntl.LOCK_UN)
