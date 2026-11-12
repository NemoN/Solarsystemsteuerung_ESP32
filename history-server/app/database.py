from contextlib import contextmanager
import fcntl
from pathlib import Path
import sqlite3

from .models import Reading

SCHEMA_VERSION = 3
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
            if version not in (0, 1, 2, SCHEMA_VERSION):
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
            db.execute("""CREATE TABLE IF NOT EXISTS pump_runtime (
                kind TEXT NOT NULL CHECK (kind IN ('day', 'month', 'year')),
                period INTEGER NOT NULL,
                solar INTEGER NOT NULL CHECK (solar >= 0),
                combi_to_buffer INTEGER NOT NULL CHECK (combi_to_buffer >= 0),
                buffer_to_combi INTEGER NOT NULL CHECK (buffer_to_combi >= 0),
                observed_at_utc INTEGER NOT NULL,
                PRIMARY KEY (kind, period)
            )""")
            db.execute("""CREATE TABLE IF NOT EXISTS discovered_sensors (
                sensor_id INTEGER PRIMARY KEY, name TEXT NOT NULL
            )""")
            db.execute("""CREATE TABLE IF NOT EXISTS sensor_samples (
                timestamp_utc INTEGER NOT NULL REFERENCES samples(timestamp_utc),
                sensor_id INTEGER NOT NULL REFERENCES discovered_sensors(sensor_id),
                value REAL, status TEXT NOT NULL, PRIMARY KEY(timestamp_utc, sensor_id)
            )""")
            db.execute(f"PRAGMA user_version={SCHEMA_VERSION}")
            db.commit()

    def insert(self, timestamp: int, observed_at: int, reading: Reading) -> bool:
        with connect(self.path) as db:
            cursor = db.execute(
                f"INSERT INTO samples ({COLUMNS}) VALUES (?,?,?,?,?,?,?,?,?,?) ON CONFLICT(timestamp_utc) DO NOTHING",
                (timestamp, observed_at, *reading.temperatures, *reading.sensor_status, reading.result, reading.detail),
            )
            if cursor.rowcount == 1:
                for sensor_id, name, value, status in reading.extra_sensors:
                    db.execute("INSERT INTO discovered_sensors VALUES (?, ?) ON CONFLICT(sensor_id) DO UPDATE SET name=excluded.name", (sensor_id, name))
                    db.execute("INSERT INTO sensor_samples VALUES (?,?,?,?)", (timestamp, sensor_id, value, status))
            for record in reading.runtime:
                db.execute("""INSERT INTO pump_runtime VALUES (?,?,?,?,?,?)
                    ON CONFLICT(kind, period) DO UPDATE SET
                      solar = max(pump_runtime.solar, excluded.solar),
                      combi_to_buffer = max(pump_runtime.combi_to_buffer, excluded.combi_to_buffer),
                      buffer_to_combi = max(pump_runtime.buffer_to_combi, excluded.buffer_to_combi),
                      observed_at_utc = max(pump_runtime.observed_at_utc, excluded.observed_at_utc)""",
                    (record.kind, record.period, record.solar, record.combi_to_buffer, record.buffer_to_combi, observed_at))
            db.commit()
            return cursor.rowcount == 1

    def latest(self):
        with connect(self.path, readonly=True) as db:
            row = db.execute("SELECT * FROM samples ORDER BY timestamp_utc DESC LIMIT 1").fetchone()
            return dict(row) if row else None

    def runtime_periods(self, kind: str, limit: int = 365):
        with connect(self.path, readonly=True) as db:
            return [dict(row) for row in db.execute(
                "SELECT * FROM pump_runtime WHERE kind = ? ORDER BY period DESC LIMIT ?", (kind, limit))]

    def sensors(self):
        from .config import SENSORS
        colors = ("#ff947e", "#6dc9f2", "#f2d36f", "#d6a2ec", "#b9dc87", "#f3a8b6")
        with connect(self.path, readonly=True) as db:
            extras = db.execute("SELECT sensor_id, name FROM discovered_sensors ORDER BY sensor_id").fetchall()
        return list(SENSORS) + [(row[0], row[1], colors[(row[0] - 3) % len(colors)]) for row in extras]

    def rows(self, start: float, end: float):
        with connect(self.path, readonly=True) as db:
            cursor = db.execute("SELECT * FROM samples WHERE timestamp_utc >= ? AND timestamp_utc < ? ORDER BY timestamp_utc", (start, end))
            extra_cursor = db.execute("SELECT * FROM sensor_samples WHERE timestamp_utc >= ? AND timestamp_utc < ? ORDER BY timestamp_utc, sensor_id", (start, end))
            extra = extra_cursor.fetchone()
            while batch := cursor.fetchmany(1024):
                for sample in batch:
                    row = dict(sample)
                    while extra is not None and extra["timestamp_utc"] <= row["timestamp_utc"]:
                        if extra["timestamp_utc"] == row["timestamp_utc"]:
                            row[f"t{extra['sensor_id']}"] = extra["value"]
                            row[f"s{extra['sensor_id']}"] = extra["status"]
                        extra = extra_cursor.fetchone()
                    yield row


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
