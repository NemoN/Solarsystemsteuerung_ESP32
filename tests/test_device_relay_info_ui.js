const assert=require('node:assert/strict'),fs=require('node:fs'),vm=require('node:vm'),path=require('node:path');
const sketch=fs.readFileSync(path.join(__dirname,'../Backload/DashboardPage.h'),'utf8');
class Node{
  constructor(tag){this.tag=tag;this.textContent='';this.children=[];}
  append(...children){this.children.push(...children);}
  appendChild(child){this.children.push(child);return child;}
  replaceChildren(...children){this.children=children;}
}
const status=new Node('p'),list=new Node('div');
const document={getElementById:id=>id==='info-relays-status'?status:list,createElement:tag=>new Node(tag)};
const start=sketch.indexOf('let infoRelaySignature;'),end=sketch.indexOf("if(document.body.classList.contains('history-page'))",start);
assert(start>0&&end>start);
const context=vm.createContext({document});vm.runInContext(sketch.slice(start,end),context);
const online={id:0,configured:true,ready:true,online:true,address:'192.168.40.83',version:'2.9.0',mask:9,labels:['Heizung','<img src=x>','Relais 3','Reserve']};
const offline={id:2,configured:true,ready:true,online:false,address:'192.168.40.84',version:'old',mask:15,labels:['A','B','C','D']};
const rows=entry=>Object.fromEntries(entry.children[1].children.map(row=>row.children.map(node=>node.textContent)));
context.renderInfoRelays([online,{configured:false},offline]);
assert.equal(list.children.length,2);assert.equal(list.children[0].children[0].textContent,'Relaiserweiterung 1');
assert.deepEqual({...rows(list.children[0])},{'Status':'Verbunden','IP-Adresse':'192.168.40.83','Firmware':'2.9.0','Relais 1':'Heizung · EIN','Relais 2':'<img src=x> · AUS','Relais 3':'Relais 3 · AUS','Relais 4':'Reserve · EIN'});
assert.equal(list.children[1].children[0].textContent,'Relaiserweiterung 3');assert.equal(rows(list.children[1]).Status,'Nicht erreichbar');assert.equal(rows(list.children[1]).Firmware,'Nicht verfügbar');assert.equal(rows(list.children[1])['Relais 1'],'A · unbekannt');
context.renderInfoRelays([{...online,version:'',mask:null}]);assert.equal(rows(list.children[0]).Firmware,'Nicht verfügbar');assert.equal(rows(list.children[0])['Relais 1'],'Heizung · unbekannt');
context.renderInfoRelays([{...online,ready:false}]);assert.equal(rows(list.children[0]).Status,'Nicht verfügbar');
context.renderInfoRelays([{configured:false}]);assert.equal(list.children.length,0);assert.match(status.textContent,/Keine Relaiserweiterungen eingerichtet/);
context.renderInfoRelays(null);assert.equal(list.children.length,0);assert.match(status.textContent,/Keine aktuellen Daten/);
console.log('PASS: Erweiterungsinformationen bei Verbindung, Ausfall und fehlenden Daten.');
