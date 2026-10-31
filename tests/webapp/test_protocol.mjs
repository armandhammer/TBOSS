// SPDX-License-Identifier: GPL-3.0-or-later - the web editor's protocol module against known values
// and against the real firmware SysEx code (tests/webapp/harness.c, compiled with gcc;
// on Windows through WSL).   node tests/webapp/test_protocol.mjs
import assert from 'node:assert/strict';
import { spawnSync } from 'node:child_process';
import { fileURLToPath } from 'node:url';
import path from 'node:path';
import fs from 'node:fs';
import * as tb from '../../webapp/tb-protocol.js';
import * as chords from '../../webapp/tb-chords.js';
import * as lights from '../../webapp/tb-lights.js';
import * as pads from '../../webapp/tb-pads.js';
import * as feel from '../../webapp/tb-feel.js';
import * as lanes from '../../webapp/tb-lanes.js';
import * as presets from '../../webapp/tb-presets.js';
import * as midi from '../../webapp/tb-midi.js';

// 2.0.6: the board's two ports (src/tb/usb_descriptors.c): the editor talks on the editor's, input paired to its output.
{
  const access = (outs, ins) => ({ outputs: new Map(outs.map((n) => [n, { id: n, name: n }])), inputs: new Map(ins.map((n) => [n, { id: n, name: n }])) });
  for (const [outs, ins, out, inp] of [
    [['Synth', 'TheoryBoard MIDI', 'MIDIOUT2 (TheoryBoard MIDI)'], ['TheoryBoard MIDI', 'MIDIIN2 (TheoryBoard MIDI)'], 'MIDIOUT2 (TheoryBoard MIDI)', 'MIDIIN2 (TheoryBoard MIDI)'],
    [['TheoryBoard MIDI', 'TheoryBoard Editor'], ['TheoryBoard MIDI', 'TheoryBoard Editor'], 'TheoryBoard Editor', 'TheoryBoard Editor'],
    [['TheoryBoard MIDI'], ['TheoryBoard MIDI'], 'TheoryBoard MIDI', 'TheoryBoard MIDI'],
    [['Synth'], ['Synth'], 'Synth', 'Synth']]) { // none named TheoryBoard: chosen by hand, paired by name as before
    const a = access(outs, ins), first = midi.boardOutputs(a)[0], pair = midi.inputFor(a, first);
    assert.equal(first.name, out); assert.equal(pair ? pair.name : null, inp);
  }
  const a = access(['TheoryBoard MIDI', 'MIDIOUT2 (TheoryBoard MIDI)'], ['MIDIIN2 (TheoryBoard MIDI)', 'TheoryBoard MIDI']);
  assert.equal(midi.inputFor(a, a.outputs.get('TheoryBoard MIDI')).name, 'TheoryBoard MIDI'); // the board's port chosen by hand
}

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
assert.deepEqual(tb.neighbors(0xab5).map((n) => `${tb.INTERVALS[n.from]}>${tb.INTERVALS[n.to]} ${n.scale.short}`),
  ['3>b3 MEL MINOR', '4>#4 LYDIAN', '5>b6 IONIAN #5', '6>b6 HARM MAJOR', '7>b7 MIXOLYDIAN']);
