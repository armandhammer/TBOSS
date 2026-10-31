// SPDX-License-Identifier: GPL-3.0-or-later - the editor and installer pages in a real browser: a headless
// Chrome or Edge (a throwaway profile, over the DevTools protocol) opens webapp/ from a local server, with a
// stand-in board (Web MIDI answered from memory, TBOSS 1.0.0) put in before the pages' scripts. Clicks
// through connecting, typing and saving a progression, errors, tabs, a settings backup, clearing and
// restoring, chord sets (and a progression recorded on one) and the center buttons, the scale library, favorites and custom scales, and the
// installer's "Save settings"; fails on
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
  const board = { records: {}, previews: [], setItems: [], scales: Array.from({ length: 12 }, () => ({ mask: 0, name: '' })) };
  // TBOSS 1.0.0, or 1.9.3 with a small menu (MENU, ITEM, SET_ITEM) and the content editors' requests (VALUES,
  // SET_EDITS, CHORD_NAMES) when the test asks for it; 1.9.4 with STATE; 2.0.0 the same, named 2.0.0.
  const fake = sessionStorage.getItem('fake-tboss'), v20 = fake === '2.0.0', v194 = v20 || fake === '1.9.4', v19 = v194 || fake === '1.9.3';
  window.confirm = () => true; // the editor asks before overwriting or clearing
  const menu = [{ name: 'PLAY', items: [{ id: 12, name: 'CHORD VEL', kind: 0, labels: ['NORMAL', 'FIXED 100', 'LIGHT', 'FIRM'], value: 0 },
    { id: 77, name: 'RESET OPTIONS', kind: 3, labels: [''], value: 0 }] },
  { name: 'MIDI', items: [{ id: 45, name: 'TRANSPORT', kind: 1, labels: ['STOPPED', 'RUNNING'], value: 0 },
    { id: 117, name: 'GLIDE TIME', kind: 0, labels: ['25 MS', '50 MS', '100 MS'], value: 2, hidden: true }] },
  { name: 'SEQUENCE', items: [{ id: 115, name: 'PLAY', kind: 1, labels: ['STOPPED', 'PLAYING'], value: 0 }] },
  { name: 'PRESETS', items: [{ id: 13, name: 'LOAD PRESET', kind: 2, labels: ['1', '2 EMPTY', '3 EMPTY', '4 EMPTY', '5 EMPTY', '6 EMPTY', '7 EMPTY', '8 EMPTY'], value: 0 },
    { id: 14, name: 'SAVE PRESET', kind: 2, labels: ['1', '2 EMPTY', '3 EMPTY', '4 EMPTY', '5 EMPTY', '6 EMPTY', '7 EMPTY', '8 EMPTY'], value: 0 },
    { id: 121, name: 'KEEP LOOK', kind: 0, labels: ['OFF', 'ON'], value: 0 }, { id: 122, name: 'KEEP FEEL', kind: 0, labels: ['OFF', 'ON'], value: 1 }] }];
  const ascii = (s) => [...s].map((c) => c.charCodeAt(0));
  if (v19) board.records['8/9'] = [1, ...new Array(39).fill(0)]; // the look on show: Default, built-in colors
  if (v19) { // the feel (kind 9): the default curves, as the firmware sends them
    const lin = [0, 18, 36, 54, 73, 91, 109, 127], bytes = [0, 1, 2, 3].flatMap((i) => [...lin, i < 2 ? 1 : 0, 127]);
    board.records['9/0'] = Array.from({ length: 40 }, (_, w) => (!w ? 1 | 1 << 8 : w <= 10 ? (bytes[4 * w - 4] | bytes[4 * w - 3] << 8 | bytes[4 * w - 2] << 16 | bytes[4 * w - 1] << 24) >>> 0 : 0));
    board.records['9/9'] = [...board.records['9/0']]; // the curves in use (1.9.3): the board's, Keep feel on
  }
  const live = new Array(40).fill(0);
  live[0] = (9 | 1 << 8 | 36 << 16 | 48 << 24) >>> 0; live[39] = 0x56330000; // A, natural minor
  board.records['1/0'] = live;
  if (v19) { board.records['2/0'] = [...live]; board.records['2/0'][1] = 3 << 8; } // preset 1: Chord vel Firm
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
      case 1: return reply(1, 0, [1, 4, 12, 8, 40, 1, ...pack([0xef, 0x40, 0x18, 0, 0, 0, 1]), ...(v20 ? [2, 0, 0] : v194 ? [1, 9, 4] : v19 ? [1, 9, 3] : [1, 0, 0])]);
      case 9: { const p = menu[d[0]]; if (!v19) return reply(9, 4); if (!p) return reply(9, 3);
        return reply(9, 0, [d[0], menu.length, p.items.length, ...p.items.flatMap((it) => [it.id & 127, it.id >> 7]), ...ascii(p.name)]); }
      case 10: { const it = menu.flatMap((p) => p.items).find((x) => x.id === (d[0] | d[1] << 7)), first = d[2] | d[3] << 7;
        if (!v19) return reply(10, 4); if (!it) return reply(10, 3);
        const names = it.labels.slice(first, first + 2); // two at a time: the editor asks again
        return reply(10, 0, [it.id & 127, it.id >> 7, it.kind, it.hidden ? 0 : 1, it.labels.length, 0, it.value, 0, first, 0, names.length,
          ...ascii(it.name), 0, ...names.flatMap((n) => [...ascii(n), 0])]); }
      case 11: { board.setItems.push([d[0] | d[1] << 7, d[2] | d[3] << 7]); // no reply
        const it = menu.flatMap((p) => p.items).find((x) => x.id === (d[0] | d[1] << 7)); if (it && it.kind !== 2) it.value = d[2] | d[3] << 7;
        return undefined; }
      case 13: { if (!v19) return reply(13, 4); // VALUES: the menu's values; a stored preset's Chord vel from its record
        const w = d[0] === 2 ? board.records[`2/${d[1]}`] : board.records['1/0'];
        if (!w) return reply(13, 7, [d[0], d[1]]);
        const values = new Array(123).fill(0);
        for (const it of menu.flatMap((p) => p.items)) values[it.id] = it.value;
        if (d[0] === 2) values[12] = (w[1] >>> 8) & 255;
        return reply(13, 0, [d[0], d[1], 123, 0, ...values.flatMap((v) => [v & 127, v >> 7])]); }
      case 14: { if (!v19) return reply(14, 4); // SET_EDITS, no reply: into the live record
        const b = unpack(d), w = board.records['1/0'];
        for (let k = 0; k < 48; k++) w[5 + (k >> 2)] = ((w[5 + (k >> 2)] & ~(255 << (8 * (k & 3)))) | b[k] << (8 * (k & 3))) >>> 0;
        w[21] = (b[48] | b[49] << 8 | b[50] << 16 | b[51] << 24) >>> 0; w[22] = b[52] | b[53] << 8;
        w[29] = (b[54] | b[55] << 8 | b[56] << 16 | b[57] << 24) >>> 0; w[30] = b[58] | b[59] << 8;
        board.edits = (board.edits || 0) + 1; return undefined; }
      case 15: { if (!v19) return reply(15, 4); // CHORD_NAMES: "P1".."P48", "7" added where the 7th is
        const w = board.records['1/0'];
        return reply(15, 0, [48, ...Array.from({ length: 48 }, (_, k) => [...ascii(`P${k + 1}${(w[5 + (k >> 2)] >>> (8 * (k & 3))) & 64 ? '7' : ''}`), 0]).flat()]); }
      case 16: { board.stateCalls = (board.stateCalls || 0) + 1; if (!v194) return reply(16, 4); // STATE (1.9.4): a check of the values and where they show, counts 0
        const items = menu.flatMap((p) => p.items), five = (v) => [0, 1, 2, 3, 4].map((k) => (v >>> (7 * k)) & 127);
        const check = items.reduce((h, it) => ((h * 31 + it.id * 7 + it.value * 2 + (it.hidden ? 0 : 1)) >>> 0), 17);
        const shown = new Array(123).fill(0); for (const it of items) shown[it.id] = it.hidden ? 0 : 1;
        const bits = []; for (let i = 0; i < 123; i += 7) bits.push(shown.slice(i, i + 7).reduce((b, x, k) => b | x << k, 0));
        return reply(16, 0, [...five(check), ...five(0), ...five(board.progChanges || 0), ...five(board.changes || 0), 123, 0, ...bits]); }
      case 12: if (!v19) return reply(12, 4); // PADS: rest 300 + 10 per pad, the reading 40 above it
        return reply(12, 0, [96, 3, 7, 3, 25, 0, ...Array.from({ length: 96 }, (_, k) => { const rest = k === 7 ? 1500 : 300 + 10 * k, v = rest + 40;
          return [rest & 127, rest >> 7, v & 127, v >> 7]; }).flat()]);
      case 2: { const s = board.scales[d[0]]; if (!s) return reply(2, 3);
        return reply(2, 0, [d[0], s.mask & 127, s.mask >> 7, ...Array.from({ length: 12 }, (_, i) => (i < s.name.length ? s.name.charCodeAt(i) : 0))]); }
      case 3: board.scales[d[0]] = { mask: d[1] | d[2] << 7, name: String.fromCharCode(...d.slice(3).filter((c) => c)) }; return reply(3, 0, [d[0]]);
      case 4: board.previews.push(d[0] | d[1] << 7); return undefined; // preview: no reply
      case 5: { const w = board.records['1/0']; w[0] = ((w[0] & ~0xff00) | (d[0] << 8)) >>> 0; return reply(5, 0); }
      case 6: { let w = board.records[`${d[0]}/${d[1]}`];
        if (d[0] === 3) { const s = board.scales[d[1]]; w = s && s.mask ? [s.mask, ...new Array(39).fill(0)] : null; }
        if ([2, 8, 9].includes(d[0]) && w && w.every((x) => !x)) w = null; // 1.9.3: a cleared preset, look or curves
        return w ? reply(6, 0, [d[0], d[1], ...pack(w2b(w))]) : reply(6, 7, [d[0], d[1]]); }
      case 7: { const quiet = d.length === 3 + 183, w = b2w(unpack(d.slice(quiet ? 3 : 2))); // 1.9.0: kind, slot, flags 1: no reply
        if (d[0] === 3) board.scales[d[1]] = { mask: w[0], name: 'RESTORED' }; else board.records[`${d[0]}/${d[1]}`] = w;
        if (quiet) { board.quiet = (board.quiet || 0) + 1; return undefined; }
        return reply(7, 0, [d[0], d[1]]); }
      default: return reply(cmd, 4);
    }
  };
  const output = { id: 'out', name: 'TheoryBoard MIDI', type: 'output', state: 'connected', send(m) { handle(Array.from(m)); } };
  window.fakeBoard = board; window.fakeBoardMenu = menu; // the test changes the menu as the board would (1.9.4)
  Object.defineProperty(navigator, 'requestMIDIAccess', { configurable: true,
    value: async () => ({ inputs: new Map([['in', input]]), outputs: new Map([['out', output]]), onstatechange: null, sysexEnabled: true }) });
}

