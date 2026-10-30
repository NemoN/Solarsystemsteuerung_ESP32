import asyncio
from contextlib import asynccontextmanager, suppress
from pathlib import Path
import time

from fastapi import FastAPI, HTTPException, Query
from fastapi.responses import FileResponse, StreamingResponse
from fastapi.staticfiles import StaticFiles
from starlette.background import BackgroundTask

from .collector import Collector
from .config import Settings
from .database import Database, collector_lock
from .history import build_history, csv_file, resolve_range, stream_csv

STATIC = Path(__file__).parent / "static"


def create_app(settings=None, *, start_collector=True, client=None, clock=time.time):
    settings = settings or Settings.from_env()
    database = Database(settings.database_path)
    collector = Collector(database, settings.source_url, client=client, clock=clock)

    @asynccontextmanager
    async def lifespan(app):
        with collector_lock(database.path):
            database.initialize()
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
