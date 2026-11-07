// SPDX-License-Identifier: GPL-3.0-or-later - the manual's pad explorer: what the 96 pads play for a root, a scale, the
// chord pads' kind (the scale's chords, the grid, Full, a chord set), the chord panel's voicing and spread, and the
// melody layout. Notes and names come from tb-pads.js (checked against the firmware); colors from tb-lights.js (the
// board's default look). scripts/build_site.py turns each "<!-- pad explorer: start -->" line of the manual into
// <div class="pad-explorer" data-start="start">; this fills every one in.
import * as pads from './tb-pads.js';
import { SCALES } from './tb-scales.js';
import * as lights from './tb-lights.js';
import { parseChord } from './tb-chords.js';

const NOTES = ['C', 'C♯', 'D', 'E♭', 'E', 'F', 'F♯', 'G', 'A♭', 'A', 'B♭', 'B'];
const IDS = [...Array(12).keys(), ...Array.from({ length: 13 }, (_, i) => 24 + i)];
const LIST = IDS.map((id) => SCALES.find((s) => s.id === id)).filter(Boolean);
const POP = { name: 'Pop', text: 'C Dm Em F G Am Bb Ab Fm D7 E7 G7' };
const SET = POP.text.split(' ').map((n) => { const c = parseChord(n); return { mask: c.set, root: c.root }; });
const KINDS = [['scale', "Scale's chords"], ['grid', 'Chord grid'], ['full', 'Full'], ['modes', 'Modes'], ['set', `Chord set (${POP.name})`]];
const START = {
  scale: {}, grid: { chords: 'grid' }, full: { chords: 'full' }, set: { chords: 'set' }, modes: { chords: 'modes' },
  voicing: { voicing: 1, pick: 3 }, guitar: { voicing: 4, pick: 3 }, spread: { spread: 1, pick: 3 },
  melody: { melodyLayout: 1 },
  lead: { voiceLead: 1 }, fold: { fold: 1, tonic: 5, scale: 7 }, edits: {},
};
const SUS = [[0, 'Off'], [2, 'Sus2'], [4, 'Sus4']], ADDS = [[pads.ADD7, '7th'], [pads.ADD9, '9th'], [pads.ADD6, '6th']];
const PLAIN = { lower: 0, sus: 0, add: 0 };
const LOOK = { scheme: 0, colorBy: 0, keep: 0, roles: new Array(lights.ROLES).fill(0) }; // the board's default look
const noteName = (n) => `${pads.PCS[n % 12].replace('#', '♯')}${Math.floor(n / 12) - 1}`;
const prettyChord = (s) => s.replace(/#/g, '♯').replace(/b(?=\d)/g, '♭'); // the screen's C#m7b5 as C♯m7♭5
const css = (rgb) => `rgb(${lights.onScreen(rgb).join(',')})`;

let fullData = null, fullLoading = null;
function loadFull() {
  fullLoading ||= fetch('full-chords.json').then((r) => r.json()).then((d) => { fullData = d; return d; });
  return fullLoading;
}
let audio = null;
function play(notes) {
  try {
    audio ||= new AudioContext();
    const now = audio.currentTime, out = audio.createGain();
    out.gain.value = 0.18 / Math.max(1, Math.sqrt(notes.length)); out.connect(audio.destination);
    notes.forEach((n, i) => {
      const osc = audio.createOscillator(), env = audio.createGain(), at = now + i * 0.012;
      osc.type = 'triangle'; osc.frequency.value = 440 * 2 ** ((n - 69) / 12);
      env.gain.setValueAtTime(0, at); env.gain.linearRampToValueAtTime(1, at + 0.01);
      env.gain.exponentialRampToValueAtTime(0.001, at + 1.6);
      osc.connect(env).connect(out); osc.start(at); osc.stop(at + 1.7);
    });
  } catch { /* no audio here: the picture still says it */ }
}

function el(tag, attrs = {}, ...kids) {
  const e = document.createElement(tag);
  for (const [k, v] of Object.entries(attrs)) if (k === 'class') e.className = v; else if (k.startsWith('on')) e[k] = v; else e.setAttribute(k, v);
  for (const k of kids) e.append(k);
  return e;
}
function select(label, options, value, onchange) {
  const s = el('select', { 'aria-label': label });
  options.forEach(([v, text]) => s.add(new Option(text, v)));
  s.value = String(value); s.onchange = () => onchange(s.value);
  return el('label', {}, el('span', {}, label), s);
}

function explorer(root) {
  const st = { tonic: 0, scale: 0, chords: 'scale', voicing: 0, capo: 0, spread: 0, spreadOf: 0, chordOct: 0, melodyLayout: 0, melodyOct: 0, pick: 3, live: 0,
    fold: 0, voiceLead: 0, edits: [], last: null, lead: null, leadFrom: '', heard: null, refused: false,
    rows: [1, 5, 2], follow: 0, melodyRow: 0, ...START[root.dataset.start] }; // 2.3, Modes: rows 2..4, Mel follows
  const controls = el('div', { class: 'pe-controls' }), edits = el('div', { class: 'pe-edits' }), board = el('div', { class: 'pe-board' }), detail = el('div', { class: 'pe-detail' });
  const note = el('p', { class: 'pe-note' });
  const head = el('div', { class: 'pe-head' }, el('strong', {}, 'Pad explorer'), ': set it up as on your board, then click a pad to hear it.');
  root.replaceChildren(head, controls, edits, board, detail, note);

  const layout = () => ({
    tonic: st.tonic, steps: SCALES.find((s) => s.id === st.scale).steps, chords: st.chords, set: SET,
    full: st.chords === 'full' && fullData ? pads.parseFull(fullData.layouts[st.scale][st.tonic]) : null,
    chordBase: pads.CHORD_BASE, melodyBase: pads.MELODY_BASE + 12 * st.melodyOct, melodyLayout: st.melodyLayout,
    livePad: st.live, // Live notes: the chord pad clicked last, as the board takes the chord played last
    rowSteps: st.rows.map((id) => SCALES.find((s) => s.id === id).steps), follow: !!st.follow, melodyRow: st.follow ? st.melodyRow : 0,
    fold: !!st.fold, voiceLead: !!st.voiceLead, edits: st.edits, lead: st.lead, // the next chord leads from the last one
  });
  // The chord panel; Chord oct is one lit Octave button (-3..+3). Live notes take only the chord's note names (tc_live_of_pad),
  // so they stay in the melody octave whatever the chord octave, as on the board.
  const panel = () => ({ voicing: st.voicing, spread: st.spread, spreadOf: st.spreadOf, octaves: 1 << (st.chordOct + 3), capo: st.capo });
  const octaves = (n) => Array.from({ length: 2 * n + 1 }, (_, i) => i - n).map((o) => [o, o > 0 ? `+${o}` : String(o)]);

  function drawControls() {
    const set = (key, num = true) => (v) => { st[key] = num ? +v : v; st.heard = null; st.refused = false; redraw(); };
    controls.replaceChildren(
      select('Root', NOTES.map((n, i) => [i, n]), st.tonic, set('tonic')),
      select('Scale', LIST.map((s) => [s.id, s.name]), st.scale, set('scale')),
      select('Chord pads', KINDS, st.chords, set('chords', false)),
      ...(st.chords === 'modes' ? [0, 1, 2].map((r) => select(`Row ${r + 2}`, LIST.map((s) => [s.id, s.name]), st.rows[r], (v) => {
        st.rows = st.rows.slice(); st.rows[r] = +v; st.heard = null; redraw();
      })).concat([select('Mel follows', [[0, 'Main'], [1, 'Last row']], st.follow, (v) => { st.follow = +v; st.melodyRow = 0; st.heard = null; redraw(); })]) : []),
      select('Voicing', pads.VOICINGS.map((v, i) => [i, v]), st.voicing, set('voicing')),
      ...(st.voicing === 4 ? [select('Capo', [[0, 'Off'], ...Array.from({ length: 11 }, (_, i) => [i + 1, `Fret ${i + 1}`])], st.capo, set('capo'))] : []),
      select('Spread', pads.SPREADS.map((v, i) => [i, v]), st.spread, set('spread')),
      ...(st.spread ? [select('Spread of', pads.SPREAD_OFS.map((v, i) => [i, v]), st.spreadOf, set('spreadOf'))] : []),
      select('Chord oct', octaves(3), st.chordOct, set('chordOct')),
      select('Voice lead', [[0, 'Off'], [1, 'On']], st.voiceLead, set('voiceLead')),
      select('Fold', [[0, 'Off'], [1, 'On']], st.fold, set('fold')),
      select('Melody', pads.MELODY_LAYOUTS.map((v, i) => [i, v]), st.melodyLayout, set('melodyLayout')),
      select('Melody oct', octaves(2), st.melodyOct, set('melodyOct')),
    );
  }
  // Chord edits (Settings > Chords) on the chord pad played last, as on the board: only the scale's chords take them.
  function drawEdits(c) {
    const k = st.last;
    if (st.chords !== 'scale' && st.chords !== 'modes') { edits.replaceChildren(el('span', { class: 'pe-hint' }, 'Chord edits: a chord set, the grid and Full play their chords as they are, so Add and Sus don’t apply.')); return; }
    if (k === null) { edits.replaceChildren(el('span', { class: 'pe-hint' }, 'Chord edits: click a chord pad, then add a 7th, 9th or 6th, or make it sus.')); return; }
    const e = st.edits[k] || PLAIN, notes = pads.padNotes({ ...c, voiceLead: false }, k) || [];
    const name = notes.length ? prettyChord(pads.chordName(notes, pads.padRoot(c, k))) : '';
    const change = (edit) => {
      st.heard = null;
      st.refused = !pads.editFits(c, k, edit); // the board refuses an edit that leaves MIDI range: OUT OF RANGE
      if (!st.refused) { st.edits = st.edits.slice(); st.edits[k] = edit; }
      redraw();
    };
    const toggle = ([bit, label]) => el('button', { type: 'button', class: 'pe-toggle', 'aria-pressed': String(!!(e.add & bit)),
      onclick: () => change({ ...e, add: e.add ^ bit }) }, label);
    const edited = st.edits.some((x) => x && (x.add || x.sus || x.lower));
    edits.replaceChildren(
      el('span', { class: 'pe-hint' }, `Chord edits on chord pad ${((k % 48) >> 2) + 1}, row ${4 - (k % 4)}${name ? ` (${name})` : ''}:`),
      ...ADDS.map(toggle),
      select('Sus', SUS, e.sus, (v) => change({ ...e, sus: +v })),
      el('button', { type: 'button', class: 'pe-toggle', onclick: () => change(PLAIN) }, 'Reset chord'),
      ...(edited ? [el('button', { type: 'button', class: 'pe-toggle', onclick: () => { st.edits = []; st.heard = null; st.refused = false; redraw(); } }, 'Reset all edits')] : []),
      ...(st.refused ? [el('span', { class: 'pe-hint pe-err' }, 'OUT OF RANGE: a note would leave MIDI, so the board doesn’t make it.')] : []),
    );
  }
  // As the editor's Lights picture draws them: each pad in exactly its color, glowing at full brightness, striped with
  // black when dim (10 %, outside the scale). Full, sets, drums, live notes and bass: the note's or the chord root's color.
  function colorOf(c, k, notes) {
    if (!notes || !notes.length) return { bg: 'var(--pe-off)', glow: false };
    const melody = k >= 48, e = c.edits[k % 48];
    if (!melody && st.chords === 'modes') { // 2.3: the chord root's color; rows 2..4's chords the key also has dim
      const color = css(lights.roleRgb(LOOK, pads.padRoot(c, k)).map((x) => (e && (e.add || e.sus || e.lower) ? (x + 255) >> 1 : x)));
      const dim = (k % 4) && !pads.borrowed(c, k);
      return dim ? { bg: `repeating-linear-gradient(45deg, ${color} 0 3px, #000 3px 4.5px)`, glow: false, color } : { bg: color, glow: pads.padRoot(c, k) === c.tonic, color };
    }
    if (!melody && e && (e.add || e.sus || e.lower) && (st.chords === 'scale' || st.chords === 'set')) { // edited: paler, full
      const color = css(lights.roleRgb(LOOK, pads.padRoot(c, k)).map((x) => (x + 255) >> 1));
      return { bg: color, glow: true, color };
    }
    if ((!melody && (st.chords === 'scale' || st.chords === 'grid')) || (melody && st.melodyLayout <= 2)) {
      const rgb = lights.padColor(LOOK, { tonic: c.tonic, steps: melody ? pads.melodySteps(c) : c.steps, layout: st.melodyLayout }, k, 255, (x, at) => Object.assign([...x], { at }));
      const color = css(rgb);
      return rgb.at < 64 ? { bg: `repeating-linear-gradient(45deg, ${color} 0 3px, #000 3px 4.5px)`, glow: false, color } : { bg: color, glow: rgb.at === 255, color };
    }
    const p = melody ? notes[0] % 12 : pads.padRoot(c, k), color = css(lights.roleRgb(LOOK, p));
    return { bg: color, glow: p === c.tonic, color };
  }
  function drawBoard(c) {
    const p = panel(), halves = [['Chord pads', 0], ['Melody pads', 48]];
    board.replaceChildren(...halves.map(([title, from]) => {
      const grid = el('div', { class: 'pe-grid', role: 'grid', 'aria-label': title });
      for (let row = 0; row < 4; row++) for (let column = 0; column < 12; column++) {
        const k = from + 4 * column + row, notes = pads.padNotes(c, k, p);
        const label = !notes ? '' : !notes.length ? '–' : k < 48 ? prettyChord(pads.chordName(notes, pads.padRoot(c, k))) : noteName(notes[0]);
        const look = colorOf(c, k, notes);
        const b = el('button', { class: 'pe-pad' + (k === st.pick ? ' picked' : '') + (look.glow ? ' glow' : ''), type: 'button', title: label,
          'aria-label': `${title}, column ${column + 1}, row ${4 - row} from the bottom: ${label || 'nothing'}` }, el('span', {}, label));
        b.style.setProperty('--pad-bg', look.bg);
        if (look.color) b.style.setProperty('--pad', look.color);
        b.onclick = () => {
          st.pick = k; st.refused = false;
          st.heard = { notes: notes || [], plain: k < 48 ? pads.padNotes({ ...c, voiceLead: false }, k) || [] : notes || [], led: ledFrom(c, k) };
          if (k < 48) { // the live notes, voice leading and chord edits follow the chord played, as on the board
            const next = pads.strike(c, k, p);
            if (next !== c) {
              st.live = next.livePad; st.last = k; st.melodyRow = next.melodyRow || 0;
              if (next.lead !== c.lead) { st.lead = next.lead; st.leadFrom = label; }
            }
          }
          const now = layout(); drawEdits(now); drawBoard(now); drawDetail(now);
          if (notes && notes.length) play(notes);
        };
        grid.append(b);
      }
      return el('div', { class: 'pe-half' }, el('div', { class: 'pe-title' }, title), grid);
    }));
  }
  function keyboard(notes, plain) {
    const all = [...notes, ...plain], white = [0, 2, 4, 5, 7, 9, 11], svg = [], W = 12, H = 64; // units of a white key
    const lo = Math.min(24, ...all.map((n) => n - n % 12)), hi = Math.max(96, ...all.map((n) => n - n % 12 + 12)); // C1..C7 or wider
    const whites = []; for (let n = lo; n <= hi; n++) if (white.includes(n % 12)) whites.push(n);
    const x = (n) => whites.indexOf(n) * W, kind = (n) => (notes.includes(n) ? ' on' : plain.includes(n) ? ' was' : '');
    whites.forEach((n) => svg.push(`<rect x="${x(n)}" y="0" width="${W}" height="${H}" class="w${kind(n)}"/>`));
    for (let n = lo; n <= hi; n++) if (!white.includes(n % 12)) svg.push(`<rect x="${x(n - 1) + W * 0.65}" y="0" width="${W * 0.7}" height="${H * 0.6}" class="b${kind(n)}"/>`);
    whites.filter((n) => n % 12 === 0).forEach((n) => svg.push(`<text x="${x(n) + W / 2}" y="${H - 6}">C${n / 12 - 1}</text>`));
    return `<svg class="pe-keys" viewBox="0 0 ${whites.length * W} ${H}" role="img" aria-label="Keyboard, C${lo / 12 - 1} to C${hi / 12 - 1}">${svg.join('')}</svg>`;
  }
  // The chord a chord pad is voice led from now ('' when it isn't: Voice lead off, the guitar voicing, nothing played yet).
  const ledFrom = (c, k) => (k < 48 && c.voiceLead && st.voicing !== 4 && c.lead ? st.leadFrom : '');
  // The picked pad: as it played when clicked (voice leading took it from the chord before), else as it would play now.
  function drawDetail(c) {
    const k = st.pick, h = st.heard, led = h ? h.led : ledFrom(c, k);
    const notes = h ? h.notes : pads.padNotes(c, k, panel()) || [], plain = h ? h.plain : k < 48 ? pads.padNotes({ ...c, voiceLead: false }, k) || [] : notes;
    const column = ((k % 48) >> 2) + 1, row = 4 - (k % 4), half = k < 48 ? 'Chord pad' : 'Melody pad';
    const name = !notes.length ? 'silent' : k < 48 ? prettyChord(pads.chordName(notes, pads.padRoot(c, k))) : noteName(notes[0]);
    const changed = k < 48 && plain.join() !== notes.join();
    detail.innerHTML = `<div class="pe-what"><strong>${half} ${column}, row ${row}</strong> (from the bottom): <strong>${name}</strong> · ` +
      `${notes.map(noteName).join(' ') || '–'}${changed ? ` <span class="pe-was">without the panel${led ? ' and voice leading' : ''}: ${plain.map(noteName).join(' ')}</span>` : ''}` +
      `${led ? ` <span class="pe-was">(led from ${led})</span>` : ''}</div>` +
      keyboard(notes, changed ? plain : []);
  }
  function redraw() {
    if (st.chords === 'full' && !fullData) {
      note.textContent = 'Loading the Full layout…';
      loadFull().then(() => redraw()).catch(() => { note.textContent = 'The Full layout could not be loaded.'; });
      return;
    }
    const c = layout();
    drawControls(); drawEdits(c); drawBoard(c); drawDetail(c);
    const full = st.chords === 'full' ? (fullData.layouts[st.scale][st.tonic].endsWith('O')
      ? 'Full: the original TheoryBoard\'s chords and voicings for this scale, as TBOSS plays them. '
      : 'Full: this scale has no chords of its own in the original\'s records, so TBOSS builds them, as here. ') : '';
    note.textContent = `${full}Notes as the screen names them (C4 = middle C); the colors follow the board's default lights.`;
  }
  redraw();
}

function start() {
  const all = document.querySelectorAll('.pad-explorer');
  if (!('IntersectionObserver' in window)) { all.forEach(explorer); return; }
  const seen = new IntersectionObserver((entries) => entries.forEach((e) => {
    if (e.isIntersecting) { seen.unobserve(e.target); explorer(e.target); }
  }), { rootMargin: '400px' });
  all.forEach((e) => seen.observe(e));
}
if (document.readyState === 'loading') document.addEventListener('DOMContentLoaded', start); else start();
