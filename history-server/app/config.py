from dataclasses import dataclass
import os
from pathlib import Path
from urllib.parse import urlsplit

INTERVAL = 15 * 60
TIMEOUT = 5.0
SENSORS = (
    (0, "Kombispeicher oben", "#17636b"),
    (1, "Kombispeicher Mitte", "#955018"),
    (2, "Pufferspeicher oben", "#6855a0"),
)


@dataclass(frozen=True)
class Settings:
    source_url: str = "http://192.168.177.83/api/status"
    database_path: Path = Path("/data/temperatures.sqlite3")

    def __post_init__(self):
        url = urlsplit(self.source_url)
        if url.scheme not in ("http", "https") or not url.hostname or url.username or url.password or url.fragment or any(c.isspace() for c in self.source_url):
            raise ValueError("ESP32_STATUS_URL muss eine HTTP(S)-Adresse ohne Zugangsdaten sein.")
        try:
            url.port
        except ValueError as exc:
            raise ValueError("ESP32_STATUS_URL enthält einen ungültigen Port.") from exc

    @classmethod
    def from_env(cls):
        return cls(
            os.getenv("ESP32_STATUS_URL", cls.source_url),
            Path(os.getenv("DATABASE_PATH", str(cls.database_path))),
        )
