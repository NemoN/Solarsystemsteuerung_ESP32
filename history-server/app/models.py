from dataclasses import dataclass
from datetime import date
import math

SENSOR_MESSAGES = {
    "ok": "OK",
    "sensor_error": "Sensorfehler",
    "missing": "Sensor fehlt",
    "invalid": "Ungültiger Sensorwert",
    "duplicate": "Sensor-ID mehrfach vorhanden",
    "unavailable": "ESP32 nicht erreichbar",
    "invalid_response": "Ungültige ESP32-Antwort",
}
RESULT_MESSAGES = {
    "ok": "Alle Sensoren gelesen",
    "partial": "Einzelne Sensorwerte fehlen",
    "sensor_error": "Keine gültigen Sensorwerte",
    "timeout": "ESP32: Zeitlimit überschritten",
    "unreachable": "ESP32 nicht erreichbar",
    "http_error": "ESP32: HTTP-Fehler",
    "invalid_response": "Ungültige ESP32-Antwort",
    "storage_error": "Datenbank konnte nicht geschrieben werden",
    "simulation": "ESP32 im Testmodus",
}


@dataclass(frozen=True)
class RuntimePeriod:
    kind: str
    period: int
    solar: int
    combi_to_buffer: int
    buffer_to_combi: int


def parse_runtime(payload) -> tuple[RuntimePeriod, ...]:
    if not isinstance(payload, dict) or payload.get("available") is not True:
        return ()
    records = []
    for kind, key, maximum in (("day", "days", 32), ("month", "months", 24), ("year", "years", 12)):
        rows = payload.get(key)
        if not isinstance(rows, list) or len(rows) > maximum:
            continue
        seen = set()
        for row in rows:
            if not isinstance(row, dict):
                continue
            period = row.get("period")
            values = [row.get(name) for name in ("solar", "combiToBuffer", "bufferToCombi")]
            if type(period) is not int or period in seen or any(type(v) is not int or not 0 <= v <= 0xffffffff for v in values):
                continue
            try:
                if kind == "day":
                    date(period // 10000, (period // 100) % 100, period % 100)
                elif kind == "month":
                    date(period // 100, period % 100, 1)
                else:
                    date(period, 1, 1)
            except ValueError:
                continue
            seen.add(period)
            records.append(RuntimePeriod(kind, period, *values))
    return tuple(records)


@dataclass(frozen=True)
class Reading:
    temperatures: tuple[float | None, ...]
    sensor_status: tuple[str, ...]
    result: str
    detail: str = ""
    runtime: tuple[RuntimePeriod, ...] = ()

    @classmethod
    def failure(cls, result: str, detail: str = ""):
        status = "invalid_response" if result in ("invalid_response", "http_error") else "unavailable"
        return cls((None,) * 3, (status,) * 3, result, detail)


def parse_reading(payload) -> Reading:
    if not isinstance(payload, dict) or not isinstance(payload.get("controllers"), list):
        return Reading.failure("invalid_response", "Liste controllers fehlt.")
    if payload.get("simulation") is True:
        return Reading.failure("simulation", "Simulierte Werte werden nicht archiviert.")
    found = {}
    duplicates = set()
    for item in payload["controllers"]:
        if not isinstance(item, dict):
            continue
        sensor_id = item.get("id")
        if type(sensor_id) is not int or sensor_id not in (0, 1, 2):
            continue
        if sensor_id in found:
            duplicates.add(sensor_id)
        found[sensor_id] = item
    values, states = [], []
    for sensor_id in range(3):
        item = found.get(sensor_id)
        value, state = None, "missing"
        if sensor_id in duplicates:
            state = "duplicate"
        elif item is not None:
            temperature = item.get("temperature")
            if item.get("valid") is not True or item.get("fault", 0) != 0 or item.get("sensorError"):
                state = "sensor_error"
            elif (type(temperature) not in (int, float) or not math.isfinite(temperature)
                  or not -200 <= temperature <= 850):
                # MAX31865/PT100/PT1000: defensiver Messbereich, keine Sentinel-Werte.
                state = "invalid"
            else:
                value, state = float(temperature), "ok"
        values.append(value)
        states.append(state)
    count = states.count("ok")
    return Reading(tuple(values), tuple(states), "ok" if count == 3 else "partial" if count else "sensor_error", runtime=parse_runtime(payload.get("pumpRuntime")))
