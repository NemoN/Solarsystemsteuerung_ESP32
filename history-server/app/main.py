import asyncio
from contextlib import asynccontextmanager, suppress
from pathlib import Path
import time
import json
import os
import subprocess
import sys

from fastapi import FastAPI, HTTPException, Query, Request
from fastapi.responses import FileResponse, StreamingResponse
from fastapi.staticfiles import StaticFiles
from starlette.background import BackgroundTask

from .collector import Collector
from .config import Settings
from .database import Database, collector_lock
from .history import build_history, csv_file, resolve_range, stream_csv
from .updater import check_updates, status_path, write_status

STATIC = Path(__file__).parent / "static"


def create_app(settings=None, *, start_collector=True, client=None, clock=time.time):
    settings = settings or Settings.from_env()
    database = Database(settings.database_path)
    collector = Collector(database, settings.source_url, client=client, clock=clock)

    @asynccontextmanager
    async def lifespan(app):
        with collector_lock(database.path):
            database.initialize()
            # Ein Neustart bestätigt den Abschluss; ein unterbrochenes Update bleibt sichtbar.
            update_file = status_path(database.path)
            try:
                previous_update = json.loads(update_file.read_text(encoding="utf-8"))
                if previous_update.get("state") == "restarting":
                    write_status(update_file, "complete", "Update installiert und Dienst neu gestartet.")
                elif previous_update.get("state") == "running":
                    write_status(update_file, "error", "Update wurde durch einen Dienstneustart unterbrochen. Serverprotokoll prüfen.")
            except (OSError, ValueError):
                pass
            task = asyncio.create_task(collector.run()) if start_collector else None
            app.state.database = database
            app.state.collector = collector
            try:
                yield
            finally:
                if task:
                    task.cancel()
                    with suppress(asyncio.CancelledError):
                        await task

    app = FastAPI(title="Backload Temperaturarchiv", lifespan=lifespan, docs_url=None, redoc_url=None)

    @app.middleware("http")
    async def headers(request, call_next):
        response = await call_next(request)
        response.headers["X-Content-Type-Options"] = "nosniff"
        response.headers["Referrer-Policy"] = "same-origin"
        response.headers["Content-Security-Policy"] = "default-src 'self'; script-src 'self'; style-src 'self'; img-src 'self' data:; connect-src 'self'; object-src 'none'; base-uri 'self'; frame-ancestors 'none'"
        if request.url.path.startswith("/api/") or request.url.path == "/":
            response.headers["Cache-Control"] = "no-store"
        return response

    def bounds(start, end):
        try:
            return resolve_range(start, end, clock())
        except ValueError as exc:
            raise HTTPException(status_code=400, detail=str(exc)) from exc

    @app.get("/healthz")
    def health():
        return {"status": "ok"}

    @app.get("/")
    def index():
        return FileResponse(STATIC / "index.html")

    @app.get("/api/collector-status")
    def collector_status():
        return collector.status()

    @app.get("/api/history")
    def history(start: str | None = Query(None, alias="from"), end: str | None = Query(None, alias="to")):
        return build_history(database, *bounds(start, end))

    @app.get("/api/pump-runtime")
    def pump_runtime(kind: str = "day", limit: int = 365):
        if kind not in ("day", "month", "year") or not 1 <= limit <= 5000:
            raise HTTPException(status_code=400, detail="Ungültiger Zeitraum oder Limit.")
        return {"kind": kind, "periods": database.runtime_periods(kind, limit)}

    @app.get("/api/update")
    def update_status():
        enabled = os.getenv("HISTORY_UPDATE_ENABLED") == "1"
        if not enabled:
            return {"enabled": False, "state": "disabled", "message": "Update per Schaltfläche ist nicht eingerichtet."}
        try:
            data = json.loads(status_path(database.path).read_text(encoding="utf-8"))
        except (OSError, ValueError):
            data = {"state": "idle", "message": "Bereit für ein Update."}
        return {"enabled": True, **data}

    def require_update_request(request: Request):
        if os.getenv("HISTORY_UPDATE_ENABLED") != "1":
            raise HTTPException(403, "Update per Schaltfläche ist nicht eingerichtet.")
        origin = request.headers.get("origin", "")
        expected = f"{request.url.scheme}://{request.headers.get('host', '')}"
        if origin != expected or request.headers.get("x-requested-with") != "HistoryUI":
            raise HTTPException(403, "Bitte das Webinterface verwenden.")

    @app.post("/api/update/check")
    def check_update(request: Request):
        require_update_request(request)
        try:
            return check_updates(database.path)
        except RuntimeError as exc:
            raise HTTPException(409, str(exc)) from exc
        except (OSError, ValueError, subprocess.SubprocessError) as exc:
            raise HTTPException(503, "Update-Prüfung fehlgeschlagen. Git-Verbindung und Upstream prüfen.") from exc

    @app.post("/api/update", status_code=202)
    def start_update(request: Request):
        require_update_request(request)
        # Nur ein gestarteter Prozess pro Server; zusätzlich schützt flock vor parallelen Updates.
        process = getattr(app.state, "update_process", None)
        if process is not None and process.poll() is None:
            raise HTTPException(409, "Ein Update läuft bereits.")
        try:
            app.state.update_process = subprocess.Popen(
                [sys.executable, "-m", "app.updater", str(database.path)],
                cwd=STATIC.parent.parent, stdin=subprocess.DEVNULL,
                stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL, start_new_session=True)
        except OSError as exc:
            raise HTTPException(500, "Updateprozess konnte nicht gestartet werden.") from exc
        return {"state": "running"}

    @app.get("/api/export.csv")
    def export(start: str | None = Query(None, alias="from"), end: str | None = Query(None, alias="to")):
        begin, stop = bounds(start, end)
        handle = csv_file(database, begin, stop)
        return StreamingResponse(
            stream_csv(handle), media_type="text/csv; charset=utf-8",
            headers={"Content-Disposition": 'attachment; filename="backload-temperaturen.csv"'},
            background=BackgroundTask(handle.close),
        )

    app.mount("/static", StaticFiles(directory=STATIC), name="static")
    return app


app = create_app()
