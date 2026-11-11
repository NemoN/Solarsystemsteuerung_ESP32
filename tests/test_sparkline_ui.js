const assert = require('node:assert/strict');
const fs = require('node:fs');
const vm = require('node:vm');
const page = fs.readFileSync(require('node:path').join(__dirname, '../Backload/DashboardPage.h'), 'utf8');
function element() {
  return {attributes:{}, children:[], textContent:'', setAttribute(name,value){this.attributes[name]=String(value);}, removeAttribute(name){delete this.attributes[name];}, replaceChildren(){this.children=[];}, appendChild(child){this.children.push(child);}};
}
const line=element(),area=element(),dots=element(),svg=element(),status=element(),range=element();
svg.querySelector=selector=>({'.sparkline-line':line,'.sparkline-area':area,'g':dots})[selector];
const chart={querySelector:selector=>({'svg':svg,'.sparkline-status':status,'.sparkline-range':range})[selector]};
const document={createElementNS:()=>element()};
const context = vm.createContext({document, sparkData:{points:[[0,10],[60,11],[120,null],[180,12],[240,13],[300,null],[360,14]]}, sparkMessage:'', temperatureNumber:new Intl.NumberFormat('de-DE',{minimumFractionDigits:1,maximumFractionDigits:1})});
const start = page.indexOf('function drawSparkline('), end = page.indexOf('async function loadSparklines(', start);
assert(start > 0 && end > start);
vm.runInContext(page.slice(start,end), context);
context.drawSparkline(chart,0,'Kombispeicher');
assert.equal((line.attributes.d.match(/M/g) || []).length,3,'Messlücken bleiben in der Linie sichtbar');
assert.equal((area.attributes.d.match(/Z/g) || []).length,2,'Farbflächen werden je Messreihe geschlossen');
assert.equal(dots.children.length,1,'Ein einzelner Messwert bleibt sichtbar');
assert.match(range.textContent,/10,0–14,0/);
context.sparkData = {points:[[0,null]]};
context.drawSparkline(chart,0,'Kombispeicher');
assert.equal(area.attributes.d,'');assert.equal(line.attributes.d,'');
assert('hidden' in svg.attributes);
console.log('PASS: Sparkline-Verlauf, Messlücken und fehlende Daten.');
