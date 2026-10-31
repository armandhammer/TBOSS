// SPDX-License-Identifier: GPL-3.0-or-later - the editor's Presets tab (TBOSS 1.9.3, Customize Stage 4): the 8 presets
// with their names (kind 10, shown on the board's Load preset), whether each has its own colors (kind 8) and curves
// (kind 9); load one on the TB (SET_ITEM Load preset), copy, swap or clear them (their records, colors, curves and
// name go along), give one the colors or curves showing now or take them away, Keep look and Keep feel; and two
// presets (or the TB now) compared setting by setting (VALUES, named by the board's menu from the Settings tab).
// 2.0: each preset's own chord set (kind 11, Keep chords) the same way: set or removed here, carried by copy, swap
// and clear, in the compare.
import * as tb from './tb-protocol.js';
import * as presets from './tb-presets.js';

const READ_AGAIN_MS = 3000;
const N = presets.PRESETS;

// root: the tab's element; link: tb-midi.js caller(); info(): IDENTIFY's answer; settings: the Settings tab
// (item(), pages(), read()).
export function presetsTab(root, link, info, settings) {
  const st = { list: null, names: null, typed: new Set(), readAt: 0, busy: false };
  root.innerHTML = `
    <p class="hint">The TB's 8 presets. Name them (the TB's <i>Load preset</i> shows the names), load one on the TB, copy,
      swap or clear them. A preset can have its own pad colors, velocity and pressure curves and (TBOSS 2.0) its own copy of
      the chord set it plays: loading it uses them, and a preset without brings the board's back.</p>
    <p class="hint hidden" data-role="old">Connect a TB with TBOSS 1.9.3 or later to manage its presets here.</p>
    <div class="row"><button data-role="read" disabled>Read from the TB</button><span class="status" data-role="status"></span></div>
    <div data-role="body" class="hidden">
      <div data-said="list"><div class="preset-list" data-role="list"></div><div class="status said pinned"></div>
      <div class="row"><button data-role="saveNames" class="primary" disabled>Save names</button>
        <label><input type="checkbox" data-role="keepLook"> Keep look: <i>Save preset</i> also saves the colors showing</label>
        <label><input type="checkbox" data-role="keepFeel"> Keep feel: <i>Save preset</i> also saves the curves in use</label>
        <label data-role="keepChordsLabel"><input type="checkbox" data-role="keepChords"> Keep chords: <i>Save preset</i> also saves a copy of its chord set</label></div></div>
      <h3>Compare</h3>
      <div data-said="compare"><div class="row"><select data-role="a" aria-label="First"></select> with <select data-role="b" aria-label="Second"></select>
        <button data-role="compare">Compare</button>
        <label><input type="checkbox" data-role="all"> Every setting, not only the differences</label></div>
      <div class="status said"></div>
      <div data-role="diff"></div></div>
    </div>`;
  const el = (role) => root.querySelector(`[data-role="${role}"]`);
  const say = tb.sayer(root, () => el('status')); // 2.0.6: messages under the list or the comparison
  const supported = () => !!info() && tb.contentSupported(info().version);
  const v2 = () => !!info() && tb.v2Supported(info().version);
  const label = (i) => `${i + 1}${st.names[i] ? ' · ' + st.names[i] : ''}`;

  // GET_RECORD: its words, or null when nothing is stored there.
  async function get(kind, slot) {
    const reply = await link.call(tb.getRecord(kind, slot));
    if (reply.status === 7) return null;
    if (reply.status) throw new Error(tb.STATUS[reply.status] || `status ${reply.status}`);
    return tb.decodeRecord(reply.data).words;
  }
  async function put(kind, slot, words) {
    const reply = await link.call(tb.setRecord(kind, slot, words));
    if (reply.status) throw new Error(`${tb.recordLabel({ kind, slot })}: ${tb.STATUS[reply.status] || reply.status}`);
  }
  async function read(quiet = false) {
    if (!supported() || st.busy) return;
    st.busy = true; el('read').disabled = true; if (!quiet) say('Reading the presets…');
    const before = JSON.stringify([st.list, st.names]);
    try {
      const list = [];
      for (let i = 0; i < N; i++) list.push({ preset: await get(tb.KIND.PRESET, i), look: await get(tb.KIND.LOOK, i + 1), feel: await get(tb.KIND.FEEL, i + 1),
        chords: v2() ? await get(tb.KIND.OWN_SET, i + 1) : null });
      const names = await get(tb.KIND.NAMES, 0);
      const stored = names ? presets.namesFromWords(names) : new Array(N).fill('');
      st.list = list; st.names = stored.map((n, i) => (st.typed.has(i) ? st.names[i] : n)); // names typed, not saved yet, stay
      for (const [role, name] of [['keepLook', 'KEEP LOOK'], ['keepFeel', 'KEEP FEEL'], ['keepChords', 'KEEP CHORDS']]) {
        const it = settings.item('PRESETS', name);
        if (it) it.value = tb.decodeItem(await link.ok(tb.item(it.id))).value;
        el(role).checked = !!(it && it.value); el(role).disabled = !it;
      }
      el('keepChordsLabel').classList.toggle('hidden', !v2());
      st.readAt = Date.now(); if (!quiet) say('');
    } catch (e) { if (!quiet) say(`Could not read the presets: ${e.message}.`, 'err'); }
    st.busy = false; el('read').disabled = !supported();
    if (!quiet || JSON.stringify([st.list, st.names]) !== before) draw();
  }
  // One change to the board (after a read going on), then read again.
  async function act(text, f) {
    while (st.busy) await new Promise((r) => setTimeout(r, 50));
    st.busy = true; say(text);
    try { await f(); st.busy = false; await read(); say(`${text.replace(/…$/, '')}: done.`, 'good'); }
    catch (e) { st.busy = false; say(`Stopped: ${e.message}.`, 'err'); await read(); }
  }
  async function saveNames(names) { await put(tb.KIND.NAMES, 0, presets.namesToWords(names)); st.typed.clear(); el('saveNames').disabled = true; }
  // Preset `from`'s record, own colors, curves and (2.0) chord set (as read: null for none) into preset `to`, its name
  // into names.
  async function copyInto(from, to, names) {
    const f = from === null ? { preset: null, look: null, feel: null, chords: null } : st.list[from];
    await put(tb.KIND.PRESET, to, f.preset || presets.cleared());
    if (f.look || st.list[to].look) await put(tb.KIND.LOOK, to + 1, f.look || presets.cleared());
    if (f.feel || st.list[to].feel) await put(tb.KIND.FEEL, to + 1, f.feel || presets.cleared());
    if (f.chords || st.list[to].chords) await put(tb.KIND.OWN_SET, to + 1, f.chords || presets.cleared());
    names[to] = from === null ? '' : st.names[from];
  }
  // Copies from what was read, so a swap is two copies.
  function row(i) {
    const p = st.list[i], r = document.createElement('div');
    r.className = 'preset' + (p.preset ? '' : ' empty');
    const num = document.createElement('b'); num.textContent = String(i + 1);
    const name = document.createElement('input'); name.type = 'text'; name.maxLength = tb.NAME_LENGTH; name.size = 14;
    name.value = st.names[i]; name.placeholder = p.preset ? 'NAME' : 'EMPTY'; name.setAttribute('aria-label', `Preset ${i + 1} name`);
    name.oninput = () => { st.names[i] = tb.cleanName(name.value); st.typed.add(i); el('saveNames').disabled = false; };
    const facts = document.createElement('span'); facts.className = 'facts';
    facts.textContent = !p.preset ? 'empty' : [p.look ? 'own colors' : '', p.feel ? 'own curves' : '', p.chords ? presets.ownChordsText(p.chords) : '']
      .filter(Boolean).join(' · ') || (v2() ? "the board's colors, curves and chords" : "the board's colors and curves");
    const tools = document.createElement('div'); tools.className = 'preset-tools';
    const button = (text, title, onclick, disabled = false) => {
      const b = document.createElement('button'); b.textContent = text; b.title = title; b.disabled = disabled || st.busy; b.onclick = onclick; tools.appendChild(b);
    };
    button('Load on the TB', `Load preset ${i + 1} on the TB now`, () => {
      const it = settings.item('PRESETS', 'LOAD PRESET');
      act(`Loading preset ${i + 1}…`, async () => { await link.send(tb.setItem(it.id, i)); });
    }, !p.preset || !settings.item('PRESETS', 'LOAD PRESET'));
    const target = document.createElement('select'); target.setAttribute('aria-label', `Preset ${i + 1}: copy or swap with`);
    for (let j = 0; j < N; j++) if (j !== i) target.add(new Option(`to ${label(j)}`, j));
    tools.appendChild(target);
    button('Copy', `Copy preset ${i + 1} (with its colors, curves, chords and name) over the chosen one`, () => {
      const j = +target.value;
      if (st.list[j].preset && !confirm(`Copy preset ${label(i)} over preset ${label(j)}?`)) return;
      act(`Copying ${i + 1} to ${j + 1}…`, async () => { const names = [...st.names]; await copyInto(i, j, names); await saveNames(names); });
    }, !p.preset);
    button('Swap', `Swap preset ${i + 1} with the chosen one (with their colors, curves, chords and names)`, () => {
      const j = +target.value;
      act(`Swapping ${i + 1} and ${j + 1}…`, async () => { const names = [...st.names]; await copyInto(i, j, names); await copyInto(j, i, names); await saveNames(names); });
    });
    button('Clear', `Empty preset ${i + 1} (its colors, curves, chords and name too)`, () => {
      if (!confirm(`Clear preset ${label(i)}? This can't be undone.`)) return;
      act(`Clearing ${i + 1}…`, async () => { const names = [...st.names]; await copyInto(null, i, names); await saveNames(names); });
    }, !p.preset && !p.look && !p.feel && !p.chords && !st.names[i]);
    button('Set own colors', "The pad colors showing now become this preset's own: loading it shows them", () =>
      act(`Colors for preset ${i + 1}…`, async () => { await put(tb.KIND.LOOK, i + 1, presets.asPresetLook(await get(tb.KIND.LOOK, tb.LOOKS))); }), !p.preset);
    if (p.look) button('Remove own colors', "Loading this preset shows the board's colors", () =>
      act(`Colors for preset ${i + 1}…`, async () => { await put(tb.KIND.LOOK, i + 1, presets.cleared()); }));
    button('Set own curves', "The velocity and pressure curves in use now become this preset's own: loading it uses them", () =>
      act(`Curves for preset ${i + 1}…`, async () => { await put(tb.KIND.FEEL, i + 1, presets.asPresetFeel(await get(tb.KIND.FEEL, tb.FEEL_NOW))); }), !p.preset);
    if (p.feel) button('Remove own curves', "Loading this preset uses the board's curves", () =>
      act(`Curves for preset ${i + 1}…`, async () => { await put(tb.KIND.FEEL, i + 1, presets.cleared()); }));
    if (v2()) { // 2.0: a copy of the chord set the preset plays, as it plays now
      const set = presets.presetChordSet(p.preset);
      button('Set own chords', set === null ? "This preset plays the scale's chords, Full or the chord grid: no chord set to keep" :
        `A copy of chord set ${set + 1} as it plays now becomes this preset's own: loading it brings that copy back`, () =>
        act(`Chords for preset ${i + 1}…`, async () => {
          const now = await get(tb.KIND.OWN_SET, tb.OWN_SET_NOW), own = now && tb.ownSetFromWords(now);
          const chords = own && own.whose && own.set === set ? own.chords : tb.chordSetFromWords((await get(tb.KIND.CHORDS, set)) || tb.chordSetToWords({ name: '', columns: new Array(12).fill(null) }));
          if (!chords || chords.columns.every((c) => !c)) throw new Error(`chord set ${set + 1} is empty`);
          await put(tb.KIND.OWN_SET, i + 1, tb.ownSetToWords(set, chords));
        }), !p.preset || set === null);
      if (p.chords) button('Remove own chords', "Loading this preset plays the board's chord sets", () =>
        act(`Chords for preset ${i + 1}…`, async () => { await put(tb.KIND.OWN_SET, i + 1, presets.cleared()); }));
    }
    r.append(num, name, facts, tools);
    return r;
  }
  function draw() {
    el('body').classList.toggle('hidden', !st.list);
    if (!st.list) return;
    const list = el('list'); list.innerHTML = '';
    for (let i = 0; i < N; i++) list.appendChild(row(i));
    for (const role of ['a', 'b']) {
      const sel = el(role), was = sel.value; sel.innerHTML = '';
      sel.add(new Option('The TB now', 'live'));
      for (let i = 0; i < N; i++) if (st.list[i].preset) sel.add(new Option(`Preset ${label(i)}`, i));
      if ([...sel.options].some((o) => o.value === was)) sel.value = was;
      else if (role === 'b' && sel.options.length > 1) sel.selectedIndex = 1;
    }
  }
  el('saveNames').onclick = () => act('Saving the names…', () => saveNames(st.names));
  for (const [role, name] of [['keepLook', 'KEEP LOOK'], ['keepFeel', 'KEEP FEEL'], ['keepChords', 'KEEP CHORDS']]) {
    el(role).onchange = () => {
      const it = settings.item('PRESETS', name);
      if (!it) return;
      it.value = el(role).checked ? 1 : 0;
      link.send(tb.setItem(it.id, it.value)).then(() => say('Sent to the TB.', 'good'), (e) => say(`Not sent: ${e.message}.`, 'err'));
    };
  }
  async function valuesOf(which) {
    const live = which === 'live', reply = await link.call(tb.values(live ? tb.KIND.LIVE : tb.KIND.PRESET, live ? 0 : +which));
    if (reply.status) throw new Error(tb.STATUS[reply.status] || `status ${reply.status}`);
    return tb.decodeValues(reply.data).values;
  }
  el('compare').onclick = async () => {
    const pages = settings.pages();
    if (!pages) { say('Read the Settings tab first (the TB\'s menu names every setting).', 'err'); return; }
    try {
      const a = await valuesOf(el('a').value), b = await valuesOf(el('b').value), rows = presets.compare(pages, a, b);
      if (v2()) { // 2.0: the own chord set of each (the TB now: the one in use, if a preset's)
        const own = async (which) => (which === 'live' ? presets.ownChordsText(await get(tb.KIND.OWN_SET, tb.OWN_SET_NOW), true) :
          presets.ownChordsText(st.list[+which].chords));
        const ca = await own(el('a').value), cb = await own(el('b').value);
        rows.push({ page: 'PRESETS', item: 'OWN CHORDS', a: ca, b: cb, same: ca === cb });
      }
      const shown = el('all').checked ? rows : rows.filter((r) => !r.same), box = el('diff'); box.innerHTML = '';
      if (!shown.length) { box.textContent = 'The same in every setting.'; return; }
      const t = document.createElement('table'); t.className = 'proposal compare';
      const head = t.insertRow(); for (const h of ['Page', 'Setting', el('a').selectedOptions[0].text, el('b').selectedOptions[0].text]) { const th = document.createElement('th'); th.textContent = h; head.appendChild(th); }
      for (const r of shown) {
        const tr = t.insertRow(); if (!r.same) tr.className = 'differs';
        for (const v of [r.page, r.item, r.a, r.b]) tr.insertCell().textContent = v;
      }
      box.appendChild(t);
      say(`${rows.filter((r) => !r.same).length} settings differ.`);
    } catch (e) { say(`Could not compare: ${e.message}.`, 'err'); }
  };
  el('read').onclick = read;
  return {
    async connected() {
      el('old').classList.toggle('hidden', supported() || !info());
      el('read').disabled = !supported();
      st.list = null; draw();
      if (supported()) await read();
    },
    shown() { if (supported() && !st.busy && Date.now() - st.readAt > READ_AGAIN_MS) read(); },
    // 2.0.6: refresh(), a quiet read when the board changed (STATE), unless something from here is on its way (board test Y6)
    async refresh() { if (supported() && st.list && !st.busy) await read(true); },
  };
}
