// SPDX-License-Identifier: GPL-3.0-or-later - TheoryBoard SysEx protocol v1 (firmware: src/tb/sysex.h,
// description: docs/tb-sysex.md). Pure functions: no Web MIDI, usable in Node tests.
//
//   request  F0 7D 54 42 cmd data... sum F7
//   reply    F0 7D 54 42 cmd|40 status data... sum F7

export const VERSION = 1;
export const CMD = Object.freeze({
  IDENTIFY: 1, GET_SCALE: 2, SET_SCALE: 3, PREVIEW: 4, SELECT_SCALE: 5,
  GET_RECORD: 6, SET_RECORD: 7, NOR_READ: 8,
});
export const STATUS = Object.freeze({
  0: 'OK', 1: 'checksum error', 2: 'wrong length', 3: 'value not accepted', 4: 'unknown command',
  5: 'storage error', 6: 'external memory unavailable or read failed', 7: 'nothing stored there',
});
export const KIND = Object.freeze({ LIVE: 1, PRESET: 2, SCALE: 3 });
export const PAYLOAD_WORDS = 20;
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
export const preview = (mask, root) => request(CMD.PREVIEW, [mask & 0x7f, (mask >> 7) & 0x1f, root % 12]);
export const selectScale = (id) => request(CMD.SELECT_SCALE, [id]);
export const getRecord = (kind, slot) => request(CMD.GET_RECORD, [kind, slot]);
export function setRecord(kind, slot, words) {
  if (words.length !== PAYLOAD_WORDS) throw new Error('a record has 20 words');
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
// Scales to start from or recognise. The first 12 are the TB's built-in scales.
export const KNOWN_SCALES = [
  ['Major', [0, 2, 4, 5, 7, 9, 11]], ['Natural minor', [0, 2, 3, 5, 7, 8, 10]],
  ['Dorian', [0, 2, 3, 5, 7, 9, 10]], ['Phrygian', [0, 1, 3, 5, 7, 8, 10]],
  ['Lydian', [0, 2, 4, 6, 7, 9, 11]], ['Mixolydian', [0, 2, 4, 5, 7, 9, 10]],
  ['Locrian', [0, 1, 3, 5, 6, 8, 10]], ['Harmonic minor', [0, 2, 3, 5, 7, 8, 11]],
  ['Melodic minor', [0, 2, 3, 5, 7, 9, 11]], ['Major pentatonic', [0, 2, 4, 7, 9]],
  ['Minor pentatonic', [0, 3, 5, 7, 10]], ['Chromatic', [0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11]],
  ['Blues', [0, 3, 5, 6, 7, 10]], ['Whole tone', [0, 2, 4, 6, 8, 10]],
  ['Harmonic major', [0, 2, 4, 5, 7, 8, 11]], ['Hungarian minor', [0, 2, 3, 6, 7, 8, 11]],
  ['Phrygian dominant', [0, 1, 4, 5, 7, 8, 10]], ['Double harmonic', [0, 1, 4, 5, 7, 8, 11]],
  ['Diminished (whole-half)', [0, 2, 3, 5, 6, 8, 9, 11]], ['Diminished (half-whole)', [0, 1, 3, 4, 6, 7, 9, 10]],
  ['Bebop dominant', [0, 2, 4, 5, 7, 9, 10, 11]], ['Hirajoshi', [0, 2, 3, 7, 8]],
  ['In sen', [0, 1, 5, 7, 10]], ['Lydian dominant', [0, 2, 4, 6, 7, 9, 10]], ['Altered', [0, 1, 3, 4, 6, 8, 10]],
].map(([name, steps]) => ({ name, mask: maskOf(steps) }));
export function scaleNamesFor(mask) { return KNOWN_SCALES.filter((s) => s.mask === mask).map((s) => s.name); }
