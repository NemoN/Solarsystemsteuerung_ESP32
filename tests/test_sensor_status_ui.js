const assert = require('node:assert/strict');
const fs = require('node:fs');
const path = require('node:path');
const vm = require('node:vm');
const page = fs.readFileSync(path.join(__dirname, '../Backload/DashboardPage.h'), 'utf8');
const styles = fs.readFileSync(path.join(__dirname, '../Backload/DashboardStyles.h'), 'utf8');
const panelStart = page.indexOf('function createPanel(c)');
const panelEnd = page.indexOf('let sparkData=', panelStart);
const panel = page.slice(panelStart, panelEnd);
assert.equal((panel.match(/\$\{sensorStatusMarkup\}/g) || []).length, 2, 'Beide Kacheltypen erhalten ein Symbol');
assert.match(styles, /\.sensor-heading\{display:flex;justify-content:space-between/);
assert.match(page, /role="img" aria-label="Sensorstatus unbekannt"/);

const context = vm.createContext({temperatureNumber:new Intl.NumberFormat('de-DE', {minimumFractionDigits:1, maximumFractionDigits:1})});
const iconStart = page.indexOf('function sensorIconMarkup(id)');
assert(iconStart > 0 && iconStart < panelStart);
vm.runInContext(page.slice(iconStart, panelStart), context);
for (const [id, level] of [[0,7], [1,12], [2,7], [3,17]]) {
  const icon = context.sensorIconMarkup(id);
  assert.match(icon, /sensor-icon-tank/);
  assert.match(icon, new RegExp(`cy="${level}"`), 'Markierung zeigt die Position des Fühlers');
  assert.match(icon, /aria-hidden="true"/);
}
assert.match(context.sensorIconMarkup(4), /sensor-icon-sun/);
assert.match(context.sensorIconMarkup(4), /<circle/);
assert.equal((panel.match(/\$\{sensorIconMarkup\(c\.id\)\}/g) || []).length, 2, 'Beide Kacheltypen erhalten ein passendes Symbol');

const start = page.indexOf('function showTemperature(p,c)');
const end = page.indexOf('function drawSparkline(', start);
assert(start > 0 && end > start);
vm.runInContext(page.slice(start,end), context);

function makePanel() {
  const mark = {setAttribute(name, value){this[name]=value;}};
  const status = {dataset:{}, setAttribute(name, value){this[name]=value;}, querySelector(){return mark;}};
  const output = {};
  return {section:{querySelector(selector){return selector === '.temperature' ? output : status;}}, status, mark, output};
}
for (const id of [0,1,2,3,4]) {
  const p = makePanel();
  context.showTemperature(p, {id,valid:true,temperature:42.5});
  assert.equal(p.status.dataset.state,'ok');
  assert.equal(p.status['aria-label'],'Sensor: OK');
  assert.equal(p.status.title,'Sensor: OK');
  assert.match(p.mark.d,/M8 12/);
  assert.equal(p.output.textContent,'42,5 °C');
  context.showTemperature(p, {id,valid:false,sensorError:'Leitung unterbrochen',fault:4});
  assert.equal(p.status.dataset.state,'error');
  assert.equal(p.status['aria-label'],'Sensorfehler');
  assert.match(p.mark.d,/M9 9l6 6/);
  assert.match(p.output.textContent,/Leitung unterbrochen.*0x4/);
  context.showTemperature(p, {id,valid:true,temperature:null});
  assert.equal(p.status.dataset.state,'error', 'Ein ungültiger Messwert ist kein OK');
  context.showTemperature(p, null);
  assert.equal(p.status.dataset.state,'unknown');
  assert.equal(p.status['aria-label'],'Sensorstatus unbekannt');
  assert.match(p.mark.d,/M9\.5 9a/);
  assert.equal(p.output.textContent,'—');
  context.showTemperature(p, {id,valid:true,temperature:20});
  assert.equal(p.status.dataset.state,'ok', 'Status erholt sich nach Verbindungsausfall');
}
const pending = makePanel();
context.showTemperature(pending,{id:4,valid:false,sensorFaultPending:true,lastValidTemperature:15.2,sensorError:'MAX31865',fault:0x10});
assert.equal(pending.output.textContent,'15,2 °C');
assert.equal(pending.status.dataset.state,'unknown');
assert.equal(pending.status['aria-label'],'Letzter gültiger Messwert');
assert.match(pending.mark.d,/M9\.5 9a/);
context.showTemperature(pending,{id:4,valid:false,sensorFaultPending:true,lastValidTemperature:null});
assert.equal(pending.output.textContent,'—');
assert.equal(pending.status.dataset.state,'unknown');
context.showTemperature(pending,{id:4,valid:false,sensorFaultPending:true,lastValidTemperature:0});
assert.equal(pending.output.textContent,'0,0 °C');
context.showTemperature(pending,{id:4,valid:false,sensorFaultPending:false,sensorError:'MAX31865',fault:0x10});
assert.equal(pending.status.dataset.state,'error');
assert.match(pending.output.textContent,/Sensorfehler.*0x10/);
context.showTemperature(pending,{id:4,valid:true,temperature:15.5});
assert.equal(pending.output.textContent,'15,5 °C');assert.equal(pending.status.dataset.state,'ok');
console.log('PASS: Sensorstatus für alle Kacheln, Fehler und Verbindungsausfall.');
