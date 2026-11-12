const assert = require('node:assert/strict');
const fs = require('node:fs');
const path = require('node:path');
const vm = require('node:vm');

const script = fs.readFileSync(path.join(__dirname, '../app/static/app.js'), 'utf8');
const start = script.indexOf('let updatePolling,');
assert(start >= 0);
const nodes = {};
for (const id of ['update-status', 'update-button', 'check-update', 'update-changelog', 'check-status', 'update-changes']) {
  nodes[id] = {
    textContent: '', disabled: false, hidden: false,
    classList: {toggle() {}, remove() {}, add() {}},
    addEventListener(_event, listener) { this.listener = listener; },
    replaceChildren(...children) { this.children = children; },
  };
}
let status = {enabled: true, state: 'complete', message: 'Update installiert und Dienst neu gestartet.'};
let check = {available: false, can_update: false, ahead: 0, dirty: false, changes: []};
const context = vm.createContext({
  $: id => nodes[id], json: async () => status,
  fetch: async url => ({ok: true, json: async () => url.endsWith('/check') ? check : {}}), AbortSignal,
  document: {createElement: () => ({append() {}})},
  setTimeout: () => 1, clearTimeout() {},
});
vm.runInContext(script.slice(start), context);
const tick = () => new Promise(resolve => setImmediate(resolve));

(async () => {
  await tick();
  assert.equal(nodes['update-status'].textContent, 'Bitte auf Updates prüfen.');
  assert.equal(nodes['update-button'].disabled, true);
  await nodes['check-update'].listener();
  assert.equal(nodes['update-status'].textContent, '');
  assert.equal(nodes['check-status'].textContent, 'Der Git-Branch ist aktuell.');
  assert.equal(nodes['update-button'].disabled, true);
  check = {available: true, can_update: false, behind: 1, ahead: 0, dirty: true, changes: []};
  await nodes['check-update'].listener();
  assert.match(nodes['check-status'].textContent, /lokale Änderungen/);
  assert.equal(nodes['update-button'].disabled, true);
  check = {available: true, can_update: true, behind: 1, ahead: 0, dirty: false, changes: []};
  await nodes['check-update'].listener();
  assert.match(nodes['check-status'].textContent, /1 neue Änderung/);
  assert.equal(nodes['update-status'].textContent, '');
  assert.equal(nodes['update-button'].disabled, false);
  await nodes['update-button'].listener();
  await context.loadUpdateStatus();
  assert.equal(nodes['update-status'].textContent, status.message);
  assert.equal(nodes['update-button'].disabled, true);
  status = {enabled: true, state: 'error', message: 'Update fehlgeschlagen.'};
  await context.loadUpdateStatus();
  assert.equal(nodes['update-status'].textContent, status.message);
  console.log('History-Server Update-Status: OK');
})().catch(error => { console.error(error); process.exitCode = 1; });
