import io
from copy import deepcopy
from unittest.mock import patch

from fastapi.testclient import TestClient

from app.backup import export_database, restore_database
from app.config import Settings
from app.database import Database
from app.history import build_history, csv_file
from app.main import create_app
from app.models import parse_reading
from app.updater import status_path, write_status
from app import updater
import json
import subprocess


def test_discovery_preserves_gaps_csv_and_backup(db, payload, tmp_path):
    db.insert(900, 900, parse_reading(payload))
    payload["controllers"] += [
        {"id": 3, "name": "Kombispeicher unten", "temperature": 42, "valid": True},
        {"id": 8, "name": "Neuer Fühler", "temperature": 32, "valid": True},
    ]
    db.insert(1800, 1800, parse_reading(payload))
    payload["controllers"][-1]["valid"] = False
    db.insert(2700, 2700, parse_reading(payload))
    data = build_history(db, 0, 3600)
    assert [s["id"] for s in data["series"]] == [0, 1, 2, 3, 8]
    assert data["series"][-1]["points"] == [{"x": 1800000, "y": 32.0}, {"x": 2700000, "y": None}]
    with csv_file(db, 0, 3600) as output:
        csv = output.read()
    assert "Neuer Fühler (°C)" in csv and "Sensorfehler" in csv
    backup = io.BytesIO(); export_database(db.path, backup)
    target = tmp_path / "restored.sqlite3"; restore_database(target, io.BytesIO(backup.getvalue()))
    restored = Database(target); restored.initialize()
    assert build_history(restored, 0, 3600) == data


def test_duplicate_extra_and_simulation(payload):
    sensor = {"id": 5, "name": "Extra", "temperature": 50, "valid": True}
    payload["controllers"] += [sensor, deepcopy(sensor)]
    reading = parse_reading(payload)
    assert reading.extra_sensors[0][2:] == (None, "duplicate")
    assert reading.result == "partial"
    payload["simulation"] = True
    assert parse_reading(payload).extra_sensors == ()


def test_update_disabled_origin_check_and_restart_status(tmp_path, monkeypatch):
    settings = Settings("http://esp32/api/status", tmp_path / "archive.sqlite3")
    monkeypatch.delenv("HISTORY_UPDATE_ENABLED", raising=False)
    with TestClient(create_app(settings, start_collector=False)) as client:
        assert not client.get("/api/update").json()["enabled"]
        assert client.post("/api/update").status_code == 403
    monkeypatch.setenv("HISTORY_UPDATE_ENABLED", "1")
    with TestClient(create_app(settings, start_collector=False)) as client:
        assert client.post("/api/update", headers={"origin": "http://evil", "x-requested-with": "HistoryUI"}).status_code == 403
        assert client.post("/api/update", headers={"origin": "http://testserver"}).status_code == 403
        with patch("app.main.subprocess.Popen") as popen:
            assert client.post("/api/update", headers={"origin": "http://testserver", "x-requested-with": "HistoryUI"}).status_code == 202
            assert popen.call_count == 1
    write_status(status_path(settings.database_path), "restarting", "Neustart")
    with TestClient(create_app(settings, start_collector=False)) as client:
        assert client.get("/api/update").json()["state"] == "complete"


def test_update_sequence_and_failure_are_reported(tmp_path, monkeypatch):
    calls = []
    def command(args, **kwargs):
        calls.append(args)
        output = "main" if "symbolic-ref" in args else "origin/main" if "rev-parse" in args else ""
        return subprocess.CompletedProcess(args, 0, stdout=output)
    monkeypatch.setattr(updater.subprocess, "run", command)
    database = tmp_path / "archive.sqlite3"
    updater.run(database)
    assert json.loads(status_path(database).read_text())["state"] == "restarting"
    assert ["git", "pull", "--ff-only", "--no-rebase"] in calls
    assert calls[-1] == ["sudo", "-n", "/usr/bin/systemctl", "restart", "--no-block", "backload-history.service"]
    calls.clear()
    def dirty(args, **kwargs):
        calls.append(args)
        return subprocess.CompletedProcess(args, 0, stdout=" M changed.py")
    monkeypatch.setattr(updater.subprocess, "run", dirty)
    updater.run(database)
    result = json.loads(status_path(database).read_text())
    assert result["state"] == "error" and "lokale Änderungen" in result["message"]
    assert len(calls) == 1
