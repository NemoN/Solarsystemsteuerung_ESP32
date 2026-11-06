const assert = require('node:assert/strict');
const fs = require('node:fs');
const vm = require('node:vm');
const sketch = fs.readFileSync(require('node:path').join(__dirname, '../Backload/Backload.ino'), 'utf8');
const styles = fs.readFileSync(require('node:path').join(__dirname, '../Backload/DashboardStyles.h'), 'utf8');
const historySection = sketch.indexOf('<section id="history-section"');
const runtimeSection = sketch.indexOf('<section id="pump-runtime-section"');
const settingsSection = sketch.indexOf('<header id="settings-intro"');
assert(historySection > 0 && runtimeSection > historySection && runtimeSection < settingsSection);
assert(styles.includes('.backload #history-section,.backload #pump-runtime-section{display:none}'));
assert(styles.includes('body.history-page .backload #pump-runtime-section{display:block}'));
assert(styles.includes(':not(#history-section):not(#pump-runtime-section):not(#test-message)'));
const nodes = new Map();
class MockOption { constructor(text, value) { this.text=text; this.value=value; } }
function get(id) {
  if (!nodes.has(id)) nodes.set(id, {value:id==='runtime-kind'?'days':'', textContent:'', disabled:false, options:[],
    addEventListener(event,callback){this[event]=callback;},
    replaceChildren(...options){this.options=options; this.value=options[0]?.value || '';}});
  return nodes.get(id);
}
const ctx=vm.createContext({document:{getElementById:get},Option:MockOption,simulationEnabled:false});
const start=sketch.indexOf('let runtimeData=null,'),end=sketch.indexOf('// Die Beispiele animieren',start);
assert(start>0 && end>start);vm.runInContext(sketch.slice(start,end),ctx);
const row=(period,solar,combiToBuffer,bufferToCombi)=>({period,solar,combiToBuffer,bufferToCombi});
const data={available:true,clockValid:true,storageHealthy:true,undated:[0,0,0],
  days:[row(20261105,61,0,0),row(20261106,3661,62,5)],months:[row(202611,9999,0,0)],years:[row(2026,9999,0,0)]};
ctx.updateRuntime(data,false);
assert.equal(get('runtime-period').value,'20261106');
assert.equal(get('runtime-solar').textContent,'1 h 01 min 01 s');
get('runtime-period').value='20261105';get('runtime-period').change();
assert.equal(get('runtime-solar').textContent,'0 h 01 min 01 s');
ctx.updateRuntime(data,false);assert.equal(get('runtime-period').value,'20261105');
get('runtime-kind').value='months';get('runtime-kind').change();
assert.equal(get('runtime-period').value,'202611');
assert.equal(get('runtime-period').options[0].text,'11.2026');
ctx.updateRuntime({...data,clockValid:false,storageHealthy:false,undated:[2,0,0]},true);
assert.match(get('runtime-note').textContent,/nicht synchronisiert/);
assert.match(get('runtime-note').textContent,/Flash fehlgeschlagen/);
assert.match(get('runtime-note').textContent,/simulierte Pumpenläufe/);
ctx.updateRuntime({available:false},false);
assert.equal(get('runtime-solar').textContent,'—');assert.equal(get('runtime-period').disabled,true);
console.log('PASS: Pumpenlaufzeiten, Richtungen, Zeitraumwahl, Uhr-, Speicher- und Testmodus-Hinweise.');
