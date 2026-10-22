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
{ // the editor's key colors are the board's pad colors (src/tb/ui.c)
  const ui = fs.readFileSync(path.join(root, 'src/tb/ui.c'), 'utf8');
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
  assert.throws(() => tb.parseBackup(JSON.stringify({ ...backup, records: [{ kind: 9, slot: 0, words: words(1) }] })), /damaged/);
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
  // Into older firmware (going back to an earlier TBOSS): what it doesn't have is left at its
  // default or left out, so it doesn't refuse whole records.
  const newer = { kind: tb.KIND.LIVE, slot: 0, words: words(0).map((w, i) => (i === 28 ? 0x00010203 : i === 31 ? 0x83000101 : i === 32 ? 0x00230612 : i === 33 ? 0x0103 : w)) };
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
    const acts = header.match(/enum \{ TC_ACT_SHIFT = TC_FUNCTIONS,([^}]*)\}/)[1].split(',').map((x) => x.trim()).filter(Boolean);
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
function compile() {
  const sources = ['src/tb/controls.c', 'src/tb/layout.c', 'src/tb/engine.c', 'src/tb/input.c', 'src/tb/scan.c',
    'src/tb/midi_in.c', 'src/tb/sequence.c', 'src/tb/settings.c', 'src/tb/sysex.c', 'tests/webapp/harness.c'];
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

// --- the editor's chords against the notes the firmware plays (tests/webapp/chords.c) -------
function compileChords() {
  const sources = ['src/tb/controls.c', 'src/tb/layout.c', 'src/tb/engine.c', 'src/tb/input.c', 'src/tb/scan.c',
    'src/tb/midi_in.c', 'src/tb/sequence.c', 'tests/webapp/chords.c'];
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
  console.log(`PASS: ${chordChecks} typed chords play the firmware notes their names say (every pad chord in every built-in scale and key, borrowed into major and minor keys, user scales as keys); ` +
    'web editor protocol: CRC/packing/names/scale masks, pad colors = firmware, settings backup read/restore order/failures/file checks, fitted to older firmware, chord names and progressions (parsing, placement with borrowed scales, every pad chord found again by name, lengths, limits, record format and checks), and against the firmware SysEx code: identify, set/get/validate/select user scales, live record, preview with and without a slot (no reply), version checks, NOR chunks with CRC and range error, absent preset, record restore round trip, favorites set/get/validate, a library scale selected, progressions set/get/validate/clear, chord sets set/get/validate/clear (not while in use), the center buttons and chord set through the live settings, corrupted replies rejected');
} else {
  console.log('PASS (pure functions only): web editor protocol');
}
