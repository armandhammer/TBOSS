// SPDX-License-Identifier: GPL-3.0-or-later - the board's feel (TBOSS 1.9.0, Customize Stage 3): the velocity and
// pressure curves of each half (src/tb/controls.h TC_FEEL_, the record docs/tb-sysex.md kind 9), the curve
// itself as the firmware draws it (controls.c tc_feel_curve, tc_feel_pressure; tests/webapp/test_protocol.mjs
// checks both against the firmware), the built-in velocity curves as points, and the calibration's proposals.

export const POINTS = 8;      // a curve's points, at inputs evenly from 0 to 127
export const PART = POINTS + 2; // its points, then two bytes: velocity's lowest/highest, pressure's start/full
export const BYTES = 4 * PART;
export const PARTS = Object.freeze([
  { name: 'Chord velocity', velocity: true, half: 0 }, { name: 'Melody velocity', velocity: true, half: 1 },
  { name: 'Chord pressure', velocity: false, half: 0 }, { name: 'Melody pressure', velocity: false, half: 1 },
]);
export const LINEAR = Object.freeze([0, 18, 36, 54, 73, 91, 109, 127]);
export const CUSTOM = 6; // Chord vel / Melody vel: Custom
export const defaults = () => PARTS.flatMap((p) => [...LINEAR, p.velocity ? 1 : 0, 127]);
// One part of the feel: { points, low, high } (velocity: lowest and highest velocity; pressure: where it starts and
// is full, on the way from Release to Full, 0..127).
export const part = (feel, i) => ({ points: feel.slice(i * PART, i * PART + POINTS), low: feel[i * PART + POINTS], high: feel[i * PART + POINTS + 1] });
export function withPart(feel, i, { points, low, high }) {
  const out = [...feel]; out.splice(i * PART, PART, ...points, low, high); return out;
}
export function feelOk(feel) {
  return feel.length === BYTES && feel.every((b) => Number.isInteger(b) && b >= 0 && b <= 127) &&
    PARTS.every((p, i) => { const { low, high } = part(feel, i); return p.velocity ? low >= 1 && low <= high : low < high; });
}
// keep: Keep feel (1.9.3; only in the board's record and the curves in use, slot 9).
export function feelToWords(feel, keep = 0) {
  if (!feelOk(feel)) throw new Error('not a feel');
  const words = new Array(40).fill(0); words[0] = (1 | (keep ? 1 : 0) << 8) >>> 0;
  feel.forEach((b, i) => { words[1 + (i >> 2)] = (words[1 + (i >> 2)] | b << (8 * (i & 3))) >>> 0; });
  return words;
}
// 1.9.3: Keep feel and, for the curves in use (slot 9), whose they are (0 the board's, n preset n's own).
export const feelKeep = (words) => (words[0] >>> 8) & 1;
export const feelWhose = (words) => words[39];
export function feelFromWords(words) {
  if (!words[0]) return defaults();
  return Array.from({ length: BYTES }, (_, i) => (words[1 + (i >> 2)] >>> (8 * (i & 3))) & 255);
}
// tc_feel_curve: straight between the points; x 0..127.
export function curve(points, x) {
  const pos = Math.min(127, Math.max(0, x)) * (POINTS - 1), i = Math.floor(pos / 127), f = pos % 127;
  if (i >= POINTS - 1) return points[POINTS - 1];
  const a = points[i], b = points[i + 1];
  return a + Math.trunc(((b - a) * f) / 127);
}
// tc_feel_pressure: a pressure level (0..127 from Release to Full) through a half's range and curve.
export function pressureOut(feel, half, level) {
  const { points, low, high } = part(feel, 2 + half);
  if (!level || (!low && high === 127 && points.every((p, i) => p === LINEAR[i]))) return level;
  const t = level <= low ? 0 : level >= high ? 127 : Math.floor(((level - low) * 127) / (high - low));
  return curve(points, t);
}
// The velocity a Custom half plays for a strike r (0..126: just past Trigger to Vel full), engine_pads.c tp_velocity.
export function velocityOut(feel, half, r) {
  const { points, low, high } = part(feel, half);
  const s = curve(points, Math.floor((r * 127) / 126)), v = low + (high > low ? Math.floor(((high - low) * s) / 127) : 0);
  return Math.max(1, Math.min(127, v));
}
// The built-in curves (Chord vel / Melody vel) as points, to start a Custom one from.
const r126 = (x) => (x * 126) / 127;
export const SHAPES = Object.freeze([
  ['Normal', (r) => r], ['Light', (r) => 126 - ((126 - r) * (126 - r)) / 126], ['Firm', (r) => (r * r) / 126],
  ['Firmer', (r) => (r * r * r) / (126 * 126)], ['Firmest', (r) => (r * r * r * r) / (126 * 126 * 126)],
].map(([name, f]) => Object.freeze({ name, points: LINEAR.map((x) => Math.round((f(r126(x)) * 127) / 126)) })));

// --- calibration (the Feel tab's wizard) -------------------------------------------------
// samples: { quiet: [per pad: the highest reading above its rest with hands off], soft, medium, hard: [per pad:
// its highest reading above rest in that round] }. Each round's level is the median of the pads pressed in it
// (clearly above the quiet noise). The proposal: Trigger at half a soft press (at least the noise + 200, 500 ..
// 2000), Release 400 below it, Full at 90 % of a hard press, Vel full at a hard press, and a velocity curve
// playing a soft press at 35 and a medium one at 85. All in hundreds above each pad's rest, as the board's.
export function median(values) {
  const v = [...values].sort((a, b) => a - b); return v.length ? v[Math.floor(v.length / 2)] : 0;
}
const clamp = (x, lo, hi) => Math.max(lo, Math.min(hi, x));
export function propose({ quiet, soft, medium, hard }) {
  const noise = Math.max(0, ...quiet), pressed = (round) => round.filter((v) => v > noise + 300);
  const s = median(pressed(soft)), m = median(pressed(medium)), h = median(pressed(hard));
  if (!s || !m || !h) return { error: 'some rounds had no clear presses: press the pads firmly enough to play them, and try again' };
  if (!(s < m && m < h)) return { error: 'the soft, medium and hard presses were not each harder than the one before: try again' };
  const trigger = clamp(Math.floor((s / 2) / 100), Math.max(5, Math.ceil((noise + 200) / 100)), 20);
  const release = clamp(trigger - 4, 1, Math.min(15, trigger - 2));
  const full = clamp(Math.floor((h * 0.9) / 100), Math.max(10, trigger + 5), 40);
  const velFull = clamp(Math.round(h / 100), 10, 40);
  const at = (x) => clamp(((x / 100) - trigger) / (velFull - trigger), 0.01, 0.99); // where a press lands, 0..1
  const knots = [[0, 0], [at(s), 35], [Math.max(at(m), at(s) + 0.01), 85], [1, 127]];
  const points = LINEAR.map((x) => {
    const t = x / 127; let k = 1; while (k < knots.length - 1 && t > knots[k][0]) k++;
    const [x0, y0] = knots[k - 1], [x1, y1] = knots[k];
    return clamp(Math.round(y0 + ((y1 - y0) * (t - x0)) / Math.max(x1 - x0, 1e-6)), 0, 127);
  });
  for (let i = 1; i < POINTS; i++) points[i] = Math.max(points[i], points[i - 1]); // never softer for a harder press
  return { noise, soft: s, medium: m, hard: h, trigger, release, full, velFull, points };
}
