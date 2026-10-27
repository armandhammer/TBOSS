/* SPDX-License-Identifier: GPL-3.0-or-later - what the screens' and lights' own files share (ui.c,
 * ui_play.c, ui_menu.c, ui_steps.c, ui_tests.c, ui_names.c, ui_lights.c). The rest of the firmware
 * uses ui.h only. */
#ifndef TB_UI_INTERNAL_H
#define TB_UI_INTERNAL_H
#include "ui.h"
#include "display.h"
#include "leds.h"
#include "chord_name.h"
#include "midi_in.h"
#include "sysex.h"
#include "version.h"
#include "jack.h"
#include "sequence.h"
#include <string.h>
/* What the screen shows; it is redrawn when this changes (ui.c ui_service). */
typedef struct {
    uint32_t sequence, changes, action, source, index, function;
    uint8_t ui, menu, edit, group, have_chord, last_chord, usb, test_phase, test_item, clocked, preview;
    uint8_t sustain, levels, pedal, held, first; /* levels..first only while the I/O test page is open */
    uint8_t preview_root, preview_slot;
    uint16_t preview_mask;
    uint32_t trs_sent;
    uint32_t refresh; /* pad test: redraw every 100 ms */
    uint8_t seq_entry, seq_total, seq_running, recording, seq_unit; /* progression shown on the play screen */
    uint8_t lane_step, step_held; /* the step screen (1.5.0): where the lane plays, the step held */
    uint8_t step_count, step_field; /* 1.7.0: how many steps are held, the field they edit */
    uint32_t lane_changes;
    tc_chord chord;
} ui_view;
/* ui.c: a line of text is built in ui_buffer (ui_start, then ui_put ...) and drawn with ui_line. */
#define UI_COLUMNS (TD_WIDTH / 6u) /* characters across the screen at size 1 */
extern char ui_buffer[40];
extern uint32_t ui_length;
void ui_put(const char *s);
void ui_number(uint32_t n);
void ui_start(void);
void ui_note(uint32_t midi);
void ui_line(uint32_t y);
void ui_last_action(const tc_controls *c);
void ui_pivot_name(uint8_t target, bool long_form);
/* The screens (ui_play.c, ui_menu.c, ui_steps.c, ui_tests.c) */
uint32_t ui_seq_unit(const tp_engine *e);
void ui_draw_play(const tc_controls *c, const tp_engine *e, bool usb, bool clocked);
void ui_draw_menu(const tc_controls *c);
void ui_draw_steps(const tc_controls *c, const tp_engine *e);
void ui_draw_io(const tc_controls *c, uint32_t pins, uint32_t sent, uint32_t held, uint32_t first);
void ui_pad_peaks(const tc_controls *c, const tp_engine *e, const ts_scan *s);
void ui_pad_peaks_clear(void);
void ui_draw_pads(const tc_controls *c, const tp_engine *e, const ts_scan *s);
void ui_draw_editor(uint16_t mask, uint8_t root, uint8_t slot);
/* The lights (ui_lights.c, ui_steps.c) */
tl_rgb ui_scaled(uint32_t r, uint32_t g, uint32_t b, uint32_t max);
extern const tl_rgb ui_hues[12];
bool ui_step_color(const tc_controls *c, const tp_engine *e, uint32_t k, uint32_t max, uint32_t pulse, tl_rgb *out);
void ui_pad_colors(const tc_controls *c, const tp_engine *e, uint32_t now);
bool ui_led_test(const tc_controls *c, uint32_t now, ui_view *v);
/* ui_names.c */
extern const char *const ui_roots[12], *const ui_mode_labels[12], *const ui_menu_items[TC_MENU_ITEMS];
extern const char *const ui_action_names[TC_ACTIONS], *const ui_group_names[TC_GROUPS], *const ui_group_short[TC_GROUPS];
extern const char *const ui_pedal_modes[TC_PEDAL_MODES], *const ui_voicings[TC_VOICINGS], *const ui_romans[TC_SPREADS];
extern const char *const ui_layouts[TP_MELODY_LAYOUTS], *const ui_tick_names[TC_STRUM_SPEEDS - TC_STRUM_MS];
extern const char *const ui_repeat_names[TC_NOTE_REPEATS], *const ui_cutoff_names[TC_CUTOFFS], *const ui_sus_names[3];
extern const char *const ui_pedal_types[TC_PEDAL_TYPES], *const ui_led_schemes[TC_LED_SCHEMES];
extern const char *const ui_at_names[TC_AT_MODES], *const ui_at_short[TC_AT_MODES];
extern const char *const ui_vel_names[TC_VEL_MODES], *const ui_vel_short[TC_VEL_MODES];
extern const char *const ui_strum_names[TC_STRUM_MODES], *const ui_arp_names[TC_ARP_MODES], *const ui_rate_names[TC_ARP_RATES];
extern const char *const ui_off_on[2], *const ui_harmony_names[TC_HARMONIES], *const ui_harmony_short[TC_HARMONIES];
extern const char *const ui_echo_names[TC_ECHO_TIMES], *const ui_arrow_names[TC_ARROWS_MODES];
extern const char *const ui_targets[TC_PEDAL_TARGETS], *const ui_unit_names[SQ_UNIT_KINDS], *const ui_kind_names[LN_KINDS];
extern const char *const ui_direction_names[LN_DIRECTIONS], *const ui_field_names[TC_FIELDS];
extern const char *const ui_condition_names[LN_CONDITIONS];
#endif
