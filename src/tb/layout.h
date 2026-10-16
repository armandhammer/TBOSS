/* SPDX-License-Identifier: GPL-3.0-or-later - pad geometry and the musical layout (scales, chords). */
#ifndef TB_LAYOUT_H
#define TB_LAYOUT_H
#include <stdbool.h>
#include <stdint.h>
#define TC_SCALES 12u      /* built-in scales, ids 0..11 */
#define TC_USER_SCALES 3u  /* U1..U3, ids 12..14, defined over SysEx (sysex.c) */
#define TC_ALL_SCALES (TC_SCALES + TC_USER_SCALES)
#define TC_USER_NAME 12u
/* A user scale: bit n = n semitones above the root. The root bit is required and
 * 5..12 notes are allowed (fewer cannot keep all 96 pads inside MIDI range). */
typedef struct { uint16_t mask; char name[TC_USER_NAME + 1u]; } tc_user_scale;
#define TP_MAX_NOTES 6u
/* Pad wiring as measured: pad k = 48*side + 4*column + row. Side 0 left (chords),
 * side 1 right (melody); row 0 top .. 3 bottom; column 0..11 = scale steps. */
typedef struct { uint8_t side, row, column; } tp_position;
/* mode is a scale id (layout.c table); bases are MIDI notes of the root at row 3. */
typedef struct { uint8_t tonic, melody_base, chord_base, mode; } tp_layout;
typedef struct { uint8_t notes[TP_MAX_NOTES], count; } tp_notes;
#define TP_CHORD_BASE 36u   /* C2 */
#define TP_MELODY_BASE 48u  /* C3 */
#define TP_OCTAVE_RANGE 24u /* bases may move up to two octaves either way */
#define TP_DEFAULT_LAYOUT {0u, TP_MELODY_BASE, TP_CHORD_BASE, 0u}
/* Per chord-pad edits (centre function buttons). lower: bit m drops triad member m
 * (0 root, 1 third, 2 fifth) an octave. sus replaces the third by the 2nd or 4th
 * scale step. add stacks diatonic extensions above the root-position chord. */
enum { TC_ADD6 = 1, TC_ADD7 = 2, TC_ADD9 = 4 };
typedef struct { uint8_t lower, sus, add; } tc_chord;
bool tp_index(tp_position p, uint8_t *k);
bool tp_position_of(uint32_t k, tp_position *p);
bool tc_layout_ok(tp_layout layout);
/* Whole-pad reject on any out-of-range note; output untouched then. */
bool tc_notes(tp_layout layout, uint32_t key, tc_chord edit, tp_notes *out);
/* tc_notes with the live per-chord edits (controls.c). */
bool tc_pad(tp_layout layout, uint32_t key, tp_notes *out);
const char *tc_scale_name(uint8_t scale);
bool tc_user_mask_ok(uint16_t mask);
/* Replaces slot (0..2); mask 0 clears it. Name: up to 12 printable characters,
 * others become spaces. False (nothing changed) for an invalid mask/slot. */
bool tc_set_user_scale(uint8_t slot, uint16_t mask, const char *name);
const tc_user_scale *tc_user_scale_get(uint8_t slot);
bool tc_scale_defined(uint8_t scale);
/* Semitones above the root of scale step d (0..). */
uint32_t tc_step_semitones(uint8_t scale, uint32_t d);
uint32_t tc_scale_size(uint8_t scale);
#endif
