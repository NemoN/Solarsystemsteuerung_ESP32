# WLAN-Relaiskarte RBS16467

Der Sketch steuert die vier Relais der Karte über den Haupt-ESP. Das Datenblatt
ordnet Relais 1–4 den GPIOs 32, 33, 25 und 26 zu und verwendet HIGH zum
Einschalten. Die Relaiszustände werden bei einer Änderung im Flash gespeichert
und nach einem Neustart wiederhergestellt. Während des Neustarts sind die
Ausgänge zunächst ausgeschaltet. Bei WLAN-Ausfall bleibt der letzte Zustand
bestehen. Der gemeldete Zustand ist der geschaltete Ausgang, keine Rückmeldung
des Relaiskontakts oder der angeschlossenen Last.

## Einrichten

Bis zu vier Erweiterungen lassen sich parallel mit dem Haupt-ESP verbinden.

1. `Relaiskarte.ino` mit ESP32-Core 2.0.13 für **ESP32 Dev Module** auf die
   Karte laden. Der Programmieranschluss benötigt einen seriellen Adapter mit
   **3,3-V-Logik**; GPIO 0 beim Flashen auf GND legen. Stromversorgung und
   Anschlüsse anhand des Datenblatts prüfen.
2. Die Karte einschalten. Ihr Einrichtungs-WLAN heißt `Relaiserweiterung-XXXX`.
3. Am Haupt-ESP unter **Einstellungen → Relaiserweiterungen** einen freien
   Platz wählen und **Neue Erweiterungen suchen**. Die gewünschte Erweiterung
   auswählen und **Erweiterung einrichten** drücken. Anschließend die Taste
   **IO0** auf der Erweiterung drei Sekunden gedrückt halten.
4. Der Haupt-ESP öffnet kurz einen Kopplungs-AP und bleibt mit dem
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

## Firmware über den Haupt-ESP aktualisieren

Die hier enthaltene Firmware mit Updatefunktion muss zunächst einmal über den
seriellen Anschluss auf die Relaiskarte geladen werden. Auch der Haupt-ESP
benötigt die zugehörige Firmware. Die Karte muss im Heimnetz erreichbar sein;
der Haupt-ESP muss im Normalbetrieb sein. Im Testmodus sind Relais-Updates gesperrt.

Unter **Einstellungen → Relaiskarte aktualisieren** gibt es zwei Möglichkeiten:

- **GitHub:** Nach Releases suchen und eine Version auswählen. Der Browser und
  der Haupt-ESP benötigen dafür Internetzugang. Angezeigt werden nur Releases
  mit `Relaiskarte.ino.bin` und einer SHA-256-Prüfsumme. Bestehende Releases
  ohne diese Datei erscheinen nicht.
- **Manuell:** `Relaiskarte.ino` für **ESP32 Dev Module** kompilieren und über
  **Sketch → Kompilierte Binärdatei exportieren** die Datei
  `Relaiskarte.ino.bin` erzeugen. Das Partitionsschema muss zwei OTA-App-Slots
  enthalten; das Standardschema mit zwei Slots zu je 1,25 MB ist geeignet.

Der Haupt-ESP lädt oder empfängt die Datei, puffert sie im PSRAM und überträgt
sie im Hintergrund an die Karte. Bei zu wenig PSRAM wird das Update abgelehnt.
Die Heizungsregelung läuft weiter. Für die GitHub-Suche muss bei einem künftigen
Release die kompilierte Datei exakt als `Relaiskarte.ino.bin` angehängt werden.

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
