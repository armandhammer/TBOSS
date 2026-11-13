/* SPDX-License-Identifier: GPL-3.0-or-later - what the controls' own files share (controls.c,
 * controls_menu.c, controls_buttons.c, controls_steps.c, controls_record.c). The rest of the firmware
 * uses controls.h only. */
#ifndef TB_CONTROLS_INTERNAL_H
#define TB_CONTROLS_INTERNAL_H
#include "controls.h"
#include <string.h>
/* A counter one on, staying at its top. */
static inline uint32_t tc_inc(uint32_t x) { return x==UINT32_MAX ? x : x+1u; }
/* controls.c: a load or save for settings.c to carry out (st_requests). */
void tc_request(tc_controls *c, uint8_t kind, uint8_t slot);
/* controls_menu.c: a navigation button on the play screen, in Settings or on the step screen. */
void tc_navigate(tc_controls *c, tp_engine *e, uint32_t index);
/* controls_buttons.c */
void tc_shortcut(tc_controls *c, tp_engine *e, uint32_t index);
void tc_button(tc_controls *c, tp_engine *e, uint32_t index, uint32_t now);
void tc_shift_root(tc_controls *c, uint32_t index);
/* 2.0.0: a centre button's action (as tc_button does it, Shift aside); U3 pressed as Shift; Shift + a button
 * under the play screen; Shut down (the menu's) and waking (the screen and lights back). */
void tc_action(tc_controls *c, tp_engine *e, uint8_t act, uint32_t now);
void tc_u3_press(tc_controls *c, uint32_t now);
/* 2.4.13: board button `logical` (tc_poll's number) pressed or let go by a MIDI map slot, as the touch chip's would be. */
void tc_board_button(tc_controls *c, tp_engine *e, uint32_t logical, bool pressed, uint32_t now);
/* 2.4.13 (D): step option `field` of every held step set across its range by value v of 0..most (controls_steps.c). */
void tc_steps_set_field(tc_controls *c, uint8_t field, uint32_t v, uint32_t most);
/* 2.5.4: what the board's buttons do, each in one place, for the buttons and for the MIDI map's named functions
 * (controls.h TC_NAMED_). controls.c: the key (a root button on the play screen), a mode button's scale, a preset
 * loaded (Shift + a mode button). controls_steps.c: a lane muted or soloed (or back), every lane playing, the next or
 * previous pattern, the step option chosen, the step screen opened. */
void tc_key(tc_controls *c, tp_engine *e, uint32_t index);
void tc_scale_button(tc_controls *c, tp_engine *e, uint32_t index);
void tc_load_preset(tc_controls *c, uint32_t index);
void tc_lane_toggle(tc_controls *c, uint32_t lane, bool solo);
void tc_lanes_on(tc_controls *c);
void tc_pattern_step(tc_controls *c, int32_t by);
void tc_step_option(tc_controls *c, uint8_t field);
void tc_open_steps(tc_controls *c);
void tc_play_shift(tc_controls *c, tp_engine *e, uint32_t index, uint32_t now);
void tc_shut_down(tc_controls *c);
void tc_wake(tc_controls *c);
/* controls_menu.c (2.0.0): Shift + a button under the screen, on any screen; false: as without Shift. */
bool tc_shift_nav(tc_controls *c, tp_engine *e, uint32_t index, uint32_t now);
/* controls_steps.c */
ln_step tc_brush_default(uint8_t kind);
void tc_lane_setting(tc_controls *c, uint8_t item, uint8_t v);
bool tc_lane_item(uint8_t item);
void tc_step_record(tc_controls *c);
void tc_steps_nav(tc_controls *c, uint32_t index);
/* 1.8.2: the step screen's centre let go (a tap: play or stop) and, every poll, held (step record). */
void tc_steps_enter_up(tc_controls *c);
void tc_steps_hold(tc_controls *c);
/* 2.0.0: Shift + a button under the step screen: move, copy, paste or clear the lane shown. */
void tc_steps_shift(tc_controls *c, uint32_t index, uint32_t now);
/* controls_record.c */
void tc_start_recording(tc_controls *c);
void tc_finish_recording(tc_controls *c);
void tc_start_capture(tc_controls *c, uint8_t slot);
void tc_finish_capture(tc_controls *c, tp_engine *e);
void tc_capture_undo(void);
void tc_toggle_transport(tc_controls *c);
#endif
