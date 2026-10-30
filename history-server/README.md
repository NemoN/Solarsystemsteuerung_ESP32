# Backload Temperaturarchiv

Dieser Dienst liest die vorhandene ESP32-Schnittstelle und speichert alle
15 Minuten die drei Temperaturen in SQLite. Die ESP32-Firmware bleibt unverändert.
Ein Container enthält Sammler, API und Webseite; ein benanntes Docker-Volume
enthält die Datenbank. Ein geöffnetes Browserfenster ist für die Erfassung nicht nötig.

## Start auf dem Docker-Server

Voraussetzung: Docker Engine mit Docker Compose v2. Der Server muss den ESP32 im
Heimnetz erreichen können und eine korrekt synchronisierte Uhr haben.

Den gesamten Ordner `history-server/` auf den Server kopieren und dort ausführen:

```sh
cd history-server
cp .env.example .env
```

In `.env` bei Bedarf die beiden Werte anpassen:

```dotenv
ESP32_STATUS_URL=http://192.168.177.83/api/status
HISTORY_PORT=8088
```

Anschließend:

```sh
docker compose up -d --build
docker compose ps
docker compose logs --tail=50 history
```

Webseite: **http://SERVER-IP:8088**. Für einen anderen Port `HISTORY_PORT` ändern
und `docker compose up -d` erneut ausführen. Die Webseite hat keine Anmeldung;
der Dienst ist für das private Heimnetz gedacht. Chart.js und alle anderen
Browserdateien werden lokal ausgeliefert, ohne CDN oder externe Schriftarten.
Internet wird beim Bau des Containers für Basisimage und Python-Pakete benötigt.

`restart: unless-stopped` startet den Dienst nach einem Serverneustart wieder,
sofern Docker selbst automatisch startet. Nach einem absichtlichen
`docker compose stop` erneut mit `docker compose up -d` starten.

## Was gespeichert wird

- Zur nächsten Viertelstunde `:00`, `:15`, `:30`, `:45` wird genau eine
  Momentaufnahme abgefragt. Es gibt keine Mittelwertbildung und keinen Sofortwert
  beim Start. Bis zur ersten Viertelstunde ist das Archiv daher leer.
- IDs statt Namen oder Reihenfolge bestimmen die Zuordnung: **0 Kombispeicher
  oben**, **1 Kombispeicher Mitte**, **2 Pufferspeicher oben**. Die Firmware darf
  ID 2 weiterhin „Pufferspeicher“ nennen.
- Der Server vergibt den UTC-Zeitstempel des Viertelstundentermins. Die tatsächliche
  Abfragezeit wird zusätzlich gespeichert. Der Firmware-Zeitstempel wird nicht verwendet.
- Das Zeitlimit für die gesamte HTTP-Abfrage beträgt **fünf Sekunden**.
- `valid: false`, Fehlercodes, fehlende oder doppelte IDs sowie ungültige
  Temperaturen werden je Sensor als Fehler gespeichert; andere gültige Sensoren
  bleiben erhalten. Erwartet werden numerische, endliche Werte von −200 bis
  +850 °C, passend zum RTD-Messbereich. Die bisherige Fehleranzeige −242 °C
  wird damit ebenfalls nicht als Temperatur übernommen.
- Bei einem nicht erreichbaren ESP32, HTTP-Fehler oder ungültiger Antwort wird
  ein Ausfalldatensatz mit drei leeren Temperaturfeldern gespeichert.
- Während der Server aus ist, entstehen keine Datensätze. Nach dem Start geht es
  zur nächsten Viertelstunde weiter. Fehlende Werte werden nie nachgefüllt.
- Ein eindeutiger Datenbankschlüssel verhindert doppelte Viertelstundenwerte.
  Ein bereits gespeicherter Datensatz wird nicht durch einen zweiten überschrieben.
- Es gibt **keine automatische Löschung**. Der Speicherbedarf wächst über die
  Jahre; pro Jahr entstehen normalerweise 35.040 Datensätze (Schaltjahr: 35.136).

