# Backload Temperaturarchiv

Der History-Server speichert alle 15 Minuten drei Temperaturen und, sofern die
Firmware sie liefert, die Pumpenlaufzeiten in einer SQLite-Datenbank. Die
Webseite zeigt den Verlauf und bietet einen CSV-Export. Zur Erfassung muss kein
Browser geöffnet sein.

Der Rechner benötigt eine synchronisierte Uhr und eine Verbindung zum ESP32.
Die Webseite hat keine Anmeldung und ist für das private Heimnetz gedacht.
Alle folgenden Befehle werden im Ordner `history-server` ausgeführt.

## Mit Docker

Voraussetzung sind Docker Engine und Docker Compose v2. Den Ordner
`history-server` auf den Server kopieren und die Konfiguration anlegen:

```sh
cd history-server
cp .env.example .env
```

In `.env` die ESP32-Adresse und bei Bedarf den Port anpassen. Dann starten:

```sh
docker compose up -d --build
docker compose logs --tail=50 history
```

Die Webseite ist unter `http://SERVER-IP:8088` erreichbar; bei geändertem
`HISTORY_PORT` entsprechend unter diesem Port. Docker startet den Dienst nach
einem Neustart wieder, sofern Docker selbst automatisch startet. Ein Update wird
nach dem Ersetzen der Projektdateien mit `docker compose up -d --build` gebaut.

Die Datenbank liegt im Volume `backload-history_history-data`. Ein anderer
Compose-Projektname erzeugt ein anderes Volume. **`docker compose down -v`
löscht das Archiv.**

## Ohne Docker

Voraussetzung sind Linux oder macOS und Python 3.13. Für die Installation der
Python-Pakete wird Internet benötigt. Den Ordner `history-server` auf den
Rechner kopieren und darin ausführen:

```sh
cd history-server
python3.13 -m venv .venv
.venv/bin/python -m pip install -r requirements.txt
mkdir -p data
```

Die Adresse durch die des eigenen ESP32 ersetzen und den Server starten:

```sh
export ESP32_STATUS_URL=http://192.168.177.83/api/status
export DATABASE_PATH="$PWD/data/temperatures.sqlite3"
.venv/bin/python -m uvicorn app.main:app --host 0.0.0.0 --port 8088 --workers 1
```

Die Webseite ist unter `http://SERVER-IP:8088` erreichbar. `--port` ändert den
Port; `--host 127.0.0.1` beschränkt den Zugriff auf den lokalen Rechner. Mit
Strg+C wird der Server beendet. Die beiden Umgebungsvariablen müssen in jeder
neuen Shell gesetzt werden; `.env` wird nur von Docker Compose eingelesen.
Das Datenverzeichnis muss für den ausführenden Benutzer beschreibbar sein.

Nach einem Update der Projektdateien die Abhängigkeiten erneut installieren und
den Server neu starten:

```sh
.venv/bin/python -m pip install -r requirements.txt
```

### Automatischer Start unter Linux

Die Vorlage [backload-history.service.example](backload-history.service.example)
als systemd-Dienst installieren:

```sh
sudo cp backload-history.service.example /etc/systemd/system/backload-history.service
sudoedit /etc/systemd/system/backload-history.service
```

In der Datei `BENUTZER`, alle Vorkommen von `/pfad/zu/history-server` und die
ESP32-Adresse anpassen. Das Datenverzeichnis sowie eine bereits vorhandene
Datenbank und Sperrdatei müssen für den Dienstbenutzer beschreibbar sein. Das
Verzeichnis benötigt außerdem Suchrechte, damit SQLite dort Journaldateien
anlegen kann. Bei der obigen Einrichtung beispielsweise (hier `BENUTZER` durch
den in der Dienstdatei eingetragenen Benutzer ersetzen):

```sh
sudo chown BENUTZER data
sudo chmod u+rwx data
for datei in data/temperatures.sqlite3 data/temperatures.lock; do
  if [ -e "$datei" ]; then
    sudo chown BENUTZER "$datei"
    sudo chmod u+rw "$datei"
  fi
done
```

