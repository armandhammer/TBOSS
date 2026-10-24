/* SPDX-License-Identifier: GPL-3.0-or-later - rhythm arithmetic (1.4.0): step lengths as fractions
 * of a MIDI clock, clock positions finer than a clock, and Euclidean rhythms. Pure functions, no
 * hardware: the arpeggiator and note repeat use them now, the step sequencer's lanes later.
 *
 * A position counts MIDI clocks (24 per beat) in 1/RH_SUB steps. A rate is a step length of
 * num/den clocks, so steps that are not whole clocks (5 or 7 to a beat) land exactly: step i
 * starts at floor(i * num * RH_SUB / den), never drifting however long it runs. */
#ifndef TB_RHYTHM_H
#define TB_RHYTHM_H
#include <stdbool.h>
#include <stdint.h>
#define RH_SUB 256u
typedef struct { uint8_t num, den; } rh_rate;
/* The step a position falls in, and where step i starts (both from position 0). */
uint32_t rh_step_at(uint32_t pos, rh_rate rate);
uint32_t rh_step_start(uint32_t step, rh_rate rate);
/* Hits spread as evenly as possible over steps (1..64): bit i = step i sounds. rotate moves the
 * pattern that many steps later. hits >= steps: every step; hits 0: none. */
uint64_t rh_euclid(uint32_t hits, uint32_t steps, uint32_t rotate);
/* The Euclidean patterns a menu lists, in order: for steps 2..16, hits 1..steps-1 (120). */
#define RH_EUCLIDS 120u
bool rh_euclid_at(uint32_t index, uint32_t *hits, uint32_t *steps);
/* Rotates the low `steps` bits of mask by rotate steps (later). */
uint64_t rh_rotate(uint64_t mask, uint32_t steps, uint32_t rotate);
#endif
