// SPDX-License-Identifier: GPL-3.0-or-later - the step pattern (TBOSS 1.5+, src/tb/lanes.h) for the editor's
// Sequencer tab (1.9.3): 4 lanes of 64 steps, each lane stored as 4 records of 16 steps (kind 7, slot = lane * 4 +
// chunk; chunk 0 also holds the lane's settings, lane 1 chunk 0 the pattern's seed), the Euclid fill as the board
// does it, and names for what a step plays. Pure functions; tests/webapp/test_protocol.mjs checks them against the
// firmware's own code.
import { SCALES } from './tb-scales.js';

export const LANES = 4, STEPS = 64, CHUNK = 16, CHUNKS = STEPS / CHUNK, RECORDS = LANES * CHUNKS;
export const LENGTH_DEFAULT = 16, RATE_DEFAULT = 3, CHANNELS = 17, VELOCITY_DEFAULT = 100, GATE_PCT = 50;
export const KINDS = Object.freeze(['Chords', 'Melody', 'Drums', 'Off']);
export const KIND = Object.freeze({ CHORDS: 0, MELODY: 1, DRUMS: 2, OFF: 3 });
// The arpeggiator's rates (src/tb/ui_names.c ui_rate_names), a lane's rate n + 1 (0 = 1/16).
export const RATES = Object.freeze(['1/4', '1/8', '1/8T', '1/16', '1/16T', '1/32', '1/4T', '1/32T', '5/beat', '7/beat', '1/8.', '1/16.']);
export const DIRECTIONS = Object.freeze(['Forward', 'Reverse', 'Ping-pong', 'Random']);
export const CONDITIONS = Object.freeze(['Always', '1:2', '2:2', '1:3', '2:3', '3:3', '1:4', '2:4', '3:4', '4:4', 'First time', 'Not the first time',
  'Fill (while Fill is held)', 'Not fill (rests while Fill is held)']);
export const CONDITIONS_BEFORE_203 = 12; // Fill and Not fill came in 2.0.3 (older firmware plays them always)
// 2.0.3: a step's nudge (4 bits, two's complement: -8..7 sixteenths of a step), a lane's swing (0..50 % of a step,
// every second step) and transpose (chord lanes in scale steps, the others in semitones; the board's menu -24..24).
export const NUDGE_MIN = -8, NUDGE_MAX = 7, SWING_MAX = 50, SWING_STEP = 5, TRANSPOSE_MENU = 24;
export const nudgeOf = (n) => (n & 7) - (n & 8 ? 8 : 0);
export const nudgeBits = (v) => v & 15;
export const nudgeName = (v) => (v ? `${v > 0 ? '+' : '−'}${Math.abs(v)}/16 of a step ${v > 0 ? 'late' : 'early'}` : 'On time');
export const swingName = (s) => (s ? `${s} %` : 'Off');
export const transposeName = (kind, t) => (t ? `${t > 0 ? '+' : '−'}${Math.abs(t)} ${kind === KIND.CHORDS ? 'scale steps' : 'semitones'}` : 'None');
export const GATES = 11, PROBABILITIES = 10, RATCHETS = 8;
export const gateName = (g) => (g ? `${10 * g} %` : `Lane's (${GATE_PCT} %)`);
export const chanceName = (p) => (p ? `${100 - 10 * p} %` : 'Always');
export const ratchetName = (r) => (r ? `${r + 1} strikes` : '1 strike');
const ALL_SCALES = Math.max(...SCALES.map((s) => s.id)) + 1; // src/tb/layout.h TC_ALL_SCALES
const DRUM_BASE = 36;

export const emptyStep = () => ({ on: false, tie: false, value: 0, velocity: 0, edit: 0, scale: 0, gate: 0, probability: 0, ratchet: 0, condition: 0, nudge: 0 });
export const emptyLane = () => ({ steps: Array.from({ length: STEPS }, emptyStep), kind: 0, channel: 0, length: 0, rate: 0, direction: 0, swing: 0, mute: 0, euclid: 0, transpose: 0 });
export const emptyPattern = () => ({ lanes: Array.from({ length: LANES }, emptyLane), seed: 0 });
export const copyPattern = (p) => ({ seed: p.seed, lanes: p.lanes.map((l) => ({ ...l, steps: l.steps.map((s) => ({ ...s })) })) });
// A new step of this kind (the board's brush: controls_steps.c tc_brush_default).
export const brushDefault = (kind) => ({ ...emptyStep(), on: true, value: kind === KIND.DRUMS ? DRUM_BASE : 3 });

