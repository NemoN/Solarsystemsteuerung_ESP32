const assert=require('node:assert/strict');
const fs=require('node:fs'),vm=require('node:vm');
const sketch=fs.readFileSync(require('node:path').join(__dirname,'../Backload/DashboardPage.h'),'utf8');
const nodes={};const get=id=>nodes[id]||(nodes[id]={});
let submit,posted=null;
const form={elements:{start:{value:'',dataset:{}}},contains:()=>false,querySelectorAll:()=>[],addEventListener:(name,fn)=>{if(name==='submit')submit=fn;}};
const message={},controlMessage={};
function switchStub(){return {disabled:true,attributes:{},setAttribute(k,v){this.attributes[k]=v;},addEventListener(){}};}
const solarManualSwitch=switchStub(),solarPowerSwitch=switchStub();
const context=vm.createContext({document:{getElementById:get,activeElement:null},solarForm:form,solarMessage:message,
  solarManualSwitch,solarPowerSwitch,solarControlMessage:controlMessage,solarState:null,solarControlBusy:false,
  solarDirty:false,solarBusy:false,solarRevision:0,simulationEnabled:true,URLSearchParams,
  inputNumber:n=>String(n),setpointNumber:new Intl.NumberFormat('de-DE',{maximumFractionDigits:1}),
  request:async(url,options)=>{posted={url,values:options.body};return {on:true,configValid:true,sensorsValid:true,paused:false,difference:6,differenceStart:6,differenceStop:1};}});
