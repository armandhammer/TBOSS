// SPDX-License-Identifier: GPL-3.0-or-later - the editor and installer pages in a real browser: a headless
// Chrome or Edge (a throwaway profile, over the DevTools protocol) opens webapp/ from a local server, with a
// stand-in board (Web MIDI answered from memory, TBOSS 0.11.0) put in before the pages' scripts. Clicks
// through connecting, typing and saving a progression, errors, tabs, a settings backup, clearing and
// restoring, the scale library, favourites and custom scales, and the installer's "Save settings"; fails on
// any page error.   node tests/webapp/test_pages.mjs   (skips without Chrome or Edge; CHROME=path to choose)
import assert from 'node:assert/strict';
import { spawn, spawnSync } from 'node:child_process';
import crypto from 'node:crypto';
import fs from 'node:fs';
import http from 'node:http';
import os from 'node:os';
import path from 'node:path';
import { fileURLToPath } from 'node:url';

const root = path.resolve(path.dirname(fileURLToPath(import.meta.url)), '../..');
const candidates = [process.env.CHROME, 'C:/Program Files/Google/Chrome/Application/chrome.exe',
  'C:/Program Files (x86)/Microsoft/Edge/Application/msedge.exe', '/Applications/Google Chrome.app/Contents/MacOS/Google Chrome',
  ...['google-chrome', 'google-chrome-stable', 'chromium', 'chromium-browser', 'microsoft-edge'].map((name) => {
    const found = spawnSync('which', [name], { encoding: 'utf8' });
    return found.status === 0 ? found.stdout.trim() : null;
  })];
const browser = candidates.find((p) => p && fs.existsSync(p));
if (!browser) { console.log('SKIP pages: no Chrome or Edge found (set CHROME)'); process.exit(0); }

// The stand-in board: runs in the page, before its scripts (its source is sent as text).
function fakeBoard() {
  const pack = (bytes) => { const out = []; for (let i = 0; i < bytes.length; i += 7) { const g = bytes.slice(i, i + 7); out.push(g.reduce((t, b, j) => t | ((b >> 7) << j), 0)); for (const b of g) out.push(b & 0x7f); } return out; };
  const unpack = (bytes) => { const out = []; for (let i = 0; i < bytes.length; i += 8) { const top = bytes[i]; for (let j = 0; j < 7 && i + 1 + j < bytes.length; j++) out.push(bytes[i + 1 + j] | (((top >> j) & 1) << 7)); } return out; };
  const w2b = (w) => w.flatMap((x) => [x & 255, (x >>> 8) & 255, (x >>> 16) & 255, (x >>> 24) & 255]);
  const b2w = (b) => { const w = []; for (let i = 0; i + 3 < b.length; i += 4) w.push((b[i] | b[i + 1] << 8 | b[i + 2] << 16 | b[i + 3] << 24) >>> 0); return w; };
  const board = { records: {}, scales: Array.from({ length: 12 }, () => ({ mask: 0, name: '' })) };
  const live = new Array(40).fill(0);
  live[0] = (9 | 1 << 8 | 36 << 16 | 48 << 24) >>> 0; live[39] = 0x56330000; // A, natural minor
  board.records['1/0'] = live;
  const listeners = new Set();
  const input = { id: 'in', name: 'TheoryBoard MIDI', type: 'input', state: 'connected', onmidimessage: null,
    addEventListener(t, f) { listeners.add(f); }, removeEventListener(t, f) { listeners.delete(f); } };
  const reply = (cmd, status, data = []) => {
    const body = [cmd | 0x40, status, ...data], sum = body.reduce((a, b) => a + b, 0);
    const m = new Uint8Array([0xf0, 0x7d, 0x54, 0x42, ...body, (128 - (sum % 128)) % 128, 0xf7]);
    setTimeout(() => { const ev = { data: m }; if (input.onmidimessage) input.onmidimessage(ev); listeners.forEach((f) => f(ev)); }, 3);
  };
  const handle = (m) => {
    const cmd = m[4], d = m.slice(5, m.length - 2);
    switch (cmd) {
      case 1: return reply(1, 0, [1, 4, 12, 8, 40, 1, ...pack([0xef, 0x40, 0x18, 0, 0, 0, 1]), 0, 11, 0]);
      case 2: { const s = board.scales[d[0]]; if (!s) return reply(2, 3);
        return reply(2, 0, [d[0], s.mask & 127, s.mask >> 7, ...Array.from({ length: 12 }, (_, i) => (i < s.name.length ? s.name.charCodeAt(i) : 0))]); }
      case 3: board.scales[d[0]] = { mask: d[1] | d[2] << 7, name: String.fromCharCode(...d.slice(3).filter((c) => c)) }; return reply(3, 0, [d[0]]);
      case 4: return undefined; // preview: no reply
      case 5: { const w = board.records['1/0']; w[0] = ((w[0] & ~0xff00) | (d[0] << 8)) >>> 0; return reply(5, 0); }
      case 6: { let w = board.records[`${d[0]}/${d[1]}`];
        if (d[0] === 3) { const s = board.scales[d[1]]; w = s && s.mask ? [s.mask, ...new Array(39).fill(0)] : null; }
        return w ? reply(6, 0, [d[0], d[1], ...pack(w2b(w))]) : reply(6, 7, [d[0], d[1]]); }
      case 7: { const w = b2w(unpack(d.slice(2)));
        if (d[0] === 3) board.scales[d[1]] = { mask: w[0], name: 'RESTORED' }; else board.records[`${d[0]}/${d[1]}`] = w;
        return reply(7, 0, [d[0], d[1]]); }
      default: return reply(cmd, 4);
    }
  };
  const output = { id: 'out', name: 'TheoryBoard MIDI', type: 'output', state: 'connected', send(m) { handle(Array.from(m)); } };
  window.fakeBoard = board;
  Object.defineProperty(navigator, 'requestMIDIAccess', { configurable: true,
    value: async () => ({ inputs: new Map([['in', input]]), outputs: new Map([['out', output]]), onstatechange: null, sysexEnabled: true }) });
}

