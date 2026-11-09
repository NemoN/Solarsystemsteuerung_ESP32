const assert=require('node:assert/strict'),fs=require('node:fs'),vm=require('node:vm'),path=require('node:path');
const sketch=fs.readFileSync(path.join(__dirname,'../Backload/Backload.ino'),'utf8');
const start=sketch.indexOf('let installedReleaseCatalog=null'),end=sketch.indexOf('async function fetchRemoteRelayReleases(',start);
assert(start>0&&end>start);
const nodes=new Map();const node=id=>{if(!nodes.has(id))nodes.set(id,{textContent:'',hidden:false});return nodes.get(id);};
let releases=[{tag_name:'v2.6.0',body:'- Fehler behoben\n- WLAN ergänzt',draft:false},{tag_name:'v2.7.0',body:'<script>nicht ausführen</script>',draft:false},{tag_name:'v3.0.0',body:'Entwurf',draft:true}],fail=false;
const cards=[{configured:true,online:true,version:'2.7.0'},{configured:true,online:true,version:''},{configured:true,online:false,version:''}];
const ctx=vm.createContext({document:{getElementById:node},currentFirmwareVersion:'2.6.0',remoteRelayCards:cards,remoteUpdateBoard:{value:'0'},remoteRelayState:null,AbortController,setTimeout,clearTimeout,
 fetch:async()=>{if(fail)throw Error('offline');return {ok:true,json:async()=>releases};}});
vm.runInContext(sketch.slice(start,end),ctx);
(async()=>{
 ctx.renderInstalledMainRelease();assert.match(node('main-installed-notes').textContent,/geladen/);
 await ctx.loadInstalledReleaseNotes();assert.equal(node('main-installed-version').textContent,'Installierte Firmware: 2.6.0');
 assert.equal(node('main-installed-notes').textContent,'- Fehler behoben\n- WLAN ergänzt');
 assert.equal(node('relay-installed-version').textContent,'Installierte Firmware: 2.7.0');
 assert.equal(node('relay-installed-notes').textContent,'<script>nicht ausführen</script>');
 ctx.renderInstalledRelayRelease(cards[1]);assert.match(node('relay-installed-version').textContent,/nicht verfügbar/);
 ctx.renderInstalledRelayRelease(cards[2]);assert.match(node('relay-installed-version').textContent,/nicht erreichbar/);
 ctx.currentFirmwareVersion='3.0.0';ctx.renderInstalledMainRelease();assert.match(node('main-installed-notes').textContent,/keine Release Notes/);
 ctx.renderSelectedReleaseNotes('relay',{body:'- Neue Funktion'});assert.equal(node('relay-selected-notes-wrap').hidden,false);assert.equal(node('relay-selected-notes').textContent,'- Neue Funktion');
 ctx.renderSelectedReleaseNotes('relay',null);assert.equal(node('relay-selected-notes-wrap').hidden,true);
 fail=true;await ctx.loadInstalledReleaseNotes();assert.match(node('main-installed-notes').textContent,/nicht verfügbar/);
 console.log('PASS: Release Notes installierter und ausgewählter Firmware, ältere Karten und GitHub-Ausfall.');
})().catch(e=>{console.error(e);process.exit(1);});
