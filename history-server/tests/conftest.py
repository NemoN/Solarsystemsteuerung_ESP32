import pytest
from app.database import Database


@pytest.fixture
def db(tmp_path):
    database = Database(tmp_path / "archive.sqlite3")
    database.initialize()
    return database


@pytest.fixture
def payload():
    return {"controllers": [
        {"id": 2, "name": "Pufferspeicher", "temperature": 49.6, "valid": True, "fault": 0, "sensorError": None},
        {"id": 0, "name": "Kombispeicher oben", "temperature": 71.5, "valid": True},
        {"id": 1, "name": "Kombispeicher Mitte", "temperature": 51.2, "valid": True},
    ]}
