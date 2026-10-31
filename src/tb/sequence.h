/* SPDX-License-Identifier: GPL-3.0-or-later - chord progressions (0.11.0): recorded by tapping chord
 * pads, played back by the engine as if the pad were held (engine_play.c tp_sequence()).
 * A step stores the pad (scale degree and inversion), not notes, so the root buttons transpose
 * a playing progression and the mode buttons re-color it; a step recorded under another mode
 * keeps that scale (a borrowed chord). Lengths are in units (1 beat .. 2 bars); tapping the
 * same chord again lengthens it. Saved as settings records (settings.c, kind ST_PROG). */
#ifndef TB_SEQUENCE_H
#define TB_SEQUENCE_H
#include "layout.h"
#define SQ_SLOTS 8u
#define SQ_ENTRIES 38u     /* distinct steps (one 40-word settings record) */
#define SQ_UNITS_MAX 64u   /* units in a progression */
#define SQ_RUN_MAX 32u     /* units in one step; more of the same chord starts another step */
#define SQ_NO_SCALE 127u   /* a step that plays the progression's current scale */
#define SQ_WORDS 40u
enum { SQ_UNIT_BEAT, SQ_UNIT_HALF, SQ_UNIT_BAR, SQ_UNIT_TWO_BARS, SQ_UNIT_KINDS };
/* 2.0.0, each chord's chance (settings.h ST_CHANCE): the chance it plays as recorded, as Chance's values
 * (controls.h tc_chance_pct: 0 = 100 % .. 9 = 10 %), | SQ_BORROW: otherwise the same chord pad in the parallel
 * key (major <-> natural minor, by the step's 3rd) instead of a rest. */
#define SQ_CHANCES 10u
#define SQ_BORROW 0x10u
enum { SQ_PLAYS, SQ_RESTS, SQ_BORROWS }; /* how a step plays this time round (engine_play.c) */
typedef struct { uint8_t pad, units, velocity, scale; tc_chord edit; uint8_t chance; } sq_entry;
typedef struct {
    sq_entry entries[SQ_ENTRIES];
    uint8_t count, unit, scale; /* scale: the mode it was recorded in (steps in another keep theirs) */
    uint8_t total;              /* units */
    uint8_t chords;             /* 1.0.0: the chord set it was recorded with (tp_layout.chords) */
} sq_prog;
extern sq_prog sq; /* the progression of the selected slot */
/* 2.0.0: every slot's progression as stored, with its chances (settings.c keeps them current), for the chain. */
extern sq_prog sq_slots[SQ_SLOTS];
/* The chain (2.0.0, settings.h ST_CHAIN): up to SQ_CHAIN progression slots played in turn, then again from the
 * first (with Settings > Sequence > Chain on). sq_chain_changes bumps when it or a progression in it changes. */
#define SQ_CHAIN 8u
typedef struct { uint8_t count, links[SQ_CHAIN]; } sq_chain_list;
extern sq_chain_list sq_chain;
extern uint32_t sq_chain_changes;
/* Bumps whenever sq is replaced or changed (the sq_ functions below on sq itself, settings.c,
 * a new unit): a playing progression then starts again from its first chord. */
extern uint32_t sq_changes;
extern uint8_t sq_present; /* bit n: slot n holds a progression (settings.c keeps it current) */
void sq_clear(sq_prog *p, uint8_t unit, uint8_t scale);
/* Recording: one tap = one unit of the chord on pad (0..47) as recorded under scale with edit.
 * The same chord again lengthens the last step. False (nothing changed) when full. */
bool sq_tap(sq_prog *p, uint8_t pad, uint8_t velocity, uint8_t scale, tc_chord edit);
void sq_undo(sq_prog *p); /* removes the last unit */
uint32_t sq_unit_ticks(uint8_t unit); /* MIDI clocks (24 per beat, 4/4): 24, 48, 96, 192 */
/* Units before step i (where it starts). */
uint32_t sq_start(const sq_prog *p, uint32_t i);
void sq_encode(const sq_prog *p, uint32_t words[SQ_WORDS]);
/* A valid record fills out and returns true; anything malformed leaves out untouched. */
bool sq_decode(const uint32_t words[SQ_WORDS], sq_prog *out);
/* The layout step en of p plays with: the current one (its root), in its own scale when it has
 * one, with p's chord set (the scale's chords if that set has been cleared since). */
tp_layout sq_layout(const sq_prog *p, tp_layout current, const sq_entry *en);
/* 2.0.0, the chances record: word 0 = 1, then a byte per step from word 1 (step i: byte i), the rest 0; all zero
 * (or absent): every chord plays. Encoded from p's steps; decoded into p's (false, p untouched: not a record). */
void sq_chances_encode(const sq_prog *p, uint32_t words[SQ_WORDS]);
bool sq_chances_decode(const uint32_t words[SQ_WORDS], sq_prog *p);
/* The chain record: word 0 = 1 | count << 8, word 1 = slot of link i in bits 4i..4i+2, the rest 0; all zero (or
 * absent): no chain. */
void sq_chain_encode(const sq_chain_list *chain, uint32_t words[SQ_WORDS]);
bool sq_chain_decode(const uint32_t words[SQ_WORDS], sq_chain_list *out);
#endif
