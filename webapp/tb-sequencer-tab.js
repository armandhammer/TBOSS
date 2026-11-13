// SPDX-License-Identifier: GPL-3.0-or-later - the editor's Sequencer tab (TBOSS 1.9.3, Customize Stage 4): the step
// pattern's 4 lanes x 64 steps as a grid. Click a step to add it (or choose it), click the chosen one to take it
// away; the step's options (what it plays, velocity, gate, tie, chance, ratchet, condition, a chord's edits) and each
// lane's kind, channel, length, rate, direction, mute and Euclid fill are beside it. Each change goes to the board as
// quiet lane records (kind 7: no reply, the board keeps playing; it saves them 3 s after the last), only the
// 16-step records that changed; Play / Stop is the Sequence page's Play through SET_ITEM. Model: tb-lanes.js.
// 2.0: copy a lane and paste it onto another (steps and settings), move a lane's steps one earlier or later, and
// copy a range of steps (shift-click the last one) and paste it from the chosen step on; sent the same quiet way.
// 1.9.4: a step that is on is ringed in the color the board lights it (ui_steps.c ui_step_color: what it plays, by
// note name; drums orange), and the tab reads the steps again when the board's change (STATE, once a second).
// 2.0.3: a step's nudge and the Fill conditions, each lane's swing and transpose, and Randomize and Mutate (as the
// board's Settings → Steps → Mutate, tb-lanes.js mutate).
// 2.4.0: 8 lanes and 8 patterns (any pattern shown and edited, the one playing marked; Play this pattern is Settings →
// Patterns → Pattern; Copy to copies the one shown), each lane's Solo, Follow (moves with the chord pads) and loop
// window, and the song (the patterns in turn, Song on or off: record kind 19). A board before 2.4: one pattern of 4.
import * as tb from './tb-protocol.js';
import * as lanes from './tb-lanes.js';
import * as lights from './tb-lights.js';

const READ_AGAIN_MS = 3000;
const SETTLE_MS = 1500; // a change of the board's steps this soon after one from here is (most likely) that one
const NOTE = { C: 0, D: 2, E: 4, F: 5, G: 7, A: 9, B: 11 };
const css = (rgb) => { const [r, g, b] = lights.onScreen(rgb); return `rgb(${r}, ${g}, ${b})`; };
const option = (sel, text, value, selected) => { const o = new Option(text, value); o.selected = selected; sel.add(o); };

