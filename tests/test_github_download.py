#!/usr/bin/env python3
"""Test the actual ESP download task with controlled HTTP, flash and TLS doubles."""
import pathlib
import subprocess
import tempfile

project = pathlib.Path(__file__).resolve().parent.parent
sketch = (project / 'Backload/Backload.ino').read_text()
start = sketch.index('enum GithubDownloadResult {')
end = sketch.index('void startGithubOta()', start)
download_task = sketch[start:end]

stubs = r'''
#include <algorithm>
#include <atomic>
#include <cassert>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <string>
#include <vector>

struct Scenario { int status = 200; int stopAt = -1; int size = 60000; bool stall = false; bool writeFail = false; };
std::vector<Scenario> scenarios;
int connection = -1, position = 0, aborts = 0, begins = 0, hashStarts = 0;
bool isConnected = false, cancelOnDelay = false, beginFails = false;
uint32_t clockMs = 0;
const uint32_t otaExpectedSize = 60000;
const unsigned GITHUB_DOWNLOAD_ATTEMPTS = 3;
const int HTTP_CODE_OK = 200, HTTPC_STRICT_FOLLOW_REDIRECTS = 1, U_FLASH = 0;
const int GH_FAILED = 1, GH_DOWNLOADED = 2;
const char* GITHUB_ROOT_CERTS = "test-ca";
std::string githubUpdateUrl = "https://example.test/firmware";
std::string githubExpectedHash(64, '0');
char githubUpdateError[256] = {};
char githubRetryErrors[GITHUB_DOWNLOAD_ATTEMPTS - 1][256] = {};
std::atomic<int> githubUpdateState(0);
std::atomic<uint32_t> githubUpdateBytes(0);
std::atomic<unsigned> githubUpdateAttempt(0);
std::atomic<bool> githubUpdateCancel(false);

uint32_t millis() { return clockMs; }
void delay(uint32_t n) { clockMs += n; if (n == 1000 && cancelOnDelay) githubUpdateCancel.store(true); }
void vTaskDelete(void*) {}
struct mbedtls_sha256_context { size_t count = 0; };
void mbedtls_sha256_init(mbedtls_sha256_context* h) { h->count = 0; }
int mbedtls_sha256_starts_ret(mbedtls_sha256_context* h, int) { h->count = 0; ++hashStarts; return 0; }
int mbedtls_sha256_update_ret(mbedtls_sha256_context* h, const uint8_t*, size_t n) { h->count += n; return 0; }
int mbedtls_sha256_finish_ret(mbedtls_sha256_context* h, uint8_t* out) {
  memset(out, h->count == otaExpectedSize ? 0 : 255, 32); return 0;
}
void mbedtls_sha256_free(mbedtls_sha256_context*) {}

struct UpdateMock {
  std::vector<uint8_t> written;
  void abort() { ++aborts; written.clear(); }
  bool begin(uint32_t, int) { ++begins; written.clear(); return !beginFails; }
  const char* errorString() { return "flash error"; }
  size_t write(const uint8_t* data, size_t n) {
    if (scenarios.at(connection).writeFail) return 0;
    written.insert(written.end(), data, data + n); return n;
  }
} Update;

struct WiFiClient {
  int available() {
    auto& s = scenarios.at(connection);
    if (s.stall) return 0;
    if (s.stopAt >= 0 && position >= s.stopAt) { isConnected = false; return -1; }
    if (position >= int(otaExpectedSize)) { isConnected = false; return 0; }
    int n = std::min(2048, int(otaExpectedSize) - position);
    return s.stopAt >= 0 ? std::min(n, s.stopAt - position) : n;
  }
  int read(uint8_t* data, size_t n) {
    for (size_t i = 0; i < n; ++i) data[i] = uint8_t((position + i) % 251);
    position += int(n); return int(n);
  }
};
struct WiFiClientSecure {
  void setCACert(const char*) {}
  void setHandshakeTimeout(int) {}
  int lastError(char*, size_t) { return 0; }
};
struct HTTPClient {
  WiFiClient stream;
  void setConnectTimeout(int) {}
  void setTimeout(int) {}
  void setFollowRedirects(int) {}
  void setRedirectLimit(int) {}
  void setUserAgent(const char*) {}
  bool begin(WiFiClientSecure&, const std::string&) { return true; }
  int GET() { ++connection; position = 0; isConnected = true; return scenarios.at(connection).status; }
  static std::string errorToString(int) { return "test error"; }
  int getSize() { return scenarios.at(connection).size; }
  WiFiClient* getStreamPtr() { return &stream; }
  bool connected() { return isConnected; }
  void end() { isConnected = false; }
};
'''

