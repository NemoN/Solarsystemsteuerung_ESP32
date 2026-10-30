import csv
from datetime import datetime
import io
import math
import sqlite3

import pytest

from app.backup import export_database, restore_database
from app.database import Database, collector_lock, connect
from app.history import build_history, csv_file, resolve_range, stream_csv
from app.models import Reading


def good(value=50):
    return Reading((value, value + 1, value + 2), ("ok",) * 3, "ok")


def test_unique_timestamp_keeps_first_snapshot_and_survives_reopen(db):
    assert db.insert(900, 902, good())
    assert not db.insert(900, 903, good(99))
    reopened = Database(db.path)
    reopened.initialize()
    assert reopened.latest()["t0"] == 50
    assert len(list(reopened.rows(0, 1800))) == 1
    with pytest.raises(sqlite3.IntegrityError):
        db.insert(901, 901, good())


def test_backup_restore_consistency_lock_and_invalid_restore(db, tmp_path):
    db.insert(900, 901, good())
    backup = io.BytesIO()
    export_database(db.path, backup)
    target = tmp_path / "restore.sqlite3"
    restore_database(target, io.BytesIO(backup.getvalue()))
    restored = Database(target)
    assert restored.latest() == db.latest()
    with pytest.raises(Exception):
        restore_database(target, io.BytesIO(b"not a database"))
    assert restored.latest() == db.latest()
    with collector_lock(target):
        with pytest.raises(RuntimeError, match="bereits"):
            restore_database(target, io.BytesIO(backup.getvalue()))
        with pytest.raises(RuntimeError):
            with collector_lock(target):
                pass
        # Online-Backup bleibt bei laufendem Collector möglich.
        export_database(target, io.BytesIO())


def test_corrupt_or_unknown_schema_not_erased(tmp_path):
    path = tmp_path / "bad.sqlite3"
    path.write_bytes(b"broken")
    with pytest.raises(sqlite3.DatabaseError):
        Database(path).initialize()
    assert path.read_bytes() == b"broken"
    path.unlink()
    with connect(path) as db:
        db.execute("PRAGMA user_version=999")
    with pytest.raises(RuntimeError):
        Database(path).initialize()


def test_dst_fall_and_spring_full_local_days():
    start, end = resolve_range("2026-10-25", "2026-10-25", 0)
    assert end - start == 25 * 3600
    start, end = resolve_range("2026-03-29", "2026-03-29", 0)
    assert end - start == 23 * 3600
    with pytest.raises(ValueError):
        resolve_range("2026-10-25T02:15:00", None, 0)


def test_csv_dst_distinguishes_repeated_hour_and_empty_temperatures(db):
    first = int(datetime.fromisoformat("2026-10-25T00:15:00+00:00").timestamp())
    second = first + 3600
    db.insert(first, first, good(51.2))
    db.insert(second, second, Reading((None, 52.2, None), ("sensor_error", "ok", "missing"), "partial"))
    content = b"".join(stream_csv(csv_file(db, first, second + 900)))
    assert content.startswith(b"\xef\xbb\xbf")
    rows = list(csv.reader(io.StringIO(content.decode("utf-8-sig")), delimiter=";"))
    assert rows[1][1] == "2026-10-25T02:15:00+02:00"
    assert rows[2][1] == "2026-10-25T02:15:00+01:00"
    assert rows[1][2] == "51,2"
    assert rows[2][2:5] == ["", "52,2", ""]
    assert rows[2][5:8] == ["Sensorfehler", "OK", "Sensor fehlt"]


def test_gaps_from_sensor_errors_and_missing_server_samples(db):
    db.insert(900, 900, good())
    db.insert(1800, 1800, Reading((None, 51, 52), ("sensor_error", "ok", "ok"), "partial"))
    db.insert(2700, 2700, good())
    db.insert(4500, 4500, good())  # Ein Termin wegen Serverausfall fehlt ganz.
    data = build_history(db, 0, 5400)
    assert data["sample_count"] == 4
    top = data["series"][0]["points"]
    middle = data["series"][1]["points"]
    assert [p["x"] for p in top if p["y"] is None] == [1800000, 3600000]
    assert [p["x"] for p in middle if p["y"] is None] == [3600000]


def seed_year(db):
    # 365 * 24 * 4 Originalzeitpunkte, einschließlich einer kurzen Störung.
    with connect(db.path) as connection:
        connection.executemany(
            "INSERT INTO samples VALUES (?,?,?,?,?,?,?,?,?,?)",
            ((i * 900, i * 900, None if i == 18000 else 50 + math.sin(i / 100), 30, 70,
              "sensor_error" if i == 18000 else "ok", "ok", "ok", "partial" if i == 18000 else "ok", "")
             for i in range(35040)),
        )
        connection.commit()


def test_year_display_downsample_keeps_extremes_and_gap_csv_is_full(db):
    seed_year(db)
    with connect(db.path) as connection:
        connection.execute("UPDATE samples SET t0=150 WHERE timestamp_utc=?", (12345 * 900,))
        connection.commit()
    data = build_history(db, 0, 365 * 86400)
    assert data["sample_count"] == 35040
    assert data["display_reduced"]
    curve = data["series"][0]["points"]
    assert len(curve) < 1300
    assert any(p["y"] == 150 for p in curve)
    assert any(p["x"] == 18000 * 900000 and p["y"] is None for p in curve)
    content = b"".join(stream_csv(csv_file(db, 0, 365 * 86400)))
    rows = list(csv.reader(io.StringIO(content.decode("utf-8-sig")), delimiter=";"))
    assert len(rows) == 35041
    assert rows[18001][2] == ""
    assert rows[12346][2] == "150,0"


def test_range_is_start_inclusive_end_exclusive(db):
    for slot in (900, 1800, 2700):
        db.insert(slot, slot, good())
    assert build_history(db, 1800, 2700)["sample_count"] == 1
    assert list(db.rows(1800, 2700))[0]["timestamp_utc"] == 1800
