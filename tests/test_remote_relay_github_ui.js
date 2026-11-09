const assert=require('node:assert/strict'),fs=require('node:fs'),vm=require('node:vm'),path=require('node:path');
const sketch=fs.readFileSync(path.join(__dirname,'../Backload/DashboardPage.h'),'utf8');
const css=fs.readFileSync(path.join(__dirname,'../Backload/DashboardStyles.h'),'utf8');
const main=sketch.slice(sketch.indexOf('<section id="remote-relay-section"'),sketch.indexOf('<section id="remote-relay-settings"'));
assert(main.includes('id="remote-relay-switches"'));
assert(!main.includes('id="remote-relay-form"')&&!main.includes('id="remote-relay-update-form"'));
assert(sketch.includes('<a href="#remote-relay-settings">')&&sketch.includes('<a href="#remote-relay-update-section">'));
assert(css.includes('body.settings-page .backload #remote-relay-settings')&&css.includes('body.settings-page .backload #remote-relay-update-section'));
const start=sketch.indexOf('async function fetchRemoteRelayReleases(');
const end=sketch.indexOf("remoteReleaseCheck.addEventListener('click'",start);
assert(start>0&&end>start);
const pollStart=sketch.indexOf('async function pollRemoteRelayGithubUpdate(');
const pollEnd=sketch.indexOf("remoteGithubForm.addEventListener('submit'",pollStart);
assert(pollStart>0&&pollEnd>pollStart);
const ctx=vm.createContext({setTimeout:fn=>fn(),Date});
vm.runInContext(sketch.slice(start,end)+sketch.slice(pollStart,pollEnd),ctx);
const digest='sha256:'+'a'.repeat(64),prefix='https://github.com/NemoN/Solarsystemsteuerung_ESP32/releases/download/';
function release(tag,extra={}){return {tag_name:tag,name:tag,published_at:'2026-11-08T10:00:00Z',draft:false,prerelease:false,assets:[{name:'Relaiskarte.ino.bin',size:700000,digest,browser_download_url:prefix+tag+'/Relaiskarte.ino.bin'}],...extra};}
(async()=>{
 const badHash=release('v1');badHash.assets[0].digest=null;
 const wrong=release('v2');wrong.assets[0].browser_download_url='https://example.org/Relaiskarte.ino.bin';
 const oversized=release('v3');oversized.assets[0].size=1310721;
 const data=[release('v2.5.0',{body:'- Relais-Firmware verbessert'}),release('v2.4.0',{draft:true}),badHash,wrong,oversized,release('../escape')];
 const result=await ctx.fetchRemoteRelayReleases(async()=>({ok:true,json:async()=>data}));
 assert.equal(result.length,1);assert.equal(result[0].tag,'v2.5.0');assert.equal(result[0].body,'- Relais-Firmware verbessert');
 const changeStart=sketch.indexOf("remoteGithubForm.elements.release.addEventListener('change'");
 const changeEnd=sketch.indexOf('async function pollRemoteRelayGithubUpdate(',changeStart);
 let onChange,shown;
 const changeForm={elements:{release:{value:'0',addEventListener:(_,fn)=>{onChange=fn;}}}};
 const changeCtx=vm.createContext({remoteGithubForm:changeForm,remoteRelayReleases:result,remoteRelayState:{},simulationEnabled:false,
   renderSelectedReleaseNotes:(prefix,release)=>{shown={prefix,release};},updateRemoteRelay:()=>{}});
 vm.runInContext(sketch.slice(changeStart,changeEnd),changeCtx);onChange();
 assert.equal(shown.prefix,'relay');assert.equal(shown.release.body,'- Relais-Firmware verbessert');
 changeForm.elements.release.value='';onChange();assert.equal(shown.release,null);
 assert.equal((await ctx.fetchRemoteRelayReleases(async()=>({ok:true,json:async()=>[]}))).length,0);
 await assert.rejects(()=>ctx.fetchRemoteRelayReleases(async()=>({ok:false,status:403})),/403/);
 let count=0;const progress={},message={};
 await ctx.pollRemoteRelayGithubUpdate(async()=>{
  count++;if(count===1)throw new Error('kurz offline');
  if(count===2)return {state:'downloading',received:50,sent:0,size:100};
  if(count===3)return {state:'sending',received:100,sent:70,size:100};
  return {state:'done',received:100,sent:100,size:100};
 },'token',progress,message);
 assert.equal(progress.value,100);assert.match(message.textContent,/Startprüfung.*bestätigt/);
 await assert.rejects(()=>ctx.pollRemoteRelayGithubUpdate(async()=>({state:'failed',error:'SHA-256-Fehler'}),'token',{},{}),/SHA-256/);
 let submit,startedPayload=null,phase=0;const updateMessage={},updateProgress={};
 const ghForm={elements:{release:{value:'0'}},querySelector:()=>({}),addEventListener:(_,fn)=>{submit=fn;}};
 const fileForm={querySelector:()=>({})};
 const submitEnd=sketch.indexOf("remoteRelayUpdateForm.addEventListener('submit'",pollEnd);
 assert(submitEnd>pollEnd);
 const submitCtx=vm.createContext({remoteGithubForm:ghForm,remoteRelayUpdateForm:fileForm,remoteRelayReleases:result,
   remoteRelayUploadBusy:false,remoteRelayState:{online:true},remoteRelayCards:[{online:false},{online:false},{online:true}],remoteUpdateBoard:{value:'2'},simulationEnabled:true,
   remoteRelayUpdateProgress:updateProgress,remoteRelayUpdateMessage:updateMessage,URLSearchParams,Date,
   confirm:()=>true,setTimeout:fn=>fn(),request:async(url,options)=>{
     const kind=url.split('/').pop();if(kind==='github'){startedPayload=options.body;return {token:'t'};}
     if(kind==='status'){phase++;return phase===1?{state:'downloading',received:30,sent:0,size:100}:{state:'done',size:100,sent:100};}
     throw new Error('Unerwarteter Aufruf: '+kind);
   }});
 vm.runInContext(sketch.slice(pollStart,submitEnd),submitCtx);
 await submit({preventDefault(){}});
 assert.equal(startedPayload.get('url'),prefix+'v2.5.0/Relaiskarte.ino.bin');
 assert.equal(startedPayload.get('sha256'),'a'.repeat(64));assert.equal(startedPayload.get('size'),'700000');assert.equal(startedPayload.get('board'),'2');
 assert.match(updateMessage.textContent,/Startprüfung.*bestätigt/);
 console.log('PASS: Einstellungen, Relais-Releases, URL- und Prüfsummenfilter sowie GitHub-Update.');
})().catch(e=>{console.error(e);process.exit(1);});
