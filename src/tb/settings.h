/* SPDX-License-Identifier: GPL-3.0-or-later - persisted settings: an append-only record log in
 * flash sector 11 holding the live settings, 8 presets, the 12 user scales and the
 * favourite scales. */
#ifndef TB_SETTINGS_H
#define TB_SETTINGS_H
#include "controls.h"
#define ST_SAVE_DELAY_MS 3000u
#define ST_PAYLOAD 40u
#define ST_PAYLOAD_V2 20u /* records from firmware before 0.6.0 (SysEx backups) */
/* Favourites (0.7.0), slot 0: up to 128 scale ids as bytes in words 0..31, in order,
 * 0xFF after the last; no id twice; words 32..39 zero. */
/* Progressions (0.11.0): kind 5, slots 0..7, the payload of sequence.h sq_encode. */
/* Chord sets (1.0.0): kind 6, slots 0..7: word 0 = 1, word 1 + i = column i's chord: tone mask |
 * root << 12 (layout.h tc_set_chord; 0 = no chord), words 13..15 the name (12 bytes), the rest 0.
 * A set without any chord is absent.
 * Step patterns (1.5.0): kind 7, slot = pattern * 16 + lane * 4 + chunk (2.4.0: patterns 1-8's lanes 1-4, slots
 * 0..127, as 1.5.0 planned; lanes 5-8 in kind 20, ST_LANE_HI, slot = pattern * 16 + (lane - 4) * 4 + chunk; before
 * 2.4, one pattern of 4 lanes, slots 0..15, so firmware before 2.4 reads pattern 1's lanes 1-4 and ignores the
 * rest), the payload of lanes.h ln_encode: 16 steps of 64 bits, chunk 0 also the lane's settings. A record that holds only defaults is absent. Edits are saved 3 s after the last one, only the chunks that
 * changed.
 * 2.4.0: each pattern's loop windows, solos and follows, kind 18 (ST_PATTERN), slot = pattern, lanes.h
 * ln_extras_encode (absent or all zero: every lane's defaults); the song, kind 19 (ST_SONG), slot 0, lanes.h
 * ln_song_encode: the pattern playing, Song, the patterns in turn (absent: pattern 1, Song off, no song). Both saved
 * with the patterns' edits, 3 s after the last. */
/* The look (1.9.0): kind 8, slot 0 the board's, slots 1..8 presets 1..8's own (present: loading the preset
 * shows it; absent: the board's); slot 9 (SysEx only) is the look showing now, whichever it is. Word 0 =
 * 1 | scheme << 8 | Color by << 16 | Keep look << 24 (slot 0 and 9 only); words 1..9 = the roles' colors,
 * a byte each from role 0 (controls.h TC_LOOK_; 0 = built-in, 1..16 = palette color n - 1); the rest 0,
 * but slot 9 reads with word 39 = whose look it is (0 the board's, n preset n's). A record of zeros clears
 * a slot. Slot 0 is stored once it says more than the live settings' two scheme bits, or a preset's own
 * look shows (then the board's comes back with the next preset that has none). */
/* The feel (1.9.0): kind 9, slot 0: word 0 = 1, words 1..10 = controls.h's TC_FEEL_BYTES bytes (the curves),
 * the rest 0; absent (or all zero) = the default curves. Changes are saved with the settings (3 s after
 * the last), so dragging a curve in the editor writes once. 1.9.3, as the look: slots 1..8 presets 1..8's own
 * (present: loading the preset uses them; absent or all zero: the board's), word 0 byte 1 of slot 0 = Keep
 * feel; slot 9 (SysEx only) the curves in use, whichever they are, word 39 = whose (0 the board's, n preset
 * n's). Slot 0 reads as the board's curves (in use or not), with Keep feel. */
/* Preset names (1.9.3): kind 10, slot 0: word 0 = 1, then 8 names of 12 bytes (' '..'~', 0 after the last
 * character) from word 1 (preset n's bytes 12n..12n+11), the rest 0; absent (or all zero): none named. */
/* A preset record of zeros (1.9.3) clears the preset: absent from then on. */
/* Presets' own chord sets (2.0.0, Keep chords): kind 11. Slots 1..8 presets 1..8's own: word 0 = 1 | the chord set
 * slot it is a copy of (0..7) << 8, words 1..15 as a chord set record's (columns, name), the rest 0; present:
 * loading the preset puts it in that set's place (the board's comes back with the next preset that has none);
 * a record of zeros clears it. Slot 0: word 0 = 1 | Keep chords << 16, the rest 0. Slot 9 (SysEx only): the
 * set a preset's own covers, as it plays (word 0 = 1 | set << 8 | Keep chords << 16, words 1..15 the set,
 * word 39 = whose: n preset n's; 0 = none covers, words 1..15 then 0). ST_CHORDS slots are always the board's
 * own sets, also while one is covered. */
