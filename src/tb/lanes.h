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
 * lane's swing, LN_SWING_MIN..LN_SWING_MAX % of a step later for every second step (2.5.1: negative, earlier). */
#define LN_NUDGES 16u
#define LN_SWING_MAX 50
#define LN_SWING_MIN (-50) /* 2.5.1 */
#define LN_GLIDES 5u /* 2.6.0, a lane's Bend glide: off, a quarter, half, three quarters, the whole of a step */
#define LN_TRANSPOSE_MAX 48 /* a lane's transpose, -48..48 (the record's range; the menu offers -24..24) */
/* A lane's direction (1.7.0; 2.6.8, the Toraiz Squid's running directions): a path through its loop window, bits 0-2,
 * and three switches: Mirror (bit 3: each row of four right to left), Reverse (bit 4: the path backwards) and Ping-pong
 * (bit 5: there and back, its ends once). A path lays the window's steps in blocks of 16 as 4 x 4, four to a row as the
 * step screen's 4X4 TOP grid shows them (a last block of fewer steps keeps the path's order of those it has): rows
 * (Forward: 1 2 3 4 5 ...), columns (1 5 9 13 2 6 ...), snake (1 2 3 4 8 7 6 5 9 ...), spiral (1 2 3 4 8 12 16 15 14 13
 * 9 5 6 7 11 10), spiral down (1 5 9 13 14 15 16 12 8 4 3 2 6 10 11 7), stagger (two on, one back in each row: 1 3 2 4
 * 5 7 6 8 ...) and random (every step once a time round, in an order of its own each time, from ln_hash, so it repeats
 * when played again; no switches). Before 2.6.8: 0 forward, 1 reverse, 2 ping-pong, 3 random. */
enum { LN_ROWS, LN_COLUMNS, LN_SNAKE, LN_SPIRAL, LN_SPIRAL_DOWN, LN_STAGGER, LN_RANDOM, LN_PATHS };
#define LN_FORWARD LN_ROWS
#define LN_PATH_BITS 7u
#define LN_MIRROR 8u
#define LN_REVERSE 16u   /* LN_ROWS | LN_REVERSE: Reverse, as before 2.6.8 */
#define LN_PINGPONG 32u  /* LN_ROWS | LN_PINGPONG: Ping-pong */
/* The board's Direction list (Settings > Steps, the step screen's U2): FORWARD REVERSE PING-PONG RANDOM COLUMNS SNAKE
 * SPIRAL SPIRAL DN STAGGER, then every other path with its switches (the web editor sets them one by one). */
#define LN_DIRECTION_VALUES 49u
bool ln_direction_ok(uint8_t direction);
uint8_t ln_direction_at(uint32_t v);         /* the v-th of the list (FORWARD past its end) */
uint32_t ln_direction_index(uint8_t direction);
/* What a lane plays: chords (a step is a chord pad, its edits as progressions keep them), melody (a
 * step is a scale step and octave: the right half's Scale layout position 0..47, 4 * column + row) or
 * drums (a step is a MIDI note). Off plays nothing. 0, the default, is chords. 2.6.3, CC (a user's request: a mod wheel
 * or any controller per step, as an analog sequencer's row drives a synth): a step's value is a controller value
 * 0..127, sent on the lane's channel as controller ln_cc (set in the web editor), no notes; Bend glide slews it. */
