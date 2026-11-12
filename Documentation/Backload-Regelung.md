# Backload- und Solarregelung

Die Steuerung überträgt Wärme zwischen Kombi- und Pufferspeicher. Die Backloadpumpe
und eine Umpolschaltung bestimmen die Förderrichtung. Relais 4 steuert die
manuelle Ölbrennerfreigabe; Relais 3 schaltet die Solarpumpe.

## Ölbrennerfreigabe und automatische Regelung

Relais 4 erteilt die Ölbrennerfreigabe. Die Beschriftung „Ölbrenner“ kann in
den Einstellungen geändert werden. Eine Rückmeldung über den tatsächlichen
Brennerlauf ist nicht angeschlossen. Die Sperre kann nicht erkennen, ob der
Brenner ohne Freigabe von anderer Stelle eingeschaltet wird.

| Förderrichtung | Messfühler für die Differenz | Start: alle Bedingungen | Stopp: eine Bedingung genügt |
| --- | --- | --- | --- |
| Kombi → Puffer | Kombi oben und Puffer oben | Beide Mindesttemperaturen erreicht, Freigabe AUS, Kombi oben ≥ Einlagerungstemperatur und Kombi oben − Puffer ≥ Startdifferenz | Mindesttemperatur unterschritten, Kombi oben − Puffer ≤ 3 K, Freigabe AN oder ungültiger erforderlicher Fühler |
| Puffer → Kombi | Kombi Mitte und Puffer oben | Beide Mindesttemperaturen erreicht und Puffer − Kombi Mitte ≥ Startdifferenz | Mindesttemperatur unterschritten, Puffer − Kombi Mitte ≤ 3 K oder ungültiger erforderlicher Fühler |

Die Mindesttemperaturen verwenden **Kombi Mitte und Puffer oben**, auch bei der
Einlagerung. Beide Grenzen müssen erreicht sein und gelten während des Laufs;
genau auf der Grenze bleibt der Betrieb erlaubt. Ein Fehler an einem dieser
beiden Fühler sperrt beide Richtungen. Ein Fehler an Kombi oben sperrt nur die
Einlagerung. Bei gleichzeitig erfüllten Startbedingungen hat Einlagerung Vorrang;
ein laufender Auftrag bleibt bis zur jeweiligen Stoppbedingung aktiv.

Die Einlagerungs-Starttemperatur ist von 68 bis 72 °C einstellbar und gilt nur
beim Start für Kombi oben. Startdifferenz: 5–10 K, Vorgabe 8 K. Mindestwerte:
je 0–80 °C, Vorgabe 35 °C. Die Stoppdifferenz beträgt fest 3 K. Vorhandene
Einstellungen bleiben erhalten. Bei unbekannter Ölfreigabe, ungültigen
Einstellungen oder nicht verfügbarem Gerätespeicher bleibt die Automatik aus.
Ein ungültiger Einlagerungssollwert verhindert nur den Einlagerungsstart.

## Solarregelung

Relais 3 schaltet unabhängig von der Backloadpumpe, ihrer manuellen Steuerung und
der Ölbrennerfreigabe. Die Einschaltdifferenz ist auf der Startseite von 3 bis 10 K
in 0,1-K-Schritten konfigurierbar und steht auch in der Regelungsübersicht.
Vorgabe sind 5 K.

| Bedingung | Verhalten |
| --- | --- |
| Solarpanel − Kombi unten ≥ Einschaltdifferenz | Pumpe EIN |
| Solarpanel − Kombi unten ≤ 1 K | Pumpe AUS |
| Differenz zwischen den Grenzen | vorheriger Zustand bleibt erhalten |
| Panel-Sensorfehler bei laufender Pumpe | Letzten gültigen Panelwert höchstens 60 Sekunden ab dessen Messung verwenden |
| Speicherfühlerfehler, veraltete Messzyklen oder ungültige Einstellungen | Automatik: Pumpe AUS |

Die Überbrückung startet eine ausgeschaltete Pumpe nicht. Der Fühler im Kombi
unten muss weiterhin gültig sein und die Messzyklen dürfen höchstens 60
Sekunden alt sein; mit seinem aktuellen Wert und dem letzten
gültigen Panelwert gilt auch die Stoppdifferenz. Nach 60 Sekunden ohne gültigen
Panelwert schaltet die Pumpe ab. Ein neuer gültiger Panelwert beendet die
Überbrückung und erlaubt wieder den normalen Automatikbetrieb. Die API meldet
den Fehler weiterhin über `sensorsValid: false` und den überbrückten Lauf über
`sensorGrace: true`.

