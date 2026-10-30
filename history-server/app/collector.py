import asyncio
import json
import logging
import math
import time
from contextlib import suppress
from datetime import datetime, timezone

import httpx

from .config import INTERVAL, TIMEOUT
from .database import Database
from .models import Reading, RESULT_MESSAGES, parse_reading

logger = logging.getLogger(__name__)


def utc_iso(timestamp):
    return datetime.fromtimestamp(timestamp, timezone.utc).isoformat(timespec="seconds").replace("+00:00", "Z") if timestamp is not None else None


def next_quarter(now: float) -> int:
    return (math.floor(now / INTERVAL) + 1) * INTERVAL


async def fetch_reading(client, url, *, timeout=TIMEOUT) -> Reading:
    try:
        # httpx-Timeouts gelten je Netzwerkphase; diese Frist gilt für die gesamte Abfrage.
        async with asyncio.timeout(timeout):
            async with client.stream("GET", url, timeout=timeout) as response:
                if response.status_code != 200:
                    return Reading.failure("http_error", f"HTTP {response.status_code}")
                data = bytearray()
                async for chunk in response.aiter_bytes():
                    data.extend(chunk)
                    if len(data) > 65536:
                        return Reading.failure("invalid_response", "Antwort ist größer als 64 KiB.")
            try:
                payload = json.loads(data)
            except (ValueError, UnicodeError, RecursionError):
                return Reading.failure("invalid_response", "Antwort enthält kein gültiges JSON.")
            return parse_reading(payload)
    except (TimeoutError, httpx.TimeoutException):
        return Reading.failure("timeout")
    except httpx.RequestError:
        return Reading.failure("unreachable")


class Collector:
    def __init__(self, database: Database, source_url: str, *, clock=time.time, sleep=asyncio.sleep, client=None):
        self.database = database
        self.source_url = source_url
        self.clock, self.sleep = clock, sleep
        self.client = client
        self.next_capture = next_quarter(self.clock())
        self.last_attempt = None
        self.last_result = None

    async def collect(self, slot: int):
        self.last_attempt = self.clock()
        reading = await fetch_reading(self.client, self.source_url)
        try:
            write = asyncio.create_task(asyncio.to_thread(self.database.insert, slot, int(self.last_attempt), reading))
            try:
                await asyncio.shield(write)
            except asyncio.CancelledError:
                # Sperre beim Herunterfahren erst nach der laufenden DB-Transaktion lösen.
                with suppress(Exception):
                    await write
                raise
            self.last_result = {"code": reading.result, "message": RESULT_MESSAGES[reading.result], "detail": reading.detail}
        except Exception:
            logger.exception("Messung konnte nicht gespeichert werden")
            self.last_result = {"code": "storage_error", "message": RESULT_MESSAGES["storage_error"], "detail": "Datenträger und Serverprotokoll prüfen."}

    async def _run(self):
        # Kein Sofortwert und kein Nachholen nach Neustart: nächster zukünftiger Termin.
        self.next_capture = next_quarter(self.clock())
        while True:
            now = self.clock()
            if now < self.next_capture:
                await self.sleep(min(1.0, self.next_capture - now))
                continue
            if now - self.next_capture >= INTERVAL:
                self.next_capture = next_quarter(now)
                continue
            slot = self.next_capture
            await self.collect(slot)
            self.next_capture = max(slot + INTERVAL, next_quarter(self.clock()))

    async def run(self):
        if self.client is not None:
            await self._run()
        else:
            async with httpx.AsyncClient(trust_env=False, follow_redirects=False) as client:
                self.client = client
                try:
                    await self._run()
                finally:
                    self.client = None

    def status(self):
        latest = self.database.latest()
        last_result = self.last_result
        if last_result is None and latest:
            code = latest["result"]
            last_result = {"code": code, "message": RESULT_MESSAGES.get(code, code), "detail": latest["detail"]}
        return {
            "server_time": utc_iso(self.clock()),
            "last_capture": utc_iso(latest["timestamp_utc"]) if latest else None,
            "last_attempt": utc_iso(self.last_attempt if self.last_attempt is not None else latest["observed_at_utc"] if latest else None),
            "last_result": last_result,
            "next_capture": utc_iso(self.next_capture),
            "interval_seconds": INTERVAL,
            "timezone": "Europe/Berlin",
        }