enum { LN_CHORDS, LN_MELODY, LN_DRUMS, LN_OFF, LN_CC, LN_KINDS };
/* A step, stored in 64 bits (ln_step_pack):
 *   bit 0 on, 1-7 value, 8-14 velocity (0 = 100), 15-22 edit (a chord pad's edits, settings.c's
 *   chord byte: lower | sus / 2 << 3 | add << 5), 23-29 scale (0 = the scale playing, n = scale id
 *   n - 1: a borrowed chord), 30-33 gate (0 = the lane's), 34-37 probability (0 = always), 38-40
 *   ratchet (0 = one strike), 41-45 condition (0 = always), 46-49 nudge (0 = on time; 2.0.3), 50 tie,
 *   51-63 pitch bend (2.4.12, roadmap C; 0 = none: firmware before 2.5 refuses a step with a bend): two's complement,
 *   LN_BEND_MIN .. LN_BEND_MAX of 4096 of the synth's bend range (controls.h Bend range), sent as 8192 + 2 * bend.
 * 2.6.8 (the Toraiz Squid's Count and Active), outside the 64 bits (the record's words 37 and 38): time, bits 0-1 how
 * many steps the step lasts less one (1..4: the lane's later steps come that much later), bit 2 skipped (taken out of
 * the lane: the steps after it come sooner). Both belong to the step's place, so turning it off or on keeps them. */
#define LN_BEND_MIN (-4096)
#define LN_BEND_MAX 4095
#define LN_LASTS 4u        /* a step lasts 1..4 steps */
#define LN_SKIP 4u         /* time's bit 2 */
#define LN_TIME_BITS 7u
typedef struct {
    bool on, tie;
    uint8_t value, velocity, edit, scale, gate, probability, ratchet, condition, nudge;
    uint8_t time; /* 2.6.8 (no padding byte before bend: the suites compare steps whole) */
    int16_t bend; /* 2.4.12 */
} ln_step;
uint32_t ln_lasts(const ln_step *s);  /* 2.6.8: 1..4 */
bool ln_skipped(const ln_step *s);    /* 2.6.8 */
/* A lane: kind, channel (0 = the kind's: chords the chord channel, melody the melody channel, drums
 * the drum channel), length (0 = 16, else 1..64), rate (0 = 1/16, n = TC_ARP_RATES index n - 1),
 * direction (LN_FORWARD ..; 1.7.0), swing (0..LN_SWING_MAX %; 2.0.3), mute, transpose (2.0.3: melody and
 * drum lanes in semitones, chord lanes in scale steps) and Euclid fill (the hits it was last filled with, 0 = none;
 * 1.7.0). 2.4.0 (the pattern record, settings.h ST_PATTERN): the loop window, steps loop_from + 1 .. loop_to (0 = the
 * lane's end; ln_window keeps it within the length), solo (while any lane of the pattern is soloed only soloed lanes
 * play) and follow (melody and chord lanes move with the chord pad struck last: engine_play.c). 2.6.0 (the pattern
 * record's word 10, 4 bits a lane): Bend glide, a step's pitch bend reached smoothly from the last step's over 1..4
 * quarters of the step (0 off: it jumps, as before 2.6). */
typedef struct {
    ln_step steps[LN_STEPS];
    uint8_t kind, channel, length, rate, direction;
    int8_t swing; /* 2.5.1: signed (stored as its byte; firmware before refuses a negative one) */
    uint8_t mute, euclid;
    int8_t transpose;
    uint8_t loop_from, loop_to, solo, follow; /* 2.4.0 */
    uint8_t glide; /* 2.6.0: Bend glide, 0 off, n = n quarters of a step (the pattern record's word 10; ln_glide) */
    uint8_t cc;    /* 2.6.3: a CC lane's controller, 1..119 (0: 1, the mod wheel); chunk 0's word 35 byte 1 */
} ln_lane;
/* 2.5.2: a pattern's groove (Settings > Patterns; the pattern record's word 9). All zero plays as before 2.5.2.
 *   swing: added to each lane's own (a delay -50..50 % of a step as a lane's, the sum kept within it);
 *   grid: what swings: each lane's own pairs of steps (LN_GRID_LANE, as before), or pairs of 8ths or 16ths whatever
 *     each lane's rate (a step between is pushed in proportion: ln_offset);
 *   density: tens of percent off 100 % (-10..10): every step's chance scaled down to 0 %, or up toward always (200 %);
 *   humanize: 0..10, a little random timing (up to LN_HUMAN_TICKS at 10) and velocity (up to LN_HUMAN_VELOCITY). */
