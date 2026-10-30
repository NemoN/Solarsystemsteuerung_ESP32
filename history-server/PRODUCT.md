# Backload Temperaturarchiv

<!-- impeccable:product-schema 1 -->

## Platform

web

## Stack

Vom Nutzer bestätigter Umsetzungsplan: Docker Compose, Python/FastAPI,
SQLite und lokal mitgeliefertes Chart.js. Ein Container auf dem Docker-Server.

## Users

Betreiber der privaten Backload-Heizungsanlage; Zugriff im Heimnetz auf Desktop
und Smartphone. Sie vergleichen den Temperaturverlauf der beiden Speicher.

## Product Purpose

Drei Temperaturen dauerhaft als Viertelstunden-Momentaufnahmen erfassen,
im Diagramm vergleichen und vollständig für Excel exportieren.

## Operating Context

Vorhandener ESP32 mit lesender `/api/status`-Schnittstelle im selben Heimnetz.
Serveruhr synchronisiert; Anzeige Europe/Berlin, Speicherung UTC.
Keine Firmwareänderung erforderlich. Webseite muss auch ohne Internet arbeiten.

## Capabilities and Constraints

- Sensor-ID 0: Kombispeicher oben; 1: Kombispeicher Mitte; 2: Pufferspeicher oben.
- Jede Viertelstunde eine Momentaufnahme; fünf Sekunden Abfragezeitlimit.
- Unabhängige Sensorfehler, sichtbare Lücken und keine nachgefüllten Messwerte.
- Sieben Tage als Startansicht; 24 Stunden, 30/365 Tage und eigener Zeitraum.
- Drei schaltbare Kurven, CSV mit allen Originalwerten, lesende HTTP-Endpunkte.
- Keine automatische Löschung; benanntes Docker-Volume, Sicherung/Wiederherstellung.

## Evidence on Hand

Bestätigter Plan im Nutzerauftrag und vorhandene Schnittstelle in
`../Backload/Backload.ino`. Bestehende Oberfläche als visuelle Referenz.
Testdaten sind synthetisch und werden nicht mit der produktiven Datenbank ausgeliefert.
