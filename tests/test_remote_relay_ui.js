const assert=require('node:assert/strict');
const fs=require('node:fs'),vm=require('node:vm'),path=require('node:path');
const sketch=fs.readFileSync(path.join(__dirname,'../Backload/DashboardPage.h'),'utf8');
const calls={};
const labels=Array.from({length:4},()=>({textContent:''}));
const buttons=labels.map((label,i)=>({dataset:{relay:String(i+1)},disabled:true,attributes:{},setAttribute(k,v){this.attributes[k]=v;},querySelector(){return label;}}));
const nodes={
 'remote-relay-status':{},'remote-relay-detail':{},
 'relay-installed-version':{},'relay-installed-notes':{replaceChildren(){}},'remote-update-availability':{},'relay-diagnostics':{},'relay-diagnostics-table':{},
 'relay-diag-uptime':{},'relay-diag-rssi':{},'relay-diag-reset':{},'relay-diag-watchdog':{},'relay-diag-restarts':{},'relay-diag-ota':{},'relay-diag-storage':{},
 'remote-relay-switches':{addEventListener(type,fn){calls.click=fn;}},
};
const form={elements:{address:{value:''},key:{value:''},label1:{value:''},label2:{value:''},label3:{value:''},label4:{value:''}},
 contains:()=>false,addEventListener(type,fn){calls[type]=fn;}};
const selectStub=()=>({value:'0',dataset:{},addEventListener(){},replaceChildren(){},add(){}});
const remoteRelayBoard=selectStub(),remoteUpdateBoard=selectStub(),relayDiscoverySelect=selectStub();
const remoteRelayForget={addEventListener(type,fn){calls.forget=fn;}},remoteRelayForgetMessage={},remoteRelaySlotStatus={};
const remoteRelayDiscard={addEventListener(type,fn){calls.discard=fn;}},relayForgetRecovery={};
const remoteExtraGroups={addEventListener(){},appendChild(){}};
const relayDiscoverySearch={addEventListener(){}},relayDiscoveryMessage={};
const relayPairForm={elements:{ssid:selectStub(),password:{value:''}},querySelector:()=>({}),addEventListener(){}};
const relayPairScan={addEventListener(){}},relayPairCancel={addEventListener(){}},relayPairScanMessage={},relayPairMessage={};
const remoteReleaseCheck={addEventListener(){}},remoteReleaseMessage={};
const remoteSelect={value:'',addEventListener(){}};
const githubButton={disabled:true};
const remoteGithubForm={elements:{release:remoteSelect},querySelector:()=>githubButton,addEventListener(){}};
const updateButton={disabled:true};
const updateForm={elements:{firmware:{files:[]}},querySelector:()=>updateButton,addEventListener(type,fn){calls.update=fn;}};
let posted=null;
const context=vm.createContext({document:{getElementById:id=>nodes[id],createElement:tag=>({tagName:tag.toUpperCase(),textContent:'',appendChild(){}}),querySelectorAll:()=>buttons,activeElement:null},
 remoteReleaseCheck,remoteReleaseMessage,remoteGithubForm,remoteRelaySearchBusy:false,remoteRelayReleases:[],simulationEnabled:false,
 remoteRelayBoard,remoteUpdateBoard,remoteRelayForget,remoteRelayForgetMessage,remoteRelaySlotStatus,remoteRelayDiscard,relayForgetRecovery,remoteExtraGroups,relayDiscoverySelect,relayDiscoverySearch,relayDiscoveryMessage,
 relayPairForm,relayPairScan,relayPairCancel,relayPairScanMessage,relayPairMessage,relayPairActive:false,relayPairScanBusy:false,
 remoteRelayCards:[],relayDiscoverySignature:'',remoteExtraCards:new Map(),wifiBusy:false,wifiScanning:false,confirm:()=>true,
 remoteRelayUpdateForm:updateForm,remoteRelayUpdateMessage:{},remoteRelayUpdateProgress:{},remoteRelayUploadBusy:false,
 updateRelayReboot:()=>{},renderRelaySettingsCards:()=>{},remoteRelayForm:form,remoteRelayMessage:{},remoteRelayConfigMessage:{},remoteRelayDirty:false,remoteRelayBusy:false,remoteRelayRevision:0,remoteRelayState:null,pendingRemoteAction:null,URLSearchParams,
 request:async(url,options)=>{posted={url,values:options.body};return {accepted:true};}});
