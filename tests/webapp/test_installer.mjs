// SPDX-License-Identifier: GPL-3.0-or-later - the browser installer against the loader model.
//   node tests/webapp/test_installer.mjs
import assert from 'node:assert/strict';
import { readFileSync, existsSync } from 'node:fs';
import { fileURLToPath } from 'node:url';
import path from 'node:path';
import * as inst from '../../webapp/tb-installer.js';
import { LoaderSim } from './loader_sim.mjs';

const root = path.resolve(path.dirname(fileURLToPath(import.meta.url)), '../..');
const fast = { timing: { reply: 300, erase: 1000, gap: 1 } };

function image(n = 40224) {
  const img = new Uint8Array(n).map((_, i) => (i * 7 + 3) & 255);
  const v = new DataView(img.buffer);
  v.setUint32(0, 0x20020000, true); v.setUint32(4, 0x08040189, true);
  img.set(new TextEncoder().encode('TheoryBoard MIDI'), 1000);
  return img;
}

// Known values: the loader's CRC (MSB first, start 0, no final XOR).
assert.equal(inst.crc32(new TextEncoder().encode('123456789')), 0x89a1897f);
assert.equal(inst.padded(new Uint8Array(65)).length, 128);
assert.equal(inst.padded(new Uint8Array(65))[127], 0xff);
assert.deepEqual(Array.from(inst.number(0x12345678).slice(0, 5)), [0x78, 0x56, 0x34, 0x12, 0]);
assert.equal(String.fromCharCode(...inst.command('DONE0000')), 'DONE0000' + inst.SUFFIX);
assert.throws(() => inst.command('XCFLERAS'), /unknown command/); // whole-chip erase can't even be built

// 1. A normal install: only the six commands, sectors 6-11 written, loader untouched, verified.
{
  const sim = new LoaderSim(), img = image(), steps = [];
  const result = await inst.install(sim.link(), img, (p) => steps.push(p.step), fast);
  assert.deepEqual(sim.violations, []);
  assert.deepEqual(sim.received, ['XCFILNFS', 'DUMPMETA', 'DONECMDS', 'XCSMDIUS', 'DONE0000', 'ZCSMCCOZ']);
  assert.ok(sim.loaderIntact(), 'loader sectors unchanged');
  assert.deepEqual(sim.flash.slice(0x40000, 0x40000 + img.length), img);
  assert.ok(sim.flash.slice(0x40000 + img.length).every((b) => b === 0xff), 'rest of sectors 6-11 erased');
  assert.ok(sim.started && result.hardware === 4 && result.tboss);
  assert.equal(result.metadata.length, 12, 'the chip unique ID (metadata 12..14) stays out of the results');
  assert.deepEqual([...new Set(steps)], ['identify', 'erase', 'write', 'verify', 'restart']);
}
// 2. A bad flash write is caught by the loader's own CRC report.
{
  const sim = new LoaderSim({ corruptBlock: 100 });
  await assert.rejects(inst.install(sim.link(), image(), () => {}, fast), /verification failed/);
  assert.ok(!sim.started, 'the board is not told to start a corrupt image');
  assert.ok(sim.loaderIntact());
}
// 3. Interrupted mid-transfer: an error, loader intact, and a fresh attempt succeeds.
{
  const sim = new LoaderSim({ dropAfterBlocks: 200 });
  await assert.rejects(inst.install(sim.link(), image(), () => {}, fast), /disconnected/);
  assert.ok(sim.loaderIntact());
  const retry = new LoaderSim({ flash: sim.flash });
  await inst.install(retry.link(), image(), () => {}, fast);
  assert.ok(retry.started && retry.loaderIntact());
}
// 4. Another hardware version: stop before anything is erased.
{
  const sim = new LoaderSim({ hardware: 5 });
  const before = Uint8Array.from(sim.flash);
  await assert.rejects(inst.install(sim.link(), image(), () => {}, fast), /hardware version 5/);
  assert.deepEqual(sim.flash, before);
  assert.ok(!sim.received.includes('XCSMDIUS'));
}
// 5. A board that never answers (not in update mode): a clear timeout, nothing written.
{
  const silent = { send: async () => {}, receive: (t) => new Promise((_, reject) => setTimeout(() => reject(new Error('the board stopped answering')), t)) };
  await assert.rejects(inst.install(silent, image(), () => {}, fast), /stopped answering|did not answer/);
}
// 6. Images that would not run are refused before connecting to anything.
for (const [bad, why] of [[new Uint8Array(100), /stack pointer/], [image(inst.MAX_IMAGE + 64), /too large/]]) {
  const sim = new LoaderSim();
  await assert.rejects(inst.install(sim.link(), bad, () => {}, fast), why);
  assert.deepEqual(sim.received, []);
}
// 6b. Version tag: "TBOSS x.y.z" ending in a NUL, anywhere in the image.
{
  const img = image();
  const tag = new TextEncoder().encode('TBOSS 12.3.45');
  img.set(tag, 300); img[300 + tag.length] = 0;
  assert.equal(inst.versionOf(img), '12.3.45');
  img[300 + tag.length] = 0x20;
  assert.equal(inst.versionOf(img), null, 'only a complete C string counts');
  assert.equal(inst.checkImage(image()).version, null);
}
// 7. The real firmware image, if built: checkImage accepts it as TBOSS (with the version
// from src/tb/version.h when the image has one).
const built = path.join(root, 'build/tboss/theoryboard-usb.bin');
const tb = path.join(root, 'build/tb/keyboard-midi-stage2/theoryboard-usb.bin');
const real = existsSync(built) ? built : existsSync(tb) ? tb : null;
if (real) {
  const img = new Uint8Array(readFileSync(real));
  const check = inst.checkImage(img);
  assert.ok(check.ok && check.tboss, check.problems.join('; '));
  if (check.version) {
    const header = readFileSync(path.join(root, 'src/tb/version.h'), 'utf8');
    const part = (name) => header.match(new RegExp(`#define TB_VERSION_${name} ([0-9]+)`))[1];
    assert.equal(check.version, `${part('MAJOR')}.${part('MINOR')}.${part('PATCH')}`);
  }
  const sim = new LoaderSim();
  await inst.install(sim.link(), img, () => {}, fast);
  assert.deepEqual(sim.flash.slice(0x40000, 0x40000 + img.length), img);
}
console.log('PASS: browser installer: CRC/report formats, only the six read-only/direct-install commands, sectors 6-11 only, ' +
  'loader untouched, bad write detected by the loader CRCs, interruption then retry, hardware-version stop, silent board, bad images, version tag, unique ID withheld' +
  (real ? ', real TBOSS image' : ''));
