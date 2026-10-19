/* SPDX-License-Identifier: GPL-3.0-or-later - chord progressions (see sequence.h).
 * Record: word 0 = 1 | unit << 8 | scale << 16 | count << 24; word 1 + i = step i:
 * pad (6 bits) | units - 1 (5) << 6 | velocity / 2 (6) << 11 | edit (8) << 17 | scale (7) << 25
 * (edit = lowered members | sus / 2 << 3 | added notes << 5); every other word 0. */
#include "sequence.h"
#include <string.h>
sq_prog sq = {.unit = SQ_UNIT_BAR};
uint8_t sq_present;
uint32_t sq_changes;
void sq_clear(sq_prog *p, uint8_t unit, uint8_t scale) {
    if (p == &sq) sq_changes++;
    memset(p, 0, sizeof *p);
    p->unit = unit < SQ_UNIT_KINDS ? unit : (uint8_t)SQ_UNIT_BAR;
    p->scale = scale;
}
static bool same(const sq_entry *a, uint8_t pad, uint8_t scale, tc_chord edit) {
    return a->pad == pad && a->scale == scale && a->edit.lower == edit.lower && a->edit.sus == edit.sus &&
           a->edit.add == edit.add;
}
bool sq_tap(sq_prog *p, uint8_t pad, uint8_t velocity, uint8_t scale, tc_chord edit) {
    if (pad >= 48u || p->total >= SQ_UNITS_MAX) return false;
    if (p == &sq) sq_changes++;
    uint8_t own = scale == p->scale ? (uint8_t)SQ_NO_SCALE : scale; /* borrowed: keeps its scale */
    sq_entry *last = p->count ? &p->entries[p->count - 1u] : 0;
    if (last && same(last, pad, own, edit) && last->units < SQ_RUN_MAX) last->units++;
    else {
        if (p->count >= SQ_ENTRIES) return false;
        p->entries[p->count++] = (sq_entry){pad, 1u, velocity ? velocity : 1u, own, edit};
    }
    p->total++;
    return true;
}
void sq_undo(sq_prog *p) {
    if (!p->count) return;
    if (p == &sq) sq_changes++;
    sq_entry *last = &p->entries[p->count - 1u];
    if (!--last->units) p->count--;
    p->total--;
}
uint32_t sq_unit_ticks(uint8_t unit) {
    static const uint8_t ticks[SQ_UNIT_KINDS] = {24u, 48u, 96u, 192u};
    return ticks[unit < SQ_UNIT_KINDS ? unit : SQ_UNIT_BAR];
}
uint32_t sq_start(const sq_prog *p, uint32_t i) {
    uint32_t n = 0;
    for (uint32_t j = 0; j < i && j < p->count; ++j) n += p->entries[j].units;
    return n;
}
/* Velocity is stored at half resolution: even values come back exact (a typed or Fixed 100
 * plays 100), odd ones 1 less; the ends stay 1 and 127. */
static uint8_t half_velocity(uint32_t v) { return (uint8_t)(!v ? 1u : v == 63u ? 127u : v * 2u); }
static uint32_t edit_byte(tc_chord e) {
    return (uint32_t)(e.lower & 7u) | (uint32_t)((e.sus / 2u) & 3u) << 3 | (uint32_t)(e.add & 7u) << 5;
}
void sq_encode(const sq_prog *p, uint32_t w[SQ_WORDS]) {
    memset(w, 0, SQ_WORDS * 4u);
    w[0] = 1u | (uint32_t)p->unit << 8 | (uint32_t)p->scale << 16 | (uint32_t)p->count << 24;
    for (uint32_t i = 0; i < p->count && i < SQ_ENTRIES; ++i) {
        const sq_entry *e = &p->entries[i];
        w[1u + i] = (uint32_t)e->pad | (uint32_t)(e->units - 1u) << 6 | (uint32_t)(e->velocity >> 1) << 11 |
                    edit_byte(e->edit) << 17 | (uint32_t)e->scale << 25;
    }
}
bool sq_decode(const uint32_t w[SQ_WORDS], sq_prog *out) {
    sq_prog p;
    uint32_t count = w[0] >> 24, unit = (w[0] >> 8) & 0xFFu, scale = (w[0] >> 16) & 0xFFu;
    if ((w[0] & 0xFFu) != 1u || unit >= SQ_UNIT_KINDS || scale >= TC_ALL_SCALES || count > SQ_ENTRIES) return false;
    sq_clear(&p, (uint8_t)unit, (uint8_t)scale);
    for (uint32_t i = 0; i < count; ++i) {
        uint32_t v = w[1u + i], pad = v & 63u, units = ((v >> 6) & 31u) + 1u, edit = (v >> 17) & 0xFFu, own = v >> 25;
        if (pad >= 48u || ((edit >> 3) & 3u) == 3u || (own >= TC_ALL_SCALES && own != SQ_NO_SCALE)) return false;
        if (p.total + units > SQ_UNITS_MAX) return false;
        p.entries[i] = (sq_entry){(uint8_t)pad, (uint8_t)units, half_velocity((v >> 11) & 63u), (uint8_t)own,
                                  (tc_chord){(uint8_t)(edit & 7u), (uint8_t)(2u * ((edit >> 3) & 3u)), (uint8_t)(edit >> 5)}};
        p.total = (uint8_t)(p.total + units);
    }
    p.count = (uint8_t)count;
    for (uint32_t i = 1u + count; i < SQ_WORDS; ++i) if (w[i]) return false;
    *out = p;
    return true;
}
tp_layout sq_layout(tp_layout current, const sq_entry *en) {
    if (en->scale != SQ_NO_SCALE && tc_scale_defined(en->scale)) current.mode = en->scale;
    return current;
}
