// SPDX-License-Identifier: GPL-3.0-or-later - chord names and progressions for the web editor (0.11.0).
// The chord pads stack scale steps like the firmware (src/tb/layout.c tones()): pad k plays
// scale step column = k / 4 with the steps 2 and 4 above it, the 2nd or 4th instead of the
// third when sus is 2 or 4, and the 6th, 7th and 9th steps when added; lowered members drop an
// octave (a lowered third or fifth is the bass). A progression step stores the pad, its edits
// and, when it is borrowed, its own scale (src/tb/sequence.h). Pure functions, no Web MIDI.
import { SCALES } from './tb-scales.js';

export const SHARPS = ['C', 'C#', 'D', 'D#', 'E', 'F', 'F#', 'G', 'G#', 'A', 'A#', 'B'];
export const FLATS = ['C', 'Db', 'D', 'Eb', 'E', 'F', 'Gb', 'G', 'Ab', 'A', 'Bb', 'B'];
export const NO_SCALE = 127; // a step in the progression's own (the board's current) scale
export const STEPS_MAX = 38, UNITS_MAX = 64, RUN_MAX = 32;
export const UNIT_NAMES = ['1 beat', '½ bar', '1 bar', '2 bars'];
const ROW_ROOT_POSITION = 3; // the bottom row plays the chord in root position
const VELOCITY = 100;

// Chord qualities: the names accepted (the first is the one shown) and the semitones above the root.
const QUALITIES = [
  [['', 'maj', 'M'], [0, 4, 7]],
  [['m', 'min', '-'], [0, 3, 7]],
  [['dim', '°', 'o'], [0, 3, 6]],
  [['aug', '+'], [0, 4, 8]],
  [['sus2'], [0, 2, 7]],
  [['sus4', 'sus'], [0, 5, 7]],
  [['6', 'maj6', 'M6'], [0, 4, 7, 9]],
  [['m6', 'min6', '-6'], [0, 3, 7, 9]],
  [['7', 'dom7'], [0, 4, 7, 10]],
  [['maj7', 'M7', 'Δ', 'Δ7', 'ma7'], [0, 4, 7, 11]],
  [['m7', 'min7', '-7'], [0, 3, 7, 10]],
  [['mMaj7', 'mM7', 'm(maj7)', 'minmaj7', 'mΔ7', '-Δ7'], [0, 3, 7, 11]],
  [['m7b5', 'ø', 'ø7', 'min7b5', '-7b5'], [0, 3, 6, 10]],
  [['dim7', '°7', 'o7'], [0, 3, 6, 9]],
  [['7#5', 'aug7', '+7'], [0, 4, 8, 10]],
  [['7b5'], [0, 4, 6, 10]],
  [['maj7#5', 'augmaj7', '+M7'], [0, 4, 8, 11]],
  [['7sus4', '7sus'], [0, 5, 7, 10]],
  [['7sus2'], [0, 2, 7, 10]],
  [['add9', 'add2'], [0, 2, 4, 7]],
  [['madd9', 'm(add9)', 'madd2'], [0, 2, 3, 7]],
  [['9'], [0, 2, 4, 7, 10]],
  [['maj9', 'M9', 'Δ9'], [0, 2, 4, 7, 11]],
  [['m9', 'min9', '-9'], [0, 2, 3, 7, 10]],
  [['6/9', '69', '6add9'], [0, 2, 4, 7, 9]],
  [['m6/9', 'm69'], [0, 2, 3, 7, 9]],
  [['9sus4', '9sus'], [0, 2, 5, 7, 10]],
  [['7b9'], [0, 1, 4, 7, 10]],
  [['7#9'], [0, 3, 4, 7, 10]],
  [['13'], [0, 2, 4, 7, 9, 10]],
  [['maj13', 'M13'], [0, 2, 4, 7, 9, 11]],
  [['m13'], [0, 2, 3, 7, 9, 10]],
].map(([names, semis]) => ({ names, set: semis.reduce((m, s) => m | (1 << s), 0) }));

