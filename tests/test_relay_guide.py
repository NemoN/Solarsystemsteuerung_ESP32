from pathlib import Path
import re

root = Path(__file__).resolve().parents[1]
sketch = (root / 'Backload/Backload.ino').read_text()
page = (root / 'Backload/DashboardPage.h').read_text()
proxy = (root / 'Backload/WebSerialMonitor.cpp').read_text()
header = (root / 'Backload/RelayBoardPhoto.h').read_text()
photo = (root / 'Documentation/relaiserweiterung-vorderseite.jpg').read_bytes()
embedded = bytes(int(value, 16) for value in re.findall(r'0x([0-9a-f]{2})', header))
assert embedded == photo and photo.startswith(b'\xff\xd8')
assert b'Exif\x00\x00' not in photo and b'http://ns.adobe.com/xap/1.0/' not in photo
assert 'src="/assets/relaiserweiterung.jpg"' in page
assert 'https://github.com/NemoN/Solarsystemsteuerung_ESP32/releases/latest/download/Relaiskarte.ino.bin' in page
for text in ('5V · TX · RX · GND · GND · IO0', 'Adapter-TX an Karten-RX', 'Adapter-RX an Karten-TX', 'IO0 gedrückt halten, EN kurz drücken und danach IO0 loslassen', 'widersprüchliche Angaben zum 5V-Pin'):
    assert text in page, f'Fehlender Hinweis zur seriellen Erstinstallation: {text}'
assert '"/assets/relaiserweiterung.jpg"' in proxy and 'RELAY_BOARD_PHOTO' in proxy
backend = set(re.findall(r'\bserver\.on\("(/api/remote-relay[^\"]+)",\s*(HTTP_\w+)', sketch))
frontend = set(re.findall(r'dashboard\.server\(\)\.on\("(/api/remote-relay[^\"]+)",\s*(HTTP_\w+)', proxy))
assert backend and backend <= frontend, f'Nicht weitergeleitete Relais-Endpunkte: {backend - frontend}'
print('PASS: Metadatenfreies Platinenfoto, Einrichtungsanleitung und Relais-Webzugänge.')
