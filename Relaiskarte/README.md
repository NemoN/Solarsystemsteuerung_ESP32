# WLAN-Relaiskarte RBS16467

Der Sketch steuert die vier Relais der Karte über den Haupt-ESP. Das Datenblatt
ordnet Relais 1–4 den GPIOs 32, 33, 25 und 26 zu und verwendet HIGH zum
Einschalten. Die Relaiszustände werden bei einer Änderung im Flash gespeichert
und nach einem Neustart wiederhergestellt. Während des Neustarts sind die
Ausgänge zunächst ausgeschaltet. Bei WLAN-Ausfall bleibt der letzte Zustand
bestehen. Der gemeldete Zustand ist der geschaltete Ausgang, keine Rückmeldung
des Relaiskontakts oder der angeschlossenen Last.

## Erstinstallation über USB

Karte und Adapter vor dem Verkabeln stromlos machen. Netzspannung und
angeschlossene Verbraucher während der Erstinstallation getrennt lassen.
Der USB-Seriell-Adapter muss **3,3-V-Logik an TX und RX** unterstützen; die
Versorgung der Karte erfolgt mit 5 V. Eine Einstellung für 3,3-V-Logik darf
den 5-V-Versorgungsausgang des Adapters nicht unbeabsichtigt verändern.

Die einreihige Programmierleiste neben dem ESP32 ist auf der Unterseite mit
`5V · TX · RX · GND · GND · IO0` beschriftet. Bei Versorgung über den Adapter:

| USB-Seriell-Adapter | Programmierleiste der Karte |
| --- | --- |
| 5V-Ausgang | 5V |
| GND | GND (einer der beiden Pins) |
| TX / TXD | RX |
| RX / RXD | TX |