Die Webseite zeigt Datum und Uhrzeit in **Europe/Berlin** mit Sommer-/Winterzeit.
Die wiederholte Stunde bei der Herbstumstellung bleibt über UTC eindeutig.
Tooltips und CSV zeigen die Zeitzone beziehungsweise den UTC-Versatz.

## Diagramm und CSV

Startansicht sind die letzten sieben Tage. Alternativ: 24 Stunden, 30 Tage,
365 Tage oder ein eigener Zeitraum. Eigene Anfangs- und Enddaten gelten als ganze
Kalendertage in Berlin; das Enddatum ist eingeschlossen, auch an Tagen mit
23 oder 25 Stunden. Die drei Kurven lassen sich einzeln ausblenden.

Sensorfehler und fehlende Viertelstunden unterbrechen die jeweilige Linie.
Wenn der ESP32 im Testmodus läuft, werden die simulierten Werte nicht als
Messungen gespeichert; die Erfassung kennzeichnet diesen Zeitraum als Testmodus.
Für lange Zeiträume fasst ausschließlich die Diagrammansicht Punkte durch
Min/Max-Auswahl zusammen. Anfang, Ende und Extremwerte der Zeitfenster sowie
Fehlerlücken bleiben erhalten. Es werden keine Temperaturen gemittelt.
Datenbank und CSV enthalten weiter alle Originalwerte.

**CSV herunterladen** exportiert den gewählten Zeitraum unabhängig von den
eingeblendeten Kurven. Format: UTF-8 mit BOM, Semikolon, Dezimalkomma, UTC-Zeit,
Berliner Zeit mit Versatz, drei Temperaturen, Status je Sensor, Abfrageergebnis,
Fehlerhinweis und tatsächliche Abfragezeit. Leere Temperaturfelder sind Fehler
und keine Null Grad. Ausfallzeilen stehen im Export; bei ausgeschaltetem Server
fehlen die entsprechenden Zeitpunkte vollständig.

Verlauf und Erfassungsstatus werden auf der Webseite jede Minute aktualisiert.
Der Archivdienst ruft den ESP32 weiterhin nur alle 15 Minuten auf.

## Lesende Schnittstellen

| Endpunkt | Inhalt |
| --- | --- |
| `GET /api/history` | JSON: drei Kurven, Zähler und Anzeigeauflösung |
| `GET /api/export.csv` | Vollständiger CSV-Export |
| `GET /api/collector-status` | Letzte gespeicherte Erfassung, letzte Abfrage/Ergebnis, nächster Termin |
| `GET /healthz` | Dienst erreichbar; ein ESP32-Ausfall macht den Container nicht ungesund |

Verlauf und CSV akzeptieren `from` und `to`. Ohne Zeitraum: letzte sieben Tage.
Beispiele:

```text
/api/history?from=2026-10-01&to=2026-10-25
/api/export.csv?from=2026-10-25T00:00:00Z&to=2026-10-26T00:00:00Z
```

Datumswerte sind ganze Berliner Tage, ISO-Zeitstempel benötigen eine Zeitzone
und verwenden eine **exklusive** obere Grenze. Bei `+01:00` in einer URL muss
das Plus als `%2B` codiert werden. Ungültige Zeiträume liefern HTTP 400.
Diagrammpunkte verwenden Unix-Millisekunden, fehlende Werte `null`.
`sample_count` zählt Originaldatensätze, `display_reduced` kennzeichnet eine
ausgedünnte Darstellung. Synthetische Lückenmarker existieren nur im Diagramm.

## Daten erhalten und aktualisieren

Die Daten liegen im Volume **`backload-history_history-data`**, nicht im Container.
Den Compose-Projektnamen und die Volume-Zuordnung beim Betrieb beibehalten.
Bei `docker compose -p ANDERER-NAME` wird ein anderes Volume verwendet.

Ein neues Image bauen und den Container ersetzen:

```sh
docker compose up -d --build --force-recreate
```

Auch `docker compose down` und anschließendes `up -d` erhalten das Volume.
**`docker compose down -v` löscht das Archiv-Volume.** Ein Volume schützt vor
Containerwechsel, aber nicht vor einem Defekt des Servers: Sicherungen außerhalb
dieses Servers aufbewahren. Siehe [Docker: Volumes](https://docs.docker.com/engine/storage/volumes/).

## Datensicherung

Eine konsistente SQLite-Sicherung kann bei laufendem Dienst erstellt werden:

```sh
docker compose exec -T history python -m app.backup export > backload-backup.sqlite3
```

Exitcode prüfen und die Datei auf einen anderen Rechner kopieren. Die Sicherung
nutzt die SQLite-Backup-API; die gerade geöffnete Datenbankdatei muss nicht mit
`cp` kopiert werden. Der Export enthält auch Fehlermeldungen und Zeitstempel.
CSV ist für Auswertung gedacht; die SQLite-Sicherung ist für Wiederherstellung.

## Wiederherstellung

Die Wiederherstellung ersetzt das Archiv vollständig durch die Sicherung.
Vorher bei Bedarf den aktuellen Stand wie oben sichern. Den Dienst stoppen:

```sh
docker compose stop history
docker compose run --rm -T --no-deps history python -m app.backup restore < backload-backup.sqlite3
docker compose up -d history
```

Die Sicherung wird vor dem Ersetzen auf Integrität, Schema und Version geprüft.
Eine ungültige Datei lässt die vorhandene Datenbank unverändert. Eine Sperre
verhindert den Restore, solange für dieses Volume noch ein Sammler läuft.
Bleibt nach einem abgebrochenen Prozess ein SQLite-Journal liegen, den Dienst
zuerst einmal sauber starten und stoppen; die Wiederherstellung überspringt
keine offene SQLite-Transaktion.

## Entwicklung und Prüfungen

Ab Python 3.13, ohne Docker:

```sh
python3 -m venv .venv
.venv/bin/pip install -r requirements-dev.txt
.venv/bin/python -m pytest -q
DATABASE_PATH=./temperatures.sqlite3 .venv/bin/uvicorn app.main:app --port 8088
```

Der Container nutzt Python 3.13 und einen Prozess. Nicht mit mehreren Uvicorn-
Workern oder mit mehreren Containern dasselbe Volume sammeln lassen; die
Dateisperre verhindert versehentliche Doppelstarts. SQLite nutzt kurze
Transaktionen mit Rollback-Journal und 30 Sekunden Wartezeit auf Datenbanksperren.
Die CSV-Datei wird vor der Übertragung erzeugt, damit langsame Downloads keine
DB-Lesesperre offen halten.

Automatisierte Prüfungen im gleichen Basisimage:

```sh
docker build --target test -t backload-history-tests .
docker run --rm backload-history-tests
```

Zusätzlicher Container-/Volume-Test auf einem Docker-fähigen Rechner:

```sh
python3 tests/check_container.py
```

Dieser Test baut das Image, erzeugt ein zufällig benanntes Testprojekt mit eigenem
Volume, speichert einen Testwert, ersetzt den Container und prüft Backup/Restore.
Er entfernt anschließend ausschließlich sein eigenes Testprojekt samt Test-Volume.
Das produktive Archiv und der echte ESP32 werden nicht angesprochen.

Die Tests verwenden ausschließlich temporäre Datenbanken und simulierte
ESP32-Antworten. Sie prüfen Terminberechnung/Neustart, Duplikate, Sensorfehler,
Ausfälle/Wiederverbindung, Gesamtzeitlimit, Zeitumstellung, Filter, Kurvenlücken,
35.040 Jahreszeitpunkte mit vollständigem CSV sowie Backup/Restore.

Der tatsächlich ausgeführte Prüfstand und die noch offene ESP32-Live-Prüfung
stehen in [VERIFICATION.md](VERIFICATION.md).
