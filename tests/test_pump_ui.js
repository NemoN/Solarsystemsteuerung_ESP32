const assert = require('node:assert/strict');
const fs = require('node:fs');
const vm = require('node:vm');
const sketch = fs.readFileSync(require('node:path').join(__dirname, '../Backload/Backload.ino'), 'utf8');
const nodes = new Map();
function node(id) {
  if (!nodes.has(id)) {
    const classes = new Set();
    nodes.set(id, {style:{}, attributes:{}, classes,
      setAttribute(key, value) { this.attributes[key] = value; },
      classList:{add:key=>classes.add(key), toggle:(key,on)=>on?classes.add(key):classes.delete(key)},
    });
  }
  return nodes.get(id);
}
const context = vm.createContext({
  document:{getElementById:node, querySelector:node}, URLSearchParams,
  manualSwitch:node('manual-switch'), pumpSwitch:node('pump-switch'), directionSwitch:node('direction-switch'),
  pumpBadge:node('pump-badge'), pumpMessage:node('pump-message'),
  heatingSwitch:node('heating-switch'), heatingMessage:node('heating-message'),
  pumpState:null, pumpBusy:false, pumpRevision:0,
  heatingState:null, heatingBusy:false, heatingRevision:0, autoRevision:0, simulationEnabled:true,
});
function load(start, end) {
  const a = sketch.indexOf(start), b = sketch.indexOf(end, a);
  assert(a > 0 && b > a);
  vm.runInContext(sketch.slice(a, b), context);
}
load('const directionName=', 'function updateStorageDiagram(');
load('async function changePump(', "manualSwitch.addEventListener('click'");
load('function updateHeating(', "heatingSwitch.addEventListener('click'");
const summer = {winter:false, configValid:true, relayOn:false};
const winter = {winter:true, configValid:true, relayOn:false};
const state = fields => ({manual:true, on:false, requestedOn:false, direction:0, requestedDirection:0,
  phase:'ready', switching:false, remainingMs:0, automaticConfigured:true, ...fields});
const offForward = state({});
const offReturn = state({direction:1, requestedDirection:1});
const onReturn = state({direction:1, requestedDirection:1, on:true, requestedOn:true});
let calls = [];
function commandResponse(result) {
  context.request = async (url, options) => {
    calls.push({url, options});
    return result;
  };
}

(async () => {
  context.updateHeating(winter);
  context.updatePump(offForward);
  assert.equal(node('.pump-flow').attributes['data-direction'], 'ccw');
  assert(node('.pump-flow').classes.has('stopped'));
  assert.equal(node('pump-switch').disabled, true);
  assert.equal(node('direction-switch').disabled, false);
  assert.match(node('pump-help').textContent, /Ölbrenner freigegeben.*Richtung EIN.*Puffer → Kombi/);
  assert.match(node('pump-direction-state').textContent, /Förderrichtung: Kombispeicher/);

  // Im Winter erst die erlaubte Richtung wählen, dann starten; Ausschalten bleibt möglich.
  commandResponse(offReturn);
  await context.changePump('direction');
  assert.equal(node('pump-switch').disabled, false);
  commandResponse(onReturn);
  await context.changePump('power');
  assert.equal(calls.at(-1).options.body.get('value'), '1');
  assert.equal(node('pump-badge').textContent, 'Backloadpumpe: AN');
  assert.equal(node('.pump-flow').attributes['data-direction'], 'cw');
  assert(!node('.pump-flow').classes.has('stopped'));
  assert.match(node('pump-flow-title').textContent, /Rechtslauf/);
  assert.equal(node('pump-switch').disabled, false);
  assert.equal(node('direction-switch').disabled, true);
  commandResponse(offReturn);
  await context.changePump('power');
  assert.equal(calls.at(-1).options.body.get('value'), '0');
  assert.equal(node('pump-badge').textContent, 'Backloadpumpe: AUS');
  assert.equal(node('direction-switch').disabled, false);

  // Im Sommer ist auch der Vorwärtsstart erlaubt.
  context.updateHeating(summer);
  context.updatePump(offForward);
  assert.equal(node('pump-switch').disabled, false);
  commandResponse(state({on:true, requestedOn:true}));
  await context.changePump('power');
  assert.equal(node('pump-badge').textContent, 'Backloadpumpe: AN');
  assert.equal(node('.pump-flow').attributes['data-direction'], 'ccw');
  assert(!node('.pump-flow').classes.has('stopped'));
  assert.match(node('pump-flow-title').textContent, /Linkslauf/);
  context.updatePump(state({switching:true,phase:'stopping',direction:0,requestedDirection:1,requestedOn:true}));
  assert(node('.pump-flow').classes.has('stopped'));
  assert.equal(node('.pump-flow').attributes['data-direction'], 'ccw');

  // Ein anderer Client kann inzwischen Winter einschalten: HTTP 409 ist kein WLAN-Ausfall.
  context.updateHeating(summer);context.updatePump(offForward);calls=[];
  context.request = async (url, options) => {
    calls.push({url, options});
    if (url === '/api/pump') throw Object.assign(new Error('Ölbrenner freigegeben; Kombi → Puffer gesperrt.'), {status:409});
    return {pump:offForward, heating:winter};
  };
  await context.changePump('power');
  assert.deepEqual(calls.map(c=>c.url), ['/api/pump', '/api/status']);
  assert.match(node('pump-message').textContent, /Befehl abgelehnt:.*Ölbrenner/);
  assert.match(node('pump-direction-state').textContent, /^Förderrichtung:/);
  assert.equal(node('pump-badge').textContent, 'Backloadpumpe: AUS');
  assert.equal(node('pump-switch').disabled, true);
  assert.equal(node('direction-switch').disabled, false);

  // Verlorene POST-Antwort: nur lesend prüfen, kein zweiter Schaltauftrag.
  context.updatePump(offReturn);calls=[];
  context.request = async (url, options) => {
    calls.push({url, options});
    if (url === '/api/pump') throw new Error('Zeitlimit bei der Antwort');
    return {pump:onReturn, heating:winter};
  };
  await context.changePump('power');
  assert.deepEqual(calls.map(c=>c.url), ['/api/pump', '/api/status']);
  assert.equal(node('pump-badge').textContent, 'Backloadpumpe: AN');
  assert.match(node('pump-direction-state').textContent, /^Förderrichtung:/);
  assert.match(node('pump-message').textContent, /Schalten nicht bestätigt:.*neu abgefragt/);

  // Erst wenn die Statusabfrage ebenfalls scheitert, ist die Richtung unbekannt.
  context.request = async () => { throw new Error('Verbindung unterbrochen'); };
  await context.changePump('power');
  assert.equal(context.pumpState, null);
  assert(node('.pump-flow').classes.has('stopped'));
  assert.equal(node('.pump-flow').attributes['data-direction'], 'unknown');
  assert.match(node('pump-direction-state').textContent, /Keine aktuelle Förderrichtung/);
  assert.equal(node('pump-switch').disabled, true);
  assert.equal(node('direction-switch').disabled, true);
  assert.match(node('pump-message').textContent, /Pumpenstatus konnte nicht geladen werden/);
  console.log('PASS: Manuelle Steuerung im Winter, Rückladung starten/stoppen, Sommer, HTTP-Ablehnung, verlorene Antwort und echter Verbindungsabbruch.');
})().catch(error => { console.error(error); process.exitCode=1; });