Die Versorgung mit 5 V über den Adapter wird auch in diesem
[Erfahrungsbericht zur ESP32 Relay X4 V1.1](https://prilchen.de/esp32-4-kanal-relais-entwicklungsplatine/)
beschrieben. Der Adapter muss dafür ausreichend Strom liefern und TX/RX müssen
3,3-V-Logik verwenden. Das beiliegende
[Datenblatt](https://www.roboter-bausatz.de/media/pdf/ce/40/8c/RBS16467-ESP32-4-Kanal-Relais-Board.pdf)
enthält zum 5V-Pin beim Programmieren allerdings widersprüchliche Angaben.
Alternativ die Karte separat mit 5 V versorgen: Dann Adapter-5V freilassen,
aber Adapter-GND und Karten-GND verbinden. Keine weiteren Stromversorgungen
gleichzeitig anschließen.

Für die Erstinstallation gibt es ein fertiges
[Komplettabbild](https://github.com/NemoN/Solarsystemsteuerung_ESP32/releases/latest/download/Relaiskarte.factory.bin)
mit Bootloader, Partitionstabelle und Firmware. Kompilieren ist nicht nötig.

1. Das Komplettabbild herunterladen und den Adapter per USB verbinden.
2. Für den **Flashmodus** auf der Karte **IO0** gedrückt halten, **EN** kurz
   drücken und anschließend **IO0** loslassen.
3. Das [ESP Tool von Espressif](https://espressif.github.io/esptool-js/) in
   Chrome oder Edge öffnen. **Connect** wählen und den Port des Adapters der
   Relaiskarte auswählen.
4. Mit **Add File** die heruntergeladene `Relaiskarte.factory.bin` auswählen,
   als **Flash Address** `0x0` eintragen und **Program** drücken. **Erase all**
   ausgeschaltet lassen; die übrigen Einstellungen können auf den Vorgaben
   bleiben.
5. Nach dem Hochladen **EN** kurz drücken, ohne IO0 zu drücken. Die Karte
   startet nun mit der Firmware.

Das Komplettabbild überschreibt beim Flashen auch die gespeicherten WLAN-Daten,
Schlüssel und Relaiszustände. Es löscht nicht den gesamten Flash. Für spätere
Updates über den Haupt-ESP ausschließlich `Relaiskarte.ino.bin` verwenden.

Alternativ lässt sich `Relaiskarte.ino` mit ESP32-Core 2.0.13 in der Arduino IDE
für **ESP32 Dev Module** kompilieren und hochladen.

## Einrichten

Bis zu vier Erweiterungen lassen sich parallel mit dem Haupt-ESP verbinden.

1. Die Karte einschalten. Ihr Einrichtungs-WLAN heißt `Relaiserweiterung-XXXX`.
2. Am Haupt-ESP unter **Einstellungen → Relaiserweiterungen** einen freien
   Platz wählen und **Neue Erweiterungen suchen**. Die gewünschte Erweiterung
   auswählen und **Erweiterung einrichten** drücken. Anschließend die Taste
   **IO0** auf der Erweiterung drei Sekunden gedrückt halten.
3. Der Haupt-ESP öffnet kurz einen Kopplungs-AP und bleibt mit dem
   Heim-WLAN verbunden. Die Erweiterung übernimmt darüber die WLAN-Daten. Der
   Haupt-ESP sucht sie anschließend im Heimnetz und prüft ihren übertragenen
   Verbindungsschlüssel.
   Danach lassen sich die Beschriftungen in den Einstellungen ändern und die
   vier Relais auf der Startseite schalten.

Für die automatische Suche müssen beide Geräte im selben IPv4-Heimnetz sein und
Multicast-DNS zulassen. Bei WLAN-Client-Isolation kann die Suche fehlschlagen.
Die Kopplung überträgt die WLAN-Daten nach einem kurzlebigen Schlüsselaustausch
verschlüsselt. Die Geräte prüfen ihre Identität dabei nicht unabhängig: Ein
aktiver Angreifer in Funkreichweite kann die kurze Kopplung abfangen. Das
individuelle Passwort des Einrichtungs-WLANs bleibt für die manuelle Einrichtung
erhalten; es steht im seriellen Monitor (115200 Baud).

### Manuelle Einrichtung

Alternativ mit `Relaiserweiterung-XXXX` verbinden, `http://192.168.4.1`
öffnen und das 2,4-GHz-Heimnetz eintragen. Danach im Haupt-ESP einen freien
Platz auswählen und unter **Im Heimnetz gefunden** die Erweiterung wählen.
Den im seriellen Monitor angezeigten Verbindungsschlüssel eingeben. Eine feste
DHCP-Zuordnung im Router hält die Adresse stabil. Ein leerer Schlüssel behält
nur bei unveränderter Adresse den gespeicherten Schlüssel.

Die Karte und der Haupt-ESP müssen im selben Heimnetz sein. Die Kommunikation
ist durch den individuellen Schlüssel geschützt, erfolgt aber unverschlüsselt;
für ein vertrauenswürdiges privates Netzwerk verwenden. Den Schlüssel nicht
weitergeben. Falls die Karte nicht erreichbar ist, zeigt die Oberfläche den
Zustand als unbekannt und sperrt die Schalter. Ein Schaltauftrag wird nicht
blind wiederholt, wenn seine Rückmeldung verloren geht.

Im Testmodus des Haupt-ESP bleiben die WLAN-Schalter nutzbar und schalten die
Ausgänge tatsächlich. Die Karte arbeitet
unabhängig von der Backload- und Solarregelung. Der Dachsensor-Empfang des
Haupt-ESP ist vorläufig deaktiviert.

Unter **Einstellungen → Relaiserweiterungen** lassen sich die vier Relaisnamen
ändern und eine Erweiterung vergessen. Beim Vergessen schaltet die Karte alle
Relais aus und löscht WLAN-Zugangsdaten, Verbindungsschlüssel und gespeicherte
Schaltzustände. Danach kann sie erneut gekoppelt werden. Ist die Karte nicht
erreichbar, bleibt die Zuordnung gespeichert. Geht die Rückmeldung verloren,
zeigt die Oberfläche den unklaren Zustand; die Karte kann bereits zurückgesetzt
sein. Die Firmware auf der Karte muss den Werksreset unterstützen.
Unter **Probleme beim Zurücksetzen?** kann die Zuordnung bei Bedarf auch nur am
Haupt-ESP entfernt werden. Dabei bleiben die Daten auf der Karte erhalten.

Vor dem Anschließen von Verbrauchern die Pinbelegung, die tatsächliche
Schaltlogik und das Verhalten beim Start an der vorliegenden Karte prüfen.
Insbesondere können gespeicherte Relais beim Neustart wieder einschalten.

## Relaiskarte neu starten

Unter **Einstellungen → Relaiserweiterungen** kann eine eingerichtete Karte
gezielt neu gestartet werden. Die Ausgänge schalten während des Neustarts kurz
aus und stellen anschließend die gespeicherten Zustände wieder her. Der Befehl
benötigt Relaiskarten-Firmware 3.1.0 oder neuer. Ältere Karten müssen zuerst
über die Firmware-Aktualisierung auf eine unterstützte Version gebracht werden.
Eine unterbrochene Antwort bestätigt den Neustart nicht; den Kartenstatus und
die Relaiszustände anschließend prüfen.

## Firmware über den Haupt-ESP aktualisieren

Die hier enthaltene Firmware mit Updatefunktion muss zunächst einmal über den
seriellen Anschluss auf die Relaiskarte geladen werden. Auch der Haupt-ESP
benötigt die zugehörige Firmware. Die Karte muss im Heimnetz erreichbar sein.
Das Update funktioniert auch im Testmodus des Haupt-ESP.

Unter **Einstellungen → Relaiskarte aktualisieren** gibt es zwei Möglichkeiten:

- **GitHub:** Nach Releases suchen und eine Version auswählen. Der Browser und
  der Haupt-ESP benötigen dafür Internetzugang. Angezeigt werden nur Releases
  mit `Relaiskarte.ino.bin` und einer SHA-256-Prüfsumme. Bestehende Releases
  ohne diese Datei erscheinen nicht.
- **Manuell:** `Relaiskarte.ino` für **ESP32 Dev Module** kompilieren und über
  **Sketch → Kompilierte Binärdatei exportieren** die Datei
  `Relaiskarte.ino.bin` erzeugen. Das Partitionsschema muss zwei OTA-App-Slots
  enthalten; das Standardschema mit zwei Slots zu je 1,25 MB ist geeignet.

Die installierte Version und die Hinweise zur Relaiskarten-Firmware stehen
ebenfalls dort. Die auswählbaren Releases tragen die gemeinsame Release-Nummer;
die Versionsnummer der Relaiskarten-Firmware kann davon abweichen. Ältere
Karten melden ihre Version erst nach einem Firmware-Update.

Der Haupt-ESP lädt oder empfängt die Datei, puffert sie im PSRAM und überträgt
sie im Hintergrund an die Karte. Bei zu wenig PSRAM wird das Update abgelehnt.
Die Heizungsregelung läuft weiter. Für die GitHub-Suche muss bei einem künftigen
Release die kompilierte Datei exakt als `Relaiskarte.ino.bin` angehängt werden.
Die Release Notes benötigen für die Anzeige getrennte Abschnitte `### Haupt-ESP`
und `### Relaiskarte`. Ältere Releases ohne diese Abschnitte zeigen in den
jeweiligen Bereichen keine allgemeinen Release Notes an. Im Abschnitt zur
Relaiskarte die Änderungen der Relaiskarten-Firmware
beschreiben oder, wenn nur die Versionsnummer geändert wurde, zum Beispiel
`Funktional unverändert zur Vorversion; die Versionsnummer wurde aktualisiert.`
eintragen. Ohne diesen Abschnitt zeigt die Oberfläche für neue Releases an,
dass keine eigenen Hinweise zur Relaiskarte vorliegen.

Die Karte prüft Zielgerät, Größe und SHA-256-Prüfsumme vor dem Neustart. Während
der Übertragung bleiben die Relais unverändert und lassen sich nicht schalten.
Beim Neustart gehen die Ausgänge zunächst aus und stellen danach ihre letzten
Zustände wieder her. Erst nach der Startprüfung bestätigt die Oberfläche den
Erfolg. Wird die Übertragung vor dem Abschluss unterbrochen, bleibt die bisherige
Firmware aktiv. Bei fehlgeschlagener Startprüfung ist ein Rückfall auf den
vorherigen Slot vorgesehen.
Wenn die Rückmeldung nach dem Neustart fehlt, zeigt die Oberfläche den Status
als unklar: Verbindung und Relaiszustände prüfen, bevor ein neues Update startet.

Die Verbindung verwendet denselben individuellen Schlüssel wie die Schalter.
Sie ist unverschlüsselt und nur für das vertrauenswürdige Heimnetz vorgesehen.

## Komplettabbild für ein Release erzeugen

Nach einem Build für **ESP32 Dev Module** mit ESP32-Core 2.0.13 erzeugt
`build_factory.sh BUILD_ORDNER ESPTOOL BOOT_APP0_BIN AUSGABE_BIN` die Datei
`Relaiskarte.factory.bin`. Als `BOOT_APP0_BIN` dient `boot_app0.bin` aus dem
Partitionsordner des ESP32-Cores. Das fertige Abbild zusätzlich zur unveränderten
OTA-Datei `Relaiskarte.ino.bin` an das Release anhängen.
