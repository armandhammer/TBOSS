/* SPDX-License-Identifier: GPL-3.0-or-later - bit helpers for sets of pitch classes (12-bit masks:
 * bit n is n semitones above C, or above a chord's root), shared by the chord, scale and capture code. */
#ifndef TB_BITS_H
#define TB_BITS_H
#include <stdint.h>
/* How many bits are set. */
static inline uint32_t tb_count(uint32_t x) { uint32_t n = 0; for (; x; x &= x - 1u) n++; return n; }
/* A set moved up (or down) by semitones, round the octave. */
static inline uint32_t tb_rotate_up(uint32_t set, uint32_t by) {
    set &= 0xFFFu; by %= 12u;
    return ((set << by) | (set >> (12u - by))) & 0xFFFu;
}
static inline uint32_t tb_rotate_down(uint32_t set, uint32_t by) { return tb_rotate_up(set, 12u - by % 12u); }
/* The n lowest notes of a set. */
static inline uint32_t tb_lowest(uint32_t set, uint32_t n) {
    uint32_t kept = 0;
    for (set &= 0xFFFu; set && n; --n) { kept |= set & (0u - set); set &= set - 1u; }
    return kept;
}
#endif
