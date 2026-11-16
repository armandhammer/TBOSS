// SPDX-License-Identifier: GPL-3.0-or-later - Standard MIDI Files for the web editor (TBOSS 2.6.6): a progression or a step
// pattern saved as a .mid file to drag into a DAW, and a .mid file's notes (or a controller's values) read into a lane.
// Pure functions, no Web MIDI or page: tests/webapp/test_protocol.mjs checks them. Format 1 files, PPQ ticks a beat.
import * as lanes from './tb-lanes.js';

export const PPQ = 480;
// A lane's rate (tb-lanes.js RATES, as the board's ui_rate_names) in beats a step.
export const RATE_BEATS = Object.freeze([1, 1 / 2, 1 / 3, 1 / 4, 1 / 6, 1 / 8, 2 / 3, 1 / 12, 1 / 5, 1 / 7, 3 / 4, 3 / 8]);
// A progression's unit (tb-chords.js UNIT_NAMES) in beats.
export const UNIT_BEATS = Object.freeze([1, 2, 4, 8]);

const vlq = (n) => { const out = [n & 0x7f]; while ((n >>= 7)) out.unshift((n & 0x7f) | 0x80); return out; };
const u32 = (n) => [(n >>> 24) & 255, (n >>> 16) & 255, (n >>> 8) & 255, n & 255];
const text = (s) => [...new TextEncoder().encode(s)];

// tracks: [{ name, events: [{ tick, data: [status, ...] }] }]; the first track also gets the tempo and 4/4. Events at the
// same tick keep their order, except that a note's end comes before another's start (so a note struck again sounds).
export function writeMidi(tracks, { bpm = 120, ppq = PPQ } = {}) {
  const chunks = [];
  const chunk = (id, body) => chunks.push(...text(id), ...u32(body.length), ...body);
  chunk('MThd', [0, 1, (tracks.length >> 8) & 255, tracks.length & 255, (ppq >> 8) & 255, ppq & 255]);
  const tempo = Math.round(60000000 / Math.max(20, Math.min(300, bpm)));
  tracks.forEach((t, i) => {
    const head = [{ tick: 0, data: [0xff, 0x03, ...vlq(text(t.name || '').length), ...text(t.name || '')] }];
    if (!i) head.push({ tick: 0, data: [0xff, 0x51, 3, (tempo >> 16) & 255, (tempo >> 8) & 255, tempo & 255] },
      { tick: 0, data: [0xff, 0x58, 4, 4, 2, 24, 8] });
    const isOff = (d) => (d[0] & 0xf0) === 0x80 || ((d[0] & 0xf0) === 0x90 && d[2] === 0);
    const events = [...head, ...t.events.map((e, n) => ({ ...e, tick: Math.max(0, Math.round(e.tick)), n }))]
      .sort((a, b) => a.tick - b.tick || (b.data[0] === 0xff) - (a.data[0] === 0xff) || isOff(b.data) - isOff(a.data) || (a.n ?? -1) - (b.n ?? -1));
    const body = []; let at = 0;
    for (const e of events) { body.push(...vlq(e.tick - at), ...e.data); at = e.tick; }
    body.push(0, 0xff, 0x2f, 0);
    chunk('MTrk', body);
  });
  return new Uint8Array(chunks);
}

// A file's notes and controllers: { ppq, notes: [{ tick, end, note, velocity, channel, track }], ccs: [{ tick, cc, value,
// channel, track }], end (the last tick of anything) }. Throws on something that is not a MIDI file. A time-code
// division (SMPTE) is read as 480 a beat (DAWs write beats).
export function readMidi(bytes) {
  const b = bytes instanceof Uint8Array ? bytes : new Uint8Array(bytes);
  const id = (at) => String.fromCharCode(b[at], b[at + 1], b[at + 2], b[at + 3]);
  const be = (at, n) => { let v = 0; for (let i = 0; i < n; i++) v = v * 256 + b[at + i]; return v; };
  if (b.length < 14 || id(0) !== 'MThd') throw new Error('not a MIDI file');
  const headLen = be(4, 4), division = be(12, 2), ppq = division & 0x8000 ? PPQ : division || PPQ;
  const notes = [], ccs = []; let end = 0, at = 8 + headLen, track = 0;
  while (at + 8 <= b.length) {
    const len = be(at + 4, 4), stop = Math.min(b.length, at + 8 + len);
    if (id(at) !== 'MTrk') { at = stop; continue; }
    let p = at + 8, tick = 0, running = 0;
    const open = new Map();
    // 2.7.1 (review TBR-041): a variable-length number has four bytes at most (a fifth overflowed the 32-bit shift and
    // came out negative: a meta event's length then moved p back to the event's start, forever), and a meta or SysEx
    // event's length must end inside the track, so every event moves p forward and the loop ends at stop.
    const read = () => {
      let v = 0, c, n = 0;
      do { if (++n > 4) throw new Error('not a MIDI file'); c = b[p++]; v = v * 128 + (c & 0x7f); } while (c & 0x80 && p < stop);
      return v;
    };
    const skip = (n) => { if (p + n > stop) throw new Error('not a MIDI file'); p += n; };
    while (p < stop) {
      tick += read();
      let status = b[p];
      if (status & 0x80) p++; else status = running;
      if (status === 0xff) { p++; skip(read()); continue; }
      if (status === 0xf0 || status === 0xf7) { skip(read()); continue; }
      running = status;
      const kind = status & 0xf0, ch = status & 15, d1 = b[p], d2 = kind === 0xc0 || kind === 0xd0 ? 0 : b[p + 1];
      p += kind === 0xc0 || kind === 0xd0 ? 1 : 2;
      const key = ch * 128 + d1;
      if (kind === 0x90 && d2) {
        if (open.has(key)) { const o = open.get(key); o.end = tick; open.delete(key); }
        const n = { tick, end: tick, note: d1, velocity: d2, channel: ch, track };
        notes.push(n); open.set(key, n);
      } else if (kind === 0x80 || kind === 0x90) {
        const o = open.get(key); if (o) { o.end = tick; open.delete(key); }
      } else if (kind === 0xb0) ccs.push({ tick, cc: d1, value: d2, channel: ch, track });
      end = Math.max(end, tick);
    }
    for (const o of open.values()) o.end = Math.max(o.tick, tick);
    at = stop; track++;
  }
  notes.sort((x, y) => x.tick - y.tick || x.note - y.note);
  ccs.sort((x, y) => x.tick - y.tick);
  return { ppq, notes, ccs, end };
}

