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

// Output ports, TheoryBoard first (all ports if none is named so).
export function boardOutputs(midi) {
  const outs = [...midi.outputs.values()];
  const named = outs.filter((p) => /theoryboard/i.test(p.name));
  return named.length ? named : outs;
}
// The input belonging to an output (same name), else any TheoryBoard input.
export function inputFor(midi, output) {
  const ins = [...midi.inputs.values()];
  return ins.find((p) => output && p.name === output.name) || ins.find((p) => /theoryboard/i.test(p.name)) || null;
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
    async ok(message, timeout) {
      const reply = await link.call(message, timeout);
      if (reply.status !== 0) throw new Error(tb.STATUS[reply.status] || `status ${reply.status}`);
      return reply.data;
    },
  };
  return link;
}
