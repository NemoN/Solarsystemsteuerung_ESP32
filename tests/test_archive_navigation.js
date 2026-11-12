const assert = require('node:assert/strict');
const fs = require('node:fs');
const path = require('node:path');
const vm = require('node:vm');
const js = fs.readFileSync(path.join(__dirname, '../history-server/app/static/app.js'), 'utf8');
const start = js.indexOf('const navToggle =');
const end = js.indexOf('const zone =', start);
assert(start >= 0 && end > start);
function element() {
  return {hidden: false, attrs: {}, handlers: {}, focused: false,
    setAttribute(name, value) { this.attrs[name] = value; },
    getAttribute(name) { return this.attrs[name]; },
    addEventListener(name, fn) { this.handlers[name] = fn; },
    focus() { this.focused = true; }};
}
const button = element(), links = element();
const media = {matches: false, addEventListener(name, fn) { this.change = fn; }};
const nodes = {'nav-toggle': button, 'nav-links': links};
vm.runInNewContext(js.slice(start, end), {$: id => nodes[id], window: {matchMedia: () => media}});
assert.equal(button.hidden, true); assert.equal(links.hidden, false);
media.matches = true; media.change();
assert.equal(button.hidden, false); assert.equal(links.hidden, true);
button.handlers.click();
assert.equal(links.hidden, false); assert.equal(button.getAttribute('aria-expanded'), 'true');
links.handlers.keydown({key: 'Escape'});
assert.equal(links.hidden, true); assert.equal(button.getAttribute('aria-expanded'), 'false'); assert.equal(button.focused, true);
button.handlers.click(); links.handlers.click({target: {closest: () => ({})}});
assert.equal(links.hidden, true);
button.handlers.click(); media.matches = false; media.change();
assert.equal(button.hidden, true); assert.equal(links.hidden, false);
console.log('PASS: Archivnavigation auf Desktop, Mobilgeräten und mit Escape.');
