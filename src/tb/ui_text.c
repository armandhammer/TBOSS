/* SPDX-License-Identifier: GPL-3.0-or-later - the screens (ui.h): a line of text built in ui_buffer, and
 * every setting's value as the menu shows it (1.9.0: here, apart from the drawing, so that sysex.c can give
 * the editor the same names without the display). */
#include "ui_internal.h"
char ui_buffer[UI_TEXT_MAX + 1u];
uint32_t ui_length;
void ui_put(const char *s) { while (*s && ui_length < sizeof ui_buffer - 1u) ui_buffer[ui_length++] = *s++; ui_buffer[ui_length] = 0; }
void ui_number(uint32_t n) {
    char digits[11]; uint32_t i = 0;
    do { digits[i++] = (char)('0' + n % 10u); n /= 10u; } while (n && i < sizeof digits);
    while (i) { char d[2] = {digits[--i], 0}; ui_put(d); }
}
void ui_start(void) { ui_length = 0; ui_buffer[0] = 0; }
void ui_note(uint32_t midi) {
    ui_put(ui_roots[midi % 12u]);
    if (midi < 12u) ui_put("-1"); else ui_number(midi / 12u - 1u);
}
/* A pivot target (TC_PIVOTS): G MAJOR, G MINOR (long) or G, Gm. */
void ui_pivot_name(uint8_t target, bool long_form) {
    ui_put(ui_roots[(target - 1u) % 12u]);
    if (long_form) ui_put(target > 12u ? " MINOR" : " MAJOR"); else if (target > 12u) ui_put("m");
}
/* A Euclidean rhythm value (TC_RHYTHMS..) as hits/steps, e.g. 3/8. */
static void euclid_name(uint8_t v) {
    uint32_t hits = 0, steps = 0;
    (void)rh_euclid_at(v - TC_RHYTHMS, &hits, &steps);
    ui_number(hits); ui_put("/"); ui_number(steps);
}
void ui_value_name(uint8_t item, uint8_t v) {
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
    case TC_MENU_PAD_VEL_FULL: if (v) { ui_put("+"); ui_number(100u * (TC_PAD_VEL_FULL_MIN + v - 1u)); } else ui_put("= FULL"); break;
    case TC_MENU_PAD_UNSTICK: ui_put(ui_off_on[v & 1u]); break;
    case TC_MENU_PROGRAM_IN: if (!v) ui_put("OFF"); else if (v > 16u) ui_put("ALL"); else { ui_put("CH "); ui_number(v); } break;
    case TC_MENU_CHORD_PROG: case TC_MENU_MELODY_PROG: if (v) ui_number(v); else ui_put("OFF"); break;
    case TC_MENU_SPREAD_OF: ui_put(v == TC_SPREAD_OF_BASS ? "BASS" : v ? "CHORD" : "ROOT"); break; /* Bass 2.0.2 */
    case TC_MENU_MPE: ui_put(ui_mpe_names[v % TC_MPE_MODES]); break; /* 1.8.0 */
    case TC_MENU_GLIDE: ui_put(ui_glide_names[v % TC_GLIDE_MODES]); break; /* 1.8.5 */
    case TC_MENU_GLIDE_TIME: ui_number(tc_glide_ms[v % TC_GLIDE_TIMES]); ui_put(" MS"); break;
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
    case TC_MENU_SEQ_PLAY: ui_put(v ? "PLAYING" : "STOPPED"); break; /* 1.8.2 */
    case TC_MENU_PEDAL_NOTE: ui_note(v); ui_put(" "); ui_number(v); break;
    case TC_MENU_PEDAL: ui_put(ui_pedal_modes[v % TC_PEDAL_MODES]); break;
    case TC_MENU_PEDAL_TYPE: ui_put(ui_pedal_types[v % TC_PEDAL_TYPES]); break;
    case TC_MENU_COLORS: ui_put(ui_led_schemes[v % TC_LED_SCHEMES]); break;
    case TC_MENU_COLOR_BY: ui_put(ui_color_bys[v % TC_COLOR_BYS]); break; /* 1.9.0 */
    case TC_MENU_KEEP_LOOK: case TC_MENU_KEEP_FEEL: case TC_MENU_KEEP_CHORDS: case TC_MENU_CHAIN: ui_put(ui_off_on[v & 1u]); break;
    case TC_MENU_ARP_LOCK: ui_number(10u * (v % TC_ARP_LOCKS)); ui_put("%"); break; /* 2.0.0 */
    case TC_MENU_ARP_LOOP: ui_number(TC_ARP_LOOP_MIN + v % TC_ARP_LOOPS); ui_put(" STEPS"); break;
    case TC_MENU_U3: ui_put(v ? "SHIFT" : "U3 SCALE"); break;
    case TC_MENU_SLEEP: if (v % TC_SLEEPS) { ui_number(tc_sleep_minutes[v % TC_SLEEPS]); ui_put(" MIN"); } else ui_put("OFF"); break;
    case TC_MENU_STEP_PADS: ui_put(v ? "SILENT" : "SOUND"); break;
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
    case TC_MENU_LOAD: case TC_MENU_SAVE: case TC_MENU_CLEAR_PRESET:
        ui_number(v + 1u); if (!(tc_presets_present & (1u << (v % TC_PRESETS)))) ui_put(" EMPTY");
        else if (tc_preset_names[v % TC_PRESETS][0]) { ui_put(" "); ui_put(tc_preset_names[v % TC_PRESETS]); } /* 1.9.3 */
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
    case TC_MENU_LANE_SWING: if (v) { ui_number(v * TC_SWING_STEP); ui_put("%"); } else ui_put("OFF"); break; /* 2.0.3 */
    case TC_MENU_LANE_TRANSPOSE: { /* chord lanes in scale steps, the others in semitones */
        int32_t t = (int32_t)v - TC_TRANSPOSE_MENU;
        if (!t) { ui_put("0"); break; }
        ui_put(t < 0 ? "-" : "+"); ui_number((uint32_t)(t < 0 ? -t : t));
        ui_put(ln.lanes[tc_state.lane % LN_LANES].kind == LN_CHORDS ? " STEPS" : " SEMIS");
        break;
    }
    case TC_MENU_LANE_MUTATE: ui_number(10u * (v % TC_MUTATES + 1u)); ui_put("% OF STEPS"); break; /* 2.0.5: an action */
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
const char *ui_page_name(uint8_t group) { return ui_group_names[group % TC_GROUPS]; }
const char *ui_item_name(uint8_t item) { return item < TC_MENU_ITEMS ? ui_menu_items[item] : ""; }
const char *ui_value_text(uint8_t item, uint8_t value) { ui_value_name(item, value); return ui_buffer; }
