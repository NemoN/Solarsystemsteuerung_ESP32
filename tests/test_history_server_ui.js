const assert = require('node:assert/strict');
const fs = require('node:fs');
const vm = require('node:vm');
const path = require('node:path');
const page = fs.readFileSync(path.join(__dirname, '../Backload/DashboardPage.h'), 'utf8');
const styles = fs.readFileSync(path.join(__dirname, '../Backload/DashboardStyles.h'), 'utf8');
const backend = fs.readFileSync(path.join(__dirname, '../Backload/Backload.ino'), 'utf8');
const proxy = fs.readFileSync(path.join(__dirname, '../Backload/WebSerialMonitor.cpp'), 'utf8');
assert.match(page, /id="history-server-link"[^>]*hidden/);
assert.match(page, /href="#history-server-section"/);
assert.match(styles, /body\.settings-page \.backload #history-server-section/);
assert.match(backend, /server\.on\("\/api\/history-server", HTTP_GET, sendHistoryServerUrl\)/);
assert.match(backend, /server\.on\("\/api\/history-server", HTTP_POST, saveHistoryServerUrl\)/);
assert.match(proxy, /on\("\/api\/history-server", HTTP_GET \| HTTP_POST, proxyRequest\)/);

const nodes = {};
for (const id of ['history-server-form', 'history-server-url', 'history-server-message', 'history-server-link']) {
  nodes[id] = {value:'', hidden:true, disabled:true, textContent:'', className:'',
    addEventListener(_type, fn){this.listener=fn;}, removeAttribute(name){delete this[name];}};
}
const button = {disabled:true};
nodes['history-server-form'].querySelector = () => button;
let saved = 'http://history.local:8088/', calls = [], failure = false;
const context = vm.createContext({URL, URLSearchParams,
  document:{getElementById:id=>nodes[id]},
  request:async (url, options)=>{
    calls.push({url, options});
    if (failure) throw new Error('Verbindung unterbrochen');
    if (options) saved=options.body.get('url');
    return {url:saved};
  },
});
const start = page.indexOf("const historyServerForm=");
const end = page.indexOf('const updateForm=', start);
assert(start > 0 && end > start);
vm.runInContext(page.slice(start,end), context);
const tick = () => new Promise(resolve=>setImmediate(resolve));
(async()=>{
  await tick();
  assert.equal(nodes['history-server-url'].value, saved);
  assert.equal(nodes['history-server-link'].href, saved);
  assert.equal(nodes['history-server-link'].hidden, false);
  nodes['history-server-url'].value='https://example.org/archive';
  await nodes['history-server-form'].listener({preventDefault(){}});
  assert.equal(saved,'https://example.org/archive');
  assert.equal(nodes['history-server-link'].href,saved);
  nodes['history-server-url'].value='javascript:alert(1)';
  const count=calls.length;
  await nodes['history-server-form'].listener({preventDefault(){}});
  assert.equal(calls.length,count);
  assert.match(nodes['history-server-message'].textContent,/gültige/);
  nodes['history-server-url'].value='http://user:password@example.org';
  await nodes['history-server-form'].listener({preventDefault(){}});
  assert.equal(calls.length,count);
  failure=true;nodes['history-server-url'].value='http://other.local';
  await nodes['history-server-form'].listener({preventDefault(){}});
  assert.equal(nodes['history-server-link'].href,'https://example.org/archive');
  assert.match(nodes['history-server-message'].textContent,/Nicht gespeichert/);
  failure=false;nodes['history-server-url'].value='';
  await nodes['history-server-form'].listener({preventDefault(){}});
  assert.equal(saved,'');
  assert.equal(nodes['history-server-link'].hidden,true);
  assert.equal(nodes['history-server-link'].href,undefined);
  await context.loadHistoryServer();
  assert.equal(nodes['history-server-link'].hidden,true);
  console.log('History-Server-Einstellung und Menü: OK');
})().catch(error=>{console.error(error);process.exitCode=1;});
