// SPDX-License-Identifier: GPL-3.0-or-later - what each of the 96 pads plays, as the firmware does (src/tb/layout.c
// tc_notes_panel and the melody layouts) and the chord names the screen shows (src/tb/chord_name.c tn_name), for the
// manual's pad explorer. tests/webapp/test_protocol.mjs checks it pad by pad against the firmware (tests/webapp/lights.c).
//
// A layout: { tonic (0..11), steps (the scale's semitones), chords ('scale' | 'grid' | 'set' | 'full'), set (12 chord
// set columns { mask, root }, for 'set'), full (48 Full pads { root: a pitch class, notes } at chord base 48, for 'full'),
// chordBase,
// melodyBase, melodyLayout (MELODY_ index) }. A panel: { voicing (0 off, 1..3, 4 guitar), spread (0..3), spreadOf
// (0 root, 1 chord, 2 bass), octaves (bit i: i - 3 octaves; 8 = as is), capo (0..11) }.
// Pads: k = 48 * side + 4 * column + row (side 0 the chord pads, row 0 the top, 3 the bottom).

export const CHORD_BASE = 36, MELODY_BASE = 48, DRUM_BASE = 36, BASS_DROP = 24, LIVE_COLUMN = 7;
export const MELODY_LAYOUTS = ['Scale', 'Chromatic', 'Fourths', 'Drums', 'Live notes', 'Bass'];
export const VOICINGS = ['Off', 'I', 'II', 'III', 'Guitar'];
export const SPREADS = ['Off', 'I', 'II', 'III'];
export const SPREAD_OFS = ['Root', 'Chord', 'Bass'];
const VOICING_GUITAR = 4, SPREAD_OF_BASS = 2, OCTAVES_DEFAULT = 0x08;
export const NEUTRAL = Object.freeze({ voicing: 0, spread: 0, spreadOf: 0, octaves: OCTAVES_DEFAULT, capo: 0 });

const position = (k) => ({ side: k >= 48 ? 1 : 0, row: k % 4, column: (k % 48) >> 2 });
const sortPitches = (t) => t.sort((a, b) => a - b);
const pc = (n) => ((n % 12) + 12) % 12;

function degree(c, d) { const n = c.steps.length; return 12 * Math.floor(d / n) + c.steps[d % n]; }
const setChord = (c, p) => (c.chords === 'set' ? c.set[p.column % 12] : null);
const fullChord = (c, p) => (c.chords === 'full' ? c.full[(4 * p.column + p.row) % 48] : null);
function setTones(s) { const out = []; for (let b = 0; b < 12; b++) if ((s.mask >> b) & 1) out.push(b); return out; }
// Semitones from the key's root up to the chord's root.
function chordRoot(c, p) {
  const f = fullChord(c, p); if (f) return pc(f.root - c.tonic); // Full roots are pitch classes (tc_pad_root)
  const s = setChord(c, p); return s ? s.root : degree(c, p.column);
}
export function padRoot(c, k) { return pc(c.tonic + chordRoot(c, position(k % 48))); }

