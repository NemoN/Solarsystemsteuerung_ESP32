const assert=require('node:assert/strict'),fs=require('node:fs'),vm=require('node:vm'),path=require('node:path');
const sketch=fs.readFileSync(path.join(__dirname,'../Backload/Backload.ino'),'utf8');
class Node {
 constructor(){this.children=[];this.dataset={};this.attributes={};this.textContent='';this.disabled=false;this.removed=false;}
 appendChild(child){this.children.push(child);return child;}
 append(...children){this.children.push(...children);}
 setAttribute(name,value){this.attributes[name]=value;}
 remove(){this.removed=true;}
}
const groups=new Node(),extraCards=new Map();
const start=sketch.indexOf('function updateExtraRelays('),end=sketch.indexOf('function updateRelayDiscovery(',start);
assert(start>0&&end>start);
const ctx=vm.createContext({document:{createElement:()=>new Node()},remoteExtraCards:extraCards,remoteExtraGroups:groups,remoteRelayBusy:false});
vm.runInContext(sketch.slice(start,end),ctx);
const primary={configured:true,online:true,mask:0,labels:['A','B','C','D']};
const secondary={configured:true,online:true,busy:false,updating:false,mask:2,labels:['Licht','Pumpe','Tor','Reserve']};
const offline={configured:true,online:false,busy:false,updating:false,mask:15,labels:['A','B','C','D']};
ctx.updateExtraRelays([primary,secondary,offline],false);
assert.equal(extraCards.size,2);assert.equal(extraCards.get(1).buttons[1].label.textContent,'Pumpe · EIN');
assert.equal(extraCards.get(1).buttons[1].button.dataset.board,'1');assert.equal(extraCards.get(1).buttons[1].button.attributes['aria-checked'],'true');
assert.equal(extraCards.get(2).buttons[0].button.disabled,true);assert.match(extraCards.get(2).status.textContent,/unbekannt/);
ctx.updateExtraRelays([primary,secondary,offline],true);assert(extraCards.get(1).buttons.every(x=>!x.button.disabled));
ctx.updateExtraRelays([],false);assert(extraCards.get(1).buttons.every(x=>x.button.disabled));assert.match(extraCards.get(1).status.textContent,/Keine aktuellen Daten/);
ctx.updateExtraRelays([primary,{...secondary,configured:false},offline],false);assert(!extraCards.has(1));
const switchStart=sketch.indexOf('async function switchRemoteRelay('),switchEnd=sketch.indexOf("document.getElementById('remote-relay-switches')",switchStart);
assert(switchStart>0&&switchEnd>switchStart);let posted;
const primaryState={online:true,mask:0},state3={online:true,mask:4};
const c=vm.createContext({remoteRelayCards:[primaryState,secondary,state3],remoteRelayState:primaryState,remoteRelayBusy:false,remoteRelayRevision:0,remoteRelayMessage:{},pendingRemoteAction:null,
 updateRemoteRelay(){},updateExtraRelays(){},URLSearchParams,request:async(url,options)=>{posted={url,body:options.body};return {};}});
vm.runInContext(sketch.slice(switchStart,switchEnd),c);
const scanStart=sketch.indexOf("relayPairScan.addEventListener('click'");const scanEnd=sketch.indexOf("relayPairForm.addEventListener('submit'",scanStart);
assert(scanStart>0&&scanEnd>scanStart);let scanClick,scanCalls=[];
const scanSelect={value:'',options:[],replaceChildren(option){this.options=[option];},add(option){this.options.push(option);}};
const scanButton={disabled:false,addEventListener:(_,fn)=>{scanClick=fn;}},scanMessage={},scanForm={elements:{ssid:scanSelect},querySelector:()=>({})};
const scanCtx=vm.createContext({relayPairScan:scanButton,relayPairScanMessage:scanMessage,relayPairForm:scanForm,relayPairScanBusy:false,relayPairActive:false,wifiBusy:false,wifiScanning:false,
  setTimeout:fn=>fn(),Option:function(label,value){this.label=label;this.value=value;},request:async(url)=>{
    scanCalls.push(url);return url==='/api/wifi/scan'?{state:'scanning'}:{state:'complete',networks:[{ssid:'Relaiserweiterung-ABCD',open:false},{ssid:'Relaiserweiterung-ABCD',open:false},{ssid:'Fremdnetz',open:false},{ssid:'Relaiserweiterung-1234',open:true}]};
  }});
vm.runInContext(sketch.slice(scanStart,scanEnd),scanCtx);
const pairStart=sketch.indexOf("relayPairForm.addEventListener('submit'");const pairEnd=sketch.indexOf("remoteRelayBoard.addEventListener('change'",pairStart);
assert(pairStart>0&&pairEnd>pairStart);let pairSubmit,pairPosted;
const pairForm={elements:{ssid:{value:'Relaiserweiterung-ABCD'}},querySelector:()=>({}),addEventListener:(_,fn)=>{pairSubmit=fn;}};
const pairCtx=vm.createContext({relayPairForm:pairForm,relayPairActive:false,relayPairMessage:{},relayPairScan:{},relayPairCancel:{addEventListener(){}},remoteRelayDirty:false,confirm:()=>true,remoteRelayBoard:{value:'2'},URLSearchParams,
 request:async(url,opts)=>{pairPosted={url,body:opts.body};return {started:true};}});
vm.runInContext(sketch.slice(pairStart,pairEnd),pairCtx);
(async()=>{
 await c.switchRemoteRelay({target:{closest:()=>({dataset:{board:'2',relay:'3'},disabled:false})}});
 assert.equal(posted.url,'/api/remote-relay/command');assert.equal(posted.body.get('board'),'2');assert.equal(posted.body.get('id'),'3');assert.equal(posted.body.get('value'),'0');
 assert.equal(c.pendingRemoteAction.board,2);
 await scanClick();assert.deepEqual(scanCalls,['/api/wifi/scan','/api/remote-relay/pair/scan']);
 assert.equal(scanSelect.options.length,2);assert.equal(scanSelect.options[1].value,'Relaiserweiterung-ABCD');
 await pairSubmit({preventDefault(){}});assert.equal(pairPosted.url,'/api/remote-relay/pair');assert.equal(pairPosted.body.get('board'),'2');
 assert.equal(pairPosted.body.get('ssid'),'Relaiserweiterung-ABCD');assert.equal(pairPosted.body.has('password'),false);
 console.log('PASS: Mehrere Erweiterungen, Offline-Anzeige, gezielte Schaltaufträge und Kopplungsformular.');
})().catch(e=>{console.error(e);process.exit(1);});
