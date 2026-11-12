const assert = require('node:assert/strict');
const fs = require('node:fs');
const path = require('node:path');
const vm = require('node:vm');

const script = fs.readFileSync(path.join(__dirname, '../app/static/app.js'), 'utf8');
const start = script.indexOf('let updatePolling,');
assert(start >= 0);
const nodes = {};
for (const id of ['update-status', 'update-button', 'check-update', 'update-changelog', 'check-status']) {
  nodes[id] = {
    textContent: '', disabled: false, hidden: false,
    classList: {toggle() {}, remove() {}, add() {}},
    addEventListener(_event, listener) { this.listener = listener; },
  };
}
let status = {enabled: true, state: 'complete', message: 'Update installiert und Dienst neu gestartet.'};
const context = vm.createContext({
  $: id => nodes[id], json: async () => status,
  fetch: async () => ({ok: true}), AbortSignal,
  setTimeout: () => 1, clearTimeout() {},
});
vm.runInContext(script.slice(start), context);
const tick = () => new Promise(resolve => setImmediate(resolve));

(async () => {
  await tick();
  assert.equal(nodes['update-status'].textContent, 'Bereit für ein Update.');
  await nodes['update-button'].listener();
  await context.loadUpdateStatus();
  assert.equal(nodes['update-status'].textContent, status.message);
  status = {enabled: true, state: 'error', message: 'Update fehlgeschlagen.'};
  await context.loadUpdateStatus();
  assert.equal(nodes['update-status'].textContent, status.message);
  console.log('History-Server Update-Status: OK');
})().catch(error => { console.error(error); process.exitCode = 1; });
