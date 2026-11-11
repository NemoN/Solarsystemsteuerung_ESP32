const assert=require('node:assert/strict');
const fs=require('node:fs'),vm=require('node:vm');
const sketch=fs.readFileSync(require('node:path').join(__dirname,'../Backload/DashboardPage.h'),'utf8');
const nodes={};
function node(id){
  if(!nodes[id]){
    const classes=new Set();
    nodes[id]={textContent:'',attributes:{},classes,
      setAttribute(key,value){this.attributes[key]=value;},
      classList:{toggle:(key,on)=>on?classes.add(key):classes.delete(key)}};
  }
  return nodes[id];
}
const context=vm.createContext({document:{getElementById:node,querySelector:node},temperatureNumber:new Intl.NumberFormat('de-DE',{minimumFractionDigits:1,maximumFractionDigits:1})});
const start=sketch.indexOf('function updateSolarDiagram('),end=sketch.indexOf('async function changePump(',start);
assert(start>0&&end>start);vm.runInContext(sketch.slice(start,end),context);
const solar={on:true,relayOn:true,configValid:true,sensorsValid:true,paused:false};
const controllers=[{id:4,valid:true,temperature:58.2}];
context.updateSolarDiagram(solar,false,controllers);
assert.equal(node('solar-diagram-state').textContent,'EIN');
assert.equal(node('solar-diagram-panel').textContent,'Solarpanel: 58,2 °C');
assert.equal(node('solar-diagram-detail').textContent,'Relais 3: AN');
assert(node('solar-pump-diagram').classes.has('running'));
assert(node('.burner-pipes').classes.has('solar-active'));
assert.match(node('solar-pump-diagram').attributes['aria-label'],/EIN.*58,2 °C.*Relais 3: AN/);
context.updateSolarDiagram({...solar,relayOn:false},true,controllers);
assert.equal(node('solar-diagram-state').textContent,'EIN (simuliert)');
assert.equal(node('solar-diagram-detail').textContent,'Relais 3: AUS · Testmodus');
context.updateSolarDiagram({...solar,on:false,relayOn:false,sensorsValid:false},false,[{id:4,valid:false,temperature:null}]);
assert.equal(node('solar-diagram-state').textContent,'Sensorfehler · AUS');
assert.equal(node('solar-diagram-panel').textContent,'Solarpanel: Sensorfehler');
assert(!node('solar-pump-diagram').classes.has('running'));
assert(!node('.burner-pipes').classes.has('solar-active'));
context.updateSolarDiagram({...solar,sensorsValid:false,sensorGrace:true},false,[{id:4,valid:false,temperature:null}]);
assert.equal(node('solar-diagram-state').textContent,'EIN');
assert.match(node('solar-diagram-detail').textContent,/Panel-Fehler kurz überbrückt/);
assert(node('solar-pump-diagram').classes.has('running'));
context.updateSolarDiagram({...solar,sensorsValid:false,sensorGrace:true,sensorFaultPending:true},false,[{id:4,valid:false,sensorFaultPending:true,lastValidTemperature:58.2,temperature:null}]);
assert.equal(node('solar-diagram-panel').textContent,'Solarpanel: 58,2 °C');
assert.equal(node('solar-diagram-panel').title,'Letzter gültiger Messwert');
assert.match(node('solar-pump-diagram').attributes['aria-label'],/letzter gültiger Messwert/);
assert.equal(node('solar-diagram-detail').textContent,'Relais 3: AN · letzter Panelwert');
assert(node('solar-pump-diagram').classes.has('running'));
assert.doesNotMatch(node('solar-diagram-detail').textContent,/Fehler|nicht verfügbar/);
context.updateSolarDiagram({...solar,on:false,relayOn:false,sensorsValid:false,sensorFaultPending:true},false,[{id:4,valid:false,sensorFaultPending:true,lastValidTemperature:58.2,temperature:null}]);
assert.equal(node('solar-diagram-state').textContent,'AUS');
assert.equal(node('solar-diagram-panel').textContent,'Solarpanel: 58,2 °C');
context.updateSolarDiagram({...solar,on:false,relayOn:false,sensorsValid:false,sensorFaultPending:true},false,[{id:4,valid:false,sensorFaultPending:true,lastValidTemperature:null,temperature:null}]);
assert.equal(node('solar-diagram-panel').textContent,'Solarpanel: —');
context.updateSolarDiagram({...solar,manual:true,sensorsValid:false},false,[{id:4,valid:false,temperature:null}]);
assert.equal(node('solar-diagram-state').textContent,'EIN');
assert.match(node('solar-diagram-detail').textContent,/Handbetrieb/);
assert(node('solar-pump-diagram').classes.has('running'));
context.updateSolarDiagram({...solar,on:false,relayOn:false,paused:true},false,controllers);
assert.equal(node('solar-diagram-state').textContent,'Pausiert · AUS');
context.updateSolarDiagram(null,false,null);
assert.equal(node('solar-diagram-state').textContent,'Status unbekannt');
assert.equal(node('solar-diagram-panel').textContent,'Solarpanel: keine aktuellen Daten');
assert.equal(node('solar-diagram-detail').textContent,'Keine aktuellen Daten');
assert(!node('solar-pump-diagram').classes.has('running'));
// Die Verbindung erreicht bei Desktop- und mobiler Anordnung den Solar-Pumpenanschluss.
const paths={};
const wires={setAttribute(){},querySelector:selector=>paths[selector]||(paths[selector]={setAttribute(key,value){this[key]=value;}})};
const rect=(left,top,width,height)=>({left,top,width,height,right:left+width,bottom:top+height});
let tankRect=rect(170,30,440,280),solarRect=rect(630,110,128,110);
const burner={getBoundingClientRect:()=>rect(0,100,128,96)};
const tank={getBoundingClientRect:()=>tankRect},solarIcon={getBoundingClientRect:()=>solarRect};
const box={getBoundingClientRect:()=>rect(0,0,784,420),querySelector:selector=>({'.burner-icon':burner,'.pump-flow':tank,'.burner-pipes':wires,'.solar-icon':solarIcon}[selector])};
const layoutContext=vm.createContext({document:{querySelector:()=>box}});
const layoutStart=sketch.indexOf('function drawBurnerPipes(){'),layoutEnd=sketch.indexOf("window.addEventListener('resize',drawBurnerPipes)",layoutStart);
vm.runInContext(sketch.slice(layoutStart,layoutEnd),layoutContext);layoutContext.drawBurnerPipes();
assert.match(paths['.solar-link'].d,/^M666 191 H622 V324 H266 V290$/);
tankRect=rect(0,140,440,280);solarRect=rect(0,450,88,75.625);layoutContext.drawBurnerPipes();
assert.match(paths['.solar-link'].d,/^M24\.75 505\.6875 H-8 V434 H96 V400$/);
// Die Animation läuft nur im sichtbaren Tab und bleibt bei reduzierter Bewegung stehen.
let observerCallback,visibilityHandler,observed,visible=false;
const motionOverview={classList:{toggle:(name,value)=>{assert.equal(name,'motion-visible');visible=value;}}};
class MotionObserver{constructor(callback){observerCallback=callback;}observe(target){observed=target;}}
const doc={hidden:false,querySelector:()=>motionOverview,addEventListener:(name,fn)=>{assert.equal(name,'visibilitychange');visibilityHandler=fn;}};
const motionStart=sketch.indexOf('function setupPumpMotion(){'),motionEnd=sketch.indexOf('setupPumpMotion();',motionStart);
assert(motionStart>0&&motionEnd>motionStart);
vm.runInNewContext(sketch.slice(motionStart,motionEnd)+'setupPumpMotion();',{document:doc,window:{IntersectionObserver:MotionObserver},IntersectionObserver:MotionObserver});
assert.equal(observed,motionOverview);assert.equal(visible,false);
observerCallback([{isIntersecting:true}]);assert.equal(visible,true);
doc.hidden=true;visibilityHandler();assert.equal(visible,false);
doc.hidden=false;visibilityHandler();assert.equal(visible,true);
observerCallback([{isIntersecting:false}]);assert.equal(visible,false);
const styles=fs.readFileSync(require('node:path').join(__dirname,'../Backload/DashboardStyles.h'),'utf8');
assert(sketch.includes('class="solar-pump-rotor"'));
assert(styles.includes('.solar-diagram.running .solar-pump-rotor{animation:pump-rotor-spin 5s linear infinite}'));
assert(styles.includes('@media(prefers-reduced-motion:reduce){.backload .storage-overview.motion-visible .solar-diagram.running .solar-pump-rotor{animation:none}}'));
// Die zusätzliche Backloadpumpe folgt denselben Sichtbarkeits- und Bewegungseinstellungen.
assert(sketch.includes('class="transfer-pump-rotor"'));
assert(styles.includes('.pump-flow:not(.stopped) .transfer-pump-rotor{animation:transfer-pump-spin 3s linear infinite}'));
assert(styles.includes('.pump-flow[data-direction="ccw"]:not(.stopped) .transfer-pump-rotor{animation-direction:reverse}'));
assert(styles.includes('@media(prefers-reduced-motion:reduce){.backload .storage-overview.motion-visible .pump-flow:not(.stopped) .transfer-pump-rotor{animation:none}}'));
console.log('PASS: Solarübersicht bei Betrieb, Testmodus, Sensorfehler, Pause und Verbindungsabbruch.');
