// Übt die echte WLAN-Anzeige mit ein- und ausgeschaltetem ESP-Zugang aus.
const assert = require('node:assert/strict');
const fs = require('node:fs');
const vm = require('node:vm');
const sketch = fs.readFileSync(require('node:path').join(__dirname, '../Backload/Backload.ino'), 'utf8');
const start = sketch.indexOf('function updateWifiSignalBars(wifi)');
const end = sketch.indexOf('function updateRoof(', start);
assert(start >= 0 && end > start);
const nodes = Object.fromEntries(['wifi-status', 'wifi-address', 'wifi-signal', 'wifi-ap-description'].map(id => [id, {}]));
const bars = Array.from({length: 5}, () => ({active: false, classList: {toggle(_key, active) { this.owner.active = active; }}}));
for (const bar of bars) bar.classList.owner = bar;
nodes['wifi-signal-bars'] = {attributes: {}, querySelectorAll: () => bars, setAttribute(key, value) { this.attributes[key] = value; }};
const context = vm.createContext({document: {getElementById: id => nodes[id]}, wifiPending: null, wifiDirty: true});
vm.runInContext(sketch.slice(start, end), context);
const wifi = {state: 'connected', ssid: 'Heimnetz', ip: '192.168.1.10', apIp: '192.168.4.1', rssi: -64, apRecoveryOnly: false};
context.updateWifi(wifi);
assert.match(nodes['wifi-address'].textContent, /192\.168\.4\.1/);
assert.match(nodes['wifi-ap-description'].textContent, /bleibt erreichbar/);
assert.equal(bars.filter(bar => bar.active).length, 4);
assert.match(nodes['wifi-signal-bars'].attributes['aria-label'], /4 von 5/);
for (const [rssi, expected] of [[-60,5], [-61,4], [-68,3], [-76,2], [-85,2], [-86,1]]) {
  context.updateWifiSignalBars({...wifi, rssi});
  assert.equal(bars.filter(bar => bar.active).length, expected);
}
context.updateWifi({...wifi, apIp: '', apRecoveryOnly: true});
assert.match(nodes['wifi-address'].textContent, /ausgeschaltet/);
assert(!nodes['wifi-address'].textContent.includes('192.168.4.1'));
assert.match(nodes['wifi-ap-description'].textContent, /längerem WLAN-Ausfall/);
context.updateWifi({...wifi, state: 'retry', ip: '', rssi: null, apRecoveryOnly: true});
assert.match(nodes['wifi-address'].textContent, /192\.168\.4\.1/);
assert.match(nodes['wifi-signal'].textContent, /nicht verfügbar/);
assert.equal(bars.filter(bar => bar.active).length, 0);
assert.match(nodes['wifi-signal-bars'].attributes['aria-label'], /nicht verfügbar/);
context.updateWifiSignalBars(null);
assert.equal(bars.filter(bar => bar.active).length, 0);
console.log('PASS: WLAN-Anzeige für aktiven AP, ausgeschalteten AP und Notzugang.');
