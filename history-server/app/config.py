from dataclasses import dataclass
import os
from pathlib import Path
from urllib.parse import urlsplit, urlunsplit

INTERVAL = 15 * 60
TIMEOUT = 5.0
SENSORS = (
    (0, "Kombispeicher oben", "#53dec6"),
    (1, "Kombispeicher Mitte", "#f6bd68"),
    (2, "Pufferspeicher oben", "#ae9bf5"),
)


@dataclass(frozen=True)
class Settings:
    source_url: str = "http://192.168.177.83/api/status"
    database_path: Path = Path("/data/temperatures.sqlite3")

    def __post_init__(self):
        url = urlsplit(self.source_url)
        if url.scheme not in ("http", "https") or not url.hostname or url.username is not None or url.password is not None or url.fragment or any(c.isspace() for c in self.source_url):
            raise ValueError("ESP32_STATUS_URL muss eine HTTP(S)-Adresse ohne Zugangsdaten sein.")
        try:
            url.port
        except ValueError as exc:
            raise ValueError("ESP32_STATUS_URL enthält einen ungültigen Port.") from exc

    @property
    def dashboard_url(self):
        url = urlsplit(self.source_url)
        return urlunsplit((url.scheme, url.netloc, "/", "", ""))

    @classmethod
    def from_env(cls):
        return cls(
            os.getenv("ESP32_STATUS_URL", cls.source_url),
            Path(os.getenv("DATABASE_PATH", str(cls.database_path))),
        )
