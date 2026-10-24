/* SPDX-License-Identifier: GPL-3.0-or-later - chords and keys from what is played (chord_detect.h). */
#include "chord_detect.h"
#include "chord_name.h"
#include "controls.h"
static uint32_t count(uint32_t x) { uint32_t n = 0; for (; x; x &= x - 1u) n++; return n; }
static uint32_t up(uint32_t set, uint32_t by) { by %= 12u; return ((set << by) | (set >> (12u - by))) & 0xFFFu; }
bool cd_chord(uint16_t classes, uint8_t bass, uint8_t *root, uint16_t *tones) {
    uint32_t set = classes & 0xFFFu;
    if (count(set) < 2u) return false;
    uint8_t r = tn_root((uint16_t)set, bass);
    uint32_t relative = up(set, 12u - r), kept = 0;
    for (uint32_t b = 0; b < 12u && count(kept) < TC_SET_TONES; ++b) kept |= relative & (1u << b);
    *root = r; *tones = (uint16_t)kept;
    return true;
}
uint8_t cd_key(const uint8_t *roots, const uint16_t *tones, uint32_t n) {
    int32_t best_score = INT32_MIN; uint8_t best = 0;
    for (uint8_t key = 1; key < TC_PIVOT_KEYS && n; ++key) {
        uint32_t scale = tc_pivot_scale(key), tonic = (key - 1u) % 12u;
        bool minor = key > 12u;
        int32_t score = 0;
        for (uint32_t i = 0; i < n; ++i) {
            uint32_t notes = up(tones[i], roots[i]);
            score += 2 * (int32_t)count(notes & scale) - 3 * (int32_t)count(notes & ~scale);
        }
        /* A first or last chord on the tonic, minor in a minor key and major in a major one. */
        for (uint32_t end = 0; end < 2u; ++end) {
            uint32_t i = end ? n - 1u : 0u;
            bool chord_minor = ((tones[i] >> 3) & 1u) && !((tones[i] >> 4) & 1u);
            if (roots[i] % 12u == tonic && chord_minor == minor) score += end ? 3 : 4;
        }
        if (score > best_score) { best_score = score; best = key; }
    }
    return best;
}
