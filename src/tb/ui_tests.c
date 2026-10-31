/* SPDX-License-Identifier: GPL-3.0-or-later - the screens (ui.h): the test pages (I/O test, pad test) and the web
 * editor's preview. */
#include "ui_internal.h"
/* I/O test: the pedal input and two alternatives, live, the test note on the TRS jack,
 * the buttons held (2.0.0: to see which a touch chip reports together) and the pads that read as pressed
 * (held, or stuck: the engine keeps those silent). */
void ui_draw_io(const tc_controls *c, uint32_t pins, uint32_t sent, uint32_t held, uint32_t first) {
    static const char *const level[2] = {"LOW","HIGH"};
    td_text(0, 0, "I/O TEST", 1);
    ui_start(); ui_put("PEDAL PA10 "); ui_put(level[pins & 1u]); ui_put(c->pedal_pressed ? " DOWN" : " UP"); ui_line(10);
    ui_start(); ui_put("PD13 "); ui_put(level[(pins >> 1) & 1u]); ui_put("  PD5 "); ui_put(level[(pins >> 2) & 1u]); ui_line(19);
    ui_start(); ui_put("TRS: C4/S, SENT "); ui_number(sent); ui_line(28);
    static const char *const nav[5] = {"<", "^", "O", "v", ">"};
    ui_start(); ui_put("BTN");
    for (uint32_t client = 0; client < BI_CLIENTS; ++client)
        for (uint8_t bit = 0; bit < 16u; ++bit) {
            uint32_t source, index, logical, aliases;
            if (!((c->debounce[client].stable >> bit) & 1u) ||
                !tc_decode((uint8_t)c->sampler.records[client].address, bit, &source, &index, &logical, &aliases)) continue;
            ui_put(" ");
            if (source == TC_NAV) ui_put(nav[index % 5u]);
            else if (source == TC_FUNCTION) { ui_put("F"); ui_number(index + 1u); }
            else if (source == TC_ROOT) ui_put(ui_roots[index % 12u]);
            else ui_put(ui_mode_labels[index % 12u]);
        }
    if (ui_length > UI_COLUMNS) { ui_length = UI_COLUMNS; ui_buffer[ui_length] = 0; }
    ui_line(37);
    ui_start(); ui_put("PADS HELD "); ui_number(held);
    tp_position p;
    if (held && tp_position_of(first, &p)) {
        ui_put(p.side ? " R C" : " L C"); ui_number(p.column + 1u); ui_put(" R"); ui_number(p.row + 1u);
    }
    ui_line(46);
    td_text(0, 56, "<BACK", 1);
}
/* Pad test: the raw pad readings (0..4095, higher = harder) against the press and release
 * thresholds, so a player can see how hard a light touch reads and how much a press bleeds
 * onto the neighbouring pads. The peak is taken on every pass, the screen redraws 10x a second. */
static uint32_t pad_peak, pad_peak_pad;
static bool pad_touch;
/* The thresholds of the pad at physical position k as it plays (1.5.0: raised for its own resting
 * level), the settings while no session runs. */
static tk_config pad_config(const tc_controls *c, const tp_engine *e, uint32_t k) {
    return e->state == TK_ACTIVE ? tp_key_config(e, tc_physical(c, k)) : tc_pad_config(c);
}
void ui_pad_peaks(const tc_controls *c, const tp_engine *e, const ts_scan *s) {
    uint8_t top;
    ts_strongest(s->values, &top, 1u);
    const tk_config d = pad_config(c, e, top);
    uint32_t v = s->values[top];
    if (!pad_touch && v > d.release) { pad_touch = true; pad_peak = 0u; } /* a new touch */
    if (pad_touch && v > pad_peak) { pad_peak = v; pad_peak_pad = top; }
    if (v <= d.release) pad_touch = false; /* the peak stays shown until the next touch */
}
static void pad_name(uint32_t k) {
    tp_position p;
    if (!tp_position_of(k, &p)) return;
    ui_put(p.side ? "R C" : "L C"); ui_number(p.column + 1u); ui_put(" R"); ui_number(p.row + 1u);
}
static void pad_value(uint32_t v, uint32_t at) { /* right-aligned in 4 digits from character `at` */
    while (ui_length < at) ui_put(" ");
    for (uint32_t d = 1000u; d > 1u && v < d; d /= 10u) ui_put(" ");
    ui_number(v);
}
void ui_draw_pads(const tc_controls *c, const tp_engine *e, const ts_scan *s) {
    uint8_t top[3];
    ts_strongest(s->values, top, 3u);
    /* Trigger / release / full as the strongest pad plays them (1.7.0: the settings above its resting
     * level); each row: a pad, its reading, * while it plays, its rest. */
    const tk_config d = pad_config(c, e, top[0]);
    ui_start(); ui_put("PAD "); ui_number(d.press); ui_put("/"); ui_number(d.release); ui_put("/"); ui_number(d.full_scale);
    ui_line(0);
    for (uint32_t i = 0; i < 3u; ++i) {
        ui_start(); pad_name(top[i]); pad_value(s->values[top[i]], 9u);
        ui_put(s->values[top[i]] >= pad_config(c, e, top[i]).press ? " *" : "  ");
        if (e->state == TK_ACTIVE) pad_value(e->rest[top[i]], 16u);
        ui_line(11u + 9u * i);
    }
    ui_start();
    if (pad_peak) { ui_put("PEAK"); pad_value(pad_peak, 5u); ui_put(" "); pad_name(pad_peak_pad); }
    else ui_put("PEAK -");
    ui_line(38);
    ui_start(); ui_put("LAST VEL ");
    uint32_t newest = TS_KEYS;
    for (uint32_t k = 0; k < TS_KEYS; ++k) if (e->admissions && e->keys[k].order == e->admissions) newest = k;
    if (newest < TS_KEYS) { ui_number(e->keys[newest].velocity); ui_put(" "); pad_name(tc_physical(c, newest)); } else ui_put("-");
    ui_line(47);
    uint32_t lo, hi;
    ui_start(); ui_put("<BACK  REST ");
    if (e->state == TK_ACTIVE && tp_rest_range(e, &lo, &hi)) { ui_number(lo); ui_put("-"); ui_number(hi); } else ui_put("-");
    ui_line(56);
}
/* Web editor preview: which slot, the root, the notes. */
void ui_draw_editor(uint16_t mask, uint8_t root, uint8_t slot) {
    td_text(0, 0, "WEB EDITOR", 1);
    if (slot < TC_USER_SCALES) { ui_start(); ui_put("U"); ui_number(slot + 1u); td_text(128u - 12u * ui_length, 0, ui_buffer, 2); }
    uint32_t count = 0;
    for (uint32_t i = 0; i < 12u; ++i) count += ((uint32_t)mask >> i) & 1u;
    ui_start(); ui_put("ROOT "); ui_put(ui_roots[root % 12u]); ui_put("  "); ui_number(count); ui_put(" NOTES"); ui_line(18);
    /* The notes, six per line: up to 12 names of up to 2 letters. */
    uint32_t listed = 0, y = 30;
    ui_start();
    for (uint32_t i = 0; i < 12u; ++i) {
        if (!(((uint32_t)mask >> i) & 1u)) continue;
        if (listed && listed % 6u == 0u) { ui_line(y); y += 10u; ui_start(); }
        ui_put(ui_roots[(root + i) % 12u]); ui_put(" "); listed++;
    }
    ui_line(y);
    td_text(0, 56, "PADS SHOW THE SCALE", 1);
}
void ui_pad_peaks_clear(void) { pad_peak = 0u; pad_touch = false; }
