"""Konsistente Sicherung via SQLite-Backup-API; Restore nur bei gestopptem Dienst."""
from contextlib import closing
import os
from pathlib import Path
import shutil
import sqlite3
import sys
import tempfile

from .config import Settings
from .database import SCHEMA_VERSION, collector_lock, connect


def validate(path):
    with connect(path, readonly=True) as db:
        if db.execute("PRAGMA integrity_check").fetchone()[0] != "ok":
            raise ValueError("Die Sicherung ist beschädigt.")
        if db.execute("PRAGMA user_version").fetchone()[0] != SCHEMA_VERSION:
            raise ValueError("Die Sicherung hat eine unbekannte Datenbankversion.")
        columns = [row[1] for row in db.execute("PRAGMA table_info(samples)")]
        if columns != ["timestamp_utc", "observed_at_utc", "t0", "t1", "t2", "s0", "s1", "s2", "result", "detail"]:
            raise ValueError("Die Sicherung ist kein Backload-Temperaturarchiv.")


def export_database(path: Path, output):
    with tempfile.TemporaryDirectory() as directory:
        snapshot = Path(directory) / "backup.sqlite3"
        with connect(path, readonly=True) as source, closing(sqlite3.connect(snapshot)) as target:
            source.backup(target)
        validate(snapshot)
        with snapshot.open("rb") as source:
            shutil.copyfileobj(source, output)


def restore_database(path: Path, source):
    with collector_lock(path):
        handle, filename = tempfile.mkstemp(dir=path.parent, prefix="restore-", suffix=".sqlite3")
        snapshot = Path(filename)
        try:
            with os.fdopen(handle, "wb") as target:
                shutil.copyfileobj(source, target)
                target.flush()
                os.fsync(target.fileno())
            validate(snapshot)
            # Nie einen möglicherweise noch offenen SQLite-Journalzustand übergehen.
            if any(Path(str(path) + suffix).exists() for suffix in ("-journal", "-wal", "-shm")):
                raise RuntimeError("SQLite-Journal vorhanden. Dienst erst sauber starten und stoppen.")
            os.replace(snapshot, path)
        finally:
            snapshot.unlink(missing_ok=True)


def main():
    try:
        path = Settings.from_env().database_path
        if sys.argv[1:] == ["export"]:
            export_database(path, sys.stdout.buffer)
        elif sys.argv[1:] == ["restore"]:
            restore_database(path, sys.stdin.buffer)
        else:
            raise ValueError("Aufruf: python -m app.backup export|restore")
    except Exception as exc:
        print(f"Sicherung/Wiederherstellung fehlgeschlagen: {exc}", file=sys.stderr)
        sys.exit(1)


if __name__ == "__main__":
    main()