// webapp/ on a local port, as scripts/tboss_editor.py serves it (with a stand-in firmware image).
const types = { '.html': 'text/html', '.js': 'text/javascript', '.json': 'application/json', '.css': 'text/css' };
const image = Buffer.alloc(4096, 0xff);
const server = http.createServer((req, res) => {
  const where = new URL(req.url, 'http://x').pathname;
  if (where === '/firmware/tboss.bin' || where === '/firmware/tboss.json') {
    const body = where.endsWith('.bin') ? image : JSON.stringify({ version: 'test', file: 'tboss.bin', bytes: image.length,
      sha256: crypto.createHash('sha256').update(image).digest('hex') });
    res.writeHead(200, { 'content-type': where.endsWith('.bin') ? 'application/octet-stream' : 'application/json' }); res.end(body); return;
  }
  const file = path.join(root, 'webapp', decodeURIComponent(new URL(req.url, 'http://x').pathname).replace(/^\/$/, '/index.html'));
  if (!file.startsWith(path.join(root, 'webapp')) || !fs.existsSync(file)) { res.writeHead(404); res.end(); return; }
  res.writeHead(200, { 'content-type': types[path.extname(file)] || 'application/octet-stream' }); res.end(fs.readFileSync(file));
});
await new Promise((r) => server.listen(0, '127.0.0.1', r));
const site = `http://localhost:${server.address().port}/`;
const work = fs.mkdtempSync(path.join(os.tmpdir(), 'tb-pages-')), downloads = path.join(work, 'downloads');
fs.mkdirSync(downloads);
const chrome = spawn(browser, ['--headless=new', '--remote-debugging-port=0', `--user-data-dir=${path.join(work, 'profile')}`,
  '--no-first-run', '--no-default-browser-check', '--window-size=1280,2000', 'about:blank'], { stdio: 'ignore' });
