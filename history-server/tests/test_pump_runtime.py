import asyncio
import io
import sqlite3

import httpx
from fastapi.testclient import TestClient

from app.backup import export_database, restore_database
from app.collector import Collector
from app.config import Settings
from app.database import Database, connect
from app.main import create_app
from app.models import parse_reading


def runtime():
    return {"available": True, "clockValid": True, "storageHealthy": True,
            "days": [{"period": 20261025, "solar": 3600, "combiToBuffer": 120, "bufferToCombi": 240}],
            "months": [{"period": 202610, "solar": 7200, "combiToBuffer": 120, "bufferToCombi": 240}],
            "years": [{"period": 2026, "solar": 10000, "combiToBuffer": 120, "bufferToCombi": 240}]}


def test_parse_validates_records_and_never_archives_simulation(payload):
    payload["pumpRuntime"] = runtime()
    values = parse_reading(payload).runtime
    assert [(r.kind, r.period, r.solar) for r in values] == [("day", 20261025, 3600), ("month", 202610, 7200), ("year", 2026, 10000)]
    payload["pumpRuntime"]["days"].extend([
        {"period": 20260230, "solar": 1, "combiToBuffer": 1, "bufferToCombi": 1},
        {"period": 20261024, "solar": True, "combiToBuffer": 1, "bufferToCombi": 1},
        {"period": 20261025, "solar": 9999, "combiToBuffer": 1, "bufferToCombi": 1},
    ])
    assert len(parse_reading(payload).runtime) == 3
    payload["simulation"] = True
    assert not parse_reading(payload).runtime
    payload["simulation"] = False
    payload["pumpRuntime"]["available"] = False
    assert not parse_reading(payload).runtime


def test_collector_archives_updates_retrieves_and_survives_restart(db, payload):
    async def scenario():
        payload["pumpRuntime"] = runtime()
        async with httpx.AsyncClient(transport=httpx.MockTransport(lambda req: httpx.Response(200, json=payload))) as client:
            collector = Collector(db, "http://esp32/api/status", client=client, clock=lambda: 1800)
            await collector.collect(900)
            payload["pumpRuntime"]["days"][0]["solar"] = 3700
            await collector.collect(1800)
            # Ein nach Stromausfall älterer Flash-Stand darf die Archivwerte nicht überschreiben.
            payload["pumpRuntime"]["days"][0]["solar"] = 3650
            await collector.collect(2700)
            payload["simulation"] = True
            payload["pumpRuntime"]["days"][0]["solar"] = 9999
            await collector.collect(3600)
    asyncio.run(scenario())
    reopened = Database(db.path); reopened.initialize()
    assert reopened.runtime_periods("day")[0]["solar"] == 3700
    assert reopened.runtime_periods("month")[0]["solar"] == 7200
    assert len(list(reopened.rows(0, 4500))) == 4


def test_v1_database_migration_backup_and_restore(tmp_path):
    path = tmp_path / "v1.sqlite3"
    with sqlite3.connect(path) as db:
        db.execute("""CREATE TABLE samples (timestamp_utc INTEGER PRIMARY KEY, observed_at_utc INTEGER NOT NULL,
                    t0 REAL,t1 REAL,t2 REAL,s0 TEXT NOT NULL,s1 TEXT NOT NULL,s2 TEXT NOT NULL,
                    result TEXT NOT NULL,detail TEXT NOT NULL DEFAULT '')""")
        db.execute("INSERT INTO samples VALUES (900,901,1,2,3,'ok','ok','ok','ok','')")
        db.execute("PRAGMA user_version=1")
    # Auch alte Sicherungen bleiben wiederherstellbar und werden beim nächsten Start migriert.
    old = io.BytesIO(); export_database(path, old)
    target = tmp_path / "restored.sqlite3"; restore_database(target, io.BytesIO(old.getvalue()))
    migrated = Database(target); migrated.initialize()
    assert migrated.latest()["t0"] == 1
    assert migrated.runtime_periods("day") == []
    with connect(target) as db:
        assert db.execute("PRAGMA user_version").fetchone()[0] == 3
    new = io.BytesIO(); export_database(target, new)
    restore_database(tmp_path / "v2.sqlite3", io.BytesIO(new.getvalue()))


def test_runtime_api_returns_archived_periods_and_rejects_bad_input(tmp_path, payload):
    settings = Settings("http://esp32/api/status", tmp_path / "archive.sqlite3")
    with TestClient(create_app(settings, start_collector=False)) as client:
        payload["pumpRuntime"] = runtime()
        client.app.state.database.insert(900, 902, parse_reading(payload))
        response = client.get("/api/pump-runtime?kind=day")
        assert response.status_code == 200
        assert response.json()["periods"][0]["buffer_to_combi"] == 240
        assert client.get("/api/pump-runtime?kind=invalid").status_code == 400
        assert client.get("/api/pump-runtime?limit=0").status_code == 400
