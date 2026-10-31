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
const KINDS = [['scale', "Scale's chords"], ['grid', 'Chord grid'], ['full', 'Full'], ['set', `Chord set (${POP.name})`]];
const START = {
  scale: {}, grid: { chords: 'grid' }, full: { chords: 'full' }, set: { chords: 'set' },
  voicing: { voicing: 1, pick: 3 }, guitar: { voicing: 4, pick: 3 }, spread: { spread: 1, pick: 3 },
  melody: { melodyLayout: 1 },
};
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
  const st = { tonic: 0, scale: 0, chords: 'scale', voicing: 0, capo: 0, spread: 0, spreadOf: 0, melodyLayout: 0, melodyOct: 0, pick: 3,
    ...START[root.dataset.start] };
  const controls = el('div', { class: 'pe-controls' }), board = el('div', { class: 'pe-board' }), detail = el('div', { class: 'pe-detail' });
  const note = el('p', { class: 'pe-note' });
  const head = el('div', { class: 'pe-head' }, el('strong', {}, 'Pad explorer'), ': set it up as on your board, then click a pad to hear it.');
  root.replaceChildren(head, controls, board, detail, note);

  const layout = () => ({
    tonic: st.tonic, steps: SCALES.find((s) => s.id === st.scale).steps, chords: st.chords, set: SET,
    full: st.chords === 'full' && fullData ? pads.parseFull(fullData.layouts[st.scale][st.tonic]) : null,
    chordBase: pads.CHORD_BASE, melodyBase: pads.MELODY_BASE + 12 * st.melodyOct, melodyLayout: st.melodyLayout,
  });
  const panel = () => ({ voicing: st.voicing, spread: st.spread, spreadOf: st.spreadOf, octaves: 8, capo: st.capo });

  function drawControls() {
    const set = (key, num = true) => (v) => { st[key] = num ? +v : v; redraw(); };
    controls.replaceChildren(
      select('Root', NOTES.map((n, i) => [i, n]), st.tonic, set('tonic')),
      select('Scale', LIST.map((s) => [s.id, s.name]), st.scale, set('scale')),
      select('Chord pads', KINDS, st.chords, set('chords', false)),
      select('Voicing', pads.VOICINGS.map((v, i) => [i, v]), st.voicing, set('voicing')),
      ...(st.voicing === 4 ? [select('Capo', [[0, 'Off'], ...Array.from({ length: 11 }, (_, i) => [i + 1, `Fret ${i + 1}`])], st.capo, set('capo'))] : []),
      select('Spread', pads.SPREADS.map((v, i) => [i, v]), st.spread, set('spread')),
      ...(st.spread ? [select('Spread of', pads.SPREAD_OFS.map((v, i) => [i, v]), st.spreadOf, set('spreadOf'))] : []),
      select('Melody', pads.MELODY_LAYOUTS.map((v, i) => [i, v]), st.melodyLayout, set('melodyLayout')),
      select('Melody oct', [-2, -1, 0, 1, 2].map((o) => [o, o > 0 ? `+${o}` : String(o)]), st.melodyOct, set('melodyOct')),
    );
  }
  function colorOf(c, k, notes) {
    if (!notes || !notes.length) return 'var(--pe-off)';
    const melody = k >= 48;
    if ((!melody && (st.chords === 'scale' || st.chords === 'grid')) || (melody && st.melodyLayout <= 2))
      return css(lights.padColor(LOOK, { tonic: c.tonic, steps: c.steps, layout: st.melodyLayout }, k, 255, (rgb, level) => rgb.map((x) => Math.round(x * Math.max(level, 110) / 255))));
    const p = melody ? notes[0] % 12 : pads.padRoot(c, k); // Full, sets, drums, live, bass: by the note or the chord's root
    return css(lights.roleRgb(LOOK, p).map((x) => Math.round(x * (p === c.tonic ? 1 : 0.7))));
  }
  function drawBoard(c) {
    const p = panel(), halves = [['Chord pads', 0], ['Melody pads', 48]];
    board.replaceChildren(...halves.map(([title, from]) => {
      const grid = el('div', { class: 'pe-grid', role: 'grid', 'aria-label': title });
      for (let row = 0; row < 4; row++) for (let column = 0; column < 12; column++) {
        const k = from + 4 * column + row, notes = pads.padNotes(c, k, p);
        const label = !notes ? '' : !notes.length ? '–' : k < 48 ? prettyChord(pads.chordName(notes, pads.padRoot(c, k))) : noteName(notes[0]);
        const b = el('button', { class: 'pe-pad' + (k === st.pick ? ' picked' : ''), type: 'button', title: label,
          'aria-label': `${title}, column ${column + 1}, row ${4 - row} from the bottom: ${label || 'nothing'}` }, el('span', {}, label));
        b.style.setProperty('--pad', colorOf(c, k, notes));
        b.onclick = () => { st.pick = k; drawBoard(c); drawDetail(c); if (notes && notes.length) play(notes); };
        grid.append(b);
      }
      return el('div', { class: 'pe-half' }, el('div', { class: 'pe-title' }, title), grid);
    }));
  }
  function keyboard(notes, plain) {
    const lo = 24, hi = 96, white = [0, 2, 4, 5, 7, 9, 11], svg = [], W = 12, H = 64; // C1..C7, units of a white key
    const whites = []; for (let n = lo; n <= hi; n++) if (white.includes(n % 12)) whites.push(n);
    const x = (n) => whites.indexOf(n) * W, kind = (n) => (notes.includes(n) ? ' on' : plain.includes(n) ? ' was' : '');
    whites.forEach((n) => svg.push(`<rect x="${x(n)}" y="0" width="${W}" height="${H}" class="w${kind(n)}"/>`));
    for (let n = lo; n <= hi; n++) if (!white.includes(n % 12)) svg.push(`<rect x="${x(n - 1) + W * 0.65}" y="0" width="${W * 0.7}" height="${H * 0.6}" class="b${kind(n)}"/>`);
    whites.filter((n) => n % 12 === 0).forEach((n) => svg.push(`<text x="${x(n) + W / 2}" y="${H - 6}">C${n / 12 - 1}</text>`));
    return `<svg class="pe-keys" viewBox="0 0 ${whites.length * W} ${H}" role="img" aria-label="Keyboard, C1 to C7">${svg.join('')}</svg>`;
  }
  function drawDetail(c) {
    const k = st.pick, notes = pads.padNotes(c, k, panel()) || [], plain = k < 48 ? pads.padNotes(c, k) || [] : notes;
    const column = ((k % 48) >> 2) + 1, row = 4 - (k % 4), half = k < 48 ? 'Chord pad' : 'Melody pad';
    const name = !notes.length ? 'silent' : k < 48 ? prettyChord(pads.chordName(notes, pads.padRoot(c, k))) : noteName(notes[0]);
    const changed = k < 48 && plain.join() !== notes.join();
    detail.innerHTML = `<div class="pe-what"><strong>${half} ${column}, row ${row}</strong> (from the bottom): <strong>${name}</strong> · ` +
      `${notes.map(noteName).join(' ') || '–'}${changed ? ` <span class="pe-was">without the panel: ${plain.map(noteName).join(' ')}</span>` : ''}</div>` +
      keyboard(notes, changed ? plain : []);
  }
  function redraw() {
    if (st.chords === 'full' && !fullData) {
      note.textContent = 'Loading the Full layout…';
      loadFull().then(() => redraw()).catch(() => { note.textContent = 'The Full layout could not be loaded.'; });
      return;
    }
    const c = layout();
    drawControls(); drawBoard(c); drawDetail(c);
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
