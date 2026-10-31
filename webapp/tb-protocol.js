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
  MENU: 9, ITEM: 10, SET_ITEM: 11, // 1.9.0: the board's menu, and setting an item without a reply
  PADS: 12, // 1.9.0: every pad's resting level and reading now
  VALUES: 13, SET_EDITS: 14, CHORD_NAMES: 15, // 1.9.3: a preset's values, the chord pads' edits and names
  STATE: 16, // 1.9.4: what changed on the board (the editor asks once a second)
});
export const STATUS = Object.freeze({
  0: 'OK', 1: 'checksum error', 2: 'wrong length', 3: 'value not accepted', 4: 'unknown command',
  5: 'storage error', 6: 'external memory unavailable or read failed', 7: 'nothing stored there',
});
// favorites 0.7.0+, progressions 0.11.0+, chord sets 1.0.0+, step patterns 1.5.0+, looks 1.9.0+ (tb-lights.js),
// preset names 1.9.3+ (tb-presets.js), presets' own chord sets, the chain and each progression's chances 2.0.0+
export const KIND = Object.freeze({ LIVE: 1, PRESET: 2, SCALE: 3, FAVORITES: 4, PROG: 5, CHORDS: 6, LANE: 7, LOOK: 8, FEEL: 9,
  NAMES: 10, OWN_SET: 11, CHAIN: 12, CHANCE: 13 });
