---
name: Backload Temperaturarchiv
description: Bestehendes Gestaltungssystem der lokalen Temperaturhistorie.
colors:
  background: "#f3f5f4"
  surface: "#fff"
  ink: "#192f35"
  muted: "#4e6268"
  teal: "#17636b"
  teal-hover: "#104c53"
  border: "#b9c8c9"
  border-strong: "#8c9fa3"
  control-hover: "#e3eceb"
  combi-middle: "#955018"
  buffer: "#6855a0"
  error: "#8b351f"
typography:
  headline:
    fontSize: "clamp(1.7rem, 3.5vw, 2.15rem)"
    fontWeight: 700
    lineHeight: 1.2
    letterSpacing: "-.025em"
  title:
    fontSize: "1.25rem"
    fontWeight: 700
    lineHeight: 1.3
  body:
    fontFamily: 'system-ui, -apple-system, BlinkMacSystemFont, "Segoe UI", sans-serif'
    fontSize: "16px"
    lineHeight: 1.5
  action:
    fontSize: ".875rem"
    fontWeight: 600
    lineHeight: 1.4
  label:
    fontSize: ".8rem"
rounded:
  control: "6px"
spacing:
  compact: "8px"
  field-gap: "12px"
  regular: "16px"
  section: "24px"
  spacious: "32px"
components:
  button-primary:
    backgroundColor: "{colors.teal}"
    textColor: "{colors.surface}"
    typography: "{typography.action}"
    rounded: "{rounded.control}"
    padding: "10px 16px"
  button-primary-hover:
    backgroundColor: "{colors.teal-hover}"
  button-neutral:
    backgroundColor: "{colors.surface}"
    textColor: "{colors.ink}"
    typography: "{typography.action}"
    rounded: "{rounded.control}"
    padding: "10px 16px"
  button-secondary:
    backgroundColor: "{colors.surface}"
    textColor: "{colors.teal}"
    typography: "{typography.action}"
    rounded: "{rounded.control}"
    padding: "10px 16px"
  button-text:
    backgroundColor: "transparent"
    textColor: "{colors.teal}"
    typography: "{typography.action}"
    padding: "10px 0"
  date-field:
    backgroundColor: "{colors.surface}"
    textColor: "{colors.ink}"
    rounded: "{rounded.control}"
    padding: "8px 10px"
  chart-surface:
    backgroundColor: "{colors.surface}"
    rounded: "{rounded.control}"
    padding: "16px 16px 8px"
---

# Design System: Backload Temperaturarchiv

## Overview

Das Archiv übernimmt die helle graue Grundfläche, Systemschrift und das gedeckte Petrol der vorhandenen Backload-Oberfläche. Die Darstellung ist sachlich und kompakt: Erfassungsstatus, Zeitraumwahl und beschriftete Sensoren führen zum großen Temperaturdiagramm. Messwerte bleiben ohne Animation unmittelbar vergleichbar.

**Key Characteristics:**
- Helle Flächen, dunkle Schrift und Petrol für aktive Bedienung.
- Drei Kurven mit Farbe und unterschiedlicher Strichart.
- Umbruchfähige Bedienelemente und sichtbarer Tastaturfokus.

## Colors

### Primary

Petrol kennzeichnet aktive Zeitraumknöpfe, die primäre Aktion, Links, Checkboxen und die durchgezogene Kurve „Kombispeicher oben“. Die dunklere Variante ist der Hoverzustand.

### Secondary

Braun kennzeichnet ausschließlich „Kombispeicher Mitte“ mit gestrichelter Linie; Violett kennzeichnet „Pufferspeicher oben“ mit gepunkteter Linie. Rotbraun kennzeichnet Fehler und auffällige Abfrageergebnisse.

### Neutral

Die graue Seitenfläche trägt weiße Eingaben und Diagrammflächen. Dunkle Schrift trägt Hauptinhalte, die gedämpfte Schrift Beschriftungen und Hinweise. Zwei Randtöne und ein heller Hoverton gliedern die Bedienung.

**The Sensor Identity Rule.** Sensorname, Farbe und Strichart bleiben gemeinsam erkennbar; Farbe allein trägt die Unterscheidung nicht.

## Typography

Die gesamte Seite verwendet die Systemschrift aus dem Body-Token; Zahlen sind tabellarisch. Die Überschrift skaliert fließend, die Abschnittsüberschrift bleibt fest. Aktionen und Legenden liegen bei `.875rem`, Beschriftungen und Hinweise bei `.8rem`; Statuswerte liegen dazwischen (`.9rem`). Fließtext ist auf `75ch` begrenzt, Diagrammhinweise auf `48ch`. Es gibt keine separate Displayschrift.