// The chord grid's row shape: from the top the 7th and 9th, a sus, the 7th, nothing.
function gridShape(c, p, edit) {
  if (c.chords !== 'grid') return edit;
  const e = { ...edit, add: edit.add | [6, 0, 2, 0][p.row % 4] };
  if (p.row === 1 && !e.sus) {
    const root = degree(c, p.column);
    e.sus = degree(c, p.column + 3) - root !== 5 && degree(c, p.column + 1) - root === 2 ? 2 : 4;
  }
  return e;
}
// Ladder step n's notes (0 root position, 1 first inversion ...; a Full pad raises its lowest note n times).
function tones(c, p, edit, n) {
  const f = fullChord(c, p);
  if (f) {
    const t = f.notes.slice(0, 6).map((x) => x + c.chordBase - 48);
    for (let s = 0; t.length && s < n; s++) { t[0] += 12; sortPitches(t); }
    return t;
  }
  edit = gridShape(c, p, edit);
  const set = setChord(c, p);
  if (set) {
    const semis = setTones(set), m = semis.length, t = [];
    if (!m) return t;
    const inv = n % m, up = 12 * Math.floor(n / m);
    for (let i = 0; i < m; i++) {
      const member = (i + inv) % m;
      let pitch = c.chordBase + c.tonic + set.root + semis[member] + (i + inv >= m ? 12 : 0) + up;
      if (member < 3 && (edit.lower & (1 << member))) pitch -= 12;
      t.push(pitch);
    }
    return t;
  }
  const memberSteps = [0, edit.sus ? edit.sus - 1 : 2, 4], inv = n % 3, up = 12 * Math.floor(n / 3), t = [];
  for (let i = 0; i < 3; i++) {
    const member = (i + inv) % 3;
    let pitch = c.chordBase + c.tonic + degree(c, p.column + memberSteps[member]) + (i + inv >= 3 ? 12 : 0) + up;
    if (edit.lower & (1 << member)) pitch -= 12;
    t.push(pitch);
  }
  [5, 6, 8].forEach((x, i) => { if (edit.add & (1 << i)) t.push(c.chordBase + c.tonic + degree(c, p.column + x) + up); });
  return t;
}
// Ladder step n, also below 0: a whole round higher (3 steps, or one per note for a set or Full chord), an octave down.
function ladder(c, p, edit, n) {
  if (n >= 0) return tones(c, p, edit, n);
  let period = setChord(c, p) || fullChord(c, p) ? tones(c, p, edit, 0).length : 3;
  if (period < 1) period = 1;
  const rounds = Math.floor((-n + period - 1) / period);
  return tones(c, p, edit, n + rounds * period).map((x) => x - 12 * rounds);
}
// Guitar: six strings (E2 A2 D3 G3 B3 E4), a capo, a five-fret window per row; strings below the root muted.
function guitar(root, t, n, capo) {
  const open = [40, 45, 50, 55, 59, 64], from = capo + [0, 3, 5, 7][n % 4], fret = [];
  let classes = 0, bass = 6;
  for (const x of t) classes |= 1 << pc(x);
  for (let s = 0; s < 6; s++) {
    fret[s] = -1;
    for (let f = from; f < from + 5 && fret[s] < 0; f++) if ((classes >> ((open[s] + f) % 12)) & 1) fret[s] = f;
    for (let f = from; f < from + 5 && bass === 6; f++) if ((open[s] + f) % 12 === root) { bass = s; fret[s] = f; }
  }
  const v = [];
  for (let s = bass === 6 ? 0 : bass; s < 6; s++) if (fret[s] >= 0) v.push(open[s] + fret[s]);
  return v;
}
const homeStep = (c, p) => (c.chords === 'full' || c.chords === 'grid' ? 0 : 3 - p.row);

