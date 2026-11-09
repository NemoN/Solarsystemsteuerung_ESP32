"""Prüft die Bestandteile eines erzeugten ESP32-Erstinstallationsabbilds."""
import sys
from pathlib import Path

if len(sys.argv) != 4:
    raise SystemExit('Aufruf: test_relay_factory_image.py FACTORY_BIN BUILD_ORDNER BOOT_APP0_BIN')
image = Path(sys.argv[1]).read_bytes()
build = Path(sys.argv[2])
parts = [
    (0x1000, (build / 'Relaiskarte.ino.bootloader.bin').read_bytes()),
    (0x8000, (build / 'Relaiskarte.ino.partitions.bin').read_bytes()),
    (0xe000, Path(sys.argv[3]).read_bytes()),
    (0x10000, (build / 'Relaiskarte.ino.bin').read_bytes()),
]
assert image[:0x1000] == bytes([0xff]) * 0x1000
for address, data in parts:
    assert image[address:address + len(data)] == data, f'Abweichung bei {address:#x}'
assert image[0x9000:0xe000] == bytes([0xff]) * 0x5000, 'NVS-Bereich muss leer sein'
assert len(image) == 0x10000 + len(parts[-1][1]) <= 0x150000
assert b'RBS16467-RELAY-FIRMWARE-OTA1' in parts[-1][1]
print('PASS: Bootloader, Partitionstabelle, OTA-Startdaten und Relais-Firmware am richtigen Offset; NVS leer.')
