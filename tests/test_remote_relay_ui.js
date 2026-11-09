const assert=require('node:assert/strict');
const fs=require('node:fs'),vm=require('node:vm'),path=require('node:path');
const sketch=fs.readFileSync(path.join(__dirname,'../Backload/Backload.ino'),'utf8');
const calls={};
const labels=Array.from({length:4},()=>({textContent:''}));
const buttons=labels.map((label,i)=>({dataset:{relay:String(i+1)},disabled:true,attributes:{},setAttribute(k,v){this.attributes[k]=v;},querySelector(){return label;}}));
const nodes={
 'remote-relay-status':{},'remote-relay-detail':{},
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
const remoteGithubForm={elements:{release:remoteSelect},querySelector:()=>({}),addEventListener(){}};
const updateButton={disabled:true};
const updateForm={elements:{firmware:{files:[]}},querySelector:()=>updateButton,addEventListener(type,fn){calls.update=fn;}};
let posted=null;
const context=vm.createContext({document:{getElementById:id=>nodes[id],querySelectorAll:()=>buttons,activeElement:null},
 remoteReleaseCheck,remoteReleaseMessage,remoteGithubForm,remoteRelaySearchBusy:false,remoteRelayReleases:[],simulationEnabled:false,
 remoteRelayBoard,remoteUpdateBoard,remoteRelayForget,remoteRelayForgetMessage,remoteRelaySlotStatus,remoteRelayDiscard,relayForgetRecovery,remoteExtraGroups,relayDiscoverySelect,relayDiscoverySearch,relayDiscoveryMessage,
 relayPairForm,relayPairScan,relayPairCancel,relayPairScanMessage,relayPairMessage,relayPairActive:false,relayPairScanBusy:false,
 remoteRelayCards:[],relayDiscoverySignature:'',remoteExtraCards:new Map(),wifiBusy:false,wifiScanning:false,confirm:()=>true,
 remoteRelayUpdateForm:updateForm,remoteRelayUpdateMessage:{},remoteRelayUpdateProgress:{},remoteRelayUploadBusy:false,
 remoteRelayForm:form,remoteRelayMessage:{},remoteRelayConfigMessage:{},remoteRelayDirty:false,remoteRelayBusy:false,remoteRelayRevision:0,remoteRelayState:null,pendingRemoteAction:null,URLSearchParams,
 request:async(url,options)=>{posted={url,values:options.body};return {accepted:true};}});
const start=sketch.indexOf('function updateRemoteRelay(state,simulation)');
const end=sketch.indexOf('async function request(url,options={})',start);
assert(start>0&&end>start);vm.runInContext(sketch.slice(start,end),context);
const state={ready:true,configured:true,online:true,busy:false,address:'192.168.1.2',mask:4,labels:['Licht','Ventil','Pumpe','Reserve']};
context.updateRemoteRelay(state,false);
assert.equal(nodes['remote-relay-status'].textContent,'Verbunden');
assert.equal(labels[2].textContent,'Pumpe · EIN');
assert.equal(buttons[2].attributes['aria-checked'],'true');
assert.equal(buttons[0].disabled,false);
context.updateRemoteRelay(state,true);
assert.equal(buttons[0].disabled,false);
assert.match(nodes['remote-relay-detail'].textContent,/Testmodus/);
context.updateRemoteRelay({...state,online:false},false);
assert.equal(buttons[2].disabled,true);
assert.match(labels[2].textContent,/unbekannt/);
context.updateRemoteRelay(state,true);
(async()=>{
 await calls.click({target:{closest:()=>buttons[0]}});
 assert.equal(posted.url,'/api/remote-relay/command');
 assert.equal(posted.values.get('id'),'1');assert.equal(posted.values.get('value'),'1');
 context.updateRemoteRelay({...state,mask:5,busy:false},false);
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