const note = (tick, ch, n, v, len) => [{ tick, data: [0x90 | ch, n, Math.max(1, Math.min(127, v))] }, { tick: tick + Math.max(1, len), data: [0x80 | ch, n, 0] }];

// A chord's notes as a close voicing: the root in octave 3 (C3 = 48), each tone above it; a bass other than the root
// an octave below the root. chord: tb-chords.js { root, set, bass }.
export function chordNotes(chord, octave = 3) {
  const root = 12 * (octave + 1) + chord.root, out = [];
  for (let s = 0; s < 24; s++) if ((chord.set >> s) & 1) out.push(root + s);
  if (chord.bass !== null && chord.bass !== undefined) out.unshift(12 * octave + chord.bass);
  return out;
}

// A progression as a file: chords [{ chord (or null: a rest), units }] (tb-chords.js progressionChords), unit (0..3).
export function progressionMidi(chordList, unit, { bpm = 120, channel = 0, velocity = 100, name = 'Progression' } = {}) {
  const events = [], len = UNIT_BEATS[unit] * PPQ;
  let tick = 0;
  for (const c of chordList) {
    if (c.chord) for (const n of chordNotes(c.chord)) events.push(...note(tick, channel, n, velocity, c.units * len - 1));
    tick += c.units * len;
  }
  return writeMidi([{ name, events }], { bpm });
}

// The steps a lane plays in turn, from its loop window (tb-lanes.js window) and direction: forward, reverse, ping-pong
// (the ends once each), random as forward (the board's order is its own); 2.6.8 every path and switch, skipped steps left out.
export const laneOrder = (l) => lanes.timeline(l).order;

// A pattern as a file: a track per lane that plays (not Off, not muted, any step on), `bars` bars of 4/4 of each lane
// looping on its own. pitches(i, lane, step) gives a step's notes (chord, melody and drum lanes); channel(i, lane) its MIDI
// channel (0..15). Chance, conditions and Mutate are left out (every step is written); ratchets, nudge, a lane's swing
// and the groove's swing (Each lane grid), gate, ties (legato) and pitch bends are in; a CC lane writes its controller.
export function patternMidi(pattern, { bars = 4, bpm = 120, pitches, channel } = {}) {
  const tracks = [], total = bars * 4 * PPQ, soloed = pattern.lanes.some((l) => l.solo);
  const groove = { ...lanes.emptyGroove(), ...pattern.groove };
  pattern.lanes.forEach((l, i) => {
    if (l.kind === lanes.KIND.OFF || l.mute || (soloed && !l.solo) || !l.steps.slice(0, lanes.length(l)).some((s) => s.on)) return;
    const step = RATE_BEATS[lanes.rate(l)] * PPQ, { order, at, units } = lanes.timeline(l), ch = channel(i, l) & 15, events = [];
    if (!units) return; // 2.6.8: every step skipped
    const swing = Math.max(-50, Math.min(50, (l.swing || 0) + (groove.grid === 0 ? groove.swing || 0 : 0)));
    const bends = l.kind !== lanes.KIND.CC && l.steps.some((s) => s.on && s.bend);
    let bent = 0;
    // 2.6.8: each visit at its own place in steps (lasts and skips), swing on the lane's odd steps, its length its lasts
    for (let n = 0; ; n++) {
      const k = n % order.length, unit = Math.floor(n / order.length) * units + at[k];
      if (unit * step >= total) break;
      const s = l.steps[order[k]], length = lanes.lastsOf(s) * step;
      if (!s || !s.on) continue;
      const start = unit * step + (unit % 2 ? (swing / 100) * step : 0) + (lanes.nudgeOf(s.nudge || 0) / 16) * step;
      if (start >= total) break;
      if (l.kind === lanes.KIND.CC) { events.push({ tick: start, data: [0xb0 | ch, lanes.ccOf(l), s.value & 127] }); continue; }
      if (bends && (s.bend || 0) !== bent) {
        bent = s.bend || 0; const v = Math.max(0, Math.min(16383, 8192 + 2 * bent));
        events.push({ tick: Math.max(0, start - 1), data: [0xe0 | ch, v & 127, v >> 7] });
      }
      const strikes = (s.ratchet || 0) + 1, each = length / strikes, gate = s.tie ? 1 : (s.gate ? s.gate / 10 : lanes.GATE_PCT / 100);
      const notes = pitches(i, l, s) || [];
      for (let r = 0; r < strikes; r++) {
        for (const p of notes) if (p >= 0 && p <= 127) events.push(...note(start + r * each, ch, p, s.velocity || lanes.VELOCITY_DEFAULT, Math.round(each * gate) - (s.tie ? 0 : 1)));
      }
    }
    if (bends && bent) events.push({ tick: total, data: [0xe0 | ch, 0, 64] });
    tracks.push({ name: `Lane ${i + 1} ${lanes.KINDS[l.kind]}`, events });
  });
  return tracks.length ? writeMidi(tracks, { bpm }) : null;
}

