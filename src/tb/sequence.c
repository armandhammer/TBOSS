/* SPDX-License-Identifier: GPL-3.0-or-later - chord progressions (see sequence.h).
 * Record: word 0 = 1 | unit << 8 | scale << 16 | count << 24; word 1 + i = step i:
 * pad (6 bits) | units - 1 (5) << 6 | velocity / 2 (6) << 11 | edit (8) << 17 | scale (7) << 25
 * (edit = lowered members | sus / 2 << 3 | added notes << 5); word 39 = the chord set (1.0.0,
 * 0 = the scale's chords); every other word 0. */
#include "sequence.h"
#include <string.h>
sq_prog sq = {.unit = SQ_UNIT_BAR};
uint8_t sq_present;
uint32_t sq_changes;
sq_prog sq_slots[SQ_SLOTS];
sq_chain_list sq_chain;
uint32_t sq_chain_changes;
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
        p->entries[p->count++] = (sq_entry){pad, 1u, velocity ? velocity : 1u, own, edit, 0u}; /* every chord plays (2.0.0) */
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
    w[SQ_WORDS - 1u] = p->chords;
}
bool sq_decode(const uint32_t w[SQ_WORDS], sq_prog *out) {
    sq_prog p;
    uint32_t count = w[0] >> 24, unit = (w[0] >> 8) & 0xFFu, scale = (w[0] >> 16) & 0xFFu;
    if ((w[0] & 0xFFu) != 1u || unit >= SQ_UNIT_KINDS || scale >= TC_RETIRED_SCALES_END || count > SQ_ENTRIES ||
        w[SQ_WORDS - 1u] > TC_CHORDS_MODES) return false;
    if (scale >= TC_ALL_SCALES) scale = 0u; /* 2.3.3: recorded in a retired tuned scale: Major */
    sq_clear(&p, (uint8_t)unit, (uint8_t)scale);
    for (uint32_t i = 0; i < count; ++i) {
        uint32_t v = w[1u + i], pad = v & 63u, units = ((v >> 6) & 31u) + 1u, edit = (v >> 17) & 0xFFu, own = v >> 25;
        if (pad >= 48u || ((edit >> 3) & 3u) == 3u || (own >= TC_ALL_SCALES && own != SQ_NO_SCALE)) return false;
        if (p.total + units > SQ_UNITS_MAX) return false;
        p.entries[i] = (sq_entry){(uint8_t)pad, (uint8_t)units, half_velocity((v >> 11) & 63u), (uint8_t)own,
                                  (tc_chord){(uint8_t)(edit & 7u), (uint8_t)(2u * ((edit >> 3) & 3u)), (uint8_t)(edit >> 5)}, 0u};
        p.total = (uint8_t)(p.total + units);
    }
    p.count = (uint8_t)count; p.chords = (uint8_t)w[SQ_WORDS - 1u];
    for (uint32_t i = 1u + count; i < SQ_WORDS - 1u; ++i) if (w[i]) return false;
    *out = p;
    return true;
}
void sq_chances_encode(const sq_prog *p, uint32_t w[SQ_WORDS]) {
    memset(w, 0, SQ_WORDS * 4u);
    bool any = false;
    for (uint32_t i = 0; i < p->count && i < SQ_ENTRIES; ++i) {
        w[1u + i / 4u] |= (uint32_t)p->entries[i].chance << (8u * (i % 4u));
        any = any || p->entries[i].chance;
    }
    if (any) w[0] = 1u;
}
bool sq_chances_decode(const uint32_t w[SQ_WORDS], sq_prog *p) {
    uint8_t chances[SQ_ENTRIES];
    if (w[0] > 1u) return false;
    for (uint32_t i = 1u + (SQ_ENTRIES + 3u) / 4u; i < SQ_WORDS; ++i) if (w[i]) return false;
    for (uint32_t i = SQ_ENTRIES; i < 4u * ((SQ_ENTRIES + 3u) / 4u); ++i) if ((w[1u + i / 4u] >> (8u * (i % 4u))) & 0xFFu) return false;
    for (uint32_t i = 0; i < SQ_ENTRIES; ++i) {
        uint8_t b = (uint8_t)(w[1u + i / 4u] >> (8u * (i % 4u)));
        if ((b & 15u) >= SQ_CHANCES || (b & ~(15u | SQ_BORROW)) || (b && !w[0])) return false;
        chances[i] = b;
    }
    for (uint32_t i = 0; i < SQ_ENTRIES; ++i) p->entries[i].chance = chances[i];
    return true;
}
void sq_chain_encode(const sq_chain_list *chain, uint32_t w[SQ_WORDS]) {
    memset(w, 0, SQ_WORDS * 4u);
    if (!chain->count) return;
    w[0] = 1u | (uint32_t)chain->count << 8;
    for (uint32_t i = 0; i < chain->count && i < SQ_CHAIN; ++i) w[1] |= (uint32_t)chain->links[i] << (4u * i);
}
bool sq_chain_decode(const uint32_t w[SQ_WORDS], sq_chain_list *out) {
    sq_chain_list t; memset(&t, 0, sizeof t);
    for (uint32_t i = 2u; i < SQ_WORDS; ++i) if (w[i]) return false;
    if (!w[0]) { if (w[1]) return false; *out = t; return true; }
    uint32_t count = (w[0] >> 8) & 0xFFu;
    if ((w[0] & 0xFFu) != 1u || w[0] >> 16 || !count || count > SQ_CHAIN) return false;
    for (uint32_t i = 0; i < SQ_CHAIN; ++i) {
        uint32_t slot = (w[1] >> (4u * i)) & 15u;
        if (i < count ? slot >= SQ_SLOTS : slot != 0u) return false;
        if (i < count) t.links[i] = (uint8_t)slot;
    }
    t.count = (uint8_t)count;
    *out = t;
    return true;
}
/* 2.6.6: the progression library (sequence.h). Lower-case numerals are minor chords, as the scale makes them. */
const sq_library_entry sq_library[SQ_LIBRARY] = {
    {"I-V-vi-IV", 0u, SQ_UNIT_BAR, "1 5 6 4"},            /* the pop progression */
    {"I-vi-IV-V", 0u, SQ_UNIT_BAR, "1 6 4 5"},            /* the 1950s progression */
    {"vi-IV-I-V", 0u, SQ_UNIT_BAR, "6 4 1 5"},
    {"I-IV-vi-V", 0u, SQ_UNIT_BAR, "1 4 6 5"},
    {"I-IV-V-IV", 0u, SQ_UNIT_BAR, "1 4 5 4"},
    {"I-IV-I-V", 0u, SQ_UNIT_BAR, "1 4 1 5"},
    {"IV-I-V-vi", 0u, SQ_UNIT_BAR, "4 1 5 6"},
    {"ii-V-I", 0u, SQ_UNIT_BAR, "2+ 5+ 1+*2"},            /* jazz: ii7 V7 Imaj7 */
    {"I-vi-ii-V", 0u, SQ_UNIT_BAR, "1+ 6+ 2+ 5+"},        /* the turnaround, with 7ths */
    {"BLUES 12 BAR", 0u, SQ_UNIT_BAR, "1x+*4 4d+*2 1x+*2 5+ 4d+ 1x+ 5+"}, /* dominant 7ths: I7 from Mixolydian, IV7 Dorian */
    {"CANON", 0u, SQ_UNIT_HALF, "1 5 6 3 4 1 4 5"},       /* Pachelbel's */
    {"CIRCLE", 0u, SQ_UNIT_BAR, "1 4 7 3 6 2 5 1"},       /* round the circle of fifths in the key */
    {"IV-V-iii-vi", 0u, SQ_UNIT_BAR, "4+ 5+ 3+ 6"},       /* the royal road (J-pop), with 7ths */
    {"I-bVI-bVII-I", 0u, SQ_UNIT_BAR, "1 6m 7m 1"},       /* bVI and bVII borrowed from the minor key */
    {"I-IV-iv-I", 0u, SQ_UNIT_BAR, "1 4 4m 1"},           /* iv borrowed from the minor key */
    {"I-bVII-IV-I", 5u, SQ_UNIT_BAR, "1 7 4 1"},          /* Mixolydian rock */
    {"i-IV DORIAN", 2u, SQ_UNIT_BAR, "1+*2 4+*2"},        /* i7 IV7 */
    {"i-VI-III-VII", 1u, SQ_UNIT_BAR, "1 6 3 7"},
    {"i-VII-VI-VII", 1u, SQ_UNIT_BAR, "1 7 6 7"},
    {"i-VII-VI-V", 1u, SQ_UNIT_BAR, "1 7 6 5h"},          /* the Andalusian cadence (a major V) */
    {"i-iv-V-i", 1u, SQ_UNIT_BAR, "1 4 5h 1"},
    {"i-iv-VII-III", 1u, SQ_UNIT_BAR, "1 4 7 3"},
    {"ii-V-i MINOR", 1u, SQ_UNIT_BAR, "2+ 5h+ 1+*2"},     /* iim7b5 V7 im7 */
    {"i-VI-VII-i", 1u, SQ_UNIT_BAR, "1 6 7 1"},
};
static uint8_t library_scale(char c) {
    return c == 'M' ? 0u : c == 'm' ? 1u : c == 'h' ? 7u : c == 'x' ? 5u : c == 'd' ? 2u : 255u;
}
bool sq_library_load(uint32_t i, sq_prog *out, uint8_t *mode) {
    if (i >= SQ_LIBRARY) return false;
    const sq_library_entry *l = &sq_library[i];
    sq_prog p;
    sq_clear(&p, l->unit, l->mode);
    for (const char *s = l->steps; *s;) {
        if (*s == ' ') { s++; continue; }
        if (*s < '1' || *s > '7' || p.count >= SQ_ENTRIES) return false;
        uint8_t column = (uint8_t)(*s++ - '1'), scale = l->mode, add = 0u;
        uint32_t units = 1u;
        if (library_scale(*s) != 255u) scale = library_scale(*s++);
        if (*s == '+') { add = TC_ADD7; s++; }
        if (*s == '*') { s++; units = 0u; while (*s >= '0' && *s <= '9') units = units * 10u + (uint32_t)(*s++ - '0'); }
        if (!units || units > SQ_RUN_MAX || p.total + units > SQ_UNITS_MAX || (*s && *s != ' ')) return false;
        p.entries[p.count++] = (sq_entry){(uint8_t)(4u * column + 3u), (uint8_t)units, 100u,
                                          scale == l->mode ? (uint8_t)SQ_NO_SCALE : scale, (tc_chord){0u, 0u, add}, 0u};
        p.total = (uint8_t)(p.total + units);
    }
    *out = p; *mode = l->mode;
    return true;
}
tp_layout sq_layout(const sq_prog *p, tp_layout current, const sq_entry *en) {
    if (en->scale != SQ_NO_SCALE && tc_scale_defined(en->scale)) current.mode = en->scale;
    current.chords = tc_chords_ok(p->chords) ? p->chords : 0u;
    return current;
}
