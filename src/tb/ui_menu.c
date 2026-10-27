/* SPDX-License-Identifier: GPL-3.0-or-later - the screens (ui.h): Settings. The pages, a page's items with their
 * values, and a reset's confirmation. */
#include "ui_internal.h"
/* A Euclidean rhythm value (TC_RHYTHMS..) as hits/steps, e.g. 3/8. */
static void euclid_name(uint8_t v) {
    uint32_t hits = 0, steps = 0;
    (void)rh_euclid_at(v - TC_RHYTHMS, &hits, &steps);
    ui_number(hits); ui_put("/"); ui_number(steps);
}
static void value_name(uint8_t item, uint8_t v) {
    ui_start();
    switch (item) {
    case TC_MENU_SCALE: ui_put(tc_scale_name(tc_scale_at(v))); if (!tc_scale_defined(tc_scale_at(v))) ui_put(" EMPTY"); break;
    case TC_MENU_HOLD: case TC_MENU_MIDI_LEDS: case TC_MENU_TRS_OUT: case TC_MENU_ADD7: case TC_MENU_ADD9:
    case TC_MENU_ADD6: case TC_MENU_PAD_STRUM: case TC_MENU_ARP_SYNC: case TC_MENU_ARP_LATCH: case TC_MENU_LIVE:
    case TC_MENU_CLOCK_OUT: case TC_MENU_SOUND: case TC_MENU_FOLD: case TC_MENU_VOICE_LEAD: ui_put(ui_off_on[v & 1u]); break;
    case TC_MENU_NOTE_REPEAT: ui_put(ui_repeat_names[v % TC_NOTE_REPEATS]); break;
    case TC_MENU_REPEAT_RHYTHM: if (v) euclid_name((uint8_t)(TC_RHYTHMS + v - 1u)); else ui_put("ALL"); break;
    case TC_MENU_ARP_ROTATE: ui_number(v % TC_ROTATES); break;
    case TC_MENU_PIVOT: if (v && v < TC_PIVOTS) ui_pivot_name(v, true); else ui_put("OFF"); break;
    case TC_MENU_CAPTURE: ui_put("SET "); ui_number(v + 1u); if (!tc_chord_set_defined(v % TC_CHORD_SETS)) ui_put(" EMPTY"); break;
    case TC_MENU_LIVE_CH: if (v) ui_number(v); else ui_put("MELODY"); break;
    case TC_MENU_LIVE_IN: if (!v) ui_put("OFF"); else if (v > 16u) ui_put("ALL"); else { ui_put("CH "); ui_number(v); } break;
    case TC_MENU_VOICING: ui_put(ui_voicings[v % TC_VOICINGS]); break;
    case TC_MENU_SPREAD: ui_put(ui_romans[v % TC_SPREADS]); break;
    case TC_MENU_SUS: ui_put(ui_sus_names[v % 3u]); break;
    case TC_MENU_RESET_CHORD: case TC_MENU_RESET_ALL: case TC_MENU_BUTTONS_RESET: case TC_MENU_RESET_PAGE:
    case TC_MENU_FACTORY: break; /* no value in the list; the confirmation has its own screen */
    case TC_MENU_LAYOUT: ui_put(ui_layouts[v % TP_MELODY_LAYOUTS]); break;
    case TC_MENU_BASS_CH: case TC_MENU_DRUM_CH: ui_number(v + 1u); break;
    case TC_MENU_PAD_TRIGGER: ui_put("+"); ui_number(100u * (TC_PAD_TRIGGER_MIN + v)); break; /* above rest (1.7.0) */
    case TC_MENU_PAD_RELEASE: ui_put("+"); ui_number(100u * (TC_PAD_RELEASE_MIN + v)); break;
    case TC_MENU_PAD_FULL: ui_put("+"); ui_number(100u * (TC_PAD_FULL_MIN + v)); break;
    case TC_MENU_PAD_UNSTICK: ui_put(ui_off_on[v & 1u]); break;
    case TC_MENU_PROGRAM_IN: if (!v) ui_put("OFF"); else if (v > 16u) ui_put("ALL"); else { ui_put("CH "); ui_number(v); } break;
    case TC_MENU_CHORD_PROG: case TC_MENU_MELODY_PROG: if (v) ui_number(v); else ui_put("OFF"); break;
    case TC_MENU_SPREAD_OF: ui_put(v ? "CHORD" : "ROOT"); break;
    case TC_MENU_REPEAT: ui_put(ui_repeat_names[v % TC_REPEATS]); break;
    case TC_MENU_CUTOFF: ui_put(ui_cutoff_names[v % TC_CUTOFFS]); break;
    case TC_MENU_RHYTHM: {
        uint32_t steps;
        uint64_t r = tc_rhythm_mask((uint8_t)(v % TC_RHYTHM_VALUES), 0u, &steps);
        if (v >= TC_RHYTHMS) { euclid_name(v); if (steps > 8u) break; ui_put(" "); }
        for (uint32_t i = 0; i < steps; ++i) ui_put((r >> i) & 1u ? "X" : ".");
        break;
    }
    case TC_MENU_SWING: ui_number(tc_swing_pct[v % TC_SWINGS]); ui_put("%"); break;
    case TC_MENU_TRANSPORT: ui_put(v ? "RUNNING" : "STOPPED"); break;
    case TC_MENU_PEDAL_NOTE: ui_note(v); ui_put(" "); ui_number(v); break;
    case TC_MENU_PEDAL: ui_put(ui_pedal_modes[v % TC_PEDAL_MODES]); break;
    case TC_MENU_PEDAL_TYPE: ui_put(ui_pedal_types[v % TC_PEDAL_TYPES]); break;
    case TC_MENU_COLORS: ui_put(ui_led_schemes[v % TC_LED_SCHEMES]); break;
    case TC_MENU_STRUM: ui_put(ui_strum_names[v % TC_STRUM_MODES]); break;
    case TC_MENU_STRUM_SPEED:
        if (v < TC_STRUM_MS) { ui_number(tc_strum_ms[v]); ui_put(" MS"); }
        else ui_put(ui_tick_names[(v - TC_STRUM_MS) % (TC_STRUM_SPEEDS - TC_STRUM_MS)]);
        break;
    case TC_MENU_ARP: ui_put(ui_arp_names[v % TC_ARP_MODES]); break;
    case TC_MENU_ARP_RATE: ui_put(ui_rate_names[v % TC_ARP_RATES]); break;
    case TC_MENU_ARP_OCTAVES: ui_number(v + 1u); break;
    case TC_MENU_ARP_GATE: ui_number(tc_arp_gate_pct[v % TC_ARP_GATES]); ui_put("%"); break;
    case TC_MENU_TEMPO: ui_number(tc_tempo_bpm(v)); ui_put(" BPM"); break;
    case TC_MENU_CHORD_CH: case TC_MENU_MELODY_CH: ui_number(v + 1u); break;
    case TC_MENU_CHORD_AT: case TC_MENU_MELODY_AT: ui_put(ui_at_names[v % TC_AT_MODES]); break;
    case TC_MENU_VELOCITY: ui_put(ui_vel_names[v % TC_VEL_MODES]); break;
    case TC_MENU_LOAD: case TC_MENU_SAVE:
        ui_number(v + 1u); if (!(tc_presets_present & (1u << (v % TC_PRESETS)))) ui_put(" EMPTY");
        break;
    case TC_MENU_BRIGHTNESS: ui_number(v + 1u); break;
    case TC_MENU_VERSION: ui_put(TB_VERSION_LABEL); break;
    case TC_MENU_ARROWS: ui_put(ui_arrow_names[v % TC_ARROWS_MODES]); break;
    case TC_MENU_MELODY_VEL: ui_put(ui_vel_names[v % TC_VEL_MODES]); break;
    case TC_MENU_PEDAL_TARGET: ui_put(ui_targets[v % TC_PEDAL_TARGETS]); break;
    case TC_MENU_SEQ_SLOT: ui_number(v + 1u); if (!(sq_present & (1u << (v % SQ_SLOTS)))) ui_put(" EMPTY"); break;
    case TC_MENU_SEQ_UNIT: ui_put(ui_unit_names[v % SQ_UNIT_KINDS]); break;
    case TC_MENU_SEQ_GUIDE: case TC_MENU_HINTS: ui_put(ui_off_on[v & 1u]); break;
    case TC_MENU_CHORD_SET: {
        const tc_chord_set *s = v ? tc_chord_set_get((uint8_t)(v - 1u)) : 0;
        if (!v) ui_put("SCALE");
        else if (v == TC_CHORDS_FULL) ui_put("FULL");
        else if (v == TC_CHORDS_GRID) ui_put("GRID");
        else if (!tc_chord_set_defined((uint8_t)(v - 1u))) { ui_number(v); ui_put(" EMPTY"); }
        else if (s && s->name[0] && !s->name[10]) ui_put(s->name); /* names up to 10 characters fit beside the item */
        else { ui_put("SET "); ui_number(v); }
        break;
    }
    case TC_MENU_LOCK: if (v) { ui_put("CH "); ui_number(v); } else ui_put("OFF"); break;
    case TC_MENU_SEQ_CLEAR: ui_put("CLEAR"); break;
    case TC_MENU_FAMILY: ui_put(tc_family_name(v)); break;
    case TC_MENU_CAPO: if (v) { ui_put("FRET "); ui_number(v); } else ui_put("OFF"); break;
    case TC_MENU_FEEL_TIME: if (v) { ui_put("0-"); ui_number(tc_feel_ms[v % TC_FEEL_TIMES]); ui_put(" MS"); } else ui_put("OFF"); break;
    case TC_MENU_FEEL_VEL: if (v) { ui_put("+-"); ui_number(tc_feel_vel[v % TC_FEEL_VELS]); } else ui_put("OFF"); break;
    case TC_MENU_STEPS: ui_put("EDIT"); break;
    case TC_MENU_LANE: ui_put("L"); ui_number(v % LN_LANES + 1u); ui_put(" "); ui_put(ui_kind_names[ln.lanes[v % LN_LANES].kind % LN_KINDS]); break;
    case TC_MENU_LANE_KIND: ui_put(ui_kind_names[v % LN_KINDS]); break;
    case TC_MENU_LANE_LENGTH: ui_number(v + 1u); break;
    case TC_MENU_LANE_RATE: ui_put(ui_rate_names[v % TC_ARP_RATES]); break;
    case TC_MENU_LANE_CHANNEL: { /* AUTO: the kind's own channel */
        uint8_t ch; tp_notes n; ln_step s; memset(&s, 0, sizeof s);
        (void)tc_step_notes(tc_layout(&tc_state), ln.lanes[tc_state.lane % LN_LANES].kind, v, &s, &n, &ch);
        if (!v) ui_put("AUTO ");
        ui_number(ch + 1u);
        break;
    }
    case TC_MENU_LANE_MUTE: ui_put(ui_off_on[v & 1u]); break;
    case TC_MENU_CHORDS_SIDE: ui_put(v ? "RIGHT" : "LEFT"); break;
    case TC_MENU_HARMONY: ui_put(ui_harmony_names[v % TC_HARMONIES]); break;
    case TC_MENU_ECHO: ui_put(ui_echo_names[v % TC_ECHO_TIMES]); break;
    case TC_MENU_ECHOES: ui_number(v % TC_ECHOES + 1u); break;
    case TC_MENU_MONO: ui_put(ui_off_on[v & 1u]); break;
    case TC_MENU_CHANCE: ui_number(tc_chance_pct[v % TC_CHANCES]); ui_put("%"); break;
    case TC_MENU_LANE_DIRECTION: ui_put(ui_direction_names[v % LN_DIRECTIONS]); break;
    case TC_MENU_LANE_EUCLID: /* 5/16: hits of the lane's length */
        if (!v) ui_put("OFF");
        else { ui_number(v); ui_put("/"); ui_number(ln_length(&ln.lanes[tc_state.lane % LN_LANES])); }
        break;
    case TC_MENU_LANE_ROTATE: ui_number(v); break;
    case TC_MENU_MELODY_OCT: {
        uint32_t mid = TP_OCTAVE_RANGE / 12u;
        ui_put(v < mid ? "-" : v > mid ? "+" : ""); ui_number(v < mid ? mid - v : v - mid);
        break;
    }
    default:
        if (item >= TC_MENU_BUTTON1 && item < TC_MENU_BUTTON1 + TC_FUNCTIONS) ui_put(v < TC_ACTIONS ? ui_action_names[v] : "?");
        else ui_put("START");
        break;
    }
}
/* Four rows; the list scrolls so the selection stays visible. */
#define ROWS 4u
static uint32_t first_row(uint32_t selected, uint32_t count) {
    uint32_t first = selected >= ROWS - 1u ? selected - (ROWS - 2u) : 0u;
    return count <= ROWS ? 0u : first > count - ROWS ? count - ROWS : first;
}
static void position_text(uint32_t at, uint32_t count) {
    ui_start(); ui_number(at + 1u); ui_put("/"); ui_number(count);
    td_text(128u - 6u * ui_length, 0, ui_buffer, 1);
}
/* A reset's confirmation (1.4.0): what it does, then CANCEL (where it opens) and RESET, reached
 * with up. The chosen one is drawn first and inverted. */