const mod12 = (n) => ((n % 12) + 12) % 12;
const bits = (m) => { let n = 0; for (; m; m &= m - 1) n++; return n; };
function noteOf(text) {
  const m = /^([A-G])([#♯b♭]?)$/.exec(text);
  if (!m) return null;
  return mod12(SHARPS.indexOf(m[1]) + (m[2] === '#' || m[2] === '♯' ? 1 : m[2] ? -1 : 0));
}
// Flats for keys that are written with them (F, Bb, Eb, Ab, Db major; D, G, C, F, Bb, Eb minor).
export function usesFlats(tonic, steps) {
  const minor = steps.includes(3) && !steps.includes(4);
  return (minor ? [2, 7, 0, 5, 10, 3] : [5, 10, 3, 8, 1]).includes(mod12(tonic));
}
export function noteName(pc, flats = false) { return (flats ? FLATS : SHARPS)[mod12(pc)]; }
// Note names in a key: its own notes as the key writes them, the others as flats (Eb, Ab, Bb
// in C), except the raised 4th (F# in C).
export function speller(tonic, steps) {
  const flats = usesFlats(tonic, steps);
  return (pc) => {
    const rel = mod12(pc - tonic);
    return noteName(pc, steps.includes(rel) ? flats : rel !== 6);
  };
}

// "Am", "E7", "F#m7b5", "C/G" -> { root, set (bit n = n semitones above the root), bass }.
export function parseChord(text) {
  const t = String(text).trim();
  const m = /^([A-Ga-g][#♯b♭]?)(.*)$/.exec(t);
  if (!m) throw new Error(`"${t}" is not a chord name`);
  const root = noteOf(m[1][0].toUpperCase() + m[1].slice(1));
  let rest = m[2], bass = null;
  const slash = /\/([A-G][#♯b♭]?)$/.exec(rest);
  if (slash && !/^m?6\/9$/.test(rest)) { bass = noteOf(slash[1]); rest = rest.slice(0, slash.index); }
  const quality = QUALITIES.find((q) => q.names.includes(rest));
  if (root === null || !quality) throw new Error(`"${t}" is not a chord name I know`);
  return { root, set: quality.set, bass: bass === null || bass === root ? null : bass };
}
export function chordName(chord, spell = (pc) => noteName(pc)) {
  const q = QUALITIES.find((x) => x.set === chord.set);
  const semis = []; for (let s = 1; s < 12; s++) if ((chord.set >> s) & 1) semis.push(s);
  const name = spell(chord.root) + (q ? q.names[0] : `(${semis.join(',')})`);
  return chord.bass === null || chord.bass === undefined ? name : `${name}/${spell(chord.bass)}`;
}

// The chord a pad plays: steps = the scale's semitones, tonic = the board's root.
export function padChord(steps, tonic, pad, edit) {
  const n = steps.length, column = pad >> 2;
  const at = (k) => mod12(tonic + steps[(column + k) % n]);
  const members = [0, edit.sus ? edit.sus - 1 : 2, 4], root = at(0);
  let set = 0;
  for (const k of members) set |= 1 << mod12(at(k) - root);
  [5, 6, 8].forEach((k, x) => { if ((edit.add >> x) & 1) set |= 1 << mod12(at(k) - root); });
  const lowered = edit.lower & 1 ? 0 : edit.lower & 2 ? 1 : edit.lower & 4 ? 2 : 0;
  const bass = lowered ? at(members[lowered]) : null;
  return { root, set, bass: bass === root ? null : bass };
}

// The chord grid (TBOSS 1.3, a progression's chords value 10): each row adds a chord type to the
// pad's edit, from the top a 7th and 9th, a sus (sus4, or sus2 where the 4th above isn't perfect
// and the 2nd above is a whole step; a sus in the edit wins), a 7th, nothing.
export const GRID_CHORDS = 10;
export function gridEdit(steps, pad, edit) {
  const n = steps.length, column = pad >> 2, row = pad & 3;
  const at = (k) => 12 * Math.floor((column + k) / n) + steps[(column + k) % n];
  const add = edit.add | [6, 0, 2, 0][row];
  const sus = row === 1 && !edit.sus ? (at(3) - at(0) !== 5 && at(1) - at(0) === 2 ? 2 : 4) : edit.sus;
  return { ...edit, sus, add };
}

// Edits from simplest: no sus before sus, fewer added notes first.
const EDITS = [];
for (const sus of [0, 2, 4]) for (let add = 0; add < 8; add++) EDITS.push({ lower: 0, sus, add });
EDITS.sort((a, b) => (a.sus ? 1 : 0) - (b.sus ? 1 : 0) || bits(a.add) - bits(b.add) || a.add - b.add);

// The major and minor modes, harmonic and melodic minor, harmonic major: borrowed from first.
const COMMON = new Set([0, 1, 2, 3, 4, 5, 6, 7, 8, 26]);
// Where to play `chord` in key tonic with home scale `home` (an id in `scales`, [{ id, steps }]):
// the home scale first, then the scales closest to it (each note it has or lacks that the home
// scale lacks or has counts 1, a less common seven-note scale 1 more, any other scale 2 more),
// so D in A minor comes from A Dorian, E7 from A harmonic minor and Fm in C from C harmonic major. { pad, scale (NO_SCALE for home),
// edit } or null.
export function placeChord(chord, tonic, home, scales = SCALES) {
  const maskOf = (steps) => steps.reduce((m, s) => m | (1 << s), 0);
  const homeScale = scales.find((s) => s.id === home);
  if (!homeScale) throw new Error(`unknown scale ${home}`);
  const homeMask = maskOf(homeScale.steps);
  const cost = (s) => bits(maskOf(s.steps) ^ homeMask) + (COMMON.has(s.id) ? 0 : s.steps.length === 7 ? 1 : 2);
  const order = scales.map((s, i) => ({ s, i, d: s.id === home ? -1 : cost(s) }))
    .sort((a, b) => a.d - b.d || a.i - b.i);
  for (const { s } of order) {
    const column = s.steps.indexOf(mod12(chord.root - tonic));
    if (column < 0) continue;
    const pad = 4 * column + ROW_ROOT_POSITION;
    for (const edit of EDITS) {
      const got = padChord(s.steps, tonic, pad, edit);
      if (got.set !== chord.set) continue;
      // A bass note other than the root: lower the third or fifth that is that note.
      const member = chord.bass === null ? 0 :
        [1, 2].find((m) => padChord(s.steps, tonic, pad, { ...edit, lower: 1 << m }).bass === chord.bass);
      if (member === undefined) continue;
      return { pad, scale: s.id === home ? NO_SCALE : s.id, edit: { ...edit, lower: member ? 1 << member : 0 } };
    }
  }
  return null;
}

// Text -> [{ text, chord, units }]: chord names separated by spaces, commas or bar lines;
// "Am*2" is two units (as is "Am Am").
export function parseProgressionText(text) {
  const out = [];
  for (const token of String(text).split(/[\s,|]+/).filter(Boolean)) {
    const m = /^(.*?)(?:\*(\d+))?$/.exec(token);
    const units = m[2] === undefined ? 1 : +m[2];
    if (!units) throw new Error(`"${token}": a chord lasts at least one unit`);
    out.push({ text: m[1], chord: parseChord(m[1]), units });
  }
  return out;
}

// Text -> a progression in key (tonic, home scale id): { prog, errors }. prog is null when
// anything could not be placed or the progression is too long for a slot.
export function progressionFromText(text, tonic, home, unit = 2, scales = SCALES) {
  const errors = [];
  let parsed = [];
  try { parsed = parseProgressionText(text); } catch (e) { return { prog: null, errors: [e.message] }; }
  const steps = [];
  for (const p of parsed) {
    const at = placeChord(p.chord, tonic, home, scales);
    if (!at) { errors.push(`${p.text} cannot be played on the chord pads in this key`); continue; }
    for (let left = p.units; left > 0;) {
      const last = steps[steps.length - 1];
      if (last && last.pad === at.pad && last.scale === at.scale && sameEdit(last.edit, at.edit) && last.units < RUN_MAX) {
        const n = Math.min(left, RUN_MAX - last.units); last.units += n; left -= n;
      } else {
        const n = Math.min(left, RUN_MAX); steps.push({ ...at, edit: { ...at.edit }, units: n, velocity: VELOCITY }); left -= n;
      }
    }
  }
  const total = steps.reduce((a, s) => a + s.units, 0);
  if (!parsed.length) errors.push('no chords');
  if (steps.length > STEPS_MAX) errors.push(`${steps.length} chord changes: a slot holds up to ${STEPS_MAX}`);
  if (total > UNITS_MAX) errors.push(`${total} units: a slot holds up to ${UNITS_MAX}`);
  return { prog: errors.length ? null : { unit, scale: home, steps }, errors };
}
export function sameEdit(a, b) { return a.lower === b.lower && a.sus === b.sus && a.add === b.add; }

// Each step's chord in key tonic: stepsOf(id) gives a scale's semitones (null if unknown).
// { chord, name, units, borrowed (scale id or null) }; chord is null for an unknown scale.
export function progressionChords(prog, tonic, stepsOf) {
  const homeSteps = stepsOf(prog.scale) || [0, 2, 4, 5, 7, 9, 11];
  const spell = speller(tonic, homeSteps);
  return prog.steps.map((s) => {
    const own = s.scale === NO_SCALE ? homeSteps : stepsOf(s.scale);
    const edit = own && prog.chords === GRID_CHORDS ? gridEdit(own, s.pad, s.edit) : s.edit;
    const chord = own ? padChord(own, tonic, s.pad, edit) : null;
    return { chord, name: chord ? chordName(chord, spell) : '?', units: s.units, borrowed: s.scale === NO_SCALE ? null : s.scale };
  });
}
export function progressionToText(prog, tonic, stepsOf) {
  const out = [];
  for (const c of progressionChords(prog, tonic, stepsOf)) {
    const last = out[out.length - 1];
    if (last && last.name === c.name) last.units += c.units; else out.push({ name: c.name, units: c.units });
  }
  return out.map((c) => (c.units === 1 ? c.name : `${c.name}*${c.units}`)).join(' ');
}