const sleep = (ms) => new Promise((r) => setTimeout(r, ms));
let page;
for (let i = 0; i < 200 && !page; i++) {
  try {
    const port = fs.readFileSync(path.join(work, 'profile', 'DevToolsActivePort'), 'utf8').split('\n')[0];
    page = (await (await fetch(`http://127.0.0.1:${port}/json/list`)).json()).find((t) => t.type === 'page');
  } catch { await sleep(100); }
}
assert.ok(page, 'the browser started');
const ws = new WebSocket(page.webSocketDebuggerUrl);
await new Promise((r) => { ws.onopen = r; });
let id = 0; const pending = new Map(), problems = [];
ws.onmessage = (ev) => {
  const msg = JSON.parse(ev.data);
  if (msg.id && pending.has(msg.id)) { pending.get(msg.id)(msg); pending.delete(msg.id); return; }
  if (msg.method === 'Runtime.exceptionThrown') problems.push('exception: ' + (msg.params.exceptionDetails.exception?.description || msg.params.exceptionDetails.text));
  if (msg.method === 'Runtime.consoleAPICalled' && (msg.params.type === 'error' || msg.params.type === 'warning'))
    problems.push(`console.${msg.params.type}: ` + msg.params.args.map((a) => a.value ?? a.description).join(' '));
  if (msg.method === 'Log.entryAdded' && msg.params.entry.level === 'error') problems.push('log: ' + msg.params.entry.text + ' ' + (msg.params.entry.url || ''));
};
const send = (method, params = {}) => new Promise((resolve) => { const i = ++id; pending.set(i, resolve); ws.send(JSON.stringify({ id: i, method, params })); });
async function js(expression) {
  const r = await send('Runtime.evaluate', { expression, awaitPromise: true, returnByValue: true });
  if (r.result.exceptionDetails) throw new Error(expression + ': ' + r.result.exceptionDetails.exception?.description);
  return r.result.result.value;
}
async function until(expression, ms = 8000) {
  const t = Date.now();
  while (Date.now() - t < ms) { if (await js(expression)) return; await sleep(50); }
  throw new Error('timed out waiting for: ' + expression + '\n' + await js(`[...document.querySelectorAll('.status')].map(e => e.id + '=' + e.textContent).join(' | ')`));
}
const text = (sel) => js(`document.querySelector(${JSON.stringify(sel)}).textContent`);
const click = (sel) => js(`document.querySelector(${JSON.stringify(sel)}).click()`);
const set = (sel, value, event) => js(`(() => { const e = document.querySelector(${JSON.stringify(sel)}); e.value = ${JSON.stringify(value)}; e.dispatchEvent(new Event(${JSON.stringify(event)})); })()`);
let checks = 0;
try {
  await send('Page.enable'); await send('Runtime.enable'); await send('Log.enable');
  await send('Page.setDownloadBehavior', { behavior: 'allow', downloadPath: downloads });
  await send('Page.addScriptToEvaluateOnNewDocument', { source: `(${fakeBoard.toString()})();` });
  // The editor: connect; the Progressions panel reads the board's key.
  await send('Page.navigate', { url: site }); await until(`document.readyState === 'complete' && !!document.querySelector('#progTabs button')`);
  await click('#connect');
  await until(`document.querySelector('#conn').textContent === 'Connected' && /TBOSS 0\\.11\\.0/.test(document.querySelector('#facts').textContent)`);
  await until(`document.querySelector('#progHint').classList.contains('hidden')`);
  assert.deepEqual(await js(`[document.querySelector('#progRoot').value, document.querySelector('#progScale').value]`), ['9', '1']);
  assert.match(await text('#progTabs'), /P1 · empty/); checks++;
  // A song with borrowed chords, saved to P1.
  await set('#progText', 'Am D Dm Am C G E7 Am', 'input');
  assert.equal(await js(`document.querySelectorAll('#progChords .chord').length`), 8);
  assert.deepEqual(await js(`[...document.querySelectorAll('#progChords .chord.borrowed small')].map(e => e.textContent)`), ['1 bar · Dorian', '1 bar · Harmonic minor']);
  assert.equal(await text('#progStatus'), '8 chord changes, 8 of 64 units.'); checks++;
  await click('#progSave'); await until(`/^Saved to P1/.test(document.querySelector('#progStatus').textContent)`);
  const saved = await js(`window.fakeBoard.records['5/0']`);
  assert.deepEqual([saved[0] >>> 24, (saved[0] >>> 16) & 255], [8, 1]);
  assert.match(await text('#progTabs'), /P1 · 8 chords/); checks++;
  // Errors, tabs, key and unit.
  await set('#progText', 'C F#7b9 Xm', 'input');
  assert.match(await text('#progStatus'), /not a chord name/);
  assert.equal(await js(`document.querySelector('#progSave').disabled`), true); checks++;
  await js(`document.querySelectorAll('#progTabs button')[1].click()`); assert.equal(await js(`document.querySelector('#progText').value`), '');
  await js(`document.querySelectorAll('#progTabs button')[0].click()`); assert.equal(await js(`document.querySelector('#progText').value`), 'Am D Dm Am C G E7 Am'); checks++;
  await set('#progUnit', '0', 'change'); assert.match(await js(`document.querySelector('#progChords .chord small').textContent`), /1 beat/);
  await set('#progUnit', '2', 'change'); checks++;
  // Backup with the progression, clear, restore from the file.
  await set('#progText', 'Am*3 F G', 'input');
  await click('#progSave'); await until(`/^Saved to P1/.test(document.querySelector('#progStatus').textContent)`);
  await click('#backup'); await until(`/^Backup saved: \\d+ records/.test(document.querySelector('#settingsStatus').textContent)`);
  let file;
  for (let i = 0; i < 100 && !file; i++) { file = fs.readdirSync(downloads).find((f) => f.endsWith('.json')); if (!file) await sleep(100); }
  const backup = JSON.parse(fs.readFileSync(path.join(downloads, file), 'utf8'));
  assert.ok(backup.records.some((r) => r.kind === 5 && r.slot === 0) && backup.firmware === '0.11.0'); checks++;
  await click('#progClear'); await until(`document.querySelector('#progStatus').textContent === 'P1 is empty now.'`);
  assert.equal(await js(`window.fakeBoard.records['5/0'][0] >>> 24`), 0); checks++;
  const doc = await send('DOM.getDocument');
  const node = await send('DOM.querySelector', { nodeId: doc.result.root.nodeId, selector: '#restoreFile' });
  await send('DOM.setFileInputFiles', { nodeId: node.result.nodeId, files: [path.join(downloads, file)] });
  await until(`/^Restored \\d+ of \\d+ records\\.$/.test(document.querySelector('#settingsStatus').textContent)`);
  await until(`document.querySelector('#progText').value === 'Am*3 F G'`);
  assert.equal(await js(`window.fakeBoard.records['5/0'][0] >>> 24`), 3); checks++;
  // The rest of the editor.
  await click('#libUse'); await until(`/now plays/.test(document.querySelector('#libStatus').textContent)`);
  await click('#libFav'); await click('#favSave'); await until(`/^Saved \\d+ favourites/.test(document.querySelector('#favStatus').textContent)`);
  await click('#save'); await until(`/^Saved to U1/.test(document.querySelector('#scaleStatus').textContent)`); checks++;
  // The installer: step 2 saves the board's settings.
  await send('Page.navigate', { url: site + 'install.html' }); await until(`document.readyState === 'complete' && !!document.querySelector('#save')`);
  await click('#save'); await until(`/records saved from TBOSS 0\\.11\\.0/.test(document.querySelector('#saved').textContent)`); checks++;
  assert.deepEqual(problems.filter((p) => !/favicon/.test(p)), [], 'no errors in the pages');
} finally {
  ws.close(); chrome.kill(); server.close();
  await sleep(300); try { fs.rmSync(work, { recursive: true, force: true }); } catch { /* the browser may still hold a file */ }
}
console.log(`PASS: pages in ${path.basename(browser)}: ${checks} groups of checks (connect, the Progressions panel: typing, borrowed chords, save, errors, tabs, unit; backup with progressions, clear, restore from the file; scale library, favourites, custom scales; the installer saving settings), no page errors`);
