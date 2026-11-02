# Backload-Regelung und Ölbrennerfreigabe

Backload verschiebt Wärme zwischen Kombispeicher und Pufferspeicher. Zwei
Relais schalten die Transferpumpe und deren Förderrichtung. Ein viertes Relais
übernimmt die manuelle Ölbrennerfreigabe; Relais 3 bleibt für die spätere
Solarpumpe reserviert und ausgeschaltet.

## Sommer und Winter

Der manuelle Schalter im Webinterface hat genau diese Bedeutung:

| Betriebsart | Relais 4 / Ölbrennerfreigabe | Kombi → Puffer | Puffer → Kombi |
| --- | --- | --- | --- |
| Sommer | AUS | bei passenden Temperaturen möglich | bei passenden Temperaturen möglich |
| Winter | AN | gesperrt, auch im Handbetrieb | bei passenden Temperaturen möglich |

Die Anzeige beschreibt die Freigabe durch das Relais. Die Steuerung hat keine
Rückmeldung, ob der Brenner tatsächlich feuert. Die SVG neben dem Kombispeicher
zeigt deshalb die Freigabe grau oder orange sowie den Relaiszustand, ohne
Flammenanimation. Simulation und fehlende Daten werden ausdrücklich benannt.
Eine laufende Einlagerung wird
beim Einschalten der Ölfreigabe zuerst gestoppt; erst danach schaltet Relais 4.
Ein verworfener Handauftrag startet nach Abschalten der Ölfreigabe nicht erneut.
Die Automatik prüft die Temperaturen dann wieder selbstständig.

Die Betriebsart wird gespeichert. Beim ersten Start ohne gespeicherte neue
Betriebsart gilt Sommer, also Ölbrenner AUS. Die frühere Sommer-/Winterauswahl
hatte den Kombifühler gewechselt und wird **nicht** als Ölfreigabe übernommen.

## Fühler, Starttemperatur und Startdifferenz

Ausschließlich **Kombispeicher oben (ID 0)** und **Pufferspeicher oben (ID 2)**
steuern die Automatik. Kombispeicher Mitte, Kombispeicher unten und Solarpanel
werden angezeigt und aufgezeichnet. Ihre Fehler beeinflussen die Pumpe nicht.

Es gibt zwei Einstellungen:

- **Einlagerung ab:** Mindesttemperatur im Kombispeicher oben für den Start
  von Kombi → Puffer. Einstellbar von **65 bis 70 °C**, Vorgabe **68 °C**.
- **Startdifferenz:** erforderlicher Temperaturvorsprung der Wärmequelle
  gegenüber dem Ziel. Einstellbar von **5 bis 10 K**, Vorgabe **8 K**.

Die Starttemperatur wird ausschließlich beim Start der Einlagerung geprüft.
Eine laufende Einlagerung endet nicht beim Unterschreiten dieser Temperatur.
Für die Rückladung hat die Starttemperatur keine Bedeutung, weder beim Start
noch im laufenden Betrieb. Ein zusätzlicher Sollwert-Startabstand und ein
separater Stoppabstand entfallen.

Die Grenzen gelten im Formular, in den Schreib-APIs und in der Regelung.
Die Schaltübersicht zeigt gespeicherte Werte; ungespeicherte Eingaben verändern
sie nicht.

## Schalttabelle der Automatik

Zum Start müssen alle Bedingungen erfüllt sein. Zum Stoppen genügt eine.
Kombi und Puffer bezeichnen immer die beiden oberen Fühler.

| Förderrichtung | Start | Stopp |
| --- | --- | --- |
| Kombi → Puffer | Ölbrennerfreigabe AUS **und** Kombi ≥ Starttemperatur **und** Kombi − Puffer ≥ Startdifferenz | Ölbrenner freigegeben / Betriebsart unbekannt **oder** Kombi ≤ Puffer |
| Puffer → Kombi | Puffer − Kombi ≥ Startdifferenz, unabhängig von Starttemperatur und Ölbrenner | Puffer ≤ Kombi |

Nach dem Start läuft die Pumpe auch bei kleiner werdender Differenz weiter.
Sie stoppt beim Temperaturausgleich oder wenn der abgebende Speicher bereits
kälter ist. Dadurch wird kein exakt gleicher Fließkommawert vorausgesetzt.
Erneut startet sie erst bei Erreichen der Startdifferenz. Ein großer Sprung
in die Gegenrichtung kann einen Richtungswechsel anfordern; dafür gilt
weiterhin die Pumpenpause.

Bei Starttemperatur **68 °C** und Startdifferenz **8 K**:

