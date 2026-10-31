/* SPDX-License-Identifier: GPL-3.0-or-later - the screens (ui.h): the play screen. The key and scale, the chord
 * name, the last action, the octaves (or the way to a pivot target) and a line for what plays. */
#include "ui_internal.h"
/* The unit of the progression playing now, 0 .. total - 1. */
uint32_t ui_seq_unit(const tp_engine *e) {
    const sq_prog *p = tp_seq_prog(e);
    return p->total ? tp_seq_pos(e) / sq_unit_ticks(p->unit) % p->total : 0u;
}
/* With a pivot target: the last chord's role there (ii IN G: D7>G) when it is a pivot chord, else
 * the target (PIVOT TO G MAJOR), in at most room characters: without the way in (ii IN G) or as
 * TO G MAJOR when that is all there is room for. */
static void pivot_line(const tc_controls *c, uint32_t room) {
    static const char *const numerals[7] = {"I","II","III","IV","V","VI","VII"};
    static const char *const lower[7] = {"i","ii","iii","iv","v","vi","vii"};
    static const uint8_t major[7] = {0,2,4,5,7,9,11}, minor[7] = {0,2,3,5,7,8,10};
    tp_layout layout = tc_layout(c);
    uint32_t k = c->last_chord % 48u, root = (c->pivot - 1u) % 12u;
    uint16_t classes = c->have_chord ? tc_pad_classes(layout, k, tc_pad_edit(c, layout, k)) : 0u;
    uint16_t key = tc_pivot_scale(c->pivot);
    uint32_t chord = tc_pad_root(layout, k) % 12u, d = (chord + 12u - root) % 12u, step = 7u;
    for (uint32_t i = 0; i < 7u; ++i) if ((c->pivot > 12u ? minor : major)[i] == d) step = i;
    uint32_t mark = ui_length;
    if (!classes || (classes & ~key) || step == 7u) {
        ui_put("PIVOT TO "); ui_pivot_name(c->pivot, true);
        if (ui_length > room) { ui_length = mark; ui_buffer[ui_length] = 0; ui_put("TO "); ui_pivot_name(c->pivot, true); }
        return;
    }
    bool third = ((uint32_t)classes >> ((chord + 3u) % 12u)) & 1u, major_third = ((uint32_t)classes >> ((chord + 4u) % 12u)) & 1u;
    bool flat5 = ((uint32_t)classes >> ((chord + 6u) % 12u)) & 1u, fifth = ((uint32_t)classes >> ((chord + 7u) % 12u)) & 1u;
    ui_put(third && !major_third ? lower[step] : numerals[step]);
    if (third && !major_third && flat5 && !fifth) ui_put("o");
    ui_put(" IN "); ui_pivot_name(c->pivot, false);
    mark = ui_length;
    ui_put(": "); ui_put(ui_roots[(root + 7u) % 12u]); ui_put("7>"); ui_pivot_name(c->pivot, false);
    if (ui_length > room) { ui_length = mark; ui_buffer[ui_length] = 0; }
}
/* The chord name of step i of progression p, as it plays in the current key (2.0.0: this time round: a rest, or
 * the borrowed chord). */