export const length = (l) => (l.length && l.length <= STEPS ? l.length : LENGTH_DEFAULT);
export const rate = (l) => (l.rate && l.rate <= RATES.length ? l.rate - 1 : RATE_DEFAULT);
export function setLength(l, n) { l.length = n === LENGTH_DEFAULT || !n || n > STEPS ? 0 : n; }
export function setRate(l, r) { l.rate = r === RATE_DEFAULT || r >= RATES.length ? 0 : r + 1; }

// A step in 64 bits (lanes.h ln_step_pack): bit 0 on, 1-7 value, 8-14 velocity, 15-22 edit, 23-29 scale, 30-33 gate,
// 34-37 probability, 38-40 ratchet, 41-45 condition, 46-49 nudge, 50 tie; [low word, high word].
const F = [['on', 0, 1], ['value', 1, 7], ['velocity', 8, 7], ['edit', 15, 8], ['scale', 23, 7], ['gate', 30, 4],
  ['probability', 34, 4], ['ratchet', 38, 3], ['condition', 41, 5], ['nudge', 46, 4], ['tie', 50, 1]];
export function stepPack(s) {
  let w = 0n;
  for (const [name, at, n] of F) w |= (BigInt(+s[name]) & ((1n << BigInt(n)) - 1n)) << BigInt(at);
  return [Number(w & 0xffffffffn) >>> 0, Number(w >> 32n) >>> 0];
}
// null for spare bits set, sus 3 or an unknown scale (ln_step_unpack).
export function stepUnpack(lo, hi) {
  const w = BigInt(lo >>> 0) | BigInt(hi >>> 0) << 32n;
  if (w >> 51n) return null;
  const s = {};
  for (const [name, at, n] of F) s[name] = Number((w >> BigInt(at)) & ((1n << BigInt(n)) - 1n));
  s.on = !!s.on; s.tie = !!s.tie;
  if (((s.edit >> 3) & 3) === 3 || s.scale > ALL_SCALES) return null;
  return s;
}
// A step's chord edits (settings.c's chord byte) as { lower, sus, add } and back.
export const editOf = (b) => ({ lower: b & 7, sus: 2 * ((b >> 3) & 3), add: b >> 5 });
export const editByte = (e) => (e.lower & 7) | ((e.sus >> 1) & 3) << 3 | (e.add & 7) << 5;

// Record slot's 40 words (ln_encode).
export function encodeChunk(p, slot) {
  const w = new Array(40).fill(0), lane = slot >> 2, chunk = slot & 3, l = p.lanes[lane];
  w[0] = 1;
  for (let i = 0; i < CHUNK; i++) [w[1 + 2 * i], w[2 + 2 * i]] = stepPack(l.steps[chunk * CHUNK + i]);
  if (chunk) return w;
  w[33] = (l.kind | l.channel << 8 | l.length << 16 | l.rate << 24) >>> 0;
  w[34] = (l.direction | l.swing << 8 | l.mute << 16 | (l.transpose & 255) << 24) >>> 0;
  w[35] = l.euclid;
  if (!lane) w[36] = p.seed;
  return w;
}
// A valid record for slot goes into p (true); otherwise p is untouched (ln_decode).
export function decodeChunk(w, slot, p) {
  if (slot < 0 || slot >= RECORDS || w.length !== 40 || w[0] !== 1) return false;
  const lane = slot >> 2, chunk = slot & 3, b = (x, n) => (x >>> (8 * n)) & 255;
  for (let i = 37; i < 40; i++) if (w[i]) return false;
  if (chunk && (w[33] || w[34] || w[35])) return false;
  if ((lane || chunk) ? w[36] !== 0 : w[36] > 0xffff) return false;
  const steps = [];
  for (let i = 0; i < CHUNK; i++) { const s = stepUnpack(w[1 + 2 * i], w[2 + 2 * i]); if (!s) return false; steps.push(s); }
  const l = p.lanes[lane];
  if (!chunk) {
    const transpose = (b(w[34], 3) << 24) >> 24;
    if (b(w[33], 0) >= KINDS.length || b(w[33], 1) >= CHANNELS || b(w[33], 2) > STEPS || b(w[33], 3) > RATES.length ||
      b(w[34], 0) > 3 || b(w[34], 1) > 50 || b(w[34], 2) > 1 || transpose < -48 || transpose > 48 || w[35] > STEPS) return false;
    Object.assign(l, { kind: b(w[33], 0), channel: b(w[33], 1), length: b(w[33], 2), rate: b(w[33], 3), direction: b(w[34], 0),
      swing: b(w[34], 1), mute: b(w[34], 2), transpose, euclid: w[35] });
    if (!lane) p.seed = w[36];
  }
  steps.forEach((s, i) => { l.steps[chunk * CHUNK + i] = s; });
  return true;
}
export const chunkDefault = (p, slot) => encodeChunk(p, slot).every((x, i) => !i || !x);
// The pattern from its 16 records (null: absent, the defaults); throws on one the board would refuse.
export function patternFromRecords(records) {
  const p = emptyPattern();
  records.forEach((w, slot) => { if (w && !decodeChunk(w, slot, p)) throw new Error(`lane ${(slot >> 2) + 1} steps ${(slot & 3) * 16 + 1}-${(slot & 3) * 16 + 16} unreadable`); });
  return p;
}

