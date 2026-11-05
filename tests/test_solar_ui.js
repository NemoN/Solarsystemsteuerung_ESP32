const assert=require('node:assert/strict');
const fs=require('node:fs'),vm=require('node:vm');
const sketch=fs.readFileSync(require('node:path').join(__dirname,'../Backload/Backload.ino'),'utf8');
const nodes={};const get=id=>nodes[id]||(nodes[id]={});
let submit,posted=null;
const form={elements:{start:{value:'',dataset:{}}},contains:()=>false,querySelectorAll:()=>[],addEventListener:(name,fn)=>{if(name==='submit')submit=fn;}};
const message={};
const context=vm.createContext({document:{getElementById:get,activeElement:null},solarForm:form,solarMessage:message,
  solarDirty:false,solarBusy:false,solarRevision:0,simulationEnabled:true,URLSearchParams,
  inputNumber:n=>String(n),setpointNumber:new Intl.NumberFormat('de-DE',{maximumFractionDigits:1}),
  request:async(url,options)=>{posted={url,values:options.body};return {on:true,configValid:true,sensorsValid:true,paused:false,difference:6,differenceStart:6,differenceStop:1};}});
const start=sketch.indexOf('function updateSolar(state,simulation)');
const end=sketch.indexOf('let wifiBusy=',start);
assert(start>0&&end>start);vm.runInContext(sketch.slice(start,end),context);
context.updateSolar({on:false,configValid:true,sensorsValid:true,paused:false,differenceStart:5,difference:2},true);
assert.equal(get('solar-badge').textContent,'AUS · simuliert');assert.equal(form.elements.start.value,'5');
context.updateSolar({on:true,configValid:true,sensorsValid:true,paused:false,differenceStart:6,difference:6},false);
assert.equal(get('solar-badge').textContent,'EIN');assert.match(get('solar-detail').textContent,/6 K/);
context.updateSolar({on:false,configValid:true,sensorsValid:false,paused:false,differenceStart:5,difference:null},false);
assert.equal(get('solar-badge').textContent,'Sensorfehler · AUS');
context.updateSolar({on:false,configValid:false,sensorsValid:true,paused:false,differenceStart:null},false);
assert.equal(get('solar-badge').textContent,'Einstellung ungültig · AUS');
context.updateSolar(null,false);assert.equal(get('solar-badge').textContent,'Status unbekannt');
(async()=>{
 form.elements.start.value='2.9';await submit({preventDefault(){}});assert.equal(posted,null);assert.match(message.textContent,/3 bis 10/);
 form.elements.start.value='5.5';await submit({preventDefault(){}});assert.equal(posted.url,'/api/solar');assert.equal(posted.values.get('start'),'5.5');assert.equal(message.textContent,'Solarregelung gespeichert.');
 const last=posted;form.elements.start.value='5.55';await submit({preventDefault(){}});assert.equal(posted,last);
 form.elements.start.value='10.1';await submit({preventDefault(){}});assert.equal(posted,last);
 console.log('PASS: Solar-Einstellung, simulierte und tatsächliche Statusanzeige.');
})().catch(error=>{console.error(error);process.exit(1);});