checks = r'''
void reset(std::vector<Scenario> values) {
  scenarios = values; connection = -1; position = aborts = begins = hashStarts = 0;
  clockMs = 0; cancelOnDelay = beginFails = false; Update.written.clear();
  githubExpectedHash = std::string(64, '0'); githubUpdateError[0] = 0;
  githubUpdateBytes.store(0); githubUpdateAttempt.store(0);
  githubUpdateState.store(0); githubUpdateCancel.store(false);
}
void checkImage() {
  assert(Update.written.size() == otaExpectedSize);
  for (size_t i = 0; i < Update.written.size(); ++i) assert(Update.written[i] == i % 251);
}
int main() {
  Scenario broken; broken.stopAt = 49152;
  Scenario ok;
  reset({broken, ok}); githubDownloadTask(nullptr);
  assert(githubUpdateState == GH_DOWNLOADED && githubUpdateAttempt == 2);
  assert(strstr(githubRetryErrors[0], "unterbrochen"));
  assert(aborts == 1 && begins == 1 && hashStarts == 2); checkImage();

  reset({broken, broken, broken}); githubDownloadTask(nullptr);
  assert(githubUpdateState == GH_FAILED && githubUpdateAttempt == 3);
  assert(aborts == 2 && begins == 2 && strstr(githubUpdateError, "unterbrochen"));

  Scenario wrongSize; wrongSize.size = 123;
  reset({wrongSize}); githubDownloadTask(nullptr);
  assert(githubUpdateState == GH_FAILED && githubUpdateAttempt == 1 && begins == 0);

  reset({ok}); githubExpectedHash = std::string(64, 'a'); githubDownloadTask(nullptr);
  assert(githubUpdateState == GH_FAILED && githubUpdateAttempt == 1 && begins == 0);
  assert(strstr(githubUpdateError, "SHA-256"));

  Scenario writeFail; writeFail.writeFail = true;
  reset({writeFail}); githubDownloadTask(nullptr);
  assert(githubUpdateState == GH_FAILED && githubUpdateAttempt == 1 && begins == 0);

  Scenario unavailable; unavailable.status = 503;
  reset({unavailable, ok}); githubDownloadTask(nullptr);
  assert(githubUpdateState == GH_DOWNLOADED && githubUpdateAttempt == 2); checkImage();

  Scenario missing; missing.status = 404;
  reset({missing}); githubDownloadTask(nullptr);
  assert(githubUpdateState == GH_FAILED && githubUpdateAttempt == 1 && begins == 0);

  Scenario stall; stall.stall = true;
  reset({stall, ok}); githubDownloadTask(nullptr);
  assert(githubUpdateState == GH_DOWNLOADED && githubUpdateAttempt == 2); checkImage();

  reset({broken, ok}); cancelOnDelay = true; githubDownloadTask(nullptr);
  assert(githubUpdateState == GH_FAILED && begins == 0 && strstr(githubUpdateError, "abgebrochen"));

  reset({broken, ok}); beginFails = true; githubDownloadTask(nullptr);
  assert(githubUpdateState == GH_FAILED && begins == 1 && strstr(githubUpdateError, "flash error"));
  puts("PASS: Download-Neuversuch, Neustart bei Byte 0, Fehler, Abbruch und Pruefsummenkontrolle.");
}
'''

with tempfile.TemporaryDirectory(prefix='backload-ota-test-') as temp:
    source = pathlib.Path(temp) / 'github_download_test.cpp'
    binary = pathlib.Path(temp) / 'github_download_test'
    source.write_text(stubs + download_task + checks)
    subprocess.run(['c++', '-std=c++11', '-Wall', '-Wextra', '-Werror', str(source), '-o', str(binary)], check=True)
    subprocess.run([str(binary)], check=True)
