#pragma once

extern const char INDEX_HTML[] PROGMEM = R"HTML(<!doctype html>
<html lang="de">
<head>
<meta charset="utf-8">
<meta name="viewport" content="width=device-width, initial-scale=1">
<title>Schubis Solarsystemsteuerung</title>
<style>
:root{font-family:system-ui,sans-serif;color:#192f35;background:#f3f5f4;color-scheme:light}
*{box-sizing:border-box}body{margin:0}main{max-width:860px;margin:auto;padding:36px 22px}
h1{font-size:clamp(1.7rem,5vw,2.4rem);margin:0 0 12px}h2{font-size:1.25rem;margin:0}
.dashboard-header{display:flex;align-items:center;justify-content:space-between;gap:18px 28px;flex-wrap:wrap}.header-status{margin-left:auto;text-align:right}
.page-nav a{display:inline-flex;align-items:center;gap:7px}.nav-icon{width:18px;height:18px;flex:none}body:not(.organigram-page) #organigram-section{display:none}body.organigram-page main>:not(#organigram-section):not(.page-nav):not(script):not(#test-message){display:none}
p{line-height:1.6}header{margin-bottom:30px}.muted{color:#4e6268}
section{border-top:1px solid #b9c8c9;padding:26px 0} .heading,.readings{display:flex;gap:18px;justify-content:space-between;align-items:center;flex-wrap:wrap}
.readings{justify-content:flex-start;margin:20px 0;font-variant-numeric:tabular-nums}.temperature{font-size:1.7rem;font-weight:650}
.sensor-heading{display:flex;justify-content:space-between;align-items:flex-start;gap:10px}.sensor-heading h2{min-width:0}.sensor-status{display:inline-flex;flex:none;width:24px;height:24px;color:#4e6268}.sensor-status svg{width:100%;height:100%}.sensor-status[data-state="ok"]{color:#08705c}.sensor-status[data-state="error"]{color:#b42318}
.badge{padding:6px 12px;border-radius:6px;background:#e0e7e7;font-weight:600}.on{background:#c9e9d9;color:#145239}.error{color:#a32620}
form{display:grid;grid-template-columns:1fr 1fr auto;gap:16px;align-items:end}label{display:block;font-weight:600;font-size:.95rem}
input{min-width:0;display:block;width:100%;min-height:46px;margin-top:8px;padding:10px;border:1px solid #8c9fa3;border-radius:6px;font:inherit;background:white;color:inherit}
button{min-height:46px;padding:10px 20px;border:0;border-radius:6px;background:#17636b;color:white;font:inherit;font-weight:600;cursor:pointer}
button:hover{background:#104d53}button:disabled{opacity:.55;cursor:wait}:focus-visible{outline:3px solid #bd650c;outline-offset:3px}::selection{background:#c9e9d9}
.stepper{display:grid;grid-template-columns:46px minmax(0,1fr) 46px;gap:6px;margin-top:8px}
.stepper input{margin:0;text-align:center;appearance:textfield;-moz-appearance:textfield}
.stepper input::-webkit-inner-spin-button,.stepper input::-webkit-outer-spin-button{-webkit-appearance:none;margin:0}
.stepper button{padding:0;font-size:1.35rem;line-height:1;touch-action:manipulation}
.relay-switch{display:inline-flex;align-items:center;gap:12px;background:white;color:#192f35;border:1px solid #8c9fa3;touch-action:manipulation}
.relay-switch:hover{background:#e0e7e7}.switch-track{display:block;width:50px;height:28px;padding:3px;border-radius:14px;background:#4e6268}
.switch-track::after{content:'';display:block;width:22px;height:22px;border-radius:50%;background:white}
.relay-switch[aria-checked="true"] .switch-track{background:#17636b}.relay-switch[aria-checked="true"] .switch-track::after{transform:translateX(22px)}
.pump-flow{display:block;width:100%;max-width:600px;margin:24px auto 8px;color:#17636b}.pump-flow rect{fill:none;stroke:#8c9fa3}.pump-flow text{fill:#192f35;font:24px system-ui,sans-serif}.pump-flow>path{fill:none;stroke:currentColor;stroke-width:3}.pump-flow.stopped{color:#4e6268}.pump-flow:not(.stopped)>path{stroke-dasharray:12 8;animation:flow 1s linear infinite}@keyframes flow{to{stroke-dashoffset:-20}}@media(prefers-reduced-motion:reduce){.pump-flow:not(.stopped)>path{animation:none}}
.transfer-pump-body{fill:var(--bg2,#fff);stroke:currentColor;stroke-width:2}.transfer-pump-rotor{fill:currentColor;transform-box:fill-box;transform-origin:center}.storage-overview.motion-visible .pump-flow:not(.stopped) .transfer-pump-rotor{animation:transfer-pump-spin 3s linear infinite}.storage-overview.motion-visible .pump-flow[data-direction="ccw"]:not(.stopped) .transfer-pump-rotor{animation-direction:reverse}@keyframes transfer-pump-spin{to{transform:rotate(360deg)}}@media(prefers-reduced-motion:reduce){.storage-overview.motion-visible .pump-flow:not(.stopped) .transfer-pump-rotor{animation:none}}
.storage-overview{display:grid;grid-template-columns:160px minmax(0,1fr);align-items:center;gap:24px;max-width:784px;margin:22px auto 8px}.storage-overview .pump-flow{width:100%;max-width:none;margin:0}.burner-diagram{display:grid;gap:4px;text-align:center;color:var(--ink2,#4e6268);line-height:1.4}.burner-icon{display:block;width:128px;max-width:100%;margin:0 auto}.burner-body{fill:var(--bg2,#fff)}.burner-indicator{fill:currentColor}.burner-diagram strong{color:var(--ink1,#192f35);font-size:1rem}.burner-diagram output{font-size:.88rem;font-weight:700}.burner-diagram>span{font-size:.78rem;color:var(--ink2,#4e6268)}.burner-diagram.released{color:#ffb454}html.light .burner-diagram.released{color:#9c4b0b}
@media(max-width:700px){.storage-overview{grid-template-columns:minmax(0,1fr);gap:12px}.storage-overview .pump-flow{grid-row:1;grid-column:1}.burner-diagram{grid-template-columns:88px minmax(0,1fr);text-align:left;gap:2px 14px;justify-self:start}.burner-icon{width:88px;grid-row:1/span 3;grid-column:1}.burner-diagram>:not(svg){grid-column:2}}
.pump-controls{display:grid;grid-template-columns:repeat(3,minmax(0,1fr));gap:20px;margin-top:24px}.pump-controls>div{min-width:0}.pump-controls .relay-switch{width:100%;text-align:left;min-height:58px}.pump-controls .switch-track{flex-shrink:0}.pump-controls p{margin:10px 0 0;font-size:.9rem}.relay-switch:active{background:#c9e9d9}
#heating-detail:empty,#pump-progress:empty,#pump-message:empty{display:none}
.rule-table-hint{display:none}.automatic-rules{margin-top:20px}.automatic-rules summary{cursor:pointer;font-weight:600}.rule-table-wrap{overflow-x:auto;max-width:100%}.automatic-rules table{width:100%;min-width:560px;border-collapse:collapse;font-size:.9rem;line-height:1.5}.automatic-rules th,.automatic-rules td{padding:12px;text-align:left;vertical-align:top;border-bottom:1px solid #b9c8c9}.automatic-rules th:first-child{width:140px}
@media(max-width:760px){.pump-controls{grid-template-columns:1fr}.pump-controls .relay-switch{width:auto;min-width:240px}}
.relay-switch[data-state="off"]{background:#d8f2e2;color:#164d30;border-color:#72b58d}.relay-switch[data-state="off"] .switch-track{background:#17643c}
#manual-switch[data-state="off"],#manual-switch[data-state="off"]:hover,#solar-manual-switch[data-state="off"],#solar-manual-switch[data-state="off"]:hover{background:#e6eaee;color:#25313b;border-color:#aab5bd}#manual-switch[data-state="off"] .switch-track,#solar-manual-switch[data-state="off"] .switch-track{background:#60757b}
.relay-switch[data-state="on"]{background:#ffe1df;color:#7d1f22;border-color:#d88482}.relay-switch[data-state="on"] .switch-track{background:#a42b30}
#heating-switch[data-state="off"],#heating-switch[data-state="off"]:hover{background:#fff0b5;color:#5c4100;border-color:#c89c28}#heating-switch[data-state="off"] .switch-track{background:#805b00}.badge.summer-active{background:#fff0b5;color:#5c4100}
.storage-overview{position:relative}.storage-overview>.burner-diagram,.storage-overview>.pump-flow{position:relative;z-index:1}.storage-overview>.burner-pipes{position:absolute;inset:0;width:100%;height:100%;overflow:visible;pointer-events:none;z-index:0}.burner-pipes path{fill:none;stroke-width:2.5;stroke-linejoin:round}.burner-pipes .pipe-vl{stroke:#e76c58}.burner-pipes .pipe-rl{stroke:#4d9bd5}.burner-pipes text{font:600 10px system-ui,sans-serif;fill:var(--ink1,#192f35);paint-order:stroke;stroke:var(--bg1,#f3f5f4);stroke-width:3px;stroke-linejoin:round}
@media(max-width:700px){.storage-overview{grid-template-columns:minmax(0,1fr);gap:28px}.storage-overview .pump-flow{grid-row:2;grid-column:1}.burner-diagram{grid-row:1;grid-template-columns:88px minmax(0,1fr);text-align:left;gap:2px 14px;justify-self:start}.burner-icon{width:88px;grid-row:1/span 3;grid-column:1}.burner-diagram>:not(svg){grid-column:2}}
.message{min-height:1.6em;margin:12px 0 0}.rule{margin:16px 0 0;font-size:.9rem}footer{border-top:1px solid #b9c8c9;padding-top:18px;font-size:.9rem}
#wifi-form{grid-template-columns:repeat(2,minmax(0,1fr))}#wifi-form button{grid-column:1/-1;justify-self:start}#wifi-status,#wifi-address{overflow-wrap:anywhere}
#wifi-networks{display:block;width:100%;min-height:46px;margin:8px 0 16px;padding:10px;border:1px solid #8c9fa3;border-radius:6px;font:inherit;background:white;color:inherit}#wifi-scan-message{margin:10px 0;min-height:1.6em}#wifi-network-label[hidden]{display:none}
#controllers form{grid-template-columns:minmax(0,1fr) auto}#auto-form{grid-template-columns:minmax(0,var(--setting-stepper-width,1fr)) auto;max-width:640px}#auto-form .full{grid-column:1/-1}#auto-form select{display:block;width:100%;min-height:46px;margin-top:8px;padding:10px;border:1px solid #8c9fa3;border-radius:6px;font:inherit;background:white;color:inherit}#auto-form .check{display:flex;align-items:center;gap:10px;min-height:44px}#auto-form input[type=checkbox]{width:20px;min-height:20px;margin:0;accent-color:#17636b}#auto-form button{justify-self:start}#auto-form .stepper button{justify-self:stretch}
#sim-banner{padding:12px 16px;background:#e6eaee;color:#25313b;border:1px solid #c7cfd5;border-radius:6px}#sim-banner button{background:#f6f8fa;color:#25313b;border:1px solid #aab5bd}#sim-banner button:hover{background:#dce2e7}#sim-form{display:block}#sim-fields{display:grid;grid-template-columns:repeat(2,minmax(0,1fr));gap:16px}.sim-item{padding:0}.sim-check{display:flex;align-items:center;gap:8px;margin-top:10px;font-size:.9rem}.sim-check input{width:20px;min-height:20px;margin:0;accent-color:#17636b}#sim-form button{margin-top:20px}
@media(max-width:560px){#sim-fields{grid-template-columns:1fr}}
@media(max-width:560px){.rule-table-hint{display:block;font-size:.8rem}main{padding:26px 18px}form{grid-template-columns:1fr 1fr}#controllers form,#auto-form{grid-template-columns:1fr}button{grid-column:1/-1}.stepper button{grid-column:auto}.heading{align-items:flex-start}}
#pump-runtime-section{display:none}body.history-page #pump-runtime-section{display:block}
#auto-form{grid-template-columns:repeat(3,minmax(0,1fr));max-width:880px}#auto-form>button{grid-column:1/-1}#auto-form label{white-space:normal}@media(max-width:850px){#auto-form{grid-template-columns:repeat(2,minmax(0,1fr))}}@media(max-width:560px){#auto-form{grid-template-columns:1fr}}
.runtime-controls{display:flex;gap:12px;flex-wrap:wrap}.runtime-controls label{flex:1 1 180px;max-width:260px}.runtime-controls select{width:100%;min-height:46px;margin-top:8px;padding:8px;border:1px solid #8c9fa3;border-radius:6px;font:inherit;color:var(--ink1,#192f35);background:var(--bg2,#fff)}
.runtime-values{display:grid;grid-template-columns:repeat(3,minmax(0,1fr));gap:12px;margin:20px 0 0}.runtime-values div{padding:12px;border:1px solid var(--line,#b9c8c9);border-radius:6px}.runtime-values dt{font-size:.85rem;color:var(--ink2,#4e6268)}.runtime-values dd{margin:6px 0 0;font-size:1.25rem;font-weight:700;font-variant-numeric:tabular-nums}.runtime-note{margin-top:14px}
@media(max-width:600px){.runtime-controls label{max-width:none}.runtime-values{grid-template-columns:1fr}.runtime-values div{display:flex;align-items:center;justify-content:space-between;gap:12px}.runtime-values dd{margin:0}}
</style>
</head>
<body><main>
<script>document.body.classList.toggle('settings-page',['/einstellungen','/testwerte','/sicherheit'].includes(location.pathname));document.body.classList.toggle('history-page',location.pathname==='/verlauf');document.body.classList.toggle('info-page',location.pathname==='/info');document.body.classList.toggle('organigram-page',location.pathname==='/regelungsuebersicht');</script>
<nav class="page-nav" aria-label="Navigation">
  <div class="nav-sticky">
  <button id="nav-toggle" type="button" aria-controls="nav-links" aria-expanded="false" hidden><svg class="nav-icon" viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="1.8" stroke-linecap="round" aria-hidden="true" focusable="false"><path d="M4 6h16M4 12h16M4 18h16"/></svg>Menü</button>
  <div id="nav-links" class="nav-links">
  <a class="dashboard-link" href="/"><svg class="nav-icon" viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="1.8" stroke-linecap="round" stroke-linejoin="round" aria-hidden="true" focusable="false"><path d="m3 10 9-7 9 7v10a1 1 0 0 1-1 1H4a1 1 0 0 1-1-1z"/><path d="M9 21v-7h6v7"/></svg><span>Steuerung</span></a>
  <a class="history-link" href="/verlauf"><svg class="nav-icon" viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="1.8" stroke-linecap="round" stroke-linejoin="round" aria-hidden="true" focusable="false"><path d="M4 4v16h16"/><path d="M7 15l4-4 3 2 5-7"/></svg><span>Verlauf (Lokal)</span></a>
  <a id="history-server-link" href="#" target="_blank" rel="noopener noreferrer" hidden><svg class="nav-icon" viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="1.8" stroke-linecap="round" stroke-linejoin="round" aria-hidden="true" focusable="false"><rect x="3" y="4" width="18" height="6" rx="1"/><rect x="3" y="14" width="18" height="6" rx="1"/><path d="M7 7h.01M7 17h.01M11 7h7M11 17h7"/></svg><span>History-Server</span></a>
  <a class="organigram-link" href="/regelungsuebersicht"><svg class="nav-icon" viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="1.8" stroke-linecap="round" stroke-linejoin="round" aria-hidden="true" focusable="false"><rect x="8" y="2" width="8" height="5" rx="1"/><rect x="2" y="17" width="8" height="5" rx="1"/><rect x="14" y="17" width="8" height="5" rx="1"/><path d="M12 7v5M6 17v-5h12v5"/></svg><span>Regelungsübersicht</span></a>
  <a class="info-link" href="/info"><svg class="nav-icon" viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="1.8" stroke-linecap="round" stroke-linejoin="round" aria-hidden="true" focusable="false"><circle cx="12" cy="12" r="9"/><path d="M12 11v5M12 7.5h.01"/></svg><span>ESP-Info</span></a>
  <a href="/webserial" id="console-link" target="_blank" rel="noopener"><svg class="nav-icon" viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="1.8" stroke-linecap="round" stroke-linejoin="round" aria-hidden="true" focusable="false"><rect x="3" y="4" width="18" height="16" rx="2"/><path d="m7 9 3 3-3 3M13 15h4"/></svg><span>Live-Monitor öffnen</span></a>
  <div class="nav-divider" role="separator"></div>
  <a href="/?test=on" id="test-link" hidden><svg class="nav-icon" viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="1.8" stroke-linecap="round" stroke-linejoin="round" aria-hidden="true" focusable="false"><circle cx="12" cy="12" r="9"/><path d="m10 8 6 4-6 4z"/></svg><span>Testmodus starten</span></a>
  <a class="settings-link" href="/einstellungen"><svg class="nav-icon" viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="1.8" stroke-linecap="round" stroke-linejoin="round" aria-hidden="true" focusable="false"><path d="M12 8a4 4 0 1 0 0 8 4 4 0 0 0 0-8Z"/><path d="M19.4 13.5a7.7 7.7 0 0 0 0-3l1.5-1.2-1.5-2.6-1.8.7a8 8 0 0 0-2.6-1.5L14.7 4h-3l-.4 1.9a8 8 0 0 0-2.6 1.5l-1.8-.7-1.5 2.6 1.5 1.2a7.7 7.7 0 0 0 0 3l-1.5 1.2 1.5 2.6 1.8-.7a8 8 0 0 0 2.6 1.5l.4 1.9h3l.4-1.9a8 8 0 0 0 2.6-1.5l1.8.7 1.5-2.6-1.5-1.2Z" transform="translate(-.2 0)"/></svg><span>Einstellungen</span></a>
  <div id="settings-subnav" class="settings-subnav" role="group" aria-label="Einstellungsbereiche">
    <a href="#appearance-section">Darstellung</a>
    <a href="#security-section">Zugangspasswort</a>
    <a href="#history-server-section">History-Server</a>
    <a href="#hostname-section">Hostname</a>
    <a href="#wifi-section">WLAN einrichten</a>
    <a href="#remote-relay-settings">Relaiserweiterungen</a>
    <a href="#remote-relay-update-section">Relaiskarte aktualisieren</a>
    <a href="#update-section">Firmware aktualisieren</a>
    <a href="#simulation-settings">Testwerte</a>
  </div>
  </div>
  </div>
</nav>
<header class="dashboard-header"><div class="dashboard-title"><h1>Solarsystemsteuerung</h1><p id="firmware-version" class="muted">Version: …</p></div><div class="header-status"><p class="connection-status" role="status"><span id="connection-dot" class="connection-dot" aria-hidden="true"></span><span id="connection">Verbindung wird aufgebaut …</span></p><p id="device-clock" class="muted">ESP32-Zeit wird geladen …</p><p class="wifi-signal-status muted"><svg id="wifi-signal-bars" class="wifi-signal-bars" viewBox="0 0 25 18" role="img" aria-label="WLAN-Signalstärke nicht verfügbar"><rect x="0" y="15" width="3" height="3" rx=".5"/><rect x="5" y="12" width="3" height="6" rx=".5"/><rect x="10" y="9" width="3" height="9" rx=".5"/><rect x="15" y="6" width="3" height="12" rx=".5"/><rect x="20" y="3" width="3" height="15" rx=".5"/></svg><span id="wifi-signal">WLAN-Signal wird geladen …</span></p></div></header>
<div id="sim-banner" role="status" hidden><strong>Testmodus:</strong> Werte, Verlauf und Pumpe sind simuliert. Relais am Haupt-ESP bleiben aus; WLAN-Relais schalten tatsächlich. <button id="test-stop" type="button">Testmodus beenden</button></div>
<p id="test-message" class="message" role="status"></p>
<section aria-labelledby="heating-heading">
<div class="heading"><h2 id="heating-heading">Sommer / Winter · Ölbrenner</h2><span id="heating-badge" class="badge" role="status">Status wird geladen …</span></div>
<p>Sommer: Kombi oben, Öl gesperrt. Winter: Kombi Mitte, Öl freigegeben.</p>
<button id="heating-switch" class="relay-switch" type="button" role="switch" aria-checked="false" aria-describedby="heating-detail" disabled><span class="switch-track" aria-hidden="true"></span><span id="heating-switch-label">Winterbetrieb · Ölbrenner freigeben</span></button>
<p id="heating-detail" class="muted">Betriebsart wird geladen …</p>
<p class="muted">Öl freigegeben: Kombi → Puffer gesperrt, auch bei manueller Steuerung. Puffer → Kombi bleibt möglich.</p>
<p id="heating-message" class="message" role="status"></p>
</section>
<section aria-labelledby="pump-heading">
<div class="heading"><h2 id="pump-heading">Backloadpumpe</h2><span id="pump-badge" class="badge" role="status">Status wird geladen …</span></div>
<div class="storage-overview">
<svg class="burner-pipes" aria-hidden="true"><path class="pipe-vl"/><path class="pipe-rl"/><path class="solar-link"/><text class="pipe-vl-label">VL</text><text class="pipe-rl-label">RL</text></svg>
<div id="oil-burner-diagram" class="burner-diagram" role="img" aria-label="Ölbrennerfreigabe: Status unbekannt">
<svg class="burner-icon" viewBox="0 0 128 96" aria-hidden="true">
<g fill="none" stroke="currentColor" stroke-width="2" stroke-linejoin="round" stroke-linecap="round">
<rect class="burner-body" x="10" y="22" width="74" height="52" rx="8"/>
<circle cx="39" cy="48" r="18"/><circle cx="39" cy="48" r="4"/>
<path d="M39 30V40M57 48H47M39 66V56M21 48H31M84 36H109V60H84M109 42H123M109 54H123M24 74V82H38M66 74V82H78"/>
<circle class="burner-indicator" cx="71" cy="33" r="3"/>
</g></svg>
<strong>Ölbrenner</strong><output id="oil-burner-state">Status unbekannt</output>
<span id="oil-burner-detail">Keine aktuellen Daten</span>
</div>
<svg class="pump-flow stopped" viewBox="0 0 440 280" role="img" aria-labelledby="pump-flow-title" aria-describedby="pump-flow-desc">
<title id="pump-flow-title">Förderrichtung wird geladen</title>
<desc id="pump-flow-desc">Speichertemperaturen werden geladen.</desc>
<defs><marker id="flow-arrowhead" markerWidth="8" markerHeight="8" refX="7" refY="4" orient="auto"><path d="M0 0L8 4L0 8Z" fill="currentColor"/></marker></defs>
<g class="storage">
<text class="tank-name" x="96" y="24" text-anchor="middle">Kombispeicher</text>
<text class="tank-name" x="344" y="24" text-anchor="middle">Pufferspeicher</text>
<rect class="tank" x="14" y="42" width="164" height="218" rx="42"/>
<rect class="tank" x="262" y="42" width="164" height="218" rx="42"/>
<path class="tank-rim" d="M16 76 Q96 96 176 76 M264 76 Q344 96 424 76 M16 226 Q96 246 176 226 M264 226 Q344 246 424 226"/>
<text class="sensor-label" x="28" y="98">Oben</text><text class="sensor-value" id="diagram-temp-0" x="163" y="118" text-anchor="end">—</text>
<text class="sensor-label" x="28" y="148">Mitte</text><text class="sensor-value" id="diagram-temp-1" x="163" y="168" text-anchor="end">—</text>
<text class="sensor-label" x="28" y="198">Unten</text><text class="sensor-value" id="diagram-temp-3" x="163" y="218" text-anchor="end">—</text>
<text class="sensor-label" x="276" y="98">Oben</text><text class="sensor-value" id="diagram-temp-2" x="411" y="118" text-anchor="end">—</text>
</g>
<g class="transfer-pump" transform="translate(220 111)" aria-hidden="true">
<circle class="transfer-pump-body" r="19"/>
<g class="transfer-pump-rotor"><circle r="13" fill="none" stroke="none"/><path d="M0 0 C-3 -5 -3 -11 2 -13 C5 -9 4 -3 0 0Z"/><path d="M0 0 C-3 -5 -3 -11 2 -13 C5 -9 4 -3 0 0Z" transform="rotate(120)"/><path d="M0 0 C-3 -5 -3 -11 2 -13 C5 -9 4 -3 0 0Z" transform="rotate(240)"/><circle r="2.5"/></g>
</g>
<path id="pump-flow-arrow" d="M187 154H253" marker-end="url(#flow-arrowhead)"/>
</svg>
<div id="solar-pump-diagram" class="solar-diagram" role="img" aria-label="Solarpumpe: Status unbekannt">
<svg class="solar-icon" viewBox="0 0 128 110" aria-hidden="true">
<g fill="none" stroke="currentColor" stroke-width="2" stroke-linecap="round" stroke-linejoin="round">
<circle cx="96" cy="15" r="6"/><path d="M96 3v3m0 18v3M84 15h3m18 0h3M87.5 6.5l2.2 2.2m12.6 12.6 2.2 2.2m0-17-2.2 2.2"/>
<path class="solar-panel-body" d="M19 14h55l8 30H11z"/><path d="M15 29h63M35 14l-3 30m23-30 3 30M46 44v9h18v14"/>
<circle class="solar-pump-body" cx="64" cy="81" r="14"/><g class="solar-pump-rotor"><circle cx="64" cy="81" r="3"/><path d="M64 70v8m11 3h-8m-3 11v-8m-11-3h8"/></g><path d="M50 81H36"/>
</g></svg>
<strong>Solarpumpe</strong><output id="solar-diagram-state">Status unbekannt</output>
<span id="solar-diagram-panel">Solarpanel: keine aktuellen Daten</span><span id="solar-diagram-detail">Keine aktuellen Daten</span>
</div>
</div>
<p id="pump-direction-state" class="muted">Förderrichtung wird geladen …</p>
<div class="pump-controls">
<div><button id="manual-switch" class="relay-switch" type="button" role="switch" aria-checked="false" aria-describedby="manual-help" disabled><span class="switch-track" aria-hidden="true"></span><span>Manuelle Steuerung</span></button><p id="manual-help" class="muted">Die manuelle Steuerung gilt nur für die Backloadpumpe und übergeht dort die Temperaturregelung und Sensorfehler. Nach einem Neustart ist sie ausgeschaltet.</p></div>
<div><button id="pump-switch" class="relay-switch" type="button" role="switch" aria-checked="false" aria-describedby="pump-help" disabled><span class="switch-track" aria-hidden="true"></span><span>Backloadpumpe ein / aus</span></button><p id="pump-help" class="muted">Zuerst die manuelle Steuerung einschalten.</p></div>
<div><button id="direction-switch" class="relay-switch" type="button" role="switch" aria-checked="false" aria-describedby="direction-help" disabled><span class="switch-track" aria-hidden="true"></span><span>Pumprichtung · Relais 2</span></button><p id="direction-help" class="muted">Aus: Kombi → Puffer. Ein: Puffer → Kombi.</p></div>
</div>
<p id="pump-mode-detail" class="muted">Steuerungsmodus wird geladen …</p>
<p id="pump-progress" class="message" role="status"></p>
<p id="pump-message" class="message" role="status"></p>
<p class="muted">Richtungswechsel: Die Backloadpumpe pausiert 2 Sekunden, auch bei manueller Steuerung.</p>
</section>
<section aria-labelledby="auto-heading">
<h2 id="auto-heading">Automatische Regelung</h2>
<p class="muted">Kombi oben im Sommer, Kombi Mitte im Winter; verglichen wird jeweils mit Puffer oben.</p>
<form id="auto-form">
<div><label for="auto-start">Startdifferenz zwischen den Speichern (5–10 K)</label><div class="stepper"><button type="button" data-field="start" data-direction="-1" aria-label="Startdifferenz senken">−</button><input id="auto-start" type="number" name="start" min="5" max="10" step="1" inputmode="numeric" required><button type="button" data-field="start" data-direction="1" aria-label="Startdifferenz erhöhen">+</button></div></div>
<div><label for="auto-min-combi">Mindesttemperatur Kombispeicher (0–80 °C)</label><div class="stepper"><button type="button" data-field="minimumCombi" data-direction="-1" aria-label="Mindesttemperatur Kombispeicher senken">−</button><input id="auto-min-combi" type="number" name="minimumCombi" min="0" max="80" step="1" inputmode="numeric" required><button type="button" data-field="minimumCombi" data-direction="1" aria-label="Mindesttemperatur Kombispeicher erhöhen">+</button></div></div>
<div><label for="auto-min-buffer">Mindesttemperatur Pufferspeicher (0–80 °C)</label><div class="stepper"><button type="button" data-field="minimumBuffer" data-direction="-1" aria-label="Mindesttemperatur Pufferspeicher senken">−</button><input id="auto-min-buffer" type="number" name="minimumBuffer" min="0" max="80" step="1" inputmode="numeric" required><button type="button" data-field="minimumBuffer" data-direction="1" aria-label="Mindesttemperatur Pufferspeicher erhöhen">+</button></div></div>
<button type="submit">Regelung speichern</button>
</form>
<p class="muted">Automatik: Beide Speicher müssen ihre Mindesttemperatur erreichen. Im Handbetrieb gelten diese Grenzen nicht. Die Einlagerungs-Starttemperatur gilt nur für Kombi → Puffer; Stopp bei 3 K Differenz.</p>
<p id="auto-min-status" class="muted" role="status">Mindesttemperaturen werden geprüft …</p>
<p id="auto-error" class="error" role="alert" hidden></p>
<p id="auto-message" class="message" role="status"></p>
<details class="automatic-rules"><summary>Gespeicherte Schaltbedingungen</summary>
<p id="auto-rules-sensor" class="muted">Schaltbedingungen werden geladen …</p>
<p id="auto-rules-state" role="status"></p>
<div id="auto-rules-table" class="rule-table-wrap" role="region" aria-label="Schaltbedingungen der Backload-Regelung" tabindex="0" hidden>
<p class="muted rule-table-hint">Tabelle seitlich verschieben, um Start und Stopp zu sehen.</p>
<table><thead><tr><th scope="col">Förderrichtung</th><th scope="col">Start: alle Bedingungen</th><th scope="col">Stopp: eine Bedingung genügt</th></tr></thead>
<tbody><tr><th scope="row">Kombi → Puffer</th><td id="auto-charge-start"></td><td id="auto-charge-stop"></td></tr>
<tr><th scope="row">Puffer → Kombi</th><td id="auto-return-start"></td><td id="auto-return-stop"></td></tr></tbody></table></div>
</details>
</section>
<section id="solar-section" aria-labelledby="solar-heading">
<div class="heading"><h2 id="solar-heading">Solarpumpe</h2><span id="solar-badge" class="badge" role="status">Status wird geladen …</span></div>
<p class="muted">Relais 3 regelt unabhängig von der Backloadpumpe anhand von Solarpanel und Kombispeicher unten.</p>
<p id="solar-detail" class="muted">Werte werden geladen …</p>
<div class="solar-manual-controls">
<button id="solar-manual-switch" class="relay-switch" type="button" role="switch" aria-checked="false" aria-describedby="solar-manual-help" disabled><span class="switch-track" aria-hidden="true"></span><span>Manuelle Steuerung</span></button>
<button id="solar-power-switch" class="relay-switch" type="button" role="switch" aria-checked="false" aria-describedby="solar-manual-help" disabled><span class="switch-track" aria-hidden="true"></span><span>Solarpumpe ein / aus</span></button>
</div>
<p id="solar-manual-help" class="muted">Im Handbetrieb gelten weder Temperaturregelung noch Sensorfehler. Beim Einschalten des Handbetriebs ist die Pumpe zunächst aus; nach Neustart gilt wieder Automatik.</p>
<p id="solar-control-message" class="message" role="status"></p>
<form id="solar-form">
<div><label for="solar-start">Einschaltdifferenz (3–10 K)</label><div class="stepper"><button type="button" data-direction="-1" aria-label="Solar-Einschaltdifferenz senken">−</button><input id="solar-start" type="number" name="start" min="3" max="10" step="0.1" inputmode="decimal" required><button type="button" data-direction="1" aria-label="Solar-Einschaltdifferenz erhöhen">+</button></div></div>
<button type="submit">Solarregelung speichern</button>
</form>
<p class="muted">Automatik: Einschalten ab eingestellter Differenz; Ausschalten bei höchstens 1 K oder Sensorfehler.</p>
<p id="solar-message" class="message" role="status"></p>
</section>



<section id="organigram-section" aria-labelledby="organigram-heading">
<header class="org-header"><div><h1 id="organigram-heading">Regelungsübersicht</h1><p class="muted">So entscheidet die Steuerung über Betriebsart und Förderrichtung.</p></div><span id="org-status" class="badge" role="status">Status wird geladen …</span></header>
<div class="org-flow">
  <div class="org-mode">
    <div class="org-section-title"><svg class="org-icon" viewBox="0 0 24 24" aria-hidden="true"><circle cx="12" cy="12" r="3.5"/><path d="M12 2v2m0 16v2M2 12h2m16 0h2M5 5l1.5 1.5m11 11L19 19M19 5l-1.5 1.5m-11 11L5 19"/></svg><h2>Betriebsart</h2></div>
    <div class="org-modes">
      <div id="org-mode-summer" class="org-season"><div class="org-season-title"><strong>Sommer</strong><span id="org-mode-summer-state" class="org-mode-state"></span></div><span>Kombispeicher oben · Öl gesperrt</span></div>
      <div id="org-mode-winter" class="org-season"><div class="org-season-title"><strong>Winter</strong><span id="org-mode-winter-state" class="org-mode-state"></span></div><span>Kombispeicher Mitte · Öl freigegeben</span></div>
    </div>
    <p class="org-mode-note">Puffer oben ist in beiden Betriebsarten der Vergleichsfühler.</p>
  </div>
  <div class="org-branches">
    <article class="org-branch org-charge">
      <div class="org-branch-head"><svg class="org-icon" viewBox="0 0 24 24" aria-hidden="true"><path d="M3 7h16m-4-4 4 4-4 4M21 17H5"/></svg><div><h2>Kombi → Puffer</h2><span>Einlagern</span></div></div>
      <h3>Start · alle Bedingungen</h3><ul><li>Kombi ≥ <strong id="org-target">— °C</strong></li><li>Kombi − Puffer ≥ <strong class="org-start">— K</strong></li><li>Ölfreigabe aus</li></ul>
      <p class="org-stop"><strong>Stopp:</strong> Differenz ≤ <strong class="org-stop-value">— K</strong> oder Ölfreigabe an.</p>
    </article>
    <article class="org-branch org-return">
      <div class="org-branch-head"><svg class="org-icon" viewBox="0 0 24 24" aria-hidden="true"><path d="M21 7H5m4-4L5 7l4 4M3 17h16"/></svg><div><h2>Puffer → Kombi</h2><span>Rückladen</span></div></div>
      <h3>Start</h3><ul><li>Puffer − Kombi ≥ <strong class="org-start">— K</strong></li><li>In Sommer und Winter möglich</li></ul>
      <p class="org-stop"><strong>Stopp:</strong> Differenz ≤ <strong class="org-stop-value">— K</strong>.</p>
    </article>
  </div>
  <div class="org-interlock"><svg class="org-icon" viewBox="0 0 24 24" aria-hidden="true"><path d="M12 3 4 6v5c0 5 3 8 8 10 5-2 8-5 8-10V6z"/><path d="M12 8v4m0 4h.01"/></svg><div><strong>Backloadpumpe &amp; Richtungswechsel</strong><p>Automatik nur ab Mindesttemperatur: Kombi <strong id="org-min-combi">— °C</strong> und Puffer <strong id="org-min-buffer">— °C</strong>. Im Handbetrieb gelten diese Grenzen nicht.</p><p>Bei Fehler eines Regelungsfühlers stoppt die Automatik. Vor einem Richtungswechsel pausiert die Backloadpumpe 2 Sekunden.</p></div></div>
</div>
<div class="org-solar" aria-labelledby="org-solar-heading">
  <div class="heading"><div class="org-section-title"><svg class="org-icon" viewBox="0 0 24 24" aria-hidden="true"><circle cx="12" cy="7" r="3"/><path d="M12 1v2M4 7h3m10 0h3M6 2l1.5 1.5M18 2l-1.5 1.5M4 13h16l2 8H2zM5 17h14m-7-4v8"/></svg><h2 id="org-solar-heading">Solarregelung</h2></div><span id="org-solar-state" class="badge">Status wird geladen …</span></div>
  <p class="muted">Die Solarpumpe an Relais 3 arbeitet unabhängig von der Backloadpumpe.</p>
  <dl class="org-solar-values"><div><dt>Solarpanel</dt><dd id="org-solar-panel">—</dd></div><div><dt>Kombispeicher unten</dt><dd id="org-solar-tank">—</dd></div><div><dt>Aktuelle Differenz</dt><dd id="org-solar-difference">—</dd></div></dl>
  <p><strong>Start:</strong> Solarpanel − Kombi unten ≥ <strong id="org-solar-start">— K</strong> <span class="muted">(einstellbar: 3–10 K)</span>.</p>
  <p><strong>Stopp:</strong> Differenz ≤ <strong id="org-solar-stop">— K</strong> oder Fehler eines der beiden Sensoren.</p>
  <p class="org-solar-note muted">Im Handbetrieb werden Temperaturregelung und Sensorfehler übergangen. Bei Update oder Neustart ist die Pumpe ausgeschaltet; im Testmodus wird nur der Zustand simuliert.</p>
</div>
<div class="org-led-guide" id="org-led-guide" aria-labelledby="org-led-heading">
  <div class="org-section-title"><svg class="org-icon" viewBox="0 0 24 24" aria-hidden="true"><circle cx="12" cy="12" r="4"/><path d="M12 2v3m0 14v3M2 12h3m14 0h3M4.9 4.9 7 7m10 10 2.1 2.1m0-14.2L17 7M7 17l-2.1 2.1"/></svg><h2 id="org-led-heading">RGB-Statusanzeige</h2></div>
  <p class="org-led-intro muted">Beispiele für die Signale der LED am ESP32.</p>
  <ul class="org-led-examples">
    <li><span class="org-led-dot org-led-normal" aria-hidden="true"></span><span><strong>Automatik</strong><small>Grün · kurzer Impuls alle 2 Sekunden</small></span></li>
    <li><span class="org-led-dot org-led-manual" aria-hidden="true"></span><span><strong>Manuelle Steuerung</strong><small>Blau · langsames Blinken</small></span></li>
    <li><span class="org-led-dot org-led-fault" aria-hidden="true"></span><span><strong>Störung</strong><small>Gelb · kurzer Blitz</small></span></li>
    <li><span class="org-led-dot org-led-test" aria-hidden="true"></span><span><strong>Testmodus</strong><small>Violett · kurzes Blinken</small></span></li>
    <li><span class="org-led-dot org-led-update" aria-hidden="true"></span><span><strong>Firmware-Update</strong><small>Weiß · pulsierend</small></span></li>
  </ul>
  <p class="org-led-priority muted">Bei mehreren Zuständen gilt: Störung vor Update, manueller Steuerung, Testmodus und Automatik.</p>
</div>
<div class="org-hardware">
  <div class="org-hardware-head"><div class="org-section-title"><svg class="org-icon" viewBox="0 0 24 24" aria-hidden="true"><rect x="5" y="5" width="14" height="14" rx="2"/><rect x="9" y="9" width="6" height="6"/><path d="M9 2v3m6-3v3M9 19v3m6-3v3M2 9h3m-3 6h3m14-6h3m-3 6h3"/></svg><h2>ESP32-S3 &amp; Anschlüsse</h2></div><p class="muted">Die verwendeten GPIOs und ihre Funktionen. Die Nummern stammen aus der laufenden Firmware.</p></div>
  <div class="org-pin-layout">
    <div class="org-pin-group org-sensors"><h3>Temperaturfühler</h3><p class="org-pin-intro">MAX31865 · Chip-Select</p><dl class="org-pin-list">
      <div><dt>Kombispeicher oben <small>Sommerfühler</small></dt><dd>GPIO <span id="org-cs-0">—</span></dd></div>
      <div><dt>Kombispeicher Mitte <small>Winterfühler</small></dt><dd>GPIO <span id="org-cs-1">—</span></dd></div>
      <div><dt>Pufferspeicher oben <small>Vergleichsfühler</small></dt><dd>GPIO <span id="org-cs-2">—</span></dd></div>
      <div><dt>Kombispeicher unten <small>Solarregelung</small></dt><dd>GPIO <span id="org-cs-3">—</span></dd></div>
      <div><dt>Solarpanel <small>Solarregelung</small></dt><dd>GPIO <span id="org-cs-4">—</span></dd></div>
    </dl><h3>Gemeinsamer SPI-Bus</h3><dl class="org-pin-list">
      <div><dt>CLK <small>Taktsignal</small></dt><dd>GPIO <span id="org-spi-clk">—</span></dd></div>
      <div><dt>MOSI <small>Daten zu den Fühlern</small></dt><dd>GPIO <span id="org-spi-mosi">—</span></dd></div>
      <div><dt>MISO <small>Daten von den Fühlern</small></dt><dd>GPIO <span id="org-spi-miso">—</span></dd></div>
    </dl></div>
    <figure class="org-board"><div class="org-board-photo"><img src="/assets/esp32-board.jpg" alt="ESP32-S3 N16R8 Entwicklungsboard" width="1376" height="1500" loading="lazy"></div><figcaption><strong>ESP32-S3</strong><span>N16R8 · 16 MB Flash · 8 MB PSRAM</span></figcaption></figure>
    <div class="org-pin-group org-outputs"><h3>Relaisausgänge</h3><p class="org-pin-intro">Active Low · LOW = AN</p><dl class="org-pin-list">
      <div><dt>Relais 1 <small>Backloadpumpe ein / aus</small></dt><dd>GPIO <span id="org-relay-pump">—</span></dd></div>
      <div><dt>Relais 2 <small>Pumprichtung</small></dt><dd>GPIO <span id="org-relay-direction">—</span></dd></div>
      <div><dt>Relais 3 <small>Solarpumpe</small></dt><dd>GPIO <span id="org-relay-solar">—</span></dd></div>
      <div><dt>Relais 4 <small>Ölbrennerfreigabe</small></dt><dd>GPIO <span id="org-relay-oil">—</span></dd></div>
    </dl><p class="org-direction muted">Richtungsrelais AUS: <span id="org-direction-off">—</span>.</p><h3>Weitere Anschlüsse</h3><dl class="org-pin-list">
      <div><dt>LED <small>Lebenszeichen</small></dt><dd>GPIO <span id="org-led">—</span></dd></div>
      <div><dt>RGB-LED <small>Betriebsstatus</small></dt><dd>GPIO <span id="org-rgb-led">—</span></dd></div>
      <div><dt>USB-UART <small>Serielle Ausgabe · TX / RX</small></dt><dd>GPIO 43 / 44</dd></div>
    </dl></div>
  </div>
  <p class="org-layout-note muted">Die Einträge sind nach Funktion gruppiert; ihre Position neben dem Bild entspricht nicht der physischen Pin-Reihenfolge.</p>
  <p class="org-note muted">Die Ölfreigabe zeigt den Schaltbefehl; eine Rückmeldung über den tatsächlichen Brennerlauf gibt es nicht.</p>
</div>
<div class="org-hardware relay-guide" aria-labelledby="relay-guide-heading">
  <div class="org-section-title"><svg class="org-icon" viewBox="0 0 24 24" aria-hidden="true"><rect x="3" y="5" width="18" height="14" rx="2"/><path d="M7 9h3v3H7zM14 9h3v3h-3zM7 16h10"/></svg><h2 id="relay-guide-heading">Relaiserweiterung einrichten</h2></div>
  <p class="muted">Für die vierfache WLAN-Relaiskarte RBS16467. Bis zu vier Karten können verbunden werden.</p>
  <div class="relay-guide-layout"><div>
    <ol>
      <li><strong>Vorbereiten:</strong> Karte spannungsfrei verkabeln; Netzspannung und Verbraucher getrennt lassen. Stromversorgung und Polung anhand des Datenblatts prüfen.</li>
      <li><strong>Programmierleiste verbinden:</strong> Die einreihige Stiftleiste neben dem ESP32 ist rückseitig mit <code>5V · TX · RX · GND · GND · IO0</code> beschriftet. Bei passender Versorgung Adapter-5V an Karten-5V, GND an GND, Adapter-TX an Karten-RX und Adapter-RX an Karten-TX anschließen. Die Datenleitungen benötigen 3,3-V-Logik; einen ausreichend starken 5-V-Ausgang verwenden. Keine weitere Stromversorgung gleichzeitig anschließen.</li>
      <li><strong>Flashmodus:</strong> Adapter per USB anschließen, auf der Karte IO0 gedrückt halten, EN kurz drücken und danach IO0 loslassen.</li>
      <li><strong>Firmware laden:</strong> Das <a href="https://github.com/NemoN/Solarsystemsteuerung_ESP32/releases/latest/download/Relaiskarte.factory.bin"><strong>Komplettabbild</strong></a> herunterladen und mit dem <a href="https://espressif.github.io/esptool-js/">ESP Tool</a> in Chrome oder Edge auf Adresse <code>0x0</code> übertragen. Es löscht gespeicherte WLAN-Daten, Schlüssel und Relaiszustände. Anschließend EN kurz drücken.</li>
      <li><strong>Verbinden:</strong> Unter Einstellungen → Relaiserweiterungen einen freien Platz wählen, die Karte suchen und die Einrichtung starten. Anschließend IO0 drei Sekunden gedrückt halten.</li>
      <li><strong>Verwenden:</strong> Relais in den Einstellungen beschriften und auf der Startseite schalten. Auch im Testmodus schalten die WLAN-Relais tatsächlich.</li>
    </ol>
    <p class="muted">Relais 1–4: GPIO 32, 33, 25 und 26; HIGH schaltet ein. Für spätere Updates über die Einstellungen die <a href="https://github.com/NemoN/Solarsystemsteuerung_ESP32/releases/latest/download/Relaiskarte.ino.bin">OTA-Firmware</a> verwenden.</p>
  </div><figure><img src="/assets/relaiserweiterung.svg" width="1715" height="1772" loading="lazy" alt="Vierkanal-Relaiserweiterung mit ESP32, Programmierleiste sowie Tasten EN und IO0"><figcaption>Relaiserweiterung RBS16467 · Vorderseite</figcaption></figure></div>
</div>
</section>
<section id="info-section" aria-labelledby="info-heading">
<h2 id="info-heading">ESP-Informationen</h2><p class="muted">Speicherwerte: Aktualisierung alle 5 Sekunden.</p>
<p id="info-status" class="muted" role="status">Gerätedaten werden geladen …</p>
<div class="info-grid">
<div><h3>Chip</h3><dl><div><dt>Modell</dt><dd id="info-chip">—</dd></div><div><dt>Revision</dt><dd id="info-revision">—</dd></div><div><dt>CPU-Kerne</dt><dd id="info-cores">—</dd></div><div><dt>CPU-Takt</dt><dd id="info-cpu">—</dd></div><div><dt>Laufzeit</dt><dd id="info-uptime">—</dd></div></dl></div>
<div><h3>Firmware</h3><dl><div><dt>Version</dt><dd id="info-version">—</dd></div><div><dt>ESP32-Core</dt><dd id="info-core">—</dd></div><div><dt>ESP-IDF</dt><dd id="info-idf">—</dd></div><div><dt>Aktiver Firmware-Slot</dt><dd id="info-slot">—</dd></div><div><dt>Update-Slot</dt><dd id="info-update-slot">—</dd></div></dl></div>
<div><h3>Speicher</h3><dl><div><dt>Flash</dt><dd id="info-flash">—</dd></div><div><dt>Interner RAM nutzbar</dt><dd id="info-heap">—</dd></div><div><dt>Interner RAM frei</dt><dd id="info-heap-free">—</dd></div><div><dt>PSRAM nutzbar</dt><dd id="info-psram">—</dd></div><div><dt>PSRAM frei</dt><dd id="info-psram-free">—</dd></div></dl><p class="muted">Der Temperaturverlauf reserviert PSRAM; die nutzbare Größe kann unter der verbauten Größe liegen.</p></div>
<div class="info-wifi"><h3>WLAN</h3><dl><div><dt>Status</dt><dd id="info-wifi-state">—</dd></div><div><dt>Heimnetz (SSID)</dt><dd id="info-wifi-ssid">—</dd></div><div><dt>Hostname / mDNS</dt><dd id="info-wifi-host">—</dd></div><div><dt>Signalstärke</dt><dd id="info-wifi-rssi">—</dd></div><div><dt>Kanal</dt><dd id="info-wifi-channel">—</dd></div><div><dt>WLAN-Sicherheit</dt><dd id="info-wifi-security">—</dd></div><div><dt>WLAN-Standards des Zugangspunkts</dt><dd id="info-wifi-standards">—</dd></div><div><dt>IP-Adresse</dt><dd id="info-wifi-ip">—</dd></div><div><dt>Subnetzmaske</dt><dd id="info-wifi-mask">—</dd></div><div><dt>Gateway</dt><dd id="info-wifi-gateway">—</dd></div><div><dt>DNS-Server</dt><dd id="info-wifi-dns">—</dd></div><div><dt>MAC-Adresse</dt><dd id="info-wifi-mac">—</dd></div><div><dt>Access-Point (BSSID)</dt><dd id="info-wifi-bssid">—</dd></div><div><dt>ESP-Zugang</dt><dd id="info-wifi-ap">—</dd></div><div><dt>ESP-Zugang IP / MAC</dt><dd id="info-wifi-ap-network">—</dd></div><div><dt>Verbundene Geräte</dt><dd id="info-wifi-clients">—</dd></div></dl></div>
</div></section>
<section id="history-section" aria-labelledby="history-heading">
<h2 id="history-heading">Temperaturverlauf</h2>
<p class="muted">Eine Messung pro Minute, bis zu einem Jahr. Beim Neustart wird der Verlauf gelöscht. Lange Zeiträume werden für die Anzeige zusammengefasst.</p>
<label class="history-range-label" for="history-range">Zeitraum<select id="history-range"><option value="24h">24 Stunden</option><option value="7d">7 Tage</option><option value="30d">30 Tage</option><option value="365d">365 Tage</option></select></label>
<div id="history-legend" class="history-legend" aria-label="Temperaturkurven"></div>
<p id="history-status" class="muted" role="status">Verlauf wird geladen …</p>
<div class="history-chart"><canvas id="history-chart" role="img" aria-label="Temperaturverlauf der ausgewählten Sensoren"></canvas></div>
<details><summary>Letzte dargestellte Werte</summary><p id="history-latest" class="muted">Noch keine Messwerte.</p></details>
</section>
<section id="pump-runtime-section" aria-labelledby="runtime-heading">
<h2 id="runtime-heading">Pumpenlaufzeiten</h2>
<p class="muted">Gezählt wird nur, wenn das jeweilige Relais eingeschaltet ist.</p>
<div class="runtime-controls">
<label>Zeitraum<select id="runtime-kind"><option value="days">Tag</option><option value="months">Monat</option><option value="years">Jahr</option></select></label>
<label>Auswahl<select id="runtime-period" disabled><option>Wird geladen …</option></select></label>
</div>
<dl class="runtime-values"><div><dt>Solarpumpe</dt><dd id="runtime-solar">—</dd></div><div><dt>Kombi → Puffer</dt><dd id="runtime-charge">—</dd></div><div><dt>Puffer → Kombi</dt><dd id="runtime-return">—</dd></div></dl>
<p id="runtime-note" class="muted runtime-note" role="status">Laufzeiten werden geladen …</p>
<p class="muted">Auf dem ESP32: letzte 32 Tage, 24 Monate und 12 Jahre mit Daten. Der History-Server archiviert die abgeholten Werte dauerhaft.</p>
</section>
<header id="settings-intro"><h1>Einstellungen</h1><p class="muted">Darstellung, Zugang, WLAN, Firmware und Testwerte dieser Steuerung.</p></header>
<section id="appearance-section" aria-labelledby="appearance-heading">
<h2 id="appearance-heading">Darstellung</h2>
<p class="muted">Das Design wird in diesem Browser gespeichert.</p>
<div class="theme-options" role="group" aria-label="Darstellung wählen"><button id="theme-light" type="button" aria-pressed="false">Hell</button><button id="theme-dark" type="button" aria-pressed="false">Dunkel</button></div>
</section>
<section id="security-section" aria-labelledby="security-heading">
<h2 id="security-heading">Zugangspasswort</h2>
<p class="muted">Das Passwort schützt die Steuerung und den Live-Monitor. Die Statusabfrage für das Temperaturarchiv bleibt lesbar.</p>
<form id="password-form">
<div class="password-field"><label for="access-password">Neues Passwort</label><div class="password-control"><input id="access-password" type="password" minlength="8" maxlength="128" autocomplete="new-password" required><button class="password-visibility" type="button" aria-label="Neues Passwort anzeigen" aria-pressed="false" aria-controls="access-password" title="Passwort anzeigen"><svg viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="1.8" stroke-linecap="round" stroke-linejoin="round" aria-hidden="true" focusable="false"><path d="M2 12s3.6-6 10-6 10 6 10 6-3.6 6-10 6S2 12 2 12Z"/><circle cx="12" cy="12" r="3"/><path class="password-eye-slash" d="M3 3l18 18"/></svg></button></div></div>
<div class="password-field"><label for="access-confirm">Passwort wiederholen</label><div class="password-control"><input id="access-confirm" type="password" minlength="8" maxlength="128" autocomplete="new-password" required><button class="password-visibility" type="button" aria-label="Passwortwiederholung anzeigen" aria-pressed="false" aria-controls="access-confirm" title="Passwort anzeigen"><svg viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="1.8" stroke-linecap="round" stroke-linejoin="round" aria-hidden="true" focusable="false"><path d="M2 12s3.6-6 10-6 10 6 10 6-3.6 6-10 6S2 12 2 12Z"/><circle cx="12" cy="12" r="3"/><path class="password-eye-slash" d="M3 3l18 18"/></svg></button></div></div>
<button type="submit">Passwort speichern</button></form>
<button id="password-remove" class="secondary-button" type="button">Passwortschutz entfernen</button><p id="password-message" class="message" role="status"></p>
</section>
<section id="history-server-section" aria-labelledby="history-server-heading">
<h2 id="history-server-heading">History-Server</h2>
<p class="muted">Die Adresse ergänzt einen Link zum Temperaturarchiv im Menü. Leer lassen, um den Link auszublenden.</p>
<form id="history-server-form"><label for="history-server-url">Adresse<input id="history-server-url" name="url" type="url" maxlength="255" placeholder="http://192.168.1.10:8088" autocomplete="url" autocapitalize="none" spellcheck="false" disabled></label><button type="submit" disabled>Adresse speichern</button></form>
<p id="history-server-message" class="message" role="status">Adresse wird geladen …</p>
</section>
<section id="hostname-section" aria-labelledby="hostname-heading">
<h2 id="hostname-heading">Hostname</h2>
<form id="hostname-form"><label for="hostname-input">Hostname<input id="hostname-input" name="hostname" minlength="1" maxlength="31" pattern="[A-Za-z0-9](?:[A-Za-z0-9\-]{0,29}[A-Za-z0-9])?" autocapitalize="none" spellcheck="false" required></label><button type="submit">Hostname speichern</button></form>
<p class="muted">1–31 Zeichen: Buchstaben, Zahlen und Bindestriche; kein Bindestrich am Anfang oder Ende. Änderungen gelten nach dem nächsten Neustart.</p>
<button id="hostname-reset" class="secondary-button" type="button">Standardnamen verwenden</button><p id="hostname-active" class="muted" role="status">Hostname wird geladen …</p><p id="hostname-message" class="message" role="status"></p>
</section>
<section id="wifi-section" aria-labelledby="wifi-heading">
<h2 id="wifi-heading">WLAN einrichten</h2>
<p id="wifi-ap-description" class="muted">Mit einem 2,4-GHz-WLAN verbinden. Der eigene ESP32-WLAN-Zugang bleibt erreichbar.</p>
<p id="wifi-status" role="status">WLAN-Status wird geladen …</p>
<p id="wifi-address" class="muted"></p>
<button id="wifi-scan" type="button">WLANs suchen</button>
<p id="wifi-scan-message" class="muted" role="status"></p>
<label id="wifi-network-label" for="wifi-networks" hidden>Verfügbare WLANs
<select id="wifi-networks"><option value="">WLAN auswählen …</option></select></label>
<form id="wifi-form">
<label for="wifi-ssid">WLAN-Name (SSID)<input id="wifi-ssid" name="ssid" maxlength="32" autocomplete="off" required></label>
<label for="wifi-password">WLAN-Passwort<input id="wifi-password" name="password" type="password" maxlength="63" autocomplete="new-password" aria-describedby="wifi-help"></label>
<button type="submit">Speichern &amp; verbinden</button>
</form>
<p id="wifi-help" class="muted">Passwort bei jeder Änderung eingeben; nur bei offenem WLAN leer lassen.</p>
<p id="wifi-message" class="message" role="status"></p>
<button id="wifi-reset" type="button">Gespeichertes WLAN vergessen</button>
</section>
<section id="update-section" aria-labelledby="update-heading">
<h2 id="update-heading">Firmware aktualisieren</h2>
<p class="muted">Beim Update bleibt die Pumpe aus. Der ESP startet neu und der Temperaturverlauf wird gelöscht.</p>
<p id="main-installed-version" class="muted">Installierte Firmware wird ermittelt …</p>
<details class="release-notes"><summary>Änderungen der installierten Version</summary><p id="main-installed-notes" class="release-notes-text" role="status">Release Notes werden geladen …</p></details>
<button id="release-check" type="button">GitHub-Releases suchen</button><p id="release-message" class="message" role="status">Noch nicht nach Updates gesucht.</p>
<form id="github-update-form"><label>Firmware-Version<select name="release" required disabled><option value="">Bitte zuerst Releases suchen</option></select></label><button type="submit" disabled>Ausgewählte Version installieren</button><p id="release-download" hidden><a id="release-download-link" target="_blank" rel="noopener noreferrer">Ausgewählte Firmware herunterladen (.bin)</a></p></form>
<div id="main-selected-notes-wrap" class="release-notes" hidden><h3>Änderungen der ausgewählten Version</h3><p id="main-selected-notes" class="release-notes-text"></p></div>
<progress id="update-progress" max="100" value="0" hidden></progress><p id="update-message" class="message" role="status"></p>
<details><summary>Firmware-Datei manuell hochladen</summary><p>Eine für dieses ESP32-S3-Board kompilierte Datei (.bin) auswählen.</p><form id="update-form"><label>Firmware-Datei<input name="firmware" type="file" accept=".bin,application/octet-stream" required></label><button type="submit">Datei hochladen</button></form></details>
</section>
<section id="remote-relay-section" aria-labelledby="remote-relay-heading">
<div class="heading"><h2 id="remote-relay-heading">WLAN-Relais</h2><span id="remote-relay-status" class="badge" role="status">Status wird geladen …</span></div>
<p id="remote-relay-detail" class="muted">Verbindung zur Relaiserweiterung wird geprüft.</p>
<h3>Relaiserweiterung 1</h3>
<div id="remote-relay-switches" class="remote-relay-switches">
<button type="button" class="relay-switch" role="switch" aria-checked="false" data-relay="1" disabled><span class="switch-track" aria-hidden="true"></span><span>Relais 1</span></button>
<button type="button" class="relay-switch" role="switch" aria-checked="false" data-relay="2" disabled><span class="switch-track" aria-hidden="true"></span><span>Relais 2</span></button>
<button type="button" class="relay-switch" role="switch" aria-checked="false" data-relay="3" disabled><span class="switch-track" aria-hidden="true"></span><span>Relais 3</span></button>
<button type="button" class="relay-switch" role="switch" aria-checked="false" data-relay="4" disabled><span class="switch-track" aria-hidden="true"></span><span>Relais 4</span></button>
</div><div id="remote-extra-groups"></div><p id="remote-relay-message" class="message" role="status"></p>
</section>
<section id="remote-relay-settings" aria-labelledby="remote-relay-settings-heading"><h2 id="remote-relay-settings-heading">Relaiserweiterungen</h2>
<p class="muted">Bis zu vier Erweiterungen mit je vier Relais.</p>
<label>Erweiterung<select id="remote-relay-board"><option value="0">Relaiserweiterung 1</option><option value="1">Relaiserweiterung 2</option><option value="2">Relaiserweiterung 3</option><option value="3">Relaiserweiterung 4</option></select></label>
<p id="remote-relay-slot-status" class="muted" role="status">Status wird geladen …</p>
<details id="relay-add-details"><summary>Neue Erweiterung hinzufügen</summary>
<p class="muted">Erweiterung einschalten und suchen. Nach dem Start der Einrichtung IO0 an der Erweiterung drei Sekunden drücken.</p>
<button id="relay-pair-scan" type="button">Neue Erweiterungen suchen</button><p id="relay-pair-scan-message" class="message" role="status"></p>
<form id="relay-pair-form"><label>Einrichtungs-WLAN<select name="ssid" required disabled><option value="">Bitte zuerst suchen</option></select></label><button type="submit" disabled>Erweiterung einrichten</button></form>
<button id="relay-pair-cancel" type="button" hidden>Einrichtung abbrechen</button><p id="relay-pair-message" class="message" role="status"></p></details>
<details id="relay-edit-details"><summary>Relais beschriften und Verbindung bearbeiten</summary>
<form id="remote-relay-form">
<label>Relais 1<input name="label1" maxlength="48" required></label>
<label>Relais 2<input name="label2" maxlength="48" required></label>
<label>Relais 3<input name="label3" maxlength="48" required></label>
<label>Relais 4<input name="label4" maxlength="48" required></label>
<details class="relay-manual-settings"><summary>Verbindung manuell einrichten</summary>
<p class="muted">Für bereits im Heimnetz eingerichtete Erweiterungen. Bei unveränderter Adresse bleibt der gespeicherte Schlüssel erhalten.</p>
<label>Im Heimnetz gefunden<select id="relay-discovery-select"><option value="">Erweiterung auswählen …</option></select></label>
<button id="relay-discovery-search" type="button">Erweiterungen suchen</button><p id="relay-discovery-message" class="message" role="status">Suche wird vorbereitet …</p>
<label>IP-Adresse<input name="address" maxlength="15" placeholder="192.168.1.100" autocomplete="off"></label>
<label>Verbindungsschlüssel<input name="key" type="password" maxlength="32" autocomplete="new-password" spellcheck="false"></label></details>
<button type="submit">Änderungen speichern</button></form><p id="remote-relay-config-message" class="message" role="status"></p></details>
<button id="remote-relay-forget" type="button" class="secondary-button" hidden>Erweiterung vergessen</button><p id="remote-relay-forget-message" class="message" role="status"></p>
<details id="relay-forget-recovery" hidden><summary>Probleme beim Zurücksetzen?</summary><p class="muted">Falls die Rückmeldung verloren ging, lässt sich die Zuordnung hier entfernen. Die Karte wird dabei nicht zurückgesetzt und kann noch WLAN-Zugangsdaten besitzen.</p><button id="remote-relay-discard" type="button" class="secondary-button">Nur Zuordnung entfernen</button></details>
</section>
<section id="remote-relay-update-section" aria-labelledby="remote-relay-update-heading"><h2 id="remote-relay-update-heading">Firmware der Relaiskarte aktualisieren</h2>
<p class="muted">Die ausgewählte Erweiterung startet nach dem Update neu und stellt ihre gespeicherten Zustände wieder her.</p>
<p id="remote-update-availability" class="muted" role="status"></p>
<label>Erweiterung<select id="remote-update-board"><option value="0">Relaiserweiterung 1</option><option value="1">Relaiserweiterung 2</option><option value="2">Relaiserweiterung 3</option><option value="3">Relaiserweiterung 4</option></select></label>
<p id="relay-installed-version" class="muted">Installierte Firmware wird ermittelt …</p>
<details class="release-notes"><summary>Änderungen der installierten Version</summary><p id="relay-installed-notes" class="release-notes-text" role="status">Release Notes werden geladen …</p></details>
<button id="remote-release-check" type="button">GitHub-Releases suchen</button><p id="remote-release-message" class="message" role="status">Noch nicht nach Updates gesucht.</p>
<form id="remote-github-form"><label>Release mit Relaiskarten-Firmware<select name="release" required disabled><option value="">Bitte zuerst Releases suchen</option></select></label><button type="submit" disabled>Ausgewählte Version installieren</button></form>
<div id="relay-selected-notes-wrap" class="release-notes" hidden><h3>Änderungen der ausgewählten Version</h3><p id="relay-selected-notes" class="release-notes-text"></p></div>
<details><summary>Firmwaredatei manuell hochladen</summary><form id="remote-relay-update-form"><label>Firmwaredatei (.bin)<input name="firmware" type="file" accept=".bin,application/octet-stream" required></label><button type="submit" disabled>Relaiskarte aktualisieren</button></form></details>
<progress id="remote-relay-update-progress" max="100" value="0" hidden></progress><p id="remote-relay-update-message" class="message" role="status"></p>
</section>
<section id="simulation-settings" aria-labelledby="simulation-heading">
<h2 id="simulation-heading">Testwerte für die Sensoren</h2>
<p id="test-page-status" class="muted" role="status">Verbindung wird aufgebaut …</p>
<div id="simulation-section" hidden>
<p class="muted">Temperaturen oder Sensorfehler einstellen und übernehmen. Die automatische Variation lässt Temperaturen schwanken. Werte gelten bis zum Neustart.</p>
<form id="sim-form"><div id="sim-fields"></div><label class="sim-check"><input type="checkbox" name="variation"> Temperaturen automatisch variieren</label><button type="submit">Testwerte übernehmen</button></form>
<p id="sim-message" class="message" role="status"></p>
</div>
</section>
<div id="sensor-grid"><div id="controllers"></div>
<section id="roof-section" aria-labelledby="roof-heading" hidden>
<div class="heading"><h2 id="roof-heading">Dachsensor</h2><span id="roof-badge" class="badge" role="status">Status wird geladen …</span></div>
<p id="roof-detail" class="muted">Warte auf ein Lebenszeichen aus dem Heimnetz.</p>
</section></div>
<noscript><p>Bitte JavaScript aktivieren, um Messwerte und Einstellungen anzuzeigen.</p></noscript>
<script>
const root=document.getElementById('controllers');
const connection=document.getElementById('connection');
function setConnectionStatus(connected){
  const text=connected?'Verbunden':'Keine Verbindung';
  connection.className=connected?'muted':'error';
  if(connection.textContent!==text)connection.textContent=text;
  document.getElementById('connection-dot').classList.toggle('online',connected);
  document.getElementById('connection-dot').classList.toggle('offline',!connected);
}
const navToggle=document.getElementById('nav-toggle'),navLinks=document.getElementById('nav-links');
const compactNav=window.matchMedia('(max-width: 999px)');
function syncNavigation(){
  navToggle.hidden=!compactNav.matches;
  navLinks.hidden=compactNav.matches;
  navToggle.setAttribute('aria-expanded','false');
}
navToggle.addEventListener('click',()=>{
  const expanded=navToggle.getAttribute('aria-expanded')==='true';
  navLinks.hidden=expanded;
  navToggle.setAttribute('aria-expanded',String(!expanded));
});
navToggle.addEventListener('keydown',event=>{
  if(event.key==='Escape'&&!navLinks.hidden){navLinks.hidden=true;navToggle.setAttribute('aria-expanded','false');navToggle.focus();}
});
navLinks.addEventListener('keydown',event=>{
  if(event.key==='Escape'&&compactNav.matches){navLinks.hidden=true;navToggle.setAttribute('aria-expanded','false');navToggle.focus();}
});
if(compactNav.addEventListener)compactNav.addEventListener('change',syncNavigation);else compactNav.addListener(syncNavigation);
syncNavigation();
const settingsSubnav=document.getElementById('settings-subnav');
settingsSubnav.addEventListener('click',event=>{
  if(compactNav.matches&&event.target.closest('a')){navLinks.hidden=true;navToggle.setAttribute('aria-expanded','false');}
});
function syncSettingsNavigation(){
  document.querySelectorAll('#settings-subnav a').forEach(link=>{
    if(location.hash&&link.getAttribute('href')===location.hash)link.setAttribute('aria-current','location');
    else link.removeAttribute('aria-current');
  });
}
window.addEventListener('hashchange',syncSettingsNavigation);
syncSettingsNavigation();
function markCurrentPage(){
  const pages={'/':'.dashboard-link','/verlauf':'.history-link','/regelungsuebersicht':'.organigram-link','/info':'.info-link','/einstellungen':'.settings-link','/testwerte':'.settings-link','/sicherheit':'.settings-link'};
  const selector=pages[location.pathname],link=selector&&document.querySelector(selector);
  if(link)link.setAttribute('aria-current','page');
}
markCurrentPage();
function syncThemeControls(){
  const light=document.documentElement.classList.contains('light');
  document.getElementById('theme-light').setAttribute('aria-pressed',String(light));
  document.getElementById('theme-dark').setAttribute('aria-pressed',String(!light));
}
function setTheme(light){
  document.documentElement.classList.toggle('light',light);
  document.documentElement.classList.toggle('dark',!light);
  try{localStorage.setItem('rd-th',light?'light':'dark');}catch(error){}
  syncThemeControls();
  if(document.body.classList.contains('history-page'))drawHistory();
}
document.getElementById('theme-light').addEventListener('click',()=>setTheme(true));
document.getElementById('theme-dark').addEventListener('click',()=>setTheme(false));
syncThemeControls();
const temperatureNumber=new Intl.NumberFormat('de-DE',{minimumFractionDigits:1,maximumFractionDigits:1});
const setpointNumber=new Intl.NumberFormat('de-DE',{maximumFractionDigits:1});
const inputNumber=value=>typeof value==='number'&&Number.isFinite(value)?value.toFixed(1):'';
// Gespeicherte Nachkommastellen unverfälscht zeigen; neue Eingaben sind ganzzahlig.
const settingNumber=value=>typeof value==='number'&&Number.isFinite(value)?String(value):'';
function stepIntegerInput(input,direction){
  const current=input.value===''?Number(input.dataset.savedValue):Number(input.value);
  const minimum=Number(input.min),maximum=Number(input.max);
  const next=Math.min(maximum,Math.max(minimum,Number.isInteger(current)?current+direction:direction>0?Math.ceil(current):Math.floor(current)));
  if(!Number.isFinite(next)||input.value===settingNumber(next))return;
  input.value=settingNumber(next);
  input.dispatchEvent(new Event('input',{bubbles:true}));
}
const panels=new Map();
const wifiForm=document.getElementById('wifi-form');
const wifiMessage=document.getElementById('wifi-message');
const wifiReset=document.getElementById('wifi-reset');
const wifiScanButton=document.getElementById('wifi-scan');
const wifiScanMessage=document.getElementById('wifi-scan-message');
const wifiNetworks=document.getElementById('wifi-networks');
const wifiNetworkLabel=document.getElementById('wifi-network-label');
const roofBadge=document.getElementById('roof-badge');
const roofDetail=document.getElementById('roof-detail');
const remoteRelayForm=document.getElementById('remote-relay-form');
const remoteRelayBoard=document.getElementById('remote-relay-board');
const remoteRelayForget=document.getElementById('remote-relay-forget');
const remoteRelayForgetMessage=document.getElementById('remote-relay-forget-message');
const remoteRelaySlotStatus=document.getElementById('remote-relay-slot-status');
const remoteRelayDiscard=document.getElementById('remote-relay-discard');
const relayForgetRecovery=document.getElementById('relay-forget-recovery');
const remoteUpdateBoard=document.getElementById('remote-update-board');
const remoteExtraGroups=document.getElementById('remote-extra-groups');
const relayDiscoverySelect=document.getElementById('relay-discovery-select');
const relayDiscoverySearch=document.getElementById('relay-discovery-search');
const relayDiscoveryMessage=document.getElementById('relay-discovery-message');
const relayPairForm=document.getElementById('relay-pair-form');
const relayPairScan=document.getElementById('relay-pair-scan');
const relayPairScanMessage=document.getElementById('relay-pair-scan-message');
const relayPairMessage=document.getElementById('relay-pair-message');
const relayPairCancel=document.getElementById('relay-pair-cancel');
let remoteRelayCards=[],relayDiscoverySignature='',relayPairActive=false,relayPairScanBusy=false;
const remoteExtraCards=new Map();
const remoteRelayMessage=document.getElementById('remote-relay-message');
const remoteRelayConfigMessage=document.getElementById('remote-relay-config-message');
let remoteRelayDirty=false,remoteRelayBusy=false,remoteRelayRevision=0,remoteRelayState=null;
let pendingRemoteAction=null;
const remoteRelayUpdateForm=document.getElementById('remote-relay-update-form');
const remoteRelayUpdateMessage=document.getElementById('remote-relay-update-message');
const remoteRelayUpdateProgress=document.getElementById('remote-relay-update-progress');
const remoteReleaseCheck=document.getElementById('remote-release-check');
const remoteReleaseMessage=document.getElementById('remote-release-message');
const remoteGithubForm=document.getElementById('remote-github-form');
let remoteRelayUploadBusy=false,remoteRelaySearchBusy=false,remoteRelayReleases=[];
const manualSwitch=document.getElementById('manual-switch');
const pumpSwitch=document.getElementById('pump-switch');
const directionSwitch=document.getElementById('direction-switch');
const pumpBadge=document.getElementById('pump-badge');
const pumpMessage=document.getElementById('pump-message');
const autoForm=document.getElementById('auto-form');
const autoMessage=document.getElementById('auto-message');
const solarForm=document.getElementById('solar-form'),solarMessage=document.getElementById('solar-message');
const solarManualSwitch=document.getElementById('solar-manual-switch'),solarPowerSwitch=document.getElementById('solar-power-switch'),solarControlMessage=document.getElementById('solar-control-message');
let solarDirty=false,solarBusy=false,solarControlBusy=false,solarRevision=0,solarState=null;
const heatingSwitch=document.getElementById('heating-switch');
function drawBurnerPipes(){
  const box=document.querySelector('.storage-overview'),icon=box.querySelector('.burner-icon'),tank=box.querySelector('.pump-flow'),wires=box.querySelector('.burner-pipes');
  const a=box.getBoundingClientRect(),b=icon.getBoundingClientRect(),t=tank.getBoundingClientRect();
  if(!a.width||!a.height||!b.width||!t.width)return;
  wires.setAttribute('viewBox',`0 0 ${a.width} ${a.height}`);
  const x1=b.left-a.left+b.width*123/128,x2=t.left-a.left+t.width*14/440,mid=(x1+x2)/2,mobile=t.top>=b.bottom;
  for(const [name,by,ty] of [['vl',42,90],['rl',54,116]]){
    const y1=b.top-a.top+b.height*by/96,y2=t.top-a.top+t.height*ty/280;
    const lane=b.bottom-a.top+(name==='vl'?6:14),edge=x2-(name==='vl'?7:14);
    wires.querySelector('.pipe-'+name).setAttribute('d',mobile?`M${x1} ${y1} H${x1+6} V${lane} H${edge} V${y2} H${x2}`:`M${x1} ${y1} H${mid} V${y2} H${x2}`);
    const label=wires.querySelector('.pipe-'+name+'-label');label.setAttribute('x',mobile?x1+9:mid+2);label.setAttribute('y',mobile?lane:y2-3);
  }
  const solar=box.querySelector('.solar-icon').getBoundingClientRect();
  if(!solar.width||!solar.height)return;
  const tx=t.left-a.left+t.width*96/440,ty=t.top-a.top+t.height*260/280,lane=t.bottom-a.top+14;
  const sx=solar.left-a.left+solar.width*36/128,sy=solar.top-a.top+solar.height*81/110,bend=solar.left-a.left-8;
  wires.querySelector('.solar-link').setAttribute('d',`M${sx} ${sy} H${bend} V${lane} H${tx} V${ty}`);
}
window.addEventListener('resize',drawBurnerPipes);
if(typeof ResizeObserver!=='undefined')new ResizeObserver(drawBurnerPipes).observe(document.querySelector('.storage-overview'));
requestAnimationFrame(drawBurnerPipes);
// Die dekorative Drehung ruht außerhalb des sichtbaren Bereichs und in Hintergrund-Tabs.
function setupPumpMotion(){
  const overview=document.querySelector('.storage-overview');
  let visible=false;
  const sync=()=>overview.classList.toggle('motion-visible',visible&&!document.hidden);
  if('IntersectionObserver' in window){
    const observer=new IntersectionObserver(entries=>{visible=entries.some(entry=>entry.isIntersecting);sync();});
    observer.observe(overview);
  }else{visible=true;sync();}
  document.addEventListener('visibilitychange',sync);
}
setupPumpMotion();
const heatingMessage=document.getElementById('heating-message');
let heatingState=null,heatingBusy=false,heatingRevision=0;
const simForm=document.getElementById('sim-form');
const simMessage=document.getElementById('sim-message');
let simDirty=false,simBusy=false,simRevision=0,simulationEnabled=false;
let testParameterHandled=false,modeChanging=false;
function reflectTestModeInUrl(enabled){
  const url=new URL(location.href);
  if(enabled)url.searchParams.set('test','on');else url.searchParams.delete('test');
  history.replaceState(null,'',url.pathname+url.search+url.hash);
}
let autoDirty=false,autoBusy=false,autoRevision=0;
let pumpState=null,pumpBusy=false,pumpRevision=0;
const directionName=value=>value===0?'Kombispeicher → Pufferspeicher':'Pufferspeicher → Kombispeicher';
function updatePumpControls(){
  manualSwitch.disabled=pumpBusy||pumpState===null;
  pumpSwitch.disabled=directionSwitch.disabled=pumpBusy||pumpState===null||!pumpState.manual;
  if(pumpState===null)return;
  const blocked=!heatingState||!heatingState.configValid||heatingState.winter;
  if(blocked&&pumpState.manual){
    // Ausschalten bleibt immer möglich; der gesperrte Start wird nicht angeboten.
    if(!pumpState.requestedOn&&pumpState.requestedDirection===0)pumpSwitch.disabled=true;
    if(pumpState.requestedOn&&pumpState.requestedDirection===1)directionSwitch.disabled=true;
  }
  const reason=heatingState&&heatingState.configValid&&heatingState.winter?'Ölbrenner freigegeben':'Betriebsart unbekannt';
  document.getElementById('pump-help').textContent=!pumpState.manual?'Zuerst die manuelle Steuerung einschalten.':
    blocked&&pumpState.requestedDirection===0&&!pumpState.requestedOn?reason+': Kombi → Puffer gesperrt. Zum Starten Richtung EIN wählen (Puffer → Kombi).':
    'Pumpenauftrag: '+(pumpState.requestedOn?'EIN':'AUS')+'. Der tatsächliche Zustand steht oben.';
  document.getElementById('direction-help').textContent=blocked?'Richtung EIN: Puffer → Kombi. Kombi → Puffer ist gesperrt ('+reason+').':'Aus: Kombi → Puffer. Ein: Puffer → Kombi.';
}
function updatePump(state){
  if(!state||typeof state.manual!=='boolean'||typeof state.on!=='boolean'||typeof state.requestedOn!=='boolean'||![0,1].includes(state.direction)||![0,1].includes(state.requestedDirection))throw new Error('Ungültiger Pumpenstatus');
  pumpState=state;
  manualSwitch.setAttribute('aria-checked',String(state.manual));
  manualSwitch.setAttribute('data-state',state.manual?'on':'off');
  pumpSwitch.setAttribute('aria-checked',String(state.requestedOn));
  directionSwitch.setAttribute('aria-checked',String(state.requestedDirection===1));
  pumpBadge.textContent='Backloadpumpe: '+(state.on?'AN':'AUS')+(state.switching?' · Richtungswechsel':'');
  pumpBadge.className='badge'+(state.on?' on':'');
  document.getElementById('pump-direction-state').textContent='Förderrichtung: '+directionName(state.direction)+(state.direction!==state.requestedDirection?' · Angefordert: '+directionName(state.requestedDirection):'');
  document.getElementById('pump-flow-title').textContent='Förderrichtung: '+directionName(state.direction)+', Backloadpumpe '+(state.on?'an · Fördersymbol: '+(state.direction===0?'Linkslauf':'Rechtslauf'):'aus');
  document.getElementById('pump-flow-arrow').setAttribute('d',state.direction===0?'M187 154H253':'M253 154H187');
  document.getElementById('pump-flow-arrow').style.visibility='visible';
  const diagram=document.querySelector('.pump-flow');
  diagram.setAttribute('data-direction',state.direction===0?'ccw':'cw');
  diagram.classList.toggle('stopped',!state.on);
  document.getElementById('pump-mode-detail').textContent=state.manual?'Manuelle Steuerung aktiv · Ölbrennersperre gilt.':state.automaticConfigured?'Automatik aktiv · Bei Fehler eines Regelungsfühlers bleibt die Backloadpumpe aus.':'Automatik gesperrt · Backloadpumpe aus.';
  document.getElementById('pump-progress').textContent=state.phase==='stopping'?'Backloadpumpe aus · Richtungswechsel nach 2 Sekunden.':state.phase==='settling'?'Richtung umgeschaltet · Relais schaltet.':'';
  updatePumpControls();
}
function unknownPump(){
  pumpState=null;
  manualSwitch.setAttribute('data-state','unknown');
  pumpBadge.textContent='Backloadpumpe: Status unbekannt';pumpBadge.className='badge';
  document.getElementById('pump-direction-state').textContent='Keine aktuelle Förderrichtung. Verbindung zum ESP32 prüfen.';
  document.getElementById('pump-flow-title').textContent='Förderrichtung unbekannt';
  document.getElementById('pump-flow-arrow').style.visibility='hidden';
  document.getElementById('pump-help').textContent='Pumpenstatus unbekannt. Warte auf eine bestätigte Verbindung.';
  document.getElementById('pump-mode-detail').textContent='Steuerungsmodus unbekannt. Die Schalter werden nach erneuter Statusabfrage freigegeben.';
  document.getElementById('pump-progress').textContent='';
  document.querySelector('.pump-flow').setAttribute('data-direction','unknown');
  document.querySelector('.pump-flow').classList.add('stopped');
  updatePumpControls();
}
function updateStorageDiagram(controllers){
  const names={0:'Kombispeicher oben',1:'Kombispeicher Mitte',2:'Pufferspeicher oben',3:'Kombispeicher unten'};
  const descriptions=[];
  for(const id of [0,1,3,2]){
    const sensor=Array.isArray(controllers)?controllers.find(c=>c.id===id):null;
    const valid=sensor&&sensor.valid===true&&typeof sensor.temperature==='number'&&Number.isFinite(sensor.temperature);
    const value=valid?temperatureNumber.format(sensor.temperature)+' °C':sensor?'Fehler':'—';
    const output=document.getElementById('diagram-temp-'+id);
    output.textContent=value;
    output.classList.toggle('fault',!!sensor&&!valid);
    descriptions.push(names[id]+': '+(valid?value:sensor?'Sensorfehler':'keine aktuellen Daten'));
  }
  document.getElementById('pump-flow-desc').textContent=descriptions.join('; ')+'.';
}
function updateSolarDiagram(state,simulation,controllers){
  const diagram=document.getElementById('solar-pump-diagram'),wires=document.querySelector('.burner-pipes');
  const sensor=Array.isArray(controllers)?controllers.find(c=>c.id===4):null;
  const validSensor=!!state&&state.sensorsValid&&sensor&&sensor.valid===true&&typeof sensor.temperature==='number'&&Number.isFinite(sensor.temperature);
  const panel=validSensor?'Solarpanel: '+temperatureNumber.format(sensor.temperature)+' °C':sensor&&!sensor.valid?'Solarpanel: Sensorfehler':'Solarpanel: keine aktuellen Daten';
  const issue=state&&(state.paused?'Pausiert':!state.manual&&!state.configValid?'Einstellung ungültig':!state.manual&&!state.sensorsValid?'Sensorfehler':null);
  const active=!!(state&&!issue&&state.on);
  const label=!state?'Status unbekannt':issue?issue+' · AUS':active?(simulation?'EIN (simuliert)':'EIN'):(simulation?'AUS (simuliert)':'AUS');
  const detail=!state?'Keine aktuellen Daten':(simulation?'Relais 3: AUS · Testmodus':'Relais 3: '+(state.relayOn?'AN':'AUS'))+(state.manual?' · Handbetrieb':'');
  diagram.classList.toggle('running',active);
  wires.classList.toggle('solar-active',active);
  document.getElementById('solar-diagram-state').textContent=label;
  document.getElementById('solar-diagram-panel').textContent=panel;
  document.getElementById('solar-diagram-detail').textContent=detail;
  diagram.setAttribute('aria-label','Solarpumpe: '+label+'. '+panel+'. '+detail+'.');
}
async function changePump(action){
  if(pumpBusy||pumpState===null||(action!=='manual'&&!pumpState.manual))return;
  const value=action==='manual'?!pumpState.manual:action==='power'?!pumpState.requestedOn:pumpState.requestedDirection===0;
  pumpBusy=true;pumpRevision++;updatePumpControls();
  pumpMessage.className='message';pumpMessage.textContent='Befehl wird übertragen …';
  try{
    const state=await request('/api/pump',{method:'POST',headers:{'Content-Type':'application/x-www-form-urlencoded','X-Requested-With':'HeatingUI'},body:new URLSearchParams({action,value:value?'1':'0'})});
    updatePump(state);
    pumpMessage.textContent=action==='manual'?(state.manual?'Manuelle Steuerung eingeschaltet. Backloadpumpe ist zunächst AUS.':'Manuelle Steuerung ausgeschaltet.'):'Pumpenbefehl übernommen.';
  }catch(error){
    // Eine HTTP-Ablehnung ist kein Verbindungsabbruch. Auch bei verlorener
    // Antwort ausschließlich lesen: einen Schaltauftrag niemals blind wiederholen.
    let refreshed=false;
    const heatingReadRevision=heatingRevision;
    pumpMessage.className='message error';pumpMessage.textContent='Pumpenstatus wird nach dem Fehler erneut geprüft …';
    try{
      const data=await request('/api/status');
      if(heatingReadRevision===heatingRevision)updateHeating(data.heating);
      updatePump(data.pump);refreshed=true;
    }catch(statusError){unknownPump();}
    pumpMessage.textContent=(error.status?'Befehl abgelehnt: ':'Schalten nicht bestätigt: ')+error.message+
      (refreshed?' Aktueller Pumpenstatus wurde neu abgefragt.':' Pumpenstatus konnte nicht geladen werden. Verbindung prüfen.');
  }finally{pumpBusy=false;updatePumpControls();}
}
manualSwitch.addEventListener('click',()=>changePump('manual'));
pumpSwitch.addEventListener('click',()=>changePump('power'));
directionSwitch.addEventListener('click',()=>changePump('direction'));
function updateHeating(state){
  if(!state||typeof state.winter!=='boolean'||typeof state.configValid!=='boolean'||typeof state.relayOn!=='boolean')throw new Error('Ungültige Sommer-/Winterumschaltung');
  heatingState=state;
  heatingSwitch.disabled=heatingBusy;
  heatingSwitch.setAttribute('aria-checked',String(state.configValid&&state.winter));
  heatingSwitch.setAttribute('data-state',!state.configValid?'unknown':state.winter?'on':'off');
  document.getElementById('heating-switch-label').textContent=state.configValid?(state.winter?'Winterbetrieb':'Sommerbetrieb'):'Sommerbetrieb bestätigen';
  const badge=document.getElementById('heating-badge');
  badge.textContent=state.configValid?(state.winter?'Winter · Kombi Mitte · Öl freigegeben':'Sommer · Kombi oben · Öl gesperrt'):'Betriebsart unbekannt';
  badge.className='badge'+(!state.configValid?' waiting':state.winter?' heat-active':' summer-active');
  document.getElementById('heating-detail').textContent=!state.configValid?'Betriebsart ungültig. Sommerbetrieb bestätigen; Ölbrenner und Automatik bleiben aus.':
    simulationEnabled?'':
    'Relais 4: '+(state.relayOn?'AN · Ölbrenner freigegeben.':state.winter?'AUS · Freigabe während Update oder Neustart pausiert.':'AUS · Ölbrenner gesperrt.');
  updateBurnerDiagram(state);
  updatePumpControls();
}
function unknownHeating(){
  heatingState=null;heatingSwitch.disabled=true;
  heatingSwitch.setAttribute('data-state','unknown');
  document.getElementById('heating-badge').textContent='Betriebsart unbekannt';
  document.getElementById('heating-badge').className='badge waiting';
  document.getElementById('heating-detail').textContent='Keine aktuellen Daten. Verbindung zum ESP32 prüfen; die Betriebsart wird erneut abgefragt.';
  updateBurnerDiagram(null);
  updatePumpControls();
}
function updateBurnerDiagram(state){
  const known=!!state&&state.configValid;
  const released=known&&(simulationEnabled?state.winter:state.relayOn);
  const label=!known?'Status unbekannt':simulationEnabled?(state.winter?'Freigegeben (simuliert)':'Gesperrt (simuliert)'):
    state.relayOn?'Freigegeben':state.winter?'Freigabe pausiert':'Gesperrt';
  const detail=!known?(state?'Betriebsart prüfen':'Keine aktuellen Daten'):simulationEnabled?'Relais 4: AUS · Testmodus':'Relais 4: '+(state.relayOn?'AN':'AUS');
  const diagram=document.getElementById('oil-burner-diagram');
  diagram.classList.toggle('released',!!released);
  diagram.setAttribute('aria-label','Ölbrennerfreigabe: '+label+'. '+detail+'.');
  document.getElementById('oil-burner-state').textContent=label;
  document.getElementById('oil-burner-detail').textContent=detail;
}
async function changeHeating(){
  if(heatingBusy||!heatingState)return;
  const next=heatingState.configValid?!heatingState.winter:false;
  heatingBusy=true;heatingRevision++;autoRevision++;pumpRevision++;heatingSwitch.disabled=true;
  heatingMessage.className='message';heatingMessage.textContent='Betriebsart wird gespeichert und geschaltet …';
  try{
    const state=await request('/api/heating',{method:'POST',headers:{'Content-Type':'application/x-www-form-urlencoded','X-Requested-With':'HeatingUI'},body:new URLSearchParams({winter:next?'1':'0'})});
    updateHeating(state);
    heatingMessage.textContent=state.winter?'Winterbetrieb gespeichert. Kombi → Puffer gesperrt.':'Sommerbetrieb gespeichert. Ölbrenner gesperrt.';
  }catch(error){unknownHeating();heatingMessage.className='message error';heatingMessage.textContent='Umschalten nicht bestätigt: '+error.message+' Aktuellen Status prüfen.';}
  finally{heatingBusy=false;heatingSwitch.disabled=heatingState===null;}
}
heatingSwitch.addEventListener('click',changeHeating);
autoForm.addEventListener('input',()=>{autoDirty=true;autoMessage.textContent='Noch nicht gespeichert.';autoMessage.className='message';});
autoForm.addEventListener('change',()=>{autoDirty=true;autoMessage.textContent='Noch nicht gespeichert.';autoMessage.className='message';});
autoForm.addEventListener('click',event=>{
  const button=event.target.closest('.stepper button');
  if(button&&!button.disabled&&autoForm.elements[button.dataset.field])stepIntegerInput(autoForm.elements[button.dataset.field],Number(button.dataset.direction));
});
function updateAutomatic(state){
  if(!state)return;
  document.getElementById('auto-error').hidden=state.configValid;
  document.getElementById('auto-error').textContent=state.configValid?'':'Automatik gesperrt: Betriebsart und Einstellungen prüfen.';
  autoForm.elements.start.dataset.savedValue=state.differenceStart;
  autoForm.elements.minimumCombi.dataset.savedValue=state.minimumCombi;
  autoForm.elements.minimumBuffer.dataset.savedValue=state.minimumBuffer;
  if(autoDirty||autoBusy||autoForm.contains(document.activeElement))return;
  autoForm.elements.start.value=settingNumber(state.differenceStart);
  autoForm.elements.minimumCombi.value=settingNumber(state.minimumCombi);
  autoForm.elements.minimumBuffer.value=settingNumber(state.minimumBuffer);
}
function automaticSensorId(state){
  return state.controlSensorId === 1 ? 1 : 0;
}
function automaticRuleText(state,controllers,pump){
  if(!state||!state.configValid||!Array.isArray(controllers))return null;
  const id=automaticSensorId(state),combi=controllers.find(c=>c.id===id),buffer=controllers.find(c=>c.id===2),settings=controllers.find(c=>c.id===0);
  if(!combi||!buffer||!settings||!Number.isFinite(state.differenceStart)||
     state.differenceStart<5||state.differenceStart>10||!Number.isFinite(state.minimumCombi)||state.minimumCombi<0||state.minimumCombi>80||
     !Number.isFinite(state.minimumBuffer)||state.minimumBuffer<0||state.minimumBuffer>80)return null;
  const temp=value=>setpointNumber.format(value)+' °C',delta=value=>setpointNumber.format(value)+' K';
  const targetValid=settings.configValid&&Number.isFinite(settings.target)&&settings.target>=68&&settings.target<=72;
  const valuesValid=combi.valid&&buffer.valid&&Number.isFinite(combi.temperature)&&Number.isFinite(buffer.temperature);
  const below=[];
  if(valuesValid&&combi.temperature<state.minimumCombi)below.push('Kombi '+temp(combi.temperature)+' < '+temp(state.minimumCombi));
  if(valuesValid&&buffer.temperature<state.minimumBuffer)below.push('Puffer '+temp(buffer.temperature)+' < '+temp(state.minimumBuffer));
  return {
    sensor:'Fühler: '+combi.name+' und Pufferspeicher oben. Starttemperatur: '+(targetValid?temp(settings.target):'ungültig')+'.',
    minimumStatus:pump&&pump.manual?'Handbetrieb: Mindesttemperaturen nicht aktiv.':!valuesValid?'Mindesttemperaturen: keine gültigen Fühlerwerte.':below.length?'Automatik gesperrt: '+below.join(' · ')+'.':'Mindesttemperaturen erreicht.',
    status:pump&&pump.manual?'Manuelle Steuerung aktiv · Ölbrennersperre gilt.':
      !combi.valid||!buffer.valid?'Sensorfehler · Automatik hält die Backloadpumpe aus.':'Gespeicherte Einstellungen.',
    chargeStart:!state.allowCharging?'Einlagerung durch Sommer-/Winterumschaltung gesperrt.':!targetValid?'Einlagerung gesperrt: Starttemperatur muss 68–72 °C betragen.':
      'Ölbrennerfreigabe AUS UND Kombi ≥ '+temp(settings.target)+' UND Kombi ≥ '+temp(state.minimumCombi)+' UND Puffer ≥ '+temp(state.minimumBuffer)+' UND Kombi − Puffer ≥ '+delta(state.differenceStart)+'.',
    chargeStop:'Kombi < '+temp(state.minimumCombi)+' ODER Puffer < '+temp(state.minimumBuffer)+' ODER Kombi − Puffer ≤ 3 K ODER Ölbrenner freigegeben / Betriebsart unbekannt.',
    returnStart:'Kombi ≥ '+temp(state.minimumCombi)+' UND Puffer ≥ '+temp(state.minimumBuffer)+' UND Puffer − Kombi ≥ '+delta(state.differenceStart)+'. Ohne Ölsperre.',
    returnStop:'Kombi < '+temp(state.minimumCombi)+' ODER Puffer < '+temp(state.minimumBuffer)+' ODER Puffer − Kombi ≤ 3 K.'
  };
}
function updateAutomaticRules(state,controllers,pump){
  const rules=automaticRuleText(state,controllers,pump);
  document.getElementById('auto-min-status').textContent=rules?rules.minimumStatus:'Mindesttemperaturen: keine gültigen Regelungseinstellungen.';
  document.getElementById('auto-rules-table').hidden=!rules;
  document.getElementById('auto-rules-sensor').textContent=rules?rules.sensor:'Keine aktuellen, gültigen Regelungseinstellungen verfügbar.';
  document.getElementById('auto-rules-state').textContent=rules?rules.status:'';
  for(const [id,key] of [['auto-charge-start','chargeStart'],['auto-charge-stop','chargeStop'],['auto-return-start','returnStart'],['auto-return-stop','returnStop']]){
    document.getElementById(id).textContent=rules?rules[key]:'';
  }
}
function updateSimulation(data){
  simulationEnabled=data.simulation===true;
  document.getElementById('sim-banner').hidden=!simulationEnabled;
  document.getElementById('test-link').hidden=simulationEnabled;
  document.getElementById('simulation-section').hidden=!simulationEnabled;
  document.getElementById('test-page-status').textContent=simulationEnabled?'Testmodus aktiv · Relais am Haupt-ESP aus; WLAN-Relais schalten tatsächlich.':'Testwerte nur im Testmodus verfügbar.';
  document.querySelector('.dashboard-link').href=simulationEnabled?'/?test=on':'/';
  document.querySelector('.history-link').href=simulationEnabled?'/verlauf?test=on':'/verlauf';
  document.querySelector('.info-link').href=simulationEnabled?'/info?test=on':'/info';
  document.querySelector('.organigram-link').href=simulationEnabled?'/regelungsuebersicht?test=on':'/regelungsuebersicht';
  document.querySelector('.settings-link').href=simulationEnabled?'/einstellungen?test=on':'/einstellungen';
  if(!testParameterHandled){
    testParameterHandled=true;
    const url=new URL(location.href),wanted=url.searchParams.get('test');
    if(wanted==='on'&&!simulationEnabled)changeTestMode(true);
    else if(simulationEnabled&&wanted==='off')changeTestMode(false);
    else reflectTestModeInUrl(simulationEnabled);
  }
  if(!simulationEnabled)return;
  const fields=document.getElementById('sim-fields');
  if(!fields.children.length){
    for(const c of data.controllers){
      const item=document.createElement('div');item.className='sim-item';
      const label=document.createElement('label');label.textContent=c.name+' (°C)';
      const input=document.createElement('input');input.name='t'+c.id;input.type='number';input.min='-200';input.max='850';input.step='0.1';input.required=true;
      label.appendChild(input);item.appendChild(label);
      const faultLabel=document.createElement('label');faultLabel.className='sim-check';
      const fault=document.createElement('input');fault.type='checkbox';fault.name='f'+c.id;
      faultLabel.append(fault,document.createTextNode('Sensorfehler simulieren'));item.appendChild(faultLabel);fields.appendChild(item);
    }
  }
  if(simDirty||simBusy||simForm.contains(document.activeElement))return;
  simForm.elements.variation.checked=data.simulationVariation===true;
  for(const c of data.controllers){
    const value=data.simulationValues[c.id];
    simForm.elements['t'+c.id].value=inputNumber(value.temperature);
    simForm.elements['f'+c.id].checked=value.fault;
  }
}
async function changeTestMode(enable){
  if(modeChanging)return;
  const prompt=enable?'Testmodus starten? Pumpe und Ölbrennerfreigabe werden ausgeschaltet und simuliert. WLAN-Relais bleiben schaltbar. Der ESP32 startet neu.':'Testmodus beenden? Der ESP32 startet neu. Die gespeicherte Ölbrennerfreigabe und die echte Pumpenregelung werden wieder aktiv.';
  if(!confirm(prompt)){reflectTestModeInUrl(simulationEnabled);return;}
  modeChanging=true;
  const message=document.getElementById('test-message');
  message.className='message';message.textContent='Betriebsart wird umgestellt. Die Pumpe bleibt während des Neustarts aus …';
  try{
    await request('/api/test-mode',{method:'POST',headers:{'Content-Type':'application/x-www-form-urlencoded','X-Requested-With':'HeatingUI'},body:new URLSearchParams({enabled:enable?'1':'0'})});
    message.textContent='ESP32 startet neu. Falls die Verbindung nicht wiederkommt, mit dem ESP32-WLAN verbinden und die Seite erneut öffnen.';
    setTimeout(()=>location.assign(enable?'/?test=on':'/'),7000);
  }catch(error){modeChanging=false;reflectTestModeInUrl(simulationEnabled);message.className='message error';message.textContent='Umschalten nicht bestätigt: '+error.message+' Bitte den Status nach dem Neuladen prüfen.';}
}
document.getElementById('test-stop').addEventListener('click',()=>changeTestMode(false));
simForm.addEventListener('input',()=>{simDirty=true;simMessage.className='message';simMessage.textContent='Noch nicht übernommen.';});
simForm.addEventListener('change',()=>{simDirty=true;simMessage.className='message';simMessage.textContent='Noch nicht übernommen.';});
simForm.addEventListener('submit',async event=>{
  event.preventDefault();if(!simulationEnabled||simBusy)return;
  const values=new URLSearchParams();
  for(let i=0;i<5;i++){values.set('t'+i,simForm.elements['t'+i].value);values.set('f'+i,simForm.elements['f'+i].checked?'1':'0');}
  values.set('variation',simForm.elements.variation.checked?'1':'0');
  simBusy=true;simRevision++;pumpRevision++;simForm.querySelectorAll('input,button').forEach(e=>e.disabled=true);simMessage.className='message';simMessage.textContent='Wird übernommen …';
  try{await request('/api/simulation',{method:'POST',headers:{'Content-Type':'application/x-www-form-urlencoded','X-Requested-With':'HeatingUI'},body:values});simDirty=false;simMessage.textContent='Testwerte übernommen.';}
  catch(error){simMessage.className='message error';simMessage.textContent='Nicht übernommen: '+error.message;}
  finally{simBusy=false;simForm.querySelectorAll('input,button').forEach(e=>e.disabled=false);}
});
autoForm.addEventListener('submit',async event=>{
  event.preventDefault();if(autoBusy)return;
  if(!Number.isInteger(Number(autoForm.elements.start.value))||autoForm.elements.start.value.trim()===''||Number(autoForm.elements.start.value)<5||Number(autoForm.elements.start.value)>10){
    autoMessage.className='message error';autoMessage.textContent='Startdifferenz: ganze Zahl von 5 bis 10 K eingeben.';return;
  }
  for(const [field,label] of [['minimumCombi','Kombispeicher'],['minimumBuffer','Pufferspeicher']]){
    const value=autoForm.elements[field].value;
    if(!Number.isInteger(Number(value))||value.trim()===''||Number(value)<0||Number(value)>80){
      autoMessage.className='message error';autoMessage.textContent='Mindesttemperatur '+label+': ganze Zahl von 0 bis 80 °C eingeben.';return;
    }
  }
  const values=new URLSearchParams({start:autoForm.elements.start.value,minimumCombi:autoForm.elements.minimumCombi.value,minimumBuffer:autoForm.elements.minimumBuffer.value});
  autoBusy=true;autoRevision++;pumpRevision++;autoForm.querySelectorAll('input,select,button').forEach(e=>e.disabled=true);autoMessage.textContent='Wird gespeichert …';autoMessage.className='message';
  try{
    await request('/api/automatic',{method:'POST',headers:{'Content-Type':'application/x-www-form-urlencoded','X-Requested-With':'HeatingUI'},body:values});
    autoDirty=false;autoMessage.textContent='Regelung gespeichert.';
  }catch(error){autoMessage.className='message error';autoMessage.textContent='Speichern nicht bestätigt: '+error.message;}
  finally{autoBusy=false;autoForm.querySelectorAll('input,select,button').forEach(e=>e.disabled=false);}
});
solarForm.addEventListener('input',()=>{solarDirty=true;solarMessage.className='message';solarMessage.textContent='Noch nicht gespeichert.';});
solarForm.addEventListener('click',event=>{
  const button=event.target.closest('.stepper button');
  if(!button||button.disabled)return;
  const input=solarForm.elements.start,old=Number(input.value);
  input.value=String(Math.max(3,Math.min(10,(Number.isFinite(old)&&input.value!==''?old:5)+Number(button.dataset.direction))));
  input.dispatchEvent(new Event('input',{bubbles:true}));
});
function updateSolar(state,simulation){
  const badge=document.getElementById('solar-badge'),detail=document.getElementById('solar-detail');
  solarState=state;
  const manual=!!(state&&state.manual),requested=!!(state&&state.requestedOn);
  solarManualSwitch.disabled=!state||solarControlBusy||solarBusy||!!state.paused;
  solarPowerSwitch.disabled=!state||solarControlBusy||solarBusy||!!state.paused||!manual;
  solarManualSwitch.setAttribute('aria-checked',String(manual));solarManualSwitch.setAttribute('data-state',!state?'unknown':manual?'on':'off');
  solarPowerSwitch.setAttribute('aria-checked',String(requested));solarPowerSwitch.setAttribute('data-state',!state?'unknown':requested?'on':'off');
  if(!state){badge.className='badge';badge.textContent='Status unbekannt';detail.textContent='Keine aktuellen Daten.';return;}
  const issue=state.paused?'Pausiert':!manual&&!state.configValid?'Einstellung ungültig':!manual&&!state.sensorsValid?'Sensorfehler':null;
  badge.className='badge'+(issue?' waiting':state.on?' on':'');
  badge.textContent=issue?issue+' · AUS':state.on?(simulation?'EIN · simuliert':'EIN'):(simulation?'AUS · simuliert':'AUS');
  detail.textContent=(manual?'Handbetrieb aktiv. ':'Automatik aktiv. ')+(Number.isFinite(state.difference)?'Aktuelle Differenz: '+setpointNumber.format(state.difference)+' K. ':'Differenz nicht verfügbar. ')+
    (state.paused?'Update oder Neustart läuft.':manual&&!state.sensorsValid?'Sensorfehler wird im Handbetrieb übergangen.':!state.configValid?'Bitte Einschaltdifferenz erneut speichern.':!state.sensorsValid?'Solarpanel und Kombispeicher unten prüfen.':'');
  if(!solarDirty&&!solarBusy&&!solarForm.contains(document.activeElement)&&Number.isFinite(state.differenceStart))solarForm.elements.start.value=inputNumber(state.differenceStart);
}
async function changeSolarControl(action){
  if(solarControlBusy||solarBusy||!solarState||solarState.paused||(action==='power'&&!solarState.manual))return;
  const value=action==='manual'?!solarState.manual:!solarState.requestedOn;
  solarControlBusy=true;solarRevision++;updateSolar(solarState,simulationEnabled);
  solarControlMessage.className='message';solarControlMessage.textContent='Befehl wird übertragen …';
  try{
    const state=await request('/api/solar/control',{method:'POST',headers:{'Content-Type':'application/x-www-form-urlencoded','X-Requested-With':'HeatingUI'},body:new URLSearchParams({action,value:value?'1':'0'})});
    updateSolar(state,simulationEnabled);
    solarControlMessage.textContent=action==='manual'?(state.manual?'Handbetrieb eingeschaltet. Solarpumpe ist zunächst AUS.':'Automatik eingeschaltet.'):('Solarpumpe '+(state.on?'eingeschaltet.':'ausgeschaltet.'));
  }catch(error){
    let refreshed=false;
    try{const data=await request('/api/status');updateSolar(data.solar,data.simulation);updateSolarDiagram(data.solar,data.simulation,data.controllers);refreshed=true;}
    catch(statusError){updateSolar(null,false);updateSolarDiagram(null,false,null);}
    solarControlMessage.className='message error';solarControlMessage.textContent=(error.status?'Befehl abgelehnt: ':'Schalten nicht bestätigt: ')+error.message+(refreshed?' Aktueller Status wurde neu abgefragt.':' Status nicht verfügbar. Verbindung prüfen.');
  }finally{solarControlBusy=false;updateSolar(solarState,simulationEnabled);}
}
solarManualSwitch.addEventListener('click',()=>changeSolarControl('manual'));
solarPowerSwitch.addEventListener('click',()=>changeSolarControl('power'));
solarForm.addEventListener('submit',async event=>{
  event.preventDefault();if(solarBusy||solarControlBusy)return;
  const input=solarForm.elements.start.value,number=Number(input);
  if(!input.trim()||!Number.isFinite(number)||number<3||number>10||Math.abs(number*10-Math.round(number*10))>0.0001){solarMessage.className='message error';solarMessage.textContent='Einschaltdifferenz von 3 bis 10 K in 0,1-K-Schritten eingeben.';return;}
  solarBusy=true;solarRevision++;updateSolar(solarState,simulationEnabled);solarForm.querySelectorAll('input,button').forEach(e=>e.disabled=true);solarMessage.className='message';solarMessage.textContent='Wird gespeichert …';
  try{
    const state=await request('/api/solar',{method:'POST',headers:{'Content-Type':'application/x-www-form-urlencoded','X-Requested-With':'HeatingUI'},body:new URLSearchParams({start:input})});
    solarDirty=false;updateSolar(state,simulationEnabled);solarMessage.textContent='Solarregelung gespeichert.';
  }catch(error){solarMessage.className='message error';solarMessage.textContent='Speichern nicht bestätigt: '+error.message;}
  finally{solarBusy=false;solarForm.querySelectorAll('input,button').forEach(e=>e.disabled=false);updateSolar(solarState,simulationEnabled);}
});
let wifiBusy=false,wifiDirty=false,wifiScanning=false,wifiRevision=0,wifiPending=null;
wifiForm.addEventListener('input',()=>{wifiDirty=true;wifiRevision++;});
wifiNetworks.addEventListener('change',()=>{
  if(!wifiNetworks.value)return;
  wifiForm.elements.ssid.value=wifiNetworks.value;
  wifiDirty=true;wifiRevision++;
  wifiForm.elements.password.focus();
});
wifiScanButton.addEventListener('click',async()=>{
  if(wifiScanning||wifiBusy)return;
  wifiScanning=true;wifiScanButton.disabled=true;wifiForm.querySelector('button').disabled=true;wifiReset.disabled=true;
  wifiNetworkLabel.hidden=true;wifiScanMessage.className='muted';wifiScanMessage.textContent='WLANs werden gesucht … Die Verbindung kann kurz unterbrochen werden.';
  try{
    await request('/api/wifi/scan',{method:'POST',headers:{'X-Requested-With':'HeatingUI'}});
    let result;
    for(let attempt=0;attempt<25;attempt++){
      await new Promise(resolve=>setTimeout(resolve,800));
      result=await request('/api/wifi/scan');
      if(result.state!=='scanning')break;
    }
    if(!result||result.state==='scanning')throw new Error('Die Suche dauert zu lange. Bitte erneut versuchen.');
    if(result.state!=='complete')throw new Error('WLAN-Suche fehlgeschlagen. Bitte erneut versuchen.');
    wifiNetworks.replaceChildren(new Option('WLAN auswählen …',''));
    const networks=new Map();
    for(const network of result.networks){
      const previous=networks.get(network.ssid);
      if(!previous||network.rssi>previous.rssi)networks.set(network.ssid,network);
    }
    for(const network of [...networks.values()].sort((a,b)=>b.rssi-a.rssi)){
      wifiNetworks.add(new Option(network.ssid+' · '+network.rssi+' dBm · '+(network.open?'offen':'geschützt'),network.ssid));
    }
    wifiNetworkLabel.hidden=networks.size===0;
    wifiScanMessage.textContent=networks.size?networks.size+' WLAN(s) gefunden. Auswählen oder den Namen unten eingeben.':'Keine sichtbaren WLANs gefunden. Erneut suchen oder Namen von Hand eingeben.';
  }catch(error){wifiScanMessage.className='error';wifiScanMessage.textContent='Suche nicht abgeschlossen: '+error.message;}
  finally{wifiScanning=false;wifiScanButton.disabled=false;wifiForm.querySelector('button').disabled=false;wifiReset.disabled=false;}
});
function updateWifiSignalBars(wifi){
  const connected=wifi&&wifi.state==='connected'&&Number.isFinite(wifi.rssi);
  const bars=connected?(wifi.rssi>=-60?5:wifi.rssi>=-67?4:wifi.rssi>=-75?3:wifi.rssi>=-85?2:1):0;
  const icon=document.getElementById('wifi-signal-bars');
  icon.querySelectorAll('rect').forEach((bar,index)=>bar.classList.toggle('active',index<bars));
  icon.setAttribute('aria-label',connected?'WLAN-Signalstärke: '+bars+' von 5 Balken':'WLAN-Signalstärke nicht verfügbar');
}
function updateWifi(wifi){
  updateWifiSignalBars(wifi);
  const states={unconfigured:'Noch kein WLAN gespeichert.',queued:'WLAN-Wechsel wird vorbereitet …',starting:'Verbindung wird vorbereitet …',connecting:'Verbindungsaufbau mit '+wifi.ssid+' …',connected:'Verbunden mit '+wifi.ssid,retry:'Keine WLAN-Verbindung. Name, Passwort und Empfang prüfen. Automatischer Wiederholungsversuch nach 30 Sekunden.'};
  const status=document.getElementById('wifi-status');
  const signal=wifi.state==='connected'&&Number.isFinite(wifi.rssi)?wifi.rssi+' dBm':null;
  status.textContent=(states[wifi.state]||'WLAN-Status unbekannt')+(signal?' · Signal: '+signal:'');
  document.getElementById('wifi-signal').textContent=signal?'Heim-WLAN: '+signal:'Heim-WLAN-Signal: nicht verfügbar';
  status.className=wifi.state==='retry'?'error':'';
  document.getElementById('wifi-ap-description').textContent=wifi.pairing?'Kopplungszugang für eine Relaiserweiterung aktiv.':wifi.apRecoveryOnly?'ESP32-Zugang nur bei längerem WLAN-Ausfall.':'2,4-GHz-WLAN nutzen. Der ESP32-Zugang bleibt erreichbar.';
  document.getElementById('wifi-address').textContent=(wifi.ip?'Im Heimnetz: http://'+wifi.ip+' · ':'')+(wifi.apIp?(wifi.pairing?'Kopplungszugang':'ESP32-Zugang')+': http://'+wifi.apIp:'ESP32-Zugang: ausgeschaltet');
  if(wifiPending&&wifi.changeId===wifiPending.id){
    if(wifiPending.revision===wifiRevision){
      wifiForm.elements.password.value='';
      if(wifiPending.reset){wifiForm.elements.ssid.value='';wifiDirty=false;}
    }
    wifiMessage.className='message';
    if(wifiPending.reset&&wifi.state==='unconfigured'){
      wifiMessage.textContent='Gespeichertes WLAN wurde vergessen.';wifiPending=null;
    }else if(!wifiPending.reset&&wifi.state==='connected'){
      wifiMessage.textContent='WLAN gespeichert und verbunden. Heimnetz: http://'+wifi.ip;wifiPending=null;
    }else if(!wifiPending.reset&&wifi.state==='retry'){
      wifiMessage.className='message error';wifiMessage.textContent='WLAN gespeichert, aber Verbindung fehlgeschlagen. Name, Passwort und Empfang prüfen.';wifiPending=null;
    }else{
      wifiMessage.textContent=wifiPending.reset?'WLAN wird getrennt …':'WLAN gespeichert. Verbindung wird aufgebaut …';
    }
  }
  if(!wifiDirty&&!wifiForm.contains(document.activeElement))wifiForm.elements.ssid.value=wifi.ssid;
}
function updateRoof(roof,wifi){
  roofBadge.textContent=roof.online?'Verbunden':roof.seen?'Keine Verbindung':'Noch kein Lebenszeichen';
  roofBadge.className='badge'+(roof.online?' on':!roof.seen?' waiting':'');
  roofDetail.textContent=wifi.state!=='connected'
    ?'Der Haupt-ESP ist nicht mit dem Heimnetz verbunden.'
    :roof.seen?'Letztes Lebenszeichen vor '+roof.ageSeconds+' Sekunden.':'Der Dach-ESP muss im selben Heimnetz sein und Lebenszeichen senden.';
}
async function changeWifi(reset){
  if(wifiBusy||wifiScanning)return;
  if(reset&&!confirm('Gespeichertes WLAN vergessen? Über den ESP32-WLAN-Zugang kannst du es erneut einrichten.'))return;
  wifiBusy=true;wifiForm.querySelector('button').disabled=true;wifiReset.disabled=true;wifiScanButton.disabled=true;
  wifiMessage.className='message';wifiMessage.textContent='Wird gespeichert …';
  const id=Date.now().toString(36)+'-'+Math.random().toString(36).slice(2);
  wifiPending={id,reset,revision:wifiRevision};
  try{
    const body=reset?new URLSearchParams({changeId:id}):new URLSearchParams({ssid:wifiForm.elements.ssid.value,password:wifiForm.elements.password.value,changeId:id});
    await request(reset?'/api/wifi/reset':'/api/wifi',{method:'POST',headers:{'Content-Type':'application/x-www-form-urlencoded','X-Requested-With':'HeatingUI'},body});
    if(wifiPending&&wifiPending.id===id){
      if(wifiPending.revision===wifiRevision)wifiForm.elements.password.value='';
      wifiMessage.textContent=reset?'WLAN wird getrennt …':'WLAN gespeichert. Verbindung wird aufgebaut …';
    }
  }catch(error){
    if(wifiPending&&wifiPending.id===id){
      if(error.status){wifiPending=null;wifiMessage.className='message error';wifiMessage.textContent='WLAN-Änderung abgelehnt: '+error.message;}
      else{wifiMessage.className='message';wifiMessage.textContent='Rückmeldung unterbrochen. Status wird erneut geprüft. Falls nötig, wieder mit dem ESP32-WLAN verbinden und http://192.168.4.1 öffnen.';}
    }
  }
  finally{wifiBusy=false;wifiForm.querySelector('button').disabled=false;wifiReset.disabled=false;wifiScanButton.disabled=false;}
}
wifiForm.addEventListener('submit',event=>{event.preventDefault();changeWifi(false);});
wifiReset.addEventListener('click',()=>changeWifi(true));
const passwordForm=document.getElementById('password-form'),passwordMessage=document.getElementById('password-message');
function setupAccessPasswordVisibility(){
  passwordForm.querySelectorAll('.password-visibility').forEach(button=>{
    const input=document.getElementById(button.getAttribute('aria-controls'));
    button.addEventListener('click',()=>{
      const visible=input.type==='password';
      input.type=visible?'text':'password';
      button.setAttribute('aria-pressed',String(visible));
      button.title=visible?'Passwort verbergen':'Passwort anzeigen';
    });
  });
}
setupAccessPasswordVisibility();
async function saveAccessPassword(password){
  passwordMessage.className='message';passwordMessage.textContent='Einstellung wird gespeichert …';
  passwordForm.querySelectorAll('input,button').forEach(e=>e.disabled=true);
  document.getElementById('password-remove').disabled=true;
  try{
    await request('/auth/password',{method:'POST',headers:{'Content-Type':'application/x-www-form-urlencoded','X-Requested-With':'HeatingUI'},body:new URLSearchParams({password})});
    // A change invalidates sessions. The next request shows the login if needed.
    location.replace('/einstellungen'+(simulationEnabled?'?test=on':''));
  }catch(error){passwordMessage.className='message error';passwordMessage.textContent='Nicht gespeichert: '+error.message;}
  finally{passwordForm.querySelectorAll('input,button').forEach(e=>e.disabled=false);document.getElementById('password-remove').disabled=false;}
}
passwordForm.addEventListener('submit',event=>{
  event.preventDefault();
  const password=document.getElementById('access-password'),confirmation=document.getElementById('access-confirm');
  if(password.value!==confirmation.value){passwordMessage.className='message error';passwordMessage.textContent='Die Passwörter stimmen nicht überein.';return;}
  const value=password.value;password.value='';confirmation.value='';saveAccessPassword(value);
});
document.getElementById('password-remove').addEventListener('click',()=>{if(confirm('Passwortschutz wirklich entfernen?'))saveAccessPassword('');});
const hostnameForm=document.getElementById('hostname-form'),hostnameInput=document.getElementById('hostname-input');
const hostnameMessage=document.getElementById('hostname-message'),hostnameReset=document.getElementById('hostname-reset');
let hostnameDirty=false,hostnameBusy=false;
hostnameInput.addEventListener('input',()=>{hostnameDirty=true;hostnameMessage.textContent='';});
function updateHostname(wifi){
  if(!wifi||typeof wifi.hostname!=='string'||typeof wifi.activeHostname!=='string')return;
  if(!hostnameDirty&&!hostnameBusy&&document.activeElement!==hostnameInput)hostnameInput.value=wifi.hostname;
  const pending=wifi.hostname!==wifi.activeHostname;
  document.getElementById('hostname-active').textContent='Aktiv: '+wifi.activeHostname+
    (wifi.mdnsReady?' · mDNS: '+wifi.activeHostname+'.local':' · mDNS nicht verfügbar')+
    (pending?' · Neuer Name nach Neustart: '+wifi.hostname:'');
}
async function saveHostnameSetting(reset){
  if(hostnameBusy)return;
  const value=reset?'':hostnameInput.value.toLowerCase();
  hostnameBusy=true;hostnameInput.disabled=true;hostnameForm.querySelector('button').disabled=true;hostnameReset.disabled=true;
  hostnameMessage.className='message';hostnameMessage.textContent='Hostname wird gespeichert …';
  try{
    const result=await request('/api/hostname',{method:'POST',headers:{'Content-Type':'application/x-www-form-urlencoded','X-Requested-With':'HeatingUI'},body:new URLSearchParams({hostname:value})});
    hostnameInput.value=result.hostname;hostnameDirty=false;
    hostnameMessage.textContent=result.restartRequired?'Gespeichert. Der neue Name gilt nach dem nächsten Neustart.':'Hostname gespeichert und bereits aktiv.';
  }catch(error){hostnameMessage.className='message error';hostnameMessage.textContent='Nicht gespeichert: '+error.message;}
  finally{hostnameBusy=false;hostnameInput.disabled=false;hostnameForm.querySelector('button').disabled=false;hostnameReset.disabled=false;}
}
hostnameForm.addEventListener('submit',event=>{event.preventDefault();saveHostnameSetting(false);});
hostnameReset.addEventListener('click',()=>saveHostnameSetting(true));
let saving=false;
function updateRemoteRelay(state,simulation){
  remoteRelayState=state;
  const badge=document.getElementById('remote-relay-status');
  const detail=document.getElementById('remote-relay-detail');
  const online=!!(state&&state.online), configured=!!(state&&state.configured);
  const target=remoteRelayCards[Number(remoteUpdateBoard.value)]||state;
  const updateOnline=!!(target&&target.online);
  document.getElementById('remote-update-availability').textContent=!target?'Status der Erweiterung wird ermittelt.':!target.configured?'Bitte zuerst eine Erweiterung einrichten.':!updateOnline?'Die ausgewählte Erweiterung ist nicht erreichbar.':target.updating?'Ein Firmware-Update läuft bereits.':'';
  renderInstalledRelayRelease(target);
  badge.textContent=!state?'Status unbekannt':!state.ready?'Nicht verfügbar':!configured?'Nicht eingerichtet':online?'Verbunden':'Nicht erreichbar';
  badge.className='badge'+(online?' on':'');
  detail.textContent=!state?'Keine aktuellen Daten vom Haupt-ESP.':!state.ready?'Die Relaisanbindung konnte nicht gestartet werden.':!configured?'Relaiskarte in den Einstellungen einrichten.':state.updating?'Firmware-Update der Relaiskarte läuft.':simulation&&online?'Testmodus: Die WLAN-Relais schalten tatsächlich.':online?'Zustand von der Relaiskarte bestätigt.':'Verbindung und IP-Adresse prüfen; der tatsächliche Zustand ist unbekannt.';
  remoteRelayUpdateForm.querySelector('button').disabled=!updateOnline||remoteRelayBusy||remoteRelayUploadBusy||!!target.updating;
  remoteGithubForm.querySelector('button').disabled=!updateOnline||remoteRelayBusy||remoteRelayUploadBusy||remoteRelaySearchBusy||!!target.updating||remoteGithubForm.elements.release.value==='';
  if(pendingRemoteAction&&state&&!state.busy&&!remoteRelayBusy){
    const action=pendingRemoteAction, actual=remoteRelayCards[action.board]||state;
    if(actual&&!actual.busy){
      pendingRemoteAction=null;
      remoteRelayMessage.textContent=!actual.online?'Keine Rückmeldung zum Schaltauftrag. Zustand prüfen.':
        !!(actual.mask&(1<<(action.id-1)))===action.on?'Relaiszustand bestätigt.':'Gewünschter Zustand nicht bestätigt. Relaiserweiterung prüfen.';
    }
  }
  for(const button of document.querySelectorAll('#remote-relay-switches button')){
    const index=Number(button.dataset.relay)-1;
    const on=online&&!!(state.mask&(1<<index));
    button.disabled=!online||remoteRelayBusy||!!state.busy||!!state.updating;
    button.setAttribute('aria-checked',String(on));
    button.dataset.state=online?(on?'on':'off'):'unknown';
    button.querySelector('span:last-child').textContent=(state&&state.labels&&state.labels[index]||'Relais '+(index+1))+(online?(on?' · EIN':' · AUS'):' · unbekannt');
  }
  const selected=remoteRelayCards[Number(remoteRelayBoard.value)]||state;
  if(remoteRelayBoard.options)for(let i=0;i<remoteRelayBoard.options.length;i++){
    const card=remoteRelayCards[i];
    remoteRelayBoard.options[i].textContent='Relaiserweiterung '+(i+1)+(card?(card.configured?(card.online?' · verbunden':' · nicht erreichbar'):' · frei'):'');
  }
  remoteRelaySlotStatus.textContent=!selected?'Status wird geladen …':!selected.configured?'Freier Platz · Neue Erweiterung hinzufügen.':selected.online?'Verbunden · '+selected.address+(selected.version?' · Firmware '+selected.version:' · Firmware-Version nicht verfügbar'):'Nicht erreichbar · '+selected.address;
  remoteRelayForget.hidden=!selected||!selected.configured;
  remoteRelayForget.disabled=!selected||!selected.online||selected.busy||selected.updating||relayPairActive||remoteRelayBusy;
  relayForgetRecovery.hidden=!selected||!selected.configured;
  remoteRelayDiscard.disabled=!selected||selected.busy||selected.updating||relayPairActive||remoteRelayBusy;
  if(selected&&selected.forget&&selected.forget!=='idle'){
    const messages={pending:'Erweiterung wird zurückgesetzt …',done:'Erweiterung zurückgesetzt und vergessen.',failed:'Zurücksetzen abgelehnt. Verbindung und Firmware der Erweiterung prüfen; sie bleibt gespeichert.',uncertain:'Rücksetzung nicht bestätigt. Die Erweiterung könnte bereits zurückgesetzt sein; Zuordnung und Karte prüfen.',save_failed:'Erweiterung zurückgesetzt, aber die Zuordnung konnte nicht gelöscht werden. Speicher am Haupt-ESP prüfen.'};
    remoteRelayForgetMessage.textContent=messages[selected.forget]||'';
  }
  if(selected&&!remoteRelayDirty&&!remoteRelayBusy&&!remoteRelayForm.contains(document.activeElement)){
    remoteRelayForm.elements.address.value=selected.address||'';
    for(let i=0;i<4;i++)remoteRelayForm.elements['label'+(i+1)].value=selected.labels[i];
  }
}
function updateExtraRelays(cards,simulation){
  if(!cards.length){
    for(const item of remoteExtraCards.values()){
      item.status.textContent='Keine aktuellen Daten vom Haupt-ESP.';
      for(const {button,label} of item.buttons){button.disabled=true;button.setAttribute('aria-checked','false');button.dataset.state='unknown';label.textContent=label.textContent.replace(/ · (EIN|AUS|unbekannt)$/,'')+' · unbekannt';}
    }
    return;
  }
  for(let board=1;board<cards.length;board++){
    const state=cards[board];let item=remoteExtraCards.get(board);
    if(!state.configured){if(item){item.container.remove();remoteExtraCards.delete(board);}continue;}
    if(!item){
      const container=document.createElement('div');container.className='remote-extra-card';
      const heading=document.createElement('h3');heading.textContent='Relaiserweiterung '+(board+1);container.appendChild(heading);
      const status=document.createElement('p');status.className='muted';container.appendChild(status);
      const controls=document.createElement('div');controls.className='remote-relay-switches';container.appendChild(controls);
      const buttons=[];
      for(let id=1;id<=4;id++){
        const button=document.createElement('button');button.type='button';button.className='relay-switch';button.dataset.board=String(board);button.dataset.relay=String(id);button.setAttribute('role','switch');
        const track=document.createElement('span');track.className='switch-track';track.setAttribute('aria-hidden','true');
        const label=document.createElement('span');button.append(track,label);controls.appendChild(button);buttons.push({button,label});
      }
      remoteExtraGroups.appendChild(container);item={container,status,buttons};remoteExtraCards.set(board,item);
    }
    item.status.textContent=state.online?'Verbunden':'Nicht erreichbar · Zustand unbekannt';
    for(let index=0;index<4;index++){
      const {button,label}=item.buttons[index],on=state.online&&!!(state.mask&(1<<index));
      button.disabled=!state.online||remoteRelayBusy||!!state.busy||!!state.updating;
      button.setAttribute('aria-checked',String(on));button.dataset.state=state.online?(on?'on':'off'):'unknown';
      label.textContent=(state.labels[index]||'Relais '+(index+1))+(state.online?(on?' · EIN':' · AUS'):' · unbekannt');
    }
  }
}
function updateRelayDiscovery(discovery){
  const devices=discovery&&Array.isArray(discovery.devices)?discovery.devices:[];
  const signature=JSON.stringify(devices);
  if(signature!==relayDiscoverySignature){
    relayDiscoverySignature=signature;const previous=relayDiscoverySelect.value;
    relayDiscoverySelect.replaceChildren(new Option('Erweiterung auswählen …',''));
    devices.forEach(device=>relayDiscoverySelect.add(new Option(device.name+' · '+device.address,device.address)));
    if(devices.some(device=>device.address===previous))relayDiscoverySelect.value=previous;
  }
  relayDiscoverySearch.disabled=!discovery||!discovery.ready||!discovery.available||discovery.searching;
  relayDiscoveryMessage.textContent=!discovery||!discovery.ready?'Suche nicht verfügbar.':!discovery.available?'Heim-WLAN ist nicht verbunden.':discovery.searching?'Erweiterungen werden gesucht …':discovery.failed?'Suche fehlgeschlagen; bitte erneut versuchen.':devices.length?devices.length+' Erweiterung(en) im Heimnetz gefunden.':'Keine Erweiterung im Heimnetz gefunden.';
}
function updateRelayProvisioning(state){
  const active=!!(state&&['waiting','claimed','searching','verifying'].includes(state.state));
  relayPairActive=active;
  const selected=remoteRelayCards[Number(remoteRelayBoard.value)];
  relayPairMessage.textContent=!state||!state.ready?'Einrichtung nicht verfügbar.':!active&&selected&&selected.configured?'Für eine weitere Erweiterung einen freien Platz auswählen.':state.message||'';
  relayPairForm.querySelector('button').disabled=!state||!state.ready||active||relayPairScanBusy||!relayPairForm.elements.ssid.value||!!(selected&&selected.configured);
  relayPairScan.disabled=!state||!state.ready||active||relayPairScanBusy;
  relayPairCancel.hidden=!active;
  remoteRelayBoard.disabled=active;
}
relayPairForm.elements.ssid.addEventListener('change',()=>{
  const selected=remoteRelayCards[Number(remoteRelayBoard.value)];
  relayPairForm.querySelector('button').disabled=relayPairActive||!relayPairForm.elements.ssid.value||!!(selected&&selected.configured);
});
relayPairScan.addEventListener('click',async()=>{
  if(relayPairScanBusy||wifiBusy||wifiScanning||relayPairActive)return;
  relayPairScanBusy=true;wifiScanning=true;relayPairScan.disabled=true;relayPairForm.querySelector('button').disabled=true;
  relayPairScanMessage.textContent='Einrichtungs-WLANs werden gesucht …';
  try{
    await request('/api/wifi/scan',{method:'POST',headers:{'X-Requested-With':'HeatingUI'}});
    let result;
    for(let i=0;i<24;i++){
      await new Promise(resolve=>setTimeout(resolve,500));result=await request('/api/remote-relay/pair/scan');
      if(result.state!=='scanning')break;
    }
    if(!result||result.state!=='complete')throw new Error('WLAN-Suche nicht abgeschlossen.');
    const names=[...new Set(result.networks.filter(n=>/^Relaiserweiterung-[0-9A-F]{4}$/.test(n.ssid)&&!n.open).map(n=>n.ssid))];
    const select=relayPairForm.elements.ssid;select.replaceChildren(new Option(names.length?'Erweiterung auswählen …':'Keine Erweiterung gefunden',''));
    names.forEach(name=>select.add(new Option(name,name)));select.disabled=!names.length;
    relayPairScanMessage.textContent=names.length?names.length+' Einrichtungs-WLAN(s) gefunden.':'Keine neue Erweiterung gefunden. Stromversorgung und Firmware prüfen.';
  }catch(error){relayPairScanMessage.textContent='Suche fehlgeschlagen: '+error.message;}
  finally{relayPairScanBusy=false;wifiScanning=false;relayPairScan.disabled=false;}
});
relayPairForm.addEventListener('submit',async event=>{
  event.preventDefault();if(relayPairActive)return;
  if(remoteRelayDirty&&!confirm('Ungespeicherte Änderungen verwerfen und die Erweiterung einrichten?'))return;
  remoteRelayDirty=false;relayPairActive=true;remoteRelayBoard.disabled=true;relayPairScan.disabled=true;
  relayPairForm.querySelector('button').disabled=true;relayPairMessage.textContent='Einrichtung wird gestartet …';
  try{
    await request('/api/remote-relay/pair',{method:'POST',headers:{'X-Requested-With':'HeatingUI','Content-Type':'application/x-www-form-urlencoded'},body:new URLSearchParams({board:remoteRelayBoard.value,ssid:relayPairForm.elements.ssid.value})});
    relayPairMessage.textContent='IO0 an der Relaiserweiterung eine Sekunde drücken.';
  }catch(error){
    if(error.status){relayPairActive=false;remoteRelayBoard.disabled=false;relayPairScan.disabled=false;relayPairMessage.textContent='Einrichtung nicht gestartet: '+error.message;}
    else relayPairMessage.textContent='Start nicht bestätigt. Der Einrichtungsstatus wird geprüft.';
  }
});
relayPairCancel.addEventListener('click',async()=>{
  relayPairCancel.disabled=true;
  try{await request('/api/remote-relay/pair/cancel',{method:'POST',headers:{'X-Requested-With':'HeatingUI'}});relayPairMessage.textContent='Einrichtung wurde abgebrochen.';}
  catch(error){relayPairMessage.textContent='Abbruch nicht bestätigt: '+error.message;}
  finally{relayPairCancel.disabled=false;}
});
remoteRelayBoard.addEventListener('change',()=>{
  if(remoteRelayDirty&&!confirm('Ungespeicherte Änderungen verwerfen?')){remoteRelayBoard.value=remoteRelayBoard.dataset.previous||'0';return;}
  remoteRelayBoard.dataset.previous=remoteRelayBoard.value;remoteRelayDirty=false;remoteRelayEditRevision++;remoteRelayForm.elements.key.value='';remoteRelayForgetMessage.textContent='';
  showRemoteRelayConfigMessage('');
  updateRemoteRelay(remoteRelayState,simulationEnabled);
  const selected=remoteRelayCards[Number(remoteRelayBoard.value)];
  relayPairForm.querySelector('button').disabled=relayPairActive||!relayPairForm.elements.ssid.value||!!(selected&&selected.configured);
});
remoteUpdateBoard.addEventListener('change',()=>updateRemoteRelay(remoteRelayState,simulationEnabled));
relayDiscoverySelect.addEventListener('change',()=>{
  if(!relayDiscoverySelect.value)return;
  remoteRelayForm.elements.address.value=relayDiscoverySelect.value;remoteRelayForm.elements.key.value='';remoteRelayDirty=true;remoteRelayEditRevision++;
  showRemoteRelayConfigMessage('Bitte den Verbindungsschlüssel der ausgewählten Erweiterung eingeben.');
});
relayDiscoverySearch.addEventListener('click',async()=>{
  relayDiscoverySearch.disabled=true;
  try{await request('/api/remote-relay/discover',{method:'POST',headers:{'X-Requested-With':'HeatingUI'}});relayDiscoveryMessage.textContent='Erweiterungen werden gesucht …';}
  catch(error){relayDiscoveryMessage.textContent='Suche nicht gestartet: '+error.message;}
});
let remoteRelayEditRevision=0,remoteRelayConfigTimer=0;
function showRemoteRelayConfigMessage(message,clearAfter=0){
  if(remoteRelayConfigTimer)clearTimeout(remoteRelayConfigTimer);
  remoteRelayConfigTimer=0;remoteRelayConfigMessage.textContent=message;
  if(clearAfter)remoteRelayConfigTimer=setTimeout(()=>{remoteRelayConfigMessage.textContent='';remoteRelayConfigTimer=0;},clearAfter);
}
remoteRelayForm.addEventListener('input',()=>{remoteRelayDirty=true;remoteRelayEditRevision++;showRemoteRelayConfigMessage('');});
remoteRelayForm.addEventListener('submit',async event=>{
  event.preventDefault();if(remoteRelayBusy)return;
  const board=remoteRelayBoard.value,editRevision=remoteRelayEditRevision;
  remoteRelayBusy=true;remoteRelayRevision++;showRemoteRelayConfigMessage('Wird gespeichert …');
  const values=new URLSearchParams();
  for(const name of ['address','key','label1','label2','label3','label4'])values.set(name,remoteRelayForm.elements[name].value.trim());
  values.set('board',board);
  try{
    await request('/api/remote-relay/config',{method:'POST',headers:{'Content-Type':'application/x-www-form-urlencoded','X-Requested-With':'HeatingUI'},body:values});
    if(remoteRelayBoard.value===board){
      if(remoteRelayEditRevision===editRevision){remoteRelayDirty=false;remoteRelayForm.elements.key.value='';showRemoteRelayConfigMessage('Gespeichert.',4000);}
      else showRemoteRelayConfigMessage('Gespeichert. Weitere Änderungen sind noch nicht gespeichert.');
    }
  }catch(error){if(remoteRelayBoard.value===board)showRemoteRelayConfigMessage(error.status?'Nicht gespeichert: '+error.message:'Speichern nicht bestätigt. Bitte Verbindung und Einstellungen prüfen.');}
  finally{remoteRelayBusy=false;}
});
remoteRelayForget.addEventListener('click',async()=>{
  const board=Number(remoteRelayBoard.value),state=remoteRelayCards[board];
  if(!state||!state.configured||!state.online||state.busy||remoteRelayBusy||relayPairActive)return;
  if(!confirm('Erweiterung zurücksetzen und vergessen? Alle vier Relais werden ausgeschaltet; WLAN-Daten und gespeicherte Schaltzustände werden gelöscht.'))return;
  remoteRelayBusy=true;remoteRelayRevision++;remoteRelayForget.disabled=true;remoteRelayForgetMessage.textContent='Erweiterung wird zurückgesetzt …';
  try{await request('/api/remote-relay/forget',{method:'POST',headers:{'Content-Type':'application/x-www-form-urlencoded','X-Requested-With':'HeatingUI'},body:new URLSearchParams({board:String(board)})});}
  catch(error){remoteRelayForgetMessage.textContent=error.status?'Zurücksetzen nicht gestartet: '+error.message:'Rücksetzung nicht bestätigt. Karte und Status prüfen.';}
  finally{remoteRelayBusy=false;}
});
remoteRelayDiscard.addEventListener('click',async()=>{
  const state=remoteRelayCards[Number(remoteRelayBoard.value)];
  if(!state||!state.configured||state.busy||remoteRelayBusy||relayPairActive)return;
  if(!confirm('Nur die Zuordnung entfernen? Die Karte wird dabei nicht zurückgesetzt und kann weiterhin WLAN-Zugangsdaten und Schaltzustände speichern.'))return;
  remoteRelayBusy=true;remoteRelayRevision++;remoteRelayDiscard.disabled=true;
  try{await request('/api/remote-relay/discard',{method:'POST',headers:{'Content-Type':'application/x-www-form-urlencoded','X-Requested-With':'HeatingUI'},body:new URLSearchParams({board:remoteRelayBoard.value})});remoteRelayDirty=false;remoteRelayForgetMessage.textContent='Zuordnung entfernt. Die Karte wurde nicht zurückgesetzt.';}
  catch(error){remoteRelayForgetMessage.textContent='Zuordnung nicht entfernt: '+error.message;}
  finally{remoteRelayBusy=false;}
});
async function switchRemoteRelay(event){
  const button=event.target.closest('button[data-relay]');
  const board=button&&button.dataset.board?Number(button.dataset.board):0;
  const state=remoteRelayCards[board]||remoteRelayState;
  if(!button||button.disabled||remoteRelayBusy||!state||!state.online)return;
  const id=Number(button.dataset.relay),on=!(state.mask&(1<<(id-1)));
  remoteRelayBusy=true;remoteRelayRevision++;remoteRelayMessage.textContent='Schaltauftrag wird gesendet …';
  updateRemoteRelay(remoteRelayState,false);updateExtraRelays(remoteRelayCards,false);
  try{
    await request('/api/remote-relay/command',{method:'POST',headers:{'Content-Type':'application/x-www-form-urlencoded','X-Requested-With':'HeatingUI'},body:new URLSearchParams({board:String(board),id:String(id),value:on?'1':'0'})});
    pendingRemoteAction={board,id,on};remoteRelayMessage.textContent='Auftrag angenommen. Warte auf Rückmeldung der Relaiserweiterung.';
  }catch(error){remoteRelayMessage.textContent='Schaltauftrag nicht bestätigt: '+error.message;}
  finally{remoteRelayBusy=false;}
}
document.getElementById('remote-relay-switches').addEventListener('click',switchRemoteRelay);
remoteExtraGroups.addEventListener('click',switchRemoteRelay);
let installedReleaseCatalog=null,installedReleaseLoadState='loading';
function releaseNotesText(release){
  const body=release&&typeof release.body==='string'?release.body:'';
  const section=/^#{2,3}[ \t]+Haupt-ESP[ \t]*\r?\n([\s\S]*?)(?=^#{1,3}[ \t]+|(?![\s\S]))/im.exec(body);
  return section&&section[1].trim()?section[1].trim().slice(0,12000):'Für den Haupt-ESP sind keine eigenen Release Notes vorhanden.';
}
function relayReleaseNotesText(release){
  const body=typeof release.body==='string'?release.body:'';
  const section=/^#{2,3}[ \t]+Relaiskarte[ \t]*\r?\n([\s\S]*?)(?=^#{1,3}[ \t]+|(?![\s\S]))/im.exec(body);
  if(section&&section[1].trim())return section[1].trim().slice(0,12000);
  // Historische Releases hatten noch keinen eigenen Abschnitt für die Relaiskarte.
  const known={
    '2.7.0':'Die Relaiskarte meldet ihre Firmware-Version.',
    '2.7.1':'Die WLAN-Verbindung der Relaiskarte wurde verbessert.',
    '2.7.2':'Funktional unverändert zur Vorversion; die Versionsnummer wurde aktualisiert.',
    '2.8.0':'Funktional unverändert zur Vorversion; die Versionsnummer wurde aktualisiert.',
    '2.9.0':'Funktional unverändert zur Vorversion; die Versionsnummer wurde aktualisiert.'
  };
  const version=(release.tag_name||release.tag||'').replace(/^v/i,'');
  return known[version]||'Für die Relaiskarte sind keine eigenen Release Notes vorhanden.';
}
function installedReleaseText(version,relay=false){
  if(!version)return 'Die installierte Version ist nicht bekannt.';
  if(installedReleaseLoadState==='loading')return 'Release Notes werden geladen …';
  if(installedReleaseLoadState==='error')return 'Release Notes nicht verfügbar. Internetverbindung zu GitHub prüfen.';
  const release=installedReleaseCatalog.find(item=>item.tag_name.replace(/^v/i,'')===version.replace(/^v/i,''));
  return release?(relay?relayReleaseNotesText(release):releaseNotesText(release)):'Für diese Version wurden keine Release Notes auf GitHub gefunden.';
}
function renderInstalledMainRelease(){
  document.getElementById('main-installed-version').textContent=currentFirmwareVersion?'Installierte Firmware: '+currentFirmwareVersion:'Installierte Firmware wird ermittelt …';
  document.getElementById('main-installed-notes').textContent=installedReleaseText(currentFirmwareVersion);
}
function renderInstalledRelayRelease(target){
  const version=target&&target.online&&target.version||'';
  document.getElementById('relay-installed-version').textContent=!target||!target.configured?'Keine Erweiterung ausgewählt.':!target.online?'Relaiserweiterung nicht erreichbar.':version?'Installierte Firmware: '+version:'Firmware-Version nicht verfügbar; ältere Versionen melden sie nicht.';
  document.getElementById('relay-installed-notes').textContent=version?installedReleaseText(version,true):'Ohne gemeldete Firmware-Version lassen sich keine Release Notes zuordnen.';
}
function renderSelectedReleaseNotes(prefix,release){
  const container=document.getElementById(prefix+'-selected-notes-wrap');
  container.hidden=!release;
  document.getElementById(prefix+'-selected-notes').textContent=release?(prefix==='relay'?relayReleaseNotesText(release):releaseNotesText(release)):'';
}
async function loadInstalledReleaseNotes(){
  installedReleaseLoadState='loading';
  const abort=new AbortController();const timeout=setTimeout(()=>abort.abort(),10000);
  try{
    const response=await fetch('https://api.github.com/repos/NemoN/Solarsystemsteuerung_ESP32/releases?per_page=100',{cache:'no-store',signal:abort.signal,headers:{Accept:'application/vnd.github+json'}});
    if(!response.ok)throw new Error('GitHub '+response.status);
    const releases=await response.json();if(!Array.isArray(releases))throw new Error('Ungültige Antwort');
    installedReleaseCatalog=releases.filter(r=>!r.draft&&typeof r.tag_name==='string');installedReleaseLoadState='ready';
  }catch(error){installedReleaseCatalog=[];installedReleaseLoadState='error';}
  finally{clearTimeout(timeout);}
  renderInstalledMainRelease();renderInstalledRelayRelease(remoteRelayCards[Number(remoteUpdateBoard.value)]||remoteRelayState);
}
async function fetchRemoteRelayReleases(fetcher){
  const response=await fetcher('https://api.github.com/repos/NemoN/Solarsystemsteuerung_ESP32/releases?per_page=100',{cache:'no-store',headers:{Accept:'application/vnd.github+json'}});
  if(!response.ok)throw new Error('GitHub meldet HTTP '+response.status);
  const releases=await response.json();if(!Array.isArray(releases))throw new Error('Ungültige GitHub-Antwort');
  return releases.filter(r=>!r.draft&&Array.isArray(r.assets)&&typeof r.tag_name==='string'&&/^[A-Za-z0-9._-]+$/.test(r.tag_name)).map(r=>{
    const asset=r.assets.find(a=>a.name==='Relaiskarte.ino.bin'&&typeof a.digest==='string'&&/^sha256:[0-9a-f]{64}$/.test(a.digest)&&a.size>=1024&&a.size<=1310720&&
      a.browser_download_url==='https://github.com/NemoN/Solarsystemsteuerung_ESP32/releases/download/'+r.tag_name+'/Relaiskarte.ino.bin');
    return {tag:r.tag_name,name:r.name||r.tag_name,date:r.published_at||'',prerelease:r.prerelease,asset,body:typeof r.body==='string'?r.body:''};
  }).filter(r=>r.asset).sort((a,b)=>b.date.localeCompare(a.date)).slice(0,8);
}
remoteReleaseCheck.addEventListener('click',async()=>{
  if(remoteRelaySearchBusy)return;
  remoteRelaySearchBusy=true;remoteReleaseCheck.disabled=true;remoteGithubForm.elements.release.disabled=true;remoteGithubForm.querySelector('button').disabled=true;
  remoteRelayReleases=[];renderSelectedReleaseNotes('relay',null);remoteReleaseMessage.className='message';remoteReleaseMessage.textContent='GitHub-Releases werden gesucht …';
  try{
    remoteRelayReleases=await fetchRemoteRelayReleases(fetch);
    if(installedReleaseLoadState==='error')loadInstalledReleaseNotes();
    const select=remoteGithubForm.elements.release;
    select.replaceChildren(new Option(remoteRelayReleases.length?'Version auswählen …':'Keine Releases mit Relaiskarten-Firmware',''));
    remoteRelayReleases.forEach((r,i)=>select.add(new Option(r.name+(r.prerelease?' · Vorabversion':''),String(i))));
    select.disabled=!remoteRelayReleases.length;
    remoteReleaseMessage.textContent=remoteRelayReleases.length?'Relaiskarten-Firmware gefunden.':'Noch kein Release mit Relaiskarte.ino.bin und SHA-256-Prüfsumme vorhanden.';
  }catch(error){remoteReleaseMessage.className='message error';remoteReleaseMessage.textContent='GitHub nicht erreichbar: '+error.message;}
  finally{remoteRelaySearchBusy=false;remoteReleaseCheck.disabled=false;}
});
remoteGithubForm.elements.release.addEventListener('change',()=>{const selected=remoteGithubForm.elements.release.value;renderSelectedReleaseNotes('relay',selected===''?null:remoteRelayReleases[Number(selected)]);updateRemoteRelay(remoteRelayState,simulationEnabled);});
async function pollRemoteRelayGithubUpdate(post,token,progress,message){
  let disconnectedAt=null;
  for(;;){
    await new Promise(resolve=>setTimeout(resolve,1000));
    let status;
    try{status=await post('status',{token});disconnectedAt=null;}
    catch(error){
      if(disconnectedAt===null)disconnectedAt=Date.now();
      if((error.status&&error.status<500)||Date.now()-disconnectedAt>45000){const uncertain=new Error('Update-Status nicht erreichbar. Relaiskarte prüfen.');uncertain.updateUncertain=true;throw uncertain;}
      message.textContent='Verbindung unterbrochen. Update-Status wird erneut abgefragt …';continue;
    }
    if(status.state==='done'){progress.value=100;message.textContent='Firmware aktualisiert. Neustart und Startprüfung der Relaiskarte bestätigt.';return;}
    if(status.state==='failed'||status.state==='canceled')throw new Error(status.error||'Update nicht abgeschlossen');
    if(status.state==='unknown'){const uncertain=new Error(status.error||'Neustart nicht bestätigt.');uncertain.updateUncertain=true;throw uncertain;}
    const downloading=status.state==='downloading';
    progress.value=status.size?Math.round((downloading?status.received:status.sent)*100/status.size):0;
    message.textContent=status.state==='verifying'?'Relaiskarte startet neu; Startprüfung läuft …':
      downloading?'Firmware wird von GitHub geladen: '+progress.value+' %':'Zur Relaiskarte übertragen: '+progress.value+' %';
  }
}
remoteGithubForm.addEventListener('submit',async event=>{
  event.preventDefault();
  const selected=remoteGithubForm.elements.release.value,release=selected===''?null:remoteRelayReleases[Number(selected)];
  const target=remoteRelayCards[Number(remoteUpdateBoard.value)]||remoteRelayState;
  if(!release||remoteRelayUploadBusy||!target||!target.online)return;
  if(!confirm('Relaiskarten-Firmware aus Release '+release.tag+' installieren? Beim Neustart sind die Relais kurz aus und stellen danach ihre gespeicherten Zustände wieder her.'))return;
  remoteRelayUploadBusy=true;remoteGithubForm.querySelector('button').disabled=true;remoteRelayUpdateForm.querySelector('button').disabled=true;
  remoteRelayUpdateProgress.hidden=false;remoteRelayUpdateProgress.value=0;remoteRelayUpdateMessage.className='message';remoteRelayUpdateMessage.textContent='GitHub-Download wird gestartet …';
  const post=(path,values)=>request('/api/remote-relay/update/'+path,{method:'POST',headers:{'X-Requested-With':'HeatingUI','Content-Type':'application/x-www-form-urlencoded'},body:new URLSearchParams(values)});
  let started=false;
  try{
    const result=await post('github',{url:release.asset.browser_download_url,size:String(release.asset.size),sha256:release.asset.digest.slice(7),board:remoteUpdateBoard.value});
    started=true;await pollRemoteRelayGithubUpdate(post,result.token,remoteRelayUpdateProgress,remoteRelayUpdateMessage);
  }catch(error){
    remoteRelayUpdateMessage.className='message error';
    if(error.updateUncertain||(!started&&(!error.status||error.status>=500)))remoteRelayUpdateMessage.textContent='Update-Status unklar: Bitte die Relaiskarte und ihre Firmware prüfen.';
    else remoteRelayUpdateMessage.textContent='Update nicht abgeschlossen: '+(error.message||'Verbindung unterbrochen');
  }finally{remoteRelayUploadBusy=false;}
});
remoteRelayUpdateForm.addEventListener('submit',async event=>{
  event.preventDefault();
  const file=remoteRelayUpdateForm.elements.firmware.files[0];
  if(!file||remoteRelayUploadBusy)return;
  if(!confirm('Firmware der Relaiskarte aktualisieren? Beim Neustart sind die Relais kurz aus und stellen danach ihre gespeicherten Zustände wieder her.'))return;
  remoteRelayUploadBusy=true;remoteRelayUpdateForm.querySelector('button').disabled=true;
  remoteRelayUpdateProgress.hidden=false;remoteRelayUpdateProgress.value=0;
  remoteRelayUpdateMessage.className='message';remoteRelayUpdateMessage.textContent='Firmware wird vorbereitet …';
  const post=(path,values)=>request('/api/remote-relay/update/'+path,{method:'POST',headers:{'X-Requested-With':'HeatingUI','Content-Type':'application/x-www-form-urlencoded'},body:new URLSearchParams(values)});
  let token='',started=false,startUncertain=false;
  try{
    const prepared=await post('prepare',{size:String(file.size),board:remoteUpdateBoard.value});token=prepared.token;
    for(let offset=0;offset<file.size;offset+=4096){
      const bytes=new Uint8Array(await file.slice(offset,offset+4096).arrayBuffer());
      const data=btoa(Array.from(bytes,byte=>String.fromCharCode(byte)).join(''));
      let result;
      for(let attempt=0;attempt<3;attempt++){
        try{result=await post('chunk',{token,offset:String(offset),data});break;}
        catch(error){if(attempt===2||(error.status&&error.status<500))throw error;await new Promise(resolve=>setTimeout(resolve,400));}
      }
      if(!result||result.received!==offset+bytes.length)throw new Error('Datenblock nicht bestätigt');
      remoteRelayUpdateProgress.value=Math.round((offset+bytes.length)*100/file.size);
      remoteRelayUpdateMessage.textContent='Zum Haupt-ESP übertragen: '+remoteRelayUpdateProgress.value+' %';
    }
    remoteRelayUpdateProgress.value=0;
    try{await post('start',{token});started=true;}
    catch(error){if(error.status&&error.status<500)throw error;startUncertain=true;}
    let disconnectedAt=null,resent=false;
    for(;;){
      await new Promise(resolve=>setTimeout(resolve,1000));
      let status;
      try{status=await post('status',{token});disconnectedAt=null;}
      catch(error){
        if(disconnectedAt===null)disconnectedAt=Date.now();
        if((error.status&&error.status<500)||Date.now()-disconnectedAt>45000){const uncertain=new Error('Der Update-Status ist nicht erreichbar. Relaiskarte prüfen.');uncertain.updateUncertain=true;throw uncertain;}
        remoteRelayUpdateMessage.textContent='Verbindung unterbrochen. Update-Status wird erneut abgefragt …';continue;
      }
      if(status.state==='receiving'&&startUncertain&&!resent){
        resent=true;try{await post('start',{token});started=true;startUncertain=false;}catch(error){if(error.status&&error.status<500&&error.status!==409)throw error;}
        continue;
      }
      if(status.state==='done'){token='';remoteRelayUpdateProgress.value=100;remoteRelayUpdateMessage.textContent='Firmware aktualisiert. Neustart und Startprüfung der Relaiskarte bestätigt.';break;}
      if(status.state==='failed'||status.state==='canceled')throw new Error(status.error||'Update nicht abgeschlossen');
      if(status.state==='unknown'){const uncertain=new Error(status.error||'Neustart nicht bestätigt.');uncertain.updateUncertain=true;throw uncertain;}
      remoteRelayUpdateProgress.value=status.size?Math.round(status.sent*100/status.size):0;
      remoteRelayUpdateMessage.textContent=status.state==='verifying'?'Relaiskarte startet neu; Startprüfung läuft …':'Zur Relaiskarte übertragen: '+remoteRelayUpdateProgress.value+' %';
    }
  }catch(error){
    if(token&&!started&&!startUncertain&&!error.updateUncertain){try{await post('abort',{token});}catch(ignored){}}
    remoteRelayUpdateMessage.className='message error';
    remoteRelayUpdateMessage.textContent=(error.updateUncertain?'Update-Status unklar: ':'Update nicht abgeschlossen: ')+(error.message||'Verbindung unterbrochen');
  }finally{remoteRelayUploadBusy=false;}
});
async function request(url,options={}){
  const abort=new AbortController();
  const timer=setTimeout(()=>abort.abort(),6000);
  try{
    const response=await fetch(url,{...options,cache:'no-store',signal:abort.signal});
    if(!response.ok){const error=new Error(await response.text());error.status=response.status;throw error;}
    return await response.json();
  }finally{clearTimeout(timer);}
}
const historyServerForm=document.getElementById('history-server-form'),historyServerInput=document.getElementById('history-server-url');
const historyServerMessage=document.getElementById('history-server-message'),historyServerLink=document.getElementById('history-server-link');
function setHistoryServerLink(value){
  try{
    const parsed=new URL(value);
    if(!['http:','https:'].includes(parsed.protocol)||parsed.username||parsed.password)throw new Error('Ungültige Adresse');
    historyServerLink.href=parsed.href;historyServerLink.hidden=false;
  }catch(error){historyServerLink.hidden=true;historyServerLink.removeAttribute('href');}
}
async function loadHistoryServer(){
  try{
    const result=await request('/api/history-server');
    historyServerInput.value=result.url;setHistoryServerLink(result.url);
    historyServerInput.disabled=false;historyServerForm.querySelector('button').disabled=false;historyServerMessage.textContent='';
  }catch(error){historyServerMessage.className='message error';historyServerMessage.textContent='Adresse konnte nicht geladen werden. Bitte Seite neu laden.';}
}
historyServerForm.addEventListener('submit',async event=>{
  event.preventDefault();
  let value=historyServerInput.value.trim();
  try{
    if(value){const url=new URL(value);if(!['http:','https:'].includes(url.protocol)||url.username||url.password)throw new Error();value=url.href;}
    if(value.length>255)throw new Error();
  }catch(error){historyServerMessage.className='message error';historyServerMessage.textContent='Bitte eine gültige HTTP- oder HTTPS-Adresse ohne Zugangsdaten eingeben.';return;}
  historyServerInput.disabled=true;historyServerForm.querySelector('button').disabled=true;
  historyServerMessage.className='message';historyServerMessage.textContent='Adresse wird gespeichert …';
  try{
    const result=await request('/api/history-server',{method:'POST',headers:{'Content-Type':'application/x-www-form-urlencoded','X-Requested-With':'HeatingUI'},body:new URLSearchParams({url:value})});
    historyServerInput.value=result.url;setHistoryServerLink(result.url);historyServerMessage.textContent='Adresse gespeichert.';
  }catch(error){historyServerMessage.className='message error';historyServerMessage.textContent='Nicht gespeichert: '+error.message;}
  finally{historyServerInput.disabled=false;historyServerForm.querySelector('button').disabled=false;}
});
loadHistoryServer();
const updateForm=document.getElementById('update-form');
const githubForm=document.getElementById('github-update-form'),releaseMessage=document.getElementById('release-message');
const releaseDownload=document.getElementById('release-download'),releaseDownloadLink=document.getElementById('release-download-link');
let githubReleases=[],currentFirmwareVersion='';
function updateReleaseDownload(){
  const selected=githubForm.elements.release.value;
  const release=selected===''?null:githubReleases[Number(selected)];
  renderSelectedReleaseNotes('main',release);
  releaseDownload.hidden=!release;
  if(release)releaseDownloadLink.href='https://github.com/NemoN/Solarsystemsteuerung_ESP32/releases/download/'+encodeURIComponent(release.tag)+'/'+encodeURIComponent(release.downloadAsset?release.downloadAsset.name:'Backload.ino.bin');
  else releaseDownloadLink.removeAttribute('href');
}
githubForm.elements.release.addEventListener('change',updateReleaseDownload);
function compareVersions(a,b){
  const parse=s=>/^v?(\d+)\.(\d+)\.(\d+)(?:-(?:mit|ohne)-ap)?$/i.exec(s||'');
  const x=parse(a),y=parse(b);if(!x||!y)return null;
  for(let i=1;i<=3;i++){const d=Number(x[i])-Number(y[i]);if(d)return Math.sign(d);}return 0;
}
document.getElementById('release-check').addEventListener('click',async event=>{
  const button=event.currentTarget,select=githubForm.elements.release;
  button.disabled=true;select.disabled=true;githubForm.querySelector('button').disabled=true;githubReleases=[];updateReleaseDownload();releaseMessage.className='message';releaseMessage.textContent='GitHub-Releases werden gesucht …';
  try{
    const device=await request('/api/status');currentFirmwareVersion=device.version||'';
    const response=await fetch('https://api.github.com/repos/NemoN/Solarsystemsteuerung_ESP32/releases?per_page=100',{cache:'no-store',headers:{Accept:'application/vnd.github+json'}});
    if(!response.ok)throw new Error('GitHub meldet HTTP '+response.status);
    const releases=await response.json();if(!Array.isArray(releases))throw new Error('Ungültige GitHub-Antwort');
    if(installedReleaseLoadState==='error')loadInstalledReleaseNotes();
    const matchingReleases=releases.filter(r=>!r.draft&&Array.isArray(r.assets)&&typeof r.tag_name==='string'&&/^[A-Za-z0-9._-]+$/.test(r.tag_name)).map(r=>{
      const asset=r.assets.find(a=>a.name==='Backload.ino.bin'&&typeof a.digest==='string'&&/^sha256:[0-9a-f]{64}$/.test(a.digest)&&a.size>=1024&&a.size<=3145728);
      const downloadAsset=asset&&r.assets.find(a=>a.name==='Backload-'+r.tag_name.replace(/^v/,'')+'.bin'&&a.digest===asset.digest&&a.size===asset.size);
      return {tag:r.tag_name,name:r.name||r.tag_name,date:r.published_at||'',prerelease:r.prerelease,asset,downloadAsset,body:typeof r.body==='string'?r.body:''};
    }).filter(r=>r.asset).sort((a,b)=>b.date.localeCompare(a.date));
    githubReleases=matchingReleases.slice(0,8);
    select.replaceChildren(new Option(githubReleases.length?'Version auswählen …':'Keine passenden Releases',''));
    githubReleases.forEach((r,i)=>select.add(new Option(r.name+(r.prerelease?' · Vorabversion':'')+(r.tag.replace(/^v/,'')===currentFirmwareVersion?' · installiert':''),String(i))));
    updateReleaseDownload();
    select.disabled=!githubReleases.length;githubForm.querySelector('button').disabled=!githubReleases.length;
    if(!githubReleases.length)releaseMessage.textContent='Keine Releases mit passender Firmware und SHA-256-Prüfsumme gefunden.';
    else{
      const latest=matchingReleases.find(r=>!r.prerelease)||matchingReleases[0],comparison=compareVersions(latest.tag,currentFirmwareVersion);
      const availability=comparison===null?'':comparison>0?'Neue Version verfügbar. ':comparison===0?'Firmware ist aktuell. ':'Installierte Version ist neuer. ';
      releaseMessage.textContent=availability+'Neueste Version: '+latest.tag+' · Installiert: '+(currentFirmwareVersion||'unbekannt')+'.'+(githubReleases.length>1?' Ältere Versionen sind ebenfalls auswählbar.':'');
    }
  }catch(error){githubReleases=[];select.replaceChildren(new Option('Suche fehlgeschlagen',''));releaseMessage.className='message error';releaseMessage.textContent='GitHub nicht erreichbar: '+error.message+'. Internetverbindung dieses Geräts prüfen.';}
  finally{button.disabled=false;}
});
// Ein Netzwerkfehler bei der Statusabfrage beendet den Download auf dem ESP
// nicht. Nur Statusabfragen wiederholen, niemals den Update-Start erneut senden.
async function pollGithubUpdate(post,token,progress,message){
  let unreachableSince=null;
  for(;;){
    await new Promise(resolve=>setTimeout(resolve,1000));
    let state;
    try{
      state=await post('github-status',{token});
      unreachableSince=null;
    }catch(error){
      if(error.status===404){
        const unknown=new Error('Der Update-Status ist nicht mehr verfügbar. Bitte die installierte Firmware-Version nach dem Neustart prüfen.');
        unknown.updateUncertain=true;throw unknown;
      }
      if(error.status&&error.status<500)throw error;
      if(unreachableSince===null)unreachableSince=Date.now();
      if(Date.now()-unreachableSince>=45000){
        const unknown=new Error('Der ESP antwortet weiterhin nicht. Der Download kann noch laufen; bitte nach kurzer Zeit die installierte Firmware-Version prüfen.');
        unknown.updateUncertain=true;throw unknown;
      }
      message.className='message';message.textContent='Verbindung zum ESP unterbrochen. Update-Status wird erneut abgefragt …';
      continue;
    }
    progress.value=Math.round(state.received*100/state.size);
    if(state.state==='failed')throw new Error(state.error||'Download fehlgeschlagen');
    if(state.state==='finished')return;
    const attempt=state.attempt>1?' · Verbindungsversuch '+state.attempt+'/'+state.maxAttempts:'';
    message.textContent='Firmware wird von GitHub geladen: '+progress.value+' %'+attempt;
  }
}
githubForm.addEventListener('submit',async event=>{
  event.preventDefault();const selected=githubForm.elements.release.value;
  const release=selected===''?null:githubReleases[Number(selected)];if(!release)return;
  if(!confirm('Version '+release.tag+' installieren? Die Pumpe wird ausgeschaltet und der ESP startet danach neu.'))return;
  const button=githubForm.querySelector('button'),progress=document.getElementById('update-progress'),message=document.getElementById('update-message');
  button.disabled=true;progress.hidden=false;progress.value=0;message.className='message';message.textContent='GitHub-Download wird gestartet …';
  const post=(path,values)=>request('/api/update/'+path,{method:'POST',headers:{'X-Requested-With':'HeatingUI','Content-Type':'application/x-www-form-urlencoded'},body:new URLSearchParams(values)});
  let startConfirmed=false;
  try{
    const start=await post('github',{url:release.asset.browser_download_url,size:String(release.asset.size),sha256:release.asset.digest.slice(7)});
    startConfirmed=true;
    await pollGithubUpdate(post,start.token,progress,message);
    message.textContent='Update erfolgreich. Der ESP startet neu; bitte die Seite anschließend neu laden.';
  }catch(error){
    message.className='message error';
    if(error.updateUncertain)message.textContent='Update-Status unklar: '+error.message;
    else if(!startConfirmed&&(!error.status||error.status>=500))message.textContent='Update-Start nicht bestätigt. Der ESP könnte bereits laden; bitte nach kurzer Zeit den Status und die installierte Firmware-Version prüfen.';
    else message.textContent='Update nicht abgeschlossen: '+(error.message||'Verbindung unterbrochen')+' Bitte Status prüfen und erneut versuchen.';
  }
  finally{button.disabled=false;}
});
updateForm.addEventListener('submit',async event=>{
  event.preventDefault();
  const file=updateForm.elements.firmware.files[0];
  if(!file)return;
  if(!confirm('Firmware aktualisieren? Die Pumpe wird ausgeschaltet und der ESP startet danach neu.'))return;
  const button=updateForm.querySelector('button'),progress=document.getElementById('update-progress'),message=document.getElementById('update-message');
  button.disabled=true;progress.hidden=false;progress.value=0;message.className='message';message.textContent='Update wird vorbereitet …';
  const post=(path,values)=>request('/api/update/'+path,{method:'POST',headers:{'X-Requested-With':'HeatingUI','Content-Type':'application/x-www-form-urlencoded'},body:new URLSearchParams(values)});
  let token='';
  try{
    const start=await post('prepare',{size:String(file.size)});token=start.token;
    for(let offset=0;offset<file.size;offset+=4096){
      const bytes=new Uint8Array(await file.slice(offset,offset+4096).arrayBuffer());
      const data=btoa(Array.from(bytes,byte=>String.fromCharCode(byte)).join(''));
      let sent=false;
      for(let attempt=0;attempt<3&&!sent;attempt++){
        try{await post('chunk',{token,offset:String(offset),data});sent=true;}
        catch(error){if(attempt===2||(error.status&&error.status<500))throw error;await new Promise(resolve=>setTimeout(resolve,400));}
      }
      progress.value=Math.round((offset+bytes.length)*100/file.size);message.textContent='Firmware wird übertragen: '+progress.value+' %';
    }
    await post('finish',{token});token='';
    message.textContent='Update erfolgreich. Der ESP startet neu; bitte die Seite anschließend neu laden.';
  }catch(error){
    if(token){try{await post('abort',{token});}catch(ignored){}}
    message.className='message error';message.textContent='Update nicht abgeschlossen: '+(error.message||'Verbindung unterbrochen')+'. Bitte Status prüfen und erneut versuchen.';
  }finally{button.disabled=false;}
});
const sensorStatusMarkup='<span class="sensor-status" data-state="unknown" role="img" aria-label="Sensorstatus unbekannt" title="Sensorstatus unbekannt"><svg viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="1.8" stroke-linecap="round" stroke-linejoin="round" aria-hidden="true"><circle cx="12" cy="12" r="9"/><path class="sensor-status-mark" d="M9.5 9a2.5 2.5 0 0 1 5 0c0 2-2.5 2-2.5 4M12 16h.01"/></svg></span>';
function createPanel(c){
  const section=document.createElement('section');
  section.dataset.sensorId=String(c.id);
  if(!c.hasSettings||c.id!==0){
    section.innerHTML=`<div class="sensor-heading"><h2></h2>${sensorStatusMarkup}</div><div class="readings"><output class="temperature">—</output></div>`;
    section.querySelector('h2').textContent=c.name;
    const sparkline=addTemperatureSparkline(section,c);
    root.appendChild(section);
    return {section,sparkline,form:null,isDirty:()=>false};
  }
  const limits=c.limits;
  section.innerHTML=`<div class="sensor-heading"><h2></h2>${sensorStatusMarkup}</div>
    <div class="readings"><output class="temperature">—</output></div>
    <form><div><label for="target-${c.id}">Einlagerung ab (68–72 °C)</label><div class="stepper"><button type="button" data-field="target" data-direction="-1">−</button><input id="target-${c.id}" name="target" type="number" inputmode="numeric" step="1" required><button type="button" data-field="target" data-direction="1">+</button></div></div>
    <input name="hysteresis" type="hidden">
    <button type="submit">Speichern</button></form><p class="message" role="status"></p><p class="config-error error" role="alert" hidden></p><p class="rule muted"></p>`;
  section.querySelector('h2').textContent=c.name;
  const form=section.querySelector('form');
  form.elements.target.min=limits.minTarget;form.elements.target.max=limits.maxTarget;
  form.elements.hysteresis.min=limits.minHysteresis;form.elements.hysteresis.max=limits.maxHysteresis;
  form.elements.target.value=settingNumber(c.target);form.elements.hysteresis.value=inputNumber(c.hysteresis);
  form.elements.target.dataset.savedValue=c.target;
  form.elements.hysteresis.dataset.savedValue=c.hysteresis;
  form.querySelectorAll('.stepper button').forEach(button=>{
    const field=button.dataset.field;
    const label=field==='target'?'Starttemperatur':'Hysterese';
    button.setAttribute('aria-label',c.name+': '+label+(button.dataset.direction==='-1'?' senken':' erhöhen'));
  });
  const message=section.querySelector('.message');
  let dirty=false;
  form.addEventListener('input',()=>{dirty=true;message.className='message';message.textContent='Noch nicht gespeichert.';});
  form.addEventListener('focusout',event=>{
    if(!event.target.matches('input[type="number"]')||event.target.value==='')return;
    const value=Number(event.target.value);
    if(Number.isFinite(value))event.target.value=settingNumber(value);
  });
  form.addEventListener('click',event=>{
    const button=event.target.closest('.stepper button');
    if(!button||button.disabled)return;
    stepIntegerInput(form.elements[button.dataset.field],Number(button.dataset.direction));
  });
  form.addEventListener('submit',async event=>{
    event.preventDefault();if(saving||section.dataset.configValid!=='true')return;
    saving=true;
    root.querySelectorAll('button').forEach(b=>b.disabled=true);
    const target=form.elements.target.value,hysteresis=form.elements.hysteresis.value;
    message.className='message';message.textContent='Wird gespeichert …';
    try{
      await request('/api/settings',{method:'POST',headers:{'Content-Type':'application/x-www-form-urlencoded','X-Requested-With':'HeatingUI'},body:new URLSearchParams({id:c.id,target,hysteresis})});
      dirty=form.elements.target.value!==target||form.elements.hysteresis.value!==hysteresis;
      message.textContent=dirty?'Gespeichert. Weitere Änderungen sind noch offen.':'Einstellungen gespeichert.';
    }catch(error){message.className='message error';message.textContent='Speichern nicht bestätigt: '+error.message+' Werte prüfen und erneut speichern.';}
    finally{saving=false;root.querySelectorAll('button').forEach(b=>b.disabled=b.closest('section').dataset.configValid!=='true');}
  });
  const sparkline=addTemperatureSparkline(section,c);
  root.appendChild(section);
  return {section,sparkline,form,isDirty:()=>dirty};
}
let sparkData=null,sparkMessage='Verlauf wird geladen …',sparkLoading=false;
function addTemperatureSparkline(section,c){
  const chart=document.createElement('div');chart.className='sparkline';
  chart.innerHTML='<svg viewBox="0 0 360 62" preserveAspectRatio="none" role="img"><path></path><g></g></svg><div class="sparkline-info"><span>Letzte 6 Stunden · 1 Min.</span><span class="sparkline-range"></span></div><div class="sparkline-status"></div>';
  section.querySelector('.readings').after(chart);
  drawSparkline(chart,c.id,c.name);
  return chart;
}
function showTemperature(p,c){
  const valid=c&&c.valid===true&&typeof c.temperature==='number'&&Number.isFinite(c.temperature);
  const output=p.section.querySelector('.temperature');
  const detail=c?'Sensorfehler'+(c.sensorError?': '+c.sensorError:'')+(c.fault?' (0x'+c.fault.toString(16).toUpperCase()+')':''):'—';
  output.textContent=valid?temperatureNumber.format(c.temperature)+' °C':detail;
  output.className='temperature'+(c&&!valid?' error':'');
  const status=p.section.querySelector('.sensor-status');
  const state=!c?'unknown':valid?'ok':'error';
  const label=state==='ok'?'Sensor: OK':state==='error'?'Sensorfehler':'Sensorstatus unbekannt';
  status.dataset.state=state;status.setAttribute('aria-label',label);status.setAttribute('title',label);
  status.querySelector('.sensor-status-mark').setAttribute('d',state==='ok'?'M8 12l2.5 2.5L16.5 9':state==='error'?'M9 9l6 6M15 9l-6 6':'M9.5 9a2.5 2.5 0 0 1 5 0c0 2-2.5 2-2.5 4M12 16h.01');
}
function drawSparkline(chart,id,name){
  const svg=chart.querySelector('svg'),path=svg.querySelector('path'),dots=svg.querySelector('g');
  const status=chart.querySelector('.sparkline-status'),range=chart.querySelector('.sparkline-range');
  const points=sparkData&&Array.isArray(sparkData.points)?sparkData.points:[];
  const valid=points.filter(p=>typeof p[id+1]==='number'&&Number.isFinite(p[id+1]));
  path.setAttribute('d','');dots.replaceChildren();range.textContent='';
  if(!valid.length){
    svg.setAttribute('hidden','');status.textContent=sparkMessage||'Keine gültigen Werte im Zeitraum.';
    svg.setAttribute('aria-label',name+': '+status.textContent);return;
  }
  svg.removeAttribute('hidden');status.textContent='';
  const values=valid.map(p=>p[id+1]),min=Math.min(...values),max=Math.max(...values);
  const pad=Math.max(.5,(max-min)*.1),low=min-pad,high=max+pad;
  const end=points[points.length-1][0],start=end-359*60;
  const x=t=>Math.max(3,Math.min(357,3+(t-start)/(359*60)*354));
  const y=v=>4+(high-v)/(high-low)*54;
  let d='',active=false;
  for(const p of points){const v=p[id+1];if(typeof v!=='number'||!Number.isFinite(v)){active=false;continue;}d+=(active?' L':' M')+x(p[0]).toFixed(1)+' '+y(v).toFixed(1);active=true;}
  path.setAttribute('d',d);
  for(let i=0;i<points.length;i++){
    const p=points[i],v=p[id+1],prev=i>0&&typeof points[i-1][id+1]==='number',next=i+1<points.length&&typeof points[i+1][id+1]==='number';
    if(typeof v==='number'&&Number.isFinite(v)&&!prev&&!next){const dot=document.createElementNS('http://www.w3.org/2000/svg','circle');dot.setAttribute('cx',x(p[0]));dot.setAttribute('cy',y(v));dot.setAttribute('r','2.5');dots.appendChild(dot);}
  }
  range.textContent=temperatureNumber.format(min)+'–'+temperatureNumber.format(max)+' °C';
  svg.setAttribute('aria-label',name+': Verlauf der letzten sechs Stunden, Minimum '+temperatureNumber.format(min)+', Maximum '+temperatureNumber.format(max)+' Grad Celsius.');
}
async function loadSparklines(){
  if(sparkLoading)return;sparkLoading=true;
  try{
    sparkData=await request('/api/history?range=6h');
    sparkMessage=!sparkData.available?'Verlauf nicht verfügbar: PSRAM fehlt.':!sparkData.samples?'Noch keine Daten. Erfassung beginnt nach Zeitsynchronisierung.':'';
  }catch(error){sparkData=null;sparkMessage='Verlauf nicht erreichbar. Neuer Versuch erfolgt automatisch.';}
  finally{sparkLoading=false;panels.forEach((p,id)=>drawSparkline(p.sparkline,id,p.section.querySelector('h2').textContent));}
}
let runtimeData=null,runtimePeriodSelected=false;
const runtimeKind=document.getElementById('runtime-kind'),runtimePeriod=document.getElementById('runtime-period');
function runtimeDuration(value){
  const seconds=Number(value);
  if(!Number.isFinite(seconds)||seconds<0)return '—';
  const hours=Math.floor(seconds/3600),minutes=Math.floor(seconds%3600/60),remaining=seconds%60;
  return hours+' h '+String(minutes).padStart(2,'0')+' min '+String(remaining).padStart(2,'0')+' s';
}
function runtimePeriodLabel(kind,key){
  const text=String(key);
  if(kind==='days')return text.slice(6,8)+'.'+text.slice(4,6)+'.'+text.slice(0,4);
  if(kind==='months')return text.slice(4,6)+'.'+text.slice(0,4);
  return text;
}
function updateRuntime(data, simulation){
  runtimeData=data;
  const fields=['runtime-solar','runtime-charge','runtime-return'];
  const note=document.getElementById('runtime-note');
  if(!data||!data.available){
    runtimePeriod.replaceChildren(new Option('Keine Daten',''));runtimePeriod.disabled=true;
    fields.forEach(id=>document.getElementById(id).textContent='—');
    note.textContent=data?'Laufzeitspeicher nicht verfügbar.':'Keine Verbindung oder noch keine Laufzeiterfassung.';
    return;
  }
  const kind=runtimeKind.value,rows=Array.isArray(data[kind])?[...data[kind]].sort((a,b)=>b.period-a.period):[];
  const previous=runtimePeriod.value;
  runtimePeriod.replaceChildren(...rows.map(row=>new Option(runtimePeriodLabel(kind,row.period),String(row.period))));
  if(runtimePeriodSelected&&rows.some(row=>String(row.period)===previous))runtimePeriod.value=previous;
  runtimePeriod.disabled=!rows.length;
  const selected=rows.find(row=>String(row.period)===runtimePeriod.value);
  for(const [id,key] of [['runtime-solar','solar'],['runtime-charge','combiToBuffer'],['runtime-return','bufferToCombi']])
    document.getElementById(id).textContent=selected?runtimeDuration(selected[key]):'—';
  const notices=[];
  if(!data.clockValid)notices.push('ESP32-Uhr noch nicht synchronisiert; neue Laufzeiten sind keinem Datum zugeordnet.');
  if(!data.storageHealthy)notices.push('Speichern im Flash fehlgeschlagen; die Werte können bei Neustart verloren gehen.');
  if(simulation)notices.push('Testmodus: simulierte Pumpenläufe werden nicht gezählt.');
  if(data.undated?.some(value=>value>0))notices.push('Laufzeiten ohne gültige Uhr sind nicht in den Zeiträumen enthalten.');
  if(!rows.length&&data.clockValid)notices.push('Für diesen Zeitraum liegen noch keine Daten vor.');
  note.textContent=notices.join(' ')||'Erfassung seit Installation dieser Funktion · Speicherung etwa alle 15 Minuten.';
}
runtimeKind.addEventListener('change',()=>{runtimePeriodSelected=false;runtimePeriod.value='';updateRuntime(runtimeData,simulationEnabled);});
runtimePeriod.addEventListener('change',()=>{runtimePeriodSelected=true;updateRuntime(runtimeData,simulationEnabled);});
// Die Beispiele animieren nur, solange sie sichtbar sind; ohne JS bleiben sie lesbar.
if(document.body.classList.contains('organigram-page')){
  const guide=document.getElementById('org-led-guide');
  let guideVisible=false;
  const syncLedExamples=()=>guide.classList.toggle('is-visible',guideVisible&&!document.hidden);
  if('IntersectionObserver' in window){
    const observer=new IntersectionObserver(entries=>{guideVisible=entries.some(entry=>entry.isIntersecting);syncLedExamples();});
    observer.observe(guide);
  }else{guideVisible=true;syncLedExamples();}
  document.addEventListener('visibilitychange',syncLedExamples);
}
function updateOrganigram(data){
  const status=document.getElementById('org-status');
  const controllers=data&&Array.isArray(data.controllers)?data.controllers:[];
  const automatic=data&&data.automatic,heating=data&&data.heating,hardware=data&&data.hardware;
  const valid=!!(automatic&&automatic.configValid&&heating&&heating.configValid);
  const summary=!data?'Status nicht verfügbar':!valid?'Automatik gesperrt':
    (data.simulation?'Testmodus · ':'')+(heating.winter?'Winter · Kombi Mitte':'Sommer · Kombi oben');
  if(status.textContent!==summary)status.textContent=summary;
  status.className='badge'+(!data||!valid?' waiting':'');
  const modeKnown=!!(heating&&heating.configValid&&typeof heating.winter==='boolean');
  const summerActive=modeKnown&&!heating.winter,winterActive=modeKnown&&heating.winter;
  document.getElementById('org-mode-summer').className='org-season'+(summerActive?' is-active':'');
  document.getElementById('org-mode-winter').className='org-season'+(winterActive?' is-active':'');
  document.getElementById('org-mode-summer-state').textContent=summerActive?'Aktiv':'';
  document.getElementById('org-mode-winter-state').textContent=winterActive?'Aktiv':'';
  const settings=controllers.find(c=>c.id===0);
  document.getElementById('org-target').textContent=settings&&settings.configValid&&Number.isFinite(settings.target)?setpointNumber.format(settings.target)+' °C':'— °C';
  const start=automatic&&Number.isFinite(automatic.differenceStart)?setpointNumber.format(automatic.differenceStart)+' K':'— K';
  const stop=automatic&&Number.isFinite(automatic.differenceStop)?setpointNumber.format(automatic.differenceStop)+' K':'— K';
  document.getElementById('org-min-combi').textContent=automatic&&Number.isFinite(automatic.minimumCombi)?setpointNumber.format(automatic.minimumCombi)+' °C':'— °C';
  document.getElementById('org-min-buffer').textContent=automatic&&Number.isFinite(automatic.minimumBuffer)?setpointNumber.format(automatic.minimumBuffer)+' °C':'— °C';
  document.querySelectorAll('.org-start').forEach(e=>e.textContent=start);
  document.querySelectorAll('.org-stop-value').forEach(e=>e.textContent=stop);
  for(let i=0;i<5;i++){
    const c=controllers.find(item=>item.id===i);
    document.getElementById('org-cs-'+i).textContent=c&&Number.isInteger(c.csPin)?String(c.csPin):'—';
  }
  for(const [id,key] of [['org-relay-pump','pump'],['org-relay-direction','direction'],['org-relay-solar','solar'],['org-relay-oil','oil'],['org-spi-clk','clk'],['org-spi-mosi','mosi'],['org-spi-miso','miso'],['org-led','led'],['org-rgb-led','rgbLed']]){
    const pin=hardware&&hardware[key];
    document.getElementById(id).textContent=Number.isInteger(pin)?String(pin):'—';
  }
  const solar=data&&data.solar;
  const solarBadge=document.getElementById('org-solar-state');
  const solarIssue=solar&&(solar.paused?'Pausiert':!solar.manual&&!solar.configValid?'Einstellung ungültig':!solar.manual&&!solar.sensorsValid?'Sensorfehler':null);
  solarBadge.className='badge'+(solarIssue?' waiting':solar&&solar.on?' on':'');
  solarBadge.textContent=!solar?'Status unbekannt':solarIssue?solarIssue+' · AUS':solar.on?(data.simulation?'EIN · simuliert':'EIN'):(data.simulation?'AUS · simuliert':'AUS');
  for(const [id,index] of [['org-solar-panel',4],['org-solar-tank',3]]){
    const sensor=controllers.find(c=>c.id===index);
    document.getElementById(id).textContent=sensor&&sensor.valid&&Number.isFinite(sensor.temperature)?setpointNumber.format(sensor.temperature)+' °C':'—';
  }
  for(const [id,value] of [['org-solar-start',solar&&solar.differenceStart],['org-solar-stop',solar&&solar.differenceStop],['org-solar-difference',solar&&solar.difference]]){
    document.getElementById(id).textContent=Number.isFinite(value)?setpointNumber.format(value)+' K':'— K';
  }
  document.getElementById('org-direction-off').textContent=hardware&&hardware.directionOff===0?'Kombi → Puffer':hardware&&hardware.directionOff===1?'Puffer → Kombi':'—';
}
async function refresh(){
  const revision=pumpRevision;
  const autoRequestRevision=autoRevision;
  const simRequestRevision=simRevision;
  const solarRequestRevision=solarRevision;
  const heatingRequestRevision=heatingRevision;
  const remoteRelayRequestRevision=remoteRelayRevision;
  try{
    if(saving||wifiBusy||wifiScanning)return;
    const data=await request('/api/status');
    currentFirmwareVersion=typeof data.version==='string'?data.version:'';
    renderInstalledMainRelease();
    updateRuntime(data.pumpRuntime,data.simulation);
    if(simRequestRevision===simRevision)updateSimulation(data);
    updateWifi(data.wifi);
    updateHostname(data.wifi);
    updateRoof(data.roof,data.wifi);
    if(remoteRelayRequestRevision===remoteRelayRevision&&!remoteRelayBusy){
      remoteRelayCards=Array.isArray(data.remoteRelays)?data.remoteRelays:[data.remoteRelay];
      updateRemoteRelay(remoteRelayCards[0],data.simulation);updateExtraRelays(remoteRelayCards,data.simulation);
    }
    updateRelayDiscovery(data.relayDiscovery);updateRelayProvisioning(data.relayProvisioning);
    if(!heatingBusy&&heatingRequestRevision===heatingRevision)updateHeating(data.heating);
    if(autoRequestRevision===autoRevision){updateAutomatic(data.automatic);updateAutomaticRules(data.automatic,data.controllers,data.pump);}
    if(!pumpBusy&&revision===pumpRevision)updatePump(data.pump);
    if(!solarControlBusy&&solarRequestRevision===solarRevision){updateSolar(data.solar,data.simulation);updateSolarDiagram(data.solar,data.simulation,data.controllers);}
    if(document.body.classList.contains('organigram-page')&&!heatingBusy&&heatingRequestRevision===heatingRevision&&autoRequestRevision===autoRevision)updateOrganigram(data);
    updateStorageDiagram(data.controllers);
    drawBurnerPipes();
    const clock=document.getElementById('device-clock');
    clock.textContent=data.clock.available
      ?'ESP32-Zeit: '+data.clock.local+' '+data.clock.zone+(data.wifi.ip?'':' · läuft ohne WLAN weiter')
      :'ESP32-Zeit: noch nicht synchronisiert. WLAN mit Internetzugang erforderlich.';
    document.getElementById('firmware-version').textContent='Version: '+data.version.replace(/-(?:ohne|mit)-ap$/i,'');
    currentFirmwareVersion=data.version;
    for(const c of data.controllers){
      if(!panels.has(c.id))panels.set(c.id,createPanel(c));
      const p=panels.get(c.id);
      showTemperature(p,c);
      if(!p.form)continue;
      p.section.dataset.configValid=String(c.configValid);
      const configError=p.section.querySelector('.config-error');
      configError.hidden=c.configValid;
      configError.textContent=c.configValid?'':'Konfigurationsfehler: Grenzen oder Standardwerte im Sketch prüfen.';
      p.form.querySelectorAll('input,button').forEach(e=>e.disabled=!c.configValid||(e.tagName==='BUTTON'&&saving));
      p.section.querySelector('.rule').textContent=data.pump.manual?'Manuelle Steuerung: Starttemperatur ohne Wirkung. Ölbrennersperre gilt.':'Starttemperatur für den gewählten Kombifühler.';
      p.form.elements.target.dataset.savedValue=c.target;
      p.form.elements.hysteresis.dataset.savedValue=c.hysteresis;
      if(!p.isDirty()&&!p.form.contains(document.activeElement)&&!saving){p.form.elements.target.value=settingNumber(c.target);p.form.elements.hysteresis.value=inputNumber(c.hysteresis);}
    }
    setConnectionStatus(true);
  }catch(error){
    updateRuntime(null,false);
    remoteRelayCards=[];updateRemoteRelay(null,false);updateExtraRelays([],false);updateRelayDiscovery(null);updateRelayProvisioning(null);
    if(document.body.classList.contains('organigram-page'))updateOrganigram(null);
    if(!solarControlBusy&&solarRequestRevision===solarRevision){updateSolar(null,false);updateSolarDiagram(null,false,null);}
    if(!pumpBusy&&revision===pumpRevision)unknownPump();
    if(!heatingBusy&&heatingRequestRevision===heatingRevision)unknownHeating();
    updateStorageDiagram(null);
    updateAutomaticRules(null,null,null);
    document.getElementById('test-page-status').textContent='Keine Verbindung zum ESP32. Testmodus und Werte sind nicht aktuell.';
    document.getElementById('device-clock').textContent='ESP32-Zeit: keine aktuellen Daten.';
    document.getElementById('wifi-signal').textContent='Heim-WLAN-Signal: keine aktuellen Daten.';
    updateWifiSignalBars(null);
    document.getElementById('wifi-status').textContent='WLAN-Status nicht aktuell: Verbindung zum ESP32 unterbrochen.';
    roofBadge.textContent='Status unbekannt';roofBadge.className='badge';
    roofDetail.textContent='Keine aktuellen Daten vom Haupt-ESP.';
    setConnectionStatus(false);
    panels.forEach(p=>showTemperature(p,null));
  }finally{setTimeout(refresh,pumpState&&pumpState.switching?250:2000);}
}
const historyNames=['Kombispeicher oben','Kombispeicher Mitte','Pufferspeicher oben','Kombispeicher unten','Solarpanel'];
const historyColorsDark=['#63dac6','#ffd27a','#8bbcff','#c6a5ff','#ff9d85'];
const historyColorsLight=['#006b5d','#8a4d00','#145d9d','#6738a8','#a73517'];
let historyData=null,historyRequest=0;
const historyRange=document.getElementById('history-range'),historyLegend=document.getElementById('history-legend');
function historyColors(){return document.documentElement.classList.contains('light')?historyColorsLight:historyColorsDark;}
function selectedHistory(){return [...historyLegend.querySelectorAll('input')].map(input=>input.checked);}
function drawHistory(){
  if(!document.body.classList.contains('history-page'))return;
  const canvas=document.getElementById('history-chart'),box=canvas.getBoundingClientRect();
  if(!box.width||!box.height)return;
  const ratio=Math.min(window.devicePixelRatio||1,2),width=box.width,height=box.height;
  canvas.width=Math.round(width*ratio);canvas.height=Math.round(height*ratio);
  const ctx=canvas.getContext('2d');ctx.setTransform(ratio,0,0,ratio,0,0);ctx.clearRect(0,0,width,height);
  const colors=historyColors(),selected=selectedHistory(),points=historyData&&historyData.points||[];
  historyLegend.querySelectorAll('label').forEach((label,i)=>label.style.setProperty('--curve',colors[i]));
  let values=[];for(const p of points)for(let i=0;i<5;i++)if(selected[i]&&typeof p[i+1]==='number'&&Number.isFinite(p[i+1]))values.push(p[i+1]);
  if(!values.length){canvas.setAttribute('aria-label','Keine darstellbaren Messwerte für die ausgewählten Sensoren.');return;}
  let min=Math.min(...values),max=Math.max(...values),padding=Math.max(1,(max-min)*0.08);min=Math.floor(min-padding);max=Math.ceil(max+padding);
  const left=48,right=14,top=14,bottom=35,w=Math.max(1,width-left-right),h=Math.max(1,height-top-bottom);
  const textColor=getComputedStyle(document.getElementById('history-status')).color;
  ctx.font='12px system-ui, sans-serif';ctx.fillStyle=textColor;ctx.strokeStyle=textColor;ctx.globalAlpha=.18;
  for(let i=0;i<=4;i++){let y=top+h*i/4;ctx.beginPath();ctx.moveTo(left,y);ctx.lineTo(left+w,y);ctx.stroke();}
  ctx.globalAlpha=1;ctx.textAlign='right';ctx.textBaseline='middle';for(let i=0;i<=4;i++){let y=top+h*i/4;ctx.fillText((max-(max-min)*i/4).toFixed(1).replace('.',','),left-7,y);}
  const start=points[0][0],end=points[points.length-1][0],duration=Math.max(1,end-start);
  const x=t=>left+(t-start)/duration*w,y=v=>top+(max-v)/(max-min)*h;
  for(let i=0;i<5;i++){if(!selected[i])continue;ctx.strokeStyle=colors[i];ctx.fillStyle=colors[i];ctx.lineWidth=2;let active=false;ctx.beginPath();
    for(const point of points){const value=point[i+1];if(typeof value!=='number'||!Number.isFinite(value)){active=false;continue;}const px=points.length===1?left+w/2:x(point[0]),py=y(value);if(active)ctx.lineTo(px,py);else ctx.moveTo(px,py);active=true;}
    ctx.stroke();
    for(let j=0;j<points.length;j++){
      const value=points[j][i+1],previous=j>0&&typeof points[j-1][i+1]==='number',next=j+1<points.length&&typeof points[j+1][i+1]==='number';
      if(typeof value==='number'&&Number.isFinite(value)&&!previous&&!next){ctx.beginPath();ctx.arc(points.length===1?left+w/2:x(points[j][0]),y(value),3,0,Math.PI*2);ctx.fill();}
    }
  }
  const longRange=end-start>86400,small=width<400;
  const timeOptions=longRange?{day:'2-digit',month:'2-digit',year:'2-digit',...(small?{}:{hour:'2-digit',minute:'2-digit'})}:{day:'2-digit',month:'2-digit',hour:'2-digit',minute:'2-digit'};
  const timeFormat=new Intl.DateTimeFormat('de-DE',{timeZone:'Europe/Berlin',...timeOptions});
  ctx.fillStyle=textColor;ctx.textBaseline='top';ctx.textAlign='left';ctx.fillText(timeFormat.format(new Date(start*1000)),left,top+h+10,w/2-4);
  if(end>start){ctx.textAlign='right';ctx.fillText(timeFormat.format(new Date(end*1000)),left+w,top+h+10,w/2-4);}
  canvas.setAttribute('aria-label','Temperaturverlauf in Grad Celsius; '+points.length+' dargestellte Zeitpunkte.');
}
async function loadHistory(){
  if(!document.body.classList.contains('history-page'))return;
  const revision=++historyRequest,status=document.getElementById('history-status');
  try{
    const data=await request('/api/history?range='+encodeURIComponent(historyRange.value));
    if(revision!==historyRequest)return;
    historyData=data;
    status.className='muted';
    status.textContent=!data.available?'Kein ausreichender PSRAM verfügbar. Die Steuerung läuft weiter.':!data.samples?'Noch keine Daten. Nach Zeitsynchronisierung beginnt die Erfassung.':new Intl.NumberFormat('de-DE').format(data.samples)+' Minuten gespeichert · '+data.points.length+' Punkte dargestellt.';
    const latest=data.points[data.points.length-1];
    document.getElementById('history-latest').textContent=latest?historyNames.map((name,i)=>name+': '+(typeof latest[i+1]==='number'?temperatureNumber.format(latest[i+1])+' °C':'kein gültiger Wert')).join(' · '):'Noch keine Messwerte.';
    drawHistory();
  }catch(error){if(revision!==historyRequest)return;historyData=null;status.className='error';status.textContent='Verlauf nicht erreichbar. Verbindung prüfen und erneut versuchen.';document.getElementById('history-latest').textContent='Keine aktuellen Daten.';drawHistory();}
}
let infoBusy=false;
async function loadDeviceInfo(){
  if(infoBusy)return;infoBusy=true;
  const status=document.getElementById('info-status');
  const set=(id,value)=>document.getElementById('info-'+id).textContent=value;
  const size=value=>typeof value==='number'&&Number.isFinite(value)?new Intl.NumberFormat('de-DE',{maximumFractionDigits:2}).format(value>=1048576?value/1048576:value/1024)+(value>=1048576?' MiB':' KiB'):'—';
  try{
    const d=await request('/api/device-info');
    set('chip',d.chip);set('revision',d.revision);set('cores',String(d.cores));set('cpu',d.cpuMHz+' MHz');
    const seconds=d.uptimeSeconds,days=Math.floor(seconds/86400),hours=Math.floor(seconds%86400/3600),minutes=Math.floor(seconds%3600/60);
    set('uptime',(days?days+' Tage · ':'')+hours+' Std. '+minutes+' Min.');
    set('version',d.firmware);set('core',d.arduinoCore);set('idf',d.idf);
    set('slot',d.runningSlot?d.runningSlot.label+' · '+size(d.runningSlot.size):'Nicht verfügbar');
    set('update-slot',d.updateSlot?d.updateSlot.label+' · '+size(d.updateSlot.size):'Nicht verfügbar');
    set('flash',size(d.flashBytes));set('heap',size(d.heapBytes));set('heap-free',size(d.heapFreeBytes));
    set('psram',d.psramBytes?size(d.psramBytes):'Nicht verfügbar');set('psram-free',d.psramBytes?size(d.psramFreeBytes):'Nicht verfügbar');
    const w=d.wifi||{},n=d.wifiDetails||{},connected=w.state==='connected',ap=!!n.apActive;
    const shown=value=>value&&value!=='0.0.0.0'?value:'—';
    const states={unconfigured:'Nicht eingerichtet',queued:'Wird vorbereitet',starting:'Verbindung wird gestartet',connecting:'Verbindet',connected:'Verbunden',retry:'Nicht verbunden · neuer Versuch folgt'};
    set('wifi-state',states[w.state]||'Nicht verfügbar');set('wifi-ssid',shown(w.ssid));
    set('wifi-host',w.activeHostname?(w.activeHostname+(w.mdnsReady?' · '+w.activeHostname+'.local':' · mDNS nicht verfügbar')):'—');
    set('wifi-rssi',connected&&typeof w.rssi==='number'?w.rssi+' dBm':'—');set('wifi-channel',n.channel?String(n.channel):'—');
    const security=['Offen','WEP','WPA','WPA2','WPA / WPA2','WPA2 Enterprise','WPA3','WPA2 / WPA3'];
    set('wifi-security',Number.isInteger(n.auth)?(security[n.auth]||'Andere'):'—');set('wifi-standards',shown(n.standards));
    set('wifi-ip',shown(w.ip));set('wifi-mask',shown(n.mask));set('wifi-gateway',shown(n.gateway));
    set('wifi-dns',[n.dns1,n.dns2].filter(v=>v&&v!=='0.0.0.0').join(' · ')||'—');set('wifi-mac',shown(n.mac));set('wifi-bssid',shown(n.bssid));
    set('wifi-ap',ap?(shown(n.apSsid)+(w.pairing?' · Kopplung':' · Notzugang')):'Aus');
    set('wifi-ap-network',ap?[n.apIp,n.apMac].filter(Boolean).join(' · '):'—');set('wifi-clients',ap?String(n.apClients):'—');
    status.className='muted';status.textContent='Verbunden · Aktualisierung alle 5 Sekunden';
  }catch(error){
    document.querySelectorAll('#info-section dd').forEach(e=>e.textContent='—');
    status.className='error';status.textContent='Gerätedaten nicht erreichbar. Neuer Versuch erfolgt automatisch.';
  }finally{infoBusy=false;}
}
if(document.body.classList.contains('history-page')){
  historyNames.forEach((name,i)=>{const label=document.createElement('label'),input=document.createElement('input'),line=document.createElement('i');input.type='checkbox';input.checked=true;input.addEventListener('change',drawHistory);line.setAttribute('aria-hidden','true');label.append(input,line,document.createTextNode(name));historyLegend.appendChild(label);});
  historyRange.addEventListener('change',loadHistory);window.addEventListener('resize',drawHistory);loadHistory();setInterval(loadHistory,60000);
}
if(document.body.classList.contains('info-page')){loadDeviceInfo();setInterval(loadDeviceInfo,5000);}
if(!document.body.classList.contains('history-page')&&!document.body.classList.contains('settings-page')&&!document.body.classList.contains('info-page')&&!document.body.classList.contains('organigram-page')){loadSparklines();setInterval(loadSparklines,60000);}
if(document.body.classList.contains('settings-page'))loadInstalledReleaseNotes();
refresh();
</script></main></body></html>)HTML";