function chordPad(c, p, panel, edit = { lower: 0, sus: 0, add: 0 }) {
  const n = homeStep(c, p);
  let t = ladder(c, p, edit, n);
  if (t.some((x) => x < 0 || x > 127)) return null;
  if (!t.length) return [];
  const octaves = panel.octaves || OCTAVES_DEFAULT;
  if (!panel.voicing && !panel.spread && octaves === OCTAVES_DEFAULT) return t;
  let v;
  if (panel.voicing && panel.voicing < VOICING_GUITAR) v = ladder(c, p, edit, n + panel.voicing);
  else if (panel.voicing === VOICING_GUITAR) v = guitar(pc(c.tonic + chordRoot(c, p)), t, n, panel.capo);
  else v = t.slice();
  const root = pc(c.tonic + chordRoot(c, p));
  if (panel.spread && panel.spreadOf === SPREAD_OF_BASS) {
    let low = 0;
    for (let i = 1; i < v.length; i++) if (v[i] < v[low]) low = i;
    const bass = v[low] - ((v[low] + 120 - root) % 12) - 12 * (panel.spread - 1);
    if (bass !== v[low] && v.length < 16) v.push(bass);
  } else if (panel.spread) {
    let drop = panel.spread;
    if (panel.spreadOf) { sortPitches(v); for (let i = 1; i < v.length; i += 2) v[i] += 12; drop--; }
    let low = -1;
    for (let i = 0; i < v.length; i++) if (pc(v[i]) === root && (low < 0 || v[i] < v[low])) low = i;
    if (low >= 0) v[low] -= 12 * drop;
  }
  const present = new Set();
  for (let o = 0; o < 7; o++) if (octaves & (1 << o)) for (const x of v) { const y = x + 12 * (o - 3); if (y >= 0 && y <= 127) present.add(y); }
  return [...present].sort((a, b) => a - b).slice(0, 16);
}
// The live notes layout's first chord (the key's first chord pad, top row: root position).
function liveChord(c) {
  const p = position(0), t = tones(c, p, { lower: 0, sus: 0, add: 0 }, 0), root = pc(c.tonic + chordRoot(c, p));
  let classes = 1 << root; for (const x of t) classes |= 1 << pc(x);
  const out = []; for (let s = 0; s < 12; s++) if ((classes >> ((root + s) % 12)) & 1) out.push(s);
  return { root, tones: out };
}
function melodyPad(c, p) {
  const up = 3 - p.row, x = p.column, base = c.melodyBase + c.tonic;
  let pitch;
  switch (c.melodyLayout) {
    case 4: {
      if (x < LIVE_COLUMN) { pitch = base + 12 * up + degree(c, x); break; }
      const ch = liveChord(c), n = ch.tones.length || 1, j = x - LIVE_COLUMN;
      pitch = base + (ch.root + 12 - c.tonic % 12) % 12 + 12 * up + ch.tones[j % n] + 12 * Math.floor(j / n);
      while (pitch > 127) pitch -= 12;
      break;
    }
    case 1: pitch = base + x + 12 * up; break;
    case 2: pitch = base + x + 5 * up; break;
    case 3: {
      const n = 4 * up + x % 4, grid = x >> 2;
      pitch = grid === 2 ? DRUM_BASE + n : base + degree(c, n);
      if (grid === 1) { if (pitch < 12) return null; pitch -= 12; }
      break;
    }
    case 5: {
      const at = base + degree(c, 6 * up + (x >> 1));
      if (at < BASS_DROP) return null;
      pitch = at - BASS_DROP; break;
    }
    default: pitch = base + 12 * up + degree(c, x);
  }
  return pitch > 127 ? null : [pitch];
}
// Pad k's notes, low to high: null when it can't play (outside MIDI range), [] for a silent pad (a set's empty column).
export function padNotes(c, k, panel = NEUTRAL) {
  const p = position(k);
  return p.side ? melodyPad(c, p) : chordPad(c, p, panel);
}

