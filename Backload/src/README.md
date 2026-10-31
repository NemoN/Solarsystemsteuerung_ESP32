# Eingebundene Bibliotheken

Die folgenden MIT-lizenzierten Quellen werden mit dem Sketch kompiliert. Die
Lizenztexte liegen im jeweiligen Verzeichnis.

- RisalDash 0.13.0, Stand `6d22da191d74e3a859c751e42348af06a3969aad`: <https://github.com/ziyarago/RisalDash>
- AsyncWebConsole 0.3.2, Stand `098b0e34642590056deade17551db6f70145f06e`: <https://github.com/drakosha/AsyncWebConsole>

Anpassungen: RisalDash startet ohne eigene WLAN- oder Zeitsynchronisation und
kann ein vom Sketch bereitgestelltes Widget und eine zusätzliche Seite einbinden. Seine zusätzlichen API-
und WLAN-Reset-Endpunkte sind deaktiviert. AsyncWebConsole nimmt keine
Browserbefehle an, zeigt keine Eingabezeile und verwendet die deutsche
Beschriftung des Live-Monitors. Die HTML-Datei trägt die Endung `.h`, damit
Arduino sie in das Buildverzeichnis kopiert. Vor dem Senden werden getrennte
WebSocket-Clients bereinigt, damit erneute Verbindungen Ausgaben empfangen.
Leere Logzeilen bleiben im Live-Monitor auch bei pausierter Ausgabe erhalten.