// Copy, paste and shift (TBOSS 2.0; the board's step screen does the same with Shift + its buttons, lanes.c ln_rotate).
// A lane moved one step later (by 1) or earlier (by -1), the last step coming round to the first; only the steps within
// its length move, those past it stay where they are.
export function rotate(l, by) {
  const n = length(l), shift = ((by % n) + n) % n, moved = l.steps.slice(0, n).map((s) => ({ ...s }));
  moved.forEach((s, i) => { l.steps[(i + shift) % n] = s; });
}
export const copyLane = (l) => ({ ...l, steps: l.steps.map((s) => ({ ...s })) });
// Pasting a lane makes the target a copy of it: its steps (all 64, also those past its length) and its settings (kind,
// channel, length, rate, direction, mute, Euclid fill), so when the lengths differ the pasted lane takes the copied one's.
export function pasteLane(p, i, copied) { p.lanes[i] = copyLane(copied); }
// A range of steps, from..to (either order, both included); pasted from step `at` on, over what is there, as far as the
// lane's 64 steps go (the lane's length and settings stay). Only onto a lane of the same kind: a chord step's value is a
// chord pad, a drum step's a note. Returns how many steps went in (0: refused).
export function copySteps(l, from, to) {
  const a = Math.max(0, Math.min(from, to)), b = Math.min(STEPS - 1, Math.max(from, to));
  return { kind: l.kind, steps: l.steps.slice(a, b + 1).map((s) => ({ ...s })) };
}
export function pasteSteps(l, at, copied) {
  if (!copied || copied.kind !== l.kind || at < 0 || at >= STEPS) return 0;
  const n = Math.min(copied.steps.length, STEPS - at);
  for (let i = 0; i < n; i++) l.steps[at + i] = { ...copied.steps[i] };
  return n;
}

// The firmware's repeatable randomness (lanes.c ln_hash: lowbias32 chained over the inputs).
const mix = (x) => {
  x ^= x >>> 16; x = Math.imul(x, 0x7feb352d); x ^= x >>> 15; x = Math.imul(x, 0x846ca68b); x ^= x >>> 16;
  return x >>> 0;
};
export const hash = (seed, lane, loop, step) => mix(mix(mix(mix((seed ^ 0x9e3779b9) >>> 0) ^ lane) ^ loop) ^ step);
// Mutate (2.0.3) as the board's Settings → Steps → Mutate does it (lanes.c ln_mutate): about pct % of the lane's steps
// change a little, repeatably from the pattern's seed, the lane and salt. Chord and melody steps that are on move one or
// two pads left or right (the row kept, reflected at the ends) and some change velocity; a drum step goes off, or on
// as the nearest step on before it.
export function mutate(l, lane, seed, pct, salt) {
  const n = length(l), was = l.steps.map((s) => ({ ...s }));
  for (let i = 0; i < n; i++) {
    const h = hash(seed, lane, (0x10000 + salt) >>> 0, i);
    if (h % 100 >= pct) continue;
    const s = l.steps[i];
    if (l.kind === KIND.DRUMS) {
      if (s.on) { s.on = false; continue; }
      for (let back = 1; back < n; back++) {
        const t = was[(i + n - back) % n];
        if (t.on) { l.steps[i] = { ...t }; break; }
      }
      continue;
    }
    if (!s.on || s.value >= 48) continue;
    const move = (h >>> 8) % 4;
    let column = (s.value >> 2) + (move < 2 ? move - 2 : move - 1);
    if (column < 0) column = -column;
    if (column > 11) column = 22 - column;
    s.value = 4 * column + (s.value & 3);
    if ((h >>> 12) & 1) s.velocity = Math.max(1, Math.min(127, (s.velocity || VELOCITY_DEFAULT) + 8 * (((h >>> 13) % 5) - 2)));
  }
}
// Randomize (2.0.3, the editor only): new steps over the lane's length, each on at `density` %: chord lanes the scale's
// chords in root position (the bottom row, columns 1-7), melody lanes the bottom two rows of the Scale layout's first
// octave, drum lanes the notes already in the lane (else kick, snare, closed hat). rnd: () => [0, 1).
export function randomize(l, density, rnd = Math.random) {
  const n = length(l), drums = [...new Set(l.steps.filter((s, i) => s.on && i < n).map((s) => s.value))];
  const kit = drums.length ? drums : [36, 38, 42];
  for (let i = 0; i < n; i++) {
    if (rnd() * 100 >= density) { l.steps[i] = emptyStep(); continue; }
    const pick = (k) => Math.floor(rnd() * k);
    const value = l.kind === KIND.DRUMS ? kit[pick(kit.length)] : l.kind === KIND.CHORDS ? 4 * pick(7) + 3 : 4 * pick(7) + 2 + pick(2);
    l.steps[i] = { ...emptyStep(), on: true, value };
  }
  l.euclid = 0;
}

