#!/bin/sh
# Erzeugt das komplette Erstinstallationsabbild aus einem ESP32-Dev-Module-Build.
set -eu
if [ "$#" -ne 4 ]; then
  printf 'Aufruf: %s BUILD_ORDNER ESPTOOL BOOT_APP0_BIN AUSGABE_BIN\n' "$0" >&2
  exit 2
fi
build_dir=$1
esptool_bin=$2
boot_app0=$3
output=$4
"$esptool_bin" --chip esp32 merge_bin --flash_mode dio --flash_freq 80m --flash_size 4MB \
  -o "$output" \
  0x1000 "$build_dir/Relaiskarte.ino.bootloader.bin" \
  0x8000 "$build_dir/Relaiskarte.ino.partitions.bin" \
  0xe000 "$boot_app0" \
  0x10000 "$build_dir/Relaiskarte.ino.bin"