static void draw_confirm(const tc_controls *c) {
    const char *text[3] = {"", "", ""};
    ui_start();
    switch (c->menu) {
    case TC_MENU_RESET_CHORD:
        ui_put("RESET CHORD EDITS?"); text[0] = "THE LAST CHORD PLAYED"; text[1] = "GOES BACK TO PLAIN."; break;
    case TC_MENU_RESET_ALL:
        ui_put("RESET ALL EDITS?"); text[0] = "EVERY CHORD PAD GOES"; text[1] = "BACK TO PLAIN."; break;
    case TC_MENU_BUTTONS_RESET:
        ui_put("RESET ALL BUTTONS?"); text[0] = "EVERY CENTER BUTTON"; text[1] = "BACK TO ITS PRINTED"; text[2] = "FUNCTION."; break;
    case TC_MENU_FACTORY:
        ui_put("RESET ALL OPTIONS?"); text[0] = "PRESETS, SCALES, SETS"; text[1] = "AND PROGRESSIONS STAY";
        text[2] = "ALL ELSE TO DEFAULTS"; break;
    default:
        ui_put("RESET "); ui_put(ui_group_short[c->group % TC_GROUPS]); ui_put(" OPTIONS?");
        text[1] = "THEIR DEFAULTS."; text[2] = "YOUR CHANGES ARE LOST"; break;
    }
    td_text(0, 0, ui_buffer, 1);
    if (c->menu == TC_MENU_RESET_PAGE) { ui_start(); ui_number(tc_reset_count(c->group % TC_GROUPS)); ui_put(" SETTINGS BACK TO"); td_text(0, 11, ui_buffer, 1); }
    for (uint32_t i = 0; i < 3u; ++i) if (text[i][0]) td_text(0, 11u + 10u * i, text[i], 1);
    static const char *const choice[2] = {"CANCEL", "RESET"};
    static const uint32_t at[2] = {12u, 86u};
    uint32_t chosen = c->edit_value ? 1u : 0u;
    for (uint32_t k = 0; k < 2u; ++k) {
        uint32_t i = k ? 1u - chosen : chosen;
        uint32_t end = td_text(at[i], 43, choice[i], 1);
        if (!k) td_invert(at[i] - 2u, 42, end + 1u, 51);
    }
    td_text(0, 56, chosen ? "O RESETS   <CANCEL" : "UP THEN O TO RESET", 1);
}
void ui_draw_menu(const tc_controls *c) {
    uint8_t group = c->group % TC_GROUPS;
    if (c->ui == TC_UI_EDIT && tc_confirm_item(c->menu)) { draw_confirm(c); return; }
    if (c->ui == TC_UI_GROUPS) {
        td_text(0, 0, "SETTINGS", 1);
        position_text(group, TC_GROUPS);
        uint32_t first = first_row(group, TC_GROUPS);
        for (uint32_t row = 0; row < ROWS && first + row < TC_GROUPS; ++row) {
            uint32_t i = first + row, y = 11u + 10u * row;
            td_text(0, y, i == group ? ">" : " ", 1);
            td_text(8, y, ui_group_names[i], 1);
            td_text(122, y, ">", 1);
        }
    } else {
        uint8_t items[TC_GROUP_MAX];
        uint32_t count = tc_page_items(c, group, items), at = 0;
        for (uint32_t i = 0; i < count; ++i) if (items[i] == c->menu) at = i;
        td_text(0, 0, ui_group_names[group], 1);
        position_text(at, count);
        uint32_t first = first_row(at, count);
        for (uint32_t row = 0; row < ROWS && first + row < count; ++row) {
            uint8_t item = items[first + row];
            uint32_t y = 11u + 10u * row;
            bool editing = item == c->menu && c->ui == TC_UI_EDIT;
            td_text(0, y, item == c->menu ? ">" : " ", 1);
            td_text(8, y, ui_menu_items[item], 1);
            value_name(item, editing ? c->edit_value : tc_menu_value(c, item));
            uint32_t x = 128u - 6u * ui_length;
            td_text(x, y, ui_buffer, 1);
            if (editing) td_invert(x - 1u, y - 1u, 128u, y + 8u);
        }
    }
    /* Bottom line: what the buttons do here, or what the last action did. */
    switch (c->last_action) {
    case TC_NONE: case TC_PREVIOUS: case TC_NEXT: case TC_MENU: case TC_COMMIT: case TC_CANCEL:
        ui_start();
        if (c->ui == TC_UI_EDIT) ui_put("<CANCEL   O SET");
        else if (c->ui == TC_UI_MENU && c->menu == TC_MENU_VERSION) { ui_put("<BACK   "); ui_put(TB_VERSION_TAG); } /* the installer finds this tag */
        else if (c->ui == TC_UI_MENU && (c->menu == TC_MENU_LED_TEST || c->menu == TC_MENU_IO_TEST ||
                                         c->menu == TC_MENU_PAD_TEST)) ui_put("<BACK   O START");
        else if (c->ui == TC_UI_MENU && tc_confirm_item(c->menu)) ui_put("<BACK   O RESET");
        else if (c->ui == TC_UI_MENU && c->menu == TC_MENU_SEQ_RECORD) ui_put("<BACK   O RECORD");
        else if (c->ui == TC_UI_MENU && c->menu == TC_MENU_SEQ_CLEAR) ui_put("<BACK   O CLEAR");
        else if (c->ui == TC_UI_MENU && c->menu == TC_MENU_TRANSPORT) ui_put(c->transport ? "<BACK   O STOP" : "<BACK   O PLAY");
        else if (c->ui == TC_UI_MENU && c->menu == TC_MENU_STEPS) ui_put("<BACK   O OPEN");
        else if (c->ui == TC_UI_MENU) ui_put("<BACK   O CHANGE");
        else ui_put("<BACK   O OPEN");
        break;
    case TC_TODO_CONTEXT: ui_start(); ui_put("< TO LEAVE THE MENU"); break;
    default: ui_last_action(c); break;
    }
    td_text(0, 56, ui_buffer, 1);
}
