import asyncio
from copy import deepcopy
import json

import httpx
import pytest

from app.collector import Collector, fetch_reading, next_quarter
from app.models import parse_reading


def test_sensor_ids_are_independent_of_order_and_name(payload):
    reading = parse_reading(payload)
    assert reading.temperatures == (71.5, 51.2, 49.6)
    assert reading.result == "ok"


def test_simulation_creates_gap_without_archiving_dummy_values(payload):
    payload["simulation"] = True
    reading = parse_reading(payload)
    assert reading.temperatures == (None, None, None)
    assert reading.result == "simulation"
    assert reading.sensor_status == ("unavailable",) * 3
    payload["simulation"] = False
    assert parse_reading(payload).temperatures == (71.5, 51.2, 49.6)


@pytest.mark.parametrize("temperature", [None, "51.2", True, float("nan"), float("inf"), -242, 851])
def test_bad_sensor_does_not_discard_other_values(payload, temperature):
    payload["controllers"][2]["temperature"] = temperature
    reading = parse_reading(payload)
    assert reading.temperatures == (71.5, None, 49.6)
    assert reading.result == "partial"


def test_fault_missing_duplicate_and_unknown_ids(payload):
    original = deepcopy(payload)
    payload["controllers"][2]["valid"] = False
    assert parse_reading(payload).sensor_status == ("ok", "sensor_error", "ok")
    payload = deepcopy(original)
    payload["controllers"][1]["fault"] = 128
    assert parse_reading(payload).temperatures == (None, 51.2, 49.6)
    payload = deepcopy(original)
    payload["controllers"].pop(2)
    payload["controllers"].append({"id": True, "temperature": 99, "valid": True})
    assert parse_reading(payload).sensor_status == ("ok", "missing", "ok")
    payload = deepcopy(original)
    payload["controllers"].append(payload["controllers"][2])
    assert parse_reading(payload).sensor_status == ("ok", "duplicate", "ok")


@pytest.mark.parametrize("body", [{}, [], None, {"controllers": {}}])
def test_invalid_response_schema(body):
    assert parse_reading(body).result == "invalid_response"


def test_network_failures_and_reconnection(db, payload):
    async def scenario():
        responses = ["offline", "timeout", "html", "oversize", 503, "schema", payload]
        def handler(request):
            value = responses.pop(0)
            if value == "offline":
                raise httpx.ConnectError("offline", request=request)
            if value == "timeout":
                raise httpx.ReadTimeout("timeout", request=request)
            if value == "html":
                return httpx.Response(200, content=b"<html>Fehler</html>")
            if value == "oversize":
                return httpx.Response(200, content=b"x" * 65537)
            if value == "schema":
                return httpx.Response(200, json={"other": []})
            if isinstance(value, int):
                return httpx.Response(value)
            return httpx.Response(200, json=value)
        async with httpx.AsyncClient(transport=httpx.MockTransport(handler)) as client:
            collector = Collector(db, "http://esp32/api/status", client=client, clock=lambda: 9000)
            for i in range(7):
                await collector.collect((i + 1) * 900)
        rows = list(db.rows(0, 9000))
        assert [row["result"] for row in rows] == ["unreachable", "timeout", "invalid_response", "invalid_response", "http_error", "invalid_response", "ok"]
        assert all(row["t0"] is None for row in rows[:-1])
        assert rows[-1]["t0"] == 71.5
        assert collector.last_result["code"] == "ok"
    asyncio.run(scenario())


def test_five_seconds_is_a_total_deadline(payload):
    class SlowStream(httpx.AsyncByteStream):
        async def __aiter__(self):
            for chunk in (b'{"controllers":', json.dumps(payload["controllers"]).encode(), b"}"):
                await asyncio.sleep(.03)
                yield chunk
    async def scenario():
        async with httpx.AsyncClient(transport=httpx.MockTransport(lambda _: httpx.Response(200, stream=SlowStream()))) as client:
            reading = await fetch_reading(client, "http://esp32/api/status", timeout=.05)
            assert reading.result == "timeout"
    asyncio.run(scenario())


@pytest.mark.parametrize("now,expected", [(0, 900), (899.99, 900), (900, 1800), (1799, 1800), (1800, 2700)])
def test_next_quarter(now, expected):
    assert next_quarter(now) == expected


def test_scheduler_restart_no_backfill_and_clock_jump(db):
    async def scenario():
        now = [901.0]
        slots = []
        async def sleep(seconds):
            now[0] += seconds
        collector = Collector(db, "http://esp32/api/status", clock=lambda: now[0], sleep=sleep, client=object())
        async def collect(slot):
            slots.append(slot)
            if len(slots) == 1:
                now[0] = 5401  # Nach langem Prozessstillstand nichts nachfüllen.
            else:
                raise asyncio.CancelledError()
        collector.collect = collect
        with pytest.raises(asyncio.CancelledError):
            await collector.run()
        assert slots == [1800, 6300]
        restarted = Collector(db, "http://esp32/api/status", clock=lambda: 6400)
        assert restarted.next_capture == 7200
    asyncio.run(scenario())


def test_storage_error_remains_visible(db, payload, monkeypatch):
    async def scenario():
        def fail(*args):
            raise OSError("disk full")
        monkeypatch.setattr(db, "insert", fail)
        async with httpx.AsyncClient(transport=httpx.MockTransport(lambda _: httpx.Response(200, json=payload))) as client:
            collector = Collector(db, "http://esp32", clock=lambda: 900, client=client)
            await collector.collect(900)
            assert collector.status()["last_result"]["code"] == "storage_error"
            assert db.latest() is None
    asyncio.run(scenario())
