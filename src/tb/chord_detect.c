/* SPDX-License-Identifier: GPL-3.0-or-later - chords and keys from what is played (chord_detect.h). */
#include "chord_detect.h"
#include "chord_name.h"
#include "controls.h"
#include "bits.h"
bool cd_chord(uint16_t classes, uint8_t bass, uint8_t *root, uint16_t *tones) {
    uint32_t set = classes & 0xFFFu;
    if (tb_count(set) < 2u) return false;
    uint8_t r = tn_root((uint16_t)set, bass);
    uint32_t kept = tb_lowest(tb_rotate_down(set, r), TC_SET_TONES);
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
            uint32_t notes = tb_rotate_up(tones[i], roots[i]);
            score += 2 * (int32_t)tb_count(notes & scale) - 3 * (int32_t)tb_count(notes & ~scale);
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
/* Krumhansl and Kessler's profiles (1982) times 100, degree 0 the tonic. */
static const uint16_t kk_profile[2][12] = {{635, 223, 348, 233, 438, 409, 252, 519, 239, 366, 229, 288},
                                           {633, 268, 352, 538, 260, 353, 254, 475, 398, 269, 334, 317}};
static uint32_t isqrt(uint32_t v) {
    uint32_t r = 0, bit = 1u << 30;
    while (bit > v) bit >>= 2;
    while (bit) { if (v >= r + bit) { v -= r + bit; r = (r >> 1) + bit; } else r >>= 1; bit >>= 2; }
    return r;
}
uint32_t cd_keys(const uint16_t heard[12], uint8_t *keys, uint32_t max) {
    uint32_t total = 0, classes = 0;
    for (uint32_t i = 0; i < 12u; ++i) { total += heard[i]; classes += heard[i] ? 1u : 0u; }
    if (total < CD_KEY_NOTES || classes < 3u || !max) return 0;
    /* Pearson's r for key k is sum(h[i] * (12 p[i - k] - P)) / (|12 p - P| * the counts' own spread); the counts' part is
     * the same for every key, so keys compare by score / norm: a / na against b / nb as a * nb against b * na. */
    int32_t score[24];
    uint32_t norm[2];
    for (uint32_t m = 0; m < 2u; ++m) {
        int32_t sum = 0; uint32_t squares = 0;
        for (uint32_t i = 0; i < 12u; ++i) sum += kk_profile[m][i];
        for (uint32_t i = 0; i < 12u; ++i) { int32_t c = 12 * kk_profile[m][i] - sum; squares += (uint32_t)(c * c); }
        norm[m] = isqrt(squares);
        for (uint32_t k = 0; k < 12u; ++k) {
            int32_t s = 0;
            for (uint32_t i = 0; i < 12u; ++i) s += (int32_t)heard[(k + i) % 12u] * (12 * kk_profile[m][i] - sum);
            score[12u * m + k] = s;
        }
    }
    uint32_t taken = 0, n = 0;
    while (n < max && n < 24u) {
        int32_t best = -1;
        for (uint32_t key = 0; key < 24u; ++key) {
            if ((taken >> key) & 1u) continue;
            if (best < 0) { best = (int32_t)key; continue; }
            uint32_t was = (uint32_t)best;
            int64_t a = (int64_t)score[key] * norm[was / 12u], b = (int64_t)score[was] * norm[key / 12u];
            if (a > b) best = (int32_t)key; /* a tie keeps the earlier: major before minor, then the lower root */
        }
        if (best < 0 || score[(uint32_t)best] <= 0) break;
        taken |= 1u << (uint32_t)best; keys[n++] = (uint8_t)(best + 1);
    }
    return n;
}
