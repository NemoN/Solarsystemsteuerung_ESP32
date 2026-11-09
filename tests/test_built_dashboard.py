"""Prüft das JavaScript in der tatsächlich gebauten ESP32-Firmware."""
from html.parser import HTMLParser
from pathlib import Path
import subprocess
import sys
import tempfile

firmware = Path(sys.argv[1]).read_bytes()
marker = b"const root=document.getElementById('controllers');"
assert firmware.count(marker) == 1, 'Hauptskript in Firmware nicht eindeutig gefunden'
position = firmware.index(marker)
start = firmware.rfind(b'<!doctype html>', 0, position)
end = firmware.find(b'</html>', position)
assert start >= 0 and end >= 0, 'HTML in Firmware nicht vollständig gefunden'
page = firmware[start:end + len(b'</html>')].decode('utf-8')

class Scripts(HTMLParser):
    def __init__(self):
        super().__init__()
        self.collecting = False
        self.current = []
        self.items = []
    def handle_starttag(self, tag, attrs):
        if tag == 'script' and not dict(attrs).get('src'):
            self.collecting = True
            self.current = []
    def handle_data(self, data):
        if self.collecting:
            self.current.append(data)
    def handle_endtag(self, tag):
        if tag == 'script' and self.collecting:
            self.items.append(''.join(self.current))
            self.collecting = False

scripts = Scripts()
scripts.feed(page)
assert len(scripts.items) == 2, f'Unerwartete Anzahl eingebetteter Skripte: {len(scripts.items)}'
with tempfile.TemporaryDirectory() as tmp:
    for index, script in enumerate(scripts.items):
        path = Path(tmp) / f'script-{index}.js'
        path.write_text(script)
        subprocess.run(['node', '--check', str(path)], check=True)
print('PASS: JavaScript aus der gebauten Firmware ist syntaktisch gültig.')
