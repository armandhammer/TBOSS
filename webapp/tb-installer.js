// SPDX-License-Identifier: GPL-3.0-or-later - install firmware through the TheoryBoard's
// built-in USB update mode (the resident loader), from a browser with WebHID.
// Pure protocol code: the transport is passed in, so tests can use a simulated loader.
//
// The board's loader speaks 64-byte HID reports (USB 0333:0333). Every command is an
// 8-character name followed by a fixed 56-character constant; numbers are a little-endian
// 32-bit value padded with zeros; firmware data travels in 64-byte blocks.
// Only these commands are ever sent (documented in docs/installer-protocol.md):
//   XCFILNFS  open the loader's metadata commands      DUMPMETA  report its metadata (reads only)
//   DONECMDS  close them                               XCSMDIUS  direct install to flash sectors 6-11
//   DONE0000  end of data                              ZCSMCCOZ  finish and start the firmware
// The loader erases and writes only 0x08040000-0x080FFFFF (sectors 6-11); its own
// sectors 0-5 and the external memory chip are not touched by these commands.

export const VENDOR_ID = 0x0333;
export const PRODUCT_ID = 0x0333;
export const SUFFIX = 'sSJFfddSAD$srAffy84JappEKL3$SALgK@3J4hwd5920xg1!8hs7J6Kg';
export const COMMANDS = Object.freeze(['XCFILNFS', 'DUMPMETA', 'DONECMDS', 'XCSMDIUS', 'DONE0000', 'ZCSMCCOZ']);
export const HARDWARE_VERSION = 4;
export const APP_BASE = 0x08040000;
export const MAX_IMAGE = 0x080e0000 - APP_BASE; // a TBOSS image ends below sector 11 (settings)
const TEXT = new TextEncoder();

export function command(name) {
  if (!COMMANDS.includes(name)) throw new Error(`refusing to send unknown command ${name}`);
  const report = new Uint8Array(64);
  report.set(TEXT.encode(name + SUFFIX));
  return report;
}

export function number(value) {
  const report = new Uint8Array(64);
  new DataView(report.buffer).setUint32(0, value >>> 0, true);
  return report;
}

const TABLE = (() => {
  const t = new Uint32Array(256);
  for (let i = 0; i < 256; i++) {
    let c = i << 24;
    for (let k = 0; k < 8; k++) c = c & 0x80000000 ? (c << 1) ^ 0x04c11db7 : c << 1;
    t[i] = c >>> 0;
  }
  return t;
})();

// CRC-32 as the loader computes it: polynomial 0x04C11DB7, MSB first, start 0, no final XOR.
export function crc32(bytes) {
  let crc = 0;
  for (const b of bytes) crc = (TABLE[(b ^ (crc >>> 24)) & 255] ^ (crc << 8)) >>> 0;
  return crc >>> 0;
}

// The loader writes whole 64-byte blocks; the last one is padded with erased-flash bytes.
export function padded(image) {
  const out = new Uint8Array(Math.ceil(image.length / 64) * 64).fill(0xff);
  out.set(image);
  return out;
}

// The version tag TBOSS 0.3.0 and later carry ("TBOSS 0.3.0"); null for older images.
export function versionOf(image) {
  const text = new TextDecoder('latin1').decode(image.subarray(0, MAX_IMAGE));
  const found = /TBOSS (\d{1,3}\.\d{1,3}\.\d{1,3})\0/.exec(text);
  return found ? found[1] : null;
}

// What the image must look like to run from 0x08040000. Returns { ok, problems, tboss, version }.
export function checkImage(image) {
  const problems = [];
  if (!(image instanceof Uint8Array) || image.length < 8) return { ok: false, problems: ['not a firmware image'], tboss: false, version: null };
  if (image.length > MAX_IMAGE) problems.push(`too large (${image.length} bytes; at most ${MAX_IMAGE})`);
  const view = new DataView(image.buffer, image.byteOffset, image.byteLength);
  const sp = view.getUint32(0, true), reset = view.getUint32(4, true);
  if (sp <= 0x20000000 || sp > 0x20020000 || sp % 8) problems.push('no valid stack pointer: not built for this board');
  if (!(reset & 1) || reset <= APP_BASE || reset >= APP_BASE + image.length) problems.push('no valid start address: not built for 0x08040000');
  const tboss = indexOf(image, TEXT.encode('TheoryBoard MIDI')) >= 0;
  return { ok: problems.length === 0, problems, tboss, version: tboss ? versionOf(image) : null };
}

function indexOf(haystack, needle) {
  outer: for (let i = 0; i + needle.length <= haystack.length; i++) {
    for (let j = 0; j < needle.length; j++) if (haystack[i + j] !== needle[j]) continue outer;
    return i;
  }
  return -1;
}

