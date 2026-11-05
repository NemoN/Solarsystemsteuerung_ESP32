// Prüft die tatsächlichen Theme-, Passwort- und Testmodus-Funktionen aus dem Sketch.
const assert = require('node:assert/strict');
const fs = require('node:fs');
const vm = require('node:vm');
const sketch = fs.readFileSync(require('node:path').join(__dirname, '../Backload/Backload.ino'), 'utf8');
const styles = fs.readFileSync(require('node:path').join(__dirname, '../Backload/DashboardStyles.h'), 'utf8');
const server = fs.readFileSync(require('node:path').join(__dirname, '../Backload/WebSerialMonitor.cpp'), 'utf8');
const nav = sketch.slice(sketch.indexOf('<nav class="page-nav"'), sketch.indexOf('</nav>'));
assert(!nav.includes('class="wifi-link"') && !nav.includes('class="update-link"'));
assert(!nav.includes('theme-toggle'));
assert(!nav.includes('firmware-version'));
assert.match(sketch, /<h1>Solarsystemsteuerung<\/h1><p id="firmware-version"/);
assert.match(sketch, /'\/einstellungen','\/testwerte','\/sicherheit'/);
assert(!sketch.includes("location.pathname==='/wlan'") && !sketch.includes("location.pathname==='/update'"));
assert(!server.includes('server().on("/wlan"') && !server.includes('server().on("/update"'));
assert(server.includes('server().on("/regelungsuebersicht"') && !server.includes('server().on("/organigramm"'));
assert.match(server, /server\(\)\.onNotFound\([\s\S]*?send\(404/);
assert.match(styles, /body\.settings-page \.backload #wifi-section/);
assert.match(styles, /body\.settings-page \.backload #update-section/);
assert.match(styles, /a\[aria-current="page"\]/);
for (const cls of ['dashboard', 'history', 'organigram', 'info', 'settings'])
  assert(!styles.includes('.' + cls + '-link{display:none}'));
assert(sketch.indexOf('id="settings-intro"') < sketch.indexOf('id="wifi-section"'));
assert(sketch.indexOf('id="wifi-section"') < sketch.indexOf('id="update-section"'));
const hostSection = sketch.slice(sketch.indexOf('<section id="hostname-section"'), sketch.indexOf('<section id="wifi-section"'));
const wifiSection = sketch.slice(sketch.indexOf('<section id="wifi-section"'), sketch.indexOf('<section id="update-section"'));
assert(hostSection.includes('id="hostname-form"') && !wifiSection.includes('id="hostname-form"'));
for (const id of ['appearance-section','security-section','hostname-section','wifi-section','update-section','simulation-settings'])
  assert(nav.includes('href="#' + id + '"') && sketch.includes('id="' + id + '"'));
assert.match(styles, /body\.settings-page \.backload \.settings-subnav\{display:flex/);
assert.match(styles, /settings-page \.backload #hostname-section\{display:block\}|settings-page \.backload #hostname-section,/);
const sectionLinks = ['#hostname-section', '#wifi-section'].map(href => ({href, attributes:{},
  getAttribute(name){return name==='href'?this.href:this.attributes[name];},
  setAttribute(name,value){this.attributes[name]=value;}, removeAttribute(name){delete this.attributes[name];}}));
const navigationContext = vm.createContext({location:{hash:'#wifi-section'},
  document:{querySelectorAll:()=>sectionLinks}});
const sectionStart=sketch.indexOf('function syncSettingsNavigation()');
const sectionEnd=sketch.indexOf("window.addEventListener('hashchange'",sectionStart);
assert(sectionStart>=0 && sectionEnd>sectionStart);
vm.runInContext(sketch.slice(sectionStart,sectionEnd),navigationContext);
navigationContext.syncSettingsNavigation();
assert.equal(sectionLinks[1].attributes['aria-current'],'location');
navigationContext.location.hash='#hostname-section';navigationContext.syncSettingsNavigation();
assert.equal(sectionLinks[0].attributes['aria-current'],'location');
assert.equal(sectionLinks[1].attributes['aria-current'],undefined);
let subnavClick;
const mobileMenu = {hidden:false};
const mobileToggle = {attributes:{},setAttribute(name,value){this.attributes[name]=value;}};
const mobileContext = vm.createContext({document:{getElementById:()=>({addEventListener:(_name,fn)=>{subnavClick=fn;}})},
  compactNav:{matches:true},navLinks:mobileMenu,navToggle:mobileToggle});
const mobileStart=sketch.indexOf("const settingsSubnav=document.getElementById('settings-subnav')");
assert(mobileStart>=0 && sectionStart>mobileStart);
vm.runInContext(sketch.slice(mobileStart,sectionStart),mobileContext);
subnavClick({target:{closest:()=>true}});
assert.equal(mobileMenu.hidden,true);
assert.equal(mobileToggle.attributes['aria-expanded'],'false');
const nodes = new Map();
function node(id) {
  if (!nodes.has(id)) nodes.set(id, {hidden:false, value:'', textContent:'', className:'', attributes:{},
    setAttribute(k,v){this.attributes[k]=v;}, addEventListener(k,fn){this[k]=fn;},
    querySelectorAll(){return [];}, querySelector(){return node('hostname-save');}, contains(){return true;}, children:[{}]});
  return nodes.get(id);
}
const classes = new Set(['dark']);
let stored = {}, calls = [], modeChanges = [], location = {href:'http://esp/', replace(url){this.redirect=url;}};
const context = vm.createContext({
  document:{getElementById:node,querySelector:node,activeElement:null,
    documentElement:{classList:{contains(k){return classes.has(k);},toggle(k,v){v?classes.add(k):classes.delete(k);}}},
    body:{classList:{contains(){return false;}}}},
  localStorage:{setItem(k,v){stored[k]=v;}},
  URL, URLSearchParams, location,
  history:{replaceState(_a,_b,url){location.href='http://esp'+url;}},
  simForm:node('sim-form'),simDirty:false,simBusy:false,
  simulationEnabled:false,testParameterHandled:false,
  changeTestMode:enable=>modeChanges.push(enable),
  passwordForm:node('password-form'),passwordMessage:node('password-message'),
  request:async (url,opts)=>{calls.push({url,opts});return url==='/api/hostname'?{hostname:opts.body.get('hostname')||'backload-test',restartRequired:true}:{saved:true};},
  confirm:()=>true,
});
function load(start,end){const a=sketch.indexOf(start),b=sketch.indexOf(end,a);assert(a>=0&&b>a);vm.runInContext(sketch.slice(a,b),context);}
load('function markCurrentPage()', 'function syncThemeControls()');
for(const [path,selector] of [['/','.dashboard-link'],['/verlauf','.history-link'],['/regelungsuebersicht','.organigram-link'],['/info','.info-link'],['/einstellungen','.settings-link'],['/testwerte','.settings-link'],['/sicherheit','.settings-link']]){
  node(selector).attributes={};location.pathname=path;context.markCurrentPage();
  assert.equal(node(selector).attributes['aria-current'],'page');
}
load('function syncThemeControls()', 'const temperatureNumber=');
context.setTheme(true);
assert(classes.has('light')&&!classes.has('dark'));
assert.equal(stored['rd-th'],'light');
assert.equal(node('theme-light').attributes['aria-pressed'],'true');
context.setTheme(false);
assert.equal(stored['rd-th'],'dark');
assert.equal(node('theme-dark').attributes['aria-pressed'],'true');
load('function reflectTestModeInUrl(', 'let autoDirty=');
load('function updateSimulation(data)', 'async function changeTestMode(');
context.updateSimulation({simulation:true,controllers:[]});
assert.deepEqual(modeChanges,[],'Anmeldung und Hauptseite dürfen Testmodus nicht beenden');
assert.equal(node('simulation-section').hidden,false);
assert.equal(node('.settings-link').href,'/einstellungen?test=on');
assert.match(location.href,/test=on/);
context.testParameterHandled=false;location.href='http://esp/einstellungen';
context.updateSimulation({simulation:false});
assert.equal(node('simulation-section').hidden,true);
assert.match(node('test-page-status').textContent,/nur im Testmodus/);
load('async function saveAccessPassword(', 'let saving=false;');
(async()=>{
  context.simulationEnabled=true;
  node('access-password').value='abcdefgh';node('access-confirm').value='mismatch';
  node('password-form').submit({preventDefault(){}});
  assert.equal(calls.length,0);
  assert.match(node('password-message').textContent,/stimmen nicht überein/);
  node('access-password').value='abcdefgh';node('access-confirm').value='abcdefgh';
  node('password-form').submit({preventDefault(){}});
  await new Promise(resolve=>setImmediate(resolve));
  assert.equal(calls[0].url,'/auth/password');
  assert.equal(calls[0].opts.body.get('password'),'abcdefgh');
  assert.equal(location.redirect,'/einstellungen?test=on');
  assert.equal(node('access-password').value,'');
  context.updateHostname({hostname:'backload-test',activeHostname:'backload-test',mdnsReady:true});
  assert.equal(node('hostname-input').value,'backload-test');
  assert.match(node('hostname-active').textContent,/backload-test\.local/);
  node('hostname-input').value='MEINE-ANLAGE';node('hostname-input').input();
  context.updateHostname({hostname:'backload-test',activeHostname:'backload-test',mdnsReady:true});
  assert.equal(node('hostname-input').value,'MEINE-ANLAGE');
  await context.saveHostnameSetting(false);
  assert.equal(calls.at(-1).url,'/api/hostname');
  assert.equal(calls.at(-1).opts.body.get('hostname'),'meine-anlage');
  assert.match(node('hostname-message').textContent,/nächsten Neustart/);
  await context.saveHostnameSetting(true);
  assert.equal(calls.at(-1).opts.body.get('hostname'),'');
  assert.equal(node('hostname-input').value,'backload-test');
  console.log('PASS: Einstellungen, Theme, Passwort und Testmodus.');
})().catch(e=>{console.error(e);process.exitCode=1;});
