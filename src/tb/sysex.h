/* SPDX-License-Identifier: GPL-3.0-or-later - the TheoryBoard SysEx protocol (version 1), used by
 * the web editor (webapp/index.html). Full description: docs/tb-sysex.md.
 *
 *   request  F0 7D 54 42 cmd data... sum F7
 *   reply    F0 7D 54 42 cmd|40 status data... sum F7
 *
 * 7D is the MIDI manufacturer ID for non-commercial use; 54 42 ("TB") marks this
 * protocol. sum makes cmd + data (+ status) + sum a multiple of 128. Binary data
 * travels 7-bit packed: each group of up to 7 bytes becomes a byte of their top
 * bits (bit i = byte i) followed by the bytes' low 7 bits. */
#ifndef TB_SYSEX_H
#define TB_SYSEX_H
#include "settings.h"
#define SX_VERSION 1u
#define SX_OUT_MAX 2400u
enum { SX_IDENTIFY = 1, SX_GET_SCALE, SX_SET_SCALE, SX_PREVIEW, SX_SELECT_SCALE,
       SX_GET_RECORD, SX_SET_RECORD, SX_NOR_READ };
/* Status byte; 0, 3, 5 and 7 are shared with settings.h. */
enum { SX_OK = 0, SX_CHECKSUM = 1, SX_LENGTH = 2, SX_BAD_VALUE = 3, SX_UNKNOWN = 4,
       SX_STORAGE = 5, SX_NOR = 6, SX_ABSENT = 7 };
#define SX_PREVIEW_MS 5000u
extern uint8_t sx_out[SX_OUT_MAX];
/* Handles one complete message (F0..F7). Returns the reply length in sx_out, 0 for
 * no reply (preview, or not addressed to us). */
uint32_t sx_handle(const uint8_t *msg, uint32_t len, tc_controls *c, tp_engine *e, uint32_t now);
/* The scale the editor is previewing (bit n = n semitones above root), 0 if none. */
uint16_t sx_preview(uint32_t now, uint8_t *root);
/* The user slot (0..2) the editor said it is editing with its last preview, 0xFF if none. */
uint8_t sx_preview_slot(void);
/* 7-bit packing (exposed for tests). Returns the packed / unpacked length. */
uint32_t sx_pack(const uint8_t *in, uint32_t n, uint8_t *out);
uint32_t sx_unpack(const uint8_t *in, uint32_t n, uint8_t *out);
#endif
