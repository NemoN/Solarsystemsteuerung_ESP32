const assert=require('node:assert/strict'),fs=require('node:fs'),vm=require('node:vm'),path=require('node:path');
const sketch=fs.readFileSync(path.join(__dirname,'../Backload/DashboardPage.h'),'utf8');
const start=sketch.indexOf('let installedReleaseCatalog=null'),end=sketch.indexOf('async function fetchRemoteRelayReleases(',start);
assert(start>0&&end>start);
class Node {
 constructor(tag='div'){this.tagName=tag.toUpperCase();this.children=[];this._text='';this.hidden=false;}
 get textContent(){return this.children.length?this.children.map(x=>x.textContent).join(''):this._text;}
 set textContent(value){this._text=String(value);this.children=[];}
 appendChild(child){this.children.push(child);return child;}
 replaceChildren(...children){this._text='';this.children=children;}
}
const nodes=new Map();const node=id=>{if(!nodes.has(id))nodes.set(id,new Node());return nodes.get(id);};
let releases=[{tag_name:'v2.6.0',body:'### Haupt-ESP\n- Fehler behoben\n- WLAN ergänzt\n\n### Relaiskarte\nNicht anzeigen',draft:false},{tag_name:'v2.7.0',body:'<script>nicht ausführen</script>',draft:false},{tag_name:'v3.0.0',body:'Entwurf',draft:true}],fail=false;
const cards=[{configured:true,online:true,version:'2.7.0'},{configured:true,online:true,version:''},{configured:true,online:false,version:''}];
const ctx=vm.createContext({document:{getElementById:node,createElement:tag=>new Node(tag)},currentFirmwareVersion:'2.6.0',remoteRelayCards:cards,remoteUpdateBoard:{value:'0'},remoteRelayState:null,AbortController,setTimeout,clearTimeout,
 fetch:async()=>{if(fail)throw Error('offline');return {ok:true,json:async()=>releases};}});
vm.runInContext(sketch.slice(start,end),ctx);
(async()=>{
 ctx.renderInstalledMainRelease();assert.match(node('main-installed-notes').textContent,/geladen/);
 await ctx.loadInstalledReleaseNotes();assert.equal(node('main-installed-version').textContent,'Installierte Firmware: 2.6.0');
 assert.equal(node('main-installed-notes').children[0].tagName,'UL');assert.deepEqual(node('main-installed-notes').children[0].children.map(x=>x.textContent),['Fehler behoben','WLAN ergänzt']);
 assert.equal(node('relay-installed-version').textContent,'Installierte Firmware: 2.7.0');
 assert.equal(node('relay-installed-notes').textContent,'Die Relaiskarte meldet ihre Firmware-Version.');
 ctx.renderInstalledRelayRelease(cards[1]);assert.match(node('relay-installed-version').textContent,/nicht verfügbar/);
 ctx.renderInstalledRelayRelease(cards[2]);assert.match(node('relay-installed-version').textContent,/nicht erreichbar/);
 ctx.currentFirmwareVersion='3.0.0';ctx.renderInstalledMainRelease();assert.match(node('main-installed-notes').textContent,/keine Release Notes/);
 ctx.renderSelectedReleaseNotes('main',{tag:'v2.9.1',body:'### Haupt-ESP\n- Hauptänderung\n\n### Relaiskarte\nRelais-Hinweis'});
 assert.equal(node('main-selected-notes').children[0].tagName,'UL');assert.equal(node('main-selected-notes').children[0].children[0].textContent,'Hauptänderung');
 ctx.renderSelectedReleaseNotes('main',{tag:'v2.9.0',body:'- Alte gemeinsame Hinweise'});
 assert.match(node('main-selected-notes').textContent,/keine eigenen Release Notes/);
 ctx.renderSelectedReleaseNotes('main',{tag:'v3.1.0',body:'### Relaiskarte\nNicht anzeigen'});
 assert.match(node('main-selected-notes').textContent,/keine eigenen Release Notes/);
 ctx.renderSelectedReleaseNotes('main',{tag:'v3.2.0',body:'## Haupt-ESP\r\nLetzte Änderung'});
 assert.equal(node('main-selected-notes').textContent,'Letzte Änderung');
 ctx.renderSelectedReleaseNotes('relay',{tag:'v2.9.0',body:'- Änderung am Haupt-ESP'});
 assert.equal(node('relay-selected-notes-wrap').hidden,false);assert.match(node('relay-selected-notes').textContent,/unverändert zur Vorversion/);
 ctx.renderSelectedReleaseNotes('relay',{tag:'v4.0.0',body:'## Haupt-ESP\n- Nur Haupt-ESP\n\n### Relaiskarte\n- Neue Relaisfunktion\n<script>Text</script>\n\n## Sonstiges\n- Andere Änderung'});
 assert.equal(node('relay-selected-notes').children[0].tagName,'UL');assert.equal(node('relay-selected-notes').children[1].textContent,'<script>Text</script>');assert.equal(node('relay-selected-notes').children[1].tagName,'P');
 ctx.renderSelectedReleaseNotes('relay',{tag:'v4.1.0',body:'- Nur Haupt-ESP'});assert.match(node('relay-selected-notes').textContent,/keine eigenen Release Notes/);
 ctx.renderSelectedReleaseNotes('relay',{tag:'v4.2.0',body:'### Relaiskarte\nUnverändert zur Vorversion.'});assert.equal(node('relay-selected-notes').textContent,'Unverändert zur Vorversion.');
 const rendered=new Node();ctx.renderReleaseNotes(rendered,'Einleitung\n\n* Erster Punkt\n  Fortsetzung\n+ Zweiter Punkt\n\n1. Erster Schritt\n2. Zweiter Schritt\n\nEnde');
 assert.deepEqual(rendered.children.map(x=>x.tagName),['P','UL','OL','P']);
 assert.equal(rendered.children[1].children[0].textContent,'Erster Punkt\nFortsetzung');assert.equal(rendered.children[2].children.length,2);
 const oldChildren=rendered.children;ctx.renderReleaseNotes(rendered,'Einleitung\n\n* Erster Punkt\n  Fortsetzung\n+ Zweiter Punkt\n\n1. Erster Schritt\n2. Zweiter Schritt\n\nEnde');assert.equal(rendered.children,oldChildren);
 ctx.renderSelectedReleaseNotes('relay',null);assert.equal(node('relay-selected-notes-wrap').hidden,true);
 fail=true;await ctx.loadInstalledReleaseNotes();assert.match(node('main-installed-notes').textContent,/nicht verfügbar/);
 console.log('PASS: Release Notes installierter und ausgewählter Firmware, ältere Karten und GitHub-Ausfall.');
})().catch(e=>{console.error(e);process.exit(1);});
