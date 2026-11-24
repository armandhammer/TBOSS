// SPDX-License-Identifier: GPL-3.0-or-later - the Chords tab's chord pad edits (TBOSS 1.9.3, Customize Stage 4): all 48
// chord pads at once, each with its chord's name as the TB's screen shows it, and its edits: add the 7th, 9th or 6th,
// sus2 or sus4, its strum off, its sound off (what Settings → Chords → Add 7th … Sound do for the last chord played).
// Changes go to the board with SET_EDITS (no reply: the board keeps playing; it takes all of them or none), and
// the names are read again a moment after the last change (CHORD_NAMES; before 1.9.4 the board paused its notes for that).
import * as tb from './tb-protocol.js';

const NAMES_AFTER_MS = 900;
const SUS = [0, 2, 4], SUS_NAMES = ['', 'sus2', 'sus4'];
// Many pads at once (the whole board, a row or a column): add or remove the 7th, 9th or 6th, all three (a 13th chord's
// notes: the 6th is the 13th an octave down), or no added notes. A pad k is column k / 4, row k % 4 from the top.
const ADD6 = 1, ADD7 = 2, ADD9 = 4, ADD13 = ADD6 | ADD7 | ADD9;
const BULK = [['add7', '7th', ADD7], ['add9', '9th', ADD9], ['add6', '6th', ADD6], ['add13', '13th (6th + 7th + 9th)', ADD13]];
export function padsIn(scope) { // '' every pad, 'r0'..'r3' a row from the top, 'c0'..'c11' a column from the left
  const all = Array.from({ length: 48 }, (_, k) => k);
  if (scope[0] === 'r') return all.filter((k) => k % 4 === +scope.slice(1));
  if (scope[0] === 'c') return all.filter((k) => Math.floor(k / 4) === +scope.slice(1));
  return all;
}
// The edits with bits added to every pad in pads, or taken off them all when every one has them already.
export function withAdded(edits, pads, bits) {
  const every = pads.every((k) => (edits[k].add & bits) === bits);
  return edits.map((e, k) => (pads.includes(k) ? { ...e, add: every ? e.add & ~bits : e.add | bits } : e));
}
export function withoutAdded(edits, pads) { return edits.map((e, k) => (pads.includes(k) ? { ...e, add: 0, sus: 0 } : e)); }

