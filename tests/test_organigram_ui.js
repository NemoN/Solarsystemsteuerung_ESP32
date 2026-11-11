// Prüft die Anzeige mit wechselnden Betriebszuständen und GPIO-Zuordnungen.
const assert = require('node:assert/strict');
const fs = require('node:fs');
const vm = require('node:vm');
const sketch = fs.readFileSync(require('node:path').join(__dirname, '../Backload/DashboardPage.h'), 'utf8');
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
  automatic:{configValid:true,differenceStart:8,minimumCombi:35,minimumBuffer:40,differenceStop:3},
  controllers:[{id:0,csPin:4,configValid:true,target:68},{id:1,csPin:5},{id:2,csPin:6},{id:3,csPin:7,valid:true,temperature:40},{id:4,csPin:15,valid:true,temperature:46}],
  solar:{on:true,configValid:true,sensorsValid:true,paused:false,differenceStart:5,differenceStop:1,difference:6},
  hardware:{clk:38,mosi:11,miso:13,pump:3,direction:8,solar:18,oil:17,led:2,rgbLed:48,directionOff:0}};
context.updateOrganigram(data);
assert.equal(get('org-status').textContent,'Testmodus · Winter · Kombi Mitte');
assert.equal(get('org-solar-state').textContent,'EIN · simuliert');
assert.equal(get('org-solar-panel').textContent,'46 °C');
assert.equal(get('org-solar-tank').textContent,'40 °C');
assert.equal(get('org-solar-difference').textContent,'6 K');
assert.equal(get('org-solar-start').textContent,'5 K');
assert.equal(get('org-solar-stop').textContent,'1 K');
data.solar.sensorsValid=false;data.solar.on=false;context.updateOrganigram(data);
assert.equal(get('org-solar-state').textContent,'Sensorfehler · AUS');
data.solar.manual=true;data.solar.on=true;context.updateOrganigram(data);
assert.equal(get('org-solar-state').textContent,'EIN · simuliert');
data.solar.manual=false;
data.solar.sensorsValid=true;data.solar.on=true;context.updateOrganigram(data);
assert.equal(get('org-mode-winter').className,'org-season is-active');
assert.equal(get('org-mode-winter-state').textContent,'Aktiv');
assert.equal(get('org-mode-summer').className,'org-season');
assert.equal(get('org-target').textContent,'68 °C');
assert.equal(get('org-min-combi').textContent,'35 °C');
assert.equal(get('org-min-buffer').textContent,'40 °C');
assert(starts.every(n=>n.textContent==='8 K') && stops.every(n=>n.textContent==='3 K'));
assert.deepEqual([0,1,2,3,4].map(i=>get('org-cs-'+i).textContent),['4','5','6','7','15']);
assert.deepEqual(['pump','direction','solar','oil'].map(n=>get('org-relay-'+n).textContent),['3','8','18','17']);
assert.deepEqual(['clk','mosi','miso'].map(n=>get('org-spi-'+n).textContent),['38','11','13']);
assert.equal(get('org-direction-off').textContent,'Kombi → Puffer');
assert.equal(get('org-led').textContent,'2');
assert.equal(get('org-rgb-led').textContent,'48');
data.hardware.direction=12;data.hardware.directionOff=1;data.controllers[0].csPin=14;data.heating.winter=false;
context.updateOrganigram(data);
assert.equal(get('org-status').textContent,'Testmodus · Sommer · Kombi oben');
assert.equal(get('org-mode-summer').className,'org-season is-active');
assert.equal(get('org-mode-winter-state').textContent,'');
assert.equal(get('org-relay-direction').textContent,'12');assert.equal(get('org-cs-0').textContent,'14');
assert.equal(get('org-direction-off').textContent,'Puffer → Kombi');
data.automatic.configValid=false;
context.updateOrganigram(data);
assert.equal(get('org-status').textContent,'Automatik gesperrt');
assert.equal(get('org-mode-summer-state').textContent,'Aktiv');
context.updateOrganigram(null);
assert.equal(get('org-status').textContent,'Status nicht verfügbar');
assert.equal(get('org-mode-summer').className,'org-season');
assert.equal(get('org-mode-winter').className,'org-season');
assert.equal(get('org-relay-direction').textContent,'—');assert.equal(get('org-cs-0').textContent,'—');
assert.equal(get('org-direction-off').textContent,'—');
assert.equal(get('org-led').textContent,'—');
assert.equal(get('org-rgb-led').textContent,'—');
assert.equal(get('org-solar-state').textContent,'Status unbekannt');
assert.equal(get('org-solar-start').textContent,'— K');
// Die Beispiele laufen nur, wenn die Legende sichtbar und der Tab aktiv ist.
const examplesStart=sketch.indexOf("if(document.body.classList.contains('organigram-page')){",sketch.indexOf('// Die Beispiele animieren'));
const examplesEnd=sketch.indexOf('function updateOrganigram(data)',examplesStart);
assert(examplesStart>0 && examplesEnd>examplesStart);
let observerCallback, observed, visible=false, visibilityHandler;
const guide={classList:{toggle:(name,value)=>{assert.equal(name,'is-visible');visible=value;}}};
const doc={body:{classList:{contains:()=>true}},hidden:false,getElementById:()=>guide,addEventListener:(event,handler)=>{assert.equal(event,'visibilitychange');visibilityHandler=handler;}};
class FakeObserver {constructor(callback){observerCallback=callback;} observe(element){observed=element;}}
vm.runInNewContext(sketch.slice(examplesStart,examplesEnd),{document:doc,window:{IntersectionObserver:FakeObserver},IntersectionObserver:FakeObserver});
assert.equal(observed,guide);assert.equal(visible,false);
observerCallback([{isIntersecting:true}]);assert.equal(visible,true);
doc.hidden=true;visibilityHandler();assert.equal(visible,false);
doc.hidden=false;visibilityHandler();assert.equal(visible,true);
observerCallback([{isIntersecting:false}]);assert.equal(visible,false);
const styles=fs.readFileSync(require('node:path').join(__dirname,'../Backload/DashboardStyles.h'),'utf8');
assert(styles.includes('@media(prefers-reduced-motion:reduce){.backload .org-led-guide .org-led-dot{animation:none;opacity:1}}'));
for(const name of ['normal','manual','fault','test','update'])assert(sketch.includes('org-led-'+name));
console.log('PASS: Regelungsübersicht mit Live-Werten und zugänglichen RGB-Beispielen.');
