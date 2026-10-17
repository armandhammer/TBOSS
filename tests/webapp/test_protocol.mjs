// SPDX-License-Identifier: GPL-3.0-or-later - the web editor's protocol module against known values
// and against the real firmware SysEx code (tests/webapp/harness.c, compiled with gcc;
// on Windows through WSL).   node tests/webapp/test_protocol.mjs
import assert from 'node:assert/strict';
import { spawnSync } from 'node:child_process';
import { fileURLToPath } from 'node:url';
import path from 'node:path';
import fs from 'node:fs';
import * as tb from '../../webapp/tb-protocol.js';

const root = path.resolve(path.dirname(fileURLToPath(import.meta.url)), '../..');

// --- pure functions ------------------------------------------------------------
assert.equal(tb.crc32(Buffer.from('123456789')), 0xcbf43926); // standard check value
for (let n = 0; n <= 50; n++) {
  const bytes = Array.from({ length: n }, (_, i) => (i * 37 + n) & 255);
  const packed = tb.pack(bytes);
  assert.equal(packed.length, tb.packedLength(n));
  assert.ok(packed.every((b) => b < 128));
  assert.deepEqual(Array.from(tb.unpack(packed)), bytes);
}
assert.deepEqual(tb.pack([0x80, 1, 0xff]), [0b101, 0, 1, 0x7f]);
const id = tb.identify();
assert.deepEqual(Array.from(id), [0xf0, 0x7d, 0x54, 0x42, 1, 127, 0xf7]);
assert.equal(tb.cleanName('my scale #2 é'), 'MY SCALE #2');
assert.equal(tb.maskOf([0, 2, 4, 5, 7, 9, 11]), 0xab5);
assert.ok(tb.maskOk(0xab5) && !tb.maskOk(0xab4) && !tb.maskOk(0b1011));
assert.deepEqual(tb.scaleNamesFor(0xab5), ['Major (Ionian)']);
assert.equal(tb.KNOWN_SCALES.length, 86);
assert.ok(tb.KNOWN_SCALES.every((s) => tb.maskOk(s.mask)), 'every listed scale is storable');
assert.equal(new Set(tb.KNOWN_SCALES.map((s) => s.id)).size, 86, 'ids unique');
assert.ok(tb.KNOWN_SCALES.every((s) => s.id < 127 && !(s.id >= 12 && s.id < 24)), '7-bit ids, none a user slot');
{ // the library's ids and board names are the firmware's (src/tb/scale_library.h)
  const lib = fs.readFileSync(path.join(root, 'src/tb/scale_library.h'), 'utf8');
  const names = [...lib.match(/library_names\[TC_LIBRARY_SCALES\]=\{([^;]*)\};/)[1].matchAll(/"([^"]*)"/g)].map((m) => m[1]);
  const listed = tb.KNOWN_SCALES.filter((s) => s.id >= tb.LIBRARY_FIRST).sort((a, b) => a.id - b.id);
  assert.deepEqual(listed.map((s) => s.short), names);
  assert.deepEqual(listed.map((s) => s.id), names.map((_, i) => tb.LIBRARY_FIRST + i));
}
// Scale tools: rotation, brightness, find by notes, one note away.
assert.equal(tb.rotateMask(0xab5, 7), tb.maskOf([7, 9, 11, 0, 2, 4, 6]), 'G major');
const modes = ['Lydian', 'Major (Ionian)', 'Mixolydian', 'Dorian', 'Natural minor (Aeolian)', 'Phrygian', 'Locrian'];
const bright = modes.map((n) => tb.brightness(tb.KNOWN_SCALES.find((s) => s.name === n).mask));
assert.ok(bright.every((b, i) => i === 0 || b < bright[i - 1]), 'major modes from bright to dark');
const ceg = tb.scalesContaining(tb.maskOf([0, 4, 7]));
assert.ok(ceg.some((f) => f.root === 0 && f.scale.name === 'Major (Ionian)') && ceg.some((f) => f.root === 5 && f.scale.name === 'Lydian'));
assert.ok(ceg.every((f) => (tb.rotateMask(f.scale.mask, f.root) & 0x91) === 0x91), 'every result holds C, E and G');
assert.ok(tb.noteCount(ceg[0].scale.mask) <= tb.noteCount(ceg[ceg.length - 1].scale.mask), 'fewest notes first');
assert.deepEqual(tb.neighbours(0xab5).map((n) => `${tb.INTERVALS[n.from]}>${tb.INTERVALS[n.to]} ${n.scale.short}`),
  ['3>b3 MEL MINOR', '4>#4 LYDIAN', '5>b6 IONIAN #5', '6>b6 HARM MAJOR', '7>b7 MIXOLYDIAN']);
