/* SPDX-License-Identifier: GPL-3.0-or-later - the step sequencer's core (1.5.0): a pattern of lanes
 * of steps, its stored form, and which step plays where. Pure: no engine, UI or hardware, so all of
 * it runs in the native tests and the fuzzers (engine_play.c plays the lanes, controls_steps.c edits them).
 *
 * Built so later features need no format change: every field of a step and a lane exists from the
 * start, and 0 is its default (gate 0 = the lane's, probability 0 = always, ratchet 0 = one strike,
 * condition 0 = always ...). 1.5.0 uses a step's on, value and velocity and a lane's kind, channel,
 * length, rate and mute; 1.7.0 a step's gate, probability, ratchet, condition and tie and a lane's
 * direction and Euclid fill; 2.0.3 a step's nudge, a lane's swing and transpose, and the Fill conditions.
 *
 * A lane's step comes from the clock position, as a progression's does (engine_play.c seq_locate): step
 * count = rh_step_at(pos, rate), the step it plays = count % length. Lanes of different lengths and
 * rates are polymeter by themselves, and a DAW's Start, Continue and Song Position land correctly.
 * Randomness (probability, the random direction) comes from ln_hash(seed, lane, loop, step), not
 * from a stored state, so playback repeats exactly and can be located.
 *
 * 2.4.0, "the sequencer grows up": 8 lanes a pattern and 8 patterns (ln_bank; ln is the one playing, which the step
 * screen edits), each lane's loop window (the steps it plays, within its length), solo, and whether it follows the
 * chord pads (live transpose); the patterns chained into a song (ln_song). Pattern 1's lanes 1-4 keep the record
 * slots they had (0..15), so firmware before 2.4 still reads them and ignores the rest. */
#ifndef TB_LANES_H
#define TB_LANES_H
#include <stdbool.h>
#include <stdint.h>
#include "rhythm.h"
#include "layout.h"
#define LN_LANES 8u                          /* 2.4.0; 4 before */
#define LN_LANES_V1 4u                       /* the lanes firmware before 2.4 has (pattern 1's, slots 0..15) */
#define LN_STEPS 64u
#define LN_CHUNK 16u                         /* steps per stored record */
#define LN_CHUNKS (LN_STEPS / LN_CHUNK)
#define LN_PATTERN_RECORDS (LN_LANES * LN_CHUNKS)          /* records of one pattern: 32 */
#define LN_PATTERNS 8u                                     /* 2.4.0; 1 before */
#define LN_RECORDS (LN_PATTERNS * LN_PATTERN_RECORDS)      /* records g = pattern * 32 + lane * 4 + chunk (stored as kinds 7 and 20: settings.h) */
#define LN_SONG 16u                          /* 2.4.0: a song's links (patterns in turn), at most */
#define LN_NO_PATTERN 0xFFu
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
 * 2:3 3:3 1:4 2:4 3:4 4:4); only the first time round; every time but the first; 2.0.3: only while Fill is
 * held (a centre button or the pedal), only while it isn't. Firmware before 2.0.3 plays Fill steps always. */
enum { LN_ALWAYS, LN_FIRST_AB, LN_FIRST = 10, LN_NOT_FIRST, LN_FILL, LN_NOT_FILL, LN_CONDITIONS };
/* 2.0.3: a step's nudge, -8..7 sixteenths of a step (stored as 4 bits, two's complement: 0 on time), and a
 * lane's swing, 0..LN_SWING_MAX % of a step later for every second step. */
#define LN_NUDGES 16u
#define LN_SWING_MAX 50u
#define LN_TRANSPOSE_MAX 48 /* a lane's transpose, -48..48 (the record's range; the menu offers -24..24) */
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
 *   ratchet (0 = one strike), 41-45 condition (0 = always), 46-49 nudge (0 = on time; 2.0.3), 50 tie,
 *   51-63 spare (0). */
typedef struct {
    bool on, tie;
    uint8_t value, velocity, edit, scale, gate, probability, ratchet, condition, nudge;
} ln_step;
/* A lane: kind, channel (0 = the kind's: chords the chord channel, melody the melody channel, drums
 * the drum channel), length (0 = 16, else 1..64), rate (0 = 1/16, n = TC_ARP_RATES index n - 1),
 * direction (LN_FORWARD ..; 1.7.0), swing (0..LN_SWING_MAX %; 2.0.3), mute, transpose (2.0.3: melody and
 * drum lanes in semitones, chord lanes in scale steps) and Euclid fill (the hits it was last filled with, 0 = none;
 * 1.7.0). 2.4.0 (the pattern record, settings.h ST_PATTERN): the loop window, steps loop_from + 1 .. loop_to (0 = the
 * lane's end; ln_window keeps it within the length), solo (while any lane of the pattern is soloed only soloed lanes
 * play) and follow (melody and chord lanes move with the chord pad struck last: engine_play.c). */
