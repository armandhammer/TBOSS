// SPDX-License-Identifier: GPL-3.0-or-later - TheoryBoard SysEx protocol v1 (firmware: src/tb/sysex.h,
// description: docs/tb-sysex.md). Pure functions: no Web MIDI, usable in Node tests.
//
//   request  F0 7D 54 42 cmd data... sum F7
//   reply    F0 7D 54 42 cmd|40 status data... sum F7
import { SCALES } from './tb-scales.js';
export { SCALES, FAMILIES, LIBRARY_FIRST, USER_FIRST } from './tb-scales.js';

export const VERSION = 1;
export const CMD = Object.freeze({
  IDENTIFY: 1, GET_SCALE: 2, SET_SCALE: 3, PREVIEW: 4, SELECT_SCALE: 5,
  GET_RECORD: 6, SET_RECORD: 7, NOR_READ: 8,
});
export const STATUS = Object.freeze({
  0: 'OK', 1: 'checksum error', 2: 'wrong length', 3: 'value not accepted', 4: 'unknown command',
  5: 'storage error', 6: 'external memory unavailable or read failed', 7: 'nothing stored there',
});
// favorites 0.7.0+, progressions 0.11.0+, chord sets 1.0.0+
export const KIND = Object.freeze({ LIVE: 1, PRESET: 2, SCALE: 3, FAVORITES: 4, PROG: 5, CHORDS: 6 });
export const PROG_SLOTS = 8;
export const CHORD_SETS = 8;
export const FAVORITES = 128; // favorite scales the board keeps
export const PAYLOAD_WORDS = 40; // firmware 0.6.0+; earlier firmware has 20, which 0.6.0 still accepts
export const PAYLOAD_WORDS_V2 = 20;
const recordLengthOk = (words) => words.length === PAYLOAD_WORDS || words.length === PAYLOAD_WORDS_V2;
export const NAME_LENGTH = 12;
export const NOR_CHUNK = 2048;
export const BUILT_IN_SCALES = 12; // user scale slots are scale ids 12, 13, 14

const HEADER = [0xf0, 0x7d, 0x54, 0x42];

export function checksum(bytes) {
  const sum = bytes.reduce((a, b) => a + b, 0);
  return (128 - (sum % 128)) % 128;
}

export function request(cmd, data = []) {
  for (const b of data) if (!Number.isInteger(b) || b < 0 || b > 127) throw new Error(`data byte ${b} is not 7-bit`);
  return Uint8Array.from([...HEADER, cmd, ...data, checksum([cmd, ...data]), 0xf7]);
}

// A reply to `cmd`: { status, data } or null if the message is something else.
export function parseReply(message, cmd) {
  const m = Array.from(message);
  if (m.length < 8 || HEADER.some((b, i) => m[i] !== b) || m[m.length - 1] !== 0xf7) return null;
  if (m[4] !== (cmd | 0x40)) return null;
  const body = m.slice(4, m.length - 1);
  if (body.reduce((a, b) => a + b, 0) % 128 !== 0) throw new Error('reply checksum mismatch');
  return { status: m[5], data: m.slice(6, m.length - 2) };
}

// 7-bit packing: each group of up to 7 bytes -> a byte of their top bits, then the low 7 bits.
export function pack(bytes) {
  const out = [];
  for (let i = 0; i < bytes.length; i += 7) {
    const group = Array.from(bytes.slice(i, i + 7));
    out.push(group.reduce((top, b, j) => top | ((b >> 7) << j), 0));
    for (const b of group) out.push(b & 0x7f);
  }
  return out;
}

export function unpack(bytes) {
  const out = [];
  for (let i = 0; i < bytes.length; i += 8) {
    const top = bytes[i];
    for (let j = 0; j < 7 && i + 1 + j < bytes.length; j++) out.push(bytes[i + 1 + j] | (((top >> j) & 1) << 7));
  }
  return Uint8Array.from(out);
}