- Kombi 68 °C / Puffer 60 °C: Einlagerung startet, wenn Öl gesperrt ist.
- Danach Kombi 66 °C / Puffer 64 °C: Einlagerung läuft weiter.
- Kombi 65 °C / Puffer 65 °C: Pumpe stoppt.
- Kombi 37 °C / Puffer 45 °C: Rückladung startet auch im Winter.
- Kombi 72 °C / Puffer 80 °C: Rückladung startet ebenfalls, oberhalb des Sollwerts.

Ein Fehler an einem der beiden oberen Fühler, ungültige Startdifferenzen oder
nicht verfügbarer Gerätespeicher schalten die Automatik aus. Ein ungültiger
Einlagerungssollwert sperrt den Start der Einlagerung; eine Rückladung bleibt
bei gültigen Messungen und gültiger Startdifferenz möglich.

## Handbetrieb, Testmodus und Richtungswechsel

Handbetrieb übergeht Temperaturregelung und Sensorfehler, jedoch **nicht die
Ölbrennersperre**. Bei Ölfreigabe kann keine Einlagerung von Kombi nach Puffer
angefordert werden. Die Rückrichtung bleibt bedienbar.

Bei jeder Richtungsumkehr gilt: Pumpe AUS, mindestens zwei Sekunden Stillstand,
Richtungsrelais schalten, 100 ms Schaltzeit abwarten und nur bei weiter
bestehendem Einschaltauftrag die Pumpe starten.

Der Testmodus simuliert auch die Sommer-/Winterumschaltung. Alle echten
Relais bleiben ausgeschaltet; Test- und Anlageneinstellungen sind getrennt.
Während eines Firmwareupdates, dessen Startprüfung und eines angeforderten
Neustarts bleibt die echte Ölbrennerfreigabe ebenfalls AUS. Danach gilt die
gespeicherte Betriebsart wieder.

## Anschlüsse und gespeicherte Daten

| Relais | GPIO im Sketch | Aufgabe |
| --- | --- | --- |
| 1 | 3 | Transferpumpe EIN/AUS |
| 2 | 8 | Pumprichtung |
| 3 | 9 | Solarpumpe, reserviert und AUS |
| 4 | 10 | Ölbrennerfreigabe |

Die GPIOs 9 und 10 sind die vorgesehene Erweiterung und müssen zur tatsächlichen
Verdrahtung passen. Die Konstanten stehen am Anfang von Backload.ino. Alle
Relais sind als Active Low vorgesehen; Richtungsrelais AUS bedeutet vorläufig
Kombi → Puffer. Die mechanische Zuordnung bleibt am Aufbau zu prüfen.

Gültige Startdifferenzen aus `auto1`, Slot 1, werden übernommen. Die früheren
Sollwert-Startabstände und Stoppschwellen darin werden ignoriert. Ein alter
Startwert außerhalb von 5–10 K sperrt die Automatik, bis er über das Formular
neu gespeichert wird. Das Formular bietet dafür die Vorgabe 8 K an.
Beim Speichern bleibt das historische Fünf-Float-Format bestehen; stillgelegte
Abstände werden mit 0 gespeichert. Die alten Fühlerwahl-/Freigabefelder sind
weiterhin stillgelegt. Ölfreigabe liegt separat in `oil1`.

Der obere Kombi-Sollwert in `c0` wird übernommen, wenn er in 65–70 °C liegt.
Andernfalls verwendet die neue Firmware 68 °C und protokolliert die Abweichung;
der alte NVS-Datensatz wird beim Lesen nicht überschrieben. Erst Speichern
über das Formular schreibt den neuen Wert. Alte Einstellungen für Mitte und
Puffer bleiben unbenutzt erhalten. Der frühere per-Sensor-Hysteresewert in `c0`
ist nur aus Formatgründen vorhanden und hat keinen Einfluss auf die Regelung.

`GET /api/status` liefert `automatic.controlSensorId=0`, die wirksame
`automatic.allowCharging` und `heating` mit `winter`, `configValid` und
`relayOn` (echter Relaiszustand, im Test immer AUS). Die aus Kompatibilitätsgründen
vorhandenen Felder `targetOffset` und `differenceStop` sind fest 0.
`POST /api/heating` schaltet mit `winter=0/1`.
`POST /api/automatic` erwartet nur noch `start` (5–10 K). Schreibende Clients
aus älteren Versionen müssen angepasst werden. Die Sensor-IDs und die
Schnittstellen für das Temperaturarchiv bleiben unverändert.
