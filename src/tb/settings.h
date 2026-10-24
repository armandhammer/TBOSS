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
 * Step patterns (1.5.0): kind 7, slot = pattern * 16 + lane * 4 + chunk (1.5.0: pattern 0, slots
 * 0..15), the payload of lanes.h ln_encode: 16 steps of 64 bits, chunk 0 also the lane's settings.
 * A record that holds only defaults is absent. Edits are saved 3 s after the last one, only the
 * chunks that changed. */
enum { ST_LIVE = 1, ST_PRESET = 2, ST_SCALE = 3, ST_FAVOURITES = 4, ST_PROG = 5, ST_CHORDS = 6, ST_LANE = 7 };
/* Results shared with the SysEx protocol (sysex.h status byte). */
enum { ST_OK = 0, ST_BAD_VALUE = 3, ST_STORAGE = 5, ST_ABSENT = 7 };
/* Boot, before USB: restore user scales, then the newest valid live settings
 * (defaults otherwise), and note which presets exist. If the log is nearly full,
 * erase the sector and rewrite every current record first. */
bool st_load(tc_controls *c);
/* Every pass: write the live settings, and the pattern's changed chunks, 3 s after the last change.
 * Never erases. */
void st_service(tc_controls *c, uint32_t now);
/* Every pass: carry out a preset load/save the controls asked for (c->request). */
void st_requests(tc_controls *c, tp_engine *e);
/* A record's payload: the live settings are encoded from c; presets and scales
 * come from flash. ST_ABSENT if there is none. */
uint32_t st_get(uint8_t kind, uint8_t slot, const tc_controls *c, uint32_t payload[ST_PAYLOAD]);
/* Validates and stores a record; live settings and scales also take effect now
 * (the engine follows the new layout). ST_BAD_VALUE leaves everything unchanged. */
uint32_t st_put(uint8_t kind, uint8_t slot, const uint32_t payload[ST_PAYLOAD], tc_controls *c, tp_engine *e);
/* Stores user scale slot as it is now in layout.c. */
uint32_t st_save_scale(uint8_t slot);
uint32_t st_records(void); /* records in the log (diagnostics) */
#endif
