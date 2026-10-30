import csv
from datetime import date, datetime, timedelta, timezone
import math
import tempfile
from zoneinfo import ZoneInfo

from .collector import utc_iso
from .config import INTERVAL, SENSORS
from .models import RESULT_MESSAGES, SENSOR_MESSAGES

BERLIN = ZoneInfo("Europe/Berlin")


def parse_boundary(value: str | None, *, end=False):
    if value is None:
        return None
    try:
        if len(value) == 10:
            day = date.fromisoformat(value)
            if end:
                day += timedelta(days=1)
            return datetime.combine(day, datetime.min.time(), BERLIN).timestamp()
        timestamp = datetime.fromisoformat(value.replace("Z", "+00:00"))
        if timestamp.tzinfo is None:
            raise ValueError("Zeitzone fehlt")
        return timestamp.timestamp()
    except (ValueError, OverflowError) as exc:
        raise ValueError("Zeitraum als YYYY-MM-DD oder ISO-Zeitstempel mit Zeitzone angeben.") from exc


def resolve_range(start: str | None, end: str | None, now: float):
    stop = parse_boundary(end, end=True)
    stop = now if stop is None else stop
    begin = parse_boundary(start)
    begin = stop - 7 * 86400 if begin is None else begin
    if not math.isfinite(begin) or not math.isfinite(stop) or begin >= stop:
        raise ValueError("Der Beginn muss vor dem Ende des Zeitraums liegen.")
    return begin, stop


class Curve:
    """Min/Max pro Zeitfenster, jede zusammenhängende Messfolge separat.

    Nullpunkte werden nie durch einen gültigen Wert ersetzt. Auch sehr kurze
    Ausfälle bleiben daher sichtbar, unabhängig von der gewählten Auflösung.
    """
    def __init__(self, width: int):
        self.width = width
        self.points = []
        self.bucket = None
        self.first = self.last = self.low = self.high = None
        self.previous_timestamp = None
        self.reduced = False
        self.bucket_count = 0

    def flush(self):
        if self.first is None:
            return
        ordered = sorted({p[0]: p for p in (self.first, self.low, self.high, self.last)}.values())
        self.points.extend({"x": x * 1000, "y": y} for x, y in ordered)
        self.reduced |= self.bucket_count > len(ordered)
        self.first = self.last = self.low = self.high = None
        self.bucket_count = 0

    def gap(self, timestamp):
        self.flush()
        # Zusammenhängende Fehler brauchen nur einen Marker, keinen Nullpunkt je Viertelstunde.
        if self.points and self.points[-1]["y"] is not None:
            self.points.append({"x": timestamp * 1000, "y": None})

    def add(self, timestamp, value):
        if self.previous_timestamp is not None and timestamp - self.previous_timestamp > INTERVAL:
            self.gap(self.previous_timestamp + INTERVAL)
        self.previous_timestamp = timestamp
        if value is None:
            self.gap(timestamp)
            return
        bucket = timestamp // self.width
        if self.first is not None and self.bucket != bucket:
            self.flush()
        self.bucket = bucket
        point = (timestamp, value)
        if self.first is None:
            self.first = self.low = self.high = point
        self.last = point
        self.low = point if value < self.low[1] else self.low
        self.high = point if value > self.high[1] else self.high
        self.bucket_count += 1


def build_history(database, start, end):
    width = INTERVAL if end - start <= 1200 * INTERVAL else math.ceil((end - start) / (1200 / 4) / INTERVAL) * INTERVAL
    curves = [Curve(width) for _ in SENSORS]
    count, valid_count = 0, 0
    for row in database.rows(start, end):
        count += 1
        for sensor_id, curve in enumerate(curves):
            value = row[f"t{sensor_id}"]
            valid_count += value is not None
            curve.add(row["timestamp_utc"], value)
    for curve in curves:
        curve.flush()
    return {
        "from": utc_iso(start), "to": utc_iso(end), "timezone": "Europe/Berlin",
        "interval_seconds": INTERVAL, "sample_count": count, "valid_value_count": valid_count,
        "display_reduced": any(curve.reduced for curve in curves),
        "series": [
            {"id": sensor_id, "label": label, "color": color, "points": curve.points}
            for (sensor_id, label, color), curve in zip(SENSORS, curves)
        ],
    }


def csv_file(database, start, end):
    # Vor der Übertragung materialisieren: ein langsamer Download hält keine DB-Lesesperre.
    output = tempfile.TemporaryFile(mode="w+", encoding="utf-8", newline="")
    try:
        output.write("\ufeff")
        writer = csv.writer(output, delimiter=";", lineterminator="\r\n")
        writer.writerow([
            "Zeit UTC", "Zeit Europe/Berlin", "Kombispeicher oben (°C)",
            "Kombispeicher Mitte (°C)", "Pufferspeicher oben (°C)",
            "Status Kombispeicher oben", "Status Kombispeicher Mitte", "Status Pufferspeicher oben",
            "Abfrageergebnis", "Fehlerhinweis", "Abgefragt UTC",
        ])
        for row in database.rows(start, end):
            local = datetime.fromtimestamp(row["timestamp_utc"], timezone.utc).astimezone(BERLIN).isoformat(timespec="seconds")
            writer.writerow([
                utc_iso(row["timestamp_utc"]), local,
                *["" if row[f"t{i}"] is None else str(row[f"t{i}"]).replace(".", ",") for i in range(3)],
                *[SENSOR_MESSAGES.get(row[f"s{i}"], "Unbekannter Fehler") for i in range(3)],
                RESULT_MESSAGES.get(row["result"], "Unbekannter Fehler"), row["detail"], utc_iso(row["observed_at_utc"]),
            ])
        output.seek(0)
        return output
    except BaseException:
        output.close()
        raise


def stream_csv(handle):
    try:
        while chunk := handle.read(65536):
            yield chunk.encode("utf-8")
    finally:
        handle.close()
