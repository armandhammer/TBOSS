/* SPDX-License-Identifier: GPL-3.0-or-later - OLED screens and LED colors.
 * Play screen: root, scale, mode button or preset, the chord name of the last chord
 * pad, the last action (every centre button names its function, so the board
 * documents itself), octaves/hold, then arpeggiator, strum or velocity/pressure.
 * Settings: a list of pages, each a scrolling list of items with values, and a bottom
 * line with the buttons' meaning or the result of the last action.
 * Pads (Default, rainbow by note): each pad the color of its note's pitch class, root columns
 * brightest; a touched pad white, brighter with pressure; sounding (held, latched, arp
 * input) softer white; edited chords paler; notes arriving over MIDI white.
 * Pads (root+5th): root orange, fifth blue, other steps dim, edited chords purple,
 * sounding pads white, MIDI in green.
 * Pads (Rainbow, 1.5.0): seven colors across both halves, the same on every row: red, orange,
 * yellow, green, dark blue, light blue, magenta, then red to dark blue again (as its requester
 * asked; the left column brightest) whatever the key, scale or layout plays; touched and sounding pads white,
 * no hints, guide or MIDI in lights. The step screen and the editor preview keep their own.
 * Web editor preview (either scheme): both halves become 12 semitone columns from the
 * editor's root: root column white, scale notes in their rainbow color at full
 * brightness, other notes nearly dark; the display names the slot and the notes.
 * The screens' files: ui.c (what they share, the last action's message), ui_play.c, ui_menu.c,
 * ui_steps.c, ui_tests.c, ui_names.c (every name shown) and ui_lights.c (the pads' and buttons'
 * lights); ui_internal.h joins them. */
