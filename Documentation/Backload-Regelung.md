# Backload- und Solarregelung

Die Steuerung überträgt Wärme zwischen Kombi- und Pufferspeicher. Die Backloadpumpe
und eine Umpolschaltung bestimmen die Förderrichtung. Relais 4 steuert die
manuelle Ölbrennerfreigabe; Relais 3 schaltet die Solarpumpe.

## Sommer und Winter

| Betriebsart | Kombifühler | Ölbrennerfreigabe | Kombi → Puffer | Puffer → Kombi |
| --- | --- | --- | --- | --- |
| Sommer | oben, ID 0 | AUS | bei passenden Temperaturen möglich | bei passenden Temperaturen möglich |
| Winter | Mitte, ID 1 | AN | gesperrt, auch bei manueller Steuerung | bei passenden Temperaturen möglich |

Puffer oben (ID 2) ist immer der Vergleichsfühler für die Backloadpumpe.
Kombi unten und Solarpanel regeln zusätzlich die Solarpumpe. Beim Betriebsartwechsel wird ein laufender
Automatikauftrag zurückgesetzt und anhand des neu gewählten Fühlers neu
bewertet. Bei unbekannter Betriebsart ist die Automatik ausgeschaltet.

Die Ölbrenneranzeige zeigt die durch Relais 4 erteilte **Freigabe**. Eine
Rückmeldung über den tatsächlichen Brennerlauf ist nicht angeschlossen. Die
Ölsperre kann daher nicht erkennen, ob der Brenner trotz fehlender Freigabe
von anderer Stelle eingeschaltet wird. Beim Wechsel auf Winter wird die
Einlagerung gestoppt, bevor Relais 4 eingeschaltet wird.

Die Betriebsart liegt im Gerätespeicher unter `oil1` und bleibt nach Neustart
erhalten. Wenn kein Eintrag vorhanden ist, gilt Sommer / Öl AUS. Frühere
Fühlerwahl-Felder aus `auto1` werden nicht als Brennerfreigabe übernommen.

## Automatische Regelung

Einstellungen:

- **Einlagerung ab:** 68–72 °C, Vorgabe 68 °C. Die Starttemperatur gilt für
  den gewählten Kombifühler und nur für Kombi → Puffer.
- **Startdifferenz:** 5–10 K, Vorgabe 8 K, für beide Richtungen.
- **Mindesttemperaturen:** je 0–80 °C, Vorgabe je 35 °C für den gewählten
  Kombifühler und für Puffer oben. In der Automatik müssen beide ihre jeweilige
  Grenze erreichen. Im Handbetrieb gelten die Mindesttemperaturen nicht.
- **Stoppdifferenz:** fest 3 K für beide Richtungen.

Neue Eingaben im Webinterface sind ganzzahlig. Ältere gespeicherte Dezimalwerte
innerhalb des zulässigen Bereichs bleiben wirksam und sichtbar.

| Förderrichtung | Start: alle Bedingungen | Stopp: eine Bedingung genügt |
| --- | --- | --- |
| Kombi → Puffer | Kombi und Puffer jeweils ≥ Mindesttemperatur, Ölfreigabe AUS, Kombi ≥ Starttemperatur und Kombi − Puffer ≥ Startdifferenz | Kombi oder Puffer unter jeweiliger Mindesttemperatur, Kombi − Puffer ≤ 3 K oder Ölfreigabe AN / Betriebsart unbekannt |
| Puffer → Kombi | Kombi und Puffer jeweils ≥ Mindesttemperatur und Puffer − Kombi ≥ Startdifferenz, unabhängig von Einlagerungs-Starttemperatur und Ölfreigabe | Kombi oder Puffer unter jeweiliger Mindesttemperatur oder Puffer − Kombi ≤ 3 K |

Die Mindesttemperaturen gelten auch während des Pumpenlaufs. Genau auf der
jeweiligen Grenze ist der Betrieb erlaubt. Bei Puffer 50 °C und Kombi 20 °C
bleibt die Automatik beispielsweise mit der Vorgabe von je 35 °C ausgeschaltet.

