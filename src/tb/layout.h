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
 * of the live chord, see tc_live_chord); bass (1.7.0, the original's promised bass mode: each scale
 * note on two side-by-side pads, 6 to a row, the scale climbing on from the bottom row's left to the
 * top row's right, TP_BASS_DROP below the melody octave; a pad struck on a note its twin still
 * plays strikes it again, engine_effects.c tp_bass_take). */
enum { TP_MELODY_SCALE, TP_MELODY_CHROMATIC, TP_MELODY_FOURTHS, TP_MELODY_DRUMS, TP_MELODY_LIVE,
       TP_MELODY_BASS, TP_MELODY_LAYOUTS };
#define TP_LIVE_COLUMN 7u /* first column of the live chord's notes */
#define TP_BASS_DROP 24u  /* the bass layout's root: two octaves below the melody base (C1 by default) */
/* mode is a scale id (layout.c table); bases are MIDI notes of the root at row 3. fold
 * (0.9.0): every chord's notes inside one fixed octave, G# to G above chord_base's C,
 * whatever the key (the original TheoryBoard's octave folding, as a backer described it);
 * the rows then raise its lowest note an octave at a time. chords (1.0.0): 0 = the scale's
 * chords on the left half, n = chord set n (tc_chord_set_get(n - 1)), TC_CHORDS_FULL (1.3.0)
 * the Full layout, TC_CHORDS_GRID (1.3.0) the chord grid. */
typedef struct { uint8_t tonic, melody_base, chord_base, mode, melody_layout, fold, chords; } tp_layout;
typedef struct { uint8_t notes[TP_MAX_NOTES], count; } tp_notes;
#define TP_CHORD_BASE 36u   /* C2 */
#define TP_MELODY_BASE 48u  /* C3 */
#define TP_DRUM_BASE 36u    /* drum pad: bottom-left C1 (GM kick) .. top-right 51 */
#define TP_OCTAVE_RANGE 24u /* bases may move up to two octaves either way */
#define TP_DEFAULT_LAYOUT {0u, TP_MELODY_BASE, TP_CHORD_BASE, 0u, TP_MELODY_SCALE, 0u, 0u}
/* Per chord-pad edits. lower: bit m drops triad member m (0 root, 1 third, 2 fifth) an
 * octave. sus replaces the third by the 2nd or 4th scale step. add stacks diatonic
 * extensions above the root-position chord. */
enum { TC_ADD6 = 1, TC_ADD7 = 2, TC_ADD9 = 4 };
typedef struct { uint8_t lower, sus, add; } tc_chord;
/* Chord sets (1.0.0): any chord on each of the 12 chord-pad columns instead of the scale's.
 * A column's chord: tones as a mask (bit n = n semitones above its root, root bit set, 1..6
 * notes; 0 = no chord, the column is silent) and its root in semitones above the key's root
 * (0..23), so the root buttons transpose the set. The rows climb its inversions as they do a
 * scale chord's (root position at the bottom). sus and added notes don't apply to them; a
 * lowered member (lower) does. Stored as settings records (settings.c, kind ST_CHORDS). */
#define TC_CHORD_SETS 8u
#define TC_SET_TONES 6u
#define TC_SET_ROOTS 24u
typedef struct { uint16_t mask; uint8_t root; } tc_set_chord;
typedef struct { tc_set_chord column[12]; char name[13]; } tc_chord_set;
bool tc_set_chord_ok(tc_set_chord chord);
/* Replaces set slot (0..7); one without any chord clears it. Name: as a user scale's.
 * False (nothing changed) for an invalid chord or slot. */
bool tc_chord_set_put(uint8_t slot, const tc_chord_set *set);
const tc_chord_set *tc_chord_set_get(uint8_t slot);
bool tc_chord_set_defined(uint8_t slot);
/* Full chord layout (1.3.0), tp_layout.chords == TC_CHORDS_FULL: the original TheoryBoard's
 * expanded layout. Each of the scale's chords gets a run of variations (C, Cadd2, C6, ...),
 * packed down the columns from the top left: 48 pads, one chord each in its own voicing (the
 * rows are not inversions; per-chord edits don't apply). The chords and voicings are the
 * original firmware's records on the external memory chip, read through the function given to
 * tc_full_reader (read only). An entry that differs from the same entry in the other keys is a
 * typing slip and plays as they do. A scale the chip has no record for plays a 7-note mode's
 * that holds all its notes (major modes first, then melodic minor, harmonic minor, harmonic
 * major, double harmonic, Neapolitan); a scale without one, or every scale when the chip has no
 * records, gets a layout built the same way from common chord types. */
#define TC_CHORDS_FULL (TC_CHORD_SETS + 1u)
/* Chord grid (1.3.0), tp_layout.chords == TC_CHORDS_GRID: the scale's chords with a chord type
 * per row, so where a chord is says what it is. Columns are scale steps as in the scale's
 * chords; the rows from the bottom up play the triad, the seventh, the sus chord (sus4, or
 * sus2 where the scale's 4th above the root is not a perfect fourth) and the ninth (with its
 * seventh), every one in root position (voicing I..III and voice leading invert them). A pad's
 * own edits don't apply; held chord-shape buttons do (a held sus replaces the row's). */
