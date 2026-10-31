// SPDX-License-Identifier: GPL-3.0-or-later - presets for the editor's Presets tab (TBOSS 1.9.3): the names record
// (kind 10: 8 names of 12 characters), a preset's own look and curves made from the ones showing now, and a
// setting-by-setting comparison of two presets from VALUES and the board's menu (Stage 1's names); 2.0: a preset's own
// chord set (Keep chords). Pure functions.
import { cleanName, NAME_LENGTH, PAYLOAD_WORDS, ITEM_KIND, ownSetFromWords } from './tb-protocol.js';

export const PRESETS = 8;
// Names → the record: word 0 = 1, preset n's characters from byte 12n of word 1 on, 0 after the last.
export function namesToWords(names) {
  const w = new Array(PAYLOAD_WORDS).fill(0); w[0] = 1;
  for (let n = 0; n < PRESETS; n++) {
    const name = cleanName(names[n] || '');
    for (let i = 0; i < name.length; i++) {
      const at = NAME_LENGTH * n + i;
      w[1 + (at >> 2)] = (w[1 + (at >> 2)] | name.charCodeAt(i) << (8 * (at & 3))) >>> 0;
    }
  }
  return w;
}
export function namesFromWords(words) {
  const names = [];
  for (let n = 0; n < PRESETS; n++) {
    let name = '';
    for (let i = 0; i < NAME_LENGTH; i++) {
      const at = NAME_LENGTH * n + i, c = (words[1 + (at >> 2)] >>> (8 * (at & 3))) & 255;
      if (!c) break;
      name += String.fromCharCode(c);
    }
    names.push(name);
  }
  return names;
}
// The look or curves on show (slot 9) as a preset's own: no Keep look / Keep feel, no "whose".
export function asPresetLook(words) { const w = [...words]; w[0] = (w[0] & 0x00ffffff) >>> 0; w[PAYLOAD_WORDS - 1] = 0; return w; }
export function asPresetFeel(words) { const w = [...words]; w[0] = (w[0] & 0xff) >>> 0; w[PAYLOAD_WORDS - 1] = 0; return w; }
export const cleared = () => new Array(PAYLOAD_WORDS).fill(0);
// 2.0.8 (board test C1): a preset's own Chord vel and Melody vel (its record: word 1 byte 1; word 31 byte 3 0x80 |
// Melody vel where they differ, else 0), and the record with one half's set to mode, the other's kept.
export function presetVel(words) {
  const chord = (words[1] >>> 8) & 0xff, m = words[31] >>> 24;
  return [chord, m ? m & 0x7f : chord];
}
export function withPresetVel(words, half, mode) {
  const w = [...words], v = presetVel(words); v[half] = mode;
  w[1] = ((w[1] & ~0xff00) | v[0] << 8) >>> 0;
  w[31] = ((w[31] & 0x00ffffff) | (v[1] === v[0] ? 0 : (0x80 | v[1]) << 24)) >>> 0;
  return w;
}
// TBOSS 2.0, a preset's own chord set (Keep chords, kind 11): what it is, for the list and the compare. words: the
// preset's own record (null: none), or slot 9 for the TB now (its "whose" says whether a preset's own is in use).
export function ownChordsText(words, now = false) {
  const own = words && ownSetFromWords(words);
  if (!own || (now && !own.whose)) return "the board's";
  const name = own.chords.name ? ` ${own.chords.name}` : '';
  return now ? `preset ${own.whose}'s set ${own.set + 1}${name}` : `own set ${own.set + 1}${name}`;
}
// The chord set a preset's record plays (word 34 byte 0): 0..7 for chord sets 1..8, null for the scale's chords, Full
// and the grid, which have no set to keep.
export function presetChordSet(words) {
  const n = words ? words[34] & 0xff : 0;
  return n >= 1 && n <= 8 ? n - 1 : null;
}

// Two presets (or the TB now) side by side: for each page and item of the board's menu (tb-protocol readMenu)
// that a preset can change, its value in a and in b (VALUES: by item id) by the menu's own names.
export function compare(pages, a, b) {
  const rows = [];
  for (const page of pages || []) {
    for (const it of page.items) {
      if (it.kind === ITEM_KIND.BOARD || it.kind === ITEM_KIND.CHOOSE || it.kind === ITEM_KIND.TOGGLE) continue;
      const va = a[it.id], vb = b[it.id];
      if (va === undefined || vb === undefined) continue;
      const label = (v) => (it.labels[v] !== undefined ? it.labels[v] : String(v));
      rows.push({ page: page.name, item: it.name, a: label(va), b: label(vb), same: va === vb });
    }
  }
  return rows;
}
