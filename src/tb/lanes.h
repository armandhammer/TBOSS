/* SPDX-License-Identifier: GPL-3.0-or-later - the step sequencer's core (1.5.0): a pattern of lanes
 * of steps, its stored form, and which step plays where. Pure: no engine, UI or hardware, so all of
 * it runs in the native tests and the fuzzers (engine_play.c plays the lanes, controls_steps.c edits them).
 *
 * Built so later features need no format change: every field of a step and a lane exists from the
 * start, and 0 is its default (gate 0 = the lane's, probability 0 = always, ratchet 0 = one strike,
 * condition 0 = always ...). 1.5.0 uses a step's on, value and velocity and a lane's kind, channel,
 * length, rate and mute; 1.7.0 a step's gate, probability, ratchet, condition and tie and a lane's
 * direction and Euclid fill; the other fields (nudge, swing, transpose) are kept as they are.
 *
 * A lane's step comes from the clock position, as a progression's does (engine_play.c seq_locate): step
 * count = rh_step_at(pos, rate), the step it plays = count % length. Lanes of different lengths and
 * rates are polymeter by themselves, and a DAW's Start, Continue and Song Position land correctly.
 * Randomness (probability, the random direction) comes from ln_hash(seed, lane, loop, step), not
 * from a stored state, so playback repeats exactly and can be located. */
#ifndef TB_LANES_H
#define TB_LANES_H
#include <stdbool.h>
#include <stdint.h>
#include "rhythm.h"
#include "layout.h"
#define LN_LANES 4u
#define LN_STEPS 64u
#define LN_CHUNK 16u                         /* steps per stored record */
#define LN_CHUNKS (LN_STEPS / LN_CHUNK)
#define LN_RECORDS (LN_LANES * LN_CHUNKS)    /* records of one pattern (record slots 0..15) */
#define LN_PATTERNS 1u                       /* 1.5.0: one pattern; slots leave room for 8 (0..127) */
#define LN_WORDS 40u                         /* a record's payload (settings.h ST_PAYLOAD) */
#define LN_VERSION 1u
#define LN_LENGTH_DEFAULT 16u
#define LN_RATES 12u                         /* controls.h TC_ARP_RATES: the arpeggiator's rates */
#define LN_RATE_DEFAULT 3u                   /* 1/16 */
#define LN_CHANNELS 17u                      /* 0 = the kind's own channel, 1..16 */
#define LN_GATE_PCT 50u                      /* a step's gate 0: half its length */
#define LN_VELOCITY_DEFAULT 100u
/* 1.7.0: a step's own gate (0 = LN_GATE_PCT, n = 10n %), probability (0 = always, n = 100 - 10n %),
 * ratchet (0 = one strike, n = n + 1 strikes) and condition (below); the values the step screen sets. */
#define LN_GATES 11u
#define LN_PROBABILITIES 10u
#define LN_RATCHETS 8u
/* A step's condition (1.7.0): always; A:B, play on the A-th of every B times round the lane (1:2 2:2 1:3
 * 2:3 3:3 1:4 2:4 3:4 4:4); only the first time round; every time but the first. */
enum { LN_ALWAYS, LN_FIRST_AB, LN_FIRST = 10, LN_NOT_FIRST, LN_CONDITIONS };
/* A lane's direction (1.7.0). Random plays every step once a time round, in an order of its own each
 * time (from ln_hash, so it repeats when played again). */
enum { LN_FORWARD, LN_REVERSE, LN_PINGPONG, LN_RANDOM, LN_DIRECTIONS };
/* What a lane plays: chords (a step is a chord pad, its edits as progressions keep them), melody (a
 * step is a scale step and octave: the right half's Scale layout position 0..47, 4 * column + row) or
 * drums (a step is a MIDI note). Off plays nothing. 0, the default, is chords. */
enum { LN_CHORDS, LN_MELODY, LN_DRUMS, LN_OFF, LN_KINDS };
/* A step, stored in 64 bits (ln_step_pack):
 *   bit 0 on, 1-7 value, 8-14 velocity (0 = 100), 15-22 edit (a chord pad's edits, settings.c's
 *   chord byte: lower | sus / 2 << 3 | add << 5), 23-29 scale (0 = the scale playing, n = scale id
 *   n - 1: a borrowed chord), 30-33 gate (0 = the lane's), 34-37 probability (0 = always), 38-40
 *   ratchet (0 = one strike), 41-45 condition (0 = always; Elektron style A:B later), 46-49 nudge
 *   (0 = on time), 50 tie, 51-63 spare (0). */
typedef struct {
    bool on, tie;
    uint8_t value, velocity, edit, scale, gate, probability, ratchet, condition, nudge;
} ln_step;
/* A lane: kind, channel (0 = the kind's: chords the chord channel, melody the melody channel, drums
 * the drum channel), length (0 = 16, else 1..64), rate (0 = 1/16, n = TC_ARP_RATES index n - 1),
 * direction (LN_FORWARD ..; 1.7.0), swing (reserved), mute, transpose (semitones; reserved) and Euclid
 * fill (the hits it was last filled with, 0 = none; 1.7.0). */