// Euclidean rhythm (src/tb/rhythm.c rh_euclid): `hits` spread over `steps`, rotated right by `rotate`; [bool].
export function euclid(hits, steps, rotate = 0) {
  if (!steps || steps > STEPS) return [];
  hits = Math.min(hits, steps);
  const base = Array.from({ length: steps }, (_, i) => hits > 0 && (i * hits) % steps < hits);
  const r = rotate % steps;
  return base.map((_, i) => base[(i - r + steps) % steps]);
}
// The lane filled as the board's Euclid does (lanes.c ln_euclid_fill): a hit already on keeps what it plays, a new one
// gets brush, the rest go off; 0 hits leaves the steps. Remembers the hits.
export function euclidFill(l, hits, rotate, brush) {
  const n = length(l);
  l.euclid = Math.min(hits, n);
  if (!hits) return;
  euclid(hits, n, rotate).forEach((hit, i) => {
    if (!hit) l.steps[i] = emptyStep();
    else if (!l.steps[i].on) l.steps[i] = { ...brush, on: true };
  });
}

// What a step plays, for a label: a chord pad (column and row from the top: 4 * column + row), a place in the
// melody half's Scale layout (the same numbering), or a MIDI note (drums, GM names).
const NOTES = ['C', 'C#', 'D', 'D#', 'E', 'F', 'F#', 'G', 'G#', 'A', 'A#', 'B'];
export const noteName = (n) => `${NOTES[n % 12]}${Math.floor(n / 12) - 1}`;
export const DRUMS = Object.freeze({ 35: 'Kick 2', 36: 'Kick', 37: 'Side stick', 38: 'Snare', 39: 'Clap', 40: 'Snare 2', 41: 'Low floor tom',
  42: 'Closed hat', 43: 'High floor tom', 44: 'Pedal hat', 45: 'Low tom', 46: 'Open hat', 47: 'Low mid tom', 48: 'High mid tom',
  49: 'Crash', 50: 'High tom', 51: 'Ride', 52: 'China', 53: 'Ride bell', 54: 'Tambourine', 55: 'Splash', 56: 'Cowbell',
  57: 'Crash 2', 59: 'Ride 2', 60: 'High bongo', 61: 'Low bongo', 62: 'Mute conga', 63: 'Open conga', 64: 'Low conga',
  69: 'Cabasa', 70: 'Maracas', 75: 'Claves', 76: 'High wood block', 77: 'Low wood block', 81: 'Open triangle' });
export function valueName(kind, v, chordNames = null) {
  if (kind === KIND.DRUMS) return `${noteName(v)}${DRUMS[v] ? ' ' + DRUMS[v] : ''}`;
  const where = `column ${(v >> 2) + 1}, row ${(v & 3) + 1}`;
  if (v >= 48) return `${v} (no pad)`;
  if (kind === KIND.CHORDS) return chordNames && chordNames[v] ? `${chordNames[v]} (${where})` : `Chord pad ${where}`;
  return `Melody pad ${where}`;
}
export function valueShort(kind, v, chordNames = null) {
  if (kind === KIND.DRUMS) return noteName(v);
  if (kind === KIND.CHORDS && chordNames && chordNames[v]) return chordNames[v];
  return `${(v >> 2) + 1}.${(v & 3) + 1}`;
}
export const valueCount = (kind) => (kind === KIND.DRUMS ? 128 : 48);