// The site as GitHub Pages publishes it (scripts/build_site.py: the project page, the manual, the editor and the
// installer) on a local port, with a stand-in firmware image.
const built = fs.mkdtempSync(path.join(os.tmpdir(), 'tb-site-'));
const python = ['python3', 'python'].find((p) => spawnSync(p, ['--version']).status === 0);
assert.ok(python, 'python for scripts/build_site.py');
assert.equal(spawnSync(python, [path.join(root, 'scripts/build_site.py'), '--out', path.join(built, 'site')], { stdio: 'ignore' }).status, 0);
const siteDir = path.join(built, 'site');
const types = { '.html': 'text/html', '.js': 'text/javascript', '.json': 'application/json', '.css': 'text/css', '.jpg': 'image/jpeg' };
const image = Buffer.alloc(4096, 0xff);
const server = http.createServer((req, res) => {
  const where = new URL(req.url, 'http://x').pathname;
  if (where === '/firmware/tboss.bin' || where === '/firmware/tboss.json') {
    const body = where.endsWith('.bin') ? image : JSON.stringify({ version: 'test', file: 'tboss.bin', bytes: image.length,
      sha256: crypto.createHash('sha256').update(image).digest('hex') });
    res.writeHead(200, { 'content-type': where.endsWith('.bin') ? 'application/octet-stream' : 'application/json' }); res.end(body); return;
  }
  let file = path.join(siteDir, decodeURIComponent(new URL(req.url, 'http://x').pathname).replace(/\/$/, '/index.html'));
  if (!path.extname(file) && fs.existsSync(file + '.html')) file += '.html'; /* as GitHub Pages: /install is install.html */
  if (!file.startsWith(siteDir) || !fs.existsSync(file)) { res.writeHead(404); res.end(); return; }
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
  throw new Error('timed out waiting for: ' + expression + '\n' + problems.join('\n') + '\n' + await js(`[...document.querySelectorAll('.status')].map(e => e.id + '=' + e.textContent).join(' | ')`));
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
  await send('Page.navigate', { url: site + 'editor' }); await until(`document.readyState === 'complete' && !!document.querySelector('#progTabs button')`);
  await click('#connect');
  await until(`document.querySelector('#conn').textContent === 'Connected' && /TBOSS 1\\.0\\.0/.test(document.querySelector('#facts').textContent)`);
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
  assert.ok(backup.records.some((r) => r.kind === 5 && r.slot === 0) && backup.firmware === '1.0.0'); checks++;
  await click('#progClear'); await until(`document.querySelector('#progStatus').textContent === 'P1 is empty now.'`);
  assert.equal(await js(`window.fakeBoard.records['5/0'][0] >>> 24`), 0); checks++;
  const doc = await send('DOM.getDocument');
  const node = await send('DOM.querySelector', { nodeId: doc.result.root.nodeId, selector: '#restoreFile' });
  await send('DOM.setFileInputFiles', { nodeId: node.result.nodeId, files: [path.join(downloads, file)] });
  await until(`/^Restored \\d+ of \\d+ records\\.$/.test(document.querySelector('#settingsStatus').textContent)`);
  await until(`document.querySelector('#progText').value === 'Am*3 F G'`);
  assert.equal(await js(`window.fakeBoard.records['5/0'][0] >>> 24`), 3); checks++;
  // Chord sets (1.0.0): an example, an error, saved to set 2 in D, used on the board, named in another key.
  await until(`document.querySelector('#setHint').classList.contains('hidden')`);
  assert.match(await text('#setTabs'), /1 · empty/);
  // The circle of fifths examples: every chord taken, in circle order.
  for (const [i, name, first, last] of [[4, 'FIFTHS MAJOR', 'C', 'F'], [5, 'FIFTHS MINOR', 'Am', 'Dm']]) {
    await set('#setStart', String(i), 'change');
    assert.equal(await text('#setStatus'), '12 chords on 12 columns.');
    assert.equal(await js(`document.querySelector('#setName').value`), name);
    const shown = await js(`[...document.querySelectorAll('#setChords .chord b')].map(e => e.textContent)`);
    assert.deepEqual([shown.length, shown[0], shown[11]], [12, first, last]);
  }
  await set('#setStart', '1', 'change');
  assert.equal(await js(`document.querySelectorAll('#setChords .chord').length`), 12);
  assert.equal(await text('#setStatus'), '12 chords on 12 columns.'); checks++;
  await set('#setText', 'C G/B', 'input'); assert.match(await text('#setStatus'), /no slash chords/);
  assert.equal(await js(`document.querySelector('#setSave').disabled`), true); checks++;
  await js(`document.querySelectorAll('#setTabs button')[1].click()`);
  await set('#setKey', '2', 'change'); await set('#setName', 'two', 'input'); await set('#setText', "D - A7' Gmaj7", 'input');
  assert.deepEqual(await js(`[...document.querySelectorAll('#setChords .chord b')].slice(0, 4).map(e => e.textContent)`), ['D', '–', 'A7', 'Gmaj7']);
  await click('#setSave'); await until(`/^Saved to set 2/.test(document.querySelector('#setStatus').textContent)`);
  const storedSet = await js(`window.fakeBoard.records['6/1']`);
  assert.deepEqual(storedSet.slice(0, 5), [1, 0x91, 0, 0x491 | 19 << 12, 0x891 | 5 << 12], 'roots above the key: 0, 19, 5');
  assert.match(await text('#setTabs'), /2 · TWO/); checks++;
  await click('#setUse'); await until(`/play set 2 now/.test(document.querySelector('#setStatus').textContent)`);
  assert.equal(await js(`window.fakeBoard.records['1/0'][34] & 255`), 2); checks++;
  await set('#setKey', '0', 'change'); assert.equal(await js(`document.querySelector('#setText').value`), "D - A7' Gmaj7", 'the text stays as typed');
  await js(`document.querySelectorAll('#setTabs button')[1].click()`);
  assert.equal(await js(`document.querySelector('#setText').value`), "C - G7' Fmaj7", 'the stored set named in C'); checks++;
  await click('#setOff'); await until(`/scale's chords again/.test(document.querySelector('#setStatus').textContent)`);
  assert.equal(await js(`window.fakeBoard.records['1/0'][34] & 255`), 0); checks++;
  // A progression recorded on the board with set 2: listed from the set (key A: A, Dmaj7), not as
  // text, so Save cannot turn it into the scale's chords; typing replaces it.
  await js(`window.fakeBoard.records['5/1'] = [(1 | 2 << 8 | 1 << 16 | 2 << 24) >>> 0, (3 | 50 << 11 | 127 << 25) >>> 0,
    (15 | 1 << 6 | 50 << 11 | 127 << 25) >>> 0, ...new Array(36).fill(0), 2]`);
  await click('#connect'); await until(`/P2 · 2 chords/.test(document.querySelector('#progTabs').textContent)`);
  await js(`document.querySelectorAll('#progTabs button')[1].click()`);
  assert.equal(await js(`document.querySelector('#progText').value`), '');
  assert.deepEqual(await js(`[...document.querySelectorAll('#progChords .chord b')].map(e => e.textContent)`), ['A', 'Dmaj7']);
  assert.match(await text('#progStatus'), /recorded on the TB with chord set 2 \(TWO\)/);
  assert.equal(await js(`document.querySelector('#progSave').disabled`), true);
  await set('#progText', 'Am', 'input'); assert.equal(await js(`document.querySelector('#progSave').disabled`), false);
  await js(`document.querySelectorAll('#progTabs button')[0].click()`); checks++;
  // Center buttons: button 14 becomes Shift, button 1 the arpeggiator; saved into the live settings.
  await until(`document.querySelectorAll('#btnList select').length === 14`);
  await js(`(() => { const s = document.querySelectorAll('#btnList select'); s[13].value = '14'; s[13].dispatchEvent(new Event('change')); s[0].value = '16'; s[0].dispatchEvent(new Event('change')); })()`);
  await click('#btnSave'); await until(`/^Saved: 2 buttons changed/.test(document.querySelector('#btnStatus').textContent)`);
  const liveWords = await js(`window.fakeBoard.records['1/0']`);
  assert.deepEqual([liveWords[35], liveWords[38], liveWords[34] & 255, liveWords[0] & 255], [17, 15 << 8, 0, 9], 'only the buttons changed');
  // Button 2 set on the TB after the panel was read: saving button 3 from the page keeps it.
  await js(`window.fakeBoard.records['1/0'][35] = (17 | 21 << 8) >>> 0`);
  await js(`(() => { const s = document.querySelectorAll('#btnList select')[2]; s.value = '19'; s.dispatchEvent(new Event('change')); })()`);
  await click('#btnSave'); await until(`/^Saved: 4 buttons changed/.test(document.querySelector('#btnStatus').textContent)`);
  assert.equal(await js(`window.fakeBoard.records['1/0'][35]`), (17 | 21 << 8 | 20 << 16) >>> 0, "the TB's own change kept");
  assert.equal(await js(`document.querySelectorAll('#btnList select')[1].value`), '20');
  await click('#btnReset'); await click('#btnSave'); await until(`/every button does what is printed/.test(document.querySelector('#btnStatus').textContent)`);
  assert.deepEqual((await js(`window.fakeBoard.records['1/0']`)).slice(35, 39), [0, 0, 0, 0]); checks++;
  // The scale preview takes over the TB's screen and pads: none while the other panels are used;
  // a change to a scale starts it, using another panel ends it.
  assert.deepEqual(await js(`window.fakeBoard.previews`), []);
  await js(`document.querySelector('#piano .key:not(.outside):not(.root)').click()`);
  assert.ok(await js(`window.fakeBoard.previews.length === 1 && window.fakeBoard.previews[0] !== 0`), 'a scale change previews');
  await js(`document.querySelector('#scales').dispatchEvent(new PointerEvent('pointerdown', { bubbles: true }))`);
  assert.equal(await js(`window.fakeBoard.previews.length`), 1, 'a touch in the scale panels keeps it');
  await js(`document.querySelector('#setText').dispatchEvent(new PointerEvent('pointerdown', { bubbles: true }))`);
  await until(`JSON.stringify(window.fakeBoard.previews.slice(1)) === '[0]'`, 2000); // another panel ends it (after a poll's request)
  await js(`document.querySelector('#btnList select').dispatchEvent(new PointerEvent('pointerdown', { bubbles: true }))`);
  assert.equal(await js(`window.fakeBoard.previews.length`), 2, 'ended once'); checks++;
  // The rest of the editor.
  await click('#libUse'); await until(`/now plays/.test(document.querySelector('#libStatus').textContent)`);
  await click('#libFav'); await click('#favSave'); await until(`/^Saved \\d+ favorites/.test(document.querySelector('#favStatus').textContent)`);
  await click('#save'); await until(`/^Saved to U1/.test(document.querySelector('#scaleStatus').textContent)`); checks++;
  // The installer: step 2 saves the board's settings.
  await send('Page.navigate', { url: site + 'install' }); await until(`document.readyState === 'complete' && !!document.querySelector('#save')`);
  await click('#save'); await until(`/records saved from TBOSS 1\\.0\\.0/.test(document.querySelector('#saved').textContent)`); checks++;
  // The project page: the released version from the manifest, the manual's newest news, links that land.
  await send('Page.navigate', { url: site }); await until(`document.readyState === 'complete' && document.querySelector('#version').textContent === 'test'`);
  assert.equal(await js(`document.querySelector('#version').closest('span').hidden`), false);
  assert.match(await js(`document.querySelector('.news h2').textContent`), /^New in \d/);
  assert.deepEqual(await js(`['manual', 'install', 'editor'].map(h => !!document.querySelector('.hero a[href="' + h + '"]'))`), [true, true, true]); checks++;
  // The manual: the section shown lights in the contents, the filter, the theme, the drawer on a phone.
  await send('Page.navigate', { url: site + 'manual#guitar-voicing' });
  await until(`document.readyState === 'complete' && document.querySelector('.toc a.active')?.textContent === 'Guitar voicing'`);
  assert.equal(await js(`document.querySelector('.toc a.active').closest('ul').closest('li').classList.contains('open')`), true);
  await set('.toc-filter', 'pedal', 'input');
  assert.deepEqual(await js(`[...document.querySelectorAll('.toc li:not(.hidden) > a')].map(a => a.textContent)`), ['Sustain pedal']);
  await set('.toc-filter', '', 'input');
  assert.equal(await js(`document.querySelectorAll('.toc li.hidden').length`), 0);
  assert.equal(await js(`document.querySelector('.toc a').textContent`), 'Roadmap', 'the roadmap first'); checks++;
  const before = await js(`getComputedStyle(document.body).backgroundColor`);
  await click('[data-theme-toggle]');
  assert.notEqual(await js(`getComputedStyle(document.body).backgroundColor`), before, 'the theme changes');
  await click('[data-theme-toggle]'); checks++;
  await send('Emulation.setDeviceMetricsOverride', { width: 390, height: 844, deviceScaleFactor: 1, mobile: true });
  await until(`getComputedStyle(document.querySelector('[data-contents]')).display !== 'none'`);
  await click('[data-contents]'); assert.equal(await js(`document.querySelector('.sidebar').classList.contains('open')`), true);
  await js(`[...document.querySelectorAll('.toc a')].find(a => a.textContent === 'Arpeggiator').click()`);
  await until(`!document.querySelector('.sidebar').classList.contains('open') && location.hash === '#arpeggiator'`);
  assert.equal(await js(`document.documentElement.scrollWidth <= innerWidth`), true, 'no sideways scrolling on a phone');
  await send('Emulation.clearDeviceMetricsOverride'); checks++;
  // 1.9.0: the editor's tabs and its Settings tab, built from the board's menu.
  await send('Emulation.clearDeviceMetricsOverride');
  await send('Page.navigate', { url: site + 'editor#settings' }); await until(`document.readyState === 'complete'`);
  await js(`sessionStorage.setItem('fake-tboss', '1.9.3')`);
  await send('Page.reload'); await until(`document.readyState === 'complete' && !!document.querySelector('#mainTabs')`);
  assert.equal(await js(`document.querySelector('#settings').classList.contains('shown') && !document.querySelector('#library').classList.contains('shown')`), true);
  await click('#connect'); await until(`document.querySelectorAll('#settingsBody .setting').length === 2`);
  assert.deepEqual(await js(`[...document.querySelectorAll('#settingsBody [role=tab]')].map(b => b.textContent)`), ['Play', 'MIDI', 'Sequence', 'Presets']);
  assert.deepEqual(await js(`[...document.querySelectorAll('#settingsBody .setting select option')].map(o => o.textContent)`), ['NORMAL', 'FIXED 100', 'LIGHT', 'FIRM'], 'every value, asked for in parts');
  assert.match(await js(`document.querySelector('#settingsBody .setting').textContent`), /Chord vel.*Default: Normal.*On the TB: Settings → Play → Chord vel/s);
  assert.match(await js(`document.querySelectorAll('#settingsBody .setting')[1].textContent`), /on the TB only/);
  await set('#settingsBody .setting select', '3', 'change'); await until(`window.fakeBoard.setItems.length === 1`);
  assert.deepEqual(await js(`window.fakeBoard.setItems[0]`), [12, 3]);
  await until(`/Sent to the TB: CHORD VEL FIRM/.test(document.querySelector('#settingsBody .setting .said.pop').textContent)`);
  await js(`[...document.querySelectorAll('#settingsBody [role=tab]')][1].click()`);
  assert.equal(await js(`document.querySelectorAll('#settingsBody .setting.unused').length`), 1, 'Glide time dimmed');
  await js(`document.querySelector('#settingsBody .setting button').click()`); await until(`window.fakeBoard.setItems.length === 2`);
  assert.deepEqual(await js(`window.fakeBoard.setItems[1]`), [45, 1]);
  assert.equal(await js(`document.querySelector('#settingsBody .setting button').textContent`), 'Stop');
  // The Lights tab: the look on show, a scheme and a color sent quietly, the picture of the pads.
  await js(`document.querySelector('#mainTabs [data-tab=lights]').click()`);
  await until(`document.querySelectorAll('#lightsBody .schemes button').length === 7`);
  assert.equal(await js(`document.querySelectorAll('#lightsBody .board-picture span').length`), 96);
  assert.equal(await js(`document.querySelectorAll('#lightsBody .role').length`), 12, "Default's twelve notes");
  await js(`[...document.querySelectorAll('#lightsBody .schemes button')][3].click()`);
  await until(`window.fakeBoard.records['8/9'][0] === (1 | 3 << 8)`);
  assert.equal(await js(`document.querySelectorAll('#lightsBody .role').length`), 3, "Piano's three");
  await js(`document.querySelectorAll('#lightsBody .role')[2].querySelectorAll('button.swatch')[12].click()`); // Root: White
  await until(`((window.fakeBoard.records['8/9'][7] >>> 16) & 255) === 13`); // role 26 = byte 2 of word 7, palette 13 = white
  assert.ok(await js(`window.fakeBoard.quiet >= 2`), 'sent without replies');
  if (process.env.TB_SHOT) { // TB_SHOT=file.png: a picture of the Lights tab, to look at
    const shot = await send('Page.captureScreenshot', { format: 'png', captureBeyondViewport: true });
    fs.writeFileSync(process.env.TB_SHOT, Buffer.from(shot.result.data, 'base64'));
  }
  // The Feel tab: the chord velocity curve, a point raised with the keyboard (sent quietly), Custom for the
  // chords, the pads' resting levels.
  await js(`document.querySelector('#mainTabs [data-tab=feel]').click()`);
  await until(`document.querySelectorAll('#feelBody svg.curve circle.point').length === 8`);
  const quietBefore = await js(`window.fakeBoard.quiet || 0`);
  await js(`document.querySelectorAll('#feelBody svg.curve circle.point')[3].dispatchEvent(new KeyboardEvent('keydown', { key: 'ArrowUp', bubbles: true }))`);
  await until(`(window.fakeBoard.quiet || 0) > ${quietBefore} && (window.fakeBoard.records['9/9'][1] >>> 24) === 58`); // point 4: 54 + 4
  assert.equal(await js(`window.fakeBoard.records['9/9'][0]`), 1 | 1 << 8, 'the curves in use (1.9.3), Keep feel kept');
  // 1.9.4: a drag follows the pointer all the way (each move draws the curve again): halfway, then to the top = 127.
  await js(`(() => { const svg = document.querySelector('#feelBody svg.curve'), r = svg.getBoundingClientRect();
    const p = document.querySelectorAll('#feelBody svg.curve circle.point')[3], b = p.getBoundingClientRect();
    p.dispatchEvent(new PointerEvent('pointerdown', { bubbles: true, clientX: b.x + 3, clientY: b.y + 3, pointerId: 1 }));
    window.dispatchEvent(new PointerEvent('pointermove', { clientX: b.x + 3, clientY: r.top + r.height / 2, pointerId: 1 }));
    window.dispatchEvent(new PointerEvent('pointermove', { clientX: b.x + 3, clientY: r.top - 20, pointerId: 1 }));
    window.dispatchEvent(new PointerEvent('pointerup', { clientX: b.x + 3, clientY: r.top - 20, pointerId: 1 })); })()`);
  await until(`(window.fakeBoard.records['9/9'][1] >>> 24) === 127`);
  assert.match(await js(`document.querySelector('#feelBody [data-role=whose]').textContent`), /board's curves/i);
  await js(`document.querySelector('#feelBody [data-role=use]').click()`);
  await until(`window.fakeBoard.setItems.some(([id, v]) => id === 12 && v === 6)`); // Chord vel → Custom
  await js(`document.querySelector('#feelBody [data-role=mapRead]').click()`);
  await until(`document.querySelectorAll('#feelBody [data-role=map] span').length === 96`);
  assert.match(await js(`document.querySelector('#feelBody [data-role=mapFacts]').textContent`), /Resting levels 300 to 1500/);
  if (process.env.TB_SHOT_FEEL) { // TB_SHOT_FEEL=file.png: a picture of the Feel tab, to look at
    const shot = await send('Page.captureScreenshot', { format: 'png', captureBeyondViewport: true });
    fs.writeFileSync(process.env.TB_SHOT_FEEL, Buffer.from(shot.result.data, 'base64'));
  }
  assert.equal(await js(`document.querySelectorAll('#feelBody [data-role=map] span.high').length`), 1, 'the pad more than 500 above the middle');
  // 1.9.3, the Sequencer tab: four lanes of 64 steps; a click adds a step (a quiet lane record), its velocity; a
  // Euclid fill; Play through SET_ITEM.
  await js(`document.querySelector('#mainTabs [data-tab=sequencer]').click()`);
  await until(`document.querySelectorAll('#sequencerBody .lane').length === 4 && document.querySelectorAll('#sequencerBody .step').length === 256`);
  await js(`document.querySelectorAll('#sequencerBody .lane')[0].querySelectorAll('.step')[4].click()`);
  await until(`!!window.fakeBoard.records['7/0'] && (window.fakeBoard.records['7/0'][9] & 1) === 1`); // step 5: words 9, 10
  assert.equal((await js(`window.fakeBoard.records['7/0'][9]`) >>> 1) & 127, 3, 'a new chord step: the bottom-left pad');
  assert.match(await js(`document.querySelector('#sequencerBody [data-role=stepTitle]').textContent`), /Lane 1, step 5/);
  await set('#sequencerBody .step-options input[type=number]', '90', 'change');
  await until(`((window.fakeBoard.records['7/0'][9] >>> 8) & 127) === 90`);
  await js(`(() => { const lane = document.querySelectorAll('#sequencerBody .lane')[1]; const h = lane.querySelector('.euclid input');
    h.value = '4'; [...lane.querySelectorAll('.euclid button')][0].click(); })()`);
  await until(`!!window.fakeBoard.records['7/4'] && [1, 9, 17, 25].every((w) => window.fakeBoard.records['7/4'][w] & 1) && window.fakeBoard.records['7/4'][35] === 4`);
  assert.equal(await js(`window.fakeBoard.records['7/4'][3] & 1`), 0, 'step 2 stays off');
  await js(`document.querySelector('#sequencerBody [data-role=play]').click()`);
  await until(`window.fakeBoard.setItems.some(([id, v]) => id === 115 && v === 1)`);
  assert.equal(await js(`document.querySelector('#sequencerBody [data-role=play]').textContent`), '■ Stop'); checks++;
  // 2.0: lane 2's steps one later (1 5 9 13 → 2 6 10 14), copied and pasted onto lane 3; on lane 3, steps 2-6 copied
  // (shift-click) and pasted from step 9 (9 and 13 on, 10 taken away). All sent as quiet lane records.
  const laneTool = (label) => `document.querySelector('#sequencerBody [aria-label="${label}"]').click()`;
  await js(laneTool('Move the steps one later (lane 2)'));
  await until(`[3, 11, 19, 27].every((w) => window.fakeBoard.records['7/4'][w] & 1) && !(window.fakeBoard.records['7/4'][1] & 1)`);
  await js(laneTool('Copy this lane (lane 2)'));
  await until(`!document.querySelector('#sequencerBody [aria-label="Paste lane 2 here: its steps and settings, its length too (lane 3)"]').disabled`);
  await js(laneTool('Paste lane 2 here: its steps and settings, its length too (lane 3)'));
  await until(`!!window.fakeBoard.records['7/8'] && [3, 11, 19, 27].every((w) => window.fakeBoard.records['7/8'][w] & 1) && window.fakeBoard.records['7/8'][33] === window.fakeBoard.records['7/4'][33]`);
  const laneSteps = `document.querySelectorAll('#sequencerBody .lane')[2].querySelectorAll('.step')`;
  await js(`${laneSteps}[1].click()`); // choose step 2 (on)
  await js(`${laneSteps}[5].dispatchEvent(new MouseEvent('click', { shiftKey: true, bubbles: true }))`);
  assert.equal(await js(`${laneSteps}.length && [...${laneSteps}].filter((b) => b.classList.contains('inrange')).length`), 5);
  await js(`[...document.querySelectorAll('#sequencerBody button')].find((b) => b.textContent === 'Copy steps 2–6').click()`);
  await js(`${laneSteps}[8].click()`); // step 9: added and chosen
  await js(`[...document.querySelectorAll('#sequencerBody button')].find((b) => b.textContent === 'Paste 5 steps from step 9').click()`);
  await until(`[17, 25].every((w) => window.fakeBoard.records['7/8'][w] & 1) && !(window.fakeBoard.records['7/8'][19] & 1) && (window.fakeBoard.records['7/8'][27] & 1)`);
  // 2.0.6: messages in the line where the action was (board test X5): the step options' own line
  await until(`/5 steps pasted/.test(document.querySelector('#sequencerBody [data-said=step] .said').textContent)`); checks++;
  // 2.0.3: lane 3's swing 35 % and transpose -2 (word 34 bytes 1 and 3), the chosen step 9 nudged +3/16 (high word bits
  // 14-17), then lane 3 mutated (its steps change, the status says so).
  const pick = (sel, v) => `(() => { const s = ${sel}; s.value = '${v}'; s.dispatchEvent(new Event('change')); })()`;
  await js(pick(`document.querySelector('#sequencerBody [aria-label="Lane 3 swing"]')`, 35));
  await until(`((window.fakeBoard.records['7/8'][34] >>> 8) & 255) === 35`);
  await js(pick(`document.querySelector('#sequencerBody [aria-label="Lane 3 transpose"]')`, -2));
  await until(`(window.fakeBoard.records['7/8'][34] >>> 24) === 254`);
  await js(pick(`[...document.querySelectorAll('#sequencerBody .step-options label')].find((l) => l.textContent.startsWith('Nudge')).querySelector('select')`, 11));
  await until(`((window.fakeBoard.records['7/8'][18] >>> 14) & 15) === 3`);
  const before203 = await js(`JSON.stringify(window.fakeBoard.records['7/8'].slice(1, 33))`);
  const changed203 = `JSON.stringify(window.fakeBoard.records['7/8'].slice(1, 33)) !== ${JSON.stringify(before203)}`;
  for (let tries = 0; tries < 5 && !(await js(changed203)); tries++) { // random: a mutation can leave the steps as they were
    await js(`document.querySelector('#sequencerBody [data-said=lane2] .said').textContent = ''`);
    await js(`[...document.querySelectorAll('#sequencerBody .lane')[2].querySelectorAll('button')].find((b) => b.textContent === 'Mutate').click()`);
    // either answer (Mutate is random), within the 3 s a message shows (tb-protocol.js GOOD_MS)
    await until(`/Lane 3 mutated|Lane 3: no step changed/.test(document.querySelector('#sequencerBody [data-said=lane2] .said').textContent)`); await sleep(300);
  }
  await until(changed203); checks++;
  // The Chords tab's chord pad edits: 48 pads named by the board; a 7th added goes out with SET_EDITS, the names follow.
  await js(`document.querySelector('#mainTabs [data-tab=chords]').click()`);
  await until(`document.querySelectorAll('#padEditsBody .chord-pad').length === 48`);
  assert.equal(await js(`document.querySelector('#padEditsBody .chord-pad b').textContent`), 'P1');
  await js(`document.querySelector('#padEditsBody .chord-pad .pad-tools button').click()`); // pad 1: 7
  await until(`window.fakeBoard.edits === 1 && (window.fakeBoard.records['1/0'][5] & 255) === 64`);
  await until(`document.querySelector('#padEditsBody .chord-pad b').textContent === 'P17'`, 4000);
  await js(`document.querySelectorAll('#padEditsBody .chord-pad')[1].querySelectorAll('.pad-tools button')[5].click()`); // column 2's top pad: sound off
  await until(`window.fakeBoard.edits === 2 && window.fakeBoard.records['1/0'][29] === 1 << 4`); checks++;
  // The Presets tab: preset 1 named and saved, copied to 3, 3 cleared; compared with the TB now; loaded.
  await js(`document.querySelector('#mainTabs [data-tab=presets]').click()`);
  await until(`document.querySelectorAll('#presetsBody .preset').length === 8`);
  assert.equal(await js(`document.querySelectorAll('#presetsBody .preset.empty').length`), 7);
  assert.equal(await js(`document.querySelector('#presetsBody [data-role=keepFeel]').checked`), true);
  await set('#presetsBody .preset input[type=text]', 'ballad', 'input');
  await click('#presetsBody [data-role=saveNames]');
  await until(`!!window.fakeBoard.records['10/0'] && window.fakeBoard.records['10/0'][1] === 0x4c4c4142`); // "BALL"
  await until(`/done/.test(document.querySelector('#presetsBody [data-said=list] .said').textContent)`);
  await js(`(() => { const row = document.querySelector('#presetsBody .preset'); row.querySelector('select').value = '2';
    [...row.querySelectorAll('button')].find((b) => b.textContent === 'Copy').click(); })()`);
  await until(`!!window.fakeBoard.records['2/2'] && window.fakeBoard.records['2/2'][1] === 3 << 8 && /BALLAD/.test(document.querySelectorAll('#presetsBody .preset')[2].querySelector('input').value)`);
  await until(`/done/.test(document.querySelector('#presetsBody [data-said=list] .said').textContent)`);
  await js(`[...document.querySelectorAll('#presetsBody .preset')[2].querySelectorAll('button')].find((b) => b.textContent === 'Clear').click()`);
  await until(`window.fakeBoard.records['2/2'].every((w) => w === 0) && document.querySelectorAll('#presetsBody .preset.empty').length === 7`);
  await until(`/done/.test(document.querySelector('#presetsBody [data-said=list] .said').textContent)`);
  await set('#presetsBody [data-role=a]', 'live', 'change'); await set('#presetsBody [data-role=b]', '0', 'change');
  await click('#presetsBody [data-role=compare]');
  await until(`document.querySelectorAll('#presetsBody table.compare tr.differs').length === 1`);
  assert.match(await js(`document.querySelector('#presetsBody table.compare tr.differs').textContent`), /CHORD VEL/);
  await js(`[...document.querySelector('#presetsBody .preset').querySelectorAll('button')].find((b) => b.textContent === 'Load on the TB').click()`);
  await until(`window.fakeBoard.setItems.some(([id, v]) => id === 13 && v === 0)`); checks++;
  if (process.env.TB_SHOT_CONTENT) { // TB_SHOT_CONTENT=prefix: pictures of the Sequencer, Chords and Presets tabs
    for (const tab of ['sequencer', 'chords', 'presets']) {
      await js(`document.querySelector('#mainTabs [data-tab=${tab}]').click()`); await sleep(300);
      const shot = await send('Page.captureScreenshot', { format: 'png', captureBeyondViewport: true });
      fs.writeFileSync(`${process.env.TB_SHOT_CONTENT}-${tab}.png`, Buffer.from(shot.result.data, 'base64'));
    }
  }
  // 1.9.4: a change made on the board shows within a second (STATE): Transport started and Glide time shown there.
  await js(`sessionStorage.setItem('fake-tboss', '1.9.4'); location.hash = '#settings'`);
  await send('Page.reload'); await until(`document.readyState === 'complete' && !!document.querySelector('#mainTabs')`);
  await click('#connect'); await until(`document.querySelectorAll('#settingsBody .setting').length === 2`);
  await js(`[...document.querySelectorAll('#settingsBody [role=tab]')][1].click()`);
  assert.equal(await js(`document.querySelectorAll('#settingsBody .setting.unused').length`), 1, 'Glide time dimmed');
  await until(`document.querySelector('#settingsBody .setting button')?.textContent === 'Play'`);
  await js(`window.fakeBoardMenu.find((p) => p.name === 'MIDI').items.forEach((it) => { if (it.id === 45) it.value = 1; if (it.id === 117) it.hidden = false; })`);
  await until(`document.querySelector('#settingsBody .setting button')?.textContent === 'Stop' && !document.querySelectorAll('#settingsBody .setting.unused').length`, 3000);
  assert.match(await js(`document.querySelectorAll('#settingsBody .setting .where')[1].textContent`), /Glide time$/); checks++;
  // 2.0.3 (board test U9): changed back on the board, back in the editor (every poll with a change threw before).
  await js(`window.fakeBoardMenu.find((p) => p.name === 'MIDI').items.forEach((it) => { if (it.id === 45) it.value = 0; if (it.id === 117) it.hidden = true; })`);
  await until(`document.querySelector('#settingsBody .setting button')?.textContent === 'Play' && document.querySelectorAll('#settingsBody .setting.unused').length === 1`, 3000);
  await js(`window.fakeBoardMenu.find((p) => p.name === 'MIDI').items.find((it) => it.id === 117).hidden = false`);
  await until(`!document.querySelectorAll('#settingsBody .setting.unused').length`, 3000); checks++;
  // A list chosen from once keeps the focus when it closes; the board's changes still show.
  // One just clicked open keeps its value while it is open, and shows the board's once it closes.
  const glide = `document.querySelector('#settingsBody select[aria-label="GLIDE TIME"]')`;
  const boardGlide = (v) => js(`window.fakeBoardMenu.find((p) => p.name === 'MIDI').items.find((it) => it.id === 117).value = ${v}`);
  await set('#settingsBody select[aria-label="GLIDE TIME"]', '1', 'change'); await js(`${glide}.focus()`);
  await until(`window.fakeBoardMenu.find((p) => p.name === 'MIDI').items.find((it) => it.id === 117).value === 1`);
  await boardGlide(0);
  await until(`document.activeElement === ${glide} && ${glide}.value === '0'`, 3000);
  await js(`${glide}.dispatchEvent(new MouseEvent('mousedown'))`); await boardGlide(2); await sleep(2500);
  assert.equal(await js(`${glide}.value`), '0', 'an open list stays as it is');
  await js(`${glide}.blur()`); await until(`${glide}.value === '2'`, 3000); checks++;
  // 2.0.6 (board test Y6): the Feel tab follows the board while the editor stays in front: preset 8's own curves in use.
  await js(`document.querySelector('#mainTabs [data-tab=feel]').click()`);
  await until(`/board's curves/i.test(document.querySelector('#feelBody [data-role=whose]').textContent)`);
  await js(`window.fakeBoard.records['9/9'][39] = 8; window.fakeBoard.changes = (window.fakeBoard.changes || 0) + 1`);
  await until(`/preset 8 is loaded/i.test(document.querySelector('#feelBody [data-role=whose]').textContent)`, 4000);
  await js(`window.fakeBoard.records['9/9'][39] = 0; window.fakeBoard.changes++`); checks++;
  // 2.0.7 (board test A4): "Curves of" a preset that isn't loaded edits its own record, not the curves playing; one
  // without its own is shown dimmed until it's given its own.
  await js(`window.fakeBoard.records['2/1'] = [1, ...new Array(39).fill(0)]; window.fakeBoard.changes++`); // preset 2 there
  await until(`[...document.querySelectorAll('#feelBody [data-role=target] option')].some((o) => /Preset 2: plays the board's/.test(o.textContent) && !o.disabled)`, 4000);
  await set('#feelBody [data-role=target]', '2', 'change');
  assert.equal(await js(`document.querySelector('#feelBody [data-role=drawing]').classList.contains('dimmed')`), true, 'no own curves: dimmed');
  await click('#feelBody [data-role=giveOwn]');
  await until(`!!window.fakeBoard.records['9/2'] && !document.querySelector('#feelBody [data-role=drawing]').classList.contains('dimmed')`, 4000);
  // 2.0.8 (board test C1): and heard: preset 2's own Chord vel, in its record, Custom; the button says so
  await until(`(window.fakeBoard.records['2/1'][1] >>> 8 & 255) === 6 && /Chord vel in preset 2 is Custom/.test(document.querySelector('#feelBody [data-role=use]').textContent)`, 4000);
  const playing = await js(`JSON.stringify(window.fakeBoard.records['9/9'])`);
  await js(`[...document.querySelectorAll('#feelBody [data-role=shapes] button')].find((b) => b.textContent === 'Firm').click()`);
  await until(`JSON.stringify(window.fakeBoard.records['9/2']) !== JSON.stringify(window.fakeBoard.records['9/0'])`, 4000);
  assert.equal(await js(`JSON.stringify(window.fakeBoard.records['9/9'])`), playing, 'the curves playing untouched');
  assert.match(await js(`document.querySelector('#feelBody [data-role=whose]').textContent`), /saved in preset 2 and heard once you load it/);
  // 2.0.8 (board test B1 step 7): preset 2 loaded, then one without its own: the page goes to the board's curves.
  const whoseText = `document.querySelector('#feelBody [data-role=whose]').textContent`;
  await js(`window.fakeBoard.records['9/9'][39] = 2; window.fakeBoard.changes++`);
  await until(`/Preset 2 is loaded/.test(${whoseText})`, 4000);
  await js(`window.fakeBoard.records['9/9'][39] = 0; window.fakeBoard.changes++`);
  await until(`document.querySelector('#feelBody [data-role=target]').value === '0' && /playing on the TB now/.test(${whoseText})`, 4000); checks++;
  // 2.0.7 (board test A2): a preset saved on the board shows in the Settings tab's preset lists (STATE's check moves).
  await js(`document.querySelector('#mainTabs [data-tab=settings]').click()`);
  await js(`[...document.querySelectorAll('#settingsBody [role=tab]')].find((b) => b.textContent === 'Presets').click()`);
  await until(`[...document.querySelectorAll('#settingsBody select[aria-label="LOAD PRESET"] option')].some((o) => o.textContent === '5 EMPTY')`);
  await js(`(() => { const it = window.fakeBoardMenu.find((p) => p.name === 'PRESETS').items.find((x) => x.name === 'LOAD PRESET');
    it.labels[4] = '5'; it.value = 4; })()`);
  await until(`[...document.querySelectorAll('#settingsBody select[aria-label="LOAD PRESET"] option')].some((o) => o.textContent === '5')`, 4000); checks++;
  // 2.0, the Progressions tab's chain: hidden for 1.9.3; for 2.0.0, links P1 > P1 > P2 saved as the chain record.
  await js(`document.querySelector('#mainTabs [data-tab=progressions]').click()`);
  assert.equal(await js(`document.querySelector('#chainBox').classList.contains('hidden')`), true, 'no chain before 2.0');
  await js(`sessionStorage.setItem('fake-tboss', '2.0.0')`);
  await send('Page.reload'); await until(`document.readyState === 'complete' && !!document.querySelector('#mainTabs')`);
  await js(`document.querySelector('#mainTabs [data-tab=progressions]').click()`);
  await click('#connect'); await until(`!document.querySelector('#chainBox').classList.contains('hidden') && !document.querySelector('[aria-label="Chain link 1"]').disabled`);
  for (const [link, slot] of [[1, 0], [2, 0], [3, 1]]) await set(`[aria-label="Chain link ${link}"]`, String(slot), 'change');
  assert.equal(await js(`document.querySelector('[aria-label="Chain link 5"]').disabled`), true, 'one link after the last');
  await click('#chainSave');
  await until(`!!window.fakeBoard.records['12/0'] && window.fakeBoard.records['12/0'][0] === (1 | 3 << 8) && window.fakeBoard.records['12/0'][1] === 1 << 8`);
  await until(`/Chain saved: P1 > P1 > P2/.test(document.querySelector('#chainStatus').textContent)`); checks++;
  // 2.0.5 (board test W1): a progression recorded on the board shows without Reconnect, once STATE counts it.
  await until(`window.fakeBoard.stateCalls >= 2`, 5000); // the editor's polls under way (the first one is its baseline)
  await js(`window.fakeBoard.records['5/3'] = [(1 | 2 << 8 | 1 << 16 | 2 << 24) >>> 0, (3 | 50 << 11 | 127 << 25) >>> 0,
    (15 | 1 << 6 | 50 << 11 | 127 << 25) >>> 0, ...new Array(37).fill(0)]; window.fakeBoard.progChanges = 1`);
  await until(`/P4 · 2 chords/.test(document.querySelector('#progTabs').textContent)`, 4000); checks++;
  // 2.0.5: the theme list: Medium on the page, Dark for the site's own pages.
  await set('#theme', 'medium', 'change');
  assert.equal(await js(`document.documentElement.dataset.theme + ' ' + localStorage.getItem('tboss-theme')`), 'medium dark');
  await set('#theme', 'light', 'change');
  assert.equal(await js(`getComputedStyle(document.body).backgroundColor`), 'rgb(201, 205, 213)', 'soft gray, not white'); checks++;
  await js(`document.querySelector('#mainTabs [data-tab=scales]').click()`);
  assert.equal(await js(`location.hash === '#scales' && document.querySelector('#library').classList.contains('shown') && !document.querySelector('#settings').classList.contains('shown')`), true);
  await js(`sessionStorage.removeItem('fake-tboss')`); checks++;
  assert.deepEqual(problems.filter((p) => !/favicon/.test(p)), [], 'no errors in the pages');
} finally {
  ws.close(); chrome.kill(); server.close();
  await sleep(300); try { fs.rmSync(work, { recursive: true, force: true }); } catch { /* the browser may still hold a file */ }
  fs.rmSync(built, { recursive: true, force: true });
}
console.log(`PASS: pages in ${path.basename(browser)}: ${checks} groups of checks (connect, the Progressions panel: typing, borrowed chords, save, errors, tabs, unit; backup with progressions, clear, restore from the file; chord sets: example, errors, save, use, named in another key; center buttons; scale library, favorites, custom scales; the installer saving settings; the project page; the manual: contents, filter, theme, the phone drawer; the editor's tabs and its Settings tab from a 1.9 board's menu, the Lights tab, the Feel tab; 1.9.3: the Sequencer tab, the chord pad edits, the Presets tab; 1.9.4: a curve point dragged all the way, changes on the board shown within a second; 2.0: the Sequencer's copy, paste and shift, the chain; 2.0.5: a progression from the board without Reconnect, the theme list; 2.0.6: the Feel tab following the board; 2.0.7: the Feel tab's Curves of, preset lists following the board), no page errors`);
