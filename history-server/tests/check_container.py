"""Optionaler Integrationstest mit einem ausschließlich hierfür erzeugten Volume.

Aufruf aus history-server/: python3 tests/check_container.py
Benötigt laufendes Docker und Compose v2. Keine echten ESP32-Abfragen.
"""
import json
import os
from pathlib import Path
import subprocess
import time
import urllib.request
import uuid

ROOT = Path(__file__).resolve().parents[1]


def main():
    project = "backload-check-" + uuid.uuid4().hex[:10]
    env = {**os.environ, "HISTORY_PORT": "0", "ESP32_STATUS_URL": "http://127.0.0.1:9/api/status"}

    def compose(*args, **kwargs):
        return subprocess.run(["docker", "compose", "-p", project, *args], cwd=ROOT, env=env, check=True, **kwargs)

    def url():
        binding = compose("port", "history", "8080", capture_output=True, text=True).stdout.splitlines()[0]
        return "http://127.0.0.1:" + binding.rsplit(":", 1)[1]

    def wait_and_read():
        endpoint = url() + "/api/history?from=1970-01-01&to=2090-01-01"
        for _ in range(60):
            try:
                with urllib.request.urlopen(endpoint, timeout=2) as response:
                    result = json.load(response)
                assert result["sample_count"] >= 1
                assert any(p["y"] == 71.5 for p in result["series"][0]["points"])
                return
            except (OSError, ValueError):
                time.sleep(1)
        raise AssertionError("Container wurde nicht erreichbar")

    try:
        compose("build", "history")
        compose("run", "--rm", "-T", "--no-deps", "history", "python", "-c", """
from app.config import Settings
from app.database import Database
from app.models import Reading
db=Database(Settings.from_env().database_path)
db.initialize()
db.insert(900, 901, Reading((71.5,51.2,49.6),('ok','ok','ok'),'ok'))
""")
        compose("up", "-d", "--no-build", "history")
        wait_and_read()
        before = compose("ps", "-q", "history", capture_output=True, text=True).stdout.strip()
        compose("up", "-d", "--no-build", "--force-recreate", "history")
        after = compose("ps", "-q", "history", capture_output=True, text=True).stdout.strip()
        assert before != after, "Container wurde nicht ersetzt"
        wait_and_read()
        backup = compose("exec", "-T", "history", "python", "-m", "app.backup", "export", capture_output=True).stdout
        assert backup.startswith(b"SQLite format 3")
        compose("stop", "history")
        compose("run", "--rm", "-T", "--no-deps", "history", "python", "-m", "app.backup", "restore", input=backup)
        compose("up", "-d", "--no-build", "history")
        wait_and_read()
        print("Containerersatz und Wiederherstellung: Daten im Test-Volume erhalten.")
    finally:
        # Nur das hier zufällig benannte Testprojekt samt seinem Test-Volume löschen.
        compose("down", "-v", "--remove-orphans")


if __name__ == "__main__":
    main()