const start=sketch.indexOf('function updateSolar(state,simulation)');
const end=sketch.indexOf('let wifiBusy=',start);
assert(start>0&&end>start);vm.runInContext(sketch.slice(start,end),context);
context.updateSolar({on:false,configValid:true,sensorsValid:true,paused:false,differenceStart:5,difference:2},true);
assert.equal(get('solar-badge').textContent,'AUS · simuliert');assert.equal(form.elements.start.value,'5');
context.updateSolar({on:true,configValid:true,sensorsValid:true,paused:false,differenceStart:6,difference:6},false);
assert.equal(get('solar-badge').textContent,'EIN');assert.match(get('solar-detail').textContent,/6 K/);assert.match(get('solar-detail').className,/state-ok/);
context.updateSolar({on:false,configValid:true,sensorsValid:false,paused:false,differenceStart:5,difference:null},false);
assert.equal(get('solar-badge').textContent,'Sensorfehler · AUS');assert.match(get('solar-detail').className,/state-warning/);
context.updateSolar({on:true,configValid:true,sensorsValid:false,sensorGrace:true,paused:false,difference:null},false);
assert.equal(get('solar-badge').textContent,'EIN');assert.match(get('solar-detail').textContent,/Panel-Sensorfehler.*60 Sekunden/);
context.updateSolar({on:true,configValid:true,sensorsValid:false,sensorGrace:true,sensorFaultPending:true,lastDifference:3.2,panelReadingAgeMs:59999,paused:false},false);
assert.match(get('solar-detail').textContent,/Aktuelle Differenz: 3,2 K/);assert.match(get('solar-detail').textContent,/vorübergehend nicht aktualisiert/);assert.match(get('solar-detail').className,/state-warning/);
context.updateSolar({on:false,configValid:true,sensorsValid:false,sensorFaultPending:true,lastDifference:3.3,panelReadingAgeMs:60000,paused:false},false);
assert.match(get('solar-detail').textContent,/Differenz mit letztem Panelwert: 3,3 K/);assert.match(get('solar-detail').className,/state-neutral/);
context.updateSolar({on:true,configValid:true,sensorsValid:true,difference:3.4,panelReadingAgeMs:0,paused:false},false);
assert.match(get('solar-detail').textContent,/Aktuelle Differenz: 3,4 K/);assert.doesNotMatch(get('solar-detail').textContent,/vorübergehend/);
context.updateSolar({on:false,configValid:true,sensorsValid:false,sensorFaultPending:false,panelReadingAgeMs:61000,paused:false},false);
assert.equal(get('solar-badge').textContent,'Sensorfehler · AUS');assert.match(get('solar-detail').className,/state-warning/);
context.updateSolar({on:true,configValid:true,sensorsValid:false,sensorGrace:true,sensorFaultPending:true,lastDifference:6,paused:false},false);
assert.equal(get('solar-badge').textContent,'EIN');assert.match(get('solar-detail').textContent,/Differenz mit letztem Panelwert: 6 K/);
assert.doesNotMatch(get('solar-detail').textContent,/Sensorfehler|nicht verfügbar/);assert.match(get('solar-detail').className,/state-neutral/);
context.updateSolar({on:false,configValid:true,sensorsValid:false,sensorFaultPending:true,paused:false},false);
assert.equal(get('solar-badge').textContent,'AUS');assert.doesNotMatch(get('solar-detail').textContent,/Sensorfehler|nicht verfügbar/);
context.updateSolar({on:false,configValid:false,sensorsValid:true,paused:false,differenceStart:null},false);
assert.equal(get('solar-badge').textContent,'Einstellung ungültig · AUS');
context.updateSolar(null,false);assert.equal(get('solar-badge').textContent,'Status unbekannt');
assert(solarManualSwitch.disabled&&solarPowerSwitch.disabled);
context.updateSolar({manual:true,requestedOn:true,on:true,configValid:false,sensorsValid:false,paused:false},false);
assert.equal(get('solar-badge').textContent,'EIN');assert.match(get('solar-detail').textContent,/Sensorfehler.*übergangen/);
assert.equal(solarManualSwitch.attributes['aria-checked'],'true');assert.equal(solarPowerSwitch.attributes['aria-checked'],'true');
assert(!solarManualSwitch.disabled&&!solarPowerSwitch.disabled);
context.updateSolar({manual:false,requestedOn:false,on:false,configValid:true,sensorsValid:true,paused:true},false);
assert(solarManualSwitch.disabled&&solarPowerSwitch.disabled);
(async()=>{
 form.elements.start.value='2.9';await submit({preventDefault(){}});assert.equal(posted,null);assert.match(message.textContent,/3 bis 10/);
 form.elements.start.value='5.5';await submit({preventDefault(){}});assert.equal(posted.url,'/api/solar');assert.equal(posted.values.get('start'),'5.5');assert.equal(message.textContent,'Solarregelung gespeichert.');
 const last=posted;form.elements.start.value='5.55';await submit({preventDefault(){}});assert.equal(posted,last);
 form.elements.start.value='10.1';await submit({preventDefault(){}});assert.equal(posted,last);
 const safe={on:false,manual:false,requestedOn:false,configValid:true,sensorsValid:false,paused:false,differenceStart:5};
 let posts=0,fail=false;
 context.updateSolarDiagram=()=>{};
 context.request=async(url,options)=>{
  if(url==='/api/status')return {solar:{...safe,manual:true,requestedOn:true,on:true},simulation:true,controllers:[]};
  assert.equal(url,'/api/solar/control');posts++;posted={url,values:options.body};
  if(fail)throw new Error('Antwort verloren');
  return {...safe,manual:true,requestedOn:options.body.get('action')==='power',on:options.body.get('action')==='power'};
 };
 context.updateSolar(safe,true);await context.changeSolarControl('manual');
 assert.equal(posted.values.get('action'),'manual');assert.equal(posted.values.get('value'),'1');
 assert.match(controlMessage.textContent,/zunächst AUS/);
 await context.changeSolarControl('power');assert.equal(posted.values.get('action'),'power');
 assert.equal(solarPowerSwitch.attributes['aria-checked'],'true');assert.equal(get('solar-badge').textContent,'EIN · simuliert');
 fail=true;await context.changeSolarControl('power');assert.equal(posts,3);
 assert.match(controlMessage.textContent,/nicht bestätigt.*Status wurde neu abgefragt/);
 assert.equal(solarPowerSwitch.attributes['aria-checked'],'true');
 console.log('PASS: Solar-Einstellung, simulierte und tatsächliche Statusanzeige.');
})().catch(error=>{console.error(error);process.exit(1);});
