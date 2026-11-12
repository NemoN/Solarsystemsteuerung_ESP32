import fcntl
import socket
import subprocess

import pytest
from fastapi.testclient import TestClient

from app.config import Settings
from app.main import create_app
from app.server import bind_sockets
from app.updater import check_updates


def git(directory, *args):
    return subprocess.run(["git", *args], cwd=directory, check=True, capture_output=True, text=True).stdout.strip()


def test_update_check_reports_only_history_changes_and_conflicts(tmp_path):
    remote = tmp_path / "remote.git"
    git(tmp_path, "init", "--bare", "--initial-branch=main", str(remote))
    author = tmp_path / "author"
    git(tmp_path, "clone", str(remote), str(author))
    git(author, "config", "user.name", "Test")
    git(author, "config", "user.email", "test@example.invalid")
    (author / "history-server").mkdir()
    (author / "history-server" / "test.txt").write_text("one")
    git(author, "add", ".")
    git(author, "commit", "-m", "Initial")
    git(author, "push", "-u", "origin", "main")
    checkout = tmp_path / "checkout"
    git(tmp_path, "clone", str(remote), str(checkout))
    db = tmp_path / "data" / "archive.sqlite3"
    result = check_updates(db, root=checkout)
    assert not result["available"] and result["changes"] == []
    (author / "history-server" / "test.txt").write_text("two")
    git(author, "commit", "-am", "Neue Archivfunktion", "-m", "Details zur Änderung")
    (author / "firmware.txt").write_text("other")
    git(author, "add", ".")
    git(author, "commit", "-m", "Firmware update")
    git(author, "push")
    result = check_updates(db, root=checkout)
    assert result["available"] and result["can_update"] and result["behind"] == 2
    assert [entry["title"] for entry in result["changes"]] == ["Neue Archivfunktion"]
    assert result["changes"][0]["body"] == "Details zur Änderung"
    (checkout / "history-server" / "test.txt").write_text("local changes")
    assert not check_updates(db, root=checkout)["can_update"]
    with (db.parent / "history-update.lock").open("a") as lock:
        fcntl.flock(lock, fcntl.LOCK_EX | fcntl.LOCK_NB)
        with pytest.raises(RuntimeError, match="läuft bereits"):
            check_updates(db, root=checkout)


def test_check_endpoint_requires_same_origin_and_handles_errors(tmp_path, monkeypatch):
    monkeypatch.setenv("HISTORY_UPDATE_ENABLED", "1")
    app = create_app(Settings("http://esp32/api/status", tmp_path / "archive.sqlite3"), start_collector=False)
    with TestClient(app) as client:
        assert client.post("/api/update/check", headers={"origin": "http://evil", "x-requested-with": "HistoryUI"}).status_code == 403
        monkeypatch.setattr("app.main.check_updates", lambda path: {"available": False, "changes": []})
        result = client.post("/api/update/check", headers={"origin": "http://testserver", "x-requested-with": "HistoryUI"})
        assert result.status_code == 200 and result.json()["available"] is False
        def failed(_):
            raise subprocess.CalledProcessError(1, "git")
        monkeypatch.setattr("app.main.check_updates", failed)
        assert client.post("/api/update/check", headers={"origin": "http://testserver", "x-requested-with": "HistoryUI"}).status_code == 503


def test_bind_sockets_listen_and_cleanup_on_failure():
    listeners = bind_sockets(["127.0.0.1"], 0)
    try:
        address = listeners[0].getsockname()
        with socket.create_connection(address, timeout=1):
            listeners[0].accept()[0].close()
        with pytest.raises(OSError):
            bind_sockets(["127.0.0.1", "127.0.0.1"], address[1])
    finally:
        for listener in listeners:
            listener.close()