/* The chain (2.0.0): kind 12, slot 0, sequence.h sq_chain_encode. Each progression's chances: kind 13, slots 0..7
 * (beside ST_PROG's), sequence.h sq_chances_encode; recording a progression again sets them back to 100 %. */
/* The MIDI map (2.1.0, controls.h TC_CC_): kind 14, slot 0, a board setting (presets leave it). Word 0 = 1 | the
 * Control channel << 8; word 1 + i = slot i's kind | controller << 8 | target << 16 | jump << 24, word 17 + i =
 * its lo | hi << 8 (an empty slot all zeros); words 33..39 0. Absent (or all zero): no map, Control channel off.
 * Read over SysEx, word 39 is the last controller to arrive on the Control channel: number | value << 8 | channel
 * << 16 (0: none yet) | a count of them << 24 (MIDI learn in the editor); a write ignores it. */
/* The lights (2.4.0): kind 17, slot 0, board settings (presets leave them): word 0 = 1 | Out of scale << 8 | Dim << 16
 * (controls.h TC_OUT_, tc_dim_pct index), the rest 0. Absent (or all zero): AUTO, AUTO. Saved with the settings.
 * Firmware before 2.4 ignores it (going back forgets only these two). Kinds 15 and 16 are retired (2.3.3). */
enum { ST_LIVE = 1, ST_PRESET = 2, ST_SCALE = 3, ST_FAVOURITES = 4, ST_PROG = 5, ST_CHORDS = 6, ST_LANE = 7, ST_LOOK = 8,
       ST_FEEL = 9, ST_NAMES = 10, ST_OWN_SET = 11, ST_CHAIN = 12, ST_CHANCE = 13, ST_MIDI_MAP = 14, ST_LIGHTS = 17,
       ST_PATTERN = 18, ST_SONG = 19, ST_LANE_HI = 20, ST_EXTRA = 21 };
/* 2.5.1, the extras (kind 21, slot 0): what the live settings and the presets have beyond the settings record's 40
 * words, ST_EXTRA_WORDS words a part: part 0 the live settings', part 1 + n preset n's. A part's word 0 = 1 | the
 * arpeggiator's swing (0..50: 25..75, controls.h TC_SWINGS) << 8, or all 0 when what the settings record holds is
 * exact (its swing in 2.5's six steps). Firmware before 2.5.1 ignores the kind (going back keeps the six steps). */
#define ST_EXTRA_WORDS 4u
#define ST_LOOK_NOW 9u
#define ST_FEEL_NOW 9u
#define ST_OWN_SET_NOW 9u
/* Results shared with the SysEx protocol (sysex.h status byte). */
enum { ST_OK = 0, ST_BAD_VALUE = 3, ST_STORAGE = 5, ST_ABSENT = 7 };
/* Boot, before USB: restore user scales, then the newest valid live settings
 * (defaults otherwise), and note which presets exist. If the log is nearly full,
 * erase the sector and rewrite every current record first. */
bool st_load(tc_controls *c);
/* 2.3 review: called (when set) just before that erase, to warn that power must stay on (main.c: the screen). */
extern void (*st_before_erase)(void);
/* Every pass: write the live settings, and the patterns' changed chunks (2.4.0: their windows and the song too), 3 s
 * after the last change. Never erases. 2.4.5 (review TBR-007): at most one record a pass; true when it wrote (or
 * tried) one, so more may follow in the next pass. */
bool st_service(tc_controls *c, uint32_t now);
/* Every pass: carry out a preset load/save the controls asked for (c->request). */
void st_requests(tc_controls *c, tp_engine *e);
/* A record's payload: the live settings are encoded from c; presets and scales
 * come from flash. ST_ABSENT if there is none. */
uint32_t st_get(uint8_t kind, uint8_t slot, const tc_controls *c, uint32_t payload[ST_PAYLOAD]);
/* Validates and stores a record; live settings and scales also take effect now
 * (the engine follows the new layout). ST_BAD_VALUE leaves everything unchanged. */
uint32_t st_put(uint8_t kind, uint8_t slot, const uint32_t payload[ST_PAYLOAD], tc_controls *c, tp_engine *e);
/* As st_put, but a step pattern record (the web editor's grid, 1.9.3) only plays at once and is saved as
 * the step screen's edits are, 3 s after the last change: a burst of clicks writes each chunk once. */
uint32_t st_put_quiet(uint8_t kind, uint8_t slot, const uint32_t payload[ST_PAYLOAD], tc_controls *c, tp_engine *e);
/* SysEx VALUES (1.9.3): the controls preset slot would load (ST_PRESET; the board's own settings as they
 * are), or the live ones (ST_LIVE slot 0). ST_ABSENT for an empty preset, ST_BAD_VALUE for no such record
 * or one that can't load now (it names a user scale cleared since). */
uint32_t st_values(uint8_t kind, uint8_t slot, const tc_controls *c, tc_controls *out);
/* Stores user scale slot as it is now in layout.c. */
uint32_t st_save_scale(uint8_t slot);
uint32_t st_records(void); /* records in the log (diagnostics) */
#endif