export const LOOKS = 9; // the board's look (slot 0) and presets 1..8's own (slots 1..8)
export const PROG_SLOTS = 8;
export const CHORD_SETS = 8;
// The chords value (settings word 34 byte 0, a progression's word 39) for the Full chord
// layout and the chord grid (1.3.0): 0 = the scale's chords, 1..8 = chord set n.
export const FULL_CHORDS = CHORD_SETS + 1;
export const GRID_CHORDS = FULL_CHORDS + 1; // tb-chords.js GRID_CHORDS
const chordsOk = (n) => Number.isInteger(n) && n >= 0 && n <= GRID_CHORDS;
const since13 = (n) => n === FULL_CHORDS || n === GRID_CHORDS; // chords values firmware before 1.3 refuses
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
// True when the firmware's version (from IDENTIFY) is at least major.minor (1.9.3: or major.minor.patch).
export function versionAtLeast(version, major, minor, patch = 0) {
  const m = /^(\d+)\.(\d+)\.(\d+)/.exec(version || '');
  return !!m && (+m[1] > major || (+m[1] === major && (+m[2] > minor || (+m[2] === minor && +m[3] >= patch))));
}
export const selectScale = (id) => request(CMD.SELECT_SCALE, [id]);
export const getRecord = (kind, slot) => request(CMD.GET_RECORD, [kind, slot]);
export const getFavorites = () => getRecord(KIND.FAVORITES, 0);
export const setFavorites = (ids) => setRecord(KIND.FAVORITES, 0, favoritesToWords(ids));
export function setRecord(kind, slot, words) {
  if (!recordLengthOk(words)) throw new Error('a record has 20 or 40 words');
  return request(CMD.SET_RECORD, [kind, slot, ...pack(wordsToBytes(words))]);
}
// 1.9.0: the same without a reply, so the board keeps playing (send it with link.send; a GET_RECORD
// later shows whether it was taken). 40 words only.
export function setRecordQuiet(kind, slot, words) {
  if (words.length !== PAYLOAD_WORDS) throw new Error('a quiet record has 40 words');
  return request(CMD.SET_RECORD, [kind, slot, SET_QUIET, ...pack(wordsToBytes(words))]);
}
const SET_QUIET = 1;
// --- the board's menu (1.9.0, docs/tb-sysex.md) ----------------------------------------
// MENU page: its items; ITEM: one item, its value and value names from `first` on (as many as fit,
// ask again from first + labels.length for the rest); SET_ITEM: no reply, playing goes on.
export const settingsSupported = (version) => versionAtLeast(version, 1, 9);
export const ITEM_KIND = Object.freeze({ VALUE: 0, TOGGLE: 1, CHOOSE: 2, BOARD: 3 });
const two = (n) => [n & 127, (n >> 7) & 127];
const num = (d, i) => d[i] | (d[i + 1] << 7);
export const menu = (page) => request(CMD.MENU, [page]);
export const item = (id, first = 0) => request(CMD.ITEM, [...two(id), ...two(first)]);
export const setItem = (id, value) => request(CMD.SET_ITEM, [...two(id), ...two(value)]);
// PADS (1.9.0): { rested, scanning, trigger, release, full, velFull (the settings' values), pads: [{ rest,
// value }] by pad number (0..47 the chord pads, 4 per column from the top) }.
export const pads = () => request(CMD.PADS);
export function decodePads(data) {
  const n = data[0], list = [];
  for (let k = 0; k < n; k++) list.push({ rest: num(data, 6 + 4 * k), value: num(data, 8 + 4 * k) });
  return { rested: !!(data[1] & 1), scanning: !!(data[1] & 2), trigger: data[2], release: data[3], full: data[4], velFull: data[5], pads: list };
}
const text = (bytes) => String.fromCharCode(...bytes);
export function decodeMenu(data) {
  const count = data[2], items = [];
  for (let i = 0; i < count; i++) items.push(num(data, 3 + 2 * i));
  return { page: data[0], pages: data[1], items, name: text(data.slice(3 + 2 * count)) };
}
export function decodeItem(data) {
  const strings = [];
  let at = 11;
  for (let n = 0; n <= data[10]; n++) {
    const end = data.indexOf(0, at);
    if (end < 0) throw new Error('item reply cut short');
    strings.push(text(data.slice(at, end))); at = end + 1;
  }
  return { id: num(data, 0), kind: data[2], shown: !!(data[3] & 1), count: num(data, 4), value: num(data, 6),
    first: num(data, 8), name: strings[0], labels: strings.slice(1) };
}
// Every page with every item and all its value names: one burst (playing pauses once).
export async function readMenu(ok) {
  const first = decodeMenu(await ok(menu(0))), pages = [];
  for (let p = 0; p < first.pages; p++) {
    const page = p ? decodeMenu(await ok(menu(p))) : first, items = [];
    for (const id of page.items) {
      const it = decodeItem(await ok(item(id)));
      while (it.labels.length < it.count) {
        const more = decodeItem(await ok(item(id, it.labels.length)));
        if (!more.labels.length) break;
        it.labels.push(...more.labels);
      }
      items.push(it);
    }
    pages.push({ page: p, name: page.name, items });
  }
  return pages;
}
// --- the content editors (1.9.3, docs/tb-sysex.md) ----------------------------------------
// The Sequencer, Presets and chord pad edits need TBOSS 1.9.3; the curves in use are feel slot 9 from then on.
export const contentSupported = (version) => versionAtLeast(version, 1, 9, 3);
export const FEEL_NOW = 9;
// VALUES: every menu item's value (as ITEM gives it) in preset `slot` (kind PRESET), or the live settings
// (kind LIVE, slot 0): { kind, slot, values: [by item id] }.
export const values = (kind, slot) => request(CMD.VALUES, [kind, slot]);
// STATE (1.9.4): a check of every menu value and where each item shows, the step pattern's and progressions'
// change counts, the saved changes' count, and which items the board's menu shows now (by item id). Asked once a
// second; the board answers between two notes, without pausing.
export const stateSupported = (version) => versionAtLeast(version, 1, 9, 4);
export const boardState = () => request(CMD.STATE, []);
export function decodeState(data) {
  const five = (i) => (data[i] | data[i + 1] << 7 | data[i + 2] << 14 | data[i + 3] << 21 | data[i + 4] << 28) >>> 0;
  const count = num(data, 20), shown = [];
  if (data.length < 22 + Math.ceil(count / 7)) throw new Error('short STATE reply');
  for (let i = 0; i < count; i++) shown.push(!!((data[22 + Math.floor(i / 7)] >> (i % 7)) & 1));
  return { check: five(0), lanes: five(5), progressions: five(10), changes: five(15), shown };
}
export function decodeValues(data) {
  const count = num(data, 2), list = [];
  for (let i = 0; i < count; i++) list.push(num(data, 4 + 2 * i));
  return { kind: data[0], slot: data[1], values: list };
}
// The 48 chord pads' edits (settings word 5.., src/tb/settings.c's chord byte: lower | sus / 2 << 3 | add << 5),
// strum off (words 21, 22) and sound off (words 29, 30) as the live record has them:
// { edits: [{ lower, sus, add }], strumOff: [bool], soundOff: [bool] }, by chord pad 0..47 (4 per column from the top).
export const CHORD_PADS = 48;
const editOf = (b) => ({ lower: b & 7, sus: 2 * ((b >> 3) & 3), add: b >> 5 });
export const editByte = (e) => (e.lower & 7) | ((e.sus >> 1) & 3) << 3 | (e.add & 7) << 5;
const bit48 = (lo, hi, k) => (k < 32 ? (lo >>> k) & 1 : (hi >>> (k - 32)) & 1) === 1;
export function padEditsFromWords(words) {
  const edits = [], strumOff = [], soundOff = [];
  for (let k = 0; k < CHORD_PADS; k++) {
    edits.push(editOf((words[5 + (k >> 2)] >>> (8 * (k & 3))) & 255));
    strumOff.push(bit48(words[21], words[22], k)); soundOff.push(bit48(words[29], words[30], k));
  }
  return { edits, strumOff, soundOff };
}
// SET_EDITS (no reply: the board keeps playing; it takes all or none): the pads' edits, strum off, sound off.
export function setEdits({ edits, strumOff, soundOff }) {
  const bytes = edits.map(editByte);
  for (const mask of [strumOff, soundOff])
    for (let b = 0; b < 6; b++) bytes.push(mask.slice(8 * b, 8 * b + 8).reduce((v, on, i) => v | (on ? 1 << i : 0), 0));
  for (const e of edits) if (e.sus !== 0 && e.sus !== 2 && e.sus !== 4) throw new Error('sus is 0, 2 or 4');
  return request(CMD.SET_EDITS, pack(bytes));
}
// CHORD_NAMES: each chord pad's chord with its edits, as the TB's screen names it.
export const chordNames = () => request(CMD.CHORD_NAMES);
export function decodeChordNames(data) {
  const names = []; let at = 1;
  for (let k = 0; k < data[0]; k++) {
    const end = data.indexOf(0, at);
    if (end < 0) throw new Error('names cut short');
    names.push(text(data.slice(at, end))); at = end + 1;
  }
  return names;
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
  if (!chordsOk(set)) throw new Error('not a chord set');
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
  if ((w0 & 255) !== 1 || unit > 3 || !scaleIdOk(scale) || count > PROG_STEPS || !chordsOk(words[PAYLOAD_WORDS - 1])) return null;
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
  const bytes = wordsToBytes(words.slice(13, 16));
  if (bytes.some((c) => c && (c < 32 || c > 126))) return null;
  const name = String.fromCharCode(...bytes.filter((c) => c)).trimEnd();
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
  'Next chord set', 'Nothing', 'Pivot (hold + root button)', 'Fill (while held)', 'Stutter 1/4 (while held)',
  'Stutter 1/8 (while held)', 'Stutter 1/16 (while held)'];
