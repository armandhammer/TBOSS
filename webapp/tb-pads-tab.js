// SPDX-License-Identifier: GPL-3.0-or-later - the Chords tab's chord pad edits (TBOSS 1.9.3, Customize Stage 4): all 48
// chord pads at once, each with its chord's name as the TB's screen shows it, and its edits: add the 7th, 9th or 6th,
// sus2 or sus4, its strum off, its sound off (what Settings → Chords → Add 7th … Sound do for the last chord played).
// Changes go to the board with SET_EDITS (no reply: the board keeps playing; it takes all of them or none), and
// the names are read again a moment after the last change (CHORD_NAMES; before 1.9.4 the board paused its notes for that).
import * as tb from './tb-protocol.js';

const NAMES_AFTER_MS = 900;
const SUS = [0, 2, 4], SUS_NAMES = ['', 'sus2', 'sus4'];

// root: the section's element; link: tb-midi.js caller(); info(): IDENTIFY's answer.
export function padsTab(root, link, info) {
  const st = { pads: null, names: null, stale: false, chordSet: 0, edits: 0, timer: 0, namesTimer: 0, busy: false };
  root.innerHTML = `
    <p class="hint">Each chord pad's edits, all 48 at once: click <b>7</b>, <b>9</b>, <b>6</b> to add that note, <b>sus</b>
      for sus2 then sus4, <b>strum</b> to keep the pad from strumming, <b>sound</b> to silence it. The TB takes each change
      at once and keeps playing; the chord names follow a moment later. On the TB: Settings → Chords → <i>Add 7th</i> …
      <i>Sound</i> edit the last chord played. Presets keep the edits.</p>
    <p class="hint hidden" data-role="old">Connect a TB with TBOSS 1.9.3 or later to edit its chord pads here.</p>
    <div class="row"><button data-role="read" disabled>Read from the TB</button><button data-role="plain" disabled>All plain</button>
      <span class="status" data-role="status"></span></div>
    <p class="hint note hidden" data-role="setNote"><b>A chord set, the chord grid or the Full layout plays now</b>
      (Settings → Chords → <i>Chord set</i>): its chords are as typed, so <b>7</b>, <b>9</b>, <b>6</b> and <b>sus</b> are
      off here; only <b>strum</b> and <b>sound</b> apply. They come back with <i>Chord set</i> Scale.</p>
    <div class="pad-scroll"><div class="chord-pads" data-role="pads" aria-label="The chord pads, as on the board"></div></div>`;
  const el = (role) => root.querySelector(`[data-role="${role}"]`);
  const say = (text, kind = '') => { const s = el('status'); s.textContent = text; s.className = 'status ' + kind; };
  const supported = () => !!info() && tb.contentSupported(info().version);

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
  function draw() {
    el('plain').disabled = !st.pads;
    el('setNote').classList.toggle('hidden', !st.pads || !st.chordSet);
    const box = el('pads'); box.innerHTML = '';
    if (!st.pads) return;
    const editable = !st.chordSet;
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
        tools.appendChild(toggle(label, !!(e.add & bit), `${where}: add the ${label}th`, !editable,
          () => change((p) => { p.edits[k] = { ...p.edits[k], add: p.edits[k].add ^ bit }; })));
      }
      const sus = SUS.indexOf(e.sus);
      tools.appendChild(toggle(sus ? SUS_NAMES[sus] : 'sus', sus > 0, `${where}: sus2, then sus4, then plain`, !editable,
        () => change((p) => { p.edits[k] = { ...p.edits[k], sus: SUS[(sus + 1) % 3] }; })));
      tools.appendChild(toggle('strum', !strumOff, `${where}: strums (when Strum is on); off plays it at once`, false,
        () => change((p) => { p.strumOff[k] = !p.strumOff[k]; })));
      tools.appendChild(toggle('sound', !soundOff, `${where}: sounds; off keeps it silent`, false,
        () => change((p) => { p.soundOff[k] = !p.soundOff[k]; })));
      cell.append(name, tools);
      box.appendChild(cell);
    }
  }
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
      el('old').classList.toggle('hidden', supported() || !info());
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