export function packedLength(n) { return n + Math.ceil(n / 7); }

export function crc32(bytes) {
  let crc = 0xffffffff;
  for (const b of bytes) {
    crc ^= b;
    for (let k = 0; k < 8; k++) crc = (crc >>> 1) ^ (0xedb88320 & -(crc & 1));
  }
  return (~crc) >>> 0;
}

// --- requests ---------------------------------------------------------------
export const identify = () => request(CMD.IDENTIFY);
export const getScale = (slot) => request(CMD.GET_SCALE, [slot]);
export function setScale(slot, mask, name) {
  return request(CMD.SET_SCALE, [slot, mask & 0x7f, (mask >> 7) & 0x1f, ...nameBytes(name)]);
}
// slot (0..2): the user slot being edited, shown on the TB's display (firmware 0.5.0+;
// older firmware ignores a 4-byte preview, so only send it to firmware that has it).
export const preview = (mask, root, slot) => request(CMD.PREVIEW,
  [mask & 0x7f, (mask >> 7) & 0x1f, root % 12, ...(Number.isInteger(slot) ? [slot] : [])]);
// True when the firmware's version (from IDENTIFY) is at least major.minor.
export function versionAtLeast(version, major, minor) {
  const m = /^(\d+)\.(\d+)\./.exec(version || '');
  return !!m && (+m[1] > major || (+m[1] === major && +m[2] >= minor));
}
export const selectScale = (id) => request(CMD.SELECT_SCALE, [id]);
export const getRecord = (kind, slot) => request(CMD.GET_RECORD, [kind, slot]);
export const getFavorites = () => getRecord(KIND.FAVORITES, 0);
export const setFavorites = (ids) => setRecord(KIND.FAVORITES, 0, favoritesToWords(ids));
export function setRecord(kind, slot, words) {
  if (!recordLengthOk(words)) throw new Error('a record has 20 or 40 words');
  return request(CMD.SET_RECORD, [kind, slot, ...pack(wordsToBytes(words))]);
}
export function norRead(address, length) {
  return request(CMD.NOR_READ, [address & 127, (address >> 7) & 127, (address >> 14) & 127, (address >> 21) & 127,
    length & 127, (length >> 7) & 127]);
}

// --- replies ----------------------------------------------------------------
export function decodeIdentify(data) {
  const info = unpack(data.slice(6, 14));
  return {
    protocol: data[0], stage: data[1], userScales: data[2], presets: data[3], payloadWords: data[4],
    norPresent: data[5] === 1, jedec: Array.from(info.slice(0, 3)),
    norBytes: (info[3] | (info[4] << 8) | (info[5] << 16) | (info[6] << 24)) >>> 0,
    // Firmware 0.3.0 and later append their version; older firmware reports none.
    version: data.length >= 17 ? `${data[14]}.${data[15]}.${data[16]}` : null,
  };
}
export function decodeScale(data) {
  const name = String.fromCharCode(...data.slice(3, 3 + NAME_LENGTH).filter((c) => c));
  return { slot: data[0], mask: data[1] | (data[2] << 7), name: name.trimEnd() };
}
export function decodeRecord(data) {
  return { kind: data[0], slot: data[1], words: bytesToWords(unpack(data.slice(2))) };
}
// NOR chunk: echoes the request (6 bytes), then packed data + CRC-32. Throws on corruption.
export function decodeNor(data, address, length) {
  const echo = Array.from(norRead(address, length).slice(5, 11));
  if (echo.some((b, i) => data[i] !== b)) throw new Error('reply is for another address');
  const bytes = unpack(data.slice(6));
  if (bytes.length !== length + 4) throw new Error(`expected ${length + 4} bytes, got ${bytes.length}`);
  const body = bytes.slice(0, length);
  const crc = (bytes[length] | (bytes[length + 1] << 8) | (bytes[length + 2] << 16) | (bytes[length + 3] << 24)) >>> 0;
  if (crc32(body) !== crc) throw new Error('chunk CRC mismatch');
  return body;
}

