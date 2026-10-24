/* SPDX-License-Identifier: GPL-3.0-or-later - rhythm arithmetic (rhythm.h). */
#include "rhythm.h"
/* 32-bit arithmetic only (no 64-bit division on the board): whole groups of den steps, which are
 * whole numbers of clocks, then the remainder. Positions wrap at 2^32 like the clock counts. */
uint32_t rh_step_at(uint32_t pos, rh_rate r) {
    if (!r.num || !r.den) return 0u;
    uint32_t group = (uint32_t)r.num * RH_SUB; /* den steps */
    return pos / group * r.den + pos % group * r.den / group;
}
uint32_t rh_step_start(uint32_t step, rh_rate r) {
    if (!r.den) return 0u;
    /* Rounded up, so rh_step_at(rh_step_start(i)) == i. */
    return step / r.den * r.num * RH_SUB + (step % r.den * r.num * RH_SUB + r.den - 1u) / r.den;
}
uint64_t rh_rotate(uint64_t mask, uint32_t steps, uint32_t rotate) {
    if (!steps || steps > 64u) return 0u;
    uint64_t all = steps == 64u ? ~(uint64_t)0 : ((uint64_t)1u << steps) - 1u;
    rotate %= steps;
    mask &= all;
    if (!rotate) return mask;
    return ((mask << rotate) | (mask >> (steps - rotate))) & all;
}
uint64_t rh_euclid(uint32_t hits, uint32_t steps, uint32_t rotate) {
    if (!steps || steps > 64u) return 0u;
    if (hits > steps) hits = steps;
    uint64_t mask = 0u;
    /* Step i sounds where the count of hits so far, i * hits / steps, goes up: the first step
     * always sounds and the gaps differ by at most one step (Bjorklund's patterns up to rotation). */
    for (uint32_t i = 0; i < steps; ++i)
        if (hits && (i * hits) % steps < hits) mask |= (uint64_t)1u << i;
    return rh_rotate(mask, steps, rotate);
}
bool rh_euclid_at(uint32_t index, uint32_t *hits, uint32_t *steps) {
    for (uint32_t n = 2u; n <= 16u; ++n) {
        if (index < n - 1u) { *hits = index + 1u; *steps = n; return true; }
        index -= n - 1u;
    }
    return false;
}