Die Temperaturdifferenz wird dabei jeweils von der wärmeren Quellseite zum
Ziel berechnet. Nach dem Start muss die Einlagerungstemperatur nicht weiter
erreicht werden. Bei 68 °C / 8 K startet beispielsweise Kombi 68 °C mit Puffer
60 °C die Einlagerung; bei Kombi 67 °C und Puffer 64 °C stoppt sie. Im Winter
startet Kombi Mitte 37 °C mit Puffer 45 °C die Rückladung und stoppt bei 42 / 45 °C.

Ein Fehler am gewählten Kombifühler oder Pufferfühler stoppt die Automatik.
Fehler an den nicht gewählten Fühlern beeinflussen sie nicht. Ungültige
Startdifferenzen, unbekannte Betriebsart oder nicht verfügbarer Gerätespeicher
sperren die Automatik. Ein ungültiger Einlagerungssollwert sperrt den Start der
Einlagerung; Rückladung bleibt bei gültigen Messungen und Einstellungen möglich.

## Solarregelung

Relais 3 schaltet unabhängig von der Backloadpumpe, ihrer manuellen Steuerung und
Sommer/Winter. Die Einschaltdifferenz ist auf der Startseite von 3 bis 10 K
in 0,1-K-Schritten konfigurierbar und steht auch in der Regelungsübersicht.
Vorgabe sind 5 K.

| Bedingung | Verhalten |
| --- | --- |
| Solarpanel − Kombi unten ≥ Einschaltdifferenz | Pumpe EIN |
| Solarpanel − Kombi unten ≤ 1 K | Pumpe AUS |
| Differenz zwischen den Grenzen | vorheriger Zustand bleibt erhalten |
| Fehler eines Solarfühlers, Messwerte älter als 10 Sekunden oder ungültige Einstellungen | Automatik: Pumpe AUS |

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

Bei manueller Steuerung werden Temperaturregelung und Sensorfehler übergangen, die
Ölsperre gilt weiter. Puffer → Kombi bleibt bedienbar. Beim Richtungswechsel:
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
| 3 | 9 | Solarpumpe EIN/AUS |
| 4 | 10 | Ölbrennerfreigabe |

GPIO 9/10 müssen zur tatsächlichen Verdrahtung passen. Die Relais sind Active
Low. Richtungsrelais AUS bedeutet im Sketch vorläufig Kombi → Puffer; die
Zuordnung muss an der tatsächlichen Umpolschaltung geprüft werden. GPIO 3 ist
ein ESP32-S3-Strapping-Pin; das Relaisverhalten beim Einschalten prüfen.

Der gemeinsame Sollwert bleibt aus Kompatibilitätsgründen in `c0` gespeichert.
Werte von 68–72 °C werden übernommen. Bei älteren Werten außerhalb dieses
Bereichs wird 68 °C verwendet und eine Logmeldung ausgegeben; der alte Eintrag
wird erst durch explizites Speichern überschrieben. Von `auto1` wird ein
gültiger Startabstand von 5–10 K übernommen; frühere Stoppabstände werden
ignoriert. Ungültige Startabstände sperren die Automatik bis zur Korrektur.

`GET /api/status` meldet `automatic.controlSensorId` (Sommer 0, Winter 1),
`automatic.differenceStart`, `automatic.minimumCombi`, `automatic.minimumBuffer`, `automatic.differenceStop` (3) und `heating` mit
Betriebsart, Konfigurationsgültigkeit und physischem Relaiszustand.
`POST /api/heating` schaltet mit `winter=0/1` Fühlerwahl und Ölfreigabe.
`POST /api/automatic` erwartet `start` und optional beide Felder `minimumCombi`
und `minimumBuffer`; ohne diese bleiben die bisherigen Werte erhalten. `solar` enthält den Pumpenzustand, die
Messwert- und Konfigurationsgültigkeit sowie Start-, Stopp- und aktuelle
Differenz. `POST /api/solar` speichert `start` (3–10 K). Sensor-IDs bleiben
unverändert.
