// SPDX-License-Identifier: GPL-3.0-or-later - the editor's Lights tab (TBOSS 1.9.0, Customize Stage 2): the pads'
// color scheme, Color by, every role's color from the 16 of the palette, Keep look, and a picture of the board
// at rest. Each change goes to the board at once as a quiet record of the look on show (kind 8, slot 9: no
// reply, the board keeps playing); the model and record are in tb-lights.js.
import * as tb from './tb-protocol.js';
import * as lights from './tb-lights.js';

const READ_AGAIN_MS = 3000;
// Every LED color as it looks lit (tb-lights.js onScreen: the board's numbers are light, a screen's are not).
const css = (rgb) => { const [r, g, b] = lights.onScreen(rgb); return `rgb(${r}, ${g}, ${b})`; };
// The picture draws each pad in exactly the color picked for it (the same as its swatch below), with a glow at full
// brightness. A dim pad (10%, outside the scale) is striped: its color and black, 3 px each.
const level = (rgb, at) => Object.assign([...rgb], { at });
const striped = (c) => `repeating-linear-gradient(45deg, ${c} 0 3px, #000 3px 6px)`;
const NOTES = ['C', 'C#', 'D', 'D#', 'E', 'F', 'F#', 'G', 'G#', 'A', 'A#', 'B'];
const MELODY = ['Scale', 'Chromatic', 'Fourths']; // the layouts the picture draws (tb-lights.js padColor)
const DEGREES = ['1 (root)', '♭2', '2', '♭3', '3', '4', '♭5', '5', '♭6', '6', '♭7', '7']; // Default's notes, Color by Scale degree