static uint32_t step_name(const tc_controls *c, const sq_prog *p, uint32_t i, uint8_t plays, char name[TN_MAX]) {
    if (i >= p->count) { name[0] = 0; return 0u; }
    if (plays == SQ_RESTS) { memcpy(name, "REST", 5u); return 4u; }
    const sq_entry *en = &p->entries[i];
    tp_layout l = tp_seq_layout(p, tc_layout(c), en, plays);
    tp_notes n;
    if (!tc_pad_with(l, en->pad, en->edit, &n)) { name[0] = 0; return 0u; }
    return tn_name(&n, tc_pad_root(l, en->pad), name);
}
void ui_draw_play(const tc_controls *c, const tp_engine *e, bool usb, bool clocked) {
    td_text(0, 0, ui_roots[c->tonic % 12u], 2);
    td_text(30, 0, tc_scale_name(c->scale), 1);
    uint32_t button = 12u;
    for (uint32_t i = 0; i < 12u; ++i) if (tc_mode_scale(c, i) == c->scale) { button = i; break; }
    ui_start(); if (button < 12u) { ui_put("MODE "); ui_put(ui_mode_labels[button]); }
    if (c->preset < TC_PRESETS) { ui_put(button < 12u ? "  P" : "PRESET "); ui_number(c->preset + 1u); }
    if (c->chord_set == TC_CHORDS_FULL) ui_put(ui_length ? " FULL" : "FULL CHORDS");
    else if (c->chord_set == TC_CHORDS_GRID) ui_put(ui_length ? " GRID" : "CHORD GRID");
    else if (c->chord_set) { ui_put(ui_length ? "  S" : "CHORD SET "); ui_number(c->chord_set); }
    td_text(30, 8, ui_buffer, 1);
    char name[TN_MAX];
    uint32_t n = c->have_chord ? tn_pad(tc_layout(c), c->last_chord % 48u, name) : 0u;
    if (c->melody_layout == TP_MELODY_LIVE && c->live_from_midi && c->live_midi.count) {
        /* Live notes following MIDI in: the incoming chord's name. */
        tp_notes in = {{0}, 0};
        for (uint32_t i = 0; i < c->live_midi.count; ++i) in.notes[in.count++] = (uint8_t)(60u + c->live_midi.root + c->live_midi.tones[i]);
        n = tn_name(&in, c->live_midi.root, name);
    }
    const sq_prog *playing = tp_seq_prog(e);
    if (e->seq_running && playing->count) n = step_name(c, playing, e->seq_entry, e->seq_plays, name); /* the progression's chord */
    if (n) {
        td_text(0, n <= 10u ? 18u : 22u, name, n <= 10u ? 2u : 1u);
    } else td_text(0, 22, "PLAY A CHORD", 1);
    ui_last_action(c); ui_line(36);
    /* EXT (1.7.0) whenever a MIDI clock comes in: "OCT " goes when the flags need its room; on the pivot
     * line only when it fits. */
    uint32_t flags = (c->hold ? 5u : 0u) + (c->sustain ? 4u : 0u);
    ui_start();
    if (c->pivot) pivot_line(c, UI_COLUMNS - flags); /* 1.4.0: the way there */
    else for (uint32_t form = 0; form < 2u; ++form) {
        ui_start(); if (!form) ui_put("OCT ");
        ui_note(c->chord_base + c->tonic); ui_put(" "); ui_note(c->melody_base + c->tonic);
        if (ui_length + flags + (clocked ? 4u : 0u) <= UI_COLUMNS) break;
    }
    if (c->hold) ui_put(" HOLD");
    if (c->sustain) ui_put(" SUS");
    if (clocked && ui_length + 4u <= UI_COLUMNS) ui_put(" EXT");
    ui_line(46);
    ui_start();
    if (!usb) ui_put("USB: WAITING");
    else if (c->capturing) { /* 3/12 <UNDO O SAVE (the message above says CAPTURE) */
        uint8_t pcs[12]; uint16_t tones[12];
        ui_number(tc_captured(pcs, tones)); ui_put("/12 <UNDO O SAVE");
    } else if (c->recording) { /* REC P1 12/64 Am: the last chord when it fits */
        char last[TN_MAX];
        ui_put("REC P"); ui_number(c->prog_slot + 1u); ui_put(" "); ui_number(sq.total); ui_put("/"); ui_number(SQ_UNITS_MAX);
        uint32_t n_last = sq.count ? step_name(c, &sq, sq.count - 1u, SQ_PLAYS, last) : 0u;
        if (n_last && ui_length + 1u + n_last <= UI_COLUMNS) { ui_put(" "); ui_put(last); }
    } else if (tc_shift(c)) { /* 2.0.0: what Shift + the buttons under the screen do (<PLAY ^v120 >ARP OTAP) */
        ui_put(c->transport ? "<STOP ^v" : "<PLAY ^v");
        if (c->arrows == TC_ARROWS_TEMPO) ui_put("SCL"); else ui_number(tc_tempo_bpm(c->tempo));
        ui_put(" >ARP OTAP");
    } else if (e->seq_running && playing->count) {
        /* P1 Am > F 3/8: the chord now, the next one, the unit playing of all. Names that
         * don't fit go first (the chord now is also the big name above), then the next. 2.0.0, a chain:
         * C2/4 P3 ...: its link playing of all, and that link's progression. */
        char now_name[TN_MAX], next[TN_MAX];
        (void)step_name(c, playing, e->seq_entry, e->seq_plays, now_name);
        (void)step_name(c, playing, (e->seq_entry + 1u) % playing->count, SQ_PLAYS, next);
        for (uint32_t form = 0; form < 3u; ++form) {
            ui_start();
            if (e->seq_chained) { ui_put("C"); ui_number(e->seq_link + 1u); ui_put("/"); ui_number(sq_chain.count); ui_put(" "); }
            ui_put("P"); ui_number(e->seq_slot % SQ_SLOTS + 1u); ui_put(" ");
            if (form == 0u) { ui_put(now_name); if (playing->count > 1u) { ui_put(" > "); ui_put(next); } ui_put(" "); }
            else if (form == 1u && playing->count > 1u) { ui_put("> "); ui_put(next); ui_put(" "); }
            ui_number(ui_seq_unit(e) + 1u); ui_put("/"); ui_number(playing->total);
            if (ui_length <= UI_COLUMNS) break;
        }
    } else if (c->arp) { /* ARP UP 1/16 120; without "ARP " when that doesn't fit (AS PLAYED 1/16T 200) */
        for (uint32_t form = 0; form < 2u; ++form) {
            ui_start(); if (!form) ui_put("ARP ");
            ui_put(ui_arp_names[c->arp % TC_ARP_MODES]); ui_put(" "); ui_put(ui_rate_names[c->arp_rate % TC_ARP_RATES]);
            ui_put(" "); if (clocked) ui_put("EXT"); else ui_number(tc_tempo_bpm(c->tempo));
            if (ui_length <= UI_COLUMNS) break;
        }
    } else if (c->strum) {
        ui_put("STRUM "); ui_put(ui_strum_names[c->strum % TC_STRUM_MODES]); ui_put(" ");
        if (c->strum_speed < TC_STRUM_MS) { ui_number(tc_strum_ms[c->strum_speed]); ui_put("MS"); }
        else ui_put(ui_tick_names[(c->strum_speed - TC_STRUM_MS) % (TC_STRUM_SPEEDS - TC_STRUM_MS)]);
    } else if (c->note_repeat) {
        ui_put("REPEAT "); ui_put(ui_repeat_names[c->note_repeat % TC_NOTE_REPEATS]); ui_put(" ");
        if (clocked) ui_put("EXT"); else ui_number(tc_tempo_bpm(c->tempo));
    } else if (c->harmony || c->echo || c->mono) { /* 1.6.0: HARM 3RD ECHO 1/8 MONO, or H 3RD E 1/8 MONO */
        for (uint32_t form = 0; form < 2u; ++form) {
            ui_start();
            if (c->harmony) { ui_put(form ? "H " : "HARM "); ui_put(ui_harmony_short[c->harmony % TC_HARMONIES]); }
            if (c->echo) { ui_put(ui_length ? " " : ""); ui_put(form ? "E " : "ECHO "); ui_put(ui_echo_names[c->echo % TC_ECHO_TIMES]); }
            if (c->mono) ui_put(ui_length ? " MONO" : "MONO");
            if (ui_length <= UI_COLUMNS) break;
        }
    } else {
        ui_put("V "); ui_put(ui_vel_short[c->vel_mode % TC_VEL_MODES]); ui_put("/"); ui_put(ui_vel_short[c->melody_vel % TC_VEL_MODES]);
        ui_put(" A "); ui_put(ui_at_short[c->at_chord % TC_AT_MODES]); ui_put("/"); ui_put(ui_at_short[c->at_melody % TC_AT_MODES]);
    }
    ui_line(56);
}