// The actions a firmware version knows: Pivot came in 1.4.0 (after Nothing, so the stored
// numbers of the others stay), Fill and Stutter in 2.0.3.
export const actionsFor = (version) => (versionAtLeast(version, 2, 0, 3) ? ACTIONS.length :
  versionAtLeast(version, 1, 4) ? ACTIONS.indexOf('Fill (while held)') : ACTIONS.indexOf('Pivot (hold + root button)'));
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
// The live settings' words with chord set n selected (0 = the scale's chords, FULL_CHORDS the Full
// layout, GRID_CHORDS the chord grid).
export function withChordSet(words, set) {
  const out = [...words];
  out[34] = ((out[34] & ~0xff) | set) >>> 0;
  return out;
}
export const setProgression = (slot, prog) => setRecord(KIND.PROG, slot, progressionToWords(prog));

// --- TBOSS 2.0 (docs/tb-sysex.md): the chain, each progression chord's chance, presets' own chord sets ---------
export const v2Supported = (version) => versionAtLeast(version, 2, 0);
// The chain (kind 12, slot 0; src/tb/sequence.c sq_chain_encode): up to 8 progression slots (0..7) played in turn.
// word 0 = 1 | count << 8, word 1 = link i's slot in bits 4i..4i+2; no links: all zero (the board then has none).
export const CHAIN_LINKS = 8;
export function chainToWords(links) {
  if (!Array.isArray(links) || links.length > CHAIN_LINKS || links.some((s) => !Number.isInteger(s) || s < 0 || s >= PROG_SLOTS)) {
    throw new Error(`a chain is up to ${CHAIN_LINKS} progression slots`);
  }
  const w = new Array(PAYLOAD_WORDS).fill(0);
  if (!links.length) return w;
  w[0] = 1 | links.length << 8;
  links.forEach((s, i) => { w[1] = (w[1] | s << (4 * i)) >>> 0; });
  return w;
}
// The firmware's checks (sq_chain_decode); null for anything it would refuse.
export function chainFromWords(w) {
  if (w.length !== PAYLOAD_WORDS || w.slice(2).some((x) => x)) return null;
  if (!w[0]) return w[1] ? null : [];
  const count = (w[0] >>> 8) & 255, links = [];
  if ((w[0] & 255) !== 1 || w[0] >>> 16 || !count || count > CHAIN_LINKS) return null;
  for (let i = 0; i < CHAIN_LINKS; i++) {
    const s = (w[1] >>> (4 * i)) & 15;
    if (i < count ? s >= PROG_SLOTS : s) return null;
    if (i < count) links.push(s);
  }
  return links;
}
export const setChain = (links) => setRecord(KIND.CHAIN, 0, chainToWords(links));
// A progression's chances (kind 13, slot = its slot; sq_chances_encode): a byte per chord (step i: byte i from word 1):
// the chance it plays as recorded, as the arpeggiator's Chance (0 = 100 % .. 9 = 10 %), + 16 when otherwise the borrowed
// chord plays (the same chord pad in the parallel major or minor key) instead of a rest. All 100 %: all zero.
export const CHANCE_PCT = Object.freeze([100, 90, 80, 70, 60, 50, 40, 30, 20, 10]);
const BORROW = 16, CHANCE_STEPS = 38;
export function chancesToWords(chances) { // [{ chance, borrow }] by chord
  if (chances.length > CHANCE_STEPS) throw new Error(`a progression has up to ${CHANCE_STEPS} chords`);
  const w = new Array(PAYLOAD_WORDS).fill(0);
  chances.forEach((c, i) => {
    if (!Number.isInteger(c.chance) || c.chance < 0 || c.chance >= CHANCE_PCT.length) throw new Error(`chord ${i + 1}: not a chance`);
    const b = c.chance | (c.borrow ? BORROW : 0);
    w[1 + (i >> 2)] = (w[1 + (i >> 2)] | b << (8 * (i & 3))) >>> 0;
  });
  if (w.some((x) => x)) w[0] = 1;
  return w;
}
export function chancesFromWords(w, count = CHANCE_STEPS) { // null: the firmware would refuse it
  if (w.length !== PAYLOAD_WORDS || w[0] > 1 || w.slice(11).some((x) => x) || (w[10] >>> 16)) return null;
  const out = [];
  for (let i = 0; i < CHANCE_STEPS; i++) {
    const b = (w[1 + (i >> 2)] >>> (8 * (i & 3))) & 255;
    if ((b & 15) >= CHANCE_PCT.length || (b & ~(15 | BORROW)) || (b && !w[0])) return null;
    if (i < count) out.push({ chance: b & 15, borrow: !!(b & BORROW) });
  }
  return out;
}
export const plainChances = (n) => Array.from({ length: n }, () => ({ chance: 0, borrow: false }));
// Presets' own chord sets (kind 11, Keep chords; src/tb/settings.h ST_OWN_SET): slots 1..8 preset n's: word 0 = 1 | the
// chord set slot it is a copy of (0..7) << 8, words 1..15 the chord set record's; slot 0: word 0 = 1 | Keep chords << 16;
// slot 9 (OWN_SET_NOW) the set a preset's own covers as it plays (| Keep chords << 16), word 39 = whose (0: none).
export const OWN_SET_NOW = 9;
export function ownSetToWords(set, cs, keep = 0) {
  if (!Number.isInteger(set) || set < 0 || set >= CHORD_SETS) throw new Error('not a chord set slot');
  const w = chordSetToWords(cs);
  w[0] = (1 | set << 8 | (keep ? 1 : 0) << 16) >>> 0;
  return w;
}
// { set, chords, keep, whose } or null (cleared, or one the firmware would refuse).
export function ownSetFromWords(w) {
  if (w.length !== PAYLOAD_WORDS || (w[0] & 255) !== 1 || ((w[0] >>> 8) & 255) >= CHORD_SETS || (w[0] >>> 24)) return null;
  const cs = chordSetFromWords([1, ...w.slice(1, PAYLOAD_WORDS - 1), 0]);
  return cs && { set: (w[0] >>> 8) & 255, chords: cs, keep: (w[0] >>> 16) & 1, whose: w[PAYLOAD_WORDS - 1] };
}
export const keepChordsWords = (keep) => [(1 | (keep ? 1 : 0) << 16) >>> 0, ...new Array(PAYLOAD_WORDS - 1).fill(0)];

