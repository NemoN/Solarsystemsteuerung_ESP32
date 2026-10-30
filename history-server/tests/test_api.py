from datetime import datetime, timezone
import time

from fastapi.testclient import TestClient
import httpx
import pytest

from app.config import Settings
from app.main import create_app
from app.models import Reading


@pytest.fixture
def client(tmp_path):
    settings = Settings("http://esp32/api/status", tmp_path / "test.sqlite3")
    app = create_app(settings, start_collector=False, clock=lambda: 1800)
    with TestClient(app) as client:
        yield client


def test_api_empty_status_and_static_offline_assets(client):
    response = client.get("/")
    assert response.status_code == 200
    assert "Temperaturarchiv" in response.text
    assert "default-src 'self'" in response.headers["content-security-policy"]
    assert client.get("/static/vendor/chart.umd.min.js").status_code == 200
    assert client.get("/healthz").json() == {"status": "ok"}
    status = client.get("/api/collector-status").json()
    assert status["last_capture"] is None
    assert status["next_capture"] == "1970-01-01T00:45:00Z"
    assert client.get("/api/history").json()["sample_count"] == 0


def test_partial_values_status_filters_and_csv(client):
    reading = Reading((71.5, None, 49.6), ("ok", "sensor_error", "ok"), "partial")
    client.app.state.database.insert(900, 901, reading)
    data = client.get("/api/history", params={"from": "1970-01-01T00:15:00Z", "to": "1970-01-01T00:30:00Z"}).json()
    assert data["sample_count"] == 1
    assert data["series"][0]["points"] == [{"x": 900000, "y": 71.5}]
    assert data["series"][1]["points"] == []
    assert data["series"][2]["label"] == "Pufferspeicher oben"
    status = client.get("/api/collector-status").json()
    assert status["last_capture"] == "1970-01-01T00:15:00Z"
    assert status["last_result"]["code"] == "partial"
    csv = client.get("/api/export.csv")
    assert csv.status_code == 200
    assert csv.content.startswith(b"\xef\xbb\xbf")
    assert "71,5;;49,6" in csv.text
    assert "attachment" in csv.headers["content-disposition"]
    assert client.post("/api/history").status_code == 405


@pytest.mark.parametrize("params", [
    {"from": "bad"}, {"from": "2026-10-25T02:15:00"},
    {"from": "2026-10-26", "to": "2026-10-25"},
    {"from": "9999-12-31", "to": "9999-12-31"},
])
def test_invalid_periods_return_400(client, params):
    for endpoint in ("/api/history", "/api/export.csv"):
        assert client.get(endpoint, params=params).status_code == 400


def test_same_custom_date_includes_both_dst_hours(tmp_path):
    now = datetime(2026, 10, 26, tzinfo=timezone.utc).timestamp()
    app = create_app(Settings("http://esp32", tmp_path / "dst.sqlite3"), start_collector=False, clock=lambda: now)
    with TestClient(app) as client:
        for hour in (0, 1):
            slot = int(datetime(2026, 10, 25, hour, 15, tzinfo=timezone.utc).timestamp())
            client.app.state.database.insert(slot, slot, Reading((50, 51, 52), ("ok",) * 3, "ok"))
        response = client.get("/api/history", params={"from": "2026-10-25", "to": "2026-10-25"})
        assert response.status_code == 200
        assert response.json()["sample_count"] == 2


def test_running_collector_lifespan_capture_and_restart(tmp_path, payload):
    now = [899.0]
    calls = []
    def source(request):
        calls.append(str(request.url))
        return httpx.Response(200, json=payload)
    source_client = httpx.AsyncClient(transport=httpx.MockTransport(source))
    settings = Settings("http://esp32/api/status", tmp_path / "live.sqlite3")
    app = create_app(settings, client=source_client, clock=lambda: now[0])
    with TestClient(app) as client:
        assert client.get("/api/collector-status").json()["last_capture"] is None
        assert calls == []  # Kein Sofortwert außerhalb eines Viertelstundentermins.
        now[0] = 900.0
        deadline = time.monotonic() + 4
        while time.monotonic() < deadline:
            status = client.get("/api/collector-status").json()
            if status["last_capture"] is not None:
                break
            time.sleep(.02)
        assert status["last_capture"] == "1970-01-01T00:15:00Z"
        assert status["last_result"]["code"] == "ok"
        assert calls == ["http://esp32/api/status"]
        assert status["next_capture"] == "1970-01-01T00:30:00Z"
    now[0] = 901.0
    restarted = create_app(settings, client=source_client, clock=lambda: now[0])
    with TestClient(restarted) as client:
        assert client.get("/api/collector-status").json()["last_capture"] == "1970-01-01T00:15:00Z"
        assert client.get("/api/history").json()["sample_count"] == 1
        assert len(calls) == 1