// Favorites record: ids in order, 0xFF after the last.
const favWords = tb.favoritesToWords([40, 0, 13]);
assert.equal(favWords.length, tb.PAYLOAD_WORDS);
assert.equal(favWords[0], (40 | 0 << 8 | 13 << 16 | 0xff << 24) >>> 0);
assert.ok(favWords.slice(1, 32).every((w) => w === 0xffffffff) && favWords.slice(32).every((w) => w === 0));
assert.deepEqual(tb.favoritesFromWords(favWords), [40, 0, 13]);
assert.throws(() => tb.favoritesToWords([1, 1]), /different/);
assert.throws(() => tb.favoritesToWords(Array.from({ length: 129 }, (_, i) => i % 127)), /128/);
assert.throws(() => tb.request(1, [128]));
{ // the editor's key colors are the board's pad colors (src/tb/ui_lights.c)
  const ui = fs.readFileSync(path.join(root, 'src/tb/ui_lights.c'), 'utf8');
  const table = ui.match(/hues\[12\] = \{([^;]*)\};/)[1];
  const fw = [...table.matchAll(/\{(\d+),(\d+),(\d+)\}/g)].map((m) => [+m[1], +m[2], +m[3]]);
  assert.equal(fw.length, 12);
  assert.deepEqual(tb.PAD_HUES.map((c) => [...c]), fw);
}
assert.deepEqual(Array.from(tb.preview(0xad5, 14)).slice(4, -2), [4, 0x55, 0x15, 2]);      // 3 data bytes
assert.deepEqual(Array.from(tb.preview(0xad5, 2, 1)).slice(4, -2), [4, 0x55, 0x15, 2, 1]);  // + slot
assert.ok(tb.versionAtLeast('0.5.0', 0, 5) && tb.versionAtLeast('1.0.2', 0, 5) && tb.versionAtLeast('0.12.3', 0, 5));
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
  // Firmware 0.7.0 also has favorites (kind 4), backed up after the scales.
  stored.set('4/0', tb.favoritesToWords([40, 9]));
  sent.length = 0; const withFav = await tb.readRecords(fake, 12, true);
  assert.equal(sent.length, 12 + 1 + 8 + 1, 'favorites asked once');
  assert.deepEqual(withFav.map((r) => r.kind), [3, 4, 2, 2, 1]);
  assert.equal(tb.recordLabel(withFav[1]), 'favorite scales');
  assert.deepEqual(tb.parseBackup(JSON.stringify(tb.makeBackup(withFav, '0.7.0'))), withFav);
  sent.length = 0; await tb.writeRecords(fake, [withFav[4], withFav[1], withFav[0]]);
  assert.deepEqual(sent.map((m) => m[1]), [3, 4, 1], 'favorites restored after the scales');
  // Firmware 0.11.0 also has progressions (kind 5, 8 slots), restored before the live settings.
  const song = chords.progressionFromText('Am D Dm Am', 9, 1).prog;
  stored.set('5/2', tb.progressionToWords(song));
  sent.length = 0; const withProg = await tb.readRecords(fake, 12, true, true);
  assert.equal(sent.length, 12 + 1 + 8 + 8 + 1, 'eight progression slots asked');
  assert.deepEqual(withProg.map((r) => [r.kind, r.slot]), [[3, 1], [4, 0], [2, 0], [2, 7], [5, 2], [1, 0]]);
  assert.equal(tb.recordLabel(withProg[4]), 'progression P3');
  assert.deepEqual(tb.parseBackup(JSON.stringify(tb.makeBackup(withProg, '0.11.0'))), withProg);
  sent.length = 0; await tb.writeRecords(fake, [withProg[5], withProg[4], withProg[0]]);
  assert.deepEqual(sent.map((m) => m[1]), [3, 5, 1], 'progressions after the scales they borrow from, before the live settings');
  assert.throws(() => tb.parseBackup(JSON.stringify({ ...backup, records: [{ kind: 14, slot: 0, words: words(1) }] })), /damaged/); // 2.0: kinds up to 13
  // Chord sets (1.0.0): asked for when the firmware has them, restored after the scales and
  // before the settings and presets that may select them.
  const pop = { name: 'pop', columns: [{ root: 0, set: 0x91 }, { root: 5, set: 0x891 }, null, { root: 19, set: 0x491 }, ...new Array(8).fill(null)] };
  stored.set('6/1', tb.chordSetToWords(pop));
  sent.length = 0; const withSets = await tb.readRecords(fake, 12, true, true, true);
  assert.equal(sent.length, 12 + 1 + 8 + 8 + 8 + 1, 'eight chord set slots asked');
  assert.deepEqual(withSets.map((r) => [r.kind, r.slot]), [[3, 1], [4, 0], [2, 0], [2, 7], [5, 2], [6, 1], [1, 0]]);
  assert.equal(tb.recordLabel(withSets[5]), 'chord set 2');
  sent.length = 0; await tb.writeRecords(fake, [withSets[6], withSets[5], withSets[2], withSets[0]]);
  assert.deepEqual(sent.map((m) => m[1]), [3, 6, 2, 1], 'chord sets after the scales, before presets and the live settings');
  // The step pattern (1.5.0): 16 records (4 lanes x 4 chunks of 16 steps), asked for when the firmware has
  // them, in backups, restored after the progressions and before the live settings, left out for older firmware.
  stored.set('7/0', tb.laneChunkToWords({ kind: 2, length: 8, steps: [{ on: 1, value: 36, velocity: 100 }] }, 0));
  stored.set('7/9', tb.laneChunkToWords({ steps: [null, { on: 1, value: 7, edit: 0x22, scale: 3 }] }, 1));
  sent.length = 0; const withLanes = await tb.readRecords(fake, 12, true, true, true, true);
  assert.equal(sent.length, 12 + 1 + 8 + 8 + 8 + 16 + 1, 'sixteen pattern records asked');
  assert.deepEqual(withLanes.map((r) => [r.kind, r.slot]), [[3, 1], [4, 0], [2, 0], [2, 7], [5, 2], [6, 1], [7, 0], [7, 9], [1, 0]]);
  assert.equal(tb.recordLabel(withLanes[6]), 'pattern lane L1, steps 1-16');
  assert.equal(tb.recordLabel(withLanes[7]), 'pattern lane L3, steps 17-32');
  assert.deepEqual(tb.parseBackup(JSON.stringify(tb.makeBackup(withLanes, '1.5.0'))), withLanes);
  sent.length = 0; await tb.writeRecords(fake, [withLanes[8], withLanes[7], withLanes[4], withLanes[0]], '1.5.0');
  assert.deepEqual(sent.map((m) => [m[1], m[2]]), [[3, 1], [5, 2], [7, 9], [1, 0]], 'the pattern after the progressions, before the live settings');
  assert.deepEqual(tb.recordsFor(withLanes, '2.0.3'), { records: withLanes, skipped: [] }); // (before 2.0.3 some fields are cleared)
  // Unstick off (1.7.0, word 19 bit 8): kept for 1.7, left out for 1.6 (which refuses the bit); Mono stays.
  const unstick = { kind: tb.KIND.LIVE, slot: 0, words: words(0).map((w, i) => (i === 19 ? 0x101 : w)) };
  assert.equal(tb.recordsFor([unstick], '1.7.0').records[0].words[19], 0x101);
  assert.equal(tb.recordsFor([unstick], '1.6.0').records[0].words[19], 1);
  assert.equal(tb.recordsFor([{ ...unstick, kind: tb.KIND.PRESET, slot: 1 }], '1.6.0').records[0].words[19], 1);
  assert.equal(unstick.words[19], 0x101, 'the backup itself is untouched');
  // Step pads Silent (1.9.4, bit 10) and Spread of Bass (2.0.2, bit 11): each kept from its version, cleared before.
  const bass = { kind: tb.KIND.LIVE, slot: 0, words: words(0).map((w, i) => (i === 19 ? 0xD01 : w)) };
  assert.equal(tb.recordsFor([bass], '2.0.2').records[0].words[19], 0xD01);
  assert.equal(tb.recordsFor([bass], '2.0.1').records[0].words[19], 0x501);
  assert.equal(tb.recordsFor([bass], '1.9.4').records[0].words[19], 0x501);
  assert.equal(tb.recordsFor([bass], '1.9.3').records[0].words[19], 0x101);
  // Fill and Stutter buttons and the pedal's Fill (2.0.3): kept for 2.0.3, back to the printed function and Off before.
  const fill = { kind: tb.KIND.LIVE, slot: 0, words: words(0).map((w, i) => (i === 35 ? (41 | 44 << 8 | 40 << 16 | 3 << 24) >>> 0 : i === 25 ? (8 << 24 | 9) >>> 0 : w)) };
  assert.deepEqual([tb.recordsFor([fill], '2.0.3').records[0].words[35], tb.recordsFor([fill], '2.0.3').records[0].words[25]], [fill.words[35], fill.words[25]]);
  assert.deepEqual([tb.recordsFor([fill], '2.0.2').records[0].words[35], tb.recordsFor([fill], '2.0.2').records[0].words[25]], [(40 << 16 | 3 << 24) >>> 0, 9]);
  assert.equal(tb.actionsFor('2.0.3'), tb.ACTIONS.length); assert.equal(tb.actionsFor('2.0.2'), tb.ACTIONS.indexOf('Fill (while held)'));
  // MPE Both with Glide (1.8.0, word 28 byte 3 = 7): kept for 1.8, cleared for 1.7 (which refuses the byte).
  const mpe = { kind: tb.KIND.PRESET, slot: 2, words: words(0).map((w, i) => (i === 28 ? 0x07010203 : w)) };
  assert.equal(tb.recordsFor([mpe], '1.8.0').records[0].words[28], 0x07010203);
  assert.equal(tb.recordsFor([mpe], '1.7.10').records[0].words[28], 0x010203);
  assert.equal(tb.recordsFor([{ ...mpe, kind: tb.KIND.LIVE, slot: 0 }], '1.5.0').records[0].words[28], 0x010203);
  assert.equal(mpe.words[28], 0x07010203, 'the backup itself is untouched');
  // Vel full +3500 (1.8.3, word 32 byte 3 = 26), Chord vel Firmest (word 1 byte 1 = 5), Melody vel Firmer (word
  // 31 byte 3 = 0x84): kept for 1.8; for 1.7 Vel full cleared and both curves back to Firm.
  const feel = { kind: tb.KIND.LIVE, slot: 0, words: words(0).map((w, i) => (i === 32 ? 0x1a230d0d : i === 1 ? 0x03010502 : i === 31 ? 0x84000101 : w)) };
  const kept18 = tb.recordsFor([feel], '1.8.3').records[0].words;
  assert.deepEqual([kept18[32], kept18[1], kept18[31]], [0x1a230d0d, 0x03010502, 0x84000101]);
  const into17 = tb.recordsFor([feel], '1.7.10').records[0].words;
  assert.deepEqual([into17[32], into17[1], into17[31]], [0x230d0d, 0x03010302, 0x83000101]);
  // Full +4000 (1.8.6, word 32 byte 2 = 50): kept for 1.8, back to +3000 (40) for 1.7, which refuses the word.
  const hard = { ...feel, words: feel.words.map((w, i) => (i === 32 ? 0x00320d0d : w)) };
  assert.equal(tb.recordsFor([hard], '1.8.6').records[0].words[32], 0x00320d0d);
  assert.equal(tb.recordsFor([hard], '1.7.10').records[0].words[32], 0x00280d0d);
  const firm = { ...feel, words: feel.words.map((w, i) => (i === 1 ? 0x03010302 : i === 31 ? 0x82000101 : w)) };
  const firm17 = tb.recordsFor([firm], '1.7.10').records[0].words;
  assert.deepEqual([firm17[1], firm17[31]], [0x03010302, 0x82000101], 'curves 1.7 has are kept');
  // Spread of Chord, Chord prog 5, Melody prog 128 (word 19), Program in All (33 byte 3) and the Bass layout
  // (24 byte 2 = 5): kept for 1.7; for 1.6 the word 19 extras and Program in cleared, Bass back to Scale.
  const promised = { kind: tb.KIND.PRESET, slot: 3, words: words(0).map((w, i) => (i === 19 ? 0x80050201 : i === 33 ? 0x11000103 : i === 24 ? 0x01050101 : w)) };
  const kept17 = tb.recordsFor([promised], '1.7.0').records[0].words;
  assert.deepEqual([kept17[19], kept17[33], kept17[24]], [0x80050201, 0x11000103, 0x01050101]);
  const into16 = tb.recordsFor([promised], '1.6.0').records[0].words;
  assert.deepEqual([into16[19], into16[33], into16[24]], [1, 0x000103, 0x01000101]);
  const fourths = { ...promised, words: promised.words.map((w, i) => (i === 24 ? 0x01020101 : w)) };
  assert.equal(tb.recordsFor([fourths], '1.6.0').records[0].words[24], 0x01020101, 'other layouts kept');
  const into15 = tb.recordsFor(withLanes, '1.5.0'); // every record, settings and presets without the 1.6 words 18, 19
  assert.deepEqual(into15.records.map((r) => [r.kind, r.slot]), withLanes.map((r) => [r.kind, r.slot]));
  assert.deepEqual(into15.records.map((r) => (r.kind === tb.KIND.LIVE || r.kind === tb.KIND.PRESET ? r.words.slice(18, 20) : null)),
    withLanes.map((r) => (r.kind === tb.KIND.LIVE || r.kind === tb.KIND.PRESET ? [0, 0] : null)));
  assert.equal(into15.skipped.length, 0);
  const into14 = tb.recordsFor(withLanes, '1.4.0');
  assert.deepEqual(into14.records.map((r) => r.kind), [3, 4, 2, 2, 5, 6, 1]);
  assert.deepEqual(into14.skipped.map(tb.recordLabel), ['pattern lane L1, steps 1-16', 'pattern lane L3, steps 17-32']);
  // Chords on Right (1.5.0, word 26 byte 3): kept for 1.5, cleared for 1.4 (which refuses the byte).
  const right = { kind: tb.KIND.LIVE, slot: 0, words: words(0).map((w, i) => (i === 26 ? 0x01000102 : w)) };
  assert.equal(tb.recordsFor([right], '1.5.0').records[0].words[26], 0x01000102);
  assert.equal(tb.recordsFor([right], '1.4.0').records[0].words[26], 0x0102);
  assert.equal(tb.recordsFor([{ ...right, kind: tb.KIND.PRESET, slot: 3 }], '1.4.0').records[0].words[26], 0x0102);
  assert.equal(right.words[26], 0x01000102, 'the backup itself is untouched');
  // Colors Rainbow (1.5.0, word 4 bit 30): kept for 1.5, back to Default for 1.4 (which refuses the bit).
  const rainbow = { kind: tb.KIND.LIVE, slot: 0, words: words(0).map((w, i) => (i === 4 ? 0xc5000a01 : w)) };
  assert.equal(tb.recordsFor([rainbow], '1.5.0').records[0].words[4], 0xc5000a01);
  assert.equal(tb.recordsFor([rainbow], '1.4.0').records[0].words[4], 0x85000a01);
  assert.equal(tb.recordsFor([{ ...rainbow, kind: tb.KIND.PRESET, slot: 2 }], '1.3.0').records[0].words[4], 0x85000a01);
  const rootFifth = { ...rainbow, words: words(0).map((w, i) => (i === 4 ? 0xa0000000 : w)) };
  assert.equal(tb.recordsFor([rootFifth], '1.4.0').records[0].words[4], 0xa0000000, 'Root+5th kept');
  // The 1.6.0 effects (words 18 and 19): kept for 1.6, off for 1.5 and earlier (which refuse the words).
  const effects = { kind: tb.KIND.PRESET, slot: 1, words: words(0).map((w, i) => (i === 18 ? 0x05020306 : i === 19 ? 1 : w)) };
  assert.deepEqual(tb.recordsFor([effects], '1.6.0').records[0].words.slice(18, 20), [0x05020306, 1]);
  for (const older of ['1.5.0', '1.4.0', '1.0.0']) {
    assert.deepEqual(tb.recordsFor([effects], older).records[0].words.slice(18, 20), [0, 0], older);
  }
  assert.equal(effects.words[18], 0x05020306, 'the backup itself is untouched');
  sent.length = 0; const result14 = await tb.writeRecords(fake, withLanes, '1.4.0');
  assert.ok(!sent.some((m) => m[1] === 7) && result14.skipped.length === 2, 'no pattern records sent to 1.4.0');
  // The record layout: a step's on, value, velocity, edits and scale in its low word; the lane's settings in chunk 0 only.
  const lw = tb.laneChunkToWords({ kind: 1, channel: 3, length: 12, rate: 2, mute: 1, steps: [{ on: 1, value: 3, velocity: 90, edit: 0x2a, scale: 5 }] }, 0);
  assert.deepEqual([lw[0], lw[1], lw[2], lw[33], lw[34]], [1, (1 | 3 << 1 | 90 << 8 | 0x2a << 15 | 5 << 23) >>> 0, 0, 0x020c0301, 1 << 16]);
  assert.deepEqual(tb.laneChunkToWords({ kind: 1, length: 12 }, 2).slice(1), new Array(39).fill(0), 'chunk 2: steps only');
  // Into older firmware (going back to an earlier TBOSS): what it doesn't have is left at its
  // default or left out, so it doesn't refuse whole records. (Words 18 and 19 are 0, as before 1.6.0.)
  const newer = { kind: tb.KIND.LIVE, slot: 0, words: words(0).map((w, i) => (i === 18 || i === 19 ? 0 : i === 28 ? 0x00010203 : i === 31 ? 0x83000101 : i === 32 ? 0x00230612 : i === 33 ? 0x0103 : w)) };
  const preset = { ...newer, kind: tb.KIND.PRESET, slot: 2 };
  const all = [newer, preset, withProg[4], withProg[0]];
  assert.equal(tb.recordsFor(all, null).records, all, 'unknown version: as saved');
  assert.deepEqual(tb.recordsFor(all, '1.0.0'), { records: all, skipped: [] });
  const zero12 = (r) => (r.kind === tb.KIND.LIVE || r.kind === tb.KIND.PRESET ? { ...r, words: r.words.map((w, i) => (i >= 34 && i < 39 ? 0 : w)) } : r);
  assert.deepEqual(tb.recordsFor(all, '0.11.0'), { records: all.map(zero12), skipped: [] }, '0.11.0: words 34..38 back to 0');
  const into10 = tb.recordsFor(all, '0.10.0');
  assert.deepEqual(into10.skipped.map(tb.recordLabel), ['progression P3']);
  assert.deepEqual(into10.records.map((r) => [r.kind, r.words[28], r.words[31], r.words[32], r.words[33]]),
    [[1, 0x0203, 0x000101, 0x00230612, 0], [2, 0x0203, 0x000101, 0x00230612, 0], [3, 1028, 1031, 1032, 1033]]);
  assert.equal(newer.words[33], 0x0103, 'the backup itself is untouched');
  assert.deepEqual(tb.recordsFor(all, '0.9.1').records.map((r) => r.words[32]), [0, 0, 1032]);
  { // 1.0.0's settings, chord sets and a progression's chord set, into 0.11.0
    const live12 = { kind: tb.KIND.LIVE, slot: 0, words: newer.words.map((w, i) => (i === 34 ? 0x00050102 : i === 35 ? 15 : i === 38 ? 0x2700 : w)) };
    const prog12 = { ...withProg[4], words: withProg[4].words.map((w, i) => (i === 39 ? 2 : w)) };
    const all12 = [live12, { ...live12, kind: tb.KIND.PRESET, slot: 1 }, prog12, withSets[5]];
    assert.deepEqual(tb.recordsFor(all12, '1.0.0'), { records: all12, skipped: [] });
    const into11 = tb.recordsFor(all12, '0.11.0');
    assert.deepEqual(into11.skipped.map(tb.recordLabel), ['chord set 2']);
    assert.deepEqual(into11.records.map((r) => r.words.slice(33, 40)), [[0x0103, 0, 0, 0, 0, 0, newer.words[39]], [0x0103, 0, 0, 0, 0, 0, newer.words[39]],
      [...withProg[4].words.slice(33, 39), 0]]);
    assert.equal(live12.words[34], 0x00050102, 'the backup itself is untouched');
  }
  { // 1.3.0's Full layout and chord grid (chords values 9, 10) in settings and a progression, into 1.0.0: the scale's chords
    assert.equal(tb.FULL_CHORDS, 9);
    const liveFull = { kind: tb.KIND.LIVE, slot: 0, words: tb.withChordSet(newer.words, tb.FULL_CHORDS) };
    const progFull = { ...withProg[4], words: withProg[4].words.map((w, i) => (i === 39 ? tb.FULL_CHORDS : w)) };
    assert.equal(tb.progressionFromWords(progFull.words).chords, tb.FULL_CHORDS);
    assert.deepEqual(tb.progressionToWords(tb.progressionFromWords(progFull.words)), progFull.words);
    assert.equal(tb.GRID_CHORDS, 10); assert.equal(chords.GRID_CHORDS, tb.GRID_CHORDS);
    // Its steps named as the grid plays them: C major column 0 bottom up C, Cmaj7, Csus4, Cmaj9; F's sus row Fsus2.
    const major = [0, 2, 4, 5, 7, 9, 11], none = { lower: 0, sus: 0, add: 0 };
    const gridName = (pad) => chords.chordName(chords.padChord(major, 0, pad, chords.gridEdit(major, pad, none)));
    assert.deepEqual([3, 2, 1, 0].map(gridName), ['C', 'Cmaj7', 'Csus4', 'Cmaj9']);
    assert.deepEqual([gridName(13), gridName(18)], ['Fsus2', 'G7']);
    assert.equal(chords.padChord(major, 0, 25, chords.gridEdit(major, 25, none)).set, 1 | 1 << 5 | 1 << 6, 'B E F: B keeps its 4th');
    const gridProg = { scale: 0, unit: 0, chords: tb.GRID_CHORDS, steps: [18, 1].map((pad) => ({ pad, units: 1, scale: chords.NO_SCALE, velocity: 100, edit: none })) };
    assert.equal(chords.progressionToText(gridProg, 0, () => major), 'G7 Csus4');
    assert.equal(chords.progressionToText({ ...gridProg, chords: 0 }, 0, () => major), 'G C', 'the scale\'s chords: the same chord on every row');
    const progGrid = { ...withProg[4], words: withProg[4].words.map((w, i) => (i === 39 ? tb.GRID_CHORDS : w)) };
    assert.equal(tb.progressionFromWords(progGrid.words).chords, tb.GRID_CHORDS);
    assert.equal(tb.progressionFromWords(progFull.words.map((w, i) => (i === 39 ? 11 : w))), null, 'beyond the grid: refused');
    const liveGrid = { kind: tb.KIND.LIVE, slot: 0, words: tb.withChordSet(newer.words, tb.GRID_CHORDS) };
    assert.deepEqual(tb.recordsFor([liveGrid, progGrid], '1.3.0'), { records: [liveGrid, progGrid], skipped: [] });
    const gridInto10 = tb.recordsFor([liveGrid, progGrid], '1.0.0').records;
    assert.equal(gridInto10[0].words[34] & 0xff, 0); assert.equal(gridInto10[1].words[39], 0);
    assert.deepEqual(tb.recordsFor([liveFull, progFull], '1.3.0'), { records: [liveFull, progFull], skipped: [] });
    const into10 = tb.recordsFor([liveFull, progFull], '1.0.0').records;
    assert.equal(into10[0].words[34] & 0xff, 0); assert.equal(into10[1].words[39], 0);
    assert.deepEqual(into10[0].words.slice(0, 34), liveFull.words.slice(0, 34), 'the rest stays');
  }
  { // 1.4.0: arp rate 5 per beat (8), rhythm E3/8, rotation 2, note repeat By row (8) with a Euclidean
    // rhythm, pivot to G, a button on Pivot; into 1.3.0 they go back to 1/16, the first rhythm, off,
    // nothing and the button's printed function. 1.4.0's own values stay for 1.4.0.
    const pivot = tb.ACTIONS.indexOf('Pivot (hold + root button)');
    assert.equal(pivot, tb.ACTIONS.indexOf('Fill (while held)') - 1); assert.equal(tb.ACTIONS[pivot - 1], 'Nothing'); // 2.0.3's after it
    assert.equal(tb.actionsFor('1.4.0'), pivot + 1); assert.equal(tb.actionsFor('1.3.0'), pivot);
    assert.equal(tb.actionsFor('2.0.3'), tb.ACTIONS.length); assert.equal(tb.actionsFor(null), pivot);
    const set = (w, b, v) => ((w & ~(0xff << (8 * b))) | (v << (8 * b))) >>> 0;
    const w14 = newer.words.map((w, i) => (i === 2 ? set(w, 3, 8) : i === 23 ? set(w, 2, 30) : i === 27 ? (w | 2 << 24 | 13 << 16) >>> 0
      : i === 31 ? set(w, 2, 8) : i === 33 ? set(w, 2, 8) : i === 36 ? (pivot + 1) << 8 | 3 : w));
    const live14 = { kind: tb.KIND.LIVE, slot: 0, words: w14 }, preset14 = { ...live14, kind: tb.KIND.PRESET, slot: 4 };
    assert.deepEqual(tb.recordsFor([live14, preset14], '1.4.0'), { records: [live14, preset14], skipped: [] });
    assert.deepEqual(tb.buttonsFromWords(w14)[5], pivot);
    for (const r of tb.recordsFor([live14, preset14], '1.3.0').records) {
      const w = r.words, byte = (i, b) => (w[i] >>> (8 * b)) & 0xff;
      assert.deepEqual([byte(2, 3), byte(23, 2), byte(31, 2), w[27] >>> 16, byte(33, 2)], [3, 0, 0, 0, 0]);
      assert.equal(tb.buttonsFromWords(w)[5], 5, 'Pivot: the printed function'); assert.equal(byte(36, 0), 3, 'the other buttons stay');
      assert.deepEqual(w.filter((_, i) => ![2, 23, 27, 31, 33, 36].includes(i)), w14.filter((_, i) => ![2, 23, 27, 31, 33, 36].includes(i)));
    }
    // The values 1.3.0 has stay as they are.
    const w13 = w14.map((w, i) => (i === 2 ? set(w, 3, 5) : i === 23 ? set(w, 2, 7) : i === 31 ? set(w, 2, 4) : w));
    const back13 = tb.recordsFor([{ ...live14, words: w13 }], '1.3.0').records[0].words;
    assert.deepEqual([(back13[2] >>> 24) & 0xff, (back13[23] >>> 16) & 0xff, (back13[31] >>> 16) & 0xff], [5, 7, 4]);
    assert.equal(live14.words[27] >>> 16, 0x020d, 'the backup itself is untouched');
  }
  // The center buttons in the live settings: a byte per button, 0 = its printed function.
  {
    const actions = tb.BUTTONS.map((_, i) => i); actions[0] = tb.ACTIONS.indexOf('Shift'); actions[13] = tb.ACTIONS.indexOf('Nothing');
    const lw = tb.withButtons(words(3), actions);
    assert.deepEqual(lw.slice(35, 39), [15, 0, 0, 39 << 8]);
    assert.deepEqual(tb.buttonsFromWords(lw), actions);
    assert.deepEqual(lw.slice(0, 35), words(3).slice(0, 35), 'the other settings stay');
    assert.deepEqual(tb.buttonsFromWords(words(0).map(() => 0)), tb.BUTTONS.map((_, i) => i));
    assert.throws(() => tb.withButtons(words(3), actions.slice(1)), /button map/);
    assert.throws(() => tb.withButtons(words(3), actions.map(() => tb.ACTIONS.length)), /button map/);
    assert.equal(tb.withChordSet(lw, 3)[34] & 255, 3);
    // Names and count as the firmware (src/tb/controls.h TC_ACT_ ... TC_ACTIONS).
    const header = fs.readFileSync(path.join(root, 'src/tb/controls.h'), 'utf8');
    const acts = header.match(/enum \{ TC_ACT_SHIFT = TC_FUNCTIONS,([^}]*)\}/)[1].replace(/\/\*[\s\S]*?\*\//g, '').split(',').map((x) => x.trim()).filter(Boolean);
    assert.equal(tb.ACTIONS.length, 14 + 1 + acts.length - 1, 'one name per action');
  }
  // Chord set records: as the firmware lays them out, and its checks.
  {
    const cw = tb.chordSetToWords(pop);
    assert.deepEqual(cw.slice(0, 5), [1, 0x91, 0x891 | 5 << 12, 0, 0x491 | 19 << 12]);
    assert.deepEqual(tb.chordSetFromWords(cw), { ...pop, name: 'POP' });
    for (const [why, change] of [['format', (x) => { x[0] = 2; }], ['no root', (x) => { x[1] = 0x90; }], ['seven notes', (x) => { x[1] = 0x7f; }],
      ['root 24', (x) => { x[1] = 0x91 | 24 << 12; }], ['past the name', (x) => { x[16] = 1; }]]) {
      const bad = cw.slice(); change(bad);
      assert.equal(tb.chordSetFromWords(bad), null, why);
    }
    assert.throws(() => tb.chordSetToWords({ name: '', columns: [{ root: 0, set: 0x90 }, ...new Array(11).fill(null)] }), /chord set/);
  }
  sent.length = 0; const fitted = await tb.writeRecords(fake, all, '0.10.0');
  assert.deepEqual([fitted.done, fitted.total, fitted.skipped], [3, 3, ['progression P3']]);
  assert.deepEqual(sent.map((m) => m[1]), [3, 2, 1]);
  const old = [{ kind: 1, slot: 0, words: words(5).slice(0, tb.PAYLOAD_WORDS_V2) }];
  assert.deepEqual(tb.parseBackup(JSON.stringify(tb.makeBackup(old, '0.5.1'))), old);
  assert.ok(tb.setRecord(1, 0, old[0].words).length > 0);
  assert.throws(() => tb.setRecord(1, 0, words(5).slice(0, 30)), /20 or 40/);
}

