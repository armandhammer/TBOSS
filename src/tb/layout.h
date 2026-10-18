/* SPDX-License-Identifier: GPL-3.0-or-later - pad geometry and the musical layout (scales, chords). */
#ifndef TB_LAYOUT_H
#define TB_LAYOUT_H
#include <stdbool.h>
#include <stdint.h>
#include "scale_library.h"
/* Scale ids: 0..11 the classic built-in scales, 12..23 user scales U1..U12 (defined over
 * SysEx, sysex.c; U1..U3 are the mode buttons), 24..36 more built-in scales (0.6.0),
 * 37.. the scale library (0.7.0, scale_library.h from scripts/gen_scales.py).
 * The order keeps ids stored by earlier versions meaning the same scale. */
#define TC_SCALES 12u        /* classic built-in scales, ids 0..11 */
#define TC_USER_SCALES 12u   /* U1..U12, ids 12..23 */
#define TC_EXTRA_SCALES 13u  /* more built-in scales, ids 24..36 */
#define TC_EXTRA_FIRST (TC_SCALES + TC_USER_SCALES)
#define TC_LIBRARY_FIRST (TC_EXTRA_FIRST + TC_EXTRA_SCALES)
#define TC_ALL_SCALES (TC_LIBRARY_FIRST + TC_LIBRARY_SCALES)
#define TC_USER_NAME 12u
/* A user scale: bit n = n semitones above the root. The root bit is required and
 * 5..12 notes are allowed (fewer cannot keep all 96 pads inside MIDI range). */
typedef struct { uint16_t mask; char name[TC_USER_NAME + 1u]; } tc_user_scale;
#define TP_MAX_NOTES 16u     /* a stacked chord: up to 6 notes in up to 7 octaves, capped */
/* Pad wiring as measured: pad k = 48*side + 4*column + row. Side 0 left (chords),
 * side 1 right (melody); row 0 top .. 3 bottom; column 0..11 = scale steps. */
typedef struct { uint8_t side, row, column; } tp_position;
/* The right half's layout: scale steps (columns) by octaves (rows); chromatic (columns are
 * semitones, rows octaves); fourths (semitones, each row a fourth above the one below, like
 * a bass or guitar neck); drums (three 4x4 pads: scale notes, the same an octave down,
 * and drum notes 36..51, each pad on its own MIDI channel); live notes (0.8.0, the original
 * TheoryBoard's Live Note Update: columns 0..6 the scale by octaves, columns 7..11 the notes
 * of the live chord, see tc_live_chord). */
enum { TP_MELODY_SCALE, TP_MELODY_CHROMATIC, TP_MELODY_FOURTHS, TP_MELODY_DRUMS, TP_MELODY_LIVE,
       TP_MELODY_LAYOUTS };
#define TP_LIVE_COLUMN 7u /* first column of the live chord's notes */
/* mode is a scale id (layout.c table); bases are MIDI notes of the root at row 3. fold
 * (0.9.0): every chord's notes inside one fixed octave, G# to G above chord_base's C,
 * whatever the key (the original TheoryBoard's octave folding, as a backer described it);
 * the rows then raise its lowest note an octave at a time. */
typedef struct { uint8_t tonic, melody_base, chord_base, mode, melody_layout, fold; } tp_layout;
typedef struct { uint8_t notes[TP_MAX_NOTES], count; } tp_notes;
#define TP_CHORD_BASE 36u   /* C2 */
#define TP_MELODY_BASE 48u  /* C3 */
#define TP_DRUM_BASE 36u    /* drum pad: bottom-left C1 (GM kick) .. top-right 51 */
#define TP_OCTAVE_RANGE 24u /* bases may move up to two octaves either way */
#define TP_DEFAULT_LAYOUT {0u, TP_MELODY_BASE, TP_CHORD_BASE, 0u, TP_MELODY_SCALE, 0u}
/* Per chord-pad edits. lower: bit m drops triad member m (0 root, 1 third, 2 fifth) an
 * octave. sus replaces the third by the 2nd or 4th scale step. add stacks diatonic
 * extensions above the root-position chord. */
enum { TC_ADD6 = 1, TC_ADD7 = 2, TC_ADD9 = 4 };
typedef struct { uint8_t lower, sus, add; } tc_chord;
/* The chord control panel (centre buttons), for every chord pad. voicing 1..3: start the
 * rows' inversion ladder 1..3 inversions higher; 4: guitar (0.7.0: the chord as a guitar
 * plays it, six strings in standard tuning with a capo at fret `capo`; the rows are
 * positions up the neck). spread 1..3: the chord's lowest root down 1..3 octaves.
 * octaves: bit i plays the chord i-3 octaves from where it is (bit 3 alone = as is);
 * several bits stack. */
