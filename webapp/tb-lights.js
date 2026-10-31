// SPDX-License-Identifier: GPL-3.0-or-later - the board's look (TBOSS 1.9.0, Customize Stage 2): the palette,
// the color schemes and their roles, the look record (docs/tb-sysex.md, src/tb/settings.h ST_LOOK) and each
// pad's color as the board shows it at rest (src/tb/ui_lights.c ui_pad_colors; tests/webapp/test_protocol.mjs
// checks this against the firmware's own code for every scheme, key and layout it draws).

// The palette a role picks from (ui_lights.c ui_palette), named as the editor shows them.
export const PALETTE = Object.freeze([
  ['Red', [255, 0, 0]], ['Orange', [255, 90, 0]], ['Gold', [255, 190, 0]], ['Lime', [160, 255, 0]], ['Green', [0, 255, 0]],
  ['Turquoise', [0, 255, 130]], ['Sky blue', [0, 220, 255]], ['Blue', [0, 110, 255]], ['Dark blue', [0, 0, 255]],
  ['Violet', [60, 0, 255]], ['Purple', [170, 0, 255]], ['Pink', [255, 0, 100]], ['White', [255, 255, 255]],
  ['Warm white', [255, 150, 60]], ['Gray', [40, 40, 50]], ['Off', [0, 0, 0]],
].map(([name, rgb]) => Object.freeze({ name, rgb })));
// An LED's 0..255 is light output (its PWM duty: twice the number, twice the light); a screen's 0..255 is
// gamma-encoded (sRGB). Shown as they are, a part-lit channel looks too dark on a screen (violet's red at 60 looks
// blue, turquoise's blue at 130 looks green), so the editor draws every LED color through onScreen (1.9.4).
const encode = (v) => { const x = v / 255; return Math.round(255 * (x <= 0.0031308 ? 12.92 * x : 1.055 * x ** (1 / 2.4) - 0.055)); };
export const onScreen = ([r, g, b]) => [encode(r), encode(g), encode(b)];
const NOTE_NAMES = ['C', 'C#', 'D', 'D#', 'E', 'F', 'F#', 'G', 'G#', 'A', 'A#', 'B'];
export const HUES = [[255, 0, 0], [255, 90, 0], [255, 190, 0], [160, 255, 0], [0, 255, 0], [0, 255, 130], [0, 220, 255],
  [0, 110, 255], [0, 0, 255], [60, 0, 255], [170, 0, 255], [255, 0, 100]];
// The schemes (Settings → Lights → Colors) in the board's order, each with its roles and their built-in
// colors (ui_lights.c ui_look_builtin); first: the role's index in the look record.
export const SCHEMES = Object.freeze([
  { name: 'Default', about: 'each pad the color of its note, the root column brightest',
    roles: NOTE_NAMES.map((n, i) => [n, HUES[i]]) },
  { name: 'Root+5th', about: 'the root and the 5th in their colors, the other notes dim',
    roles: [['Root', [255, 70, 0]], ['5th', [0, 90, 255]], ['Other notes', [40, 40, 50]], ['Edited chords', [160, 0, 255]], ['MIDI in', [0, 255, 60]]] },
  { name: 'Rainbow', about: 'seven colors across the columns, the same whatever plays',
    roles: [1, 2, 3, 4, 5, 6, 7].map((n, i) => [`Column ${n}${n <= 5 ? ` and ${n + 7}` : ''}`, HUES[[0, 1, 3, 4, 8, 6, 11][i]]]) },
  { name: 'Piano', about: 'white keys light, black keys dark, the root in its own color',
    roles: [['White keys', [200, 190, 170]], ['Black keys', [20, 20, 70]], ['Root', [255, 70, 0]]] },
  { name: 'Triad', about: "the key's root, 3rd and 5th, the other notes dim",
    roles: [['Root', [255, 70, 0]], ['3rd', [255, 190, 0]], ['5th', [0, 90, 255]], ['Other notes', [40, 40, 50]]] },
  { name: 'Function', about: 'tonic (I, iii, vi), subdominant (ii, IV) and dominant (V, vii) degrees',
    roles: [['Tonic', [0, 200, 80]], ['Subdominant', [255, 170, 0]], ['Dominant', [255, 30, 0]]] },
  { name: 'Octaves', about: 'each root bright, every other note dim in one color',
    roles: [['Roots', [255, 70, 0]], ['Other notes', [0, 60, 255]]] },
].reduce((out, s) => { const first = out.length ? out[out.length - 1].first + out[out.length - 1].roles.length : 0;
  out.push(Object.freeze({ ...s, first, roles: s.roles.map(([name, rgb]) => Object.freeze({ name, rgb })) })); return out; }, []));
