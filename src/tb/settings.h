/* SPDX-License-Identifier: GPL-3.0-or-later - persisted settings: an append-only record log in
 * flash sector 11 holding the live settings, 8 presets and the 12 user scales. */
#ifndef TB_SETTINGS_H
#define TB_SETTINGS_H
#include "controls.h"
#define ST_SAVE_DELAY_MS 3000u
#define ST_PAYLOAD 40u
#define ST_PAYLOAD_V2 20u /* records from firmware before 0.6.0 (SysEx backups) */
enum { ST_LIVE = 1, ST_PRESET = 2, ST_SCALE = 3 };
/* Results shared with the SysEx protocol (sysex.h status byte). */
enum { ST_OK = 0, ST_BAD_VALUE = 3, ST_STORAGE = 5, ST_ABSENT = 7 };
/* Boot, before USB: restore user scales, then the newest valid live settings
 * (defaults otherwise), and note which presets exist. If the log is nearly full,
 * erase the sector and rewrite every current record first. */
bool st_load(tc_controls *c);
/* Every pass: write the live settings 3 s after the last change. Never erases. */
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