// --- helpers ------------------------------------------------------------------
// Names show on the TB's display: capitals, digits, a few symbols (src/tb/font.c).
export const NAME_CHARS = " !#%()*+,-./0123456789:<=>?ABCDEFGHIJKLMNOPQRSTUVWXYZ_";
export function cleanName(name) {
  return Array.from(String(name).toUpperCase()).map((c) => (NAME_CHARS.includes(c) ? c : ' '))
    .join('').slice(0, NAME_LENGTH).trimEnd();
}
function nameBytes(name) {
  const clean = cleanName(name);
  return Array.from({ length: NAME_LENGTH }, (_, i) => (i < clean.length ? clean.charCodeAt(i) : 0));
}
export function wordsToBytes(words) {
  const out = [];
  for (const w of words) out.push(w & 255, (w >>> 8) & 255, (w >>> 16) & 255, (w >>> 24) & 255);
  return out;
}
export function bytesToWords(bytes) {
  const words = [];
  for (let i = 0; i + 3 < bytes.length; i += 4) {
    words.push((bytes[i] | (bytes[i + 1] << 8) | (bytes[i + 2] << 16) | (bytes[i + 3] << 24)) >>> 0);
  }
  return words;
}

// --- scales -------------------------------------------------------------------
export const INTERVALS = ['1', 'b2', '2', 'b3', '3', '4', '#4', '5', 'b6', '6', 'b7', '7'];
export function maskOf(semitones) { return semitones.reduce((m, s) => m | (1 << (s % 12)), 0) | 1; }
export function semitonesOf(mask) { return INTERVALS.map((_, i) => i).filter((i) => (mask >> i) & 1); }
export function noteCount(mask) { return semitonesOf(mask).length; }
// Firmware rule (layout.c tc_user_mask_ok): root included and 5..12 notes.
export function maskOk(mask) { return (mask & 1) === 1 && mask < 4096 && noteCount(mask) >= 5; }
// Every built-in scale on the board (tb-scales.js, generated with the firmware's table),
// in the library's group order: to start from, recognize, browse and pick favorites.
export const KNOWN_SCALES = SCALES.map((s) => ({ id: s.id, name: s.name, short: s.short, group: s.group, mask: maskOf(s.steps) }));
export function scaleNamesFor(mask) { return KNOWN_SCALES.filter((s) => s.mask === mask).map((s) => s.name); }
// The scale's notes as pitch classes when played from `root` (bit n = pitch class n, C = 0).
export function rotateMask(mask, root) { const r = ((root % 12) + 12) % 12; return ((mask << r) | (mask >> (12 - r))) & 0xfff; }
// Brightness: the average height of the notes above the root (Lydian is brightest of the
// major modes, Locrian darkest). Comparable between scales with the same number of notes.
export function brightness(mask) { const s = semitonesOf(mask); return s.reduce((a, b) => a + b, 0) / s.length; }
// Every (scale, root) whose notes include all of `notes` (pitch-class bits), fewest notes first.
export function scalesContaining(notes, scales = KNOWN_SCALES) {
  const out = [];
  for (const scale of scales) for (let root = 0; root < 12; root++) {
    if ((rotateMask(scale.mask, root) & notes) === notes) out.push({ scale, root });
  }
  return out.sort((a, b) => noteCount(a.scale.mask) - noteCount(b.scale.mask) || a.scale.name.localeCompare(b.scale.name) ||
    a.root - b.root);
}
// Named scales one note away: one note other than the root moved a semitone up or down.
// from/to are semitones above the root (INTERVALS names them).
export function neighbors(mask, scales = KNOWN_SCALES) {
  const out = [];
  for (const from of semitonesOf(mask)) {
    if (!from) continue;
    for (const to of [from - 1, from + 1]) {
      if (to <= 0 || to >= 12 || (mask >> to) & 1) continue;
      const moved = (mask & ~(1 << from)) | (1 << to);
      for (const scale of scales) if (scale.mask === moved) out.push({ scale, from, to });
    }
  }
  return out;
}
// Favorites record: scale ids as bytes, in order, 0xFF after the last (settings.h).
export function favoritesToWords(ids) {
  if (ids.length > FAVORITES || new Set(ids).size !== ids.length || ids.some((id) => !Number.isInteger(id) || id < 0 || id > 126)) {
    throw new Error('favorites: up to 128 different scale ids');
  }
  return bytesToWords(Array.from({ length: PAYLOAD_WORDS * 4 }, (_, i) => (i >= FAVORITES ? 0 : i < ids.length ? ids[i] : 0xff)));
}
export function favoritesFromWords(words) {
  const ids = [];
  for (const b of wordsToBytes(words).slice(0, FAVORITES)) { if (b === 0xff) break; ids.push(b); }
  return ids;
}

