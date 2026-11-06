# Eingebundene Bibliotheken

Die folgenden Quellen werden mit dem Sketch kompiliert und müssen nicht über
die Bibliotheksverwaltung installiert werden. Die Versionsangaben beziehen sich
auf die nachfolgend genannten Stände. Die Lizenztexte beziehungsweise Lizenz-
und Urheberrechtshinweise liegen in den jeweiligen Verzeichnissen.

- RisalDash 0.13.0, Stand `6d22da191d74e3a859c751e42348af06a3969aad`: <https://github.com/ziyarago/RisalDash>
- AsyncWebConsole 0.3.2, Stand `098b0e34642590056deade17551db6f70145f06e`: <https://github.com/drakosha/AsyncWebConsole>
- Adafruit MAX31865 1.6.2, Stand `02dd3393d9750ca6e227be84b0e3b795a6941c36`: <https://github.com/adafruit/Adafruit_MAX31865>
- Adafruit BusIO 1.14.4, Stand `ee0faec7ee8b34373772184d04864f04bcf56730`: <https://github.com/adafruit/Adafruit_BusIO>
- Async TCP 3.3.2, Stand `c90fc983bb61ba48f6a0bbc5609b0e5a6f5f1651`: <https://github.com/ESP32Async/AsyncTCP>
- ESP Async WebServer 3.6.0, Stand `c1888926ed8595f14f4971e9e1538b43ea63aa59`: <https://github.com/ESP32Async/ESPAsyncWebServer>

Die Bibliotheksquellen werden für den Sketch über relative Pfade eingebunden;
einige `#include`-Anweisungen wurden dafür angepasst. Die ungenutzten
ArduinoJson- und MessagePack-Adapter des Webservers sind nicht enthalten, damit
keine zufällig global installierte ArduinoJson-Version eingebunden wird. Die
BSD-Lizenzhinweise von Adafruit MAX31865 stehen in dessen Quelldateien; der
genannte Upstream-Stand enthält keine separate Lizenzdatei. Die übrigen
Lizenztexte liegen jeweils in `LICENSE`.

Weitere Anpassungen: RisalDash startet ohne eigene WLAN- oder Zeitsynchronisation und
kann ein vom Sketch bereitgestelltes Widget und eine zusätzliche Seite einbinden. Seine zusätzlichen API-
und WLAN-Reset-Endpunkte sind deaktiviert. AsyncWebConsole nimmt keine
Browserbefehle an, zeigt keine Eingabezeile und verwendet die deutsche
Beschriftung des Live-Monitors. Die HTML-Datei trägt die Endung `.h`, damit
Arduino sie in das Buildverzeichnis kopiert. Vor dem Senden werden getrennte
WebSocket-Clients bereinigt, damit erneute Verbindungen Ausgaben empfangen.
Leere Logzeilen bleiben im Live-Monitor auch bei pausierter Ausgabe erhalten.
