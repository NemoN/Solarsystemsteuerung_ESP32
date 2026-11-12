const assert = require('node:assert/strict');
const fs = require('node:fs');
const vm = require('node:vm');
const page = fs.readFileSync(require('node:path').join(__dirname, '../Backload/DashboardPage.h'), 'utf8');
function element() {
  return {attributes:{}, children:[], textContent:'', setAttribute(name,value){this.attributes[name]=String(value);}, removeAttribute(name){delete this.attributes[name];}, replaceChildren(){this.children=[];}, appendChild(child){this.children.push(child);}};
}
const line=element(),area=element(),dots=element(),svg=element(),status=element(),range=element(),resolution=element();
svg.querySelector=selector=>({'.sparkline-line':line,'.sparkline-area':area,'g':dots})[selector];
const chart={querySelector:selector=>({'svg':svg,'.sparkline-status':status,'.sparkline-range':range,'.sparkline-resolution':resolution})[selector]};
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
context.sparkData = {now:2700,archiveIntervalSeconds:900,
  archiveSeries:[{id:0,points:[[0,10],[900,11],[1800,12],[2700,99]]}],
  points:[[1800,20],[1860,21],[1920,null],[1980,22]]};
context.drawSparkline(chart,0,'Kombispeicher');
assert.equal((line.attributes.d.match(/M/g) || []).length,2,'Archiv und lokale Werte verbinden; lokale Fehler bleiben Lücken');
assert.match(range.textContent,/10,0–22,0/,'Lokale Werte ersetzen überlappende Archivwerte');
assert.match(resolution.textContent,/Archiv 15 Min\. \/ ESP 1 Min\./);
context.sparkData = {now:3600,archiveIntervalSeconds:900,
  archiveSeries:[{id:0,points:[[0,10],[900,null],[1800,12],[3600,13]]}],points:[]};
context.drawSparkline(chart,0,'Kombispeicher');
assert.equal((line.attributes.d.match(/M/g) || []).length,3,'Archivlücken und fehlende Intervalle bleiben sichtbar');
assert.equal(dots.children.length,3);
context.sparkData = {points:[[0,null]]};
context.drawSparkline(chart,0,'Kombispeicher');
assert.equal(area.attributes.d,'');assert.equal(line.attributes.d,'');
assert('hidden' in svg.attributes);
console.log('PASS: Sparkline-Verlauf, Messlücken und fehlende Daten.');
