# Prüfstand vom 25.10.2026

## Erfolgreich geprüft

- **38 automatisierte Tests mit Python 3.13**: einschließlich laufender
  FastAPI-Lifespan-Erfassung und anschließendem Neustart; Terminberechnung,
  keine Sofortmessung/kein Nachfüllen, eindeutige Datensätze, unabhängige
  Sensorfehler, HTTP-/Verbindungs-/JSON-Fehler und Erholung, Gesamtzeitlimit,
  Datenbankfehler, Zeitumstellung, Zeitraumfilter, Kurvenlücken und Jahresdaten.
- **35.040 Originalzeitpunkte**: Diagrammansicht ausgedünnt, Extremwert und
  einzelne Ausfalllücke erhalten; CSV mit exakt 35.040 Datenzeilen.
- **Chrome/Playwright**, Desktop 1440 px sowie Mobil 390 und 320 px:
  Sieben-Tage- und Jahresansicht, drei Kurvenschalter, eigener Zeitraum,
  Leerzustand, CSV-Download mit 35.040 Datenzeilen; zusätzlich Fehlerzustand
  mit erfolgreichem Wiederholen. Kein horizontaler Overflow, keine
  unbeabsichtigten JavaScript-Fehler und keine externen Browseranfragen.
- **Backup/Restore über die tatsächliche CLI**, bei laufendem Webdienst
  gesichert: Original, Sicherung und wiederhergestellte Datenbank enthalten
  jeweils 35.040 Datensätze; SQLite-Integritätsprüfung jeweils `ok`.
  Ungültige Sicherungen und Restore bei aktiver Sperre werden separat getestet.
- **Docker-Build und Containerlauf nachgeholt**: Test- und Laufzeitimage mit
  Docker Engine 29.0.1 erfolgreich gebaut (Linux/ARM64, Python 3.13).
  `docker run --rm backload-history-tests:local-check` besteht mit **38 Tests**.
  `docker compose config --quiet` ist ebenfalls erfolgreich.
- **Echter Container-/Volume-Integrationstest**:
  `python3 tests/check_container.py` erfolgreich ausgeführt. Ein eigener
  Testdatensatz wurde im neu angelegten benannten Volume gespeichert und über
  die HTTP-API geprüft. Nach `--force-recreate` war der Container tatsächlich
  ersetzt und der Messwert weiter vorhanden. Online-Backup und Wiederherstellung
  bei gestopptem Dienst funktionierten ebenfalls; anschließend war der Wert
  erneut per HTTP lesbar. Das Testprojekt samt Test-Volume wurde entfernt;
  die Bereinigung wurde kontrolliert.
- **Firmware unverändert**: Prüfsummen der neun vorhandenen Dateien unter
  `Backload/` und `DachSensor/` mit dem Stand vor dieser Arbeit verglichen.

Die Browserdaten waren synthetische Testdaten in einer temporären Datenbank
außerhalb dieses Ordners. Der ausgelieferte Dienst enthält weder Messdaten
noch eine Demo- oder Schreibschnittstelle. Der lokale Testclient meldet eine
Starlette-Deprecation-Warnung für httpx; die Tests bestehen.

## Noch mit dem realen ESP32 zu prüfen

Die vorherige lokale ECI-/Lizenzblockade besteht nicht mehr; die Docker-Prüfungen
sind jetzt abgeschlossen. Die ESP32-Adresse `192.168.177.83` war auch beim
erneuten Versuch aus dem Laufzeitcontainer nicht erreichbar: Die Abfrage wurde
nach dem vorgesehenen Fünf-Sekunden-Zeitlimit als `timeout` zurückgegeben.
Die Erfassung wurde gegen die vorhandene Firmware-Schnittstelle anhand
simulierter Antworten geprüft. Nach dem Start auf dem Zielserver den nächsten
Viertelstundentermin und die realen Sensorwerte kontrollieren.