typedef struct {
    ln_step steps[LN_STEPS];
    uint8_t kind, channel, length, rate, direction, swing, mute, euclid;
    int8_t transpose;
    uint8_t loop_from, loop_to, solo, follow; /* 2.4.0 */
} ln_lane;
typedef struct {
    ln_lane lanes[LN_LANES];
    uint16_t seed;
    uint8_t version;
} ln_pattern;
/* 2.4.0: a song, the patterns played in turn (each for its time round), round and round while on. */
typedef struct { uint8_t count, on, links[LN_SONG]; } ln_song_list;
extern ln_pattern ln_bank[LN_PATTERNS]; /* every pattern (in the core-coupled RAM on the board: lanes.c) */
extern uint8_t ln_current;  /* the pattern that plays and that the step screen edits */
#define ln (ln_bank[ln_current % LN_PATTERNS])
/* The pattern chosen to play next (LN_NO_PATTERN: none): at once while stopped, else at the end of the current one's
 * time round (engine_play.c). */
extern uint8_t ln_queued;
extern ln_song_list ln_song;
extern uint32_t ln_dirty[LN_PATTERNS]; /* bit lane * 4 + chunk: changed since stored (settings.c writes it) */
extern uint8_t ln_extra_dirty;         /* 2.4.0: bit n: pattern n's loop windows, solos, follows changed */
extern bool ln_song_dirty;             /* 2.4.0: the song, the pattern playing or Song changed */
extern uint32_t ln_changes;  /* bumps with every change of the patterns (settings.c saves 3 s after the last) */
void ln_clear(ln_pattern *p);
/* Marks step (or, step >= LN_STEPS, the lane's own settings in chunk 0) of the current pattern's lane as changed. */
void ln_changed(uint32_t lane, uint32_t step);
void ln_changed_in(uint32_t pattern, uint32_t lane, uint32_t step);
void ln_extra_changed(uint32_t pattern); /* 2.4.0: a lane's loop window, solo or follow */
void ln_song_changed(void);
/* 2.4.0: the pattern playing becomes n (the song goes on as it is): marked to be saved; the step screen follows. */
void ln_select(uint32_t pattern);
uint32_t ln_length(const ln_lane *l);          /* 1..64 */
/* 2.0.0: a lane's steps moved one step later (by 1) or earlier (by -1), the last coming round to the first
 * (within its length; steps past it stay), and every step off (its settings kept). The editor does the same
 * (webapp/tb-lanes.js), and copies and pastes whole lanes (steps and settings: the pasted lane takes the
 * copied one's length). */
void ln_rotate(ln_lane *l, int32_t by);
void ln_clear_steps(ln_lane *l);
uint32_t ln_rate(const ln_lane *l);            /* TC_ARP_RATES index */
void ln_set_length(ln_lane *l, uint32_t length);
void ln_set_rate(ln_lane *l, uint32_t rate);
bool ln_any(const ln_pattern *p);              /* a lane that plays has a step on */
/* 2.4.0: whether lane i of p sounds: not Off, not muted, and soloed while any lane of p is. */
bool ln_plays(const ln_pattern *p, uint32_t i);
/* 2.4.0: the loop window: the first step it plays (*first) and how many (1 .. length). */
uint32_t ln_window(const ln_lane *l, uint32_t *first);
/* 2.4.0: a pattern copied (steps, settings, windows) or emptied; marked to be saved. */
void ln_copy_pattern(uint32_t from, uint32_t to);
void ln_clear_pattern(uint32_t pattern);
uint64_t ln_step_pack(ln_step s);
/* False (out untouched) for spare bits set, sus 3 or an unknown scale. */
bool ln_step_unpack(uint64_t bits, ln_step *out);
tc_chord ln_edit(uint8_t byte);
uint8_t ln_edit_byte(tc_chord edit);
/* Records (2.4.0: ln_encode and ln_decode take the slot within the pattern,
 * lane * 4 + chunk, < LN_PATTERN_RECORDS). Word 0 = LN_VERSION; words 1 + 2i, 2 + 2i = step
 * chunk * 16 + i (low, high); chunk 0 only: word 33 = kind | channel << 8 | length << 16 | rate << 24,
 * 34 = direction | swing << 8 | mute << 16 | (uint8_t)transpose << 24, 35 = Euclid fill; lane 0 chunk
 * 0 only: word 36 = seed; everything else 0. */
