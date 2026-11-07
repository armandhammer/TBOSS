// SPDX-License-Identifier: GPL-3.0-or-later - the Buttons tab's MIDI map (TBOSS 2.1.0, external control): a MIDI
// controller's knobs, faders and buttons, arriving on the TB's Control channel, set settings (each over a range of its
// values, backwards too, with or without pickup) or do what a center button can. Up to 16 slots. Learn: move a control
// and the slot takes its number (the TB reports the last controller it got, MIDI_MAP word 39). Save sends the whole map
// (SET_RECORD kind 14), which the TB uses and stores at once. The settings to choose from are the TB's own (the
// Settings tab's menu; ITEM says which can be mapped).
import * as tb from './tb-protocol.js';

const LEARN_POLL_MS = 250, LEARN_FOR_MS = 15000;
const plain = (s) => s.charAt(0) + s.slice(1).toLowerCase();
const CHANNELS = ['Off', ...Array.from({ length: 16 }, (_, i) => `Channel ${i + 1}`), 'All channels'];

// root: the section's body; link: tb-midi.js caller(); info(): IDENTIFY's answer; settings: the Settings tab (its menu).
export function controlTab(root, link, info, settings) {
  const st = { map: tb.emptyMidiMap(), read: false, busy: false, learning: -1, learnTimer: 0, learnUntil: 0, seen: null, dirty: false };
  root.innerHTML = `
    <p class="hint">Play the TB from a MIDI controller: each knob, fader or button sends a controller number (CC) on a MIDI
      channel. Choose that channel as the <b>Control channel</b>; its controllers then only control the TB. Each slot
      below sets a setting from <i>From</i> (the control all the way down) to <i>To</i> (all the way up), or does what a
      center button can (pressed at 64 and up). <b>Pickup</b>: after the setting changed another way (on the TB, a preset),
      the control does nothing until it reaches the value; the screen shows ^ or v, which way to turn. The controller
      reaches the TB through the computer (route it to the TB in your DAW or MIDI software) or a USB MIDI host box.
      On the TB: Settings → Control, and while a setting is open, move a control to map it there.</p>
    <p class="hint hidden" data-role="old">Connect a TB with TBOSS 2.1 or later to map a MIDI controller.</p>
    <div class="row"><label>Control channel <select data-role="channel"></select></label>
      <button data-role="read" disabled>Read from the TB</button>
      <button data-role="save" class="primary" disabled>Save to the TB</button>
      <button data-role="clear" disabled>Clear the map</button></div>
    <div class="ccmap" data-role="slots"></div>
    <div class="status" data-role="status"></div>`;
  const el = (role) => root.querySelector(`[data-role="${role}"]`);
  const say = (text, kind = '') => { const s = el('status'); s.textContent = text; s.className = 'status ' + kind; };
  const supported = () => !!info() && tb.midiMapSupported(info().version);
  CHANNELS.forEach((name, i) => el('channel').add(new Option(name, i)));
  el('channel').onchange = () => { st.map.channel = +el('channel').value; changed(); };

  // The settings a controller can be mapped to, page by page, as the TB's menu has them.
  function mappable() {
    const pages = settings.pages() || [];
    return pages.map((p) => ({ name: p.name, items: p.items.filter((it) => it.mappable) })).filter((p) => p.items.length);
  }
  const itemOf = (id) => { for (const p of settings.pages() || []) for (const it of p.items) if (it.id === id) return it; return null; };
  function changed() { st.dirty = true; refresh(); say('Not saved yet.'); }
  function refresh() {
    const on = supported();
    el('old').classList.toggle('hidden', on || !info());
    for (const r of ['read', 'clear']) el(r).disabled = !on || st.busy;
    el('save').disabled = !on || st.busy || !st.read;
    el('channel').disabled = !on;
    el('channel').value = String(st.map.channel);
  }
  function select(options, value, onchange, label) {
    const sel = document.createElement('select');
    for (const o of options) {
      if (o.group) { const g = document.createElement('optgroup'); g.label = o.group; o.items.forEach((x) => g.append(new Option(x.label, x.value))); sel.append(g); }
      else sel.add(new Option(o.label, o.value));
    }
    sel.value = String(value); sel.onchange = () => onchange(+sel.value); sel.setAttribute('aria-label', label);
    return sel;
  }
  function draw() {
    const box = el('slots'); box.innerHTML = '';
    const head = document.createElement('div'); head.className = 'ccrow cchead';
    for (const t of ['', 'CC', 'Does', 'What', 'From', 'To', 'Pickup', '']) { const h = document.createElement('span'); h.textContent = t; head.append(h); }
    box.append(head);
    const pages = mappable();
    st.map.slots.forEach((s, i) => {
      const row = document.createElement('div'); row.className = 'ccrow' + (s.kind ? '' : ' unused');
      const n = document.createElement('span'); n.textContent = String(i + 1); n.className = 'ccn';
      const cc = document.createElement('input'); cc.type = 'number'; cc.min = '0'; cc.max = String(tb.CC_CONTROLS - 1);
      cc.value = String(s.cc); cc.disabled = !s.kind; cc.setAttribute('aria-label', `Slot ${i + 1}: controller number`);
      cc.onchange = () => { const v = Math.max(0, Math.min(tb.CC_CONTROLS - 1, Math.round(+cc.value) || 0)); s.cc = v; cc.value = String(v); changed(); };
      const kind = select([{ label: 'Nothing', value: 0 }, { label: 'A setting', value: 1 }, { label: 'A button action', value: 2 }], s.kind, (k) => {
        s.kind = k; s.lo = 0; s.hi = 0; s.jump = 0; s.target = 0;
        if (k === tb.CC_KIND.SETTING) {
          const first = pages.length && pages[0].items[0];
          if (first) { s.target = first.id; s.hi = first.labels.length - 1; } else s.kind = 0;
        }
        changed(); draw();
      }, `Slot ${i + 1}: what it does`);
      const cells = [n, cc, kind];
      if (s.kind === tb.CC_KIND.SETTING) {
        const it = itemOf(s.target);
        const what = select(pages.map((p) => ({ group: plain(p.name), items: p.items.map((x) => ({ label: plain(x.name), value: x.id })) })),
          s.target, (id) => { s.target = id; const t = itemOf(id); s.lo = 0; s.hi = t ? t.labels.length - 1 : 0; changed(); draw(); }, `Slot ${i + 1}: setting`);
        if (!it) { what.add(new Option(`Item ${s.target} (read the settings)`, s.target), 0); what.value = String(s.target); }
        const values = (it ? it.labels : []).map((label, v) => ({ label, value: v }));
        const lo = select(values, s.lo, (v) => { s.lo = v; changed(); }, `Slot ${i + 1}: value at the bottom`);
        const hi = select(values, s.hi, (v) => { s.hi = v; changed(); }, `Slot ${i + 1}: value at the top`);
        const pick = document.createElement('input'); pick.type = 'checkbox'; pick.checked = !s.jump;
        pick.setAttribute('aria-label', `Slot ${i + 1}: pickup`); pick.onchange = () => { s.jump = pick.checked ? 0 : 1; changed(); };
        cells.push(what, lo, hi, pick);
      } else if (s.kind === tb.CC_KIND.ACTION) {
        const known = tb.actionsFor(info() && info().version);
        const what = select(tb.ACTIONS.slice(0, known).map((label, v) => ({ label, value: v })), s.target,
          (v) => { s.target = v; changed(); }, `Slot ${i + 1}: action`);
        const span = document.createElement('span'); span.className = 'hint'; span.textContent = 'pressed at 64 and up';
        cells.push(what, span, document.createElement('span'), document.createElement('span'));
      } else for (let k = 0; k < 4; k++) cells.push(document.createElement('span'));
      const learn = document.createElement('button'); learn.textContent = st.learning === i ? 'Move a control…' : 'Learn';
      learn.disabled = !supported() || !st.read || !s.kind;
      learn.onclick = () => (st.learning === i ? stopLearning('') : startLearning(i));
      cells.push(learn);
      row.append(...cells); box.append(row);
    });
    refresh();
  }
  async function lastIn() { return tb.midiMapFromWords(tb.decodeRecord(await link.ok(tb.getMidiMap(), 1500)).words); }
  async function startLearning(i) {
    if (!st.map.channel) { say('Choose the Control channel first (and save): the TB only reports controllers on it.', 'err'); return; }
    stopLearning('');
    try { const m = await lastIn(); st.seen = m && m.last ? m.last.count : null; } catch (e) { say(`Could not ask the TB: ${e.message}.`, 'err'); return; }
    st.learning = i; st.learnUntil = Date.now() + LEARN_FOR_MS; draw();
    say(`Slot ${i + 1}: move a control on the controller (on the TB's Control channel).`);
    const tick = async () => {
      if (st.learning !== i) return;
      if (Date.now() > st.learnUntil) { stopLearning('Nothing arrived. Is the controller routed to the TB, on the Control channel?', 'err'); return; }
      try {
        const m = await lastIn();
        if (m && m.last && m.last.count !== st.seen) {
          st.map.slots[i].cc = m.last.cc; stopLearning(`Slot ${i + 1}: controller ${m.last.cc} (channel ${m.last.channel}). Not saved yet.`, 'good');
          st.dirty = true; draw(); return;
        }
      } catch { /* the next one */ }
      st.learnTimer = setTimeout(tick, LEARN_POLL_MS);
    };
    st.learnTimer = setTimeout(tick, LEARN_POLL_MS);
  }
  function stopLearning(text, kind = '') { clearTimeout(st.learnTimer); const was = st.learning; st.learning = -1; if (was >= 0) draw(); if (text) say(text, kind); }
  async function read() {
    if (!supported() || st.busy) return;
    st.busy = true; refresh(); say('Reading the MIDI map…');
    try {
      if (!settings.pages()) await settings.read();
      const m = await lastIn();
      if (!m) throw new Error('the TB sent a map this page doesn\'t understand');
      st.map = { channel: m.channel, slots: m.slots }; st.read = true; st.dirty = false;
      const used = m.slots.filter((s) => s.kind).length;
      say(used ? `${used} of ${tb.CC_SLOTS} slots in use.` : 'No controls mapped yet.');
    } catch (e) { say(`Could not read the MIDI map: ${e.message}.`, 'err'); }
    st.busy = false; draw();
  }
  el('read').onclick = read;
  el('save').onclick = async () => {
    st.busy = true; refresh();
    try {
      const reply = await link.call(tb.setMidiMap(st.map));
      if (reply.status !== 0) throw new Error(tb.STATUS[reply.status] || `status ${reply.status}`);
      st.dirty = false;
      const used = st.map.slots.filter((s) => s.kind).length;
      say(`Saved: ${used} slot${used === 1 ? '' : 's'}, Control channel ${CHANNELS[st.map.channel].toLowerCase()}.`, 'good');
      root.dispatchEvent(new CustomEvent('tb-sent', { bubbles: true })); // the Settings tab shows the Control channel
    } catch (e) { say(`Not saved: ${e.message}.`, 'err'); }
    st.busy = false; refresh();
  };
  el('clear').onclick = () => { stopLearning(''); st.map = { channel: st.map.channel, slots: tb.emptyMidiMap().slots }; st.read = true; changed(); draw(); };
  draw();
  return {
    async connected() { stopLearning(''); st.map = tb.emptyMidiMap(); st.read = false; draw(); if (supported()) await read(); },
  };
}
