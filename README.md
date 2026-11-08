# Backload-Steuerung

Steuerung für eine Solaranlage mit Kombi- und Pufferspeicher auf einem ESP32-S3.
Die Weboberfläche zeigt Temperaturen und Pumpenzustände und bietet einen Testmodus.

![Weboberfläche im Testmodus mit simulierten Werten](Documentation/steuerung-testmodus.png)

Die Backloadpumpe überträgt Wärme zwischen den Speichern. Ihre Mindesttemperaturen
lassen sich für Kombi und Puffer getrennt einstellen. Die Solarpumpe wird
unabhängig davon geregelt. Auf der Verlaufsseite stehen die Pumpenlaufzeiten nach
Tag, Monat und Jahr. Ein optionaler Docker-Dienst archiviert Temperaturen und
Pumpenlaufzeiten dauerhaft.

Die Steuerung ist für eine konkrete Heizungsanlage ausgelegt. Vor dem Einsatz
müssen Sensoranschlüsse, Förderrichtung, Regelparameter und die unabhängigen
Schutzeinrichtungen der Anlage geprüft werden.

## Arduino IDE

In der Arduino IDE 2 `Backload/Backload.ino` öffnen. Der gesamte Ordner `Backload`
mit dem Unterordner `src` muss vorhanden sein. Den ESP32-Core 2.0.13 über die
Boardverwaltung installieren und unter **Werkzeuge** einstellen:

- Board: ESP32S3 Dev Module
- Flash Size: 16 MB
- PSRAM: OPI PSRAM
- Partition Scheme: 16M Flash (3MB APP/9.9MB FATFS)
- Erase All Flash Before Sketch Upload: Disabled

Die benötigten Bibliotheken liegen bereits in `Backload/src`. Updates global
installierter Bibliotheken verändern diese Kopien nicht. Die Arduino IDE kann
aktualisiert werden; danach den Build mit dem angegebenen Core prüfen.

## Weitere Informationen

- [Firmware und Einrichtung](Backload/LIESMICH.txt)
- [Backload- und Solarregelung](Documentation/Backload-Regelung.md)
- [Temperaturarchiv](history-server/README.md)
- [Mitgelieferte Bibliotheken](Backload/src/README.md)
- [Tests](tests/)