function nameOf(report) { return String.fromCharCode(...report.slice(0, 8)); }
function isMessage(report, name) {
  return nameOf(report) === name && String.fromCharCode(...report.slice(8, 64)) === SUFFIX;
}
const sleep = (ms) => new Promise((r) => setTimeout(r, ms));

// link: { send(report: Uint8Array(64)) -> Promise, receive(timeoutMs) -> Promise<Uint8Array(64)> }
// progress(event): { step, text, done?, total? }. Throws Error with a user-facing message.
export async function install(link, image, progress = () => {}, options = {}) {
  const timing = { reply: 5000, erase: 60000, ...options.timing };
  const check = checkImage(image);
  if (!check.ok) throw new Error('This file cannot be installed: ' + check.problems.join('; '));
  const expect = async (name, timeout) => {
    const deadline = Date.now() + timeout;
    for (;;) {
      const left = deadline - Date.now();
      if (left <= 0) throw new Error(`the board did not answer (${name}); unplug it and start again`);
      const report = await link.receive(left);
      if (isMessage(report, name)) return;
      if (nameOf(report) === 'SERERRNF') throw new Error('the board did not understand a command');
    }
  };
  const readNumber = async (timeout) => {
    const report = await link.receive(timeout);
    return new DataView(report.buffer, report.byteOffset, 4).getUint32(0, true);
  };

  progress({ step: 'identify', text: 'Reading the board\'s identity...' });
  await link.send(command('XCFILNFS'));
  await sleep(timing.gap ?? 500);
  await link.send(command('DUMPMETA'));
  await expect('FWMETABG', timing.reply);
  const metadata = [];
  for (let i = 0; i < 15; i++) metadata.push(await readNumber(timing.reply));
  await expect('FWMETADN', timing.reply);
  await link.send(command('DONECMDS'));
  if (metadata[0] !== HARDWARE_VERSION && !options.anyHardware) {
    throw new Error(`this board reports hardware version ${metadata[0]}; TBOSS is only known to work on version ${HARDWARE_VERSION}`);
  }

  const data = padded(image);
  const crc = crc32(data);
  progress({ step: 'erase', text: 'Erasing the firmware area (about 10 seconds)...' });
  await link.send(command('XCSMDIUS'));
  await expect('FWSETRD1', timing.reply);
  for (const value of [crc, crc, image.length, crc, 1, 0, 1]) await link.send(number(value));
  await expect('FWSETRD2', timing.erase);

  const total = data.length / 64;
  for (let i = 0; i < total; i++) {
    await link.send(data.slice(i * 64, i * 64 + 64));
    if (i % 16 === 0 || i === total - 1) progress({ step: 'write', text: 'Writing...', done: i + 1, total });
  }

  progress({ step: 'verify', text: 'Checking what the board received and wrote...' });
  await link.send(command('DONE0000'));
  await expect('TRACRCVL', timing.reply);
  const received = await readNumber(timing.reply);
  await expect('EXTCRCVL', timing.reply);
  const written = await readNumber(timing.reply);
  await expect('FWDLYRDY', timing.reply);
  if (received !== crc || written !== crc) {
    throw new Error(`verification failed (expected ${hex(crc)}, received ${hex(received)}, flash ${hex(written)}). ` +
      'Start again from update mode; the loader itself is not affected.');
  }

  progress({ step: 'restart', text: 'Starting the new firmware...' });
  for (let i = 0; i < 5; i++) {
    try { await link.send(command('ZCSMCCOZ')); } catch { break; } // the board restarts and disconnects
    await sleep(timing.gap ?? 500);
  }
  // metadata[12..14] hold the chip's unique ID: kept out of results people paste into reports.
  return { hardware: metadata[0], metadata: metadata.slice(0, 12), crc, bytes: image.length, tboss: check.tboss, version: check.version };
}

export function hex(n) { return '0x' + (n >>> 0).toString(16).toUpperCase().padStart(8, '0'); }

// WebHID transport for a device opened from navigator.hid.
export function webhidLink(device) {
  const queue = [], waiters = [];
  device.addEventListener('inputreport', (event) => {
    const report = new Uint8Array(64);
    report.set(new Uint8Array(event.data.buffer, event.data.byteOffset, Math.min(64, event.data.byteLength)));
    const waiter = waiters.shift();
    if (waiter) { clearTimeout(waiter.timer); waiter.resolve(report); } else queue.push(report);
  });
  return {
    send: (report) => device.sendReport(0, report),
    receive: (timeout) => queue.length ? Promise.resolve(queue.shift()) : new Promise((resolve, reject) => {
      const waiter = { resolve, timer: setTimeout(() => { waiters.splice(waiters.indexOf(waiter), 1); reject(new Error('the board stopped answering; unplug it and start again')); }, timeout) };
      waiters.push(waiter);
    }),
  };
}