// --- progressions: chord names, placement on the pads, the record format -------------------
{
  const scaleOf = (id) => (tb.SCALES.find((s) => s.id === id) || {}).steps || null;
  const MINOR = 1, MAJOR = 0, DORIAN = 2, HARM_MINOR = 7, HARM_MAJOR = 26, MIXOLYDIAN = 5;
  // Parsing and naming.
  for (const [text, root, semis, bass] of [['Am', 9, [0, 3, 7], null], ['E7', 4, [0, 4, 7, 10], null],
    ['F#m7b5', 6, [0, 3, 6, 10], null], ['Bbmaj7', 10, [0, 4, 7, 11], null], ['C/G', 0, [0, 4, 7], 7], ['C6/9', 0, [0, 2, 4, 7, 9], null],
    ['Ebsus4', 3, [0, 5, 7], null], ['G°7', 7, [0, 3, 6, 9], null], ['am', 9, [0, 3, 7], null], ['D/D', 2, [0, 4, 7], null]]) {
    const c = chords.parseChord(text);
    assert.deepEqual([c.root, c.set, c.bass], [root, semis.reduce((m, s) => m | (1 << s), 0), bass], text);
  }
  for (const bad of ['H', 'C5', 'Xm', 'Cm7b9b13', '']) assert.throws(() => chords.parseChord(bad), /chord name/, bad);
  assert.equal(chords.chordName(chords.parseChord('C/E')), 'C/E');
  assert.equal(chords.chordName(chords.parseChord('Bbm7'), chords.speller(0, scaleOf(MAJOR))), 'Bbm7', 'a flat outside C major');
  assert.equal(chords.chordName(chords.parseChord('F#'), chords.speller(0, scaleOf(MAJOR))), 'F#', 'the raised 4th stays sharp');
  assert.equal(chords.chordName(chords.parseChord('A#'), chords.speller(5, scaleOf(MAJOR))), 'Bb', 'F major writes Bb');
  // The user's song in A minor: D borrowed from A Dorian (pad 15 = column 3), E7 from A harmonic
  // minor (pad 19 with the 7th added): the same pads and scales the firmware's sequence test records.
  const song = chords.progressionFromText('Am D Dm Am C G E7 Am', 9, MINOR, 0);
  assert.deepEqual(song.errors, []);
  assert.deepEqual(song.prog.steps.map((s) => [s.pad, s.scale, s.edit.add, s.units]),
    [[3, 127, 0, 1], [15, DORIAN, 0, 1], [15, 127, 0, 1], [3, 127, 0, 1], [11, 127, 0, 1], [27, 127, 0, 1], [19, HARM_MINOR, 2, 1], [3, 127, 0, 1]]);
  assert.deepEqual([song.prog.unit, song.prog.scale], [0, MINOR]);
  assert.equal(chords.progressionToText(song.prog, 9, scaleOf), 'Am D Dm Am C G E7 Am');
  assert.equal(chords.progressionToText(song.prog, 2, scaleOf), 'Dm G Gm Dm F C A7 Dm', 'the same steps name other chords in D');
  // Borrowing from the most familiar close scale; extensions, sus and slash chords as edits.
  const pick = (text, tonic, home) => chords.progressionFromText(text, tonic, home).prog.steps.map((s) => [s.pad, s.scale, s.edit]);
  assert.deepEqual(pick('Fm Bb Bdim7', 0, MAJOR), [[15, HARM_MAJOR, { lower: 0, sus: 0, add: 0 }],
    [27, MIXOLYDIAN, { lower: 0, sus: 0, add: 0 }], [27, HARM_MAJOR, { lower: 0, sus: 0, add: 2 }]]);
  assert.deepEqual(pick('Cmaj9 Gsus4 G7sus4 Dm6 C/E C/G G13', 0, MAJOR), [[3, 127, { lower: 0, sus: 0, add: 6 }],
    [19, 127, { lower: 0, sus: 4, add: 0 }], [19, 127, { lower: 0, sus: 4, add: 2 }], [7, 127, { lower: 0, sus: 0, add: 1 }],
    [3, 127, { lower: 2, sus: 0, add: 0 }], [3, 127, { lower: 4, sus: 0, add: 0 }], [19, 127, { lower: 0, sus: 0, add: 7 }]]);
  // Lengths: Am*3 Am is one step of 4; more than 32 units of one chord is two steps (as on the board).
  const long = chords.progressionFromText('Am*3 Am | F*40, G', 9, MINOR).prog;
  assert.deepEqual(long.steps.map((s) => s.units), [4, 32, 8, 1]);
  assert.equal(chords.progressionToText(long, 9, scaleOf), 'Am*4 F*40 G');
  // What cannot go in a slot.
  assert.match(chords.progressionFromText('C F#7b9 Xm', 0, MAJOR).errors[0], /chord name/);
  assert.match(chords.progressionFromText('C/F#', 0, MAJOR).errors[0], /cannot be played/);
  assert.match(chords.progressionFromText('C*0', 0, MAJOR).errors[0], /at least one unit/);
  assert.match(chords.progressionFromText('C*65', 0, MAJOR).errors.join(), /65 units/);
  assert.match(chords.progressionFromText(Array(39).fill('C G').join(' '), 0, MAJOR).errors.join(), /78 chord changes/);
  assert.equal(chords.progressionFromText('C G '.repeat(19), 0, MAJOR).prog.steps.length, 38);
  // Every chord the pads can play in every built-in scale is found again from its name.
  for (const s of tb.SCALES) for (let column = 0; column < s.steps.length; column++) for (const sus of [0, 2, 4]) for (let add = 0; add < 8; add++) {
    const edit = { lower: 0, sus, add }, chord = chords.padChord(s.steps, 0, 4 * column + 3, edit);
    let name; try { name = chords.chordName(chord); chords.parseChord(name); } catch { continue; } // unnamed shapes
    if (name.includes('(')) continue;
    const at = chords.placeChord(chords.parseChord(name), 0, s.id);
    assert.ok(at, `${name} from ${s.name}`);
    assert.equal(chords.chordName(chords.padChord(scaleOf(at.scale === 127 ? s.id : at.scale), 0, at.pad, at.edit)), name);
  }
  // The record: words as the firmware lays them out, and its checks.
  const w = tb.progressionToWords(song.prog);
  assert.equal(w.length, 40);
  assert.equal(w[0], (1 | 0 << 8 | MINOR << 16 | 8 << 24) >>> 0);
  assert.equal(w[7], (19 | 0 << 6 | (100 >> 1) << 11 | (2 << 5) << 17 | HARM_MINOR << 25) >>> 0);
  const back = tb.progressionFromWords(w);
  assert.deepEqual(back.steps.map((s) => s.velocity), new Array(8).fill(100), 'a typed 100 comes back 100');
  assert.deepEqual(tb.progressionToWords(back), w);
  for (const [why, change] of [['magic', (x) => { x[0] ^= 1; }], ['unit', (x) => { x[0] = (x[0] & ~0xff00) | 4 << 8; }],
    ['scale', (x) => { x[0] = (x[0] & ~0xff0000) | 98 << 16; }], ['pad', (x) => { x[1] = (x[1] & ~63) | 48; }],
    ['sus', (x) => { x[1] |= 3 << 20; }], ['step scale', (x) => { x[1] = (x[1] & 0x1ffffff | 98 << 25) >>> 0; }],
    ['after the last', (x) => { x[38] = 1; }], ['chords 11 (after the grid)', (x) => { x[39] = 11; }], ['units', (x) => { x[1] |= 31 << 6; x[2] |= 31 << 6; x[3] |= 31 << 6; }]]) {
    const bad = w.slice(); change(bad);
    assert.equal(tb.progressionFromWords(bad), null, why);
  }
  assert.throws(() => tb.progressionToWords({ ...song.prog, steps: [{ ...song.prog.steps[0], edit: { lower: 0, sus: 6, add: 0 } }] }), /step 1/);
}

