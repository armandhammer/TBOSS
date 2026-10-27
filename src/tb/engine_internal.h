/* SPDX-License-Identifier: GPL-3.0-or-later - what the engine's own files share (engine.c,
 * engine_pads.c, engine_effects.c, engine_arp.c, engine_play.c). The rest of the firmware uses
 * engine.h and controls.h only. */
#ifndef TB_ENGINE_INTERNAL_H
#define TB_ENGINE_INTERNAL_H
#include "engine.h"
#include "controls.h"
/* engine.c: the session and note ownership. A source owns its captured notes (references on
 * voices); a Note Off still owed on a note blocks it until it has gone out. */
void tp_stop(tp_engine *e);
tp_voice *tp_voice_of(tp_engine *e, uint8_t ch, uint8_t pitch);
bool tp_blocked(tp_engine *e, uint8_t ch, const tp_notes *n);
void tp_own(tp_engine *e, tp_key *key);
void tp_disown(tp_engine *e, tp_key *key);
void tp_start(tp_key *q, const tp_notes *notes, uint8_t ch, uint8_t velocity, uint32_t now);
/* engine_pads.c */
bool tp_latching(void);
void tp_measure_rest(tp_engine *e, const ts_scan *s);
uint8_t tp_velocity(const tp_engine *e, uint32_t k, uint8_t p, bool melody);
/* engine_effects.c */
bool tp_strums(uint32_t k);
bool tp_chance(tp_engine *e, uint32_t now);
void tp_strum(tp_engine *e, tp_key *key, bool on, uint32_t now);
void tp_strum_timers(tp_engine *e, uint32_t now);
void tp_echo_release(tp_engine *e, uint32_t i);
void tp_echo_start(tp_engine *e, const tp_key *key, uint32_t now);
void tp_echoes(tp_engine *e, uint32_t now);
bool tp_mono_returns(const tp_engine *e, uint32_t k);
void tp_mono_drop(tp_engine *e);
void tp_mono(tp_engine *e, uint32_t now);
void tp_bass_take(tp_engine *e, uint32_t k, uint8_t ch, const tp_notes *notes);
/* engine_arp.c */
void tp_arp_reshape(tp_engine *e);
void tp_arp_membership(tp_engine *e, uint32_t now);
void tp_arp_release(tp_engine *e);
void tp_arpeggiate(tp_engine *e, uint32_t now);
/* engine_play.c */
uint32_t tp_tick_us(uint32_t now);
uint32_t tp_clock_pos(const tp_engine *e, uint32_t now);
void tp_tick_clock(tp_engine *e, uint32_t now);
void tp_pedal_note(tp_engine *e, uint32_t now);
void tp_seq_release(tp_engine *e);
void tp_sequence(tp_engine *e, uint32_t now);
void tp_lane_release(tp_engine *e, uint32_t i);
void tp_lanes(tp_engine *e, uint32_t now);
void tp_lock_notes(tp_engine *e, uint32_t now);
#endif
