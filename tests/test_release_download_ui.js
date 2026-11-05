// Prüft die echte Release-Auswahl und den Downloadlink aus dem Sketch.
const assert = require('node:assert/strict');
const fs = require('node:fs');
const vm = require('node:vm');
const sketch = fs.readFileSync(require('node:path').join(__dirname, '../Backload/Backload.ino'), 'utf8');
const start = sketch.indexOf("const githubForm=document.getElementById('github-update-form')");
const end = sketch.indexOf("// Ein Netzwerkfehler bei der Statusabfrage", start);
assert(start >= 0 && end > start);

const listeners = {};
const select = {
  value: '', disabled: true, options: [],
  addEventListener: (name, fn) => { listeners[name] = fn; },
  replaceChildren(option) { this.options = [option]; this.value = option.value; },
  add(option) { this.options.push(option); },
};
const install = {}, download = {hidden: true}, link = {removeAttribute() { delete this.href; }};
const form = {elements: {release: select}, querySelector: () => install};
const message = {}, check = {addEventListener: (name, fn) => { listeners.check = fn; }};
const document = {getElementById: id => ({'github-update-form': form, 'release-message': message,
  'release-download': download, 'release-download-link': link, 'release-check': check})[id]};
const asset = {name: 'Backload.ino.bin', digest: 'sha256:' + 'a'.repeat(64), size: 1200000};
let installedVersion = '1.11.5', fail = false, releases = [
  {tag_name: 'v1.11.5', name: 'Version 1.11.5', published_at: '2026-10-31', assets: [asset, {...asset, name: 'Backload-1.11.5.bin'}]},
  {tag_name: 'v1.11.4', name: 'Version 1.11.4', published_at: '2026-10-30', assets: [asset, {...asset, name: 'Backload-1.11.4.bin', digest: 'sha256:'+'b'.repeat(64)}]},
  {tag_name: 'v1.11.3', name: 'Version 1.11.3', published_at: '2026-10-29', assets: [asset]},
];
const context = vm.createContext({document, Option: function(text, value) { this.text = text; this.value = value; },
  request: async () => ({version: installedVersion}),
  fetch: async () => { if (fail) throw new Error('offline'); return {ok: true, json: async () => releases}; }});
vm.runInContext(sketch.slice(start, end), context);

(async () => {
  assert.equal(context.compareVersions('v2.0.0-ohne-ap','1.14.5-mit-ap'),1);
  assert.equal(context.compareVersions('v2.0.0-mit-ap','2.0.0-ohne-ap'),0);
  assert.equal(context.compareVersions('v1.14.5-ohne-ap','2.0.0-mit-ap'),-1);
  assert.equal(context.compareVersions('v2.0.0-unknown','2.0.0-mit-ap'),null);
  await listeners.check({currentTarget: check});
  assert.equal(download.hidden, true);
  select.value = '0'; listeners.change();
  assert.equal(download.hidden, false);
  assert.equal(link.href, 'https://github.com/NemoN/Solarsystemsteuerung_ESP32/releases/download/v1.11.5/Backload-1.11.5.bin');
  select.value = '1'; listeners.change();
  assert.match(link.href, /\/v1\.11\.4\/Backload\.ino\.bin$/);
  select.value = '2'; listeners.change();
  assert.match(link.href, /\/v1\.11\.3\/Backload\.ino\.bin$/);
  select.value = ''; listeners.change();
  assert.equal(download.hidden, true); assert.equal(link.href, undefined);
  releases=[{tag_name:'v2.0.0-mit-ap',name:'Version 2.0.0 – mit AP',published_at:'2026-11-05',assets:[asset]},
    {tag_name:'v2.0.0-ohne-ap',name:'Version 2.0.0 – ohne AP',published_at:'2026-11-04',assets:[asset]}];
  installedVersion='1.14.5-ohne-ap';await listeners.check({currentTarget:check});
  assert.match(message.textContent,/Neue Version verfügbar/);
  installedVersion='2.0.0-ohne-ap';await listeners.check({currentTarget:check});
  assert.match(message.textContent,/Firmware ist aktuell/);
  assert.match(select.options[2].text,/installiert/);
  installedVersion='1.11.5';
  releases = Array.from({length:12}, (_,i)=>({tag_name:`v1.20.${i+1}`, name:`Version 1.20.${i+1}`,
    published_at:`2026-10-${String(i+1).padStart(2,'0')}`, prerelease:i>=4, assets:[asset]}));
  releases.push({tag_name:'v1.99.1',published_at:'2026-11-01',assets:[]},
    {tag_name:'v1.99.2',published_at:'2026-11-02',assets:[asset],draft:true});
  await listeners.check({currentTarget:check});
  assert.equal(select.options.length, 9); // Platzhalter plus acht passende Releases
  assert.match(select.options[1].text, /^Version 1\.20\.12/);
  assert.match(select.options[8].text, /^Version 1\.20\.5/);
  assert.match(message.textContent, /Neueste Version: v1\.20\.4/); // stabile Version außerhalb der Auswahl
  select.value = '0'; listeners.change(); fail = true;
  await listeners.check({currentTarget: check});
  assert.equal(download.hidden, true); assert.equal(link.href, undefined);
  console.log('PASS: Firmware-Downloadlink bei Auswahl, Versionswechsel und fehlgeschlagener Suche.');
})().catch(error => { console.error(error); process.exitCode = 1; });
