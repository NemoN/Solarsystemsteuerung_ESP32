// Prüft die Anzeige mit wechselnden Betriebszuständen und GPIO-Zuordnungen.
const assert = require('node:assert/strict');
const fs = require('node:fs');
const vm = require('node:vm');
const sketch = fs.readFileSync(require('node:path').join(__dirname, '../Backload/Backload.ino'), 'utf8');
const nodes = {};
const get = id => nodes[id] || (nodes[id] = {textContent:'',className:''});
const starts = [{},{}], stops = [{},{}];
const context = vm.createContext({
  document: {getElementById:get, querySelectorAll: selector => selector === '.org-start' ? starts : stops},
  setpointNumber: new Intl.NumberFormat('de-DE', {maximumFractionDigits:1}),
});
const start=sketch.indexOf('function updateOrganigram(data)');
const end=sketch.indexOf('async function refresh()',start);
assert(start>0 && end>start);
vm.runInContext(sketch.slice(start,end),context);
const data={simulation:true,heating:{winter:true,configValid:true},
  automatic:{configValid:true,differenceStart:8,differenceStop:3},
  controllers:[{id:0,csPin:4,configValid:true,target:68},{id:1,csPin:5},{id:2,csPin:6},{id:3,csPin:7},{id:4,csPin:15}],
  hardware:{clk:38,mosi:11,miso:13,pump:3,direction:8,solar:9,oil:10,directionOff:0}};
context.updateOrganigram(data);
assert.equal(get('org-status').textContent,'Testmodus · Winter · Kombi Mitte');
assert.equal(get('org-target').textContent,'68 °C');
assert(starts.every(n=>n.textContent==='8 K') && stops.every(n=>n.textContent==='3 K'));
assert.deepEqual([0,1,2,3,4].map(i=>get('org-cs-'+i).textContent),['4','5','6','7','15']);
assert.deepEqual(['pump','direction','solar','oil'].map(n=>get('org-relay-'+n).textContent),['3','8','9','10']);
assert.deepEqual(['clk','mosi','miso'].map(n=>get('org-spi-'+n).textContent),['38','11','13']);
assert.equal(get('org-direction-off').textContent,'Kombi → Puffer');
data.hardware.direction=12;data.hardware.directionOff=1;data.controllers[0].csPin=14;data.heating.winter=false;
context.updateOrganigram(data);
assert.equal(get('org-status').textContent,'Testmodus · Sommer · Kombi oben');
assert.equal(get('org-relay-direction').textContent,'12');assert.equal(get('org-cs-0').textContent,'14');
assert.equal(get('org-direction-off').textContent,'Puffer → Kombi');
context.updateOrganigram(null);
assert.equal(get('org-status').textContent,'Status nicht verfügbar');
assert.equal(get('org-relay-direction').textContent,'—');assert.equal(get('org-cs-0').textContent,'—');
assert.equal(get('org-direction-off').textContent,'—');
console.log('PASS: Organigramm mit Live-Betriebsart, Schwellwerten, GPIOs und Verbindungsabbruch.');
