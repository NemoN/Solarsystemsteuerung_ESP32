const assert = require('node:assert/strict');
const fs = require('node:fs');
const vm = require('node:vm');
const sketch = fs.readFileSync(require('node:path').join(__dirname, '../Backload/Backload.ino'), 'utf8');
const nodes = {};
const node = id => nodes[id] || (nodes[id] = {});
const form = {elements: Object.fromEntries(['start'].map(key => [key, {}])), contains: () => false};
const context = vm.createContext({
  document: {getElementById: node}, autoForm: form, autoDirty: false, autoBusy: false,
  inputNumber: n => n.toFixed(1), temperatureNumber: new Intl.NumberFormat('de-DE', {minimumFractionDigits:1, maximumFractionDigits:1}),
});
const start = sketch.indexOf('function updateAutomatic(state)');
const end = sketch.indexOf('function updateSimulation(', start);
assert(start > 0 && end > start);
vm.runInContext(sketch.slice(start, end), context);
const state = {configValid:true, winter:false, controlSensorId:0, allowCharging:true, targetOffset:0, differenceStart:8, differenceStop:0};
const controllers = [
  {id:0, name:'Kombispeicher oben', target:68, configValid:true, valid:true},
  {id:1, name:'Kombispeicher Mitte', target:69, configValid:true, valid:true},
  {id:2, name:'Pufferspeicher', valid:true},
];
let text = context.automaticRuleText(state, controllers, {manual:false});
assert.match(text.chargeStart, /68,0 °C.*8,0 K/);
assert.match(text.returnStart, /Puffer − Kombi ≥ 8,0 K/);
assert.doesNotMatch(text.returnStart, /68,0/);
assert.match(text.chargeStop, /Kombi ≤ Puffer/);
assert.doesNotMatch(text.chargeStop, /68,0|2,0 K/);
assert.match(text.returnStop, /Puffer ≤ Kombi/);
const blocked = context.automaticRuleText({...state, allowCharging:false}, controllers, {manual:false});
assert.match(blocked.chargeStart, /Sommer-.Winterumschaltung gesperrt/);
assert.equal(blocked.returnStart, text.returnStart);
assert.equal(blocked.returnStop, text.returnStop);
// Ein alter winter-/controlSensorId-Wert darf nie auf Mitte umschalten.
const legacy = {...state, winter:true, controlSensorId:1};
text = context.automaticRuleText(legacy, controllers, {manual:false});
assert.match(text.sensor, /oben.*68,0 °C/);
assert.match(text.chargeStart, /68,0 °C.*8,0 K/);
assert.match(text.returnStart, /Puffer − Kombi ≥ 8,0 K/);
assert.doesNotMatch(text.returnStart, /68,0/);
context.updateAutomatic(legacy);
assert.equal(form.elements.start.value, '8.0');
assert.equal(context.automaticRuleText({...state, differenceStart:4.9}, controllers, {}), null);
assert.equal(context.automaticRuleText({...state, differenceStart:10.1}, controllers, {}), null);
const badTarget=controllers.map(c=>c.id===0?{...c,target:45,configValid:false}:c);
const independent=context.automaticRuleText(state,badTarget,{manual:false});
assert.match(independent.chargeStart,/Starttemperatur muss 65–70/);
assert.equal(independent.returnStart,text.returnStart);
assert.equal(independent.returnStop,text.returnStop);
const failed = controllers.map(c => ({...c, valid:false}));
assert.match(context.automaticRuleText(state, failed, {manual:false}).status, /Sensorfehler/);
assert.match(context.automaticRuleText(state, failed, {manual:true}).status, /Handbetrieb/);
context.updateAutomaticRules(state, controllers, {manual:false});
assert.equal(node('auto-rules-table').hidden, false);
context.updateAutomaticRules(null, null, null);
assert.equal(node('auto-rules-table').hidden, true);
assert.equal(node('auto-charge-start').textContent, '');
assert.equal(context.automaticRuleText({...state, configValid:false}, controllers, {}), null);
assert.equal(context.automaticRuleText(state, [], {}), null);