// A file read into steps for a lane of `kind` at `rate` (tb-lanes.js rate index): each note's start rounded to the nearest
// step (the rest kept as the step's nudge, -8..7 sixteenths of a step), its velocity, and its length as the step's gate
// (10 % .. 100 %). Chord lanes take the notes that start on a step together (chordValue(notes) -> { value, edit, scale }
// or null), melody lanes the step's highest note, drum lanes its lowest (value(note) -> a value or null); a CC lane the
// values of controller cc (the last in each step). Steps from the first bar the file has notes in, up to 64; the length a
// whole number of bars when the rate allows. { steps, length, used, skipped } (skipped: notes with no place on the lane).
export function laneFromMidi(file, { kind, rate, cc = 1, value = (n) => n, chordValue = () => null }) {
  const stepTicks = RATE_BEATS[rate] * file.ppq, bar = 4 * file.ppq;
  const events = kind === lanes.KIND.CC ? file.ccs.filter((c) => c.cc === cc) : file.notes;
  const steps = Array.from({ length: lanes.STEPS }, lanes.emptyStep);
  if (!events.length) return { steps, length: lanes.LENGTH_DEFAULT, used: 0, skipped: 0 };
  const from = Math.floor(events[0].tick / bar) * bar; // the first bar with anything in it
  const groups = new Map();
  for (const e of events) {
    const exact = (e.tick - from) / stepTicks, k = Math.round(exact);
    if (k >= lanes.STEPS) continue;
    if (!groups.has(k)) groups.set(k, []);
    groups.get(k).push({ ...e, off: exact - k });
  }
  let used = 0, skipped = 0, last = 0;
  for (const [k, group] of groups) {
    const nudge = Math.max(lanes.NUDGE_MIN, Math.min(lanes.NUDGE_MAX, Math.round(16 * group[0].off)));
    const s = { ...lanes.emptyStep(), on: true, nudge: lanes.nudgeBits(nudge) };
    if (kind === lanes.KIND.CC) { s.value = group[group.length - 1].value & 127; s.nudge = 0; }
    else {
      const pick = kind === lanes.KIND.MELODY ? group.reduce((a, n) => (n.note > a.note ? n : a)) : group.reduce((a, n) => (n.note < a.note ? n : a));
      let got = null;
      if (kind === lanes.KIND.CHORDS) got = chordValue([...new Set(group.map((n) => n.note))]);
      else { const v = value(pick.note); got = v === null || v === undefined ? null : { value: v }; }
      if (!got) { skipped += group.length; continue; }
      Object.assign(s, got);
      const vel = Math.max(...group.map((n) => n.velocity));
      s.velocity = vel === lanes.VELOCITY_DEFAULT ? 0 : vel;
      const g = Math.max(1, Math.min(10, Math.round((10 * (pick.end - pick.tick)) / stepTicks)));
      s.gate = g * 10 === lanes.GATE_PCT ? 0 : g;
    }
    steps[k] = s; used++; last = Math.max(last, k);
  }
  const perBar = 4 / RATE_BEATS[rate];
  let length = last + 1;
  if (Number.isInteger(perBar)) length = Math.ceil(length / perBar) * perBar;
  return { steps, length: Math.max(1, Math.min(lanes.STEPS, length)), used, skipped };
}

// The one function here that touches the page: bytes offered to the browser as a file to save.
export function saveFile(bytes, name) {
  const url = URL.createObjectURL(new Blob([bytes], { type: 'audio/midi' })), a = document.createElement('a');
  a.href = url; a.download = name; document.body.appendChild(a); a.click(); a.remove();
  setTimeout(() => URL.revokeObjectURL(url), 1000);
}