enum { TC_VOICING_GUITAR = 4u, TC_VOICINGS = 5u };
#define TC_SPREADS 4u
#define TC_OCTAVES_DEFAULT 0x08u
#define TC_CAPOS 12u
/* lead (0.9.0, auto voice leading): 0 = the row's ladder step; TC_LEAD_ZERO + n = ladder
 * step n instead (-7..12; below 0 the ladder continues an octave down; the voicing's
 * inversions are then already in n). */
#define TC_LEAD_ZERO 8
typedef struct { uint8_t voicing, spread, octaves, capo, lead; } tc_panel;
#define TC_PANEL_NEUTRAL {0u, 0u, TC_OCTAVES_DEFAULT, 0u, 0u}
/* The live chord (0.8.0): the last chord pad struck, or a chord arriving over MIDI in.
 * root is a pitch class; tones are its pitch classes as semitones above the root, ascending,
 * tones[0] = 0. Each row of the live area climbs these tones from the root (then the same an
 * octave up); rows are an octave apart. */
#define TC_LIVE_TONES 12u
typedef struct { uint8_t root, count, tones[TC_LIVE_TONES]; } tc_live_chord;
/* The chord pad's tones (its edits included, the panel ignored). */
void tc_live_of_pad(tp_layout layout, uint32_t key, tc_chord edit, tc_live_chord *out);
/* Held pitches as a live chord, the lowest one the root; false (out untouched) under two
 * pitch classes. */
bool tc_live_of_notes(const uint8_t held[128], tc_live_chord *out);
/* Whether pad k is in the live area of this layout. */
bool tp_live_pad(tp_layout layout, uint32_t key);
/* A live-area pad's note for this chord, always inside MIDI range. */
uint8_t tc_live_note(tp_layout layout, uint32_t key, const tc_live_chord *chord);
bool tp_index(tp_position p, uint8_t *k);
bool tp_position_of(uint32_t k, tp_position *p);
bool tc_layout_ok(tp_layout layout);
/* Whole-pad reject on any out-of-range note of the unshaped chord (and of melody pads);
 * output untouched then. The panel's voicing, spread and octave copies only drop notes
 * that would leave the MIDI range. */
bool tc_notes_panel(tp_layout layout, uint32_t key, tc_chord edit, tc_panel panel, tp_notes *out);
bool tc_notes(tp_layout layout, uint32_t key, tc_chord edit, tp_notes *out); /* neutral panel */
/* tc_notes_panel with the live per-chord edits and panel (controls.c). */
bool tc_pad(tp_layout layout, uint32_t key, tp_notes *out);
/* Auto voice leading (0.9.0): the ladder step of chord pad k that moves least from the
 * previous chord (prev), kept near the pad's own register (its row and the panel's
 * voicing I..III). */
int32_t tc_lead_step(tp_layout layout, uint32_t key, tc_chord edit, uint32_t voicing, const tp_notes *prev);
/* Chord pad k's chord before spread and octave copies, as tc_pad picks it now (with voice
 * leading when on): what the next chord leads from. False for melody pads. */
bool tc_pad_core(tp_layout layout, uint32_t key, tp_notes *out);
/* The live chord now: the MIDI-in chord while that was the latest, else the live pad's. */
void tc_live_current(tp_layout layout, tc_live_chord *out);
/* Melody side in the drums layout: which 4x4 pad (0 scale, 1 scale an octave down, 2 drums). */
uint32_t tp_drum_grid(uint32_t key);
const char *tc_scale_name(uint8_t scale);
bool tc_user_mask_ok(uint16_t mask);
/* Replaces slot (0..11); mask 0 clears it. Name: up to 12 printable characters,
 * others become spaces. False (nothing changed) for an invalid mask/slot. */
bool tc_set_user_scale(uint8_t slot, uint16_t mask, const char *name);
const tc_user_scale *tc_user_scale_get(uint8_t slot);
bool tc_scale_defined(uint8_t scale);
/* Semitones above the root of scale step d (0..). */
uint32_t tc_step_semitones(uint8_t scale, uint32_t d);
uint32_t tc_scale_size(uint8_t scale);
/* Whether the note `semis` semitones above the root is in the scale. */
bool tc_in_scale(uint8_t scale, uint32_t semis);
/* Scales in menu order (classic, more built-in, the library, then user slots) and back. */
uint8_t tc_scale_at(uint8_t position);
uint8_t tc_scale_position(uint8_t scale);
/* Mode families (0.7.0): the scale id of mode m (0..6) of family f, and the family's name.
 * Family 0 is the major scale's modes, the M1..M7 buttons' classic meaning. */
uint8_t tc_family_mode(uint8_t family, uint32_t mode);
const char *tc_family_name(uint8_t family);
#endif
