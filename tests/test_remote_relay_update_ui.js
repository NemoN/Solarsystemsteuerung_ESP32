const assert=require('node:assert/strict'),fs=require('node:fs'),vm=require('node:vm'),path=require('node:path');
const sketch=fs.readFileSync(path.join(__dirname,'../Backload/Backload.ino'),'utf8');
const start=sketch.indexOf("remoteRelayUpdateForm.addEventListener('submit'");
const end=sketch.indexOf('async function request(url,options={})',start);
assert(start>0&&end>start);
let submit;const button={disabled:false},message={},progress={};
const data=Uint8Array.from({length:5000},(_,i)=>i%256);
const file={size:data.length,slice:(a,b)=>({arrayBuffer:async()=>data.slice(a,b).buffer})};
const form={elements:{firmware:{files:[file]}},querySelector:()=>button,addEventListener:(_,fn)=>{submit=fn;}};
const calls=[];let chunks=0,starts=0,statuses=0;
async function request(url,opts){
 const name=url.split('/').pop(),params=opts.body;calls.push(name);
 if(name==='prepare'){assert.equal(params.get('board'),'3');return {token:'token'};}
 if(name==='chunk'){
  chunks++;if(chunks===1)throw new Error('Verbindung getrennt');
  const offset=Number(params.get('offset'));const size=Buffer.from(params.get('data'),'base64').length;
  return {received:offset+size};
 }
 if(name==='start'){starts++;if(starts===1)throw new Error('Antwort verloren');return {started:true};}
 if(name==='status'){
  statuses++;if(statuses===1)return {state:'receiving',size:5000,sent:0};
  if(statuses===2)return {state:'sending',size:5000,sent:2048};
  return {state:'done',size:5000,sent:5000};
 }
 throw new Error('Unerwarteter Aufruf '+name);
}
const ctx=vm.createContext({remoteRelayUpdateForm:form,remoteRelayUpdateMessage:message,remoteRelayUpdateProgress:progress,remoteRelayUploadBusy:false,
 remoteUpdateBoard:{value:'3'},confirm:()=>true,request,URLSearchParams,Uint8Array,Array,Date,btoa:s=>Buffer.from(s,'binary').toString('base64'),setTimeout:fn=>fn()});
vm.runInContext(sketch.slice(start,end),ctx);
(async()=>{
 await submit({preventDefault(){}});
 assert.equal(chunks,3);assert.equal(starts,2);assert.equal(statuses,3);
 assert.equal(progress.value,100);assert.match(message.textContent,/Startprüfung.*bestätigt/);
 assert(!calls.includes('abort'));
 console.log('PASS: Relais-Update-Oberfläche, Wiederholungen und verlorene Startbestätigung.');
})().catch(error=>{console.error(error);process.exit(1);});
