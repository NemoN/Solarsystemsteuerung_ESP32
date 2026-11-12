"""Optionaler Git-Updateprozess für einen systemd-Dienst."""
import fcntl
import json
import os
from pathlib import Path
import subprocess
import sys
import tempfile

ROOT = Path(__file__).resolve().parents[2]
SERVER = ROOT / "history-server"


def status_path(database_path):
    return Path(database_path).parent / "history-update.json"


def write_status(path, state, message):
    path.parent.mkdir(parents=True, exist_ok=True)
    with tempfile.NamedTemporaryFile(mode="w", encoding="utf-8", dir=path.parent, delete=False) as out:
        json.dump({"state": state, "message": message}, out)
        name = out.name
    os.replace(name, path)


def run(database_path):
    path = status_path(database_path)
    with (path.parent / "history-update.lock").open("a") as lock:
        try:
            fcntl.flock(lock, fcntl.LOCK_EX | fcntl.LOCK_NB)
        except BlockingIOError:
            return
        try:
            def command(args):
                return subprocess.run(args, cwd=ROOT, check=True, text=True, stdout=subprocess.PIPE,
                                      stderr=subprocess.STDOUT, timeout=300).stdout.strip()
            write_status(path, "running", "Git-Checkout wird geprüft …")
            if command(["git", "status", "--porcelain", "--untracked-files=no"]):
                raise RuntimeError("Der Git-Checkout enthält lokale Änderungen an versionierten Dateien.")
            branch = command(["git", "symbolic-ref", "--quiet", "--short", "HEAD"])
            if not branch:
                raise RuntimeError("Kein Git-Branch ausgecheckt.")
            command(["git", "rev-parse", "--abbrev-ref", "--symbolic-full-name", "@{upstream}"])
            write_status(path, "running", "Änderungen werden geladen …")
            command(["git", "pull", "--ff-only", "--no-rebase"])
            write_status(path, "running", "Python-Abhängigkeiten werden installiert …")
            command([sys.executable, "-m", "pip", "install", "-r", str(SERVER / "requirements.txt")])
            # Der Status wird vor dem Neustart geschrieben, da systemd den Prozess beenden kann.
            write_status(path, "restarting", "Update installiert. Der Dienst wird neu gestartet.")
            command(["sudo", "-n", "/usr/bin/systemctl", "restart", "--no-block", "backload-history.service"])
        except (OSError, subprocess.SubprocessError, RuntimeError) as exc:
            detail = exc.stdout if isinstance(exc, subprocess.CalledProcessError) else str(exc)
            write_status(path, "error", "Update fehlgeschlagen: " + (detail or str(exc))[-1200:])


if __name__ == "__main__":
    run(sys.argv[1])
