/* SPDX-License-Identifier: GPL-3.0-or-later - pad geometry and the musical layout (scales, chords). */
#ifndef TB_LAYOUT_H
#define TB_LAYOUT_H
#include <stdbool.h>
#include <stdint.h>
/* Scale ids: 0..11 the classic built-in scales, 12..23 user scales U1..U12 (defined over
 * SysEx, sysex.c; U1..U3 are the mode buttons), 24..36 more built-in scales (0.6.0).
 * The order keeps ids stored by earlier versions meaning the same scale. */
#define TC_SCALES 12u        /* classic built-in scales, ids 0..11 */
#define TC_USER_SCALES 12u   /* U1..U12, ids 12..23 */
#define TC_EXTRA_SCALES 13u  /* more built-in scales, ids 24..36 */
#define TC_EXTRA_FIRST (TC_SCALES + TC_USER_SCALES)
#define TC_ALL_SCALES (TC_EXTRA_FIRST + TC_EXTRA_SCALES)
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
 * and drum notes 36..51, each pad on its own MIDI channel). */
enum { TP_MELODY_SCALE, TP_MELODY_CHROMATIC, TP_MELODY_FOURTHS, TP_MELODY_DRUMS, TP_MELODY_LAYOUTS };
/* mode is a scale id (layout.c table); bases are MIDI notes of the root at row 3. */
typedef struct { uint8_t tonic, melody_base, chord_base, mode, melody_layout; } tp_layout;
typedef struct { uint8_t notes[TP_MAX_NOTES], count; } tp_notes;
#define TP_CHORD_BASE 36u   /* C2 */
#define TP_MELODY_BASE 48u  /* C3 */
#define TP_DRUM_BASE 36u    /* drum pad: bottom-left C1 (GM kick) .. top-right 51 */
#define TP_OCTAVE_RANGE 24u /* bases may move up to two octaves either way */
#define TP_DEFAULT_LAYOUT {0u, TP_MELODY_BASE, TP_CHORD_BASE, 0u, TP_MELODY_SCALE}
/* Per chord-pad edits. lower: bit m drops triad member m (0 root, 1 third, 2 fifth) an
 * octave. sus replaces the third by the 2nd or 4th scale step. add stacks diatonic
 * extensions above the root-position chord. */
enum { TC_ADD6 = 1, TC_ADD7 = 2, TC_ADD9 = 4 };
typedef struct { uint8_t lower, sus, add; } tc_chord;
/* The chord control panel (centre buttons), for every chord pad. voicing 1..3: start the
 * rows' inversion ladder 1..3 inversions higher; 4: guitar (each chord spread over six
 * strings). spread 1..3: the chord's lowest root down 1..3 octaves. octaves: bit i plays
 * the chord i-3 octaves from where it is (bit 3 alone = as is); several bits stack. */
enum { TC_VOICING_GUITAR = 4u, TC_VOICINGS = 5u };
#define TC_SPREADS 4u
#define TC_OCTAVES_DEFAULT 0x08u
typedef struct { uint8_t voicing, spread, octaves; } tc_panel;
#define TC_PANEL_NEUTRAL {0u, 0u, TC_OCTAVES_DEFAULT}
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
/* Scales in menu order (classic, more built-in, then user slots) and back. */
uint8_t tc_scale_at(uint8_t position);
uint8_t tc_scale_position(uint8_t scale);
#endif