// Favourites record: ids in order, 0xFF after the last.
const favWords = tb.favouritesToWords([40, 0, 13]);
assert.equal(favWords.length, tb.PAYLOAD_WORDS);
assert.equal(favWords[0], (40 | 0 << 8 | 13 << 16 | 0xff << 24) >>> 0);
assert.ok(favWords.slice(1, 32).every((w) => w === 0xffffffff) && favWords.slice(32).every((w) => w === 0));
assert.deepEqual(tb.favouritesFromWords(favWords), [40, 0, 13]);
assert.throws(() => tb.favouritesToWords([1, 1]), /different/);
assert.throws(() => tb.favouritesToWords(Array.from({ length: 129 }, (_, i) => i % 127)), /128/);
assert.throws(() => tb.request(1, [128]));
{ // the editor's key colours are the board's pad colours (src/tb/ui.c)
  const ui = fs.readFileSync(path.join(root, 'src/tb/ui.c'), 'utf8');
  const table = ui.match(/hues\[12\] = \{([^;]*)\};/)[1];
  const fw = [...table.matchAll(/\{(\d+),(\d+),(\d+)\}/g)].map((m) => [+m[1], +m[2], +m[3]]);
  assert.equal(fw.length, 12);
  assert.deepEqual(tb.PAD_HUES.map((c) => [...c]), fw);
}
assert.deepEqual(Array.from(tb.preview(0xad5, 14)).slice(4, -2), [4, 0x55, 0x15, 2]);      // 3 data bytes
assert.deepEqual(Array.from(tb.preview(0xad5, 2, 1)).slice(4, -2), [4, 0x55, 0x15, 2, 1]);  // + slot
assert.ok(tb.versionAtLeast('0.5.0', 0, 5) && tb.versionAtLeast('1.0.2', 0, 5) && tb.versionAtLeast('0.12.0', 0, 5));
assert.ok(!tb.versionAtLeast('0.4.0', 0, 5) && !tb.versionAtLeast(null, 0, 5) && !tb.versionAtLeast('x', 0, 5));

// --- settings backup logic against a fake board -------------------------------------
{
  const words = (n) => Array.from({ length: tb.PAYLOAD_WORDS }, (_, i) => (n * 1000 + i) >>> 0);
  const stored = new Map([['3/1', words(1)], ['2/0', words(2)], ['2/7', words(3)], ['1/0', words(4)]]);
  const sent = [];
  const fake = async (message) => {
    const cmd = message[4], kind = message[5], slot = message[6];
    sent.push([cmd, kind, slot]);
    if (cmd === tb.CMD.GET_RECORD) {
      const w = stored.get(`${kind}/${slot}`);
      if (!w) return { status: 7, data: [] };
      return { status: 0, data: [kind, slot, ...tb.pack(tb.wordsToBytes(w))] };
    }
    if (cmd === tb.CMD.SET_RECORD) return { status: kind === tb.KIND.PRESET && slot === 7 ? 3 : 0, data: [kind, slot] };
    throw new Error('unexpected command');
  };
  const records = await tb.readRecords(fake);
  assert.deepEqual(records.map((r) => [r.kind, r.slot]), [[3, 1], [2, 0], [2, 7], [1, 0]]);
  assert.deepEqual(records[0].words, words(1));
  assert.equal(sent.length, 3 + 8 + 1, 'every slot asked once');
  const backup = tb.makeBackup(records, '0.3.0');
  const back = tb.parseBackup(JSON.stringify(backup));
  assert.deepEqual(back, records);
  sent.length = 0;
  const shuffled = [records[3], records[1], records[0], records[2]];
  const result = await tb.writeRecords(fake, shuffled);
  assert.deepEqual(sent.map((m) => m[1]), [3, 2, 2, 1], 'scales, then presets, then live settings');
  assert.deepEqual([result.done, result.total, result.failed], [3, 4, ['preset 8: value not accepted']]);
  assert.throws(() => tb.parseBackup('{"format":"x"}'), /not a TheoryBoard settings backup/);
  assert.throws(() => tb.parseBackup(JSON.stringify({ ...backup, records: [{ kind: 1, slot: 0, words: [1] }] })), /damaged/);
  await assert.rejects(tb.readRecords(async () => ({ status: 5, data: [] })), /storage error/);
  // Firmware 0.6.0 has 12 user slots and 40-word records; 20-word records from earlier
  // firmware still restore (the firmware pads them).
  sent.length = 0; await tb.readRecords(fake, 12);
  assert.equal(sent.length, 12 + 8 + 1, 'twelve user slots asked');
  // Firmware 0.7.0 also has favourites (kind 4), backed up after the scales.
  stored.set('4/0', tb.favouritesToWords([40, 9]));
  sent.length = 0; const withFav = await tb.readRecords(fake, 12, true);
  assert.equal(sent.length, 12 + 1 + 8 + 1, 'favourites asked once');
  assert.deepEqual(withFav.map((r) => r.kind), [3, 4, 2, 2, 1]);
  assert.equal(tb.recordLabel(withFav[1]), 'favourite scales');
  assert.deepEqual(tb.parseBackup(JSON.stringify(tb.makeBackup(withFav, '0.7.0'))), withFav);
  sent.length = 0; await tb.writeRecords(fake, [withFav[4], withFav[1], withFav[0]]);
  assert.deepEqual(sent.map((m) => m[1]), [3, 4, 1], 'favourites restored after the scales');
  const old = [{ kind: 1, slot: 0, words: words(5).slice(0, tb.PAYLOAD_WORDS_V2) }];
  assert.deepEqual(tb.parseBackup(JSON.stringify(tb.makeBackup(old, '0.5.1'))), old);
  assert.ok(tb.setRecord(1, 0, old[0].words).length > 0);
  assert.throws(() => tb.setRecord(1, 0, words(5).slice(0, 30)), /20 or 40/);
}