// root: the tab's element; link: tb-midi.js caller(); info(): IDENTIFY's answer; settings: the Settings tab (item());
// stepsOf(id): a scale's semitones (for a melody step's note).
export function sequencerTab(root, link, info, settings, stepsOf = () => null) {
  const st = { pattern: null, sent: null, lane: 0, step: 0, rotate: new Array(lanes.LANES).fill(0), edits: 0, playing: false, names: null, readAt: 0, timer: 0, busy: false,
    range: null, clip: null, note: '', // range: the chosen lane's other end of a range of steps; clip: { lane, from } or { steps, from, to }
    board: null, pending: false, editedAt: 0, // board: { tonic, steps, layout } from the live settings
    shown: 0, song: null, sentExtras: null }; // 2.4.0: the pattern shown here, the song ({ current, on, links }), its windows as sent
  root.innerHTML = `
    <p class="hint">The step sequencer's patterns: eight patterns of eight lanes of up to 64 steps (a TB before TBOSS 2.4:
      one pattern of four lanes), as the TB plays them (Settings → Steps → <i>Steps</i> on the board). Choose the pattern
      to show here; the one playing is marked ▶. Click a step to add it, or to choose it; click the chosen step again to
      take it away. What the chosen step plays (<b>Plays</b>: a chord pad, a melody pad or a drum) and its other options
      are right under its lane. The TB takes each change at once and keeps playing; a change made on the TB shows here too.
      Each lane's ◀ ▶ move its steps one earlier or later (the end comes round to the start); Copy and Paste take a
      whole lane, its settings too. Shift-click a second step to choose a range of steps to copy.</p>
    <p class="hint hidden" data-role="old">Connect a TB with TBOSS 1.9.3 or later to edit its steps here.</p>
    <div class="row"><button data-role="read" disabled>Read from the TB</button>
      <button data-role="play" class="primary" disabled>▶ Play</button><span class="status" data-role="status"></span></div>
    <div data-role="body" class="hidden">
      <div class="row patterns hidden" data-role="patterns" data-said="patterns"></div>
      <div class="row song hidden" data-role="song" data-said="song"></div>
      <div data-role="lanes"></div>
      <h3 data-role="stepTitle"></h3>
      <div class="step-options" data-role="step" data-said="step"></div>
    </div>`;
  const el = (role) => root.querySelector(`[data-role="${role}"]`);
  const say = tb.sayer(root, () => el('status')); // 2.0.6: a lane's messages in the line under its buttons
  const supported = () => !!info() && tb.contentSupported(info().version);
  const playItem = () => settings.item(['PROGRESSIONS', 'SEQUENCE'], 'PLAY'); // 2.0.1: Progressions (before: Sequence)
  const patternItem = () => settings.item(['PATTERNS'], 'PATTERN'); // 2.4.0
  const v24 = () => !!info() && tb.v24Supported(info().version);
  const grooveOk = () => !!info() && tb.grooveSupported(info().version); // 2.5.2: a pattern's groove (older boards refuse it)
  const glideOk = () => !!info() && tb.glideSupported(info().version); // 2.6.0: a lane's Bend glide (the same)
  // 2.4.12 (roadmap C): a board item by name on any page, and the step bend's rules: Bend range, and whether lane i has a
  // MIDI channel to itself (controls_steps.c tc_lane_bend_ok: no half, MPE zone, drum or bass pads, live notes, pedal
  // note or other lane on it). A board before 2.4.12 has no Bend range: no bends.
  const boardItem = (name) => { for (const p of settings.pages() || []) { const it = p.items.find((x) => x.name === name); if (it) return it; } return null; };
  const label = (it) => (it && it.labels ? String(it.labels[it.value] || '') : '');
  const bendSemis = () => { const it = boardItem('BEND RANGE'); return it ? lanes.BEND_RANGES[it.value % lanes.BEND_RANGES.length] : 0; };
  function laneChannel(l) {
    const ch = (n) => { const it = boardItem(n); return it ? it.value & 15 : -1; };
    return l.channel ? (l.channel - 1) & 15 : l.kind === lanes.KIND.CHORDS ? ch('CHORD CH') : l.kind === lanes.KIND.MELODY ? ch('MELODY CH') : ch('DRUM CH');
  }
  function bendShared(i) { // why lane i can't bend ('' when it can)
    const ls = st.pattern.lanes, l = ls[i], c = laneChannel(l), ch = (n) => { const it = boardItem(n); return it ? it.value & 15 : -1; };
    if (l.kind === lanes.KIND.OFF || c < 0) return 'the lane is off';
    if (c === ch('CHORD CH') || c === ch('MELODY CH')) return `channel ${c + 1} is a pad half's`;
    const mpe = label(boardItem('MPE')), layout = label(boardItem('MELODY'));
    if ((/MELODY|BOTH/.test(mpe) && c < 8) || (/CHORD|BOTH/.test(mpe) && c >= 8)) return `channel ${c + 1} is in an MPE zone`;
    if (/DRUM/.test(layout) && (c === ch('DRUM CH') || c === ch('BASS CH'))) return `the Drums layout plays on channel ${c + 1}`;
    const live = boardItem('LIVE CH'); if (/LIVE/.test(layout) && live && live.value && ((live.value - 1) & 15) === c) return `live notes play on channel ${c + 1}`;
    if (/NOTE/.test(label(boardItem('MODE'))) && c === ch('DRUM CH')) return `the pedal note plays on channel ${c + 1}`;
    const other = ls.findIndex((o, j) => j !== i && o.kind !== lanes.KIND.OFF && laneChannel(o) === c);
    return other >= 0 ? `lane ${other + 1} plays on channel ${c + 1} too` : '';
  }
  const recordCount = () => (v24() ? lanes.RECORDS : lanes.RECORDS_V1);
  const where = (slot) => (v24() ? lanes.recordOf(st.shown, slot) : { kind: tb.KIND.LANE, slot }); // the shown pattern's record

  // The records that changed since last sent, a moment after the last click.
  function send() {
    clearTimeout(st.timer); st.pending = true;
    st.timer = setTimeout(async () => {
      st.pending = false;
      const changed = [];
      for (let slot = 0; slot < recordCount(); slot++) {
        const w = lanes.encodeChunk(st.pattern, slot);
        if (w.some((x, i) => x !== st.sent[slot][i])) changed.push([slot, w]);
      }
      const extras = v24() && lanes.encodeExtras(st.pattern);
      const extrasChanged = extras && extras.some((x, i) => x !== st.sentExtras[i]);
      try {
        for (const [slot, w] of changed) { const r = where(slot); await link.send(tb.setRecordQuiet(r.kind, r.slot, w)); st.sent[slot] = w; }
        if (extrasChanged) { await link.send(tb.setRecordQuiet(tb.KIND.PATTERN, st.shown, extras)); st.sentExtras = extras; changed.push(null); }
        const note = st.note; st.note = '';
        if (changed.length || note) say(note + (changed.length ? (note ? ' ' : '') + 'Sent to the TB.' : ''), 'good');
      } catch (e) { say(`Not sent: ${e.message}.`, 'err'); }
    }, 150);
  }
  // note: what the change was, shown with "Sent to the TB." (a function: worked out after f).
  function change(f, note = '') { f(st.pattern); st.note = typeof note === 'function' ? note() : note; st.edits++; st.editedAt = Date.now(); draw(); send(); }
  const readBoard = async () => { // the key, scale and melody layout, for each step's color
    const live = tb.decodeRecord(await link.ok(tb.getRecord(tb.KIND.LIVE, 0))).words;
    st.board = { tonic: live[0] & 255, steps: stepsOf((live[0] >>> 8) & 255) || [0, 2, 4, 5, 7, 9, 11], base: (live[0] >>> 24) & 255 };
  };
  // The color the board lights step s of lane l (null: not known here).
  function stepColor(l, s) {
    if (l.kind === lanes.KIND.DRUMS) return [255, 110, 0];
    let pc = null;
    if (l.kind === lanes.KIND.CHORDS) { // the chord's root, from the board's own name for it
      const m = st.names && /^([A-G])([#b♯♭]?)/.exec(st.names[s.value] || '');
      if (m) pc = (NOTE[m[1]] + (/[#♯]/.test(m[2]) ? 1 : m[2] ? 11 : 0)) % 12;
    } else if (l.kind === lanes.KIND.MELODY) { // 2.0.5: the note it plays (before: the pad's in the layout shown)
      const pitch = melodyPitch(s.value);
      if (pitch !== null) pc = pitch % 12;
    }
    return pc === null ? null : lights.HUES[pc];
  }
  // 2.0.5: a melody step's note, as engine_play.c tc_step_notes plays it: the melody half's Scale layout whatever it
  // shows (layout.c melody: base + key + an octave a row up + the column's scale step), in the board's key and scale
  // and melody octave; null before the board is read. Shown as E3 rather than its pad, 3.4 (board test U13).
  function melodyPitch(v) {
    const b = st.board;
    if (!b || v >= 48 || !b.steps.length) return null;
    const column = v >> 2, up = 3 - (v & 3), n = b.steps.length;
    const pitch = b.base + b.tonic + 12 * up + 12 * Math.floor(column / n) + b.steps[column % n];
    return pitch <= 127 ? pitch : null;
  }
  const valueShort = (l, v) => (l.kind === lanes.KIND.MELODY && melodyPitch(v) !== null ? lanes.noteName(melodyPitch(v)) : lanes.valueShort(l.kind, v, st.names));
  const valueName = (l, v) => (l.kind === lanes.KIND.MELODY && melodyPitch(v) !== null
    ? `${lanes.noteName(melodyPitch(v))} (${lanes.valueName(l.kind, v, st.names).toLowerCase()})` : lanes.valueName(l.kind, v, st.names));
  const lane = () => st.pattern.lanes[st.lane];

  function laneRow(i) {
    const l = st.pattern.lanes[i], n = lanes.length(l), box = document.createElement('div');
    box.className = 'lane' + (i === st.lane ? ' chosen' : '') + (l.mute || l.kind === lanes.KIND.OFF ? ' quiet' : '');
    box.dataset.said = `lane${i}`;
    const head = document.createElement('div'); head.className = 'lane-head';
    const title = document.createElement('b'); title.textContent = `Lane ${i + 1}`;
    const kind = document.createElement('select'); kind.setAttribute('aria-label', `Lane ${i + 1} kind`);
    lanes.KINDS.forEach((k, v) => option(kind, k, v, v === l.kind));
    kind.onchange = () => change((p) => { p.lanes[i].kind = +kind.value; });
    const channel = document.createElement('select'); channel.setAttribute('aria-label', `Lane ${i + 1} channel`);
    for (let c = 0; c < lanes.CHANNELS; c++) option(channel, c ? `Ch ${c}` : 'Its channel', c, c === l.channel);
    channel.onchange = () => change((p) => { p.lanes[i].channel = +channel.value; });
    const len = document.createElement('input'); len.type = 'number'; len.min = 1; len.max = lanes.STEPS; len.value = n;
    len.setAttribute('aria-label', `Lane ${i + 1} length`);
    len.onchange = () => { const v = Math.max(1, Math.min(lanes.STEPS, Math.round(+len.value) || lanes.LENGTH_DEFAULT)); change((p) => lanes.setLength(p.lanes[i], v)); };
    const rate = document.createElement('select'); rate.setAttribute('aria-label', `Lane ${i + 1} rate`);
    lanes.RATES.forEach((r, v) => option(rate, r, v, v === lanes.rate(l)));
    rate.onchange = () => change((p) => lanes.setRate(p.lanes[i], +rate.value));
    const dir = document.createElement('select'); dir.setAttribute('aria-label', `Lane ${i + 1} direction`);
    lanes.DIRECTIONS.forEach((d, v) => option(dir, d, v, v === l.direction));
    dir.onchange = () => change((p) => { p.lanes[i].direction = +dir.value; });
    // 2.0.3: swing (every second step later) and transpose
    const swing = document.createElement('select'); swing.setAttribute('aria-label', `Lane ${i + 1} swing`);
    swing.title = 'Swing: where every second step lands in its pair of steps; 50 straight, above it later, below it earlier';
    // 2.5.1: 25..75 in ones as the board's menu (an odd value from before 2.5.1 kept as it is until changed)
    const cur = Math.max(lanes.SWING_MIN, Math.min(lanes.SWING_MAX, l.swing)), swings = new Set([...lanes.swingValues(), cur]);
    [...swings].sort((a, b) => a - b).forEach((v) => option(swing, `Swing ${lanes.swingName(v)}`, v, v === cur));
    swing.onchange = () => change((p) => { p.lanes[i].swing = +swing.value; });
    const trans = document.createElement('select'); trans.setAttribute('aria-label', `Lane ${i + 1} transpose`);
    trans.title = l.kind === lanes.KIND.CHORDS ? 'Transpose: every chord this many scale steps up or down' : 'Transpose: every note this many semitones up or down';
    const span = Math.max(lanes.TRANSPOSE_MENU, Math.abs(l.transpose));
    for (let t = -span; t <= span; t++) option(trans, `Transpose ${lanes.transposeName(l.kind, t)}`, t, t === l.transpose);
    trans.onchange = () => change((p) => { p.lanes[i].transpose = +trans.value; });
    const mute = document.createElement('label'); mute.innerHTML = '<input type="checkbox"> Mute';
    mute.firstChild.checked = !!l.mute; mute.firstChild.onchange = (e) => change((p) => { p.lanes[i].mute = e.target.checked ? 1 : 0; });
    // 2.4.0: solo, follow the chord pads, the loop window
    const toggle = (label, tip, value, onset) => {
      const w = document.createElement('label'); w.innerHTML = `<input type="checkbox"> ${label}`; w.title = tip;
      w.firstChild.checked = !!value; w.firstChild.onchange = (e) => change((p) => onset(p.lanes[i], e.target.checked ? 1 : 0)); return w;
    };
    const solo = toggle('Solo', 'While any lane of the pattern is soloed, only soloed lanes play', l.solo, (x, v) => { x.solo = v; });
    const follow = toggle('Follow', 'Moves with the chord pad struck last, by its degree in the scale (melody and chord lanes)', l.follow, (x, v) => { x.follow = v; });
    // 2.6.0: Bend glide, how much of a step its pitch bend takes to get there from the last step's
    const glide = document.createElement('select'); glide.setAttribute('aria-label', `Lane ${i + 1} bend glide`);
    glide.title = 'Bend glide: a step\u2019s pitch bend is reached smoothly from the last step\u2019s, over this much of the step; Off jumps (a lane with a MIDI channel of its own)';
    lanes.GLIDES.forEach((name, v) => option(glide, `Bend glide ${name}`, v, v === (l.glide || 0)));
    glide.onchange = () => change((p) => { p.lanes[i].glide = +glide.value; });
    const [first, inside] = lanes.window(l);
    const loopAt = (label, value, onset) => {
      const x = document.createElement('input'); x.type = 'number'; x.min = 1; x.max = n; x.value = value;
      x.setAttribute('aria-label', `Lane ${i + 1} loop ${label}`);
      x.onchange = () => { const v = Math.max(1, Math.min(n, Math.round(+x.value) || 1)) - 1; change((p) => onset(p.lanes[i], v)); };
      return x;
    };
    const loop = document.createElement('span'); loop.className = 'loop';
    loop.title = 'Loop window: the steps the lane plays, within its length';
    loop.append('Loop ', loopAt('from', first + 1, (x, v) => { const [, m] = lanes.window(x), [f] = lanes.window(x); lanes.setWindow(x, v, Math.max(v, f + m - 1)); }),
      '–', loopAt('to', first + inside, (x, v) => { const [f] = lanes.window(x); lanes.setWindow(x, Math.min(f, v), v); }));
    const hits = document.createElement('input'); hits.type = 'number'; hits.min = 0; hits.max = n; hits.value = l.euclid;
    hits.setAttribute('aria-label', `Lane ${i + 1} Euclid hits`); hits.dataset.role = `hits${i}`;
    const rot = document.createElement('input'); rot.type = 'number'; rot.min = 0; rot.max = n - 1; rot.value = st.rotate[i] || 0;
    rot.setAttribute('aria-label', `Lane ${i + 1} Euclid rotate`);
    const fill = document.createElement('button'); fill.textContent = 'Fill';
    fill.title = 'Euclid: spread this many hits as evenly as they go over the lane, turned right by Rotate; steps already on keep what they play';
    fill.onclick = () => {
      const h = Math.max(0, Math.min(n, Math.round(+hits.value) || 0)), r = Math.max(0, Math.min(n - 1, Math.round(+rot.value) || 0));
      st.rotate[i] = r;
      const chosen = i === st.lane && l.steps[st.step] && l.steps[st.step].on ? { ...l.steps[st.step] } : lanes.brushDefault(l.kind);
      change((p) => lanes.euclidFill(p.lanes[i], h, r, chosen));
    };
    const euclid = document.createElement('span'); euclid.className = 'euclid'; euclid.append('Euclid ', hits, ' rotate ', rot, fill);
    // 2.0.3: new steps at random, or a few of them changed a little (as the board's Mutate)
    const amount = (label, values, def) => {
      const sel = document.createElement('select'); sel.setAttribute('aria-label', `Lane ${i + 1} ${label}`);
      values.forEach((v) => option(sel, `${v} %`, v, v === def)); return sel;
    };
    const density = amount('randomize density', [25, 50, 75, 100], 50), much = amount('mutate amount', [10, 20, 30, 40, 50], 30);
    const random = document.createElement('button'); random.textContent = 'Randomize';
    random.title = 'New steps over the lane at random, this many of them on (the steps there now go)';
    random.onclick = () => { if (confirm(`Replace the steps of lane ${i + 1} with random ones?`)) change((p) => lanes.randomize(p.lanes[i], +density.value), `Lane ${i + 1} randomized.`); };
    const mutate = document.createElement('button'); mutate.textContent = 'Mutate';
    mutate.title = 'Change about this many of the steps a little, once: another chord or note nearby, a velocity; drums on or off';
    mutate.onclick = () => {
      let moved = 0;
      change((p) => {
        const was = p.lanes[i].steps.map((s) => JSON.stringify(s));
        lanes.mutate(p.lanes[i], i, p.seed, +much.value, Math.floor(Math.random() * 65536));
        moved = p.lanes[i].steps.filter((s, k) => JSON.stringify(s) !== was[k]).length;
      }, () => (moved ? `Lane ${i + 1} mutated: ${moved} step${moved > 1 ? 's' : ''} changed.`
        : `Lane ${i + 1}: no step changed this time (only steps that are on change); press Mutate again.`));
    };
    const gen = document.createElement('span'); gen.className = 'lane-gen'; gen.append(random, density, ' ', mutate, much);
    const clear = document.createElement('button'); clear.textContent = 'Clear';
    clear.onclick = () => { if (confirm(`Take every step of lane ${i + 1} away?`)) change((p) => { p.lanes[i].steps = p.lanes[i].steps.map(lanes.emptyStep); p.lanes[i].euclid = 0; }); };
    const lenLabel = document.createElement('label'); lenLabel.append('Steps ', len);
    const tool = (text, tip, onclick, disabled = false) => {
      const b = document.createElement('button'); b.textContent = text; b.title = tip; b.onclick = onclick; b.disabled = disabled;
      b.setAttribute('aria-label', `${tip} (lane ${i + 1})`); return b;
    };
    const tools = document.createElement('span'); tools.className = 'lane-tools';
    tools.append(tool('◀', 'Move the steps one earlier', () => change((p) => lanes.rotate(p.lanes[i], -1))),
      tool('▶', 'Move the steps one later', () => change((p) => lanes.rotate(p.lanes[i], 1))),
      tool('Copy', 'Copy this lane', () => { st.clip = { lane: lanes.copyLane(l), from: i }; say(`Lane ${i + 1} copied.`, 'good'); draw(); }),
      tool('Paste', st.clip && st.clip.lane ? `Paste lane ${st.clip.from + 1} here: its steps and settings, its length too` : 'Copy a lane first',
        () => { const from = st.clip.from; change((p) => lanes.pasteLane(p, i, st.clip.lane), `Lane ${from + 1} pasted onto lane ${i + 1}.`); },
        !(st.clip && st.clip.lane)));
    head.append(title, kind, channel, lenLabel, rate, dir, swing, trans, mute, ...(v24() ? [solo, follow, loop] : []), ...(glideOk() ? [glide] : []), euclid, tools, gen, clear);
    const grid = document.createElement('div'); grid.className = 'steps'; grid.setAttribute('role', 'grid');
    grid.setAttribute('aria-label', `Lane ${i + 1} steps`);
    const [ra, rb] = i === st.lane && st.range !== null ? [Math.min(st.step, st.range), Math.max(st.step, st.range)] : [-1, -2];
    const [w0, wn] = lanes.window(l); // 2.4.0: steps outside the loop window paler
    l.steps.forEach((s, k) => {
      const b = document.createElement('button');
      b.className = 'step' + (s.on ? ' on' : '') + (k >= n ? ' past' : '') + (k % 4 === 0 ? ' beat' : '') +
        (i === st.lane && k === st.step ? ' chosen' : '') + (k >= ra && k <= rb ? ' inrange' : '') +
        (k < n && (k < w0 || k >= w0 + wn) ? ' outside' : '');
      const ring = s.on && stepColor(l, s);
      if (ring) b.style.setProperty('--ring', css(ring));
      b.textContent = s.on ? valueShort(l, s.value) : String(k + 1);
      b.title = `Lane ${i + 1}, step ${k + 1}` + (s.on ? `: ${valueName(l, s.value)}` : ': off') + (k >= n ? ' (past the lane\'s length)' : '');
      b.setAttribute('aria-pressed', String(s.on));
      b.onclick = (ev) => {
        if (ev.shiftKey && i === st.lane) { st.range = k === st.step ? null : k; draw(); return; } // a range of steps
        st.range = null; clickStep(i, k);
      };
      grid.appendChild(b);
    });
    const said = document.createElement('div'); said.className = 'status said'; // 2.0.6: always there, one line high
    box.append(head, said, grid);
    return box;
  }
  function clickStep(i, k) {
    const s = st.pattern.lanes[i].steps[k], chosen = i === st.lane && k === st.step;
    if (s.on && chosen) { change((p) => { p.lanes[i].steps[k] = lanes.emptyStep(); }); return; }
    if (!s.on) {
      const l = st.pattern.lanes[i], prev = i === st.lane && l.steps[st.step] && l.steps[st.step].on ? l.steps[st.step] : null;
      st.lane = i; st.step = k;
      change((p) => { p.lanes[i].steps[k] = prev ? { ...prev, on: true } : lanes.brushDefault(l.kind); });
      return;
    }
    st.lane = i; st.step = k; draw();
  }
  // Copy the range of steps chosen (or the chosen step), paste what was copied from the chosen step on.
  function clipRow(box) {
    const l = lane(), row = document.createElement('div'); row.className = 'row';
    const [a, b] = st.range === null ? [st.step, st.step] : [Math.min(st.step, st.range), Math.max(st.step, st.range)];
    const copy = document.createElement('button'); copy.textContent = a === b ? `Copy step ${a + 1}` : `Copy steps ${a + 1}–${b + 1}`;
    copy.onclick = () => { st.clip = { steps: lanes.copySteps(l, a, b), from: a, to: b }; say(`${b - a + 1} step${b > a ? 's' : ''} copied.`, 'good'); draw(); };
    row.appendChild(copy);
    if (st.clip && st.clip.steps) {
      const c = st.clip.steps, paste = document.createElement('button'), fits = Math.min(c.steps.length, lanes.STEPS - st.step);
      paste.textContent = `Paste ${c.steps.length} step${c.steps.length > 1 ? 's' : ''} from step ${st.step + 1}`;
      paste.disabled = c.kind !== l.kind;
      paste.title = c.kind !== l.kind ? `Copied from a ${lanes.KINDS[c.kind]} lane: paste them onto a ${lanes.KINDS[c.kind]} lane` :
        fits < c.steps.length ? `Only ${fits} fit before step 64` : 'Over the steps that are there';
      paste.onclick = () => { let n = 0; change((p) => { n = lanes.pasteSteps(p.lanes[st.lane], st.step, c); }, () => `${n} step${n > 1 ? 's' : ''} pasted.`); };
      row.appendChild(paste);
    }
    box.appendChild(row);
  }
  function stepPanel() {
    const l = lane(), s = l.steps[st.step], box = el('step'); box.innerHTML = '';
    el('stepTitle').textContent = `Lane ${st.lane + 1}, step ${st.step + 1}` + (st.range !== null ? ` to ${st.range + 1}` : '') + (s.on ? '' : ' (off)');
    const said = document.createElement('div'); said.className = 'status said'; box.appendChild(said);
    clipRow(box);
    if (!s.on) {
      const add = document.createElement('button'); add.textContent = 'Add this step';
      add.onclick = () => clickStep(st.lane, st.step); box.appendChild(add); return;
    }
    const set = (f) => change(() => f(st.pattern.lanes[st.lane].steps[st.step]));
    const field = (label, control) => { const w = document.createElement('label'); w.append(label + ' ', control); box.appendChild(w); return control; };
    const select = (label, n, name, value, onset) => {
      const sel = document.createElement('select');
      for (let v = 0; v < n; v++) option(sel, name(v), v, v === value);
      sel.onchange = () => set((x) => onset(x, +sel.value));
      return field(label, sel);
    };
    if (l.kind !== lanes.KIND.OFF) { // what it plays, first and easy to find
      const plays = select(l.kind === lanes.KIND.CHORDS ? 'Plays (chord pad)' : l.kind === lanes.KIND.DRUMS ? 'Plays (drum)' : 'Plays (melody pad)',
        lanes.valueCount(l.kind), (v) => valueName(l, v), s.value, (x, v) => { x.value = v; });
      plays.parentElement.classList.add('plays');
    }
    const vel = document.createElement('input'); vel.type = 'number'; vel.min = 1; vel.max = 127; vel.value = s.velocity || lanes.VELOCITY_DEFAULT;
    vel.onchange = () => { const v = Math.max(1, Math.min(127, Math.round(+vel.value) || lanes.VELOCITY_DEFAULT)); set((x) => { x.velocity = v === lanes.VELOCITY_DEFAULT ? 0 : v; }); };
    field('Velocity', vel);
    select('Gate', lanes.GATES, lanes.gateName, s.gate, (x, v) => { x.gate = v; });
    const tie = document.createElement('input'); tie.type = 'checkbox'; tie.checked = s.tie;
    tie.onchange = () => set((x) => { x.tie = tie.checked; });
    field('Tie to the next', tie);
    select('Chance', lanes.PROBABILITIES, lanes.chanceName, s.probability, (x, v) => { x.probability = v; });
    select('Ratchet', lanes.RATCHETS, lanes.ratchetName, s.ratchet, (x, v) => { x.ratchet = v; });
    select('Condition', lanes.CONDITIONS.length, (v) => lanes.CONDITIONS[v], s.condition, (x, v) => { x.condition = v; });
    select('Nudge', lanes.NUDGE_MAX - lanes.NUDGE_MIN + 1, (v) => lanes.nudgeName(v + lanes.NUDGE_MIN), lanes.nudgeOf(s.nudge) - lanes.NUDGE_MIN,
      (x, v) => { x.nudge = lanes.nudgeBits(v + lanes.NUDGE_MIN); }); // 2.0.3
    const semis = bendSemis(); // 2.4.12 (roadmap C): pitch bend, in eighths of a semitone of the board's Bend range
    if (semis) {
      const bend = document.createElement('input'); bend.type = 'number'; bend.step = 0.125; bend.min = -semis; bend.max = semis;
      bend.value = lanes.bendEighths(s.bend || 0, semis) / 8;
      const why = bendShared(st.lane);
      bend.disabled = !!why && !s.bend;
      bend.title = why ? `No bend here: ${why} (pitch bend moves a whole channel)` : `Semitones, −${semis} … +${semis} (Settings → Steps → Bend range)`;
      bend.onchange = () => { const e = Math.round(8 * (+bend.value || 0)); set((x) => { x.bend = why ? 0 : lanes.bendOf(e, semis); }); };
      field('Bend (semitones)', bend);
      if (why) { const f = document.createElement('p'); f.className = 'facts'; f.textContent = `Bend: not on this lane, ${why}. Pitch bend moves a whole MIDI channel, so a lane bends only on a channel of its own.`; box.appendChild(f); }
    }
    if (l.kind === lanes.KIND.CHORDS) { // the chord's own edits, as a progression keeps them
      const e = lanes.editOf(s.edit);
      for (const [name, bit] of [['7th', 2], ['9th', 4], ['6th', 1]]) {
        const c = document.createElement('input'); c.type = 'checkbox'; c.checked = !!(e.add & bit);
        c.onchange = () => set((x) => { const d = lanes.editOf(x.edit); d.add = c.checked ? d.add | bit : d.add & ~bit; x.edit = lanes.editByte(d); });
        field(`Add ${name}`, c);
      }
      select('Sus', 3, (v) => ['None', 'Sus2', 'Sus4'][v], e.sus / 2, (x, v) => { const d = lanes.editOf(x.edit); d.sus = 2 * v; x.edit = lanes.editByte(d); });
      if (s.scale) { const f = document.createElement('p'); f.className = 'facts'; f.textContent = 'Borrowed from another scale (recorded on the TB): kept as it is.'; box.appendChild(f); }
    }
  }
  function drawPlay() {
    el('play').textContent = st.playing ? '■ Stop' : '▶ Play';
    el('play').disabled = !st.pattern || !playItem();
  }
  // 2.4.0: the patterns (the one shown, the one playing, play it, copy it) and the song.
  // 2.5.2: the shown pattern's groove (the board's Settings → Patterns while it plays): Swing added to every lane's, the
  // grid it swings on, Density, Humanize. Stored in the pattern's record (lanes.encodeExtras).
  function grooveRow() {
    const box = document.createElement('div'); box.className = 'groove';
    const g = { ...lanes.emptyGroove(), ...st.pattern.groove };
    const pick = (label, title, values, name, value, set) => {
      const sel = document.createElement('select'); sel.setAttribute('aria-label', `Pattern ${st.shown + 1} ${label}`); sel.title = title;
      values.forEach((v) => option(sel, `${label} ${name(v)}`, v, v === value));
      sel.onchange = () => change((p) => { p.groove = { ...lanes.emptyGroove(), ...p.groove }; set(p.groove, +sel.value); });
      return sel;
    };
    const swings = new Set([...lanes.swingValues(), Math.max(lanes.SWING_MIN, Math.min(lanes.SWING_MAX, g.swing))]);
    const range = (lo, hi) => Array.from({ length: hi - lo + 1 }, (_, k) => lo + k);
    box.append('Groove: ',
      pick('Swing', 'Added to every lane\'s own swing (kept within 25 … 75 %); 50 % adds nothing', [...swings].sort((a, b) => a - b),
        lanes.swingName, g.swing, (x, v) => { x.swing = v; }), ' ',
      pick('Grid', 'What swings: each lane\'s own pairs of steps, or pairs of 8ths or 16ths whatever each lane\'s rate', range(0, lanes.GRIDS.length - 1),
        (v) => lanes.GRIDS[v], g.grid, (x, v) => { x.grid = v; }), ' ',
      pick('Density', 'Every step\'s chance of playing scaled: below 100 % steps drop out, above it steps with a chance play more often',
        range(lanes.DENSITY_MIN, lanes.DENSITY_MAX), lanes.densityName, g.density, (x, v) => { x.density = v; }), ' ',
      pick('Humanize', 'A little random timing and velocity on every lane\'s notes', range(0, lanes.HUMANIZE_MAX),
        lanes.humanizeName, g.humanize, (x, v) => { x.humanize = v; }));
    return box;
  }
  function drawPatterns() {
    const bar = el('patterns'), row = el('song');
    bar.classList.toggle('hidden', !v24()); row.classList.toggle('hidden', !v24());
    if (!v24()) return;
    bar.innerHTML = '<b>Pattern</b> ';
    const playing = st.song ? st.song.current : 0;
    for (let p = 0; p < lanes.PATTERNS; p++) {
      const b = document.createElement('button'); b.textContent = (p === playing ? '▶ ' : '') + `P${p + 1}`;
      b.className = p === st.shown ? 'chosen' : ''; b.setAttribute('aria-pressed', String(p === st.shown));
      b.title = p === playing ? 'Playing on the TB' : `Show pattern ${p + 1}`;
      b.onclick = () => { if (p !== st.shown) { st.shown = p; st.pattern = null; read(); } };
      bar.appendChild(b);
    }
    const play = document.createElement('button'); play.textContent = `Play P${st.shown + 1}`; play.disabled = !patternItem() || st.shown === playing;
    play.title = 'The TB plays this pattern next: at once while stopped, else when the one playing comes to its end (the song stops)';
    play.onclick = () => {
      const it = patternItem(); if (!it) return;
      it.value = st.shown;
      link.send(tb.setItem(it.id, st.shown)).then(() => { say(`P${st.shown + 1} plays next.`, 'good'); setTimeout(() => readSong().then(drawPatterns), 300); },
        (e) => say(`Not sent: ${e.message}.`, 'err'));
    };
    const to = document.createElement('select'); to.setAttribute('aria-label', 'Copy to pattern');
    for (let p = 0; p < lanes.PATTERNS; p++) if (p !== st.shown) option(to, `P${p + 1}`, p, false);
    const copy = document.createElement('button'); copy.textContent = 'Copy to';
    copy.title = 'Copy this pattern (every lane, its steps and settings) over another';
    copy.onclick = async () => {
      const target = +to.value;
      if (!confirm(`Copy pattern ${st.shown + 1} over pattern ${target + 1}?`)) return;
      try {
        for (let slot = 0; slot < lanes.RECORDS; slot++) { const r = lanes.recordOf(target, slot); await link.send(tb.setRecordQuiet(r.kind, r.slot, lanes.encodeChunk(st.pattern, slot))); }
        await link.send(tb.setRecordQuiet(tb.KIND.PATTERN, target, lanes.encodeExtras(st.pattern)));
        say(`Pattern ${st.shown + 1} copied to pattern ${target + 1}.`, 'good');
      } catch (e) { say(`Not copied: ${e.message}.`, 'err'); }
    };
    const said = document.createElement('span'); said.className = 'status said';
    bar.append(' ', play, ' ', copy, to, said);
    if (grooveOk() && st.pattern) bar.append(grooveRow()); // 2.5.2
    // the song
    row.innerHTML = '';
    const song = st.song || { current: 0, on: false, links: [] };
    const on = document.createElement('label'); on.innerHTML = '<input type="checkbox"> <b>Song</b>'; on.firstChild.checked = song.on;
    on.title = 'Play the patterns below in turn, each for its time round, round and round (5 + root buttons on the step screen too)';
    const links = document.createElement('span'); links.className = 'links';
    const edit = (f) => { f(song); st.song = song; sendSong(); drawPatterns(); };
    on.firstChild.onchange = (e) => edit((x) => { x.on = e.target.checked; });
    song.links.forEach((p, k) => {
      const sel = document.createElement('select'); sel.setAttribute('aria-label', `Song step ${k + 1}`);
      for (let q = 0; q < lanes.PATTERNS; q++) option(sel, `P${q + 1}`, q, q === p);
      sel.onchange = () => edit((x) => { x.links[k] = +sel.value; });
      links.append(sel, k + 1 < song.links.length ? ' › ' : '');
    });
    const add = document.createElement('button'); add.textContent = '+'; add.title = 'Add a pattern to the song'; add.disabled = song.links.length >= lanes.SONG;
    add.onclick = () => edit((x) => { x.links.push(x.links.length ? x.links[x.links.length - 1] : st.shown); });
    const less = document.createElement('button'); less.textContent = '−'; less.title = 'Take the last pattern away'; less.disabled = !song.links.length;
    less.onclick = () => edit((x) => { x.links.pop(); });
    const said2 = document.createElement('span'); said2.className = 'status said';
    // board test S11: + and − stay put when the first pattern comes (a pattern list's room kept), the hint after them
    if (!song.links.length) {
      const room = document.createElement('select'); room.className = 'room'; room.disabled = true; room.tabIndex = -1;
      room.setAttribute('aria-hidden', 'true');
      for (let q = 0; q < lanes.PATTERNS; q++) option(room, `P${q + 1}`, q, !q); // as wide as a real one
      links.append(room);
    }
    row.append(on, ' ', links, ' ', add, less, ...(song.links.length ? [] : [' ', 'no patterns yet']), said2);
    say.again(); // the bar and the row were drawn anew: a message in them shows on (board test S11: it went at once)
  }
  async function readSong() {
    const reply = await link.call(tb.getRecord(tb.KIND.SONG, 0));
    st.song = reply.status === 0 ? lanes.decodeSong(tb.decodeRecord(reply.data).words) : { current: 0, on: false, links: [] };
  }
  async function sendSong() {
    try { await link.ok(tb.setRecord(tb.KIND.SONG, 0, lanes.encodeSong(st.song))); say(st.song.on ? 'Song on: sent to the TB.' : 'Song sent to the TB.', 'good'); }
    catch (e) { say(`Not sent: ${e.message}.`, 'err'); }
  }
  function draw() {
    el('body').classList.toggle('hidden', !st.pattern);
    drawPlay();
    if (!st.pattern) return;
    drawPatterns();
    const list = el('lanes'), y = window.scrollY; // 2.0.6: the page stays where it is (board test Y5)
    // 2.4.0: the chosen lane (the one just clicked) stays where it is on the screen, its step's options opening
    // below it, however much moves above it (board test S2: the options moving from the lane above shifted it up)
    const laneBox = () => list.querySelectorAll(':scope > .lane')[st.lane]; // (the step's options are children too)
    const was = laneBox(), top = was ? was.getBoundingClientRect().top : null;
    el('body').append(el('stepTitle'), el('step')); // out of the lanes before they are drawn again
    list.innerHTML = '';
    for (let i = 0; i < st.pattern.lanes.length; i++) list.appendChild(laneRow(i));
    list.children[st.lane].after(el('stepTitle'), el('step')); // 1.9.4: the chosen step's options under its lane
    stepPanel();
    say.again();
    if (window.scrollY !== y) window.scrollTo(window.scrollX, y);
    if (top !== null && laneBox()) { const moved = laneBox().getBoundingClientRect().top - top; if (moved) window.scrollBy(0, moved); }
  }
  el('play').onclick = () => {
    const it = playItem();
    if (!it) return;
    st.playing = !st.playing; it.value = st.playing ? 1 : 0;
    link.send(tb.setItem(it.id, st.playing ? 1 : 0)).then(() => say(st.playing ? 'Playing.' : 'Stopped.', 'good'), (e) => say(`Not sent: ${e.message}.`, 'err'));
    draw();
  };
  // quiet: the board's steps changed (STATE): read them again without a word, and draw only if they differ.
  async function read(quiet = false) {
    if (!supported() || st.busy) return;
    st.busy = true; el('read').disabled = true; if (!quiet) say('Reading the TB\'s steps…');
    const edits = st.edits; // a click while reading wins over what was read
    const before = st.pattern && JSON.stringify(st.pattern);
    try {
      const records = [], shown = st.shown;
      if (v24()) await readSong();
      for (let slot = 0; slot < recordCount(); slot++) {
        const r = where(slot), reply = await link.call(tb.getRecord(r.kind, r.slot));
        if (reply.status === 0) records.push(tb.decodeRecord(reply.data).words);
        else if (reply.status === 7) records.push(null);
        else throw new Error(tb.STATUS[reply.status] || `status ${reply.status}`);
      }
      let extras = null;
      if (v24()) {
        const reply = await link.call(tb.getRecord(tb.KIND.PATTERN, shown));
        if (reply.status === 0) extras = tb.decodeRecord(reply.data).words;
        else if (reply.status !== 7) throw new Error(tb.STATUS[reply.status] || `status ${reply.status}`);
      }
      if ((!st.pattern || st.edits === edits) && shown === st.shown) {
        st.pattern = lanes.patternFromRecords(records);
        if (extras && !lanes.decodeExtras(extras, st.pattern)) throw new Error(`pattern ${shown + 1}'s loop windows unreadable`);
        st.sent = Array.from({ length: recordCount() }, (_, slot) => lanes.encodeChunk(st.pattern, slot));
        st.sentExtras = lanes.encodeExtras(st.pattern);
        if (st.lane >= st.pattern.lanes.length) st.lane = 0;
      }
      if (!quiet) {
        try { st.names = tb.decodeChordNames(await link.ok(tb.chordNames())); } catch { st.names = null; }
        try { await readBoard(); } catch { st.board = null; }
        const it = playItem();
        if (it) { it.value = tb.decodeItem(await link.ok(tb.item(it.id))).value; st.playing = it.value === 1; }
        say('');
      }
      st.readAt = Date.now();
    } catch (e) { if (!quiet) say(`Could not read the steps: ${e.message}.`, 'err'); }
    st.busy = false; el('read').disabled = !supported();
    if (!quiet || JSON.stringify(st.pattern) !== before) draw();
  }
  el('read').onclick = read;
  return {
    async connected() {
      el('old').classList.toggle('hidden', supported() || !info());
      el('read').disabled = !supported();
      st.pattern = null; draw();
      if (supported()) await read();
    },
    shown() { if (supported() && Date.now() - st.readAt > READ_AGAIN_MS) read(); },
    // STATE said what changed (1.9.4): Play follows the Settings tab's value (read again there); the names and colors
    // after a change of the settings; the steps when the board's changed and nothing from here is on its way.
    // False: not now (ask again next time).
    async refresh({ lanes: lanesChanged, settings: settingsChanged }) {
      const it = playItem();
      if (it && (it.value === 1) !== st.playing) { st.playing = it.value === 1; drawPlay(); }
      if (!supported() || !st.pattern) return true;
      if (settingsChanged && !st.busy) { // the key or a chord pad may have changed
        try { st.names = tb.decodeChordNames(await link.ok(tb.chordNames())); } catch { /* keep the old */ }
        try { await readBoard(); } catch { /* keep the old */ }
        if (!lanesChanged) draw();
      }
      if (!lanesChanged) return true;
      if (st.busy || st.pending || Date.now() - st.editedAt < SETTLE_MS) return false;
      await read(true);
      return true;
    },
  };
}