// root: the tab's element; link: tb-midi.js caller(); info(): IDENTIFY's answer; stepsOf(id): a scale's semitones.
export function lightsTab(root, link, info, stepsOf) {
  // view: the picture's key and melody layout, '' = as the TB is now (only the picture changes).
  const st = { look: null, board: null, view: { tonic: '', layout: '' }, readAt: 0, timer: 0, busy: false };
  root.innerHTML = `
    <p class="hint">How the pads light: pick a scheme, then any of its colors. The pads follow at once while you choose
      (the TB keeps playing). On the TB: Settings → Lights → Colors and Color by. Each scheme keeps its own colors;
      <b>Built-in</b> gives a color back.</p>
    <p class="hint hidden" data-role="old">Connect a TB with TBOSS 1.9 or later to change its colors here.</p>
    <div class="row"><button data-role="read" disabled>Read from the TB</button><span class="status" data-role="status"></span></div>
    <div data-role="body" class="hidden" data-said="look">
      <p class="facts" data-role="whose"></p>
      <div class="schemes" data-role="schemes" role="radiogroup" aria-label="Color scheme"></div>
      <div class="row"><label>Show the pads in <select data-role="viewKey"></select></label>
        <label>Melody pads <select data-role="viewLayout"></select></label></div>
      <div class="board-picture" data-role="picture" aria-label="The pads at rest"></div>
      <p class="facts">The pads as the TB lights them in that key; another key or melody layout changes only this picture.
        With the melody pads in Scale, every pad plays a note of the scale (in C major, Piano's black keys never light).
        Each pad is in exactly its color below; glowing ones are at full brightness, the others a little less. Striped pads
        are lit dimly (notes outside the scale, with the melody pads in Chromatic or Fourths).</p>
      <div class="row"><label>Color by <select data-role="colorBy"></select></label>
        <span class="facts">(Default's pads, and in every scheme the theory hints and progression guide; in C both give
          the same colors)</span>
        <label><input type="checkbox" data-role="keep"> Keep look: Save preset also saves these colors with the preset</label></div>
      <h3 data-role="rolesTitle"></h3>
      <div class="roles" data-role="roles"></div>
      <div class="row"><button data-role="builtin">All built-in</button></div>
      <div class="status said pinned"></div>
    </div>`;
  const el = (role) => root.querySelector(`[data-role="${role}"]`);
  const say = tb.sayer(root, () => el('status')); // 2.0.6: a change's message at the foot of the colors, in sight
  const supported = () => !!info() && tb.settingsSupported(info().version);
  lights.COLOR_BY.forEach((n, i) => el('colorBy').add(new Option(n, i)));
  NOTES.forEach((n, i) => el('viewKey').add(new Option(n, i)));
  MELODY.forEach((n, i) => el('viewLayout').add(new Option(n, i)));
  el('viewKey').onchange = (e) => { st.view.tonic = e.target.value; drawPicture(); };
  el('viewLayout').onchange = (e) => { st.view.layout = e.target.value; drawPicture(); };

  function send() { // the look on show, a moment after the last click
    clearTimeout(st.timer); st.editedAt = Date.now();
    st.timer = setTimeout(() => {
      link.send(tb.setRecordQuiet(tb.KIND.LOOK, lights.LOOK_NOW, lights.lookToWords(st.look)))
        .then(() => say('Sent to the TB.', 'good'), (e) => say(`Not sent: ${e.message}.`, 'err'));
    }, 120);
  }
  function change(f) { f(st.look); draw(); send(); }
  function drawPicture() {
    const pic = el('picture'); pic.innerHTML = '';
    const tbLayout = st.board.layout <= 2 ? st.board.layout : 0;
    for (const [role, now, label] of [['viewKey', st.board.tonic, NOTES], ['viewLayout', tbLayout, MELODY]]) {
      const sel = el(role); sel.options[now].text = `${label[now]} (the TB now)`;
      for (let i = 0; i < sel.options.length; i++) if (i !== now) sel.options[i].text = label[i];
    }
    const board = { ...st.board, tonic: st.view.tonic === '' ? st.board.tonic : +st.view.tonic,
      layout: st.view.layout === '' ? tbLayout : +st.view.layout };
    el('viewKey').value = String(board.tonic); el('viewLayout').value = String(board.layout);
    for (const side of [0, 1]) {
      const half = document.createElement('div'); half.className = 'half';
      for (let row = 0; row < 4; row++) for (let column = 0; column < 12; column++) {
        const k = side * 48 + column * 4 + row, pad = document.createElement('span');
        const c = lights.padColor(st.look, board, k, 255, level);
        if (c.at < 64) pad.style.background = striped(css(c)); else {
          pad.style.background = css(c); pad.style.color = css(c); if (c.at === 255 && c.some((x) => x > 40)) pad.className = 'full';
        }
        half.appendChild(pad);
      }
      pic.appendChild(half);
    }
  }
  function drawRoles() {
    const scheme = lights.SCHEMES[st.look.scheme], list = el('roles');
    el('rolesTitle').textContent = `${scheme.name}: its colors`;
    list.innerHTML = '';
    scheme.roles.forEach((named, i) => {
      const role = st.look.scheme === 0 && st.look.colorBy ? { ...named, name: DEGREES[i] } : named;
      const at = scheme.first + i, row = document.createElement('div'); row.className = 'role';
      const name = document.createElement('span'); name.className = 'name';
      const now = document.createElement('i'); now.className = 'swatch'; now.style.background = css(lights.roleRgb(st.look, at));
      name.append(now, ` ${role.name}`);
      const picks = document.createElement('div'); picks.className = 'picks';
      const built = document.createElement('button'); built.textContent = 'Built-in'; built.className = 'builtin';
      built.setAttribute('aria-pressed', String(!st.look.roles[at])); built.title = `${role.name}: its built-in color`;
      built.style.setProperty('--c', css(role.rgb));
      built.onclick = () => change((l) => { l.roles[at] = 0; });
      picks.appendChild(built);
      lights.PALETTE.forEach((p, v) => {
        const b = document.createElement('button'); b.className = 'swatch'; b.style.background = css(p.rgb);
        b.title = `${role.name}: ${p.name}`; b.setAttribute('aria-label', b.title); b.setAttribute('aria-pressed', String(st.look.roles[at] === v + 1));
        b.onclick = () => change((l) => { l.roles[at] = v + 1; });
        picks.appendChild(b);
      });
      row.append(name, picks); list.appendChild(row);
    });
  }
  function draw() {
    el('body').classList.toggle('hidden', !st.look);
    if (!st.look) return;
    el('whose').textContent = st.look.showing ? `Showing preset ${st.look.showing}'s own colors: changes go to that preset.`
      : "Showing the board's colors (every preset without colors of its own).";
    const schemes = el('schemes'); schemes.innerHTML = '';
    lights.SCHEMES.forEach((s, i) => {
      const b = document.createElement('button'); b.setAttribute('role', 'radio'); b.setAttribute('aria-checked', String(i === st.look.scheme));
      b.innerHTML = `<b></b><small></small>`; b.querySelector('b').textContent = s.name; b.querySelector('small').textContent = s.about;
      b.onclick = () => change((l) => { l.scheme = i; });
      schemes.appendChild(b);
    });
    el('colorBy').value = String(st.look.colorBy);
    el('keep').checked = !!st.look.keep;
    drawPicture(); drawRoles();
  }
  el('colorBy').onchange = (e) => change((l) => { l.colorBy = +e.target.value; });
  el('keep').onchange = (e) => change((l) => { l.keep = e.target.checked ? 1 : 0; });
  el('builtin').onclick = () => change((l) => { const s = lights.SCHEMES[l.scheme]; for (let i = 0; i < s.roles.length; i++) l.roles[s.first + i] = 0; });
  async function read(quiet = false) {
    if (!supported() || st.busy) return;
    st.busy = true; el('read').disabled = true; if (!quiet) say('Reading the TB\'s colors…');
    const before = JSON.stringify([st.look, st.board]);
    try {
      const look = lights.lookFromWords(tb.decodeRecord(await link.ok(tb.getRecord(tb.KIND.LOOK, lights.LOOK_NOW))).words);
      const live = tb.decodeRecord(await link.ok(tb.getRecord(tb.KIND.LIVE, 0))).words;
      const tonic = live[0] & 255, scale = (live[0] >>> 8) & 255, layout = (live[24] >>> 16) & 255;
      st.board = { tonic, steps: stepsOf(scale) || [0, 2, 4, 5, 7, 9, 11], layout };
      st.look = look; st.readAt = Date.now(); if (!quiet) say('');
    } catch (e) { if (!quiet) say(`Could not read the colors: ${e.message}.`, 'err'); }
    st.busy = false; el('read').disabled = !supported();
    if (!quiet || JSON.stringify([st.look, st.board]) !== before) draw();
  }
  el('read').onclick = read;
  return {
    async connected() {
      el('old').classList.toggle('hidden', supported() || !info());
      el('read').disabled = !supported();
      st.look = null; draw();
      if (supported()) await read();
    },
    shown() { if (supported() && Date.now() - st.readAt > READ_AGAIN_MS) read(); },
    // 2.0.6: refresh(), a quiet read when the board changed (STATE), unless something from here is on its way (board test Y6)
    async refresh() { if (supported() && st.look && Date.now() - (st.editedAt || 0) > 1500) await read(true); },
  };
}
