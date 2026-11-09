from pathlib import Path
import base64
import re

root = Path(__file__).resolve().parents[1]
sketch = (root / 'Backload/Backload.ino').read_text()
page = (root / 'Backload/DashboardPage.h').read_text()
proxy = (root / 'Backload/WebSerialMonitor.cpp').read_text()
header = (root / 'Backload/RelayBoardPhoto.h').read_text()
photo = (root / 'Documentation/relaiserweiterung-vorderseite.jpg').read_bytes()
svg = (root / 'Documentation/relaiserweiterung-freigestellt.svg').read_text()
embedded = base64.b64decode(re.search(r'href="data:image/jpeg;base64,([^"]+)"', svg).group(1), validate=True)
assert embedded == photo and photo.startswith(b'\xff\xd8')
assert 'R"SVGDOC(' + svg + ')SVGDOC"' in header
assert '<mask id="board"' in svg and 'mask="url(#board)"' in svg
assert b'Exif\x00\x00' not in photo and b'http://ns.adobe.com/xap/1.0/' not in photo
assert 'src="/assets/relaiserweiterung.svg"' in page
assert 'https://github.com/NemoN/Solarsystemsteuerung_ESP32/releases/latest/download/Relaiskarte.ino.bin' in page
for text in ('5V · TX · RX · GND · GND · IO0', 'Adapter-TX an Karten-RX', 'Adapter-RX an Karten-TX', 'IO0 gedrückt halten, EN kurz drücken und danach IO0 loslassen', 'widersprüchliche Angaben', 'https://prilchen.de/esp32-4-kanal-relais-entwicklungsplatine/'):
    assert text in page, f'Fehlender Hinweis zur seriellen Erstinstallation: {text}'
assert '"/assets/relaiserweiterung.jpg"' in proxy and '"/assets/relaiserweiterung.svg"' in proxy and 'image/svg+xml' in proxy and 'RELAY_BOARD_PHOTO' in proxy
backend = set(re.findall(r'\bserver\.on\("(/api/remote-relay[^\"]+)",\s*(HTTP_\w+)', sketch))
frontend = set(re.findall(r'dashboard\.server\(\)\.on\("(/api/remote-relay[^\"]+)",\s*(HTTP_\w+)', proxy))
assert backend and backend <= frontend, f'Nicht weitergeleitete Relais-Endpunkte: {backend - frontend}'
print('PASS: Freigestelltes Originalfoto, Einrichtungsanleitung und Relais-Webzugänge.')
