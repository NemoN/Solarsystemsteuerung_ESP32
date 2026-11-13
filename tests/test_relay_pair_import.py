#!/usr/bin/env python3
"""Compile the actual card credential-import code against small host doubles."""
from pathlib import Path
import subprocess
import tempfile

root = Path(__file__).resolve().parents[1]
sketch = (root / 'Relaiskarte/Relaiskarte.ino').read_text()
valid = sketch[sketch.index('bool validWifi('):sketch.index('void randomHex(', sketch.index('bool validWifi('))]
start = sketch.index('bool importPairedWifi(')
importer = sketch[start:sketch.index('void claimPairing()', start)]
code = r'''
#include <Arduino.h>
#include <Preferences.h>
#include <mbedtls/base64.h>
#include <cassert>
#include <cstdio>
#include <map>
#include <string>
#include <vector>
std::map<std::string,std::vector<unsigned char>> mockPreferences;
struct Config { char ssid[33]; char password[64]; char key[33]; char apPassword[17]; };
Config config = {};
Preferences prefs;
bool relayStorageHealthy = true;
''' + valid + importer + r'''
int main() {
 assert(prefs.begin("relay-board", false));
 std::strcpy(config.key,"aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa");
 assert(importPairedWifi("PAIR1:SG91c2UtV2lGaQ==:cGFzc3dvcmQxMjM="));
 assert(!std::strcmp(config.ssid,"House-WiFi")&&!std::strcmp(config.password,"password123"));
 assert(!std::strcmp(config.key,"aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa"));
 const Config saved=config;
 assert(!importPairedWifi("PAIR1:SG91c2UtV2lGaQ==:c2hvcnQ="));
 assert(!importPairedWifi("PAIR1::cGFzc3dvcmQxMjM="));
 assert(!importPairedWifi("PAIR1:AA==:cGFzc3dvcmQxMjM="));
 assert(!importPairedWifi("PAIR1:@@@@:cGFzc3dvcmQxMjM="));
 assert(!importPairedWifi("PAIR1:SG91c2UtV2lGaQ=="));
 assert(!importPairedWifi("WRONG:SG91c2UtV2lGaQ==:cGFzc3dvcmQxMjM="));
 assert(!std::memcmp(&saved,&config,sizeof(saved)));
 assert(importPairedWifi("PAIR1:T3Blbg==:"));
 assert(!std::strcmp(config.ssid,"Open")&&!config.password[0]);
 std::puts("PASS: Erweiterung übernimmt gültige WLAN-Daten; ungültige bleiben unverändert.");
}
'''
with tempfile.TemporaryDirectory(prefix='relay-pair-import-') as directory:
    source = Path(directory) / 'test.cpp'
    binary = Path(directory) / 'test'
    source.write_text(code)
    subprocess.run(['c++','-std=c++11','-Wall','-Wextra','-Werror','-fsanitize=address,undefined',
                    '-I'+str(root/'tests/relay_ota_mocks'),str(source),'-o',str(binary)],check=True)
    subprocess.run([str(binary)],check=True)