export const ROLES = SCHEMES.reduce((n, s) => n + s.roles.length, 0); // 36
export const COLOR_BY = Object.freeze(['Note name', 'Scale degree']);
export const LOOK_NOW = 9; // the look on show (record kind 8)

// A look: { scheme, colorBy, keep, roles: [36 x 0 (built-in) or 1..16 (palette color n - 1)], showing (0 the
// board's, n preset n's) }.
export function lookFromWords(words) {
  const b = (w, n) => (words[w] >>> (8 * n)) & 255;
  return { scheme: b(0, 1), colorBy: b(0, 2), keep: b(0, 3), roles: Array.from({ length: ROLES }, (_, i) => b(1 + (i >> 2), i & 3)),
    showing: words[39] };
}
export function lookToWords(look) {
  if (!(look.scheme >= 0 && look.scheme < SCHEMES.length) || ![0, 1].includes(look.colorBy) || ![0, 1].includes(look.keep ? 1 : 0) ||
      look.roles.length !== ROLES || look.roles.some((r) => !(r >= 0 && r <= PALETTE.length))) throw new Error('not a look');
  const words = new Array(40).fill(0);
  words[0] = (1 | look.scheme << 8 | look.colorBy << 16 | (look.keep ? 1 : 0) << 24) >>> 0;
  look.roles.forEach((r, i) => { words[1 + (i >> 2)] = (words[1 + (i >> 2)] | r << (8 * (i & 3))) >>> 0; });
  return words;
}
export const roleRgb = (look, role) => (look.roles[role] ? PALETTE[look.roles[role] - 1].rgb : SCHEMES.find((s) => role < s.first + s.roles.length).roles[role - SCHEMES.find((s) => role < s.first + s.roles.length).first].rgb);

// ui_lights.c ui_scheme_role: the role of the note semis above the root (pitch class pc), percent bright.
const FUNCTION = [0, 1, 1, 0, 0, 1, 1, 2, 0, 0, 2, 2];
function schemeRole(scheme, semis, pc) {
  const s = SCHEMES[scheme];
  if (scheme === 3) return [s.first + (!semis ? 2 : (0x54a >> pc) & 1 ? 1 : 0), 100];
  if (scheme === 4) return semis === 3 || semis === 4 ? [s.first + 1, 100] : semis === 7 ? [s.first + 2, 100] : semis ? [s.first + 3, 35] : [s.first, 100];
  if (scheme === 5) return [s.first + FUNCTION[semis], semis ? 70 : 100];
  return [s.first + (semis ? 1 : 0), semis ? 30 : 100];
}
const scaled = ([r, g, b], max) => [Math.floor(r * max / 255), Math.floor(g * max / 255), Math.floor(b * max / 255)];
// Pad k (0..95: k / 48 the half, 0 the chord pads; (k % 48) / 4 the column; k % 4 the row, 3 the bottom) at rest,
// with the scale's chords on the chord pads and the melody pads in board.layout (0 Scale, 1 Chromatic, 2 Fourths;
// the other layouts are drawn as Scale). board: { tonic, steps (the scale's semitones), layout }; max: brightness;
// dim(rgb, level): a color at level of 255 (the firmware's scaling unless given, e.g. for a picture on a screen).
export function padColor(look, board, k, max = 255, dim = scaled) {
  const side = k >= 48 ? 1 : 0, column = (k % 48) >> 2, row = k & 3, up = 3 - row, steps = board.steps, n = steps.length;
  let semis = steps[column % n], inScale = true;
  if (side && board.layout === 1) semis = column % 12;
  if (side && board.layout === 2) semis = (column + 5 * up) % 12;
  if (side && (board.layout === 1 || board.layout === 2)) inScale = steps.includes(semis);
  const pc = (board.tonic + semis) % 12, note = () => roleRgb(look, (look.colorBy ? semis : pc) % 12);
  let color;
  if (look.scheme === 2) color = dim(roleRgb(look, SCHEMES[2].first + column % 12 % 7), column ? Math.floor(max * 60 / 100) : max);
  else if (!inScale && (look.scheme === 0 || look.scheme >= 3)) color = dim(look.scheme ? roleRgb(look, schemeRole(look.scheme, semis, pc)[0]) : note(), Math.floor(max * 10 / 100));
  else if (!inScale) color = dim([10, 10, 12], max);
  else if (look.scheme >= 3) { const [role, percent] = schemeRole(look.scheme, semis, pc); color = dim(roleRgb(look, role), Math.floor(max * percent / 100)); }
  else if (look.scheme === 0) color = dim(note(), semis === 0 ? max : Math.floor(max * 60 / 100));
  else color = dim(roleRgb(look, SCHEMES[1].first + (semis === 0 ? 0 : semis === 7 ? 1 : 2)), max);
  if (!color[0] && !color[1] && !color[2]) color[2] = 1;
  return color;
}