// Progression record (src/tb/sequence.c): word 0 = 1 | unit << 8 | scale << 16 | count << 24;
// word 1 + i = step i: pad (6 bits) | units - 1 (5) << 6 | velocity / 2 (6) << 11 |
// edit (lower | sus / 2 << 3 | add << 5) << 17 | scale (7, 127 = the progression's own) << 25;
// word 39 = the chord set it plays (1.0.0; 0 = the scale's chords).
// { unit, scale, steps: [{ pad, units, velocity, scale, edit: { lower, sus, add } }], chords }
const PROG_STEPS = 38, PROG_UNITS = 64, PROG_RUN = 32, NO_SCALE = 127, ALL_SCALES = 98;
const scaleIdOk = (id) => Number.isInteger(id) && id >= 0 && id < ALL_SCALES;
export function progressionToWords(prog) {
  const words = new Array(PAYLOAD_WORDS).fill(0);
  if (!prog || ![0, 1, 2, 3].includes(prog.unit) || !scaleIdOk(prog.scale) || prog.steps.length > PROG_STEPS) {
    throw new Error('not a progression');
  }
  let total = 0;
  prog.steps.forEach((s, i) => {
    const e = s.edit;
    if (!(s.pad >= 0 && s.pad < 48) || !(s.units >= 1 && s.units <= PROG_RUN) || !(s.velocity >= 0 && s.velocity <= 127) ||
        !(s.scale === NO_SCALE || scaleIdOk(s.scale)) || !(e.lower >= 0 && e.lower < 8) || ![0, 2, 4].includes(e.sus) ||
        !(e.add >= 0 && e.add < 8)) throw new Error(`progression step ${i + 1} is not valid`);
    total += s.units;
    const edit = e.lower | (e.sus / 2) << 3 | e.add << 5;
    words[1 + i] = (s.pad | (s.units - 1) << 6 | (s.velocity >> 1) << 11 | edit << 17 | s.scale << 25) >>> 0;
  });
  if (total > PROG_UNITS) throw new Error(`a progression has up to ${PROG_UNITS} units`);
  const set = prog.chords || 0;
  if (!(Number.isInteger(set) && set >= 0 && set <= CHORD_SETS)) throw new Error('not a chord set');
  words[0] = (1 | prog.unit << 8 | prog.scale << 16 | prog.steps.length << 24) >>> 0;
  words[PAYLOAD_WORDS - 1] = set;
  return words;
}
// Velocity at half resolution, as sq_decode: even values exact, odd 1 less, ends 1 and 127.
const halfVelocity = (v) => (!v ? 1 : v === 63 ? 127 : v * 2);
// The firmware's checks (sq_decode); null for anything it would refuse.
export function progressionFromWords(words) {
  if (words.length !== PAYLOAD_WORDS) return null;
  const w0 = words[0], count = w0 >>> 24, unit = (w0 >>> 8) & 255, scale = (w0 >>> 16) & 255;
  if ((w0 & 255) !== 1 || unit > 3 || !scaleIdOk(scale) || count > PROG_STEPS || words[PAYLOAD_WORDS - 1] > CHORD_SETS) return null;
  const steps = []; let total = 0;
  for (let i = 0; i < count; i++) {
    const v = words[1 + i], pad = v & 63, units = ((v >>> 6) & 31) + 1, edit = (v >>> 17) & 255, own = v >>> 25;
    if (pad >= 48 || ((edit >> 3) & 3) === 3 || !(own === NO_SCALE || scaleIdOk(own))) return null;
    if ((total += units) > PROG_UNITS) return null;
    steps.push({ pad, units, velocity: halfVelocity((v >>> 11) & 63), scale: own,
      edit: { lower: edit & 7, sus: 2 * ((edit >> 3) & 3), add: edit >> 5 } });
  }
  if (words.slice(1 + count, PAYLOAD_WORDS - 1).some((x) => x)) return null;
  return { unit, scale, steps, chords: words[PAYLOAD_WORDS - 1] };
}

