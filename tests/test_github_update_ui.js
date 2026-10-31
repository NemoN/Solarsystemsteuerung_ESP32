// Übt die echte Statusabfrage aus dem Sketch mit unterbrochenen Verbindungen.
const assert = require('node:assert/strict');
const fs = require('node:fs');
const vm = require('node:vm');

const sketch = fs.readFileSync(require('node:path').join(__dirname, '../Backload/Backload.ino'), 'utf8');
const start = sketch.indexOf('async function pollGithubUpdate(');
const end = sketch.indexOf("githubForm.addEventListener('submit'", start);
assert(start >= 0 && end > start, 'Update-Statusfunktion im Sketch nicht gefunden');

function createPoll() {
  let now = 0;
  const context = vm.createContext({
    Date: { now: () => now },
    setTimeout: (callback, ms) => { now += ms; callback(); },
  });
  vm.runInContext(sketch.slice(start, end), context);
  return context.pollGithubUpdate;
}

function httpError(status) {
  const error = new Error('HTTP ' + status);
  error.status = status;
  return error;
}

async function run() {
  {
    const attempts = [new Error('Netzwerk'), httpError(503),
      {state: 'downloading', received: 250, size: 1000},
      {state: 'finished', received: 1000, size: 1000}];
    const calls = [];
    const progress = {}, message = {};
    await createPoll()(async (path, body) => {
      calls.push({path, token: body.token});
      const response = attempts.shift();
      if (response instanceof Error) throw response;
      return response;
    }, 'test-token', progress, message);
    assert.equal(progress.value, 100);
    assert.equal(calls.length, 4);
    assert(calls.every(call => call.path === 'github-status' && call.token === 'test-token'));
  }
  {
    let calls = 0;
    const error = await createPoll()(async () => { calls++; throw new Error('offline'); }, 't', {}, {}).catch(e => e);
    assert.equal(error.updateUncertain, true);
    assert.match(error.message, /Download kann noch laufen/);
    assert(calls > 1 && calls < 60);
  }
  {
    const error = await createPoll()(async () => { throw httpError(404); }, 't', {}, {}).catch(e => e);
    assert.equal(error.updateUncertain, true);
    assert.match(error.message, /Firmware-Version/);
  }
  {
    const error = await createPoll()(async () => { throw httpError(403); }, 't', {}, {}).catch(e => e);
    assert.equal(error.status, 403);
    assert.equal(error.updateUncertain, undefined);
  }
  {
    const error = await createPoll()(async () => ({state: 'failed', received: 10, size: 100,
      error: 'Verbindung während des Downloads unterbrochen.'}), 't', {}, {}).catch(e => e);
    assert.match(error.message, /während des Downloads/);
    assert.equal(error.updateUncertain, undefined);
  }
  console.log('PASS: Statusabfrage nach Verbindungsfehlern, unbekanntem Status und echtem Downloadfehler.');
}
run().catch(error => { console.error(error); process.exitCode = 1; });
