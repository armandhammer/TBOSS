// SPDX-License-Identifier: GPL-3.0-or-later - a model of the TheoryBoard loader's USB update
// mode, built from the protocol description in docs/installer-protocol.md, for testing
// webapp/tb-installer.js without a board. Internal flash is 1 MiB; anything the
// installer is not supposed to send is recorded in `violations`.
import { SUFFIX, crc32 } from '../../webapp/tb-installer.js';

const enc = new TextEncoder();
const SECTORS = [...[0, 1, 2, 3].map((i) => [i * 0x4000, 0x4000]), [0x10000, 0x10000],
  ...[5, 6, 7, 8, 9, 10, 11].map((i) => [0x20000 * (i - 4), 0x20000])];

export class LoaderSim {
  constructor({ hardware = 4, flash, dropAfterBlocks = -1, corruptBlock = -1, eraseDelay = 0 } = {}) {
    this.flash = flash ? Uint8Array.from(flash) : new Uint8Array(0x100000).map((_, i) => (i * 31) & 255);
    this.loader = this.flash.slice(0, 0x40000);
    this.hardware = hardware;
    this.dropAfterBlocks = dropAfterBlocks;
    this.corruptBlock = corruptBlock;
    this.eraseDelay = eraseDelay;
    this.inbox = [];
    this.waiters = [];
    this.violations = [];
    this.received = [];       // command names in order
    this.state = 'main';
    this.started = false;     // ZCSMCCOZ seen: the board restarts
    this.connected = true;
  }

  // --- the host's side ---
  link() {
    return {
      send: async (report) => this.write(Uint8Array.from(report)),
      receive: (timeout) => this.inbox.length ? Promise.resolve(this.inbox.shift()) : new Promise((resolve, reject) => {
        const w = { resolve, timer: setTimeout(() => { this.waiters.splice(this.waiters.indexOf(w), 1); reject(new Error('timeout')); }, timeout) };
        this.waiters.push(w);
      }),
    };
  }

  reply(report) {
    const w = this.waiters.shift();
    if (w) { clearTimeout(w.timer); w.resolve(report); } else this.inbox.push(report);
  }
  status(name) { const r = new Uint8Array(64); r.set(enc.encode(name + SUFFIX)); this.reply(r); }
  int(value) { const r = new Uint8Array(64); new DataView(r.buffer).setUint32(0, value >>> 0, true); this.reply(r); }

  // --- the board's side ---
  async write(report) {
    if (!this.connected) throw new Error('device disconnected');
    if (report.length !== 64) this.violations.push(`report of ${report.length} bytes`);
    const name = String.fromCharCode(...report.slice(0, 8));
    const isCommand = String.fromCharCode(...report.slice(8, 64)) === SUFFIX;
    if (this.state === 'params') {
      this.params.push(new DataView(report.buffer).getUint32(0, true));
      if (this.params.length === 7) {
        this.state = 'erasing';
        await new Promise((r) => setTimeout(r, this.eraseDelay));
        for (const [start, size] of SECTORS) if (start >= 0x40000) this.flash.fill(0xff, start, start + size);
        this.address = 0x40000; this.crc = []; this.blocks = 0; this.state = 'data';
        this.status('FWSETRD2');
      }
      return;
    }
    if (this.state === 'data') {
      if (isCommand && name === 'DONE0000') {
        this.received.push(name);
        const data = Uint8Array.from(this.crc);
        this.status('TRACRCVL'); this.int(crc32(data));
        this.status('EXTCRCVL'); this.int(crc32(this.flash.slice(0x40000, 0x40000 + data.length)));
        this.status('FWDLYRDY');
        this.state = 'main';
        return;
      }
      if (this.blocks === this.dropAfterBlocks) { this.connected = false; throw new Error('device disconnected'); }
      if (this.address + 64 > 0x100000) { this.violations.push('write past the end of flash'); return; }
      const block = Uint8Array.from(report);
      this.crc.push(...block);
      if (this.blocks === this.corruptBlock) block[5] ^= 0x40; // a flash write that went wrong
      for (let i = 0; i < 64; i++) this.flash[this.address + i] &= block[i]; // programming only clears bits
      this.address += 64; this.blocks++;
      return;
    }
    if (!isCommand) { this.violations.push(`unexpected data in state ${this.state}`); return; }
    this.received.push(name);
    if (this.state === 'metadata') {
      if (name === 'DUMPMETA') {
        this.status('FWMETABG');
        for (let i = 0; i < 15; i++) this.int(i === 0 ? this.hardware : 0x1000 + i);
        this.status('FWMETADN'); this.status('FCMDDONE');
      } else if (name === 'DONECMDS') { this.status('FCMDDONE'); this.state = 'main'; }
      else { this.violations.push(`metadata command ${name} (writes or erases the memory chip)`); this.status('SERERRNF'); }
      return;
    }
    switch (name) {
      case 'XCFILNFS': this.state = 'metadata'; break;
      case 'XCSMDIUS': this.status('FWSETRD1'); this.params = []; this.state = 'params'; break;
      case 'ZCSMCCOZ': this.started = true; this.connected = false; break; // restarts after 1 s
      default: this.violations.push(`command ${name}`);
    }
  }

  loaderIntact() { return this.flash.slice(0, 0x40000).every((b, i) => b === this.loader[i]); }
}