// --- against the firmware ------------------------------------------------------------
function compile() {
  const sources = ['src/tb/controls.c', 'src/tb/layout.c', 'src/tb/engine.c', 'src/tb/input.c', 'src/tb/scan.c',
    'src/tb/midi_in.c', 'src/tb/settings.c', 'src/tb/sysex.c', 'tests/webapp/harness.c'];
  const windows = process.platform === 'win32';
  const at = (p) => (windows ? '/mnt/' + p[0].toLowerCase() + p.slice(2).replaceAll('\\', '/') : p);
  const cwd = at(root);
  const exe = '/tmp/tb_webapp_harness';
  const cmd = `cd '${cwd}' && gcc -std=c11 -O1 -Wall -Werror ${sources.join(' ')} -o ${exe}`;
  const run = (shell) => spawnSync(windows ? 'wsl' : 'bash', windows ? ['-e', 'bash', '-lc', shell] : ['-lc', shell], { encoding: 'utf8' });
  const built = run(cmd);
  if (built.status !== 0) { console.log('SKIP firmware cross-check: gcc unavailable\n' + (built.stderr || '').slice(0, 400)); return null; }
  return (requests) => {
    const hex = requests.map((r) => Buffer.from(r).toString('hex')).join('\n') + '\n';
    const out = spawnSync(windows ? 'wsl' : 'bash', windows ? ['-e', exe] : ['-lc', exe], { input: hex, encoding: 'utf8', maxBuffer: 1 << 26 });
    assert.equal(out.status, 0, out.stderr);
    return out.stdout.split('\n').slice(0, requests.length).map((l) => Uint8Array.from(Buffer.from(l.trim(), 'hex')));
  };
}
const firmware = compile();
if (firmware) {
  const lydian = tb.maskOf([0, 2, 4, 6, 7, 9, 11]);
  const steps = [
    ['identify', tb.identify(), tb.CMD.IDENTIFY],
    ['set U2', tb.setScale(1, lydian, 'bright 4th'), tb.CMD.SET_SCALE],
    ['get U2', tb.getScale(1), tb.CMD.GET_SCALE],
    ['bad scale', tb.setScale(0, 0b1011, 'x'), tb.CMD.SET_SCALE],
    ['select U2', tb.selectScale(13), tb.CMD.SELECT_SCALE],
    ['select empty U3', tb.selectScale(14), tb.CMD.SELECT_SCALE],
    ['get live', tb.getRecord(tb.KIND.LIVE, 0), tb.CMD.GET_RECORD],
    ['preview', tb.preview(lydian, 2), tb.CMD.PREVIEW],
    ['preview slot', tb.preview(lydian, 2, 1), tb.CMD.PREVIEW],
    ['nor', tb.norRead(0xabcdef, 2048), tb.CMD.NOR_READ],
    ['nor tail', tb.norRead((1 << 24) - 100, 100), tb.CMD.NOR_READ],
    ['nor past end', tb.norRead((1 << 24) - 100, 101), tb.CMD.NOR_READ],
    ['empty preset', tb.getRecord(tb.KIND.PRESET, 3), tb.CMD.GET_RECORD],
    ['no favourites', tb.getFavourites(), tb.CMD.GET_RECORD],
    ['set favourites', tb.setFavourites([40, 13, 0]), tb.CMD.SET_RECORD],
    ['get favourites', tb.getFavourites(), tb.CMD.GET_RECORD],
    ['bad favourites', tb.setRecord(tb.KIND.FAVOURITES, 0, tb.favouritesToWords([40]).map((w, i) => (i === 0 ? 0xffff2828 : w))), tb.CMD.SET_RECORD],
    ['select library', tb.selectScale(40), tb.CMD.SELECT_SCALE],
    ['live after', tb.getRecord(tb.KIND.LIVE, 0), tb.CMD.GET_RECORD],
  ];
  const replies = firmware(steps.map((s) => s[1]));
  const r = Object.fromEntries(steps.map(([name, , cmd], i) => [name, replies[i].length ? tb.parseReply(replies[i], cmd) : null]));
  const info = tb.decodeIdentify(r.identify.data);
  assert.deepEqual([r.identify.status, info.protocol, info.stage, info.userScales, info.presets, info.payloadWords],
    [0, 1, 4, 12, 8, 40]);
  const header = fs.readFileSync(path.join(root, 'src/tb/version.h'), 'utf8');
  const part = (name) => header.match(new RegExp(`#define TB_VERSION_${name} ([0-9]+)`))[1];
  assert.equal(info.version, `${part('MAJOR')}.${part('MINOR')}.${part('PATCH')}`);
  assert.ok(info.norPresent && info.norBytes === 1 << 24 && info.jedec.join() === '239,64,24');
  assert.equal(r['set U2'].status, 0);
  assert.deepEqual(tb.decodeScale(r['get U2'].data), { slot: 1, mask: lydian, name: 'BRIGHT 4TH' });
  assert.equal(r['bad scale'].status, 3);
  assert.equal(r['select U2'].status, 0);
  assert.equal(r['select empty U3'].status, 3);
  const live = tb.decodeRecord(r['get live'].data);
  assert.equal(live.words.length, 40);
  assert.equal((live.words[0] >> 8) & 255, 13, 'live settings now select U2 (scale id 13)');
  assert.equal(r.preview, null, 'preview has no reply');
  assert.equal(r['preview slot'], null, 'preview with a slot has no reply either');
  const fake = (a) => (a * 7 + (a >> 8) * 13 + (a >> 16)) & 255;
  const chunk = tb.decodeNor(r.nor.data, 0xabcdef, 2048);
  assert.ok(chunk.every((b, i) => b === fake(0xabcdef + i)));
  assert.equal(tb.decodeNor(r['nor tail'].data, (1 << 24) - 100, 100).length, 100);
  assert.equal(r['nor past end'].status, 6);
  assert.equal(r['empty preset'].status, 7);
  assert.equal(r['no favourites'].status, 7);
  assert.equal(r['set favourites'].status, 0);
  assert.deepEqual(tb.favouritesFromWords(tb.decodeRecord(r['get favourites'].data).words), [40, 13, 0]);
  assert.equal(r['bad favourites'].status, 3, 'an id twice is refused');
  assert.equal(r['select library'].status, 0);
  assert.equal((tb.decodeRecord(r['live after'].data).words[0] >> 8) & 255, 40, 'a library scale (Locrian #2) is playing');
  // Restore into a fresh board (new harness process), in the editor's order: the
  // scale first, then a preset that uses it. Without the scale the preset is refused.
  const [early, scale, store, back] = firmware([tb.setRecord(tb.KIND.PRESET, 3, live.words),
    tb.setScale(1, lydian, 'bright 4th'), tb.setRecord(tb.KIND.PRESET, 3, live.words), tb.getRecord(tb.KIND.PRESET, 3)]);
  assert.equal(tb.parseReply(early, tb.CMD.SET_RECORD).status, 3);
  assert.equal(tb.parseReply(scale, tb.CMD.SET_SCALE).status, 0);
  assert.equal(tb.parseReply(store, tb.CMD.SET_RECORD).status, 0);
  assert.deepEqual(tb.decodeRecord(tb.parseReply(back, tb.CMD.GET_RECORD).data).words, live.words);
  // A corrupted reply is detected.
  const bad = Uint8Array.from(replies[0]); bad[6] ^= 1;
  assert.throws(() => tb.parseReply(bad, tb.CMD.IDENTIFY), /checksum/);
  const badNor = { ...r.nor, data: r.nor.data.slice() }; badNor.data[20] ^= 1;
  assert.throws(() => tb.decodeNor(badNor.data, 0xabcdef, 2048), /CRC/);
  console.log('PASS: web editor protocol: CRC/packing/names/scale masks, pad colours = firmware, settings backup read/restore order/failures/file checks, and against the firmware SysEx code: identify, set/get/validate/select user scales, live record, preview with and without a slot (no reply), version checks, NOR chunks with CRC and range error, absent preset, record restore round trip, favourites set/get/validate, a library scale selected, corrupted replies rejected');
} else {
  console.log('PASS (pure functions only): web editor protocol');
}
