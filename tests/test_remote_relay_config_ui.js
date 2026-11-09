const assert=require('node:assert/strict'),fs=require('node:fs'),vm=require('node:vm'),path=require('node:path');
const sketch=fs.readFileSync(path.join(__dirname,'../Backload/Backload.ino'),'utf8');
const start=sketch.indexOf('let remoteRelayEditRevision=0,remoteRelayConfigTimer=0;');
const end=sketch.indexOf("remoteRelayForget.addEventListener('click'",start);
assert(start>0&&end>start);
const callbacks={}, fields={address:{value:'192.168.1.20'},key:{value:''}};
for(let i=1;i<=4;i++)fields['label'+i]={value:'Relais '+i};
const form={elements:fields,addEventListener:(type,fn)=>{callbacks[type]=fn;}};
const board={value:'0'},message={textContent:''},timers=new Map();let nextTimer=1,requestImpl;
const context=vm.createContext({remoteRelayForm:form,remoteRelayBoard:board,remoteRelayConfigMessage:message,
 remoteRelayBusy:false,remoteRelayRevision:0,remoteRelayDirty:false,URLSearchParams,
 setTimeout:(fn,ms)=>{assert.equal(ms,4000);const id=nextTimer++;timers.set(id,fn);return id;},
 clearTimeout:id=>timers.delete(id),request:(url,options)=>requestImpl(url,options)});
vm.runInContext(sketch.slice(start,end),context);
const submit=()=>callbacks.submit({preventDefault(){}});
(async()=>{
 requestImpl=async(url,options)=>{assert.equal(url,'/api/remote-relay/config');assert.equal(options.body.get('label1'),'Relais 1');return {saved:true};};
 callbacks.input();await submit();assert.equal(message.textContent,'Gespeichert.');assert.equal(context.remoteRelayDirty,false);
 assert.equal(timers.size,1);[...timers.values()][0]();assert.equal(message.textContent,'');timers.clear();
 let resolve;requestImpl=()=>new Promise(r=>{resolve=r;});callbacks.input();const pending=submit();
 fields.label1.value='Nachträgliche Änderung';callbacks.input();resolve({saved:true});await pending;
 assert.match(message.textContent,/Weitere Änderungen/);assert.equal(context.remoteRelayDirty,true);assert.equal(timers.size,0);
 requestImpl=async()=>{const e=new Error('Server abgelehnt');e.status=400;throw e;};await submit();assert.equal(message.textContent,'Nicht gespeichert: Server abgelehnt');assert.equal(timers.size,0);
 requestImpl=async()=>{throw new Error('Netzwerk');};await submit();assert.match(message.textContent,/nicht bestätigt/);
 let finish;requestImpl=()=>new Promise(r=>{finish=r;});const otherBoard=submit();board.value='1';finish({saved:true});await otherBoard;
 assert.notEqual(message.textContent,'Gespeichert.');
 console.log('PASS: Relaisnamen-Speicherung, automatische Rückmeldung und Änderungen während der Übertragung.');
})().catch(error=>{console.error(error);process.exit(1);});
