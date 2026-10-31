// SPDX-License-Identifier: GPL-3.0-or-later - the editor's Feel tab (TBOSS 1.9.0, Customize Stage 3): each half's
// velocity curve (Chord vel / Melody vel → Custom) and pressure curve drawn with the mouse, a calibration that
// proposes Trigger, Release, Full, Vel full and a velocity curve from soft, medium and hard presses, and a map of
// every pad's resting level. Curves go to the board as a quiet record (kind 9: no reply, the board keeps playing;
// saved there 3 s after the last change; 1.9.3: slot 9, the curves in use, which may be a preset's own); the model
// is tb-feel.js. 2.0.7 (board test A4): "Curves of" chooses whose curves to edit, the board's or any preset's own,
// without loading it; the ones in use go as slot 9 (saved with the settings), others as their own record, a moment
// after the last change (each is a flash write).
import * as tb from './tb-protocol.js';
import * as feel from './tb-feel.js';
import * as presets from './tb-presets.js';

const READ_AGAIN_MS = 3000;
const W = 320, H = 200, PAD = 24; // the curve drawing
const ROUNDS = [
  { key: 'quiet', ms: 2000, text: 'Take your hands off the pads.' },
  { key: 'soft', ms: 6000, text: 'Press pads softly, as softly as you still want a note: several pads, a few times each.' },
  { key: 'medium', ms: 6000, text: 'Now press pads at your usual strength, several pads, a few times each.' },
  { key: 'hard', ms: 6000, text: 'Now press pads as hard as you ever play, several pads, a few times each.' },
];
const svg = (tag, attrs) => { const e = document.createElementNS('http://www.w3.org/2000/svg', tag); for (const [k, v] of Object.entries(attrs)) e.setAttribute(k, v); return e; };