void ln_encode(const ln_pattern *p, uint32_t slot, uint32_t words[LN_WORDS]);
/* A valid record for slot (< LN_PATTERN_RECORDS) goes into p and returns true; otherwise p is untouched. */
bool ln_decode(const uint32_t words[LN_WORDS], uint32_t slot, ln_pattern *p);
/* Whether slot's record would hold only defaults (no record needs storing). */
bool ln_chunk_default(const ln_pattern *p, uint32_t slot);
/* 2.4.0, the pattern record (settings.h ST_PATTERN): word 0 = 1 (0 with every lane at its defaults: all zero), word
 * 1 + i = lane i's loop_from | loop_to << 8 | solo << 16 | follow << 24, the rest 0. */
void ln_extras_encode(const ln_pattern *p, uint32_t words[LN_WORDS]);
bool ln_extras_decode(const uint32_t words[LN_WORDS], ln_pattern *p);
/* 2.4.0, the song record (settings.h ST_SONG): word 0 = 1 | the pattern playing << 8 | Song << 16, words 1..4 the
 * links, a byte each from word 1's lowest (pattern + 1; 0 after the last), the rest 0. */
void ln_song_encode(uint32_t words[LN_WORDS]);
bool ln_song_decode(const uint32_t words[LN_WORDS], ln_song_list *song, uint8_t *current);
/* The value a left-half pad (0..47) gives a step of this kind: chords and melody the pad's position
 * (4 * column + row), drums a note: TP_DRUM_BASE (36) at the bottom left, up each column of 4 and on
 * to the right (36..83). */
uint8_t ln_pad_value(uint8_t kind, uint32_t pad);
/* Repeatable randomness: a 32-bit hash of the pattern's seed, the lane, how many times the lane has
 * gone round and the step. */
uint32_t ln_hash(uint32_t seed, uint32_t lane, uint32_t loop, uint32_t step);
/* 1.7.0. Steps a time round the lane (ping-pong: 2 * length - 2, the ends played once; 2.4.0: of its loop window). */
uint32_t ln_cycle(const ln_lane *l);
/* The step (< length; 2.4.0: within the loop window) the count-th step started plays, by the lane's direction, and how
 * many times the lane has gone round before it (loop, may be NULL). */
uint32_t ln_order(const ln_lane *l, uint32_t lane, uint32_t seed, uint32_t count, uint32_t *loop);
/* A:B of condition 1..9 (false for the others). */
bool ln_condition_ab(uint8_t condition, uint8_t *a, uint8_t *b);
uint32_t ln_probability_pct(uint8_t probability);
uint32_t ln_gate_pct(const ln_step *s);
uint32_t ln_strikes(const ln_step *s);
/* Whether step (of lane), on, plays the loop-th time round: its condition (fill: Fill is held, 2.0.3), then its
 * probability (ln_hash, so the same steps play each time the pattern is played from the start). */
bool ln_sounds(const ln_step *s, uint32_t lane, uint32_t seed, uint32_t loop, uint32_t step, bool fill);
/* 2.0.3, nudge and swing. A step's nudge in sixteenths (-8..7). */
int32_t ln_nudge(uint8_t nudge);
/* How far the count-th step of the lane (s: the step it plays, length: its length in rhythm.h units) starts
 * after its place (before it when negative): its nudge, plus the lane's swing on every second step, kept within
 * half a step either way so the steps stay in order. */
int32_t ln_offset(const ln_lane *l, const ln_step *s, uint32_t count, uint32_t length);
/* Where the count-th step of lane starts, its offset included (wraps below 0: compare differences). */
uint32_t ln_start(const ln_lane *l, uint32_t lane, uint32_t seed, uint32_t count, rh_rate rate);
/* The step count playing at pos (the last one started by ln_start); false before the first has started (a late
 * first step). With no nudge or swing it is rh_step_at(pos, rate). */
bool ln_count_at(const ln_lane *l, uint32_t lane, uint32_t seed, uint32_t pos, rh_rate rate, uint32_t *count);
/* 2.0.3, mutate: about pct % of the lane's steps (within its length) change a little, repeatably from seed, lane
 * and salt (ln_hash). Chord and melody steps that are on move one or two pads left or right (another chord or
 * scale step, the row kept; reflected at the ends) and some change velocity; a drum step goes off, or on as the
 * nearest step on before it. The editor does the same (webapp/tb-lanes.js mutate). */
void ln_mutate(ln_lane *l, uint32_t lane, uint32_t seed, uint32_t pct, uint32_t salt);
/* Euclid fill: hits steps spread as evenly as they go over the lane's length, rotated; a hit already on
 * keeps what it plays, a new one gets brush; the others go off (cleared, as tapping them off does).
 * 0 hits: the steps stay as they are. Remembers hits in the lane. */
void ln_euclid_fill(ln_lane *l, uint32_t hits, uint32_t rotate, ln_step brush);
#endif
