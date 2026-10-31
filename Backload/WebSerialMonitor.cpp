#include <Arduino.h>
#include <WiFi.h>
#include <AsyncTCP.h>
#include <ESPAsyncWebServer.h>
#include "src/RisalDash/src/RisalUI.h"
#include "src/AsyncWebConsole/src/AsyncWebConsole.h"
#include "DashboardStyles.h"
#include "WebSerialMonitor.h"
#include <atomic>

extern const char INDEX_HTML[];
// Die neue Firmware erst nach der Startprüfung in loop() bestätigen.
extern "C" bool verifyRollbackLater() { return true; }

namespace {
RisalUI dashboard("Solarsteuerung");
AsyncWebServer consoleServer(81);
AsyncWebConsole* monitor = nullptr;
bool started = false;

AsyncWebConsole::Config monitorConfig() {
  AsyncWebConsole::Config config;
  config.queueLen = 64;
  config.timestamps = false; // Datum und Zeit kommen aus dem gemeinsamen Logger.
  config.fileLogEnable = false;
  return config;
}

class MirroredLog : public Print {
  char line[512] = {};
  size_t used = 0;
  unsigned long lastWrite = 0;
  void flushLine() {
    if (!used) return;
    line[used] = '\0';
    if (monitor) monitor->print(line);
    used = 0;
  }
 public:
  size_t write(uint8_t c) override { return write(&c, 1); }
  size_t write(const uint8_t* data, size_t length) override {
    const size_t written = Serial.write(data, length);
    if (!monitor) return written;
    for (size_t i = 0; i < length; ++i) {
      line[used++] = char(data[i]);
      if (data[i] == '\n' || used == sizeof(line) - 1) flushLine();
    }
    lastWrite = millis();
    return written;
  }
  void flushPending() {
    if (used && millis() - lastWrite >= 250) flushLine();
  }
};
MirroredLog mirroredLog;

// The renderer streams the existing controls inside the RisalDash page. The
// JavaScript continues to use the public, same-origin /api/* endpoints.
class ControlWidget : public Widget {
 public:
  ControlWidget() : Widget("steuerung", "Steuerung") {}
  const char* typeId() const override { return "backload"; }
  const char* css() const override { return BACKLOAD_DASH_CSS; }
  void card(Print& out) override {
    const char* start = strstr(INDEX_HTML, "<body><main>");
    const char* end = start ? strstr(start, "</main></body></html>") : nullptr;
    if (!start || !end) return;
    start += strlen("<body><main>");
    out.print(F("<div class=\"backload\">"));
    out.write(reinterpret_cast<const uint8_t*>(start), end - start);
    out.print(F("</div>"));
  }
};
ControlWidget controlWidget;

// The original WebServer remains bound to 127.0.0.1:8080. AsyncTCP forwards
// HTTP without blocking its event task, keeping pump and NVS work in loop().
constexpr size_t MAX_PROXY_BYTES = 32768;
constexpr unsigned MAX_PROXY_CLIENTS = 8;
unsigned activeProxies = 0;  // only accessed by AsyncTCP callbacks
// AsyncTCP callbacks publish counters only. The main loop performs all logging
// because the shared FirmwareLog buffer is not safe for concurrent writers.
std::atomic<uint32_t> otaProxyRequests(0), otaProxyConnections(0), otaProxyComplete(0), otaProxyFailed(0);
struct ProxyContext {
  String request;
  String response;
  size_t requestSent = 0;
  bool ready = false;
  bool responseAlive = true;
  bool overflow = false;
  bool otaStart = false;
};
void sendProxyData(ProxyContext* ctx, AsyncClient* client) {
  if (!ctx->request.length()) return;
  const size_t remaining = ctx->request.length() - ctx->requestSent;
  const size_t available = client->space();
  const size_t count = remaining < available ? remaining : available;
  if (!count) return;
  ctx->requestSent += client->write(ctx->request.c_str() + ctx->requestSent, count);
  if (ctx->requestSent == ctx->request.length()) ctx->request = "";
}
const char* BAD_GATEWAY = "HTTP/1.1 502 Bad Gateway\r\nContent-Type: text/plain; charset=utf-8\r\nContent-Length: 29\r\nConnection: close\r\n\r\nSteuerung nicht erreichbar.\r\n";

class ProxyResponse : public AsyncWebServerResponse {
  ProxyContext* ctx;
  size_t sent = 0;
  size_t acked = 0;
 public:
  explicit ProxyResponse(ProxyContext* c) : ctx(c) {}
  ~ProxyResponse() override { ctx->responseAlive = false; if (ctx->ready) delete ctx; }
  bool _sourceValid() const override { return true; }
  void _respond(AsyncWebServerRequest*) override { _state = RESPONSE_CONTENT; }
  size_t _ack(AsyncWebServerRequest* req, size_t len, uint32_t) override {
    acked += len;
    if (!ctx->ready) return 0;
    size_t remain = ctx->response.length() - sent;
    size_t space = req->client()->space();
    size_t count = remain < space ? remain : space;
    if (count) { count = req->client()->write(ctx->response.c_str() + sent, count); sent += count; }
    if (sent == ctx->response.length() && acked >= sent) _state = RESPONSE_END;
    return count;
  }
};

String urlEncode(const String& s) {
  const char* hex = "0123456789ABCDEF";
  String out;
  for (size_t i = 0; i < s.length(); ++i) {
    unsigned char c = s[i];
    if ((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') ||
        (c >= '0' && c <= '9') || c == '-' || c == '_' || c == '.' || c == '~') out += char(c);
    else { out += '%'; out += hex[c >> 4]; out += hex[c & 15]; }
  }
  return out;
}

bool completeHttpResponse(const String& response) {
  int split = response.indexOf("\r\n\r\n");
  if (split < 0 || !response.startsWith("HTTP/1.")) return false;
  int pos = response.indexOf("Content-Length:");
  if (pos < 0 || pos >= split) return false;
  int end = response.indexOf("\r\n", pos);
  long expected = response.substring(pos + 15, end).toInt();
  return expected >= 0 && response.length() - size_t(split + 4) == size_t(expected);
}

void proxyRequest(AsyncWebServerRequest* req) {
  if (activeProxies >= MAX_PROXY_CLIENTS) { req->send(503, "text/plain", "Steuerung ist beschäftigt. Bitte erneut versuchen."); return; }
  String args;
  for (size_t i = 0; i < req->params(); ++i) {
    const AsyncWebParameter* param = req->getParam(i);
    if (param->isFile()) { req->send(400, "text/plain", "Dateien werden nicht unterstützt."); return; }
    if (args.length()) args += '&';
    args += urlEncode(param->name()) + '=' + urlEncode(param->value());
  }
  const size_t maxArgs = req->url() == "/api/update/chunk" ? 20000 : 2048;
  if (args.length() > maxArgs) { req->send(413, "text/plain", "Anfrage zu groß."); return; }
  const bool post = req->method() == HTTP_POST;
  String target = req->url();
  if (!post && args.length()) target += '?' + args;
  ProxyContext* ctx = new ProxyContext();
  AsyncClient* upstream = new AsyncClient();
  if (!ctx || !upstream) { delete ctx; delete upstream; req->send(503, "text/plain", "Nicht genügend Speicher."); return; }
  ctx->otaStart = req->url() == "/api/update/prepare" || req->url() == "/api/update/github";
  if (ctx->otaStart) otaProxyRequests.fetch_add(1);
  ctx->request = String(post ? "POST " : "GET ") + target + " HTTP/1.0\r\nHost: localhost\r\nConnection: close\r\n";
  if (req->hasHeader("X-Requested-With") && req->getHeader("X-Requested-With")->value() == "HeatingUI")
    ctx->request += "X-Requested-With: HeatingUI\r\n";
  if (post) ctx->request += "Content-Type: application/x-www-form-urlencoded\r\nContent-Length: " + String(args.length()) + "\r\n";
  ctx->request += "\r\n";
  if (post) ctx->request += args;
  ++activeProxies;
  upstream->onConnect([](void* arg, AsyncClient* client) {
    ProxyContext* c = static_cast<ProxyContext*>(arg);
    if (c->otaStart) otaProxyConnections.fetch_add(1);
    sendProxyData(c, client);
  }, ctx);
  upstream->onAck([](void* arg, AsyncClient* client, size_t, uint32_t) {
    sendProxyData(static_cast<ProxyContext*>(arg), client);
  }, ctx);
  upstream->onData([](void* arg, AsyncClient* client, void* data, size_t len) {
    ProxyContext* c = static_cast<ProxyContext*>(arg);
    if (c->response.length() + len > MAX_PROXY_BYTES || !c->response.concat(static_cast<const char*>(data), len)) {
      c->overflow = true;
      client->close(true);
    }
  }, ctx);
  upstream->onTimeout([](void*, AsyncClient* client, uint32_t) { client->close(true); }, ctx);
  upstream->onError([](void*, AsyncClient* client, int8_t) { client->close(true); }, ctx);
  upstream->onDisconnect([](void* arg, AsyncClient* client) {
    ProxyContext* c = static_cast<ProxyContext*>(arg);
    const bool failed = c->overflow || !completeHttpResponse(c->response);
    if (c->otaStart) {
      if (failed) otaProxyFailed.fetch_add(1);
      else otaProxyComplete.fetch_add(1);
    }
    if (failed) c->response = BAD_GATEWAY;
    c->ready = true;
    --activeProxies;
    delete client;
    if (!c->responseAlive) delete c;
  }, ctx);
  upstream->setRxTimeout(8);
  req->send(new ProxyResponse(ctx));
  if (!upstream->connect(IPAddress(127, 0, 0, 1), 8080)) {
    if (ctx->otaStart) otaProxyFailed.fetch_add(1);
    ctx->response = BAD_GATEWAY;
    ctx->ready = true;
    --activeProxies;
    delete upstream;
  }
}
void proxyUpdateRequest(AsyncWebServerRequest* req) {
  if (!req->hasHeader("X-Requested-With") || req->getHeader("X-Requested-With")->value() != "HeatingUI") {
    req->send(403, "text/plain", "Bitte das Webinterface verwenden."); return;
  }
  proxyRequest(req);
}
} // namespace

Print& FirmwareLog = mirroredLog;

void beginWebSerialMonitor() {
  if (started) return;
  monitor = new AsyncWebConsole("/konsole-ws", 0, monitorConfig());
  monitor->attachTo(consoleServer, "/");
  consoleServer.begin();
  dashboard.server().on("/wlan", HTTP_GET, [](AsyncWebServerRequest* req) { dashboard.servePage(req); });
  dashboard.server().on("/testwerte", HTTP_GET, [](AsyncWebServerRequest* req) { dashboard.servePage(req); });
  dashboard.server().on("/verlauf", HTTP_GET, [](AsyncWebServerRequest* req) { dashboard.servePage(req); });
  dashboard.server().on("/update", HTTP_GET, [](AsyncWebServerRequest* req) { dashboard.servePage(req); });
  dashboard.server().on("/info", HTTP_GET, [](AsyncWebServerRequest* req) { dashboard.servePage(req); });
  dashboard.server().on("/webserial", HTTP_GET, [](AsyncWebServerRequest* req) {
    req->redirect(String("http://") + req->client()->localIP().toString() + ":81/");
  });
  dashboard.server().on("/api/status", HTTP_GET, proxyRequest);
  dashboard.server().on("/api/history", HTTP_GET, proxyRequest);
  dashboard.server().on("/api/device-info", HTTP_GET, proxyRequest);
  dashboard.server().on("/api/settings", HTTP_POST, proxyRequest);
  dashboard.server().on("/api/pump", HTTP_POST, proxyRequest);
  dashboard.server().on("/api/automatic", HTTP_POST, proxyRequest);
  dashboard.server().on("/api/test-mode", HTTP_POST, proxyRequest);
  dashboard.server().on("/api/simulation", HTTP_POST, proxyRequest);
  dashboard.server().on("/api/wifi", HTTP_POST, proxyRequest);
  dashboard.server().on("/api/wifi/reset", HTTP_POST, proxyRequest);
  dashboard.server().on("/api/wifi/scan", HTTP_GET | HTTP_POST, proxyRequest);
  dashboard.server().on("/api/update/prepare", HTTP_POST, proxyUpdateRequest);
  dashboard.server().on("/api/update/chunk", HTTP_POST, proxyUpdateRequest);
  dashboard.server().on("/api/update/finish", HTTP_POST, proxyUpdateRequest);
  dashboard.server().on("/api/update/abort", HTTP_POST, proxyUpdateRequest);
  dashboard.server().on("/api/update/github", HTTP_POST, proxyUpdateRequest);
  dashboard.server().on("/api/update/github-status", HTTP_POST, proxyUpdateRequest);
  dashboard.brand("Solarsteuerung").theme(RisalUI::DARK).effects(false);
  dashboard.addExternalWidget(controlWidget);
  dashboard.beginOnExistingWiFi();
  started = true;
}

void processWebSerialMonitor() {
  static uint32_t requests = 0, connections = 0, complete = 0, failed = 0;
  const uint32_t newRequests = otaProxyRequests.load(), newConnections = otaProxyConnections.load();
  const uint32_t newComplete = otaProxyComplete.load(), newFailed = otaProxyFailed.load();
  if (requests != newRequests || connections != newConnections || complete != newComplete || failed != newFailed) {
    requests = newRequests; connections = newConnections; complete = newComplete; failed = newFailed;
    FirmwareLog.printf("OTA-Diagnose: Web-Weiterleitung: Anfragen=%lu, verbunden=%lu, Antworten=%lu, Fehler=%lu.\n",
      (unsigned long)requests, (unsigned long)connections, (unsigned long)complete, (unsigned long)failed);
  }
  mirroredLog.flushPending();
  if (started) dashboard.update();
}