// Chord set record (1.0.0, src/tb/settings.h ST_CHORDS): word 0 = 1, word 1 + i = column i's
// chord: tones (bit n = n semitones above its root, root bit set, 1..6 notes) | root (semitones
// above the key's root, 0..23) << 12, 0 for none; words 13..15 the name; the rest 0.
// { name, columns: [{ root, set } or null] (12) }; the root buttons transpose it on the board.
export const SET_COLUMNS = 12, SET_TONES = 6, SET_ROOTS = 24;
const toneCount = (set) => { let n = 0; for (let m = set; m; m &= m - 1) n++; return n; };
export function setChordOk(c) {
  return c === null || (Number.isInteger(c.set) && c.set > 0 && c.set < 4096 && (c.set & 1) === 1 &&
    toneCount(c.set) <= SET_TONES && Number.isInteger(c.root) && c.root >= 0 && c.root < SET_ROOTS);
}
export function chordSetToWords(cs) {
  if (!cs || cs.columns.length !== SET_COLUMNS || !cs.columns.every(setChordOk)) throw new Error('not a chord set');
  const words = new Array(PAYLOAD_WORDS).fill(0);
  words[0] = 1;
  cs.columns.forEach((c, i) => { if (c) words[1 + i] = (c.set | c.root << 12) >>> 0; });
  bytesToWords(nameBytes(cs.name || '')).forEach((w, i) => { words[13 + i] = w; });
  return words;
}
// The firmware's checks (settings.c set_ok); null for anything it would refuse.
export function chordSetFromWords(words) {
  if (words.length !== PAYLOAD_WORDS || words[0] !== 1 || words.slice(16).some((w) => w)) return null;
  const columns = [];
  for (let i = 0; i < SET_COLUMNS; i++) {
    const w = words[1 + i], c = w ? { set: w & 0xfff, root: (w >>> 12) & 31 } : null;
    if (w >>> 17 || !setChordOk(c)) return null;
    columns.push(c);
  }
  const name = String.fromCharCode(...wordsToBytes(words.slice(13, 16)).filter((c) => c)).trimEnd();
  return { name, columns };
}
export const setChordSet = (slot, cs) => setRecord(KIND.CHORDS, slot, chordSetToWords(cs));
// A set without chords clears the slot; the board refuses that while it plays the set.
export const clearChordSet = (slot) => setRecord(KIND.CHORDS, slot, [1, ...new Array(PAYLOAD_WORDS - 1).fill(0)]);

// The center buttons (1.0.0): what each of the 14 does, kept in the live settings, words
// 35..38 (a byte per button: 0 = its printed panel function, else action + 1). The
// firmware's order (src/tb/controls.h TC_ACT_), named as in the manual.
export const BUTTONS = ['Voicing I', 'Voicing II', 'Voicing III', 'Guitar', 'Spread I', 'Spread II', 'Spread III',
  'Octave −3', 'Octave −2', 'Octave −1', 'Octave 0', 'Octave +1', 'Octave +2', 'Octave +3'];
