/* SPDX-License-Identifier: GPL-3.0-or-later - chord progressions (0.11.0): recorded by tapping chord
 * pads, played back by the engine as if the pad were held (engine.c sequence()).
 * A step stores the pad (scale degree and inversion), not notes, so the root buttons transpose
 * a playing progression and the mode buttons re-colour it; a step recorded under another mode
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
typedef struct { uint8_t pad, units, velocity, scale; tc_chord edit; } sq_entry;
typedef struct {
    sq_entry entries[SQ_ENTRIES];
    uint8_t count, unit, scale; /* scale: the mode it was recorded in (steps in another keep theirs) */
    uint8_t total;              /* units */
} sq_prog;
extern sq_prog sq; /* the progression of the selected slot */
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
/* The layout step en plays with: the current one (its root), in its own scale when it has one. */
tp_layout sq_layout(tp_layout current, const sq_entry *en);
#endif
