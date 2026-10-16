/* SPDX-License-Identifier: GPL-3.0-or-later - chord symbols from pitch-class sets. Original table
 * of common symbols (jazz/pop lead-sheet spelling, ASCII: b flat, # sharp, M major). */
#include "chord_name.h"
#include "controls.h"
#define S(a) (1u << (a))
typedef struct { uint16_t set; const char *suffix; } shape;
/* Exact symbols, root bit included. Order breaks ties in the fallback (earlier wins). */
static const shape shapes[] = {
    {S(0)|S(4)|S(7), ""}, {S(0)|S(3)|S(7), "m"}, {S(0)|S(3)|S(6), "dim"}, {S(0)|S(4)|S(8), "aug"},
    {S(0)|S(2)|S(7), "sus2"}, {S(0)|S(5)|S(7), "sus4"},
    {S(0)|S(4)|S(7)|S(11), "maj7"}, {S(0)|S(4)|S(7)|S(10), "7"}, {S(0)|S(3)|S(7)|S(10), "m7"},
    {S(0)|S(3)|S(7)|S(11), "mM7"}, {S(0)|S(3)|S(6)|S(10), "m7b5"}, {S(0)|S(3)|S(6)|S(9), "dim7"},
    {S(0)|S(4)|S(8)|S(10), "7#5"}, {S(0)|S(4)|S(8)|S(11), "maj7#5"}, {S(0)|S(4)|S(6)|S(10), "7b5"},
    {S(0)|S(4)|S(7)|S(9), "6"}, {S(0)|S(3)|S(7)|S(9), "m6"},
    {S(0)|S(5)|S(7)|S(10), "7sus4"}, {S(0)|S(2)|S(7)|S(10), "7sus2"},
    {S(0)|S(5)|S(7)|S(11), "maj7sus4"}, {S(0)|S(2)|S(7)|S(11), "maj7sus2"},
    {S(0)|S(5)|S(7)|S(9), "6sus4"}, {S(0)|S(2)|S(7)|S(9), "6sus2"},
    {S(0)|S(2)|S(4)|S(7), "add9"}, {S(0)|S(2)|S(3)|S(7), "madd9"},
    {S(0)|S(4)|S(5)|S(7), "add11"}, {S(0)|S(3)|S(5)|S(7), "madd11"},
    {S(0)|S(2)|S(4)|S(7)|S(10), "9"}, {S(0)|S(2)|S(4)|S(7)|S(11), "maj9"},
    {S(0)|S(2)|S(3)|S(7)|S(10), "m9"}, {S(0)|S(2)|S(3)|S(7)|S(11), "mM9"},
    {S(0)|S(2)|S(4)|S(7)|S(9), "6/9"}, {S(0)|S(2)|S(3)|S(7)|S(9), "m6/9"},
    {S(0)|S(1)|S(4)|S(7)|S(10), "7b9"}, {S(0)|S(1)|S(3)|S(7)|S(10), "m7b9"},
    {S(0)|S(3)|S(4)|S(7)|S(10), "7#9"}, {S(0)|S(2)|S(3)|S(6)|S(10), "m9b5"},
    {S(0)|S(1)|S(3)|S(6)|S(10), "m7b5b9"}, {S(0)|S(2)|S(5)|S(7)|S(10), "9sus4"},
    {S(0)|S(4)|S(6)|S(7)|S(11), "maj7#11"}, {S(0)|S(3)|S(5)|S(7)|S(10), "m11"},
    {S(0)|S(4)|S(7)|S(8)|S(10), "7b13"},
    {S(0)|S(2)|S(4)|S(7)|S(9)|S(10), "13"}, {S(0)|S(2)|S(4)|S(7)|S(9)|S(11), "maj13"},
    {S(0)|S(2)|S(3)|S(7)|S(9)|S(10), "m13"},
    /* Fallback bases only (exact use would hide the missing fifth). */
    {S(0)|S(4), ""}, {S(0)|S(3), "m"}, {S(0)|S(7), "5"},
};
#define SHAPES (sizeof shapes / sizeof shapes[0])
#define EXACT (SHAPES - 3u)
static const char *const pcs[12] = {"C","C#","D","D#","E","F","F#","G","G#","A","A#","B"};
static const char *const extra[12] = {"","b9","9","#9","3","11","#11","5","b13","13","7","M7"};
static uint32_t rotate(uint32_t set, uint32_t root) {
    return ((set >> root) | (set << (12u - root))) & 0xFFFu;
}
static uint32_t popcount(uint32_t x) { uint32_t n = 0; for (; x; x &= x - 1u) n++; return n; }
typedef struct { char *out; uint32_t n; } text;
static void put(text *t, const char *s) {
    while (*s && t->n < TN_MAX - 1u) t->out[t->n++] = *s++;
    t->out[t->n] = 0;
}
static uint32_t finish(text *t, uint32_t root, const char *suffix, uint32_t bass) {
    put(t, pcs[root]); put(t, suffix);
    if (bass != root) { put(t, "/"); put(t, pcs[bass]); }
    return t->n;
}
uint32_t tn_name(const tp_notes *notes, uint8_t root, char out[TN_MAX]) {
    text t = {out, 0}; out[0] = 0;
    if (!notes || !notes->count || root > 11u) { put(&t, "-"); return t.n; }
    uint32_t set = 0, low = 0;
    for (uint32_t i = 0; i < notes->count; ++i) {
        set |= 1u << (notes->notes[i] % 12u);
        if (notes->notes[i] < notes->notes[low]) low = i;
    }
    uint32_t bass = notes->notes[low] % 12u;
    /* Exact symbol on the pad's root, then on another tone with that tone as root. */
    uint32_t order[12], count = 0;
    if (set & (1u << root)) order[count++] = root;
    if (bass != root) order[count++] = bass;
    for (uint32_t pc = 0; pc < 12u; ++pc)
        if ((set & (1u << pc)) && pc != root && pc != bass) order[count++] = pc;
    for (uint32_t i = 0; i < count; ++i) {
        uint32_t relative = rotate(set, order[i]);
        for (uint32_t s = 0; s < EXACT; ++s)
            if (shapes[s].set == relative) return finish(&t, order[i], shapes[s].suffix, bass);
    }
    /* Largest known chord inside the notes on the pad's root, the rest in brackets. */
    uint32_t relative = rotate(set, root) | 1u, best = SHAPES, size = 0;
    for (uint32_t s = 0; s < SHAPES; ++s) {
        uint32_t n = popcount(shapes[s].set);
        if ((shapes[s].set & ~relative) == 0u && n > size) { best = s; size = n; }
    }
    put(&t, pcs[root]);
    uint32_t rest = relative & ~1u;
    if (best < SHAPES) { put(&t, shapes[best].suffix); rest &= ~(uint32_t)shapes[best].set; }
    if (rest) {
        /* Sevenths first, then the other tones low to high (lead-sheet order). */
        static const uint8_t listed[11] = {10, 11, 1, 2, 3, 4, 5, 6, 7, 8, 9};
        put(&t, "(");
        bool first = true;
        for (uint32_t i = 0; i < 11u; ++i) if (rest & (1u << listed[i])) {
            if (!first) put(&t, ",");
            put(&t, extra[listed[i]]); first = false;
        }
        put(&t, ")");
    }
    /* The bass only if the whole name still fits the screen (21 characters). */
    if (bass != root && t.n + (pcs[bass][1] ? 3u : 2u) <= TN_SCREEN) { put(&t, "/"); put(&t, pcs[bass]); }
    return t.n;
}
uint32_t tn_pad(tp_layout layout, uint32_t k, char out[TN_MAX]) {
    tp_notes notes; tp_position p;
    if (k >= 48u || !tp_position_of(k, &p) || !tc_pad(layout, k, &notes)) { out[0] = 0; return 0u; }
    uint32_t root = (layout.tonic + tc_step_semitones(layout.mode, p.column)) % 12u;
    return tn_name(&notes, (uint8_t)root, out);
}