const start=sketch.indexOf('function updateRemoteRelay(state,simulation)');
const end=sketch.indexOf('async function request(url,options={})',start);
assert(start>0&&end>start);vm.runInContext(sketch.slice(start,end),context);
const state={ready:true,configured:true,online:true,busy:false,address:'192.168.1.2',mask:4,labels:['Licht','Ventil','Pumpe','Reserve']};
context.remoteRelayCards=[state];context.updateRemoteRelay(state,false);
assert.match(nodes['relay-diagnostics'].textContent,/noch nicht verfügbar/);
assert.equal(nodes['relay-diagnostics-table'].hidden,true);
context.remoteRelayCards=[{...state,diagnostics:{uptime:90061,rssi:-62,resetReason:3,watchdogState:1,watchdogRestarts:2,otaError:1,storageHealthy:false}}];context.updateRemoteRelay(state,false);
assert.equal(nodes['relay-diagnostics'].hidden,true);assert.equal(nodes['relay-diagnostics-table'].hidden,false);
assert.equal(nodes['relay-diag-uptime'].textContent,'1 T 1 Std 1 Min');assert.equal(nodes['relay-diag-rssi'].textContent,'-62 dBm');assert.match(nodes['relay-diag-watchdog'].textContent,/Neustartbegrenzung erreicht/);assert.equal(nodes['relay-diag-restarts'].textContent,'2 / 2');assert.match(nodes['relay-diag-ota'].textContent,/Zeitüberschreitung/);assert.equal(nodes['relay-diag-storage'].textContent,'Fehler');
context.remoteRelayCards=[state];
assert.equal(nodes['remote-relay-status'].textContent,'Verbunden');
assert.equal(labels[2].textContent,'Pumpe · EIN');
assert.equal(buttons[2].attributes['aria-checked'],'true');
assert.equal(buttons[0].disabled,false);
context.updateRemoteRelay(state,true);
assert.equal(buttons[0].disabled,false);
assert.match(nodes['remote-relay-detail'].textContent,/Testmodus/);
remoteSelect.value='0';context.updateRemoteRelay(state,true);
assert.equal(githubButton.disabled,false);assert.equal(updateButton.disabled,false);
assert.equal(nodes['remote-update-availability'].textContent,'');
context.updateRemoteRelay(state,false);
assert.equal(githubButton.disabled,false);assert.equal(updateButton.disabled,false);
assert.equal(nodes['remote-update-availability'].textContent,'');
context.remoteRelayCards=[{...state,online:false}];context.updateRemoteRelay(context.remoteRelayCards[0],false);
assert.equal(nodes['relay-diagnostics-table'].hidden,true);assert.match(nodes['relay-diagnostics'].textContent,/nicht erreichbar/);
assert.equal(buttons[2].disabled,true);
assert.equal(githubButton.disabled,true);assert.equal(updateButton.disabled,true);
assert.match(nodes['remote-update-availability'].textContent,/nicht erreichbar/);
assert.match(labels[2].textContent,/unbekannt/);
context.remoteRelayCards=[state];context.updateRemoteRelay(state,true);
(async()=>{
 await calls.click({target:{closest:()=>buttons[0]}});
 assert.equal(posted.url,'/api/remote-relay/command');
 assert.equal(posted.values.get('id'),'1');assert.equal(posted.values.get('value'),'1');
 context.remoteRelayCards=[{...state,mask:5,busy:false}];context.updateRemoteRelay(context.remoteRelayCards[0],false);
 assert.equal(context.remoteRelayMessage.textContent,'Relaiszustand bestätigt.');
 context.remoteRelayCards=[state];context.updateRemoteRelay(state,true);
 await calls.forget();assert.equal(posted.url,'/api/remote-relay/forget');assert.equal(posted.values.get('board'),'0');
 context.remoteRelayCards=[{...state,busy:true,forget:'pending'}];context.updateRemoteRelay(state,true);
 assert.equal(remoteRelayForget.disabled,true);assert.match(remoteRelayForgetMessage.textContent,/zurückgesetzt/);
 context.remoteRelayCards=[{...state,online:false,forget:'uncertain'}];context.updateRemoteRelay(state,true);
 assert.match(remoteRelayForgetMessage.textContent,/nicht bestätigt/);assert.equal(remoteRelayForget.disabled,true);
 await calls.discard();assert.equal(posted.url,'/api/remote-relay/discard');
 context.remoteRelayCards=[{...state,configured:false,online:false,forget:'done'}];context.updateRemoteRelay(state,true);
 assert.equal(remoteRelayForget.hidden,true);assert.match(remoteRelayForgetMessage.textContent,/vergessen/);
 context.remoteRelayCards=[];
 context.updateRemoteRelay(null,false);
 assert.equal(buttons[0].disabled,true);
 assert.equal(nodes['remote-relay-status'].textContent,'Status unbekannt');
 console.log('PASS: WLAN-Relais, Offline- und Testmodus, Schaltauftrag und bestätigter Zustand.');
})().catch(error=>{console.error(error);process.exit(1);});