// root: the tab's element; link: tb-midi.js caller(); info(): IDENTIFY's answer; settings: the Settings tab
// (item(page, name), read()).
export function feelTab(root, link, info, settings) {
  const st = { feel: null, keep: 0, whose: 0, part: 0, readAt: 0, timer: 0, busy: false, cal: null, dragging: null, focus: null,
    target: null, curves: null, list: null }; // 2.0.7: target 0 = the board's, k = preset k's own; curves[target] (null: none)
  // 2.0.5: the point clicked or moved with the arrow keys keeps the keyboard across redraws (each draws a new svg;
  // a click's preventDefault, for the drag, kept it from ever getting the focus: board test U12)
  document.addEventListener('pointerdown', (ev) => { if (!(ev.target.closest && ev.target.closest('svg.curve'))) st.focus = null; }, true);
  root.innerHTML = `
    <p class="hint">How the pads respond. <b>Velocity</b>: how loud a strike plays, from just past Trigger (left) to Vel full
      (right); it applies where Settings → Play → Chord vel or Melody vel is <b>Custom</b>. <b>Pressure</b>: what a held pad
      sends as you press on (aftertouch, MPE), from Release (left) to Full (right); it always applies (a straight line is
      as before). Drag the points; the TB takes each change at once and keeps playing.</p>
    <p class="hint hidden" data-role="old">Connect a TB with TBOSS 1.9 or later to change its feel here.</p>
    <div class="row"><button data-role="read" disabled>Read from the TB</button><span class="status" data-role="status"></span></div>
    <div data-role="body" class="hidden">
      <div class="row" data-role="targetRow"><label><b>Curves of</b> <select data-role="target"></select></label>
        <button data-role="giveOwn" class="hidden"></button><button data-role="removeOwn" class="hidden"></button></div>
      <p class="facts" data-role="whose"></p>
      <div class="tabs" role="tablist" data-role="parts"></div>
      <div class="curve-editor" data-said="curve">
        <div data-role="drawing"></div>
        <div class="curve-side">
          <div data-role="shapes" class="chips"></div>
          <label><span data-role="lowName"></span> <input type="number" min="0" max="127" data-role="low"></label>
          <label><span data-role="highName"></span> <input type="number" min="0" max="127" data-role="high"></label>
          <button data-role="use"></button>
          <div class="status said"></div>
          <p class="facts" data-role="note"></p>
        </div>
      </div>
      <h3>Calibrate</h3>
      <p class="hint">Measures how hard you press and proposes the pad settings and a velocity curve for you. The TB doesn't
        play while it measures. Nothing changes until you press Apply.</p>
      <div class="row"><button data-role="calStart">Calibrate…</button><button data-role="calStop" class="hidden">Stop</button>
        <span class="status" data-role="calStatus"></span></div>
      <progress data-role="calProgress" value="0" max="1" class="hidden"></progress>
      <div data-role="proposal"></div>
      <h3>The pads at rest</h3>
      <p class="hint">Each pad's resting level (what it reads untouched), from low (dark) to high (bright); the TB measures
        them when it starts and after Settings → Pads → Rescan pads. A pad far above the others needs a harder press to play.</p>
      <div class="row"><button data-role="mapRead">Read the pads</button><span class="facts" data-role="mapFacts"></span></div>
      <div class="board-picture heat" data-role="map"></div>
    </div>`;
  const el = (role) => root.querySelector(`[data-role="${role}"]`);
  const near = tb.sayer(root, () => el('status')); // 2.0.6: a curve's messages beside it
  const say = (text, kind = '', where = 'status') => {
    if (where === 'status') { near(text, kind); return; }
    const s = el(where); s.textContent = text; s.className = 'status ' + kind;
  };
  const supported = () => !!info() && tb.settingsSupported(info().version);
  // The curves in use (1.9.3, slot 9: the board's or a preset's own, with Keep feel), or 1.9.0's board curves.
  const slot = () => (tb.contentSupported(info() && info().version) ? tb.FEEL_NOW : 0);
  const words = () => feel.feelToWords(st.feel, slot() ? st.keep : 0);
  // 2.0.7: the curves chosen are the ones playing (in use: slot 9), or another's own record
  const chosen = () => !slot() || st.target === null || st.target === st.whose;
  const targetWords = () => (chosen() ? words() : st.target ? presets.asPresetFeel(feel.feelToWords(st.feel, 0))
    : feel.feelToWords(st.feel, st.keep));
  function send(at = chosen() ? 150 : 1000) {
    clearTimeout(st.timer); st.editedAt = Date.now();
    const to = chosen() ? slot() : st.target, w = targetWords();
    if (st.curves && st.target !== null) st.curves[st.target] = st.feel;
    st.timer = setTimeout(() => {
      link.send(tb.setRecordQuiet(tb.KIND.FEEL, to, w))
        .then(() => say('Sent to the TB.', 'good'), (e) => say(`Not sent: ${e.message}.`, 'err'));
    }, at);
  }
  function setPart(f) { // f(part) -> part, for the part shown
    if (slot() && st.target > 0 && !(st.curves && st.curves[st.target])) return; // 2.0.7: no own curves: nothing to edit
    const p = feel.part(st.feel, st.part), next = f(p), meta = feel.PARTS[st.part];
    next.low = Math.max(meta.velocity ? 1 : 0, Math.min(127, Math.round(next.low)));
    next.high = Math.max(meta.velocity ? next.low : next.low + 1, Math.min(127, Math.round(next.high)));
    if (next.low > next.high || (!meta.velocity && next.low >= next.high)) return;
    st.feel = feel.withPart(st.feel, st.part, next); draw(); send();
  }
  // The drawing: input left to right, output bottom to top, the 8 points draggable up and down.
  function drawCurve() {
    const box = el('drawing'); box.innerHTML = '';
    const p = feel.part(st.feel, st.part), meta = feel.PARTS[st.part];
    const sx = (x) => PAD + ((W - 2 * PAD) * x) / 127, sy = (y) => H - PAD - ((H - 2 * PAD) * y) / 127;
    const s = svg('svg', { viewBox: `0 0 ${W} ${H}`, class: 'curve', role: 'img', 'aria-label': `${meta.name} curve` });
    s.append(svg('rect', { x: PAD, y: PAD, width: W - 2 * PAD, height: H - 2 * PAD, class: 'frame' }));
    for (const v of [32, 64, 96]) { s.append(svg('line', { x1: sx(v), y1: sy(0), x2: sx(v), y2: sy(127), class: 'grid' })); s.append(svg('line', { x1: sx(0), y1: sy(v), x2: sx(127), y2: sy(v), class: 'grid' })); }
    const out = (x) => (meta.velocity ? feel.velocityOut(st.feel, meta.half, Math.floor((x * 126) / 127)) : feel.pressureOut(st.feel, meta.half, x));
    let d = ''; for (let x = 0; x <= 127; x++) d += `${x ? 'L' : 'M'}${sx(x).toFixed(1)},${sy(out(x)).toFixed(1)}`;
    s.append(svg('path', { d, class: 'line' }));
    const label = (x, y, t, anchor) => { const e = svg('text', { x, y, 'text-anchor': anchor, class: 'axis' }); e.textContent = t; s.append(e); };
    label(PAD, H - 6, meta.velocity ? 'Trigger' : 'Release', 'start'); label(W - PAD, H - 6, meta.velocity ? 'Vel full' : 'Full', 'end');
    label(6, sy(127) + 4, '127', 'start'); label(6, sy(0), '0', 'start');
    p.points.forEach((y, i) => {
      const cx = sx(feel.LINEAR[i]), dot = svg('circle', { cx, cy: sy(y), r: 7, class: 'point' + (st.dragging === i ? ' dragging' : ''),
        tabindex: 0, 'aria-label': `point ${i + 1}: ${y}` });
      // Each move draws the curve again (a new svg), so the drag follows the pointer on the window and measures
      // against the drawing as it is now (1.9.4; it used to stop after the first move).
      const move = (clientY) => {
        const now = el('drawing').querySelector('svg'), r = now.getBoundingClientRect();
        const yv = ((H - PAD - ((clientY - r.top) * H) / r.height) * 127) / (H - 2 * PAD);
        setPart((q) => { const pts = [...q.points]; pts[i] = Math.max(0, Math.min(127, Math.round(yv))); return { ...q, points: pts }; });
      };
      dot.addEventListener('pointerdown', (ev) => {
        ev.preventDefault(); st.dragging = i; st.focus = i; dot.focus({ preventScroll: true });
        const onMove = (e) => { e.preventDefault(); move(e.clientY); };
        const end = () => {
          st.dragging = null;
          if (!chosen()) send(0); // 2.0.7: another's record: once, when the drag ends
          for (const [t, f] of [['pointermove', onMove], ['pointerup', end], ['pointercancel', end]]) window.removeEventListener(t, f);
          drawCurve();
        };
        window.addEventListener('pointermove', onMove); window.addEventListener('pointerup', end); window.addEventListener('pointercancel', end);
        move(ev.clientY);
      });
      dot.addEventListener('keydown', (ev) => {
        const step = ev.key === 'ArrowUp' ? 4 : ev.key === 'ArrowDown' ? -4 : 0;
        if (step) { ev.preventDefault(); st.focus = i; setPart((q) => { const pts = [...q.points]; pts[i] = Math.max(0, Math.min(127, pts[i] + step)); return { ...q, points: pts }; }); }
      });
      dot.addEventListener('focusout', (ev) => { if (ev.relatedTarget) st.focus = null; }); // elsewhere (not a redraw)
      s.append(dot);
    });
    box.appendChild(s);
    const was = document.activeElement;
    if (st.focus !== null && (!was || was === document.body || !was.isConnected)) {
      const dot = s.querySelectorAll('.point')[st.focus];
      if (dot) dot.focus({ preventScroll: true });
    }
  }
  function draw() {
    el('body').classList.toggle('hidden', !st.feel);
    if (!st.feel) return;
    drawTarget();
    const parts = el('parts'); parts.innerHTML = '';
    feel.PARTS.forEach((pt, i) => {
      const b = document.createElement('button'); b.textContent = pt.name; b.setAttribute('role', 'tab'); b.setAttribute('aria-selected', String(i === st.part));
      b.onclick = () => { st.part = i; draw(); }; parts.appendChild(b);
    });
    const meta = feel.PARTS[st.part], p = feel.part(st.feel, st.part);
    const shapes = el('shapes'); shapes.innerHTML = '';
    (meta.velocity ? feel.SHAPES : [{ name: 'Straight', points: feel.LINEAR }]).forEach((sh) => {
      const b = document.createElement('button'); b.textContent = sh.name; b.title = meta.velocity ? `Start from the built-in ${sh.name} curve` : 'Back to a straight line';
      b.onclick = () => setPart((q) => ({ ...q, points: [...sh.points], ...(meta.velocity ? {} : { low: 0, high: 127 }) })); shapes.appendChild(b);
    });
    el('lowName').textContent = meta.velocity ? 'Softest velocity' : 'Pressure starts at (0–126)';
    el('highName').textContent = meta.velocity ? 'Loudest velocity' : 'Pressure full at (1–127)';
    el('low').value = String(p.low); el('high').value = String(p.high);
    const item = settings.item('PLAY', meta.half ? 'MELODY VEL' : 'CHORD VEL'), vel = meta.half ? 'Melody vel' : 'Chord vel';
    // 2.0.8 (board test C1): a preset's curves are heard where its own Chord vel / Melody vel, saved in it, is Custom
    const k = slot() && st.list ? st.target : 0, preset = k > 0 && st.list[k - 1].present ? st.list[k - 1].words : null;
    const mode = preset ? presets.presetVel(preset)[meta.half] : item ? item.value : null;
    const named = (v) => (item && item.labels[v]) || '?';
    el('use').classList.toggle('hidden', !meta.velocity || !item || (k > 0 && !preset));
    if (meta.velocity && item) {
      const on = mode === feel.CUSTOM;
      el('use').textContent = preset
        ? (on ? `${vel} in preset ${k} is Custom: its curve is heard` : `Use the curve in preset ${k}: its ${vel} → Custom`)
        : (on ? `${vel} is Custom: the curve is heard` : `Use the curve: ${vel} → Custom`);
      el('use').disabled = on;
    }
    el('note').textContent = !meta.velocity
      ? `The ${meta.half ? 'melody' : 'chord'} pads' pressure, between where it starts and where it is full (0 = Release, 127 = Full).`
      : preset
        ? `Preset ${k} plays this curve only while its own ${vel}, saved in preset ${k}, is Custom (now: ${named(mode)}). ` +
          `Loading preset ${k} brings its ${vel} with it; Save preset on the TB stores the one playing then.`
        : `The ${meta.half ? 'melody' : 'chord'} pads play this curve only while Settings → Play → ${vel} is Custom ` +
          `(on the TB now: ${named(mode)}). Each preset keeps its own ${vel}: choose it under Curves of to see or change it.`;
    drawCurve();
    const none = !!slot() && st.target > 0 && !(st.curves && st.curves[st.target]); // 2.0.7: a preset without its own
    for (const r of ['drawing', 'shapes', 'low', 'high', 'parts']) el(r).classList.toggle('dimmed', none);
  }
  // 2.0.7: "Curves of": whose curves the page edits, and what the board plays now (board test A4)
  function drawTarget() {
    el('targetRow').classList.toggle('hidden', !slot() || !st.list);
    if (!slot() || !st.list) { el('whose').textContent = ''; return; }
    const sel = el('target'); sel.innerHTML = '';
    const tag = (k) => (k === st.whose ? ' · playing now' : '');
    sel.add(new Option(`The board's curves (every preset without its own)${tag(0)}`, '0'));
    st.list.forEach((p, i) => {
      const k = i + 1, name = p.name ? ` · ${p.name}` : '';
      const o = new Option(`Preset ${k}${name}: ${!p.present ? 'empty slot' : st.curves[k] ? 'its own curves' : "plays the board's"}${tag(k)}`, String(k));
      o.disabled = !p.present && !st.curves[k]; sel.add(o);
    });
    sel.value = String(st.target);
    const k = st.target, own = k > 0 && !!st.curves[k];
    el('whose').textContent = k === 0
      ? "The board's curves: every preset without curves of its own plays these. " + (st.whose === 0
        ? 'They are playing on the TB now: a change is heard at once.'
        : `On the TB now preset ${st.whose} is loaded and plays its own, so a change here is heard once a preset without its own is loaded.`)
      : own ? `Preset ${k}'s own curves: loading preset ${k} plays these. ` + (st.whose === k
        ? `Preset ${k} is loaded: a change is heard at once and stays with preset ${k} (no Save preset needed).`
        : `A change is saved in preset ${k} and heard once you load it.`)
      : `Preset ${k} has no curves of its own: it plays the board's (shown dimmed). Give it its own to shape them for preset ${k} alone.`;
    el('giveOwn').classList.toggle('hidden', !(k > 0 && !own && st.list[k - 1].present));
    el('giveOwn').textContent = `Give preset ${k} its own curves`;
    el('giveOwn').title = `A copy of the board's curves becomes preset ${k}'s own; then shape them here`;
    el('removeOwn').classList.toggle('hidden', !own);
    el('removeOwn').textContent = `Remove preset ${k}'s own curves`;
  }
  el('target').onchange = (e) => {
    clearTimeout(st.timer); st.target = +e.target.value;
    st.feel = st.curves[st.target] || st.curves[0]; draw();
  };
  // 2.0.8 (board test C1): preset k's own Chord vel or Melody vel (half) → Custom, in its record without loading it
  // (quietly, as the curves: the TB keeps playing), and at once too when preset k plays its own curves now. False: it
  // was Custom already.
  async function presetCustom(k, half) {
    const words = tb.decodeRecord(await link.ok(tb.getRecord(tb.KIND.PRESET, k - 1))).words;
    if (presets.presetVel(words)[half] === feel.CUSTOM) return false;
    await link.send(tb.setRecordQuiet(tb.KIND.PRESET, k - 1, presets.withPresetVel(words, half, feel.CUSTOM)));
    const item = settings.item('PLAY', half ? 'MELODY VEL' : 'CHORD VEL');
    if (st.whose === k && item) { await link.send(tb.setItem(item.id, feel.CUSTOM)); item.value = feel.CUSTOM; }
    return true;
  }
  el('giveOwn').onclick = async () => {
    const k = st.target, w = presets.asPresetFeel(feel.feelToWords(st.curves[0], 0));
    try {
      await link.send(tb.setRecordQuiet(tb.KIND.FEEL, k, w));
      // 2.0.8: and heard: its Chord vel Custom (Melody vel as it is)
      say(`Preset ${k} has its own curves now` + ((await presetCustom(k, 0)) ? ', and its Chord vel is Custom.' : '.'), 'good');
    } catch (e) { say(`Not sent: ${e.message}.`, 'err'); return; }
    await read(true, true);
  };
  el('removeOwn').onclick = async () => {
    const k = st.target;
    if (!confirm(`Remove preset ${k}'s own curves? It plays the board's again.`)) return;
    try { await link.send(tb.setRecordQuiet(tb.KIND.FEEL, k, presets.cleared())); say(`Preset ${k} plays the board's curves again.`, 'good'); }
    catch (e) { say(`Not sent: ${e.message}.`, 'err'); return; }
    await read(true, true);
  };
  el('low').onchange = (e) => setPart((q) => ({ ...q, low: +e.target.value }));
  el('high').onchange = (e) => setPart((q) => ({ ...q, high: +e.target.value }));
  el('use').onclick = async () => {
    const meta = feel.PARTS[st.part], item = settings.item('PLAY', meta.half ? 'MELODY VEL' : 'CHORD VEL');
    if (!item) return;
    const k = slot() && st.list ? st.target : 0;
    if (k > 0) { // 2.0.8: the preset's own, saved in it
      try { await presetCustom(k, meta.half); say(`Saved in preset ${k}: its ${meta.half ? 'Melody' : 'Chord'} vel is Custom.`, 'good'); }
      catch (e) { say(`Not sent: ${e.message}.`, 'err'); return; }
      await read(true, true); return;
    }
    link.send(tb.setItem(item.id, feel.CUSTOM)).then(() => { item.value = feel.CUSTOM; draw(); say('Sent to the TB.', 'good'); },
      (e) => say(`Not sent: ${e.message}.`, 'err'));
  };

  // --- calibration ------------------------------------------------------------------------
  const sleep = (ms) => new Promise((r) => setTimeout(r, ms));
  async function measure(round, onReading) { // each pad's highest reading above its rest for round.ms
    const best = new Array(96).fill(0), until = Date.now() + round.ms;
    while (Date.now() < until && st.cal && !st.cal.stop) {
      const now = tb.decodePads(await link.ok(tb.pads(), 3000));
      now.pads.forEach((p, k) => { best[k] = Math.max(best[k], p.value - p.rest); });
      onReading(Math.max(...best), 1 - (until - Date.now()) / round.ms);
      await sleep(60);
    }
    return best;
  }
  el('calStart').onclick = async () => {
    if (st.cal || !supported()) return;
    st.cal = { stop: false }; el('proposal').innerHTML = '';
    el('calStart').classList.add('hidden'); el('calStop').classList.remove('hidden'); el('calProgress').classList.remove('hidden');
    const samples = {};
    try {
      for (const [i, round] of ROUNDS.entries()) {
        say(`${i + 1} of ${ROUNDS.length}: ${round.text}`, '', 'calStatus');
        if (i) await sleep(1500);
        samples[round.key] = await measure(round, (top, done) => {
          el('calProgress').value = (i + Math.min(1, done)) / ROUNDS.length;
          say(`${i + 1} of ${ROUNDS.length}: ${round.text} Strongest so far: +${top}.`, '', 'calStatus');
        });
        if (st.cal.stop) throw new Error('stopped');
      }
      const p = feel.propose(samples);
      if (p.error) say(`No proposal: ${p.error}.`, 'err', 'calStatus');
      else { say('Done. The proposal is below; Apply sets it on the TB.', 'good', 'calStatus'); showProposal(p); }
    } catch (e) { say(e.message === 'stopped' ? 'Stopped; nothing changed.' : `Calibration stopped: ${e.message}.`, e.message === 'stopped' ? '' : 'err', 'calStatus'); }
    st.cal = null;
    el('calStart').classList.remove('hidden'); el('calStop').classList.add('hidden'); el('calProgress').classList.add('hidden');
  };
  el('calStop').onclick = () => { if (st.cal) st.cal.stop = true; };
  function showProposal(p) {
    const box = el('proposal'); box.innerHTML = '';
    const table = document.createElement('table'); table.className = 'proposal';
    const now = (name) => { const it = settings.item('PADS', name); return it ? it.labels[it.value] : '?'; };
    const rows = [['Your presses (above rest)', `soft +${p.soft}, medium +${p.medium}, hard +${p.hard}; noise up to +${p.noise}`, ''],
      ['Trigger', `+${p.trigger * 100}`, now('TRIGGER')], ['Release', `+${p.release * 100}`, now('RELEASE')],
      ['Full (pressure)', `+${p.full * 100}`, now('FULL')], ['Vel full', `+${p.velFull * 100}`, now('VEL FULL')],
      ['Chord vel, Melody vel', 'Custom, with this curve: a soft press about 35, a medium one about 85', '']];
    table.innerHTML = '<tr><th></th><th>Proposed</th><th>Now</th></tr>';
    for (const [a, b, c] of rows) { const tr = document.createElement('tr'); for (const t of [a, b, c]) { const td = document.createElement('td'); td.textContent = t; tr.appendChild(td); } table.appendChild(tr); }
    const apply = document.createElement('button'); apply.className = 'primary'; apply.textContent = 'Apply';
    apply.onclick = () => applyProposal(p);
    box.append(table, apply);
  }
  async function applyProposal(p) {
    const item = (page, name) => settings.item(page, name);
    const at = (it, label) => it.labels.indexOf(label);
    const trig = item('PADS', 'TRIGGER'), rel = item('PADS', 'RELEASE'), full = item('PADS', 'FULL'), vf = item('PADS', 'VEL FULL');
    const cv = item('PLAY', 'CHORD VEL'), mv = item('PLAY', 'MELODY VEL');
    if (![trig, rel, full, vf, cv, mv].every(Boolean)) { say('Read the Settings tab first (the TB\'s menu).', 'err', 'calStatus'); return; }
    // The board keeps Trigger above Release and below Full: open the gaps first, then set each.
    const steps = [[rel, 0], [full, full.labels.length - 1], [trig, at(trig, `+${p.trigger * 100}`)], [rel, at(rel, `+${p.release * 100}`)],
      [full, at(full, `+${p.full * 100}`)], [vf, at(vf, `+${p.velFull * 100}`)], [cv, feel.CUSTOM], [mv, feel.CUSTOM]];
    if (steps.some(([, v]) => v < 0)) { say('A proposed value is not one the TB has; nothing changed.', 'err', 'calStatus'); return; }
    for (const [it, v] of steps) await link.send(tb.setItem(it.id, v));
    if (slot() && st.curves) { st.target = st.whose; st.feel = st.curves[st.whose] || st.curves[0]; } // 2.0.7: the ones playing
    for (const half of [0, 1]) st.feel = feel.withPart(st.feel, half, { points: p.points, low: 1, high: 127 });
    await link.send(tb.setRecordQuiet(tb.KIND.FEEL, slot(), words()));
    await settings.read(); draw();
    const ok = trig.labels && settings.item('PADS', 'TRIGGER').labels[settings.item('PADS', 'TRIGGER').value] === `+${p.trigger * 100}`;
    say(ok ? 'Applied: play a few notes to try it. The Settings tab shows the new values.' : 'Sent, but the TB shows other values: see the Settings tab.', ok ? 'good' : 'err', 'calStatus');
  }

  // --- the pads at rest ------------------------------------------------------------------
  el('mapRead').onclick = async () => {
    try {
      const now = tb.decodePads(await link.ok(tb.pads()));
      if (!now.rested) { el('mapFacts').textContent = 'The TB has not measured its pads yet: play a note, then read again.'; return; }
      const rests = now.pads.map((p) => p.rest), lo = Math.min(...rests), hi = Math.max(...rests), mid = feel.median(rests);
      const map = el('map'); map.innerHTML = '';
      for (const side of [0, 1]) {
        const half = document.createElement('div'); half.className = 'half';
        for (let row = 0; row < 4; row++) for (let column = 0; column < 12; column++) {
          const k = side * 48 + column * 4 + row, rest = rests[k], t = hi > lo ? (rest - lo) / (hi - lo) : 0;
          const pad = document.createElement('span');
          pad.style.background = `rgb(${Math.round(30 + 225 * t)}, ${Math.round(40 + 120 * t * (1 - t) * 4)}, ${Math.round(160 * (1 - t))})`;
          pad.title = `${side ? 'Melody' : 'Chord'} pad, column ${column + 1}, row ${row + 1} from the top: rest ${rest}`;
          if (rest > mid + 500) pad.className = 'high';
          half.appendChild(pad);
        }
        map.appendChild(half);
      }
      const high = rests.filter((r) => r > mid + 500).length;
      el('mapFacts').textContent = `Resting levels ${lo} to ${hi}, middle ${mid}.` + (high ? ` ${high} ${high === 1 ? 'pad rests' : 'pads rest'} more than 500 above the middle (outlined).` : '');
    } catch (e) { el('mapFacts').textContent = `Could not read the pads: ${e.message}.`; }
  };

  async function read(quiet = false, force = false) {
    if (!supported() || (st.busy && !force)) return;
    st.busy = true; el('read').disabled = true; if (!quiet) say('Reading the TB\'s curves…');
    const before = JSON.stringify([st.feel, st.keep, st.whose, st.curves, st.list]), wasWhose = st.whose;
    try {
      const w = tb.decodeRecord(await link.ok(tb.getRecord(tb.KIND.FEEL, slot()))).words;
      st.feel = feel.feelFromWords(w); st.keep = feel.feelKeep(w); st.whose = slot() ? feel.feelWhose(w) : 0;
      if (slot()) { // 2.0.7: the board's and each preset's own, and which presets there are
        const get = async (kind, s) => { const r = await link.call(tb.getRecord(kind, s)); if (r.status === 7) return null;
          if (r.status) throw new Error(tb.STATUS[r.status] || `status ${r.status}`); return tb.decodeRecord(r.data).words; };
        const curves = [feel.feelFromWords(await get(tb.KIND.FEEL, 0))], list = [];
        const names = await get(tb.KIND.NAMES, 0), named = names ? presets.namesFromWords(names) : [];
        for (let k = 1; k <= presets.PRESETS; k++) {
          const own = await get(tb.KIND.FEEL, k);
          curves.push(own ? feel.feelFromWords(own) : null);
          const words = await get(tb.KIND.PRESET, k - 1); // 2.0.8: with its own Chord vel and Melody vel
          list.push({ present: !!words, name: named[k - 1] || '', words });
        }
        curves[st.whose] = st.feel; // the ones playing, changes not saved yet included
        st.curves = curves; st.list = list;
        // 2.0.8: a preset loaded on the board brings the page to the curves it plays; before, the list kept the one
        // chosen and "playing now" moved inside the closed list, out of sight (board test B1 step 7)
        if (st.target === null || st.whose !== wasWhose ||
            (st.target > 0 && !list[st.target - 1].present && !curves[st.target])) st.target = st.whose;
        st.feel = curves[st.target] || curves[0];
      }
      st.readAt = Date.now(); if (!quiet) say('');
    }
    catch (e) { if (!quiet) say(`Could not read the curves: ${e.message}.`, 'err'); }
    st.busy = false; el('read').disabled = !supported();
    if (!quiet || JSON.stringify([st.feel, st.keep, st.whose, st.curves, st.list]) !== before) draw();
  }
  el('read').onclick = read;
  // 2.0.8: a change on the board while an edit from here is on its way is read once that's done; before, it was
  // dropped, the poll having seen it, until the board changed again
  async function refresh() {
    clearTimeout(st.again);
    if (!supported() || !st.feel || st.cal) return;
    const wait = 1500 - (Date.now() - (st.editedAt || 0));
    if (st.dragging !== null || wait >= 0) { st.again = setTimeout(refresh, Math.max(wait + 1, 300)); return; }
    await read(true);
  }
  return {
    async connected() {
      el('old').classList.toggle('hidden', supported() || !info());
      el('read').disabled = !supported();
      st.feel = null; st.target = null; st.curves = null; st.list = null; draw();
      if (supported()) await read();
    },
    shown() { if (supported() && !st.cal && Date.now() - st.readAt > READ_AGAIN_MS) read(); },
    // 2.0.6: refresh(), a quiet read when the board changed (STATE), unless something from here is on its way (board test Y6)
    async refresh() { await refresh(); },
  };
}
