const assert=require('node:assert/strict');
const fs=require('node:fs'),vm=require('node:vm');
const sketch=fs.readFileSync(require('node:path').join(__dirname,'../Backload/DashboardPage.h'),'utf8');
const inputs={'access-password':{type:'password',value:'Abc123!? X'},'access-confirm':{type:'password',value:'Abc123!? X'}};
const buttons=Object.keys(inputs).map(id=>({attributes:{'aria-controls':id,'aria-pressed':'false'},title:'Passwort anzeigen',
  getAttribute(name){return this.attributes[name];},setAttribute(name,value){this.attributes[name]=value;},
  addEventListener(name,callback){assert.equal(name,'click');this.click=callback;}}));
const form={querySelectorAll:selector=>{assert.equal(selector,'.password-visibility');return buttons;}};
const start=sketch.indexOf('function setupAccessPasswordVisibility(){');
const end=sketch.indexOf('setupAccessPasswordVisibility();',start);
assert(start>0&&end>start);
vm.runInNewContext(sketch.slice(start,end)+'setupAccessPasswordVisibility();',{passwordForm:form,document:{getElementById:id=>inputs[id]}});
buttons[0].click();
assert.equal(inputs['access-password'].type,'text');assert.equal(inputs['access-password'].value,'Abc123!? X');
assert.equal(buttons[0].attributes['aria-pressed'],'true');assert.equal(buttons[0].title,'Passwort verbergen');
assert.equal(inputs['access-confirm'].type,'password');
buttons[1].click();assert.equal(inputs['access-confirm'].type,'text');
buttons[0].click();assert.equal(inputs['access-password'].type,'password');assert.equal(inputs['access-confirm'].type,'text');
assert.equal(buttons[0].attributes['aria-pressed'],'false');assert.equal(buttons[0].title,'Passwort anzeigen');
for(const id of Object.keys(inputs)){
  assert(sketch.includes('aria-controls="'+id+'"'));
  assert.match(sketch,new RegExp('id="'+id+'" type="password"'));
}
assert.equal((sketch.match(/class="password-visibility" type="button"/g)||[]).length,2);
console.log('PASS: Passwortfelder unabhängig ein- und ausblenden, ohne den Wert zu verändern.');