typedef struct {
    ln_step steps[LN_STEPS];
    uint8_t kind, channel, length, rate, direction, swing, mute, euclid;
    int8_t transpose;
} ln_lane;
typedef struct {
    ln_lane lanes[LN_LANES];
    uint16_t seed;
    uint8_t version;
} ln_pattern;
extern ln_pattern ln;        /* the pattern that plays and that the step screen edits */
extern uint16_t ln_dirty;    /* bit lane * 4 + chunk: changed since stored (settings.c writes it) */
extern uint32_t ln_changes;  /* bumps with every change of ln (settings.c saves 3 s after the last) */
void ln_clear(ln_pattern *p);
/* Marks step (or, step >= LN_STEPS, the lane's own settings in chunk 0) of lane as changed. */
void ln_changed(uint32_t lane, uint32_t step);
uint32_t ln_length(const ln_lane *l);          /* 1..64 */
uint32_t ln_rate(const ln_lane *l);            /* TC_ARP_RATES index */
void ln_set_length(ln_lane *l, uint32_t length);
void ln_set_rate(ln_lane *l, uint32_t rate);
bool ln_any(const ln_pattern *p);              /* a lane that plays has a step on */
uint64_t ln_step_pack(ln_step s);
/* False (out untouched) for spare bits set, sus 3 or an unknown scale. */
bool ln_step_unpack(uint64_t bits, ln_step *out);
tc_chord ln_edit(uint8_t byte);
uint8_t ln_edit_byte(tc_chord edit);
/* Records: slot = pattern * 16 + lane * 4 + chunk. Word 0 = LN_VERSION; words 1 + 2i, 2 + 2i = step
 * chunk * 16 + i (low, high); chunk 0 only: word 33 = kind | channel << 8 | length << 16 | rate << 24,
 * 34 = direction | swing << 8 | mute << 16 | (uint8_t)transpose << 24, 35 = Euclid fill; lane 0 chunk
 * 0 only: word 36 = seed; everything else 0. */
void ln_encode(const ln_pattern *p, uint32_t slot, uint32_t words[LN_WORDS]);
/* A valid record for slot (< LN_RECORDS) goes into p and returns true; otherwise p is untouched. */
bool ln_decode(const uint32_t words[LN_WORDS], uint32_t slot, ln_pattern *p);
/* Whether slot's record would hold only defaults (no record needs storing). */
bool ln_chunk_default(const ln_pattern *p, uint32_t slot);
/* The lane step at clock position pos (rhythm.h units from the pattern's start): the steps started
 * so far (count, may be NULL) and count % length, the step that plays. */
uint32_t ln_locate(uint32_t pos, rh_rate rate, uint32_t length, uint32_t *count);
/* Where the gate of step count ends (pct of the step's length). */
uint32_t ln_gate_end(uint32_t count, rh_rate rate, uint32_t pct);
/* The value a left-half pad (0..47) gives a step of this kind: chords and melody the pad's position
 * (4 * column + row), drums a note: TP_DRUM_BASE (36) at the bottom left, up each column of 4 and on
 * to the right (36..83). */
uint8_t ln_pad_value(uint8_t kind, uint32_t pad);
/* Repeatable randomness: a 32-bit hash of the pattern's seed, the lane, how many times the lane has
 * gone round and the step. */
uint32_t ln_hash(uint32_t seed, uint32_t lane, uint32_t loop, uint32_t step);
/* 1.7.0. Steps a time round the lane (ping-pong: 2 * length - 2, the ends played once). */
uint32_t ln_cycle(const ln_lane *l);
/* The step (< length) the count-th step started plays, by the lane's direction, and how many times
 * the lane has gone round before it (loop, may be NULL). */
uint32_t ln_order(const ln_lane *l, uint32_t lane, uint32_t seed, uint32_t count, uint32_t *loop);
/* A:B of condition 1..9 (false for the others). */
bool ln_condition_ab(uint8_t condition, uint8_t *a, uint8_t *b);
uint32_t ln_probability_pct(uint8_t probability);
uint32_t ln_gate_pct(const ln_step *s);
uint32_t ln_strikes(const ln_step *s);
/* Whether step (of lane), on, plays the loop-th time round: its condition, then its probability
 * (ln_hash, so the same steps play each time the pattern is played from the start). */
bool ln_sounds(const ln_step *s, uint32_t lane, uint32_t seed, uint32_t loop, uint32_t step);
/* Euclid fill: hits steps spread as evenly as they go over the lane's length, rotated; a hit already on
 * keeps what it plays, a new one gets brush; the others go off (cleared, as tapping them off does).
 * 0 hits: the steps stay as they are. Remembers hits in the lane. */
void ln_euclid_fill(ln_lane *l, uint32_t hits, uint32_t rotate, ln_step brush);
#endif