#include "ui_internal.h"
static ui_view shown;
static bool forced = true;
char ui_buffer[40];
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
void ui_line(uint32_t y) { td_text(0, y, ui_buffer, 1); }
static void button_text(const tc_controls *c);
static void captured_name(uint32_t i);
void ui_last_action(const tc_controls *c) {
    ui_start();
    switch (c->last_action) {
    case TC_PANEL:
        if (c->last_function <= TC_FN_GUITAR) {
            if (c->chord_voicing == TC_VOICING_GUITAR) ui_put("GUITAR VOICING");
            else { ui_put("VOICING "); ui_put(ui_voicings[c->chord_voicing % TC_VOICINGS]); }
        } else if (c->last_function <= TC_FN_SPREAD3) {
            ui_put("SPREAD "); ui_put(ui_romans[c->chord_spread % TC_SPREADS]);
            if (c->chord_spread && c->spread_chord) ui_put(" CHORD"); /* 1.7.0: Spread of Chord */
        }
        else {
            ui_put("OCTAVES");
            for (uint32_t o = 0; o < 7u; ++o) if ((c->chord_octaves >> o) & 1u) {
                ui_put(o < 3u ? " -" : o > 3u ? " +" : " "); ui_number(o < 3u ? 3u - o : o - 3u);
            }
        }
        break;
    case TC_TRANSPORT: ui_put(c->transport ? "PLAY" : "STOP"); break;
    case TC_RESET: ui_put("CHORD EDITS RESET"); break;
    case TC_SCALE_STEP: ui_put("SCALE "); ui_put(tc_scale_name(c->scale)); break;
    case TC_TEMPO_STEP: ui_put("TEMPO "); ui_number(tc_tempo_bpm(c->tempo)); ui_put(" BPM"); break;
    case TC_FAVOURITE_STEP: {
        uint32_t at = 0;
        while (at < tc_favourite_count && tc_favourites[at] != c->scale) at++;
        ui_put("FAV "); ui_number(at + 1u); ui_put("/"); ui_number(tc_favourite_count); ui_put(" "); ui_put(tc_scale_name(c->scale));
        break;
    }
    case TC_CHORD_EDIT: ui_put("CHORD EDITED"); break;
    case TC_OCTAVE: ui_put("MELODY OCT "); ui_note(c->melody_base + c->tonic); break;
    case TC_TONIC: ui_put("ROOT "); ui_put(ui_roots[c->tonic % 12u]); break;
    case TC_MODE: ui_put("MODE "); ui_put(ui_mode_labels[c->last_index % 12u]); ui_put(" "); ui_put(tc_scale_name(c->scale)); break;
    case TC_TODO_FUNCTION:
        if (c->last_source == TC_SCALE) { ui_put(ui_mode_labels[c->last_index % 12u]); ui_put(" IS EMPTY"); }
        else ui_put("PLAY A CHORD 1ST");
        break;
    case TC_TODO_CONTEXT: ui_put("CLOSE THE MENU FIRST"); break;
    case TC_REJECT: ui_put("OUT OF RANGE"); break;
    case TC_CONFLICT: ui_put("ONE BUTTON AT A TIME"); break;
    case TC_SETTING: ui_put("SETTING CHANGED"); break;
    case TC_HOLD: ui_put(c->hold ? "HOLD ON" : "HOLD OFF"); break;
    case TC_ARP: ui_put("ARP "); ui_put(ui_arp_names[c->arp % TC_ARP_MODES]); break;
    case TC_PRESET_LOADED: ui_put("PRESET "); ui_number(c->preset + 1u); ui_put(" LOADED"); break;
    case TC_PRESET_SAVED: ui_put("PRESET "); ui_number(c->preset + 1u); ui_put(" SAVED"); break;
    case TC_PRESET_EMPTY: ui_put("NO PRESET THERE"); break;
    case TC_SEQ_RECORD: ui_put("REC: TAP CHORD PADS"); break;
    case TC_SEQ_TAP: case TC_SEQ_UNDO: ui_put("REC "); ui_number(sq.total); ui_put("/"); ui_number(SQ_UNITS_MAX);
        if (c->last_action == TC_SEQ_UNDO) ui_put(" UNDO");
        break;
    case TC_SEQ_FULL: ui_put("PROGRESSION FULL"); break;
    case TC_SEQ_SAVED: ui_put("PROGRESSION "); ui_number(c->prog_slot + 1u); ui_put(" SAVED"); break;
    case TC_SEQ_CLEARED: ui_put("PROGRESSION "); ui_number(c->prog_slot + 1u); ui_put(" CLEARED"); break;
    case TC_SEQ_EMPTY: ui_put("NO PROGRESSION YET"); break;
    case TC_SEQ_KEPT: ui_put("NOTHING RECORDED"); break;
    case TC_SAVE_FAILED: ui_put("NOT SAVED: RESTART"); break;
    case TC_REMOTE: ui_put("CHANGED BY EDITOR"); break;
    case TC_PAD_CHOSEN: ui_put("CHOSEN FOR EDITING"); break;
    case TC_SET_NO_EDITS: ui_put(c->chord_set == TC_CHORDS_FULL ? "FULL CHORDS: NO EDITS" :
                              c->chord_set == TC_CHORDS_GRID ? "CHORD GRID: NO EDITS" : "SET CHORDS: NO EDITS"); break;
    case TC_SET_EMPTY: ui_put("THAT SET IS EMPTY"); break;
    case TC_SHIFT_UNUSED: ui_put("SHIFT: BUTTONS 1-8"); break;
    case TC_SEQ_BUSY: ui_put("NOT WHILE RECORDING"); break;
    case TC_PIVOT_SET: ui_put("PIVOT TO "); ui_pivot_name(c->pivot, true); break;
    case TC_CAPTURE_START: ui_put("CAPTURE: PLAY CHORDS"); break;
    case TC_CAPTURE_ADDED: case TC_CAPTURE_UNDO: {
        uint8_t pcs[12]; uint16_t tones[12];
        uint32_t n = tc_captured(pcs, tones);
        ui_put("CAPTURE "); ui_number(n); ui_put("/12");
        if (c->last_action == TC_CAPTURE_UNDO) ui_put(" UNDO");
        else if (n) { ui_put(" "); captured_name(n - 1u); }
        break;
    }
    case TC_CAPTURE_FULL: ui_put("12 CHORDS: O SAVES"); break;
    case TC_CAPTURE_SAVED: ui_put("SET "); ui_number(c->capture_slot + 1u); ui_put(" IN "); ui_pivot_name(c->capture_key, true); break;
    case TC_CAPTURE_EMPTY: ui_put("NOTHING CAPTURED"); break;
    case TC_PIVOT_ARRIVED: ui_put("NOW IN "); ui_put(ui_roots[c->tonic % 12u]); ui_put(" "); ui_put(tc_scale_name(c->scale)); break;
    case TC_OPTIONS_RESET: ui_put(ui_group_short[c->group % TC_GROUPS]); ui_put(" OPTIONS RESET"); break;
    case TC_FACTORY_RESET: ui_put("ALL OPTIONS RESET"); break;
    case TC_RESET_KEPT: ui_put("NOTHING WAS RESET"); break;
    /* 1.5.0: the step screen */
    case TC_STEP_ON: case TC_STEP_OFF: case TC_STEP_SET: case TC_STEP_FILLED:
        ui_put(c->last_action == TC_STEP_FILLED ? "REC STEP " : "STEP "); ui_number(c->last_index + 1u);
        ui_put(c->last_action == TC_STEP_ON ? " ON" : c->last_action == TC_STEP_OFF ? " OFF" :
            c->last_action == TC_STEP_SET ? " SET" : "");
        break;
    case TC_STEP_PICKED: ui_put("TAP A STEP FOR IT"); break;
    case TC_STEP_RECORD: ui_put("REC: PLAY THE PADS"); break;
    case TC_STEP_REST: ui_put("REST"); break;
    case TC_STEP_BACK: ui_put("TAKEN BACK"); break;
    case TC_STEP_STOPPED: ui_put("STEP RECORD OFF"); break;
    case TC_STEP_LANE: ui_put("LANE "); ui_number(c->lane % LN_LANES + 1u); break;
    case TC_STEP_PAST: ui_put("PAST THE LENGTH"); break;
    case TC_STEP_FIELD: break; /* the step screen shows the field instead */
    case TC_EUCLID_FILLED: ui_put("LANE FILLED"); break;
    case TC_PADS_RESCANNED: ui_put("PADS RESCANNED"); break;
    case TC_BUTTON: button_text(c); break;
    default: ui_put("READY"); break;
    }
}
/* A pivot target (TC_PIVOTS): G MAJOR, G MINOR (long) or G, Gm. */
void ui_pivot_name(uint8_t target, bool long_form) {
    ui_put(ui_roots[(target - 1u) % 12u]);
    if (long_form) ui_put(target > 12u ? " MINOR" : " MAJOR"); else if (target > 12u) ui_put("m");
}
/* Captured chord i's name, in the octave above middle C. */
static void captured_name(uint32_t i) {
    uint8_t pcs[12]; uint16_t tones[12];
    if (i >= tc_captured(pcs, tones)) return;
    tp_notes n = {{0}, 0};
    for (uint32_t b = 0; b < 12u; ++b) if (((uint32_t)tones[i] >> b) & 1u) n.notes[n.count++] = (uint8_t)(60u + pcs[i] + b);
    char name[TN_MAX];
    if (ui_length + tn_name(&n, pcs[i], name) <= UI_COLUMNS) ui_put(name);
}
static void chord_set_text(uint8_t set) {
    if (!set) { ui_put("SCALE CHORDS"); return; }
    if (set == TC_CHORDS_FULL) { ui_put("FULL CHORDS"); return; }
    if (set == TC_CHORDS_GRID) { ui_put("CHORD GRID"); return; }
    const tc_chord_set *s = tc_chord_set_get((uint8_t)(set - 1u));
    ui_put("SET "); ui_number(set);
    if (s && s->name[0]) { ui_put(" "); ui_put(s->name); }
}
/* What a centre button just did (TC_BUTTON). */
static void button_text(const tc_controls *c) {
    switch (c->last_button) {
    case TC_ACT_SHIFT: ui_put(c->shift_latched ? "SHIFT LATCHED" : "SHIFT (TAP 2X: LATCH)"); break;
    case TC_ACT_STRUM: ui_put(c->strum ? "STRUM ON" : "STRUM OFF"); break;
    case TC_ACT_VOICE_LEAD: ui_put(c->voice_lead ? "VOICE LEAD ON" : "VOICE LEAD OFF"); break;
    case TC_ACT_FOLD: ui_put(c->fold ? "FOLD ON" : "FOLD OFF"); break;
    case TC_ACT_HINTS: ui_put(c->hints ? "HINTS ON" : "HINTS OFF"); break;
    case TC_ACT_CHORD_SET: chord_set_text(c->chord_set); break;
    case TC_ACT_PIVOT: if (c->pivot) { ui_put("PIVOT TO "); ui_pivot_name(c->pivot, true); } else ui_put("PIVOT OFF"); break;
    case TC_ACT_PANIC: ui_put("ALL NOTES OFF"); break;
    case TC_ACT_SUSTAIN: ui_put(c->button_sustain ? "SUSTAIN ON" : "SUSTAIN OFF"); break;
    case TC_ACT_ADD7: case TC_ACT_ADD9: case TC_ACT_ADD6: case TC_ACT_SUS2: case TC_ACT_SUS4:
        ui_put(ui_action_names[c->last_button]); ui_put(" WHILE HELD"); break;
    case TC_ACTIONS: ui_put("BUTTONS RESET"); break;
    default: ui_put(c->last_button < TC_ACTIONS ? ui_action_names[c->last_button] : "?"); break;
    }
}
bool ui_init(void) {
    bool display = td_init();
    (void)tl_init();
    td_text(0, 0, "TBOSS", 2);
    td_text(0, 24, "VERSION " TB_VERSION_LABEL, 1);
    td_text(0, 40, "STARTING...", 1);
    return display;
}
void ui_service(tc_controls *c, const tp_engine *e, const ts_scan *s, bool usb, uint32_t now) {
    ui_view v;
    memset(&v, 0, sizeof v); /* padding too: the redraw test compares bytes */
    v.sequence = c->event_sequence; v.changes = c->changes; v.action = c->last_action;
    v.source = c->last_source; v.index = c->last_index; v.function = c->last_function;
    v.ui = c->ui; v.menu = c->menu; v.edit = c->edit_value; v.group = c->group; v.have_chord = c->have_chord;
    v.last_chord = c->last_chord; v.usb = usb; v.chord = c->chord[c->last_chord % 48u];
    v.clocked = tm_clock_active(now); v.preview_mask = sx_preview(now, &v.preview_root);
    v.preview = v.preview_mask != 0u; v.preview_slot = v.preview ? sx_preview_slot() : 0xFFu; v.sustain = c->sustain;
    bool io = c->ui == TC_UI_EDIT && c->menu == TC_MENU_IO_TEST;
    if (io) {
        v.levels = (uint8_t)jh_levels(); v.pedal = c->pedal_pressed; v.trs_sent = sm_out.sent;
        for (uint32_t k = TL_PADS; k-- > 0u;) /* the pad where it is on the board */
            if (e->keys[tc_physical(c, k)].sensed || e->keys[tc_physical(c, k)].inhibited) { v.held++; v.first = (uint8_t)k; }
    }
    v.seq_entry = e->seq_entry; v.seq_total = sq.total; v.seq_running = e->seq_running; v.recording = c->recording;
    v.seq_unit = (uint8_t)ui_seq_unit(e);
    if (c->ui == TC_UI_STEPS) {
        v.lane_step = e->lane_step[c->lane % LN_LANES]; v.step_held = c->step_held; v.lane_changes = ln_changes;
        v.step_count = (uint8_t)__builtin_popcountll(c->step_mask); v.step_field = c->step_field;
    }
    bool pads = c->ui == TC_UI_EDIT && c->menu == TC_MENU_PAD_TEST;
    if (pads) { ui_pad_peaks(c, e, s); v.refresh = now / 100u; } /* 10 redraws a second */
    else ui_pad_peaks_clear();
    if (c->led_test && !ui_led_test(c, now, &v)) { /* done */
        c->led_test = false; forced = true;
        for (uint32_t ch = 0; ch < TL_BUTTONS; ++ch) tl_set_button(ch, 0u);
    }
    if (!c->led_test) ui_pad_colors(c, e, now);
    if (forced || memcmp(&v, &shown, sizeof v)) {
        forced = false; shown = v;
        td_clear();
        if (c->led_test) {
            static const char *const colors[3] = {"ALL RED","ALL GREEN","ALL BLUE"};
            td_text(0, 0, "LED TEST", 2);
            ui_start();
            if (v.test_phase == 1u) ui_put(colors[v.test_item % 3u]);
            else { ui_put(v.test_phase == 2u ? "PAD LED " : "BUTTON "); ui_number(v.test_item); } /* fits at size 2 */
            td_text(0, 28, ui_buffer, v.test_phase == 1u ? 1u : 2u);
        } else if (v.preview) ui_draw_editor(v.preview_mask, v.preview_root, v.preview_slot);
        else if (io) ui_draw_io(c, v.levels, v.trs_sent, v.held, v.first);
        else if (pads) ui_draw_pads(c, e, s);
        else if (c->ui == TC_UI_PLAY) ui_draw_play(c, e, usb, v.clocked);
        else if (c->ui == TC_UI_STEPS) ui_draw_steps(c, e);
        else ui_draw_menu(c);
    }
    td_service();
    tl_service();
}