// Step patterns (1.5.0, src/tb/lanes.h): kind 7, slot = lane * 4 + chunk (pattern 1 of 1 for now),
// 16 steps of 64 bits from word 1 (low word, then high: bit 0 on, 1-7 value, 8-14 velocity, 15-22
// edits, 23-29 scale + 1, the rest for later); chunk 0 also has the lane's settings (word 33: kind |
// channel << 8 | length << 16 | rate << 24, word 34: mute << 16), lane 1 chunk 0 the pattern's seed
// (word 36). A chunk of nothing but defaults is absent. Kinds: 0 chords, 1 melody, 2 drums, 3 off.
export const LANE_RECORDS = 16;
export const LANE_KINDS = Object.freeze(['chords', 'melody', 'drums', 'off']);
export function laneChunkToWords({ kind = 0, channel = 0, length = 0, rate = 0, mute = 0, steps = [] } = {}, chunk = 0) {
  const w = new Array(PAYLOAD_WORDS).fill(0);
  w[0] = 1;
  steps.slice(0, 16).forEach((s, i) => {
    if (!s) return;
    w[1 + 2 * i] = ((s.on ? 1 : 0) | (s.value & 127) << 1 | (s.velocity & 127) << 8 | (s.edit & 255) << 15 | (s.scale & 127) << 23) >>> 0;
  });
  if (chunk === 0) { w[33] = (kind | channel << 8 | length << 16 | rate << 24) >>> 0; w[34] = (mute ? 1 : 0) << 16; }
  return w;
}