enum { LN_GRID_LANE, LN_GRID_8THS, LN_GRID_16THS, LN_GRIDS };
#define LN_DENSITY_MIN (-10)
#define LN_DENSITY_MAX 10
#define LN_HUMANIZE_MAX 10u
#define LN_HUMAN_TICKS 128 /* rhythm.h units: half a MIDI clock (10 ms at 120 BPM) either way */
#define LN_HUMAN_VELOCITY 20
typedef struct { int8_t swing; uint8_t grid; int8_t density; uint8_t humanize; } ln_groove;
typedef struct {
    ln_lane lanes[LN_LANES];
    ln_groove groove; /* 2.5.2 */
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
uint32_t ln_glide(const ln_lane *l);           /* 2.6.0: 0..LN_GLIDES - 1 */
uint8_t ln_cc(const ln_lane *l);              /* 2.6.3: a CC lane's controller, 1..119 */
#define LN_CC_MAX 119u                        /* 120..127 are channel mode messages */
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
 * 0 only: word 36 = seed; 2.6.8, every chunk: word 37 = each step's lasts less one, 2 bits a step from step i's bits 2i,
 * word 38 = the skipped steps, bit i; everything else 0. */
void ln_encode(const ln_pattern *p, uint32_t slot, uint32_t words[LN_WORDS]);
/* A valid record for slot (< LN_PATTERN_RECORDS) goes into p and returns true; otherwise p is untouched. */
bool ln_decode(const uint32_t words[LN_WORDS], uint32_t slot, ln_pattern *p);
/* Whether slot's record would hold only defaults (no record needs storing). */
bool ln_chunk_default(const ln_pattern *p, uint32_t slot);
/* 2.4.0, the pattern record (settings.h ST_PATTERN): word 0 = 1 (0 with every lane at its defaults: all zero), word
 * 1 + i = lane i's loop_from | loop_to << 8 | solo << 16 | follow << 24, the rest 0. 2.5.2: word 9 the groove, its
 * swing's byte | grid << 8 | density's byte << 16 | humanize << 24 (firmware before refuses a record with it: going
 * back loses that pattern's loop windows, solos and follows; only a pattern with a groove). */
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
/* 2.6.8, a lane's time round as it plays: the steps it visits in turn (by its direction, its skipped steps left out) and
 * where each starts, in steps from the round's start (each lasts its step's 1..4). A visit is what the engine counts: the
 * count-th visit is round count / visits, visit count % visits of it. Built when the lane changes (engine_play.c keeps
 * one a lane); a random lane's order is its own each round, so its timeline is for one round (the visits and units are
 * the same every round). */
#define LN_VISITS (2u * LN_STEPS - 2u)
typedef struct {
    uint32_t round;              /* the round its order is for (a random lane's; 0 for the others) */
    uint16_t units;              /* a round's time in steps: the visits' lasts added (0: nothing visited) */
    uint8_t visits;              /* visits a round */
    uint8_t order[LN_VISITS];    /* the step each visit plays */
    uint16_t at[LN_VISITS + 1u]; /* where each visit starts, in steps from the round's start; at[visits] = units */
} ln_timeline;
void ln_timeline_build(const ln_lane *l, uint32_t lane, uint32_t seed, uint32_t round, ln_timeline *t);
/* Whether t (built for l) answers for the round count is in: always but a random lane's from another round. */
bool ln_timeline_fits(const ln_timeline *t, const ln_lane *l, uint32_t count);
/* 1.7.0. Visits a time round (ping-pong: 2 * length - 2, the ends played once; 2.4.0: of its loop window; 2.6.8: its
 * skipped steps left out), and 2.6.8 a round's time in steps (each visit its step's lasts). */
uint32_t ln_cycle(const ln_lane *l);
uint32_t ln_units(const ln_lane *l);
/* The functions below take the lane's timeline, t (NULL: built here, slower; the native suites). */
/* The step (< length; 2.4.0: within the loop window) the count-th visit plays, by the lane's direction, and how many
 * times the lane has gone round before it (loop, may be NULL). */
uint32_t ln_order(const ln_lane *l, uint32_t lane, uint32_t seed, uint32_t count, uint32_t *loop);
uint32_t ln_order_in(const ln_timeline *t, const ln_lane *l, uint32_t lane, uint32_t seed, uint32_t count, uint32_t *loop);
/* 2.6.8: where the count-th visit starts, in steps from the lane's start, and how many it lasts (*lasts, may be NULL). */
uint32_t ln_unit_in(const ln_timeline *t, const ln_lane *l, uint32_t lane, uint32_t seed, uint32_t count, uint32_t *lasts);
/* A:B of condition 1..9 (false for the others). */
bool ln_condition_ab(uint8_t condition, uint8_t *a, uint8_t *b);
uint32_t ln_probability_pct(uint8_t probability);
uint32_t ln_gate_pct(const ln_step *s);
uint32_t ln_strikes(const ln_step *s);
/* Whether step (of lane), on, plays the loop-th time round: its condition (fill: Fill is held, 2.0.3), then its
 * probability (ln_hash, so the same steps play each time the pattern is played from the start). */
bool ln_sounds(const ln_step *s, uint32_t lane, uint32_t seed, uint32_t loop, uint32_t step, bool fill, int32_t density);
/* 2.5.2: a step's chance (percent) with the pattern's density (ln_groove) */
uint32_t ln_chance(const ln_step *s, int32_t density);
/* 2.5.2: the velocity a step plays with the pattern's humanize (1..127), repeatable from the seed, lane and count */
uint8_t ln_human_velocity(const ln_groove *g, uint32_t seed, uint32_t lane, uint32_t count, uint8_t velocity);
/* 2.0.3, nudge and swing. A step's nudge in sixteenths (-8..7). */
int32_t ln_nudge(uint8_t nudge);
/* How far the count-th step of the lane (s: the step it plays, place: where it would start, length: its length in
 * rhythm.h units) starts after its place (before it when negative): its nudge (2.5.2: and humanize), plus the swing
 * (the lane's and the pattern's, g may be NULL) on every second step, kept within half a step either way so the
 * steps stay in order. 2.5.2, a grid of 8ths or 16ths: the nudged place moved along the swung pairs of the grid (its
 * off-beat by the swing, a place between in proportion), which keeps them in order too. */
int32_t ln_offset(const ln_lane *l, const ln_groove *g, const ln_step *s, uint32_t lane, uint32_t seed, uint32_t count,
                  uint32_t place, uint32_t length);
/* Where the count-th visit of lane starts, its offset included (wraps below 0: compare differences); 2.6.8 (_in): and its
 * length in rhythm.h units (*length, may be NULL: its lasts' steps, from its place). The offset's nudge and swing are of
 * one step (swing on every second step of the lane's grid: a visit starting on an odd step). */
uint32_t ln_start(const ln_lane *l, const ln_groove *g, uint32_t lane, uint32_t seed, uint32_t count, rh_rate rate);
uint32_t ln_start_in(const ln_timeline *t, const ln_lane *l, const ln_groove *g, uint32_t lane, uint32_t seed, uint32_t count,
                     rh_rate rate, uint32_t *length);
/* The visit count playing at pos (the last one started by ln_start); false before the first has started (a late
 * first step) and for a lane with nothing to visit. With no nudge, swing, lasts or skips it is rh_step_at(pos, rate). */
bool ln_count_at(const ln_lane *l, const ln_groove *g, uint32_t lane, uint32_t seed, uint32_t pos, rh_rate rate,
                 uint32_t *count, uint32_t *start); /* 2.5.2: and where it started (ln_start), unless start is NULL */
bool ln_count_at_in(const ln_timeline *t, const ln_lane *l, const ln_groove *g, uint32_t lane, uint32_t seed, uint32_t pos,
                    rh_rate rate, uint32_t *count, uint32_t *start);
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