// root: the section's element; link: tb-midi.js caller(); info(): IDENTIFY's answer.
export function padsTab(root, link, info) {
  const st = { pads: null, names: null, stale: false, chordSet: 0, edits: 0, timer: 0, namesTimer: 0, busy: false };
  root.innerHTML = `
    <p class="hint">Each chord pad's edits, all 48 at once: click <b>7</b>, <b>9</b>, <b>6</b> to add that note, <b>sus</b>
      for sus2 then sus4, <b>strum</b> to keep the pad from strumming, <b>sound</b> to silence it. The TB takes each change
      at once and keeps playing; the chord names follow a moment later. On the TB: Settings → Chords → <i>Add 7th</i> …
      <i>Sound</i> edit the last chord played. Presets keep the edits. <b>Many at once</b> adds a note to every chord pad, a
      row or a column (again: takes it off them all); <b>13th</b> adds the 6th, 7th and 9th together, a 13th chord's notes.</p>
    <div class="row"><button data-role="read" disabled>Read from the TB</button><button data-role="plain" disabled>All plain</button>
      <span class="status" data-role="status"></span></div>
    <div class="row" data-role="bulk"><label>Many at once <select data-role="scope" aria-label="Which chord pads">
      <option value="">all 48 chord pads</option>
      ${[0, 1, 2, 3].map((r) => `<option value="r${r}">row ${r + 1} from the top</option>`).join('')}
      ${Array.from({ length: 12 }, (_, c) => `<option value="c${c}">column ${c + 1}</option>`).join('')}
      </select></label>
      ${BULK.map(([role, label]) => `<button data-role="${role}" disabled>Add ${label}</button>`).join('')}
      <button data-role="none" disabled>No added notes</button></div>
    <p class="hint note hidden" data-role="setNote"><b>A chord set, the chord grid or the Full layout plays now</b>
      (Settings → Chords → <i>Chord set</i>): its chords are as typed, so <b>7</b>, <b>9</b>, <b>6</b> and <b>sus</b> are
      off here; only <b>strum</b> and <b>sound</b> apply. They come back with <i>Chord set</i> Scale.</p>
    <div class="pad-scroll"><div class="chord-pads" data-role="pads" aria-label="The chord pads, as on the board"></div></div>`;
  const el = (role) => root.querySelector(`[data-role="${role}"]`);
  const say = (text, kind = '') => { const s = el('status'); s.textContent = text; s.className = 'status ' + kind; };
  const supported = () => !!info(); // a TB connected (2.8.5: no version checks)

  async function readNames() {
    try { st.names = tb.decodeChordNames(await link.ok(tb.chordNames())); st.stale = false; }
    catch (e) { say(`Could not read the chord names: ${e.message}.`, 'err'); }
    draw();
  }
  function send() {
    clearTimeout(st.timer); clearTimeout(st.namesTimer);
    st.timer = setTimeout(() => {
      link.send(tb.setEdits(st.pads)).then(() => say('Sent to the TB.', 'good'), (e) => say(`Not sent: ${e.message}.`, 'err'));
      st.namesTimer = setTimeout(readNames, NAMES_AFTER_MS);
    }, 150);
  }
  function change(f) { f(st.pads); st.edits++; st.editedAt = Date.now(); st.stale = true; draw(); send(); }
  function toggle(label, on, title, disabled, onclick) {
    const b = document.createElement('button'); b.textContent = label; b.title = title; b.disabled = disabled;
    b.setAttribute('aria-pressed', String(on)); b.onclick = onclick;
    return b;
  }
  // 2.9.1 (review TBR-101): the pads' own edits count with the scale's chords and in Modes (controls_menu.c: a chord set,
  // Full and the grid refuse them); 2.9 locked them in Modes too
  const editable = () => !st.chordSet || st.chordSet === tb.MODES_CHORDS;
  function draw() {
    el('plain').disabled = !st.pads;
    const pads = padsIn(el('scope').value), where = el('scope').selectedOptions[0].textContent;
    for (const [role, label, bits] of BULK) { // "Remove" once every pad chosen has it
      const b = el(role), every = !!st.pads && pads.every((k) => (st.pads.edits[k].add & bits) === bits);
      b.disabled = !st.pads || !editable();
      b.textContent = `${every ? 'Remove' : 'Add'} ${label}`;
      b.title = `${every ? 'Take' : 'Add'} the ${label.split(' ')[0]} ${every ? 'off' : 'to'} ${where}`;
    }
    el('none').disabled = !st.pads || !editable();
    el('none').title = `No added notes and no sus on ${where} (strum and sound stay)`;
    el('setNote').classList.toggle('hidden', !st.pads || editable());
    const box = el('pads'); box.innerHTML = '';
    if (!st.pads) return;
    const canEdit = editable();
    for (let row = 0; row < 4; row++) for (let column = 0; column < 12; column++) {
      const k = 4 * column + row, e = st.pads.edits[k], cell = document.createElement('div');
      const strumOff = st.pads.strumOff[k], soundOff = st.pads.soundOff[k];
      cell.className = 'chord-pad' + (soundOff ? ' silent' : '') + (e.add || e.sus ? ' edited' : '');
      const name = document.createElement('b'); name.textContent = st.names ? st.names[k] || '–' : '…';
      if (st.stale) name.className = 'stale';
      const where = `Chord pad, column ${column + 1}, row ${row + 1} from the top`;
      cell.title = where;
      const tools = document.createElement('div'); tools.className = 'pad-tools';
      for (const [label, bit] of [['7', 2], ['9', 4], ['6', 1]]) {
        tools.appendChild(toggle(label, !!(e.add & bit), `${where}: add the ${label}th`, !canEdit,
          () => change((p) => { p.edits[k] = { ...p.edits[k], add: p.edits[k].add ^ bit }; })));
      }
      const sus = SUS.indexOf(e.sus);
      tools.appendChild(toggle(sus ? SUS_NAMES[sus] : 'sus', sus > 0, `${where}: sus2, then sus4, then plain`, !canEdit,
        () => change((p) => { p.edits[k] = { ...p.edits[k], sus: SUS[(sus + 1) % 3] }; })));
      tools.appendChild(toggle('strum', !strumOff, `${where}: strums (when Strum is on); off plays it at once`, false,
        () => change((p) => { p.strumOff[k] = !p.strumOff[k]; })));
      tools.appendChild(toggle('sound', !soundOff, `${where}: sounds; off keeps it silent`, false,
        () => change((p) => { p.soundOff[k] = !p.soundOff[k]; })));
      cell.append(name, tools);
      box.appendChild(cell);
    }
  }
  el('scope').onchange = () => draw();
  for (const [role, label, bits] of BULK) {
    el(role).onclick = () => {
      const pads = padsIn(el('scope').value);
      change((p) => { p.edits = withAdded(p.edits, pads, bits); });
    };
  }
  el('none').onclick = () => {
    const pads = padsIn(el('scope').value);
    change((p) => { p.edits = withoutAdded(p.edits, pads); });
  };
  el('plain').onclick = () => {
    if (!confirm('Every chord pad back to plain: no added notes, no sus, strum and sound on?')) return;
    change((p) => { p.edits = p.edits.map((e) => ({ ...e, sus: 0, add: 0 })); p.strumOff.fill(false); p.soundOff.fill(false); });
  };
  async function read() {
    if (!supported() || st.busy) return;
    st.busy = true; el('read').disabled = true; say('Reading the chord pads…');
    const edits = st.edits; // a click while reading wins over what was read
    try {
      const live = tb.decodeRecord(await link.ok(tb.getRecord(tb.KIND.LIVE, 0))).words;
      if (!st.pads || st.edits === edits) { st.pads = tb.padEditsFromWords(live); st.stale = false; }
      st.chordSet = live[34] & 255;
      st.names = tb.decodeChordNames(await link.ok(tb.chordNames()));
      say('');
    } catch (e) { say(`Could not read the chord pads: ${e.message}.`, 'err'); }
    st.busy = false; el('read').disabled = !supported(); draw();
  }
  el('read').onclick = read;
  return {
    async connected() {
      el('read').disabled = !supported();
      st.pads = null; draw();
      if (supported()) await read();
    },
    shown() { if (supported() && !st.pads) read(); },
    // 1.9.4: the settings changed on the board (STATE): read the pads and names again, unless a change from here is
    // still on its way.
    refresh() { if (supported() && st.pads && !st.busy && Date.now() - (st.editedAt || 0) > 2500) return read(); return undefined; },
    read,
  };
}
