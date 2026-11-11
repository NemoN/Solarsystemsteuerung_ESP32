const assert=require('node:assert/strict'),fs=require('node:fs'),vm=require('node:vm'),path=require('node:path');
const sketch=fs.readFileSync(path.join(__dirname,'../Backload/DashboardPage.h'),'utf8');
const start=sketch.indexOf('let controllerRebootBusy='),end=sketch.indexOf('let saving=false;',start);
assert(start>0&&end>start);
const nodes={};function node(id){return nodes[id]||(nodes[id]={disabled:false,textContent:'',addEventListener(name,fn){this[name]=fn;}});}
let allow=true,requests=[],failure=null,timer;
const card={configured:true,online:true,busy:false,updating:false,reboot:'idle'};
const context=vm.createContext({document:{getElementById:node},remoteRelayBoard:{value:'1'},remoteRelayCards:[null,card],remoteRelayBusy:false,remoteRelayRevision:0,relayPairActive:false,URLSearchParams,
 confirm:text=>{assert.match(text,/Relais|Pumpen/);return allow;},setTimeout:fn=>{timer=fn;},
 request:async(url,options)=>{requests.push({url,options});if(failure)throw failure;return {accepted:true};}});
vm.runInContext(sketch.slice(start,end),context);
(async()=>{
 context.updateRelayReboot(card);assert.equal(node('remote-relay-reboot').disabled,false);
 context.updateRelayReboot({...card,reboot:'unsupported'});assert.match(node('remote-relay-reboot-message').textContent,/Firmware.*aktualisieren/);
 context.updateRelayReboot({...card,reboot:'pending'});assert(node('remote-relay-reboot').disabled);
 context.updateRelayReboot({...card,reboot:'online'});assert.match(node('remote-relay-reboot-message').textContent,/wieder erreichbar/);
 context.updateRelayReboot({...card,online:false,reboot:'uncertain'});assert(node('remote-relay-reboot').disabled);assert.match(node('remote-relay-reboot-message').textContent,/nicht bestätigt/);
 allow=false;await context.rebootSelectedRelay();assert.equal(requests.length,0);
 allow=true;await context.rebootSelectedRelay();assert.equal(requests[0].url,'/api/remote-relay/reboot');assert.equal(requests[0].options.body.get('board'),'1');assert.match(node('remote-relay-reboot-message').textContent,/angenommen/);
 failure={status:409,message:'beschäftigt'};await context.rebootSelectedRelay();assert.match(node('remote-relay-reboot-message').textContent,/nicht angenommen.*beschäftigt/);
 failure=null;await context.rebootMainController();assert.equal(requests.at(-1).url,'/api/reboot');assert(node('controller-reboot').disabled);assert.match(node('controller-reboot-message').textContent,/Neustart angefordert/);
 const count=requests.length;await context.rebootMainController();assert.equal(requests.length,count);timer();assert(!node('controller-reboot').disabled);
 failure=new Error('Verbindung weg');await context.rebootMainController();assert.match(node('controller-reboot-message').textContent,/nicht bestätigt/);assert(!node('controller-reboot').disabled);
 console.log('PASS: Neustartbestätigung, gezielte Erweiterung und unbekannter Status.');
})().catch(error=>{console.error(error);process.exit(1);});