// ---- chord names (chord_name.c tn_name) ----
const S = (...a) => a.reduce((m, x) => m | (1 << x), 0);
const SHAPES = [
  [S(0, 4, 7), ''], [S(0, 3, 7), 'm'], [S(0, 3, 6), 'dim'], [S(0, 4, 8), 'aug'], [S(0, 2, 7), 'sus2'], [S(0, 5, 7), 'sus4'],
  [S(0, 4, 7, 11), 'maj7'], [S(0, 4, 7, 10), '7'], [S(0, 3, 7, 10), 'm7'], [S(0, 3, 7, 11), 'mM7'], [S(0, 3, 6, 10), 'm7b5'],
  [S(0, 3, 6, 9), 'dim7'], [S(0, 4, 8, 10), '7#5'], [S(0, 4, 8, 11), 'maj7#5'], [S(0, 4, 6, 10), '7b5'], [S(0, 4, 7, 9), '6'],
  [S(0, 3, 7, 9), 'm6'], [S(0, 5, 7, 10), '7sus4'], [S(0, 2, 7, 10), '7sus2'], [S(0, 5, 7, 11), 'maj7sus4'],
  [S(0, 2, 7, 11), 'maj7sus2'], [S(0, 5, 7, 9), '6sus4'], [S(0, 2, 7, 9), '6sus2'], [S(0, 2, 4, 7), 'add9'],
  [S(0, 2, 3, 7), 'madd9'], [S(0, 4, 5, 7), 'add11'], [S(0, 3, 5, 7), 'madd11'], [S(0, 2, 4, 7, 10), '9'],
  [S(0, 2, 4, 7, 11), 'maj9'], [S(0, 2, 3, 7, 10), 'm9'], [S(0, 2, 3, 7, 11), 'mM9'], [S(0, 2, 4, 7, 9), '6/9'],
  [S(0, 2, 3, 7, 9), 'm6/9'], [S(0, 1, 4, 7, 10), '7b9'], [S(0, 1, 3, 7, 10), 'm7b9'], [S(0, 3, 4, 7, 10), '7#9'],
  [S(0, 2, 3, 6, 10), 'm9b5'], [S(0, 1, 3, 6, 10), 'm7b5b9'], [S(0, 2, 5, 7, 10), '9sus4'], [S(0, 4, 6, 7, 11), 'maj7#11'],
  [S(0, 3, 5, 7, 10), 'm11'], [S(0, 4, 7, 8, 10), '7b13'], [S(0, 2, 4, 7, 9, 10), '13'], [S(0, 2, 4, 7, 9, 11), 'maj13'],
  [S(0, 2, 3, 7, 9, 10), 'm13'],
  [S(0, 4), ''], [S(0, 3), 'm'], [S(0, 7), '5'], // fallback bases only
];
const EXACT = SHAPES.length - 3;
export const PCS = ['C', 'C#', 'D', 'D#', 'E', 'F', 'F#', 'G', 'G#', 'A', 'A#', 'B'];
const EXTRA = ['', 'b9', '9', '#9', '3', '11', '#11', '5', 'b13', '13', '7', 'M7'];
const rotateDown = (set, by) => ((set >> by) | (set << (12 - by))) & 0xFFF;
const count = (m) => { let n = 0; for (; m; m &= m - 1) n++; return n; };
const MAX = 23, SCREEN = 21; // TN_MAX - 1 characters, the display's width
// The name the board's screen gives notes played from a pad whose chord builds on root (a pitch class).
export function chordName(notes, root) {
  let t = '';
  const put = (s) => { t = (t + s).slice(0, MAX); };
  if (!notes || !notes.length || root > 11) return '-';
  let set = 0, low = 0;
  notes.forEach((x, i) => { set |= 1 << (x % 12); if (x < notes[low]) low = i; });
  const bass = notes[low] % 12, order = [];
  if (set & (1 << root)) order.push(root);
  if (bass !== root) order.push(bass);
  for (let p = 0; p < 12; p++) if ((set & (1 << p)) && p !== root && p !== bass) order.push(p);
  for (const r of order) {
    const rel = rotateDown(set, r), s = SHAPES.slice(0, EXACT).find(([shape]) => shape === rel);
    if (s) { put(PCS[r]); put(s[1]); if (bass !== r) { put('/'); put(PCS[bass]); } return t; }
  }
  const relative = rotateDown(set, root) | 1;
  let best = -1, size = 0;
  SHAPES.forEach(([shape], s) => { const n = count(shape); if ((shape & ~relative) === 0 && n > size) { best = s; size = n; } });
  put(PCS[root]);
  let rest = relative & ~1;
  if (best >= 0) { put(SHAPES[best][1]); rest &= ~SHAPES[best][0]; }
  if (rest) put('(' + [10, 11, 1, 2, 3, 4, 5, 6, 7, 8, 9].filter((x) => rest & (1 << x)).map((x) => EXTRA[x]).join(',') + ')');
  if (bass !== root && t.length + (PCS[bass].length > 1 ? 3 : 2) <= SCREEN) { put('/'); put(PCS[bass]); }
  return t;
}
// Full pads from the harness's / full-chords.json's text: "root:n,n,n|..." (48 pads).
export function parseFull(text) {
  return text.split('|').slice(0, 48).map((pad) => {
    const [root, notes] = pad.split(':');
    return { root: +root, notes: notes ? notes.split(',').map(Number) : [] };
  });
}