#define TC_CHORDS_GRID (TC_CHORDS_FULL + 1u)
typedef bool (*tc_read_fn)(uint32_t address, uint8_t *out, uint32_t len);
void tc_full_reader(tc_read_fn read); /* NULL: build every layout. Forgets what was read. */
/* Reads (or builds) the Full chords of the layout's scale and key now, so that a pad never
 * waits for the chip; nothing unless layout.chords is TC_CHORDS_FULL. */
void tc_full_prepare(tp_layout layout);
/* 1.7.0: on (main.c, around the engine's masked section): a Full layout not in the cache is not read
 * from the chip but built for that call; tc_full_fetch then reads it (true when it did). */
void tc_full_quick(bool on);
bool tc_full_fetch(void);
/* Whether the layout's Full chords are the original's records (false: built). */
bool tc_full_original(tp_layout layout);
/* What the chord pads may play: 0 (the scale's chords), a defined chord set, Full or the grid. */
bool tc_chords_ok(uint8_t chords);
/* The pitch class chord pad k (0..47) builds on: its scale step, or its chord set chord's root. */
uint8_t tc_pad_root(tp_layout layout, uint32_t key);
/* Whether chord pad k has no chord (a chord set's empty column). */
bool tc_pad_empty(tp_layout layout, uint32_t key);
/* Theory hints (1.0.0): the chord pads to go to after chord pad k. Strong: the most usual
 * next chord(s); also: other common ones. In a seven-note scale with its own chords by scale
 * step (I -> IV V vi, ii -> V vii, iii -> vi IV, IV -> V I ii, V -> I vi, vi -> ii IV,
 * vii -> I iii; the first is strong), otherwise by root movement (a fifth down strong, a
 * fifth up or a third down also). Bit j = chord pad j. */
uint64_t tc_hints(tp_layout layout, uint32_t key, uint64_t *also);
/* Pivot chords (1.4.0): with a target key (1..12 the major key on C..B, 13..24 the minor key, 0
 * none), the chord pads whose every note is also in the target key (its major or natural minor
 * scale). Strong: those that are ii or IV (minor: ii or iv) there, the chords that lead best to
 * its V; also: the other shared chords. Bit k = chord pad k, as tc_hints. */
#define TC_PIVOT_KEYS 25u
uint64_t tc_pivots(tp_layout layout, uint8_t target, uint64_t *also);
uint16_t tc_pivot_scale(uint8_t target); /* the target key's pitch classes, bit n = n semitones above C */
/* The pitch classes chord pad k plays with these edits (bit n = n semitones above C); 0 if none. */
uint16_t tc_pad_classes(tp_layout layout, uint32_t key, tc_chord edit);
/* Scale lock (1.0.0): the nearest note of the layout's scale and key to pitch (between two,
 * the lower), inside MIDI range. */
uint8_t tc_quantise(tp_layout layout, uint8_t pitch);
/* The harmonizer (1.6.0): the note `by` steps of the layout's scale above pitch (below when
 * negative), counted from pitch's own step, or from the scale note below it when pitch is not in
 * the scale. 0xFF when outside MIDI range. */
uint8_t tc_harmony_note(tp_layout layout, uint8_t pitch, int32_t by);
/* The chord control panel (centre buttons), for every chord pad. voicing 1..3: start the
 * rows' inversion ladder 1..3 inversions higher; 4: guitar (0.7.0: the chord as a guitar
 * plays it, six strings in standard tuning with a capo at fret `capo`; the rows are
 * positions up the neck). spread 1..3: the chord's lowest root down 1..3 octaves; with
 * spread_chord (1.7.0, Settings > Chords > Spread of: Chord) the whole chord opens instead: every
 * second note from the bottom an octave up, and for 2..3 the lowest root also down 1..2 octaves; with
 * Bass (2.0.2) the chord stays as it is and gains a root under it: just under its lowest note (none when
 * that is the root), for 2..3 another 1..2 octaves down.
 * octaves: bit i plays the chord i-3 octaves from where it is (bit 3 alone = as is);
 * several bits stack. */
enum { TC_VOICING_GUITAR = 4u, TC_VOICINGS = 5u };
#define TC_SPREADS 4u
enum { TC_SPREAD_OF_ROOT, TC_SPREAD_OF_CHORD, TC_SPREAD_OF_BASS, TC_SPREAD_OFS }; /* Spread of; Bass 2.0.2 */
#define TC_OCTAVES_DEFAULT 0x08u
#define TC_CAPOS 12u
/* lead (0.9.0, auto voice leading): 0 = the row's ladder step; TC_LEAD_ZERO + n = ladder
 * step n instead (-7..12; below 0 the ladder continues an octave down; the voicing's
 * inversions are then already in n). */
#define TC_LEAD_ZERO 8
typedef struct { uint8_t voicing, spread, octaves, capo, lead, spread_chord; } tc_panel;
#define TC_PANEL_NEUTRAL {0u, 0u, TC_OCTAVES_DEFAULT, 0u, 0u, 0u}
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
/* tc_pad and tc_pad_core for chord pad k (0..47) with the given edits instead of the pad's
 * own (a recorded progression step keeps the edits it was recorded with). */
bool tc_pad_with(tp_layout layout, uint32_t key, tc_chord edit, tp_notes *out);
bool tc_pad_core_with(tp_layout layout, uint32_t key, tc_chord edit, tp_notes *out);
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
