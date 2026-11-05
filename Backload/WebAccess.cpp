#include "WebAccess.h"
#include <Preferences.h>
#include <esp_system.h>
#include <esp_timer.h>
#include <mbedtls/md.h>
#include <mbedtls/pkcs5.h>
#include <mbedtls/sha256.h>
#include <freertos/FreeRTOS.h>
#include <freertos/semphr.h>
#include <string.h>

namespace {
constexpr uint32_t RECORD_MAGIC = 0x424c4131;
constexpr uint32_t ITERATIONS = 12000;
constexpr int64_t SESSION_US = 12LL * 60 * 60 * 1000000;
constexpr size_t SESSION_COUNT = 8;
struct Record { uint32_t magic; uint8_t enabled; uint8_t salt[16]; uint8_t hash[32]; uint8_t checksum[32]; };
struct Session { char token[65]; int64_t until; };
Record record = {};
Session sessions[SESSION_COUNT] = {};
SemaphoreHandle_t sessionLock = nullptr;
void (*closeWebSockets)() = nullptr;
bool broken = true;
uint8_t failures = 0;
int64_t blockedUntil = 0;

const char LOGIN_HTML[] PROGMEM = R"HTML(<!doctype html>
<html lang="de"><head><meta charset="utf-8"><meta name="viewport" content="width=device-width,initial-scale=1"><title>Anmeldung · Solarsteuerung</title>
<script>(function(){var t='dark';try{t=localStorage.getItem('rd-th')||t;}catch(e){}var light=t==='light'||(t==='auto'&&!matchMedia('(prefers-color-scheme:dark)').matches);document.documentElement.classList.add(light?'light':'dark');})();</script>
<style>
:root{font-family:system-ui,sans-serif;color-scheme:dark;--bg:#101923;--surface:#1c2a35;--ink:#eaf1f1;--muted:#c2d0d1;--border:#758c91;--field:#101923;--error:#ffd0c8;--button:#207e79}
html.light{color-scheme:light;--bg:#f3f5f4;--surface:#fff;--ink:#192f35;--muted:#4e6268;--border:#8c9fa3;--field:#fff;--error:#a32620;--button:#17636b}
*{box-sizing:border-box}body{margin:0;padding:8vh 20px;background:var(--bg);color:var(--ink)}main{max-width:420px;margin:auto;padding:28px;border-radius:16px;background:var(--surface)}h1{margin:0 0 12px;font-size:1.8rem;letter-spacing:-.02em}p{line-height:1.5;color:var(--muted)}label{display:block;font-weight:600}input,button{display:block;width:100%;min-height:46px;font:inherit;border-radius:9px}input{margin:8px 0 18px;padding:10px;border:1px solid var(--border);background:var(--field);color:var(--ink)}button{padding:10px;border:0;background:var(--button);color:#fff;font-weight:650;cursor:pointer}button:hover{filter:brightness(.9)}:focus-visible{outline:3px solid #e5a23d;outline-offset:3px}#message{min-height:1.5em;color:var(--error)}@media(max-width:480px){body{padding:24px 16px}main{padding:22px}}
</style></head><body><main><h1>Solarsteuerung</h1><p>Bitte mit dem Zugangspasswort anmelden.</p><form id="login"><label for="password">Passwort</label><input id="password" type="password" autocomplete="current-password" maxlength="128" required><button>Anmelden</button></form><p id="message" role="status"></p></main>
<script>document.getElementById('login').addEventListener('submit',async e=>{e.preventDefault();let m=document.getElementById('message'),p=document.getElementById('password');m.textContent='Anmeldung wird geprüft …';try{let r=await fetch('/auth/login',{method:'POST',headers:{'Content-Type':'application/x-www-form-urlencoded','X-Requested-With':'HeatingUI'},body:new URLSearchParams({password:p.value})});p.value='';if(!r.ok)throw Error(await r.text());location.replace('/');}catch(err){m.textContent=err.message||'Anmeldung fehlgeschlagen.';}});</script></body></html>)HTML";

bool derive(const String& password, const uint8_t* salt, uint8_t* out) {
  mbedtls_md_context_t ctx;
  mbedtls_md_init(&ctx);
  const mbedtls_md_info_t* md = mbedtls_md_info_from_type(MBEDTLS_MD_SHA256);
  int rc = md ? mbedtls_md_setup(&ctx, md, 1) : -1;
  if (!rc) rc = mbedtls_pkcs5_pbkdf2_hmac(&ctx,
      reinterpret_cast<const unsigned char*>(password.c_str()), password.length(),
      salt, 16, ITERATIONS, 32, out);
  mbedtls_md_free(&ctx);
  return rc == 0;
}
bool constantEqual(const uint8_t* a, const uint8_t* b, size_t n) {
  uint8_t diff = 0;
  for (size_t i = 0; i < n; ++i) diff |= a[i] ^ b[i];
  return diff == 0;
}
bool recordChecksum(Record& r, bool verify) {
  uint8_t digest[32];
  if (mbedtls_sha256_ret(reinterpret_cast<const uint8_t*>(&r), offsetof(Record, checksum), digest, 0) != 0) return false;
  if (verify) return constantEqual(digest, r.checksum, sizeof(digest));
  memcpy(r.checksum, digest, sizeof(digest));
  return true;
}
bool validPassword(const String& p) {
  return p.length() >= 8 && p.length() <= 128 && strlen(p.c_str()) == p.length();
}
void invalidateSessions() {
  if (sessionLock && xSemaphoreTake(sessionLock, portMAX_DELAY) == pdTRUE) {
    memset(sessions, 0, sizeof(sessions));
    xSemaphoreGive(sessionLock);
  }
  if (closeWebSockets) closeWebSockets();
}
String createSession() {
  if (!sessionLock) return String();
  uint8_t random[32]; esp_fill_random(random, sizeof(random));
  static const char hex[] = "0123456789abcdef";
  char token[65];
  for (size_t i = 0; i < sizeof(random); ++i) { token[2*i] = hex[random[i] >> 4]; token[2*i+1] = hex[random[i] & 15]; }
  token[64] = 0;
  const int64_t now = esp_timer_get_time();
  if (xSemaphoreTake(sessionLock, portMAX_DELAY) != pdTRUE) return String();
  size_t slot = SESSION_COUNT;
  for (size_t i = 0; i < SESSION_COUNT; ++i) if (!sessions[i].until) { slot = i; break; }
  // Do not silently evict a live session: its open websocket would remain active.
  if (slot < SESSION_COUNT) { memcpy(sessions[slot].token, token, sizeof(token)); sessions[slot].until = now + SESSION_US; }
  xSemaphoreGive(sessionLock);
  return slot < SESSION_COUNT ? String(token) : String();
}
bool authenticated(AsyncWebServerRequest* req) {
  if (broken || !sessionLock) return false;
  if (!record.enabled) return true;
  if (!req->hasHeader("Cookie")) return false;
  String cookies = req->getHeader("Cookie")->value();
  String token;
  int start = 0;
  while (start < int(cookies.length())) {
    int end = cookies.indexOf(';', start); if (end < 0) end = cookies.length();
    String part = cookies.substring(start, end); part.trim();
    if (part.startsWith("backload_session=")) { token = part.substring(17); break; }
    start = end + 1;
  }
  if (token.length() != 64) return false;
  const int64_t now = esp_timer_get_time(); bool match = false;
  if (xSemaphoreTake(sessionLock, portMAX_DELAY) != pdTRUE) return false;
  for (size_t i = 0; i < SESSION_COUNT; ++i)
    if (sessions[i].until > now && constantEqual(reinterpret_cast<const uint8_t*>(sessions[i].token), reinterpret_cast<const uint8_t*>(token.c_str()), 64)) match = true;
  xSemaphoreGive(sessionLock);
  return match;
}
bool sameOrigin(AsyncWebServerRequest* req) {
  if (!req->hasHeader("Origin") || !req->hasHeader("Host")) return false;
  const String origin = req->getHeader("Origin")->value();
  const String host = req->getHeader("Host")->value();
  return origin == "http://" + host || origin == "https://" + host;
}
void noCache(AsyncWebServerResponse* res) { res->addHeader("Cache-Control", "no-store"); res->addHeader("X-Content-Type-Options", "nosniff"); }
void page(AsyncWebServerRequest* req, const char* html) {
  AsyncWebServerResponse* res = req->beginResponse_P(200, "text/html; charset=utf-8", html); noCache(res); req->send(res);
}
void login(AsyncWebServerRequest* req) {
  int64_t now = esp_timer_get_time();
  if (broken) { req->send(503, "text/plain; charset=utf-8", "Passwortspeicher nicht verfügbar."); return; }
  if (!record.enabled) { req->send(400, "text/plain; charset=utf-8", "Kein Passwort eingerichtet."); return; }
  if (now < blockedUntil) { req->send(429, "text/plain; charset=utf-8", "Zu viele Versuche. Bitte eine Minute warten."); return; }
  String password = req->hasParam("password", true) ? req->getParam("password", true)->value() : String();
  uint8_t hash[32] = {};
  bool ok = validPassword(password) && derive(password, record.salt, hash) && constantEqual(hash, record.hash, sizeof(hash));
  password = ""; memset(hash, 0, sizeof(hash));
  if (!ok) {
    if (++failures >= 5) { failures = 0; blockedUntil = now + 60LL * 1000000; }
    req->send(401, "text/plain; charset=utf-8", "Passwort falsch."); return;
  }
  failures = 0; blockedUntil = 0;
  String token = createSession();
  if (!token.length()) { req->send(503, "text/plain; charset=utf-8", "Zu viele aktive Sitzungen. Bitte später erneut versuchen."); return; }
  AsyncWebServerResponse* res = req->beginResponse(200, "application/json", "{\"ok\":true}");
  res->addHeader("Set-Cookie", "backload_session=" + token + "; Path=/; HttpOnly; SameSite=Strict; Max-Age=43200");
  noCache(res); req->send(res);
}
void changePassword(AsyncWebServerRequest* req) {
  if (!req->hasParam("password", true)) { req->send(400, "text/plain; charset=utf-8", "Passwort fehlt."); return; }
  String password = req->getParam("password", true)->value();
  if (password.length() && !validPassword(password)) { req->send(400, "text/plain; charset=utf-8", "Passwort muss 8 bis 128 Bytes lang sein."); return; }
  Record next = {}; next.magic = RECORD_MAGIC; next.enabled = password.length() != 0;
  if (next.enabled) { esp_fill_random(next.salt, sizeof(next.salt)); if (!derive(password, next.salt, next.hash)) { req->send(500, "text/plain; charset=utf-8", "Passwort konnte nicht verarbeitet werden."); return; } }
  password = "";
  if (!recordChecksum(next, false)) { req->send(500, "text/plain; charset=utf-8", "Passwort konnte nicht verarbeitet werden."); return; }
  Preferences prefs;
  if (!prefs.begin("backload-auth", false)) { req->send(503, "text/plain; charset=utf-8", "Passwortspeicher nicht verfügbar."); return; }
  bool saved = prefs.putBytes("auth1", &next, sizeof(next)) == sizeof(next);
  Record check = {};
  saved = saved && prefs.getBytesLength("auth1") == sizeof(check) && prefs.getBytes("auth1", &check, sizeof(check)) == sizeof(check) && constantEqual(reinterpret_cast<const uint8_t*>(&next), reinterpret_cast<const uint8_t*>(&check), sizeof(next));
  prefs.end();
  if (!saved) { broken = true; invalidateSessions(); req->send(500, "text/plain; charset=utf-8", "Speicherfehler; Passwortschutz bleibt gesperrt."); return; }
  record = next; broken = false; invalidateSessions();
  AsyncWebServerResponse* res = req->beginResponse(200, "application/json", "{\"saved\":true}");
  res->addHeader("Set-Cookie", "backload_session=; Path=/; HttpOnly; SameSite=Strict; Max-Age=0"); noCache(res); req->send(res);
}
} // namespace

void beginWebAccess(void (*closeSockets)()) {
  broken = true;
  closeWebSockets = closeSockets; sessionLock = xSemaphoreCreateMutex();
  Preferences prefs;
  if (!sessionLock || !prefs.begin("backload-auth", false)) return;
  if (!prefs.isKey("auth1")) { record = {}; record.magic = RECORD_MAGIC; broken = false; }
  else {
    Record saved = {};
    if (prefs.getBytesLength("auth1") == sizeof(saved) && prefs.getBytes("auth1", &saved, sizeof(saved)) == sizeof(saved) && saved.magic == RECORD_MAGIC && saved.enabled <= 1 && recordChecksum(saved, true)) { record = saved; broken = false; }
  }
  prefs.end();
}
void attachWebAccess(AsyncWebServer& server, bool mainServer) {
  server.addMiddleware([mainServer](AsyncWebServerRequest* req, ArMiddlewareNext next) {
    const String path = req->url();
    const bool publicStatus = mainServer && req->method() == HTTP_GET && path == "/api/status";
    const bool loginPage = req->method() == HTTP_GET && path == "/anmelden";
    const bool loginRequest = req->method() == HTTP_POST && path == "/auth/login";
    const bool websocket = path == "/ws" || path == "/konsole-ws";
    if ((req->method() != HTTP_GET || websocket) && !sameOrigin(req)) { req->send(403, "text/plain; charset=utf-8", "Ungültiger Ursprung."); return; }
    if (req->method() != HTTP_GET && (!req->hasHeader("X-Requested-With") || req->getHeader("X-Requested-With")->value() != "HeatingUI")) { req->send(403, "text/plain; charset=utf-8", "Bitte das Webinterface verwenden."); return; }
    if (publicStatus || loginPage || loginRequest || authenticated(req)) { next(); return; }
    if (req->method() == HTTP_GET && !websocket && !path.startsWith("/api/") && !path.startsWith("/auth/")) req->redirect("/anmelden");
    else req->send(401, "text/plain; charset=utf-8", "Anmeldung erforderlich.");
  });
  server.on("/anmelden", HTTP_GET, [](AsyncWebServerRequest* req) { page(req, LOGIN_HTML); });
  server.on("/auth/login", HTTP_POST, login);
  if (mainServer) {
    server.on("/auth/password", HTTP_POST, changePassword);
  }
}
void processWebAccess() {
  if (!sessionLock) return;
  bool expired = false; const int64_t now = esp_timer_get_time();
  if (xSemaphoreTake(sessionLock, 0) != pdTRUE) return;
  for (size_t i = 0; i < SESSION_COUNT; ++i) if (sessions[i].until && sessions[i].until <= now) { memset(&sessions[i], 0, sizeof(sessions[i])); expired = true; }
  xSemaphoreGive(sessionLock);
  if (expired && closeWebSockets) closeWebSockets();
}