export const ACTIONS = [...BUTTONS, 'Shift', 'Hold on/off', 'Arpeggiator on/off', 'Strum on/off', 'Voice leading on/off',
  'Fold on/off', 'Hints on/off', 'Sustain on/off', 'All notes off', 'Tap tempo', 'Next preset', 'Previous preset',
  'Next scale', 'Previous scale', 'Melody octave up', 'Melody octave down', 'Play/stop', 'Record/finish',
  'Add 7th (while held)', 'Add 9th (while held)', 'Add 6th (while held)', 'Sus2 (while held)', 'Sus4 (while held)',
  'Next chord set', 'Nothing'];
export function buttonsFromWords(words) {
  return BUTTONS.map((_, i) => { const b = (words[35 + (i >> 2)] >>> (8 * (i & 3))) & 255; return b ? b - 1 : i; });
}
// The live settings' words with these actions, every other setting as it was.
export function withButtons(words, actions) {
  if (actions.length !== BUTTONS.length || actions.some((a) => !Number.isInteger(a) || a < 0 || a >= ACTIONS.length)) {
    throw new Error('not a button map');
  }
  const out = [...words];
  for (let w = 35; w < 39; w++) out[w] = 0;
  actions.forEach((a, i) => { if (a !== i) out[35 + (i >> 2)] = (out[35 + (i >> 2)] | (a + 1) << (8 * (i & 3))) >>> 0; });
  return out;
}
// The live settings' words with chord set n selected (0 = the scale's chords).
export function withChordSet(words, set) {
  const out = [...words];
  out[34] = ((out[34] & ~0xff) | set) >>> 0;
  return out;
}
export const setProgression = (slot, prog) => setRecord(KIND.PROG, slot, progressionToWords(prog));

