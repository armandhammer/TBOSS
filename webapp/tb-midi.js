// SPDX-License-Identifier: GPL-3.0-or-later - Web MIDI link to a board running TBOSS: one SysEx
// request at a time, each reply matched to its command (protocol: tb-protocol.js).
import * as tb from './tb-protocol.js';

export async function access() {
  if (!navigator.requestMIDIAccess) throw new Error('this browser has no Web MIDI; use Chrome or Edge on a computer');
  try {
    return await navigator.requestMIDIAccess({ sysex: true });
  } catch {
    throw new Error('MIDI access was refused; allow MIDI (with SysEx) for this page in the browser\'s site settings');
  }
}

// 2.0.6: TBOSS 2.0.6 and later has two ports, the board's (TheoryBoard MIDI, for music software) and the editor's
// (TheoryBoard Editor; Windows' older names call it MIDIOUT2 / MIDIIN2 (TheoryBoard MIDI)). The board answers on the
// port it was asked on, so music software on the first never sees the editor's SysEx.
export const editorPort = (p) => /editor|MIDI(?:IN|OUT)2 \(|\(port 2\)|\[2\]|\s2$/i.test(p.name); // Live: "(Port 2)"
const named = (p) => /theoryboard/i.test(p.name);
// Output ports named TheoryBoard, the editor's first (all ports if none is named so).
export function boardOutputs(midi) {
  const outs = [...midi.outputs.values()], board = outs.filter(named);
  return board.length ? [...board.filter(editorPort), ...board.filter((p) => !editorPort(p))] : outs;
}
// The input belonging to an output (same name, or MIDIIN2 for MIDIOUT2), else a TheoryBoard input of the same kind
// (the editor's or the board's), else any TheoryBoard input.
export function inputFor(midi, output) {
  const ins = [...midi.inputs.values()];
  if (!output) return ins.find(named) || null;
  const twin = output.name.replace(/^MIDIOUT/i, 'MIDIIN');
  return ins.find((p) => p.name === output.name) || ins.find((p) => p.name === twin) ||
    ins.find((p) => named(p) && editorPort(p) === editorPort(output)) || ins.find(named) || null;
}

export function caller() {
  let out = null, inp = null, waiting = null, queue = Promise.resolve();
  function onMessage(event) {
    if (!waiting || event.data[0] !== 0xf0) return;
    let reply;
    try { reply = tb.parseReply(event.data, waiting.cmd); } catch (e) { reply = e; }
    if (!reply) return;
    const w = waiting; waiting = null; clearTimeout(w.timer);
    reply instanceof Error ? w.reject(reply) : w.resolve(reply);
  }
  const link = {
    use(output, input) {
      if (inp) inp.onmidimessage = null;
      out = output || null; inp = input || null;
      if (inp) inp.onmidimessage = onMessage;
    },
    get output() { return out; },
    // Resolves with { status, data }; rejects on timeout or a corrupted reply.
    call(message, timeout = 2000) {
      const run = () => new Promise((resolve, reject) => {
        if (!out) { reject(new Error('not connected')); return; }
        waiting = { cmd: message[4], resolve, reject, timer: setTimeout(() => { waiting = null; reject(new Error('no reply from the TB')); }, timeout) };
        try { out.send(message); } catch (e) { clearTimeout(waiting.timer); waiting = null; reject(e); }
      });
      const result = queue.then(run, run);
      queue = result.catch(() => {});
      return result;
    },
    // A request without a reply (PREVIEW, SET_ITEM, a quiet SET_RECORD), sent after any still waiting.
    send(message) {
      const run = () => { if (!out) throw new Error('not connected'); out.send(message); };
      const result = queue.then(run, run);
      queue = result.catch(() => {});
      return result;
    },
    async ok(message, timeout) {
      const reply = await link.call(message, timeout);
      if (reply.status !== 0) throw new Error(tb.STATUS[reply.status] || `status ${reply.status}`);
      return reply.data;
    },
  };
  return link;
}