Bei einem anderen `DATABASE_PATH` das zugehörige Verzeichnis und die Dateien
entsprechend anpassen. Bei einem bereits geladenen Dienst zeigt
`systemctl show backload-history.service -p User` den eingestellten Benutzer.
Danach den Dienst aktivieren:

```sh
sudo systemctl daemon-reload
sudo systemctl enable --now backload-history
systemctl status backload-history
journalctl -u backload-history -f
```

Nach Änderungen an der Dienstdatei `daemon-reload` und anschließend
`sudo systemctl restart backload-history` ausführen. Nach einem Update der
Projektdateien und Python-Pakete den Dienst ebenfalls neu starten.

Pro Datenbank darf nur ein Sammler laufen. Deshalb weder mehrere Server noch
mehrere Uvicorn-Worker oder den Entwicklungsmodus `--reload` verwenden.

## Daten und Verlauf

- Gespeichert werden Kombispeicher oben und Mitte sowie Pufferspeicher oben.
  Die Erfassung beginnt zur nächsten Viertelstunde. Sensorfehler und Ausfälle
  werden gekennzeichnet; während eines Serverausfalls fehlen die Messungen.
- Simulierte Werte im Testmodus werden nicht als Messungen gespeichert. Daten
  werden nicht automatisch gelöscht. Die Anzeige verwendet die Zeitzone Berlin.
- Pumpenlaufzeiten sind nach Tag, Monat und Jahr für die Solarpumpe und beide
  Förderrichtungen der Backloadpumpe abrufbar. Der ESP32 hält die letzten 32 Tage,
  24 Monate und 12 Jahre mit Daten vor; längere Serverausfälle können daher zu
  Lücken führen. Die Werte beschreiben Relaislaufzeiten, keinen Wasserfluss.
- Das Diagramm kann für lange Zeiträume Punkte reduzieren; der CSV-Export enthält
  alle Werte des gewählten Zeitraums. CSV dient der Auswertung, nicht als Backup.

## Sicherung und Wiederherstellung

Sicherungen auf einem anderen Rechner aufbewahren. Der Export erstellt auch bei
laufendem Server eine konsistente SQLite-Sicherung. Die Wiederherstellung ersetzt
das gesamte Archiv; vorher bei Bedarf den aktuellen Stand sichern. Eine ungültige
Sicherung wird vor dem Ersetzen zurückgewiesen.

Mit Docker:

```sh
docker compose exec -T history python -m app.backup export > backload-backup.sqlite3
```

Zum Wiederherstellen:

```sh
docker compose stop history
docker compose run --rm -T --no-deps history python -m app.backup restore < backload-backup.sqlite3
docker compose up -d history
```

Ohne Docker `DATABASE_PATH` auf dieselbe Datenbank wie beim Server setzen und
sichern. Die Befehle als derselbe Benutzer ausführen, unter dem der Server läuft,
damit die Datenbank nach einer Wiederherstellung beschreibbar bleibt:

```sh
export DATABASE_PATH="$PWD/data/temperatures.sqlite3"
.venv/bin/python -m app.backup export > backload-backup.sqlite3
```

Für die Wiederherstellung zuerst den Server mit Strg+C oder bei systemd mit
`sudo systemctl stop backload-history` stoppen. Dann ausführen:

```sh
.venv/bin/python -m app.backup restore < backload-backup.sqlite3
```

Anschließend den Server wieder starten, bei systemd mit
`sudo systemctl start backload-history`. Die Sperrdatei verhindert eine
Wiederherstellung, solange der Sammler läuft.

## Schnittstellen und Tests

`/api/history` liefert den Verlauf, `/api/export.csv` den CSV-Export,
`/api/pump-runtime?kind=day` die Pumpenlaufzeiten (`day`, `month`, `year`) und
`/api/collector-status` den Erfassungsstatus. `/healthz` prüft nur den Dienst,
nicht die Verbindung zum ESP32.

Tests ohne Docker:

```sh
.venv/bin/python -m pip install -r requirements-dev.txt
.venv/bin/python -m pytest -q
```

Weitere Angaben zum Prüfstand stehen in [VERIFICATION.md](VERIFICATION.md).