// --- settings backup (web editor and installer) ---------------------------------
// call(message) -> Promise<{ status, data }>, one request at a time (tb-midi.js).
export const BACKUP_FORMAT = 'theoryboard-settings';
// Also the restore order: scales and chord sets before the settings and presets that use them.
const ORDER = [KIND.SCALE, KIND.CHORDS, KIND.FAVORITES, KIND.PRESET, KIND.PROG, KIND.LIVE];
export function recordLabel(r) {
  return r.kind === KIND.LIVE ? 'settings' : r.kind === KIND.PRESET ? `preset ${r.slot + 1}` :
    r.kind === KIND.FAVORITES ? 'favorite scales' : r.kind === KIND.PROG ? `progression P${r.slot + 1}` :
    r.kind === KIND.CHORDS ? `chord set ${r.slot + 1}` : `scale U${r.slot + 1}`;
}
// Every stored record: user scales, favorites, presets, progressions, chord sets, then the live
// settings. userScales comes from IDENTIFY (12 on firmware 0.6.0+, 3 before); favorites exist
// from 0.7.0, progressions from 0.11.0, chord sets from 1.0.0.
export async function readRecords(call, userScales = 3, favorites = false, progressions = false, chordSets = false) {
  const records = [];
  for (const [kind, slots] of [[KIND.SCALE, userScales], [KIND.FAVORITES, favorites ? 1 : 0], [KIND.PRESET, 8],
    [KIND.PROG, progressions ? PROG_SLOTS : 0], [KIND.CHORDS, chordSets ? CHORD_SETS : 0], [KIND.LIVE, 1]]) {
    for (let slot = 0; slot < slots; slot++) {
      const reply = await call(getRecord(kind, slot));
      if (reply.status === 0) records.push(decodeRecord(reply.data));
      else if (reply.status !== 7) throw new Error(STATUS[reply.status] || `status ${reply.status}`);
    }
  }
  return records;
}
// Records for older firmware (a backup put back after going back to an earlier TBOSS):
// settings it doesn't have are left at their defaults (0) and kinds it doesn't have are
// left out, instead of the firmware refusing whole records. version: from IDENTIFY.
// 1.0.0 added chord sets and, in settings and presets, the chord set, hints and scale lock
// (word 34) and the center buttons (words 35..38), and a progression's chord set (word 39);
// 0.11.0 progressions and, in settings and presets, the pedal's target (word 28
// byte 2), the melody half's velocity (word 31 byte 3) and the progression slot (word 33);
// 0.10.0 the pad thresholds (word 32).
export function recordsFor(records, version) {
  if (!version) return { records, skipped: [] };
  const skipped = [], out = [];
  for (const r of records) {
    if (r.kind === KIND.PROG && !versionAtLeast(version, 0, 11)) { skipped.push(r); continue; }
    if (r.kind === KIND.CHORDS && !versionAtLeast(version, 1, 0)) { skipped.push(r); continue; }
    if (r.kind === KIND.PROG && !versionAtLeast(version, 1, 0)) {
      out.push({ ...r, words: r.words.map((w, i) => (i === PAYLOAD_WORDS - 1 ? 0 : w)) }); continue;
    }
    if ((r.kind === KIND.LIVE || r.kind === KIND.PRESET) && r.words.length === PAYLOAD_WORDS) {
      const words = [...r.words];
      if (!versionAtLeast(version, 1, 0)) for (let i = 34; i < 39; i++) words[i] = 0;
      if (!versionAtLeast(version, 0, 11)) { words[28] &= 0xffff; words[31] &= 0xffffff; words[33] = 0; }
      if (!versionAtLeast(version, 0, 10)) words[32] = 0;
      out.push({ ...r, words });
    } else out.push(r);
  }
  return { records: out, skipped };
}
// Scales first (presets and settings may use them), live settings last. The firmware
// checks every record; one it refuses is reported and the rest still go in. With the
// firmware's version, records are first made to fit it (recordsFor; skipped: left out).
export async function writeRecords(call, records, version = null) {
  const fit = recordsFor(records, version);
  const order = Object.fromEntries(ORDER.map((kind, i) => [kind, i]));
  const sorted = [...fit.records].sort((a, b) => order[a.kind] - order[b.kind]);
  let done = 0; const failed = [];
  for (const r of sorted) {
    const reply = await call(setRecord(r.kind, r.slot, r.words));
    if (reply.status === 0) done++; else failed.push(`${recordLabel(r)}: ${STATUS[reply.status] || reply.status}`);
  }
  return { done, total: sorted.length, failed, skipped: fit.skipped.map(recordLabel) };
}
export function makeBackup(records, firmware = null) {
  return { format: BACKUP_FORMAT, version: 1, created: new Date().toISOString(), firmware, records };
}
export function parseBackup(text) {
  const backup = JSON.parse(text);
  if (backup.format !== BACKUP_FORMAT || backup.version !== 1 || !Array.isArray(backup.records)) {
    throw new Error('not a TheoryBoard settings backup');
  }
  for (const r of backup.records) {
    if (!Object.values(KIND).includes(r.kind) || !Number.isInteger(r.slot) ||
        !Array.isArray(r.words) || !recordLengthOk(r.words)) throw new Error('damaged settings backup');
  }
  return backup.records;
}

// --- pad colors ------------------------------------------------------------------
// The board's rainbow, one color per pitch class from C (src/tb/ui.c `hues`; the
// protocol test checks they match). In the editor view the root is white.
export const PAD_HUES = Object.freeze([[255, 0, 0], [255, 90, 0], [255, 190, 0], [160, 255, 0], [0, 255, 0],
  [0, 255, 130], [0, 220, 255], [0, 110, 255], [0, 0, 255], [60, 0, 255], [170, 0, 255], [255, 0, 100]]);
