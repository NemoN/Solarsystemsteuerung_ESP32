const assert=require('node:assert/strict'),fs=require('node:fs'),vm=require('node:vm'),path=require('node:path');
const source=fs.readFileSync(path.join(__dirname,'../Backload/DashboardPage.h'),'utf8');
const start=source.indexOf('function relaySettingsBusy()'),end=source.indexOf('function updateRemoteRelay(',start);
assert(start>0&&end>start);
class Element {
 constructor(){this.children=[];this.parentElement=null;this.attributes={};this.dataset={};this.events={};this.hidden=false;this.disabled=false;this.value='';this.textContent='';}
 appendChild(child){if(child.parentElement)child.remove();this.children.push(child);child.parentElement=this;return child;}
 insertBefore(child,before){if(!before)return this.appendChild(child);if(child.parentElement)child.remove();this.children.splice(this.children.indexOf(before),0,child);child.parentElement=this;return child;}
 append(...children){children.forEach(child=>this.appendChild(child));}
 remove(){if(this.parentElement){const a=this.parentElement.children;a.splice(a.indexOf(this),1);this.parentElement=null;}}
 setAttribute(key,value){this.attributes[key]=value;}
 getAttribute(key){return this.attributes[key];}
 addEventListener(name,fn){this.events[name]=fn;}
 click(){if(!this.disabled&&this.events.click)this.events.click();}
 querySelector(){return this.summary||(this.summary=new Element());}
}
const nodes={};const node=id=>nodes[id]||(nodes[id]=new Element());
const cardsNode=node('relay-settings-cards'),addButton=node('relay-add-button'),addPanel=node('relay-add-details');
const home=node('relay-editor-home'),addEditor=node('relay-add-editor'),editor=node('relay-card-editor');home.appendChild(editor);
const board=new Element();board.value='0';
const key=new Element(),select=new Element(),ssid=new Element(),pairButton=new Element();
const form={elements:{key},contains:()=>false},github={hidden:true,elements:{release:select}},pair={elements:{ssid},querySelector:()=>pairButton};
let allow=true,updates=0;
const ctx=vm.createContext({document:{getElementById:node,createElement:()=>new Element()},relaySettingsCards:cardsNode,relayAddButton:addButton,relayAddPanel:addPanel,
 relayEditor:editor,relayEditorHome:home,relayAddEditor:addEditor,relayCardNodes:new Map(),relayUpdateUiByBoard:new Map(),relayOpenBoard:null,relayAdding:false,
 remoteRelayBoard:board,remoteRelayCards:[],remoteRelayDirty:false,remoteRelayBusy:false,remoteRelayUploadBusy:false,remoteRelaySearchBusy:false,
 relayPairActive:false,relayPairScanBusy:false,relayRebootBusy:false,remoteRelayEditRevision:0,remoteRelayForm:form,remoteRelayForgetMessage:new Element(),
 remoteGithubForm:github,remoteRelayUpdateProgress:new Element(),remoteRelayUpdateMessage:new Element(),remoteReleaseMessage:new Element(),relayPairForm:pair,
 remoteRelayState:null,simulationEnabled:false,confirm:()=>allow,showRemoteRelayConfigMessage:()=>{},updateRemoteRelay:()=>{updates++;}});
vm.runInContext(source.slice(start,end),ctx);
const free=()=>({configured:false,online:false,address:'',labels:['1','2','3','4']});
const card=(i,online=true)=>({configured:true,online,address:'192.168.1.'+(10+i),version:online?'3.1.0':'',labels:['1','2','3','4']});
ctx.remoteRelayCards=[free(),free(),free(),free()];ctx.renderRelaySettingsCards(ctx.remoteRelayCards);
assert.equal(cardsNode.children.length,0);assert.match(node('relay-settings-notice').textContent,/Noch keine/);
addButton.click();assert.equal(ctx.relayAdding,true);assert.equal(board.value,'0');assert.equal(editor.parentElement,addEditor);
assert(node('remote-relay-update-section').hidden);assert(node('relay-edit-details').hidden);
addButton.click();assert.equal(ctx.relayAdding,false);assert.equal(editor.parentElement,home);
ctx.remoteRelayCards=[card(0),free(),card(2,false),free()];ctx.renderRelaySettingsCards(ctx.remoteRelayCards);
assert.equal(cardsNode.children.length,2);assert.match(ctx.relayCardNodes.get(2).state.textContent,/Nicht erreichbar/);
ctx.relayCardNodes.get(0).button.click();assert.equal(ctx.relayOpenBoard,0);assert.equal(board.value,'0');
assert.equal(editor.parentElement,ctx.relayCardNodes.get(0).body);assert.equal(ctx.relayCardNodes.get(0).button.getAttribute('aria-expanded'),'true');
ctx.remoteRelayDirty=true;key.value='uncommitted';allow=false;ctx.relayCardNodes.get(2).button.click();
assert.equal(ctx.relayOpenBoard,0);assert.equal(key.value,'uncommitted');assert.equal(board.value,'0');
allow=true;ctx.relayCardNodes.get(2).button.click();assert.equal(ctx.relayOpenBoard,2);assert.equal(board.value,'2');assert.equal(key.value,'');
assert.equal(editor.parentElement,ctx.relayCardNodes.get(2).body);assert.equal(ctx.relayCardNodes.get(0).button.getAttribute('aria-expanded'),'false');
ctx.remoteRelayUploadBusy=true;ctx.relayCardNodes.get(0).button.click();assert.equal(ctx.relayOpenBoard,2);
ctx.remoteRelayUploadBusy=false;
ctx.remoteRelayUpdateMessage.textContent='Update nicht bestätigt. Relaiszustände prüfen.';ctx.remoteRelayUpdateMessage.className='message error';ctx.remoteRelayUpdateProgress.hidden=false;ctx.remoteRelayUpdateProgress.value=42;
ctx.relayCardNodes.get(0).button.click();assert.equal(ctx.remoteRelayUpdateMessage.textContent,'');
ctx.relayCardNodes.get(2).button.click();assert.match(ctx.remoteRelayUpdateMessage.textContent,/nicht bestätigt/);assert.equal(ctx.remoteRelayUpdateMessage.className,'message error');assert.equal(ctx.remoteRelayUpdateProgress.value,42);assert.equal(ctx.remoteRelayUpdateProgress.hidden,false);
ctx.remoteRelayCards=[card(0),card(1),card(2),card(3)];ctx.renderRelaySettingsCards(ctx.remoteRelayCards);
assert.equal(cardsNode.children.length,4);assert(addButton.disabled);assert.match(node('relay-settings-notice').textContent,/Alle vier/);
assert.deepEqual(cardsNode.children.map(el=>el.children[0].children[0].children[0].textContent),['Relaiserweiterung 1','Relaiserweiterung 2','Relaiserweiterung 3','Relaiserweiterung 4']);
ctx.remoteRelayCards=[];ctx.renderRelaySettingsCards(ctx.remoteRelayCards);
assert.equal(cardsNode.children.length,4);assert.match(ctx.relayCardNodes.get(2).state.textContent,/unbekannt/);
assert(ctx.relayCardNodes.get(2).button.disabled);assert.match(node('relay-settings-notice').textContent,/Keine aktuellen Daten/);
assert(updates>=4);
console.log('PASS: Kartenübersicht, freie Plätze, Auswahl, ungespeicherte Änderungen und Offlinezustand.');