Die Weboberfläche meldet einen Solarpanel-Sensorfehler erst nach mehr als 60 Sekunden
ununterbrochen ungültiger Messungen. Bis dahin zeigt sie den letzten gültigen
Panelwert mit einem grauen Fragezeichen auf der Temperaturkarte. Ohne vorherigen
gültigen Wert erscheint ein Gedankenstrich. Jede
gültige Panelmessung setzt den Anzeige-Timer zurück. Die Verzögerung verändert
die Regelung und die 60-sekündige Überbrückung nicht. Fehlerdetails in API und
Log sowie die Fehleranzeige der Status-LED bleiben sofort verfügbar. Die API
kennzeichnet die Wartezeit mit `sensorFaultPending` und liefert den alten
Panelwert getrennt als `lastValidTemperature`; `temperature` bleibt `null`.

Die Solarpumpe hat einen eigenen Handbetrieb. Beim Einschalten des Handbetriebs
ist sie zunächst aus und kann anschließend ein- und ausgeschaltet werden. Im
Handbetrieb werden Temperaturdifferenz und Sensorfehler übergangen. Beim Wechsel
zurück zur Automatik gelten deren Bedingungen wieder unmittelbar.

Während eines Updates, der Startprüfung und eines angeforderten Neustarts ist
die Solarpumpe aus und der Handbetrieb wird beendet. Nach dem Neustart beginnt
die Automatik mit ausgeschaltetem Relais und wartet auf gültige Messwerte.
Test- und Anlagenwerte werden getrennt gespeichert;
im Testmodus wird nur der Pumpenzustand simuliert und das Relais bleibt AUS.
Die Regelung enthält keine Maximaltemperaturbegrenzung. Unabhängige
Schutzeinrichtungen und die tatsächliche Verdrahtung vor dem Einsatz prüfen.

## Manuelle Steuerung und Richtungswechsel

Im Handbetrieb werden Temperaturregelung, Mindesttemperaturen und Sensorfehler
übergangen. Kombi → Puffer ist auch bei Ölfreigabe möglich; das Webinterface
warnt davor. Ist die Freigabe unbekannt, bleibt diese Richtung gesperrt. Beim Richtungswechsel:
Pumpe AUS, mindestens zwei Sekunden warten, Richtungsrelais schalten, 100 ms
Schaltzeit abwarten und nur bei bestehendem Einschaltauftrag wieder starten.

Der Testmodus simuliert Sensoren, Pumpe und Ölfreigabe; die echten Relais bleiben
AUS. Test- und Anlageneinstellungen sind getrennt. Während eines Firmwareupdates,
der Startprüfung und eines angeforderten Neustarts bleibt die echte
Ölbrennerfreigabe AUS; danach gilt wieder die gespeicherte Betriebsart.

## Anschlüsse und gespeicherte Daten

| Relais | GPIO | Aufgabe |
| --- | --- | --- |
| 1 | 3 | Backloadpumpe EIN/AUS |
| 2 | 8 | Pumprichtung |
| 3 | 18 | Solarpumpe EIN/AUS |
| 4 | 17 | Ölbrennerfreigabe |

GPIO 18/17 müssen zur tatsächlichen Verdrahtung passen. Die Relais sind Active
Low. Richtungsrelais AUS bedeutet im Sketch vorläufig Kombi → Puffer; die
Zuordnung muss an der tatsächlichen Umpolschaltung geprüft werden. GPIO 3 ist
ein ESP32-S3-Strapping-Pin; das Relaisverhalten beim Einschalten prüfen.

Der gemeinsame Sollwert bleibt aus Kompatibilitätsgründen in `c0` gespeichert.
Werte von 68–72 °C werden übernommen. Bei älteren Werten außerhalb dieses
Bereichs wird 68 °C verwendet und eine Logmeldung ausgegeben; der alte Eintrag
wird erst durch explizites Speichern überschrieben. Von `auto1` wird ein
gültiger Startabstand von 5–10 K übernommen; frühere Stoppabstände werden
ignoriert. Ungültige Startabstände sperren die Automatik bis zur Korrektur.

`GET /api/status` meldet `automatic.chargeSensorId` (0), `returnSensorId` (1),
`minimumCombiSensorId` (1), die Grenzwerte und `heating` einschließlich
Freigabe, Bezeichnung und Relaiszustand. Das alte Feld `controlSensorId`
nennt den Fühler der aktuellen Pumpenrichtung.
`POST /api/heating` schaltet mit dem kompatiblen Feld `winter=0/1` die Freigabe.
`POST /api/heating/label` speichert die Bezeichnung mit `label`.
`POST /api/automatic` erwartet `start` und optional beide Felder `minimumCombi`
und `minimumBuffer`; ohne diese bleiben die bisherigen Werte erhalten. `solar` enthält den Pumpenzustand, die
Messwert- und Konfigurationsgültigkeit sowie Start-, Stopp- und aktuelle
Differenz. `POST /api/solar` speichert `start` (3–10 K). Sensor-IDs bleiben
unverändert.