// --- against the firmware ------------------------------------------------------------
// The firmware's native core, as the C suites build it (tests/tb/sources.json).
const core = JSON.parse(fs.readFileSync(path.join(root, 'tests/tb/sources.json'), 'utf8')).core;
function compile() {
  const sources = [...core, 'src/tb/settings.c', 'src/tb/sysex.c', 'tests/webapp/harness.c'];
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
let stage4 = '';

// --- the editor's chords against the notes the firmware plays (tests/webapp/chords.c) -------
function compileChords() {
  const sources = [...core, 'tests/webapp/chords.c'];
  const windows = process.platform === 'win32';
  const at = (p) => (windows ? '/mnt/' + p[0].toLowerCase() + p.slice(2).replaceAll('\\', '/') : p);
  const exe = '/tmp/tb_webapp_chords';
  const run = (shell) => spawnSync(windows ? 'wsl' : 'bash', windows ? ['-e', 'bash', '-lc', shell] : ['-lc', shell], { encoding: 'utf8' });
  if (run(`cd '${at(root)}' && gcc -std=c11 -O1 -Wall -Werror ${sources.join(' ')} -o ${exe}`).status !== 0) return null;
  return (lines) => {
    const out = spawnSync(windows ? 'wsl' : 'bash', windows ? ['-e', exe] : ['-lc', exe], { input: lines.join('\n') + '\n', encoding: 'utf8', maxBuffer: 1 << 28 });
    assert.equal(out.status, 0, out.stderr);
    return out.stdout.split('\n').slice(0, lines.length);
  };
}
const padNotes = compileChords();

// --- the editor's board picture against the firmware's pad colors (tests/webapp/lights.c, 1.9.0) -----------
function compileLights() {
  const { ui } = JSON.parse(fs.readFileSync(path.join(root, 'tests/tb/sources.json'), 'utf8'));
  const sources = [...core, ...ui, 'src/tb/font.c', 'src/tb/serial_midi.c', 'src/tb/settings.c', 'src/tb/sysex.c', 'tests/webapp/lights.c'];
  const windows = process.platform === 'win32';
  const at = (p) => (windows ? '/mnt/' + p[0].toLowerCase() + p.slice(2).replaceAll('\\', '/') : p);
  const exe = '/tmp/tb_webapp_lights';
  const run = (shell) => spawnSync(windows ? 'wsl' : 'bash', windows ? ['-e', 'bash', '-lc', shell] : ['-lc', shell], { encoding: 'utf8' });
  const built = run(`cd '${at(root)}' && gcc -std=c11 -O1 -Wall -Werror ${sources.join(' ')} -o ${exe}`);
  if (built.status !== 0) { assert.ok(!firmware, 'the lights harness builds where the SysEx one does:\n' + built.stderr.slice(0, 600)); return null; }
  return (lines, numbers = false) => { // numbers: decimal numbers (feel), else rrggbb colors
    const out = spawnSync(windows ? 'wsl' : 'bash', windows ? ['-e', exe] : ['-lc', exe], { input: lines.join('\n') + '\n', encoding: 'utf8', maxBuffer: 1 << 28 });
    assert.equal(out.status, 0, out.stderr);
    return out.stdout.split('\n').slice(0, lines.length).map((l) => (numbers ? l.trim().split(' ').map(Number) :
      l.trim().split(' ').map((h) => [0, 2, 4].map((i) => parseInt(h.slice(i, i + 2), 16)))));
  };
}
const padLights = compileLights();
let lightChecks = 0;
if (padLights) {
  const [table] = padLights(['palette']);
  assert.deepEqual(lights.PALETTE.map((p) => p.rgb), table.slice(0, 16), "the palette = the firmware's");
  assert.deepEqual(lights.SCHEMES.flatMap((s) => s.roles.map((r) => r.rgb)), table.slice(16), "built-in role colors = the firmware's");
  assert.equal(lights.ROLES, 36);
  const stepsOf = (id) => tb.SCALES.find((s) => s.id === id).steps;
  const cases = [];
  let seed = 7; const rnd = (n) => { seed = (seed * 1103515245 + 12345) % 2147483648; return seed % n; };
  for (let scheme = 0; scheme < lights.SCHEMES.length; scheme++) for (const tonic of [0, 3, 10]) for (const scale of [0, 1, 8, 9])
    for (const layout of [0, 1, 2]) for (const colorBy of [0, 1]) for (const custom of [false, true]) {
      const roles = Array.from({ length: lights.ROLES }, () => (custom ? rnd(17) : 0));
      cases.push({ look: { scheme, colorBy, keep: 0, roles }, board: { tonic, steps: stepsOf(scale), layout }, line: [tonic, scale, layout, scheme, colorBy, ...roles].join(' ') });
    }
  const got = padLights(cases.map((c) => c.line));
  cases.forEach((c, i) => {
    for (let k = 0; k < 96; k++) assert.deepEqual(lights.padColor(c.look, c.board, k, 74), got[i][k], `pad ${k}: ${c.line}`);
    lightChecks++;
  });
}
// The manual's pad explorer (webapp/tb-pads.js): every pad's notes and the screen's chord names against the firmware's
// (layout.c tc_notes_panel, chord_name.c tn_name) on the same harness: every built-in scale and key with each kind of
// chord pads and melody layout, then random voicings, spreads, octaves, capos and bases. Full comes from the harness too
// (built: no chip here; the published full-chords.json is the original's, from the same harness and a chip image).
let padChecks = 0;
if (padLights) {
  const run = (lines) => {
    const windows = process.platform === 'win32';
    const out = spawnSync(windows ? 'wsl' : 'bash', windows ? ['-e', '/tmp/tb_webapp_lights'] : ['-lc', '/tmp/tb_webapp_lights'],
      { input: lines.join('\n') + '\n', encoding: 'utf8', maxBuffer: 1 << 28 });
    assert.equal(out.status, 0, out.stderr);
    return out.stdout.split('\n').slice(0, lines.length);
  };
  const ids = [...Array(12).keys(), ...Array.from({ length: 13 }, (_, i) => 24 + i)];
  const stepsOf = (id) => tb.SCALES.find((s) => s.id === id).steps;
  const pop = 'C Dm Em F G Am Bb Ab Fm D7 E7 G7'.split(' ').map((n) => chords.parseChord(n)).map((ch) => ({ mask: ch.set, root: ch.root }));
  const fullLines = [];
  for (const id of ids) for (let tonic = 0; tonic < 12; tonic++) fullLines.push(`fullpads ${tonic} ${id}`);
  const fullText = run(fullLines), fullOf = {};
  fullLines.forEach((l, i) => { fullOf[l.slice(9)] = pads.parseFull(fullText[i]); });
  let seed = 11; const rnd = (n) => { seed = (seed * 1103515245 + 12345) % 2147483648; return seed % n; };
  const KINDS = { scale: 0, set: 1, full: 9, grid: 10 }, cases = [];
  const add = (scale, tonic, chordsKind, panel, melodyLayout, melodyBase, chordBase) => {
    const c = { tonic, steps: stepsOf(scale), chords: chordsKind, set: pop, full: fullOf[`${tonic} ${scale}`], chordBase, melodyBase, melodyLayout };
    const line = ['pads', tonic, scale, KINDS[chordsKind], panel.voicing, panel.spread, panel.spreadOf, panel.octaves, panel.capo,
      melodyLayout, melodyBase, chordBase, ...(chordsKind === 'set' ? pop.flatMap((s) => [s.mask, s.root]) : [])].join(' ');
    cases.push({ c, panel, line });
  };
  for (const id of ids) for (let tonic = 0; tonic < 12; tonic++) for (const kind of Object.keys(KINDS))
    add(id, tonic, kind, pads.NEUTRAL, (tonic + id) % 6, pads.MELODY_BASE, pads.CHORD_BASE);
  for (let i = 0; i < 3000; i++) {
    const panel = { voicing: rnd(5), spread: rnd(4), spreadOf: rnd(3), octaves: rnd(3) ? 8 : 1 + rnd(127), capo: rnd(12) };
    add(ids[rnd(ids.length)], rnd(12), Object.keys(KINDS)[rnd(4)], panel, rnd(6), 24 + 12 * rnd(5), 24 + 12 * rnd(4));
  }
  const got = run(cases.map((x) => x.line));
  cases.forEach((x, i) => {
    const want = got[i].split('|');
    for (let k = 0; k < 96; k++) {
      const notes = pads.padNotes(x.c, k, x.panel);
      if (want[k] === 'x') { assert.equal(notes, null, `pad ${k} can't play: ${x.line}`); continue; }
      const [w, name] = want[k].split(';');
      assert.deepEqual(notes, w ? w.split(',').map(Number) : [], `pad ${k}: ${x.line}`);
      if (k < 48) assert.equal(notes.length ? pads.chordName(notes, pads.padRoot(x.c, k)) : '-', name, `pad ${k}'s name: ${x.line}`);
    }
    padChecks++;
  });
}
// 1.9.0 feel: the editor's curves (tb-feel.js) against the firmware's (controls.c), on the same harness.
let feelChecks = 0;
{
  assert.deepEqual(feel.defaults().slice(0, feel.POINTS), feel.LINEAR);
  assert.ok(feel.feelOk(feel.defaults()));
  assert.deepEqual(feel.feelFromWords(feel.feelToWords(feel.defaults())), feel.defaults());
  assert.deepEqual(feel.feelFromWords(new Array(40).fill(0)), feel.defaults(), 'all zero: the defaults');
  assert.throws(() => feel.feelToWords(feel.withPart(feel.defaults(), 0, { points: feel.LINEAR, low: 0, high: 127 })), /not a feel/);
  assert.equal(feel.SHAPES[0].points.join(), feel.LINEAR.join(), 'Normal is the straight line');
  for (let r = 0; r <= 126; r++) assert.ok(Math.abs(feel.velocityOut(feel.defaults(), 0, r) - (1 + r)) <= 2, 'the default Custom ~ Normal');
  // The calibration: soft 900, medium 1800, hard 3400 above rest with 150 of noise.
  const pads = (x) => [x, x + 50, x - 50, 0, 0];
  const p = feel.propose({ quiet: [100, 150], soft: pads(900), medium: pads(1800), hard: pads(3400) });
  assert.deepEqual([p.trigger, p.release, p.full, p.velFull], [5, 1, 30, 34]);
  assert.ok(p.points[0] === 0 && p.points[7] === 127 && p.points.every((v, i) => !i || v >= p.points[i - 1]));
  assert.match(feel.propose({ quiet: [0], soft: pads(900), medium: pads(800), hard: pads(3000) }).error, /harder/);
  assert.match(feel.propose({ quiet: [0], soft: [100], medium: pads(800), hard: pads(3000) }).error, /no clear presses/);
  if (padLights) {
    let seed = 11; const rnd = (n) => { seed = (seed * 1103515245 + 12345) % 2147483648; return seed % n; };
    const feels = [feel.defaults(), ...feel.SHAPES.map((s) => feel.withPart(feel.defaults(), 0, { points: s.points, low: 1, high: 127 }))];
    while (feels.length < 120) {
      let f = feel.defaults();
      for (let i = 0; i < 4; i++) {
        const low = i < 2 ? 1 + rnd(100) : rnd(100), high = Math.min(127, low + 1 + rnd(127 - low));
        f = feel.withPart(f, i, { points: Array.from({ length: feel.POINTS }, () => rnd(128)), low, high });
      }
      feels.push(f);
    }
    const out = padLights(feels.map((f) => 'feel ' + f.join(' ')), true);
    feels.forEach((f, n) => {
      const line = out[n], want = [];
      for (const half of [0, 1]) for (let x = 0; x <= 127; x++) want.push(feel.curve(feel.part(f, half).points, x));
      for (const half of [0, 1]) for (let x = 0; x <= 127; x++) want.push(feel.pressureOut(f, half, x));
      assert.deepEqual(line, want, `feel ${f.join(' ')}`);
      feelChecks++;
    });
  }
}
let chordChecks = 0;
if (padNotes) {
  // Every chord a pad can play (named, with sus, added notes, the third or fifth in the bass) in
  // every built-in scale and key, typed with that scale as the key: the editor's pad and edits
  // play exactly the chord's notes, its bass lowest. Then every one of those chord names typed
  // in major and in minor keys (mostly borrowed), and in three user scales as the key.
  const queries = [], expected = [];
  const ask = (tonic, scale, at, chord, label) => {
    queries.push(`${tonic} ${scale} ${at.pad} ${at.edit.lower} ${at.edit.sus} ${at.edit.add}`); expected.push({ chord, label });
  };
  const named = (steps, tonic, out) => {
    for (let column = 0; column < steps.length; column++) for (const sus of [0, 2, 4]) for (let add = 0; add < 8; add++)
      for (const lower of [0, 2, 4]) {
        const name = chords.chordName(chords.padChord(steps, tonic, 4 * column + 3, { lower, sus, add }));
        if (!name.includes('(')) out.add(name);
      }
    return out;
  };
  const place = (name, tonic, home, scales) => {
    const chord = chords.parseChord(name), at = chords.placeChord(chord, tonic, home, scales);
    assert.ok(at, `${name} in key ${tonic}, scale ${home}`);
    ask(tonic, at.scale === chords.NO_SCALE ? home : at.scale, at, chord, `${name} in key ${tonic}, scale ${home}`);
  };
  for (let tonic = 0; tonic < 12; tonic++) {
    const everything = new Set();
    for (const s of tb.SCALES) for (const name of named(s.steps, tonic, new Set())) { place(name, tonic, s.id); everything.add(name); }
    for (const home of [0, 1]) for (const name of everything) place(name, tonic, home);
  }
  const users = [0x6ad, 0xa95, 0x5ad].map((mask, slot) => ({ id: tb.USER_FIRST + slot, steps: tb.semitonesOf(mask), mask }));
  for (const u of users) for (let tonic = 0; tonic < 12; tonic += 5) {
    for (const name of named(u.steps, tonic, new Set())) place(name, tonic, u.id, [...tb.SCALES, u]);
  }
  const setup = users.map((u, slot) => `U ${slot} ${u.mask.toString(16)}`);
  const replies = padNotes([...setup, ...queries]);
  assert.deepEqual(replies.slice(0, setup.length), setup.map(() => 'ok'));
  replies.slice(setup.length).forEach((line, i) => {
    const { chord, label } = expected[i];
    assert.notEqual(line, '-', `${label}: the firmware can't play it`);
    const notes = line.split(' ').map((h) => parseInt(h, 16));
    const set = notes.reduce((m, n) => m | (1 << (((n - chord.root) % 12) + 12) % 12), 0);
    assert.equal(set, chord.set, `${label}: firmware notes ${notes}`);
    assert.equal(Math.min(...notes) % 12, chord.bass ?? chord.root, `${label}: bass`);
  });
  chordChecks = queries.length;
}
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
    ['no favorites', tb.getFavorites(), tb.CMD.GET_RECORD],
    ['set favorites', tb.setFavorites([40, 13, 0]), tb.CMD.SET_RECORD],
    ['get favorites', tb.getFavorites(), tb.CMD.GET_RECORD],
    ['bad favorites', tb.setRecord(tb.KIND.FAVORITES, 0, tb.favoritesToWords([40]).map((w, i) => (i === 0 ? 0xffff2828 : w))), tb.CMD.SET_RECORD],
    ['select library', tb.selectScale(40), tb.CMD.SELECT_SCALE],
    ['live after', tb.getRecord(tb.KIND.LIVE, 0), tb.CMD.GET_RECORD],
    ['no progression', tb.getRecord(tb.KIND.PROG, 2), tb.CMD.GET_RECORD],
    ['set progression', tb.setProgression(2, chords.progressionFromText('Am D Dm Am C G E7 Am', 9, 1, 3).prog), tb.CMD.SET_RECORD],
    ['get progression', tb.getRecord(tb.KIND.PROG, 2), tb.CMD.GET_RECORD],
    ['bad progression', tb.setRecord(tb.KIND.PROG, 2, tb.progressionToWords(chords.progressionFromText('Am', 9, 1).prog).map((x, i) => (i === 1 ? x | 3 << 20 : x))), tb.CMD.SET_RECORD],
    ['slot 9', tb.setProgression(8, chords.progressionFromText('Am', 9, 1).prog), tb.CMD.SET_RECORD],
    ['clear progression', tb.setProgression(5, { unit: 2, scale: 1, steps: [] }), tb.CMD.SET_RECORD],
    ['get cleared', tb.getRecord(tb.KIND.PROG, 5), tb.CMD.GET_RECORD],
    ['no chord set', tb.getRecord(tb.KIND.CHORDS, 1), tb.CMD.GET_RECORD],
    ['set chord set', tb.setChordSet(1, { name: 'pop', columns: [{ root: 0, set: 0x91 }, { root: 5, set: 0x891 }, null, { root: 19, set: 0x491 }, ...new Array(8).fill(null)] }), tb.CMD.SET_RECORD],
    ['get chord set', tb.getRecord(tb.KIND.CHORDS, 1), tb.CMD.GET_RECORD],
    ['bad chord set', tb.setRecord(tb.KIND.CHORDS, 2, [1, 0x90, ...new Array(38).fill(0)]), tb.CMD.SET_RECORD],
    ['set 9', tb.setRecord(tb.KIND.CHORDS, 8, [1, 0x91, ...new Array(38).fill(0)]), tb.CMD.SET_RECORD],
    ['live 0.12', tb.getRecord(tb.KIND.LIVE, 0), tb.CMD.GET_RECORD],
    // 1.5.0: the step pattern
    ['no lane', tb.getRecord(tb.KIND.LANE, 5), tb.CMD.GET_RECORD],
    ['set lane', tb.setRecord(tb.KIND.LANE, 4, tb.laneChunkToWords({ kind: 1, channel: 3, length: 12, rate: 2,
      steps: [{ on: 1, value: 3, velocity: 90 }, null, { on: 1, value: 7, edit: 0x22, scale: 3 }] }, 0)), tb.CMD.SET_RECORD],
    ['get lane', tb.getRecord(tb.KIND.LANE, 4), tb.CMD.GET_RECORD],
    ['bad lane', tb.setRecord(tb.KIND.LANE, 4, tb.laneChunkToWords({ kind: 4 }, 0)), tb.CMD.SET_RECORD],
    ['lane spare bits', tb.setRecord(tb.KIND.LANE, 6, tb.laneChunkToWords({}, 2).map((w, i) => (i === 2 ? 1 << 20 : w))), tb.CMD.SET_RECORD],
    ['lane settings in chunk 1', tb.setRecord(tb.KIND.LANE, 5, tb.laneChunkToWords({}, 1).map((w, i) => (i === 33 ? 1 : w))), tb.CMD.SET_RECORD],
    ['lane 17', tb.setRecord(tb.KIND.LANE, 16, tb.laneChunkToWords({ kind: 1 }, 0)), tb.CMD.SET_RECORD],
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
  assert.equal(r['no favorites'].status, 7);
  assert.equal(r['set favorites'].status, 0);
  assert.deepEqual(tb.favoritesFromWords(tb.decodeRecord(r['get favorites'].data).words), [40, 13, 0]);
  assert.equal(r['bad favorites'].status, 3, 'an id twice is refused');
  assert.equal(r['select library'].status, 0);
  assert.equal((tb.decodeRecord(r['live after'].data).words[0] >> 8) & 255, 40, 'a library scale (Locrian #2) is playing');
  assert.equal(r['no progression'].status, 7);
  assert.equal(r['set progression'].status, 0);
  const stored = tb.decodeRecord(r['get progression'].data);
  assert.deepEqual([stored.kind, stored.slot], [tb.KIND.PROG, 2]);
  assert.deepEqual(stored.words, tb.progressionToWords(chords.progressionFromText('Am D Dm Am C G E7 Am', 9, 1, 3).prog));
  assert.equal(chords.progressionToText(tb.progressionFromWords(stored.words), 9, (id) => tb.SCALES.find((s) => s.id === id).steps),
    'Am D Dm Am C G E7 Am');
  assert.equal(r['bad progression'].status, 3, 'sus 6 is refused');
  assert.equal(r['slot 9'].status, 3, 'eight slots');
  assert.equal(r['clear progression'].status, 0);
  assert.deepEqual(tb.progressionFromWords(tb.decodeRecord(r['get cleared'].data).words), { unit: 2, scale: 1, steps: [], chords: 0 });
  assert.equal(r['no chord set'].status, 7);
  assert.equal(r['set chord set'].status, 0);
  const storedSet = tb.chordSetFromWords(tb.decodeRecord(r['get chord set'].data).words);
  assert.equal(storedSet.name, 'POP'); assert.deepEqual(storedSet.columns[3], { root: 19, set: 0x491 });
  assert.equal(r['bad chord set'].status, 3, 'a chord without its root is refused');
  assert.equal(r['set 9'].status, 3, 'eight chord sets');
  assert.equal(r['no lane'].status, 7, 'a chunk of defaults is absent');
  assert.equal(r['set lane'].status, 0);
  const lane = tb.decodeRecord(r['get lane'].data);
  assert.deepEqual([lane.kind, lane.slot], [tb.KIND.LANE, 4]);
  assert.deepEqual(lane.words, tb.laneChunkToWords({ kind: 1, channel: 3, length: 12, rate: 2,
    steps: [{ on: 1, value: 3, velocity: 90 }, null, { on: 1, value: 7, edit: 0x22, scale: 3 }] }, 0), 'as sent');
  assert.equal(r['bad lane'].status, 3, 'no kind 4');
  assert.equal(r['lane spare bits'].status, 3, 'spare bits of a step are refused');
  assert.equal(r['lane settings in chunk 1'].status, 3, 'the lane settings live in chunk 0');
  assert.equal(r['lane 17'].status, 3, 'sixteen records');
  // The editor's changes to the live settings: buttons and a chord set, read back; then a set in use can't be cleared.
  const live12 = tb.decodeRecord(r['live 0.12'].data).words;
  assert.deepEqual(tb.buttonsFromWords(live12), tb.BUTTONS.map((_, i) => i), 'buttons start as printed');
  const actions = tb.BUTTONS.map((_, i) => i); actions[13] = tb.ACTIONS.indexOf('Shift'); actions[1] = tb.ACTIONS.indexOf('Add 7th (while held)');
  const [, setLive, getLive, clearInUse, setOff, clearNow, gone] = firmware([tb.setChordSet(1, storedSet),
    tb.setRecord(tb.KIND.LIVE, 0, tb.withChordSet(tb.withButtons(live12, actions), 2)), tb.getRecord(tb.KIND.LIVE, 0),
    tb.clearChordSet(1), tb.setRecord(tb.KIND.LIVE, 0, tb.withChordSet(live12, 0)), tb.clearChordSet(1), tb.getRecord(tb.KIND.CHORDS, 1)]);
  assert.equal(tb.parseReply(setLive, tb.CMD.SET_RECORD).status, 0, 'buttons and chord set 2 accepted');
  const back12 = tb.decodeRecord(tb.parseReply(getLive, tb.CMD.GET_RECORD).data).words;
  assert.deepEqual(tb.buttonsFromWords(back12), actions); assert.equal(back12[34] & 255, 2);
  assert.equal(tb.parseReply(clearInUse, tb.CMD.SET_RECORD).status, 3, 'the set in use stays');
  assert.equal(tb.parseReply(setOff, tb.CMD.SET_RECORD).status, 0);
  assert.equal(tb.parseReply(clearNow, tb.CMD.SET_RECORD).status, 0);
  assert.equal(tb.parseReply(gone, tb.CMD.GET_RECORD).status, 7, 'a cleared set is absent');
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
  // 1.9.0, the Settings tab: readMenu over the firmware's own MENU and ITEM replies (asked in three
  // rounds, every request of a round in one harness run), and help from the manual for every item.
  const answers = new Map(), key = (m) => Buffer.from(m).toString('hex');
  const ask = (requests) => firmware(requests).forEach((reply, i) => answers.set(key(requests[i]), reply));
  const PAGES_NOW = 17; // 2.0.1: the rebalanced menu (13 before)
  ask(Array.from({ length: PAGES_NOW }, (_, p) => tb.menu(p)));
  const pagesFirst = Array.from({ length: PAGES_NOW }, (_, p) => tb.decodeMenu(tb.parseReply(answers.get(key(tb.menu(p))), tb.CMD.MENU).data));
  assert.equal(pagesFirst[0].pages, PAGES_NOW);
  const ids = pagesFirst.flatMap((p) => p.items);
  ask(ids.map((id) => tb.item(id)));
  const more = ids.map((id) => tb.decodeItem(tb.parseReply(answers.get(key(tb.item(id))), tb.CMD.ITEM).data))
    .filter((it) => it.labels.length < it.count).map((it) => tb.item(it.id, it.labels.length));
  if (more.length) ask(more);
  const replyTo = async (m) => { const reply = tb.parseReply(answers.get(key(m)), m[4]); if (reply.status) throw new Error(tb.STATUS[reply.status]); return reply.data; };
  const menuRead = await tb.readMenu(replyTo);
  const { HELP, PAGES } = await import('../../webapp/tb-help.js');
  let menuItems = 0;
  for (const page of menuRead) {
    assert.ok(PAGES[page.name], `the manual's table has page ${page.name}`);
    for (const it of page.items) {
      menuItems++;
      assert.equal(it.labels.length, it.count, `${page.name}/${it.name}: every value named`);
      assert.ok(it.value < it.count || it.kind === tb.ITEM_KIND.BOARD, `${page.name}/${it.name}: value in range`);
      const help = HELP[`${page.name}/${it.name}`] || (page.name === 'BUTTONS' && HELP['BUTTONS/*']) || (it.name === 'RESET OPTIONS' && HELP['*/RESET OPTIONS']);
      assert.ok(help, `the manual's Settings menu table has ${page.name}/${it.name}`);
    }
  }
  const play = menuRead[0], scaleItem = play.items.find((it) => it.name === 'SCALE'), chordVel = play.items.find((it) => it.name === 'CHORD VEL');
  assert.equal(scaleItem.labels[0], 'MAJOR'); assert.ok(scaleItem.count > 90);
  assert.equal(chordVel.kind, tb.ITEM_KIND.VALUE); assert.equal(chordVel.count, 7); assert.equal(chordVel.value, 0);
  assert.equal(chordVel.labels[feel.CUSTOM], 'CUSTOM');
  // SET_ITEM: no reply, and ITEM then shows the new value; a quiet SET_RECORD: no reply, applied.
  const liveNow = tb.decodeRecord(tb.parseReply(firmware([tb.getRecord(tb.KIND.LIVE, 0)])[0], tb.CMD.GET_RECORD).data).words;
  const lowered = liveNow.map((w, i) => (i === 0 ? ((w & ~0xff0000) | (40 << 16)) >>> 0 : w));
  const [setNone, itemAfter, quietNone, liveAfter] = firmware([tb.setItem(chordVel.id, 3), tb.item(chordVel.id),
    tb.setRecordQuiet(tb.KIND.LIVE, 0, lowered), tb.getRecord(tb.KIND.LIVE, 0)]);
  assert.equal(setNone.length, 0, 'SET_ITEM has no reply');
  assert.equal(tb.decodeItem(tb.parseReply(itemAfter, tb.CMD.ITEM).data).value, 3);
  assert.equal(quietNone.length, 0, 'a quiet SET_RECORD has no reply');
  assert.equal(tb.decodeRecord(tb.parseReply(liveAfter, tb.CMD.GET_RECORD).data).words[0], lowered[0]);
  // 1.9.0 looks: the look on show (kind 8 slot 9) written quietly and read back through the editor's encoders.
  const look = { scheme: 4, colorBy: 1, keep: 1, roles: Array.from({ length: lights.ROLES }, (_, i) => (i * 7) % 17) };
  const [lookNone, lookBack, lookBad] = firmware([tb.setRecordQuiet(tb.KIND.LOOK, lights.LOOK_NOW, lights.lookToWords(look)),
    tb.getRecord(tb.KIND.LOOK, lights.LOOK_NOW), tb.setRecord(tb.KIND.LOOK, 1, lights.lookToWords(look))]);
  assert.equal(lookNone.length, 0);
  assert.deepEqual(lights.lookFromWords(tb.decodeRecord(tb.parseReply(lookBack, tb.CMD.GET_RECORD).data).words), { ...look, showing: 0 });
  assert.equal(tb.parseReply(lookBad, tb.CMD.SET_RECORD).status, 3, "Keep look only in the board's look");
  // 1.9.0 feel: the curves read (the defaults), written quietly and read back; PADS (no scanner here: readings 0).
  const custom = feel.withPart(feel.defaults(), 1, { points: feel.SHAPES[3].points, low: 20, high: 110 });
  const [feelFirst, feelNone, feelBack, padsReply] = firmware([tb.getRecord(tb.KIND.FEEL, 0),
    tb.setRecordQuiet(tb.KIND.FEEL, 0, feel.feelToWords(custom)), tb.getRecord(tb.KIND.FEEL, 0), tb.pads()]);
  assert.deepEqual(feel.feelFromWords(tb.decodeRecord(tb.parseReply(feelFirst, tb.CMD.GET_RECORD).data).words), feel.defaults());
  assert.equal(feelNone.length, 0);
  assert.deepEqual(feel.feelFromWords(tb.decodeRecord(tb.parseReply(feelBack, tb.CMD.GET_RECORD).data).words), custom);
  const padsNow = tb.decodePads(tb.parseReply(padsReply, tb.CMD.PADS).data);
  assert.equal(padsNow.pads.length, 96); assert.equal(padsNow.trigger, 7); assert.equal(padsNow.full, 25);
  assert.ok(padsNow.pads.every((p) => p.rest === 0 && p.value === 0) && !padsNow.scanning);
  // Custom velocity (6) is Normal for firmware before 1.9; kept for 1.9.
  const customVel = { kind: tb.KIND.LIVE, slot: 0, words: new Array(40).fill(0).map((w, i) => (i === 1 ? 0x03010602 : i === 31 ? 0x86000101 : w)) };
  const vel18 = tb.recordsFor([customVel], '1.8.5').records[0].words, vel19 = tb.recordsFor([customVel], '1.9.1').records[0].words;
  assert.deepEqual([vel18[1], vel18[31], vel19[1], vel19[31]], [0x03010002, 0x80000101, 0x03010602, 0x86000101]);
  assert.equal(tb.recordsFor([{ kind: tb.KIND.FEEL, slot: 0, words: feel.feelToWords(custom) }], '1.8.5').skipped.length, 1);
  // 1.9.3, Customize Stage 4: the step grid, chord pad edits and presets through the editor's own models, against
  // the firmware. Item ids come from the board's own menu (menuRead), never from this file.
  const itemId = (page, name) => menuRead.find((p) => p.name === page).items.find((it) => it.name === name).id;
  let seed4 = 23; const rnd4 = (n) => { seed4 = (seed4 * 1103515245 + 12345) % 2147483648; return seed4 % n; };
  const randomStep = (kind) => ({ ...lanes.emptyStep(), on: !!rnd4(2), tie: !rnd4(5), value: kind === lanes.KIND.DRUMS ? rnd4(128) : rnd4(48),
    velocity: rnd4(3) ? 0 : 1 + rnd4(127), edit: lanes.editByte({ lower: rnd4(8), sus: 2 * rnd4(3), add: rnd4(8) }),
    scale: rnd4(4) ? 0 : rnd4(40), gate: rnd4(lanes.GATES), probability: rnd4(lanes.PROBABILITIES), ratchet: rnd4(lanes.RATCHETS),
    condition: rnd4(lanes.CONDITIONS.length), nudge: rnd4(3) ? 0 : rnd4(16) });
  const randomPattern = () => {
    const p = lanes.emptyPattern(); p.seed = rnd4(65536);
    p.lanes.forEach((l) => { Object.assign(l, { kind: rnd4(4), channel: rnd4(17), length: rnd4(65), rate: rnd4(13), direction: rnd4(4),
      mute: rnd4(2), euclid: rnd4(17), swing: rnd4(4) ? 0 : rnd4(51), transpose: rnd4(4) ? 0 : rnd4(97) - 48 }); l.steps = l.steps.map(() => randomStep(l.kind)); });
    return p;
  };
  // Steps in 64 bits: every field round trips; spare bits, sus 3 and an unknown scale are refused, as lanes.c does.
  for (let i = 0; i < 2000; i++) { const s = randomStep(rnd4(3)); assert.deepEqual(lanes.stepUnpack(...lanes.stepPack(s)), s); }
  assert.equal(lanes.stepUnpack(0, 1 << 19), null, 'bit 51');
  assert.equal(lanes.stepUnpack(3 << 18, 0), null, 'sus 3');
  // Patterns written as quiet records (all 16) and read back: as sent, as the editor encodes them.
  const patterns = Array.from({ length: 12 }, randomPattern);
  const laneRequests = [];
  for (const p of patterns) {
    for (let slot = 0; slot < lanes.RECORDS; slot++) laneRequests.push(tb.setRecordQuiet(tb.KIND.LANE, slot, lanes.encodeChunk(p, slot)));
    for (let slot = 0; slot < lanes.RECORDS; slot++) laneRequests.push(tb.getRecord(tb.KIND.LANE, slot));
  }
  const laneReplies = firmware(laneRequests);
  let laneChecks = 0;
  patterns.forEach((p, n) => {
    const records = [];
    for (let slot = 0; slot < lanes.RECORDS; slot++) {
      const quiet = laneReplies[n * 32 + slot], got = tb.parseReply(laneReplies[n * 32 + 16 + slot], tb.CMD.GET_RECORD);
      assert.equal(quiet.length, 0, 'a quiet lane record has no reply');
      if (got.status === 7) { assert.ok(lanes.chunkDefault(p, slot), `slot ${slot} absent only when all defaults`); records.push(null); continue; }
      const words = tb.decodeRecord(got.data).words;
      assert.deepEqual(words, lanes.encodeChunk(p, slot), `pattern ${n} slot ${slot}`);
      records.push(words); laneChecks++;
    }
    assert.deepEqual(lanes.patternFromRecords(records), p, 'the editor reads back what it wrote');
  });
  // Damaged records: the editor's decoder and the firmware refuse the same ones.
  const damaged = [];
  for (let i = 0; i < 300; i++) {
    const slot = rnd4(16), w = lanes.encodeChunk(patterns[i % patterns.length], slot), word = rnd4(40);
    w[word] = (w[word] ^ (1 << rnd4(32))) >>> 0;
    damaged.push({ slot, w });
  }
  const damagedReplies = firmware(damaged.map((d) => tb.setRecord(tb.KIND.LANE, d.slot, d.w)));
  let laneRefusals = 0;
  damaged.forEach((d, i) => {
    const status = tb.parseReply(damagedReplies[i], tb.CMD.SET_RECORD).status, ok4 = lanes.decodeChunk(d.w, d.slot, lanes.emptyPattern());
    assert.equal(status === 0, ok4, `damaged slot ${d.slot}: firmware ${status}, editor ${ok4}`);
    if (!ok4) laneRefusals++;
  });
  // Euclid fill and Rotate: the editor's fill against the board's own (the Sequence page's items through SET_ITEM).
  const [LANE, KIND, ROTATE, EUCLID] = ['LANE', 'KIND', 'ROTATE', 'EUCLID'].map((n) => itemId('STEPS', n));
  const euclidCases = [], euclidRequests = [];
  for (let i = 0; i < 160; i++) {
    const p = randomPattern(), li = rnd4(4), l = p.lanes[li], kind = rnd4(3);
    l.kind = lanes.KIND.OFF; l.euclid = 0;
    const n = lanes.length(l), hits = rnd4(n + 1), rotate = rnd4(n);
    for (let c = 0; c < 4; c++) euclidRequests.push(tb.setRecordQuiet(tb.KIND.LANE, li * 4 + c, lanes.encodeChunk(p, li * 4 + c)));
    euclidRequests.push(tb.setItem(LANE, li), tb.setItem(KIND, kind), tb.setItem(ROTATE, rotate), tb.setItem(EUCLID, hits));
    for (let c = 0; c < 4; c++) euclidRequests.push(tb.getRecord(tb.KIND.LANE, li * 4 + c));
    const want = lanes.copyPattern(p); want.lanes[li].kind = kind;
    lanes.euclidFill(want.lanes[li], hits, rotate, lanes.brushDefault(kind));
    euclidCases.push({ li, want, hits, rotate, n });
  }
  const euclidReplies = firmware(euclidRequests);
  euclidCases.forEach((c, i) => {
    for (let k = 0; k < 4; k++) {
      const reply = tb.parseReply(euclidReplies[i * 12 + 8 + k], tb.CMD.GET_RECORD);
      const words = reply.status === 7 ? lanes.encodeChunk(lanes.emptyPattern(), c.li * 4 + k) : tb.decodeRecord(reply.data).words;
      assert.deepEqual(words.slice(0, 36), lanes.encodeChunk(c.want, c.li * 4 + k).slice(0, 36), `Euclid ${c.hits} of ${c.n} rotated ${c.rotate}, lane ${c.li + 1} chunk ${k}`);
    }
  });
  for (let steps = 1; steps <= 64; steps++) for (let hits = 0; hits <= steps; hits++) {
    const m = lanes.euclid(hits, steps); assert.equal(m.filter(Boolean).length, hits); if (hits) assert.ok(m[0], 'the first step sounds');
  }
  // Chord pad edits: SET_EDITS without a reply, the live record then holds them; CHORD_NAMES names every pad.
  const edits = { edits: Array.from({ length: 48 }, () => ({ lower: rnd4(8), sus: 2 * rnd4(3), add: rnd4(8) })),
    strumOff: Array.from({ length: 48 }, () => !rnd4(3)), soundOff: Array.from({ length: 48 }, () => !rnd4(4)) };
  const [editsNone, editsLive, names0, badEdits, liveAfterBad] = firmware([tb.setEdits(edits), tb.getRecord(tb.KIND.LIVE, 0), tb.chordNames(),
    tb.request(tb.CMD.SET_EDITS, tb.pack([3 << 3, ...new Array(59).fill(0)])), tb.getRecord(tb.KIND.LIVE, 0)]);
  assert.equal(editsNone.length, 0, 'SET_EDITS has no reply');
  assert.deepEqual(tb.padEditsFromWords(tb.decodeRecord(tb.parseReply(editsLive, tb.CMD.GET_RECORD).data).words), edits);
  const padNames = tb.decodeChordNames(tb.parseReply(names0, tb.CMD.CHORD_NAMES).data);
  assert.equal(padNames.length, 48); assert.ok(padNames.every((n) => n.length > 0));
  assert.equal(badEdits.length, 0);
  assert.deepEqual(tb.parseReply(liveAfterBad, tb.CMD.GET_RECORD).data, tb.parseReply(editsLive, tb.CMD.GET_RECORD).data, 'sus 3 refused: nothing changed');
  assert.throws(() => tb.setEdits({ ...edits, edits: [{ lower: 0, sus: 6, add: 0 }, ...edits.edits.slice(1)] }), /sus/);
  // Plain pads in C major (the bottom row, root position) are named as the editor names them.
  const plain = { edits: new Array(48).fill({ lower: 0, sus: 0, add: 0 }), strumOff: new Array(48).fill(false), soundOff: new Array(48).fill(false) };
  const [plainNone, plainNames] = firmware([tb.setEdits(plain), tb.chordNames()]);
  assert.equal(plainNone.length, 0);
  const cMajor = tb.SCALES.find((x) => x.id === 0).steps, boardNames = tb.decodeChordNames(tb.parseReply(plainNames, tb.CMD.CHORD_NAMES).data);
  for (let k = 3; k < 48; k += 4) assert.equal(boardNames[k], chords.chordName(chords.padChord(cMajor, 0, k, { lower: 0, sus: 0, add: 0 })), `pad ${k}`);
  // VALUES: the live settings' values are what ITEM gives every item; a stored preset's; compared through the menu's names.
  const [valuesLive] = firmware([tb.values(tb.KIND.LIVE, 0)]);
  const liveValues = tb.decodeValues(tb.parseReply(valuesLive, tb.CMD.VALUES).data).values;
  let valueChecks = 0;
  for (const page of menuRead) for (const it of page.items) { assert.equal(liveValues[it.id], it.value, `${page.name}/${it.name}`); valueChecks++; }
  // STATE (1.9.4): which items show = ITEM's (the menu read from a board as fresh); the check moves with a value and
  // comes back with it; the saved changes' count moves on.
  const hold = menuRead.flatMap((p) => p.items).find((it) => it.name === 'HOLD');
  const [s0, , s1, , s2] = firmware([tb.boardState(), tb.setItem(hold.id, 1), tb.boardState(), tb.setItem(hold.id, 0), tb.boardState()])
    .map((r, i) => (i % 2 ? r : tb.decodeState(tb.parseReply(r, tb.CMD.STATE).data)));
  let stateChecks = 0;
  for (const page of menuRead) for (const it of page.items) { assert.equal(s0.shown[it.id], it.shown, `shown ${page.name}/${it.name}`); stateChecks++; }
  assert.ok(s1.check !== s0.check && s2.check === s0.check && s1.changes !== s0.changes && s2.lanes === s0.lanes, 'STATE follows Hold');
  assert.ok(tb.stateSupported('1.9.4') && !tb.stateSupported('1.9.3') && tb.stateSupported('2.0.0'));
  assert.throws(() => tb.decodeState(new Uint8Array(21)), /short/);
  const live4 = tb.decodeRecord(tb.parseReply(firmware([tb.getRecord(tb.KIND.LIVE, 0)])[0], tb.CMD.GET_RECORD).data).words;
  const firm = live4.map((w, i) => (i === 1 ? ((w & ~0xff00) | 3 << 8) >>> 0 : w));
  const [storeA, , valuesA, valuesNow, emptyValues] = firmware([tb.setRecord(tb.KIND.PRESET, 2, firm), tb.setItem(chordVel.id, 1),
    tb.values(tb.KIND.PRESET, 2), tb.values(tb.KIND.LIVE, 0), tb.values(tb.KIND.PRESET, 4)]);
  assert.equal(tb.parseReply(storeA, tb.CMD.SET_RECORD).status, 0);
  assert.equal(tb.parseReply(emptyValues, tb.CMD.VALUES).status, 7, 'an empty preset');
  const rowsAB = presets.compare(menuRead, tb.decodeValues(tb.parseReply(valuesA, tb.CMD.VALUES).data).values, tb.decodeValues(tb.parseReply(valuesNow, tb.CMD.VALUES).data).values);
  assert.deepEqual(rowsAB.filter((r) => !r.same).map((r) => [r.page, r.item, r.a, r.b]), [['PLAY', 'CHORD VEL', 'FIRM', 'FIXED 100'], ['PLAY', 'MELODY VEL', 'FIRM', 'NORMAL']],
    'Chord vel, and Melody vel, which follows it in a record that sets no melody velocity of its own');
  // Preset names: stored quietly, read back, shown by Load preset (the board's ITEM labels); a preset cleared by zeros.
  const names = ['MY BALLAD 12', '', 'funk!', 'X', '', '', '', 'LAST ONE'];
  const [storeP, namesNone, namesBack, loadItem, clearP, presetGone] = firmware([tb.setRecord(tb.KIND.PRESET, 0, live4),
    tb.setRecordQuiet(tb.KIND.NAMES, 0, presets.namesToWords(names)), tb.getRecord(tb.KIND.NAMES, 0), tb.item(itemId('PRESETS', 'LOAD PRESET')),
    tb.setRecord(tb.KIND.PRESET, 0, presets.cleared()), tb.getRecord(tb.KIND.PRESET, 0)]);
  assert.equal(tb.parseReply(storeP, tb.CMD.SET_RECORD).status, 0); assert.equal(namesNone.length, 0);
  assert.deepEqual(presets.namesFromWords(tb.decodeRecord(tb.parseReply(namesBack, tb.CMD.GET_RECORD).data).words), names.map(tb.cleanName));
  assert.deepEqual(tb.decodeItem(tb.parseReply(loadItem, tb.CMD.ITEM).data).labels.slice(0, 3), ['1 MY BALLAD 12', '2 EMPTY', '3 EMPTY']);
  assert.equal(tb.parseReply(clearP, tb.CMD.SET_RECORD).status, 0);
  assert.equal(tb.parseReply(presetGone, tb.CMD.GET_RECORD).status, 7, 'a preset of zeros is cleared');
  // A preset's own look and curves made from the ones showing (slots 9) are taken; Keep feel rides in slot 9.
  const keepFeel = feel.feelToWords(custom, 1);
  const [lookNow, feelSet, feelNow, ownLook, ownFeel, ownFeelBack, keepItem] = firmware([tb.getRecord(tb.KIND.LOOK, tb.LOOKS),
    tb.setRecordQuiet(tb.KIND.FEEL, tb.FEEL_NOW, keepFeel), tb.getRecord(tb.KIND.FEEL, tb.FEEL_NOW),
    tb.setRecord(tb.KIND.LOOK, 3, presets.asPresetLook(lights.lookToWords({ ...look, keep: 1 }))),
    tb.setRecord(tb.KIND.FEEL, 3, presets.asPresetFeel(keepFeel)), tb.getRecord(tb.KIND.FEEL, 3), tb.item(itemId('PRESETS', 'KEEP FEEL'))]);
  assert.equal(tb.parseReply(lookNow, tb.CMD.GET_RECORD).status, 0); assert.equal(feelSet.length, 0);
  const nowWords = tb.decodeRecord(tb.parseReply(feelNow, tb.CMD.GET_RECORD).data).words;
  assert.deepEqual([feel.feelKeep(nowWords), feel.feelWhose(nowWords)], [1, 0]); assert.deepEqual(feel.feelFromWords(nowWords), custom);
  assert.equal(tb.parseReply(ownLook, tb.CMD.SET_RECORD).status, 0); assert.equal(tb.parseReply(ownFeel, tb.CMD.SET_RECORD).status, 0);
  assert.deepEqual(tb.decodeRecord(tb.parseReply(ownFeelBack, tb.CMD.GET_RECORD).data).words, presets.asPresetFeel(keepFeel));
  assert.equal(tb.decodeItem(tb.parseReply(keepItem, tb.CMD.ITEM).data).value, 1, 'Keep feel from the curves in use');
  const [keepInPreset] = firmware([tb.setRecord(tb.KIND.FEEL, 3, keepFeel)]);
  // 2.0.6 (board test Z3): through the menu, as on the board: the board's curves Firm, Keep feel, Save preset 8; the
  // board's curves Light; Load preset 8 brings Firm back as preset 8's own; editing them then changes preset 8's.
  const shaped = (k) => feel.withPart(feel.defaults(), 0, { points: feel.SHAPES[k].points, low: 1, high: 127 });
  const firmCurve = shaped(3), lightCurve = shaped(1);
  const z3 = firmware([tb.setRecordQuiet(tb.KIND.FEEL, tb.FEEL_NOW, feel.feelToWords(firmCurve, 1)), tb.setItem(itemId('PRESETS', 'SAVE PRESET'), 7),
    tb.setRecordQuiet(tb.KIND.FEEL, tb.FEEL_NOW, feel.feelToWords(lightCurve, 1)), tb.getRecord(tb.KIND.FEEL, tb.FEEL_NOW),
    tb.setItem(itemId('PRESETS', 'LOAD PRESET'), 7), tb.getRecord(tb.KIND.FEEL, tb.FEEL_NOW), tb.getRecord(tb.KIND.FEEL, 8),
    tb.setRecordQuiet(tb.KIND.FEEL, tb.FEEL_NOW, feel.feelToWords(lightCurve, 1)), tb.getRecord(tb.KIND.FEEL, 8)]);
  const z3Words = (i) => tb.decodeRecord(tb.parseReply(z3[i], tb.CMD.GET_RECORD).data).words;
  assert.deepEqual([feel.feelWhose(z3Words(3)), feel.feelFromWords(z3Words(3))], [0, lightCurve], "the board's curves: Light");
  assert.deepEqual([feel.feelWhose(z3Words(5)), feel.feelFromWords(z3Words(5))], [8, firmCurve], "preset 8 loaded: its own, Firm");
  assert.deepEqual(feel.feelFromWords(z3Words(6)), firmCurve, "preset 8's own as saved");
  assert.equal(tb.parseReply(z3[8], tb.CMD.GET_RECORD).status, 0);
  // 2.0.8 (board test C1): preset 4 saved with Chord vel Normal, Melody vel Fixed 100; its Chord vel made Custom in
  // its record without loading it (the TB's own Light untouched); loading it brings Custom, Melody vel kept.
  const chordId = chordVel.id, melodyVel = itemId('PLAY', 'MELODY VEL');
  const c1Setup = [tb.setItem(chordId, 0), tb.setItem(melodyVel, 1), tb.setItem(itemId('PRESETS', 'SAVE PRESET'), 3), tb.setItem(chordId, 2)];
  const c1Words = tb.decodeRecord(tb.parseReply(firmware([...c1Setup, tb.getRecord(tb.KIND.PRESET, 3)])[4], tb.CMD.GET_RECORD).data).words;
  assert.deepEqual(presets.presetVel(c1Words), [0, 1], 'as saved');
  const c1Custom = presets.withPresetVel(c1Words, 0, feel.CUSTOM);
  assert.deepEqual(presets.presetVel(c1Custom), [feel.CUSTOM, 1]);
  assert.deepEqual(presets.presetVel(presets.withPresetVel(c1Custom, 1, feel.CUSTOM)), [feel.CUSTOM, feel.CUSTOM]);
  assert.equal(presets.withPresetVel(c1Custom, 1, feel.CUSTOM)[31] >>> 24, 0, 'the same: no Melody vel of its own');
  const c1 = firmware([...c1Setup, tb.setRecordQuiet(tb.KIND.PRESET, 3, c1Custom), tb.item(chordId),
    tb.setItem(itemId('PRESETS', 'LOAD PRESET'), 3), tb.item(chordId), tb.item(melodyVel)]);
  const c1Value = (i) => tb.decodeItem(tb.parseReply(c1[i], tb.CMD.ITEM).data).value;
  assert.deepEqual([c1Value(5), c1Value(7), c1Value(8)], [2, feel.CUSTOM, 1], "the TB's Light kept; preset 4 loaded: Custom, Fixed 100");
  assert.equal(tb.parseReply(keepInPreset, tb.CMD.SET_RECORD).status, 3, "Keep feel only in the board's curves");
  // Backups: names and the presets' own curves are left out before 1.9.3, Keep feel cleared.
  const fit192 = tb.recordsFor([{ kind: tb.KIND.NAMES, slot: 0, words: presets.namesToWords(names) }, { kind: tb.KIND.FEEL, slot: 3, words: presets.asPresetFeel(keepFeel) },
    { kind: tb.KIND.FEEL, slot: 0, words: keepFeel }], '1.9.2');
  assert.deepEqual([fit192.skipped.length, fit192.records.length, fit192.records[0].words[0]], [2, 1, 1]);
  assert.equal(tb.recordsFor([{ kind: tb.KIND.NAMES, slot: 0, words: presets.namesToWords(names) }], '1.9.3').skipped.length, 0);
  assert.ok(tb.versionAtLeast('1.9.3', 1, 9, 3) && !tb.versionAtLeast('1.9.2', 1, 9, 3) && tb.versionAtLeast('1.10.0', 1, 9, 3) && tb.versionAtLeast('2.0.0', 1, 9));
  stage4 = `content (1.9.3): ${laneChecks} lane records of ${patterns.length} random patterns written quietly and read back, ${damaged.length} damaged ` +
    `ones refused alike (${laneRefusals}), ${euclidCases.length} Euclid fills and rotations = the board's, chord pad edits and names, ` +
    `VALUES = every item's ITEM value (${valueChecks}), the compare, preset names on Load preset, a cleared preset, presets' own looks and curves, Keep feel; STATE (1.9.4): ${stateChecks} items' shown = ITEM's, the check follows a value and back`;

  // --- TBOSS 2.0: the chain, each chord's chance, presets' own chord sets, copy/paste/shift, backups ---------
  let seed2 = 2026; const rnd2 = (n) => { seed2 = (seed2 * 1103515245 + 12345) >>> 0; return (seed2 >>> 8) % n; };
  const status = (reply, cmd) => tb.parseReply(reply, cmd).status;
  const wordsOf = (reply) => tb.decodeRecord(tb.parseReply(reply, tb.CMD.GET_RECORD).data).words;
  // The chain: every length, written and read back as the firmware keeps it; damaged ones refused alike.
  const chains = Array.from({ length: 40 }, (_, i) => Array.from({ length: i % 9 }, () => rnd2(8)));
  const chainReplies = firmware(chains.flatMap((c) => [tb.setChain(c), tb.getRecord(tb.KIND.CHAIN, 0)]));
  chains.forEach((c, i) => {
    assert.equal(status(chainReplies[2 * i], tb.CMD.SET_RECORD), 0, `chain ${c}`);
    const got = chainReplies[2 * i + 1];
    if (!c.length) assert.equal(status(got, tb.CMD.GET_RECORD), 7, 'no chain: absent');
    else assert.deepEqual(tb.chainFromWords(wordsOf(got)), c);
  });
  assert.throws(() => tb.chainToWords([0, 8]), /up to 8/); assert.throws(() => tb.chainToWords(new Array(9).fill(0)), /up to 8/);
  let chainRefusals = 0;
  const damagedChains = Array.from({ length: 300 }, () => {
    const w = tb.chainToWords(chains[1 + rnd2(chains.length - 1)]), at = rnd2(3) ? rnd2(2) : rnd2(40);
    w[at] = (w[at] ^ (1 << rnd2(32))) >>> 0; return w;
  });
  firmware(damagedChains.map((w) => tb.setRecord(tb.KIND.CHAIN, 0, w))).forEach((r, i) => {
    const ok2 = tb.chainFromWords(damagedChains[i]) !== null;
    assert.equal(status(r, tb.CMD.SET_RECORD) === 0, ok2, `chain ${damagedChains[i].slice(0, 2)}`); chainRefusals += !ok2;
  });
  // Each chord's chance: random chances for every progression slot, round trip; damaged ones alike.
  const chanceSets = Array.from({ length: 40 }, () => Array.from({ length: rnd2(39) }, () => ({ chance: rnd2(3) ? rnd2(10) : 0, borrow: !!rnd2(2) }))
    .map((c) => (c.chance ? c : { chance: 0, borrow: false })));
  const chanceReplies = firmware(chanceSets.flatMap((c, i) => [tb.setRecord(tb.KIND.CHANCE, i % 8, tb.chancesToWords(c)), tb.getRecord(tb.KIND.CHANCE, i % 8)]));
  chanceSets.forEach((c, i) => {
    assert.equal(status(chanceReplies[2 * i], tb.CMD.SET_RECORD), 0);
    const got = chanceReplies[2 * i + 1], plain = c.every((x) => !x.chance && !x.borrow);
    if (plain) assert.equal(status(got, tb.CMD.GET_RECORD), 7, 'every chord at 100 %: absent');
    else assert.deepEqual(tb.chancesFromWords(wordsOf(got), c.length), c);
  });
  let chanceRefusals = 0;
  const damagedChances = Array.from({ length: 300 }, () => {
    const w = tb.chancesToWords(chanceSets[rnd2(chanceSets.length)]), at = rnd2(3) ? rnd2(11) : rnd2(40);
    w[at] = (w[at] ^ (1 << rnd2(32))) >>> 0; return w;
  });
  firmware(damagedChances.map((w) => tb.setRecord(tb.KIND.CHANCE, rnd2(8), w))).forEach((r, i) => {
    const ok2 = tb.chancesFromWords(damagedChances[i]) !== null;
    assert.equal(status(r, tb.CMD.SET_RECORD) === 0, ok2, `chances ${damagedChances[i].slice(0, 11)}`); chanceRefusals += !ok2;
  });
  // Presets' own chord sets: stored and read back; Keep chords in slot 0; the TB now (slot 9) with no preset's own
  // in use; damaged ones refused alike; a preset's own of zeros clears it.
  const pop = { name: 'pop', columns: [{ root: 0, set: 0x91 }, { root: 5, set: 0x891 }, null, { root: 19, set: 0x491 }, ...new Array(8).fill(null)] };
  const own = tb.ownSetToWords(3, pop);
  const [ownSet, ownBack, ownNone, keepSet, keepBack, nowBack, ownClear, ownGone] = firmware([tb.setRecord(tb.KIND.OWN_SET, 2, own),
    tb.getRecord(tb.KIND.OWN_SET, 2), tb.getRecord(tb.KIND.OWN_SET, 5), tb.setRecord(tb.KIND.OWN_SET, 0, tb.keepChordsWords(1)),
    tb.getRecord(tb.KIND.OWN_SET, 0), tb.getRecord(tb.KIND.OWN_SET, tb.OWN_SET_NOW), tb.setRecord(tb.KIND.OWN_SET, 2, presets.cleared()),
    tb.getRecord(tb.KIND.OWN_SET, 2)]);
  assert.equal(status(ownSet, tb.CMD.SET_RECORD), 0); assert.deepEqual(wordsOf(ownBack), own);
  assert.deepEqual(tb.ownSetFromWords(wordsOf(ownBack)), { set: 3, chords: tb.chordSetFromWords(tb.chordSetToWords(pop)), keep: 0, whose: 0 });
  assert.equal(status(ownNone, tb.CMD.GET_RECORD), 7);
  assert.equal(status(keepSet, tb.CMD.SET_RECORD), 0); assert.deepEqual(wordsOf(keepBack), tb.keepChordsWords(1));
  const now2 = tb.ownSetFromWords(wordsOf(nowBack));
  assert.deepEqual([now2.keep, now2.whose], [1, 0], 'the TB now: Keep chords on, no preset\'s own in use');
  assert.equal(presets.ownChordsText(wordsOf(nowBack), true), "the board's"); assert.equal(presets.ownChordsText(own), 'own set 4 POP');
  assert.equal(status(ownClear, tb.CMD.SET_RECORD), 0); assert.equal(status(ownGone, tb.CMD.GET_RECORD), 7);
  let ownRefusals = 0;
  const damagedOwn = Array.from({ length: 200 }, () => { const w = [...own], at = rnd2(3) ? rnd2(16) : rnd2(40); w[at] = (w[at] ^ (1 << rnd2(32))) >>> 0; return w; });
  firmware(damagedOwn.map((w) => tb.setRecord(tb.KIND.OWN_SET, 1 + rnd2(8), w))).forEach((r, i) => {
    const ok2 = tb.ownSetFromWords(damagedOwn[i]) !== null && !tb.ownSetFromWords(damagedOwn[i]).keep && !tb.ownSetFromWords(damagedOwn[i]).whose;
    assert.equal(status(r, tb.CMD.SET_RECORD) === 0, ok2, `own set ${damagedOwn[i].slice(0, 2)}`); ownRefusals += !ok2;
  });
  // The 2.0 settings: Turing (Arp 7), Lock and Loop, Sleep after, Chain, U3; then backups into 1.9.3 leave them out.
  const live2 = wordsOf(firmware([tb.getRecord(tb.KIND.LIVE, 0)])[0]);
  const set2 = [...live2]; set2[2] = ((set2[2] & ~0xff0000) | 7 << 16) >>> 0; set2[17] = (set2[17] | 0x80 | 2 << 8 | (3 | 9 << 4) << 16) >>> 0;
  set2[33] = (set2[33] | 0x200) >>> 0; set2[34] = (set2[34] | 1 << 24) >>> 0;
  const [put2, back2] = firmware([tb.setRecord(tb.KIND.LIVE, 0, set2), tb.getRecord(tb.KIND.LIVE, 0)]);
  assert.equal(status(put2, tb.CMD.SET_RECORD), 0); assert.deepEqual(wordsOf(back2), set2);
  const backup2 = [{ kind: tb.KIND.LIVE, slot: 0, words: set2 }, { kind: tb.KIND.PRESET, slot: 1, words: set2 },
    { kind: tb.KIND.CHAIN, slot: 0, words: tb.chainToWords([0, 0, 1]) }, { kind: tb.KIND.CHANCE, slot: 2, words: tb.chancesToWords(chanceSets[1]) },
    { kind: tb.KIND.OWN_SET, slot: 3, words: own }, { kind: tb.KIND.OWN_SET, slot: 0, words: tb.keepChordsWords(1) }];
  const fit193 = tb.recordsFor(backup2, '1.9.3');
  assert.deepEqual(fit193.skipped.map(tb.recordLabel), ['the chain', "progression P3's chances", "preset 3's own chord set", 'Keep chords']);
  for (const r of fit193.records) {
    assert.equal((r.words[2] >>> 16) & 0xff, 1, 'Turing back to Up'); assert.equal(r.words[17] >>> 8, 0, 'Sleep, Lock, Loop cleared');
    assert.equal(r.words[33] & 0x200, 0, 'Chain cleared'); assert.equal(r.words[34] >>> 24, 0, 'U3 cleared');
  }
  assert.deepEqual(tb.recordsFor(backup2, '2.0.0'), { records: backup2, skipped: [] });
  assert.ok(tb.v2Supported('2.0.0') && !tb.v2Supported('1.9.3') && tb.versionAtLeast('2.0.0', 1, 9, 3) && tb.contentSupported('2.0.0'));
  // ... and 1.9's own firmware takes what recordsFor makes of them (here the 2.0 firmware with its 1.9 view: the
  // live settings it accepts and the order they go in).
  const order2 = []; const fake2 = async (m) => { order2.push(m[5]); return { status: 0, data: [] }; };
  await tb.writeRecords(fake2, [...backup2].reverse(), '2.0.0');
  assert.deepEqual(order2, [tb.KIND.PRESET, tb.KIND.CHANCE, tb.KIND.CHAIN, tb.KIND.LIVE, tb.KIND.OWN_SET, tb.KIND.OWN_SET], 'chances and the chain after the progressions, own sets last');
  // Copy, paste and shift (2.0): a lane moved by the editor = by the firmware's ln_rotate; shifted, pasted and
  // range-pasted patterns are records the firmware takes and gives back unchanged.
  let rotateChecks = 0;
  if (padLights) {
    const cases = Array.from({ length: 200 }, () => ({ length: 1 + rnd2(64), by: rnd2(2) ? 1 : -1, steps: Array.from({ length: 64 }, () => ({ on: !!rnd2(2), value: rnd2(128) })) }));
    const out = padLights(cases.map((c) => `rotate ${c.length} ${c.by} ${c.steps.map((s) => s.value * 2 + (s.on ? 1 : 0)).join(' ')}`), true);
    cases.forEach((c, i) => {
      const l = lanes.emptyLane(); lanes.setLength(l, c.length); c.steps.forEach((s, k) => { l.steps[k] = { ...lanes.emptyStep(), ...s }; });
      lanes.rotate(l, c.by);
      assert.deepEqual(l.steps.map((s) => s.value * 2 + (s.on ? 1 : 0)), out[i].slice(0, 64), `rotate ${c.length} by ${c.by}`); rotateChecks++;
    });
  }
  // Mutate (2.0.3): the editor's = the firmware's ln_mutate, every kind, length and amount.
  let mutateChecks = 0;
  if (padLights) {
    const pack = (s) => s.value | s.velocity << 7 | (s.on ? 1 : 0) << 14;
    const cases = Array.from({ length: 300 }, () => ({ kind: rnd2(4), length: 1 + rnd2(64), lane: rnd2(4), seed: rnd2(65536), pct: 10 * (1 + rnd2(5)), salt: rnd2(1000),
      steps: Array.from({ length: 64 }, () => ({ on: !!rnd2(2), value: rnd2(2) ? rnd2(48) : rnd2(128), velocity: rnd2(3) ? 0 : rnd2(128) })) }));
    const out = padLights(cases.map((c) => `mutate ${c.kind} ${c.length} ${c.lane} ${c.seed} ${c.pct} ${c.salt} ${c.steps.map(pack).join(' ')}`), true);
    cases.forEach((c, i) => {
      const l = lanes.emptyLane(); l.kind = c.kind; lanes.setLength(l, c.length); c.steps.forEach((s, k) => { l.steps[k] = { ...lanes.emptyStep(), ...s }; });
      lanes.mutate(l, c.lane, c.seed, c.pct, c.salt);
      assert.deepEqual(l.steps.map(pack), out[i].slice(0, 64), `mutate kind ${c.kind}, ${c.length} steps, ${c.pct} %`); mutateChecks++;
    });
  }
  { // Randomize: on at about the density, values the kind's, only within the length
    const l = lanes.emptyLane(); l.kind = lanes.KIND.CHORDS; lanes.setLength(l, 32);
    let k = 0; const rnd = () => ((k = (k * 1103515245 + 12345) % 2147483648) / 2147483648);
    lanes.randomize(l, 50, rnd);
    const on = l.steps.filter((s) => s.on);
    assert.ok(on.length > 8 && on.length < 24 && on.every((s) => s.value % 4 === 3 && s.value < 28) && l.steps.slice(32).every((s) => !s.on));
    l.kind = lanes.KIND.DRUMS; l.steps = l.steps.map(lanes.emptyStep); lanes.randomize(l, 100, rnd); // no drums yet: kick, snare, hat
    assert.ok(l.steps.slice(0, 32).every((s) => s.on && [36, 38, 42].includes(s.value)));
    l.steps[0].value = 49; l.steps.forEach((s, i) => { if (i && i < 32) s.on = false; }); lanes.randomize(l, 100, rnd); // the lane's own
    assert.ok(l.steps.slice(0, 32).every((s) => s.on && s.value === 49));
  }
  // Nudge, swing and transpose names; nudge bits as the firmware stores them (two's complement).
  for (let v = lanes.NUDGE_MIN; v <= lanes.NUDGE_MAX; v++) assert.equal(lanes.nudgeOf(lanes.nudgeBits(v)), v);
  assert.equal(lanes.nudgeName(0), 'On time'); assert.equal(lanes.transposeName(lanes.KIND.CHORDS, 2), '+2 scale steps');
  const p2 = lanes.copyPattern(patterns[0]);
  lanes.pasteLane(p2, 2, p2.lanes[0]); assert.deepEqual(p2.lanes[2], p2.lanes[0]); assert.notEqual(p2.lanes[2].steps, p2.lanes[0].steps, 'a copy, not the same steps');
  lanes.rotate(p2.lanes[1], 1); p2.lanes[3].kind = p2.lanes[1].kind;
  const clip = lanes.copySteps(p2.lanes[1], 60, 50);
  assert.equal(clip.steps.length, 11); assert.equal(lanes.pasteSteps(p2.lanes[3], 58, clip), 6, 'cut at step 64');
  assert.deepEqual(p2.lanes[3].steps.slice(58), clip.steps.slice(0, 6));
  p2.lanes[0].kind = (p2.lanes[1].kind + 1) % 4; assert.equal(lanes.pasteSteps(p2.lanes[0], 0, clip), 0, 'not onto another kind');
  const pasteReplies = firmware([...Array.from({ length: lanes.RECORDS }, (_, s) => tb.setRecordQuiet(tb.KIND.LANE, s, lanes.encodeChunk(p2, s))),
    ...Array.from({ length: lanes.RECORDS }, (_, s) => tb.getRecord(tb.KIND.LANE, s))]);
  for (let s = 0; s < lanes.RECORDS; s++) {
    const r = pasteReplies[lanes.RECORDS + s];
    assert.deepEqual(status(r, tb.CMD.GET_RECORD) === 7 ? lanes.encodeChunk(lanes.emptyPattern(), s) : wordsOf(r), lanes.encodeChunk(p2, s), `pasted, slot ${s}`);
  }
  stage4 += `; 2.0: ${chains.length} chains and ${chanceSets.length} progressions' chances written and read back, ${damagedChains.length + damagedChances.length + damagedOwn.length} ` +
    `damaged records refused alike (${chainRefusals}, ${chanceRefusals}, ${ownRefusals}), presets' own chord sets, Keep chords, the TB now, the 2.0 settings, ` +
    `backups into 1.9.3 (left out, cleared), ${rotateChecks} lanes moved = the firmware's, lanes and steps pasted and taken by the board; ` +
    `2.0.3: ${mutateChecks} lanes mutated = the firmware's, randomize, nudge bits; 2.0.6: the editor's port first and its own input paired, with ` +
    'both Windows namings and a board with one port';
  console.log(`PASS: ${chordChecks} typed chords play the firmware notes their names say (every pad chord in every built-in scale and key, borrowed into major and minor keys, user scales as keys); ` +
    'web editor protocol: CRC/packing/names/scale masks, pad colors = firmware, settings backup read/restore order/failures/file checks, fitted to older firmware, chord names and progressions (parsing, placement with borrowed scales, every pad chord found again by name, lengths, limits, record format and checks), and against the firmware SysEx code: identify, set/get/validate/select user scales, live record, preview with and without a slot (no reply), version checks, NOR chunks with CRC and range error, absent preset, record restore round trip, favorites set/get/validate, a library scale selected, progressions set/get/validate/clear, chord sets set/get/validate/clear (not while in use), the center buttons and chord set through the live settings, step pattern records (absent, set/get, validated, 16 slots) and in backups, corrupted replies rejected; ' +
    `the board's menu (1.9.0): ${menuItems} items read with every value's name, each in the manual's table, SET_ITEM and a quiet SET_RECORD without replies; ` +
    `looks: the palette and built-in colors, ${lightChecks} looks x 96 pads drawn as the firmware lights them, the look on show written quietly and read back; ` +
    `the manual's pad explorer: ${padChecks} layouts x 96 pads with the firmware's notes and chord names; ` +
    `feel: ${feelChecks} curves x 128 velocity and pressure points as the firmware draws them, the record written quietly and read back, PADS, calibration proposals; ${stage4}`);
} else {
  console.log('PASS (pure functions only): web editor protocol');
}