// Die Schaltabstände werden unabhängig von der Sommer-/Winterumschaltung gespeichert.
let submit, posted;
form.addEventListener = (_, callback) => { submit = callback; };
form.querySelectorAll = () => [];
Object.assign(context, {URLSearchParams, autoMessage:{}, autoRevision:0, pumpRevision:0,
  request: async (url, options) => { posted = {url, values:options.body}; },
});
const submitStart = sketch.indexOf("autoForm.addEventListener('submit'");
const submitEnd = sketch.indexOf('let wifiBusy=', submitStart);
vm.runInContext(sketch.slice(submitStart, submitEnd), context);
Object.assign(form.elements.start, {value:'8.0'});
(async () => {
  await submit({preventDefault(){}});
  assert.equal(posted.url, '/api/automatic');
  assert.equal(posted.values.size, 1);
  assert.equal(posted.values.has('winter'), false);
  assert.equal(posted.values.has('charge'), false);
  assert.equal(posted.values.has('offset'), false);
  assert.equal(posted.values.has('stop'), false);
  assert.equal(posted.values.get('start'), '8.0');
  const saved=posted;
  form.elements.start.value='4.9';
  await submit({preventDefault(){}});
  assert.equal(posted,saved);
  assert.match(context.autoMessage.textContent,/5 bis 10 K/);

  // Die manuelle Brennerfreigabe hat einen eigenen Zustand und API-Endpunkt.
  for(const id of ['heating-switch','heating-badge','heating-detail','heating-switch-label','oil-burner-diagram']){
    node(id).attributes={};node(id).setAttribute=function(key,value){this.attributes[key]=value;};
  }
  const diagramClasses=new Set();
  node('oil-burner-diagram').classList={toggle:(name,on)=>{if(on)diagramClasses.add(name);else diagramClasses.delete(name);}};
  Object.assign(context,{heatingState:null,heatingBusy:false,heatingRevision:0,
    heatingSwitch:node('heating-switch'),heatingMessage:{},simulationEnabled:false,updatePumpControls:()=>{}});
  const heatingStart=sketch.indexOf('function updateHeating(state)');
  const heatingEnd=sketch.indexOf('heatingSwitch.addEventListener',heatingStart);
  vm.runInContext(sketch.slice(heatingStart,heatingEnd),context);
  context.updateHeating({winter:false,configValid:true,relayOn:false});
  assert.equal(node('heating-switch').attributes['aria-checked'],'false');
  assert.match(node('heating-detail').textContent,/AUS/);
  assert.equal(node('oil-burner-state').textContent,'Gesperrt');
  assert.equal(diagramClasses.has('released'),false);
  context.request=async(url,options)=>{
    posted={url,values:options.body};
    return {winter:options.body.get('winter')==='1',configValid:true,relayOn:false};
  };
  await context.changeHeating();
  assert.equal(posted.url,'/api/heating');assert.equal(posted.values.get('winter'),'1');
  assert.equal(node('heating-switch').attributes['aria-checked'],'true');
  assert.equal(node('oil-burner-state').textContent,'Freigabe pausiert');
  assert.equal(diagramClasses.has('released'),false);
  context.updateHeating({winter:true,configValid:true,relayOn:true});
  assert.equal(node('oil-burner-state').textContent,'Freigegeben');
  assert.equal(diagramClasses.has('released'),true);
  assert.match(node('oil-burner-diagram').attributes['aria-label'],/Freigegeben.*Relais 4: AN/);
  context.simulationEnabled=true;
  context.updateHeating({winter:true,configValid:true,relayOn:false});
  assert.match(node('heating-detail').textContent,/Testmodus.*Relais 4 bleibt AUS/);
  assert.equal(node('oil-burner-state').textContent,'Freigegeben (simuliert)');
  assert.equal(diagramClasses.has('released'),true);
  assert.match(node('oil-burner-detail').textContent,/Relais 4: AUS/);
  context.updateHeating({winter:false,configValid:false,relayOn:false});
  assert.equal(node('heating-switch-label').textContent,'Sommerbetrieb bestätigen');
  assert.equal(node('oil-burner-state').textContent,'Status unbekannt');
  assert.equal(diagramClasses.has('released'),false);
  await context.changeHeating();
  assert.equal(posted.values.get('winter'),'0'); // Ungültigen NVS-Zustand sicher quittieren.
  assert.equal(node('oil-burner-state').textContent,'Gesperrt (simuliert)');
  assert.equal(diagramClasses.has('released'),false);
  context.unknownHeating();
  assert.equal(node('heating-switch').disabled,true);
  assert.equal(node('oil-burner-state').textContent,'Status unbekannt');
  assert.equal(node('oil-burner-detail').textContent,'Keine aktuellen Daten');
  const last=posted;
  await context.changeHeating();assert.equal(posted,last);
  console.log('PASS: feste obere Fühler, 5–10 K, Rückladung, getrennte Ölfreigabe, Brenner-SVG, Simulation und unbekannte Zustände.');
})().catch(error => { console.error(error); process.exitCode=1; });