// --- settings backup (web editor and installer) ---------------------------------
// call(message) -> Promise<{ status, data }>, one request at a time (tb-midi.js).
export const BACKUP_FORMAT = 'theoryboard-settings';
// Also the restore order: scales and chord sets before the settings and presets that use them.
const ORDER = [KIND.SCALE, KIND.CHORDS, KIND.FAVORITES, KIND.PRESET, KIND.PROG, KIND.CHANCE, KIND.CHAIN, KIND.LANE, KIND.LIVE, KIND.LOOK,
  KIND.FEEL, KIND.NAMES, KIND.OWN_SET];
export function recordLabel(r) {
  return r.kind === KIND.LIVE ? 'settings' : r.kind === KIND.PRESET ? `preset ${r.slot + 1}` :
    r.kind === KIND.FAVORITES ? 'favorite scales' : r.kind === KIND.PROG ? `progression P${r.slot + 1}` :
    r.kind === KIND.CHORDS ? `chord set ${r.slot + 1}` :
    r.kind === KIND.LANE ? `pattern lane L${(r.slot >> 2) % 4 + 1}, steps ${(r.slot % 4) * 16 + 1}-${(r.slot % 4) * 16 + 16}` :
    r.kind === KIND.LOOK ? (r.slot ? `preset ${r.slot}'s colors` : 'the colors') :
    r.kind === KIND.FEEL ? (r.slot ? `preset ${r.slot}'s velocity and pressure curves` : 'the velocity and pressure curves') :
    r.kind === KIND.NAMES ? 'the preset names' :
    r.kind === KIND.OWN_SET ? (r.slot ? `preset ${r.slot}'s own chord set` : 'Keep chords') :
    r.kind === KIND.CHAIN ? 'the chain' : r.kind === KIND.CHANCE ? `progression P${r.slot + 1}'s chances` :
    `scale U${r.slot + 1}`;
}
// Every stored record: user scales, favorites, presets, progressions, chord sets, the step pattern,
// then the live settings. userScales comes from IDENTIFY (12 on firmware 0.6.0+, 3 before); favorites
// exist from 0.7.0, progressions from 0.11.0, chord sets from 1.0.0, step patterns from 1.5.0, looks (the
// colors, after the live settings) from 1.9.0, the presets' own curves and the preset names from 1.9.3 (content), the
// chain, each progression's chances, Keep chords and the presets' own chord sets from 2.0 (v2).
export async function readRecords(call, userScales = 3, favorites = false, progressions = false, chordSets = false, lanes = false,
  looks = false, content = false, v2 = false) {
  const records = [];
  for (const [kind, slots] of [[KIND.SCALE, userScales], [KIND.FAVORITES, favorites ? 1 : 0], [KIND.PRESET, 8],
    [KIND.PROG, progressions ? PROG_SLOTS : 0], [KIND.CHORDS, chordSets ? CHORD_SETS : 0],
    [KIND.LANE, lanes ? LANE_RECORDS : 0], [KIND.LIVE, 1], [KIND.LOOK, looks ? LOOKS : 0],
    [KIND.FEEL, content ? LOOKS : looks ? 1 : 0], [KIND.NAMES, content ? 1 : 0],
    [KIND.CHANCE, v2 ? PROG_SLOTS : 0], [KIND.CHAIN, v2 ? 1 : 0], [KIND.OWN_SET, v2 ? LOOKS : 0]]) {
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
// 1.8.0 added MPE and Glide (word 28 byte 3), cleared for earlier firmware (1.8.6: Glide Roll when 0; firmware
// before 1.8 has none); 1.8.3 Vel full (word 32 byte 3), and 1.8.6 Full past +3000 (word 32 byte 2 above 40), back to +3000,
// cleared, and the Firmer and Firmest velocity curves (4, 5 in word 1 byte 1 and word 31 byte 3), back to Firm;
// 1.7.0 added Unstick, Spread of and each half's program (word 19 above byte 0), Program in (word 33
// byte 3) and the Bass layout (word 24 byte 2 = 5, back to Scale), cleared for earlier firmware;
// 1.6.0 added the harmonizer, echo and chance (word 18) and mono (word 19), cleared for earlier firmware;
// 1.5.0 added the step pattern (kind 7), left out for earlier firmware, Chords on Right (word
// 26 byte 3), cleared for earlier firmware, and the Rainbow colors (word 4 bit 30), back to Default;
// 1.3.0 added the Full chord layout and the chord grid (the chords values FULL_CHORDS and
// GRID_CHORDS; earlier firmware plays the scale's chords instead);
// 1.0.0 added chord sets and, in settings and presets, the chord set, hints and scale lock
// (word 34) and the center buttons (words 35..38), and a progression's chord set (word 39);
// 0.11.0 progressions and, in settings and presets, the pedal's target (word 28
// byte 2), the melody half's velocity (word 31 byte 3) and the progression slot (word 33);
// 0.10.0 the pad thresholds (word 32).
// 1.4.0 values in live settings and presets back to what older firmware knows: arp rates past
// 1/32 to 1/16, Euclidean rhythms to the first pattern, the new note repeats off, repeat rhythm
// and rotation (word 27 bytes 2-3) and the pivot target (33 byte 2) cleared, Pivot buttons to
// their printed function.
function fit14(words) {
  const set = (w, b, v) => { words[w] = ((words[w] & ~(0xff << (8 * b))) | (v << (8 * b))) >>> 0; };
  const get = (w, b) => (words[w] >>> (8 * b)) & 0xff;
  if (get(2, 3) >= 6) set(2, 3, 3);
  if (get(23, 2) >= 8) set(23, 2, 0);
  if (get(31, 2) >= 5) set(31, 2, 0);
  words[27] &= 0xffff; set(33, 2, 0);
  for (let w = 35; w < 39; w++) for (let b = 0; b < 4; b++) if (get(w, b) > actionsFor('1.3.0')) set(w, b, 0);
}
// 2.0.0 added the chain, each progression's chances and the presets' own chord sets (kinds 11..13), left out for
// earlier firmware, and in settings and presets Sleep after and the Turing pattern's Lock and Loop (word 17 bytes 1-2),
// Chain (word 33 bit 9) and the U3 button (word 34 byte 3), cleared, and the Turing pattern (word 2 byte 2, word 4
// byte 2 = 7), back to Up. 1.9.4 added Step pads Silent (word 19 bit 10) and 2.0.2 Spread of Bass (word 19 bit 11,
// back to Root), cleared for earlier firmware; 2.0.3 the Fill and Stutter centre-button actions (words 35..38, back to
// the button's printed function) and the pedal's Fill (word 25 byte 3 = 8, back to Off).
export function recordsFor(records, version) {
  if (!version) return { records, skipped: [] };
  const skipped = [], out = [];
  for (const r of records) {
    if ((r.kind === KIND.OWN_SET || r.kind === KIND.CHAIN || r.kind === KIND.CHANCE) && !versionAtLeast(version, 2, 0)) { skipped.push(r); continue; }
    if (r.kind === KIND.PROG && !versionAtLeast(version, 0, 11)) { skipped.push(r); continue; }
    if (r.kind === KIND.CHORDS && !versionAtLeast(version, 1, 0)) { skipped.push(r); continue; }
    if (r.kind === KIND.LANE && !versionAtLeast(version, 1, 5)) { skipped.push(r); continue; }
    if ((r.kind === KIND.LOOK || r.kind === KIND.FEEL) && !versionAtLeast(version, 1, 9)) { skipped.push(r); continue; }
    // 1.9.3: preset names and the presets' own curves left out before it, Keep feel (feel word 0 byte 1) cleared
    if ((r.kind === KIND.NAMES || (r.kind === KIND.FEEL && r.slot)) && !versionAtLeast(version, 1, 9, 3)) { skipped.push(r); continue; }
    if (r.kind === KIND.FEEL && !versionAtLeast(version, 1, 9, 3)) { out.push({ ...r, words: r.words.map((w, i) => (i ? w : (w & 0xff) >>> 0)) }); continue; }
    if (r.kind === KIND.PROG && !versionAtLeast(version, 1, 0)) {
      out.push({ ...r, words: r.words.map((w, i) => (i === PAYLOAD_WORDS - 1 ? 0 : w)) }); continue;
    }
    if (r.kind === KIND.PROG && !versionAtLeast(version, 1, 3) && since13(r.words[PAYLOAD_WORDS - 1])) {
      out.push({ ...r, words: r.words.map((w, i) => (i === PAYLOAD_WORDS - 1 ? 0 : w)) }); continue;
    }
    if ((r.kind === KIND.LIVE || r.kind === KIND.PRESET) && r.words.length === PAYLOAD_WORDS) {
      const words = [...r.words];
      if (!versionAtLeast(version, 2, 0, 3)) { // Fill and Stutter buttons: their printed function; pedal Fill: Off
        for (let w = 35; w < 39; w++) for (let b = 0; b < 4; b++) {
          if (((words[w] >>> (8 * b)) & 0xff) > ACTIONS.indexOf('Fill (while held)')) words[w] = (words[w] & ~(0xff << (8 * b))) >>> 0;
        }
        if (((words[25] >>> 24) & 0xff) === 8) words[25] = (words[25] & 0xffffff) >>> 0;
      }
      if (!versionAtLeast(version, 2, 0, 2)) words[19] = (words[19] & ~0x800) >>> 0; // Spread of Bass: Root
      if (!versionAtLeast(version, 1, 9, 4)) words[19] = (words[19] & ~0x400) >>> 0; // Step pads: Sound
      if (!versionAtLeast(version, 2, 0)) { // 2.0's settings cleared, the Turing pattern back to Up
        words[17] = (words[17] & 0xff) >>> 0; words[33] = (words[33] & ~0x200) >>> 0; words[34] = (words[34] & 0xffffff) >>> 0;
        if (((words[2] >>> 16) & 0xff) === 7) words[2] = ((words[2] & ~0xff0000) | (1 << 16)) >>> 0;
        if (((words[4] >>> 16) & 0xff) === 7) words[4] = ((words[4] & ~0xff0000) | (1 << 16)) >>> 0;
      }
      if (!versionAtLeast(version, 1, 9)) { // 1.9.0's Custom velocity (6) back to Normal
        if (((words[1] >>> 8) & 0xff) === 6) words[1] = (words[1] & ~0xff00) >>> 0;
        if (((words[31] >>> 24) & 0xff) === 0x86) words[31] = ((words[31] & 0xffffff) | (0x80 << 24)) >>> 0;
      }
      if (!versionAtLeast(version, 1, 8)) {
        words[28] = (words[28] & 0xffffff) >>> 0; // MPE and Glide off
        words[32] = (words[32] & 0xffffff) >>> 0; // Vel full: = Full
        if (((words[32] >>> 16) & 0xff) > 40) words[32] = ((words[32] & 0xffff) | (40 << 16)) >>> 0; // Full: +3000
        const vel = (words[1] >>> 8) & 0xff, melody = (words[31] >>> 24) & 0xff;
        if (vel === 4 || vel === 5) words[1] = ((words[1] & ~0xff00) | (3 << 8)) >>> 0; // Firmer, Firmest: Firm
        if (melody === 0x84 || melody === 0x85) words[31] = ((words[31] & 0xffffff) | (0x83 << 24)) >>> 0;
      }
      if (!versionAtLeast(version, 1, 7)) { // Unstick, Spread of, the programs (word 19), Program in (33 byte 3) left out,
        words[19] = (words[19] & 0xff) >>> 0; words[33] = (words[33] & 0xffffff) >>> 0; // the Bass layout to Scale
        if (((words[24] >>> 16) & 0xff) === 5) words[24] = (words[24] & ~0xff0000) >>> 0;
      }
      if (!versionAtLeast(version, 1, 6)) { words[18] = 0; words[19] = 0; } // the 1.6 effects off
      if (!versionAtLeast(version, 1, 5)) { // chords back on the left, the colors back to Default
        words[26] &= 0xffffff; words[4] = (words[4] & ~0x40000000) >>> 0;
      }
      if (!versionAtLeast(version, 1, 4)) fit14(words);
      if (!versionAtLeast(version, 1, 0)) for (let i = 34; i < 39; i++) words[i] = 0;
      if (!versionAtLeast(version, 1, 3) && since13(words[34] & 0xff)) words[34] = (words[34] & ~0xff) >>> 0;
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
// The board's rainbow, one color per pitch class from C (src/tb/ui_lights.c `ui_hues`; the
// protocol test checks they match). In the editor view the root is white.
export const PAD_HUES = Object.freeze([[255, 0, 0], [255, 90, 0], [255, 190, 0], [160, 255, 0], [0, 255, 0],
  [0, 255, 130], [0, 220, 255], [0, 110, 255], [0, 0, 255], [60, 0, 255], [170, 0, 255], [255, 0, 100]]);

// --- messages where the action was (2.0.6, board test X5 and Y5) -----------------------------
// A tab's say(): the message goes in the message line (.said) of the part of the page last clicked or changed (an
// element with data-said holding one), else the tab's own status line. The lines keep their height whatever they
// say, so nothing on the page moves (a .said.pop floats under its control instead). A message that something worked
// goes after GOOD_MS; an error stays until the next message. again(): show it once more after the part was drawn anew.
export const GOOD_MS = 3000;
export function sayer(root, status) {
  const last = { at: null, when: 0, text: '', kind: '', where: null, shown: null, until: 0, timer: 0 };
  const mark = (ev) => {
    const part = ev.target && ev.target.closest ? ev.target.closest('[data-said]') : null;
    last.at = part && root.contains(part) ? part.dataset.said : null; last.when = Date.now();
  };
  for (const type of ['click', 'change', 'keydown']) root.addEventListener(type, mark, true);
  const lineOf = (at) => at && root.querySelector(`[data-said="${at}"] .said`);
  const put = (line, text, kind) => {
    if (!line) return;
    const keep = ['said', 'pop', 'pinned'].filter((c) => line.classList.contains(c)).join(' ');
    line.textContent = text; line.className = `status ${keep} ${kind}`.replace(/\s+/g, ' ').trim(); line.title = text;
  };
  function say(text, kind = '') {
    const near = Date.now() - last.when < 10000 ? lineOf(last.at) : null, line = near || status();
    if (last.where && last.where !== line) put(last.where, '', ''); // one message at a time
    put(line, text, kind);
    clearTimeout(last.timer);
    const lasts = kind === 'good' ? GOOD_MS : kind === 'err' ? Infinity : 10000;
    Object.assign(last, { text, kind, where: line, shown: near ? last.at : null, until: Date.now() + lasts });
    if (kind === 'good') last.timer = setTimeout(() => { if (last.text === text) { put(last.where, '', ''); last.text = ''; last.shown = null; } }, GOOD_MS);
  }
  say.again = () => { if (last.shown && Date.now() < last.until) { last.where = lineOf(last.shown); put(last.where, last.text, last.kind); } };
  return say;
}