## Layout

Der zentrierte Inhalt ist einschließlich Innenabstand höchstens `1200px` breit. Desktop: Außenabstand innen `36px 32px 24px`, Status in drei Spalten (`1fr 1.25fr 1fr`), danach Zeitraumwahl, Legende und Diagramm. Gruppen brechen um; der Rhythmus verwendet überwiegend die Abstände aus dem Frontmatter. Das Diagramm ist `clamp(320px, 42vw, 440px)` hoch.

Bis einschließlich `650px` erhält die Seite `24px 20px` Innenabstand. Der Produktname rechts entfällt; Statuspaare stehen untereinander mit zwei Spalten pro Paar. Zeitraumknöpfe werden schmaler (`10px 12px` Innenabstand), Datumsfelder füllen ihre flexible Spalte, die Legende steht untereinander. Das Diagramm ist `330px` hoch und erhält `12px 6px 6px` Innenabstand. Die x-Achse begrenzt unter `650px` die Beschriftung auf vier statt acht Teilstriche.

## Elevation & Depth

Die Oberfläche verwendet keine Schatten und keine Verläufe. Weiße Diagramm- und Eingabeflächen, feine Ränder und horizontale Trennlinien erzeugen die Gliederung. Lade-, Leer- und Fehlertexte liegen auf einer deckend weißen Fläche innerhalb des Diagrammrahmens.

## Shapes

Bedienflächen, Datumsfelder und Diagrammrahmen teilen leicht gerundete Ecken aus dem Control-Token und einen dünnen Rand (`1px`). Status und Fußzeile werden durch gerade Trennlinien gegliedert. Die Legendenproben sind kurze Linien (`24px` breit, `3px` stark), keine gefüllten Abzeichen.

## Components

### Buttons

Neutrale Zeitraumknöpfe verwenden weißen Grund und dunkle Schrift; `aria-pressed="true"` übernimmt die primäre Petrolfläche. Sekundäre Aktionen wie CSV und Wiederholung bleiben weiß mit Petrolschrift. Aktualisieren ist ein unterstrichener Textknopf. Alle haben mindestens `44px` Höhe. Hover verändert Farbe, ohne Bewegung. Während des Ladens ist Aktualisieren deaktiviert; deaktivierte Knöpfe zeigen Wartecursor und `0.6` Deckkraft.

### Inputs / Fields

Native Datumsfelder stehen unter sichtbaren Beschriftungen und haben mindestens `44px` Höhe. Der eigene Zeitraum wird aufgeklappt, der Fokus wechselt ins Startdatum. Ungültige Datumsfolgen verwenden die native Validierungsanzeige. Alle fokussierbaren Elemente zeigen bei Tastaturfokus eine Petrolkontur (`3px`, Abstand `3px`).

### Sensor Controls

Native Checkboxen (`17px`) schalten die drei Kurven unabhängig. Jede beschriftete Zeile hat mindestens `44px` Höhe und trägt eine passende Linienprobe. Die Diagrammstriche sind `2px` stark; die Muster sind durchgezogen, `[7, 4]` gestrichelt und `[2, 4]` gepunktet. Einzelne isolierte Werte erhalten sichtbare Punkte; fehlende Werte bleiben Lücken.

### Chart / Status

Das Diagramm bleibt bei Änderungen ohne Animation stabil. Laden, leerer Zeitraum, ausschließlich ungültige Werte, ausgeblendete Kurven und Ladefehler haben eigene Textzustände im selben Rahmen. Nur Ladefehler bieten „Erneut laden“. Der CSV-Link bleibt vor dem Diagramm erreichbar. Ein nicht erreichbarer Erfassungsstatus erscheint als separate Textmeldung oberhalb des Verlaufs. Zeitangaben und Zahlen verwenden deutsche Formatierung mit Europe/Berlin.

Fehlende Temperaturen bleiben sichtbar unterbrochen; eine leere Auswahl erhält eine Erklärung statt eines unbelegten Startversprechens.

## Do's and Don'ts

### Do:
- Do Sensoren mit Name, Farbe und Strichart zusammen darstellen.
- Do Filter umbrechen und die Sensorlegende auf schmalen Bildschirmen stapeln.
- Do sichtbaren Tastaturfokus und mindestens 44px hohe Bedienflächen erhalten.

### Don't:
- Don't fehlende Messwerte durch verbindende Kurven verbergen.
- Don't Lade-, Leer- und Fehlerzustände als Messdaten darstellen.
- Don't neue Schatten oder Bewegungsinszenierungen in die bestehende sachliche Oberfläche einführen.
