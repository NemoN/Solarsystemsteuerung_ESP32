from pathlib import Path
import os
import subprocess
import tempfile

sketch = (Path(__file__).resolve().parents[1] / 'Backload/Backload.ino').read_text()
start = sketch.index('void prepareRemoteRelayUpdate() {')
end = sketch.index('void receiveRemoteRelayUpdateChunk() {', start)
handlers = sketch[start:end]
source = r'''
#include <cassert>
#include <cstdint>
#include <cstring>
#include <map>
#include <string>
using String = std::string;
bool SIMULATION_MODE = false, remoteRelayUpdateReady = true, authorized = true, validBoard = true;
struct Server {
  std::map<String, String> values; int code = 0;
  int args() { return int(values.size()); }
  bool hasArg(const String& name) { return values.count(name); }
  String arg(const String& name) { return values[name]; }
  void send(int c, const char*, const String&) { code = c; }
} server;
struct Provisioning { bool running = false; bool active() { return running; } } relayProvisioning;
struct Relay { int snapshot() { return 1; } } remoteRelays[1];
struct Update {
  bool allowed = true; int calls = 0;
  bool prepare(uint32_t, int, char* token) { ++calls; std::strcpy(token, "token"); return allowed; }
  bool prepareGithub(uint32_t, String, String, int, char* token) { ++calls; std::strcpy(token, "token"); return allowed; }
} remoteRelayUpdate;
bool remoteRelayPostAllowed() { if (!authorized) server.code = 403; return authorized; }
bool remoteRelayBoardArgument(unsigned& board) { board = 0; if (!validBoard) server.code = 400; return validBoard; }
bool otaNumber(const String& s, uint32_t& n) { if (s != "1000") return false; n = 1000; return true; }
''' + handlers + r'''
int main() {
  for (bool mode : {false, true}) {
    SIMULATION_MODE = mode;
    server.values = {{"size", "1000"}}; server.code = 0;
    prepareRemoteRelayUpdate(); assert(server.code == 200);
    server.values = {{"size", "1000"}, {"url", "url"}, {"sha256", "digest"}}; server.code = 0;
    startRemoteRelayGithubUpdate(); assert(server.code == 202);
    relayProvisioning.running = true;
    prepareRemoteRelayUpdate(); assert(server.code == 409);
    startRemoteRelayGithubUpdate(); assert(server.code == 409);
    relayProvisioning.running = false; remoteRelayUpdateReady = false;
    prepareRemoteRelayUpdate(); assert(server.code == 409);
    startRemoteRelayGithubUpdate(); assert(server.code == 409);
    remoteRelayUpdateReady = true; authorized = false;
    prepareRemoteRelayUpdate(); assert(server.code == 403);
    startRemoteRelayGithubUpdate(); assert(server.code == 403);
    authorized = true; validBoard = false;
    prepareRemoteRelayUpdate(); assert(server.code == 400);
    startRemoteRelayGithubUpdate(); assert(server.code == 400);
    validBoard = true; remoteRelayUpdate.allowed = false;
    server.values = {{"size", "1000"}}; prepareRemoteRelayUpdate(); assert(server.code == 409);
    server.values = {{"size", "1000"}, {"url", "url"}, {"sha256", "digest"}};
    startRemoteRelayGithubUpdate(); assert(server.code == 400);
    remoteRelayUpdate.allowed = true;
  }
}
'''
with tempfile.TemporaryDirectory() as tmp:
    cpp, exe = Path(tmp) / 'handlers.cpp', Path(tmp) / 'handlers'
    cpp.write_text(source)
    subprocess.run([os.environ.get('CXX', 'c++'), '-std=c++11', '-Wall', '-Wextra', '-Werror', str(cpp), '-o', str(exe)], check=True)
    subprocess.run([str(exe)], check=True)
print('PASS: Datei- und GitHub-Update in beiden Betriebsarten; Zugriffs- und Einrichtungssperren bleiben wirksam.')
