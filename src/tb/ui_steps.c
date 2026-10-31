/* SPDX-License-Identifier: GPL-3.0-or-later - the screens (ui.h): the step screen (1.5.0) and its pads' lights.
 */
#include "ui_internal.h"
/* The step screen (1.5.0): the lane (L1 CHORDS 16 1/16), the page and channel, the step held or the
 * value picked, the last action, where the lane is playing, and what the buttons do. */
static void step_value(const tc_controls *c, const ln_lane *l, const ln_step *s) {
    tp_notes n; uint8_t ch;
    tp_layout layout = tc_layout(c);
    if (!tc_step_notes(layout, l->kind, l->channel, s, &n, &ch)) { ui_put("-"); return; }
    if (l->kind == LN_CHORDS) {
        char name[TN_MAX];
        if (s->scale && tc_scale_defined((uint8_t)(s->scale - 1u))) layout.mode = (uint8_t)(s->scale - 1u);
        uint32_t size = tn_name(&n, tc_pad_root(layout, s->value), name);
        if (size && ui_length + size <= UI_COLUMNS) ui_put(name);
        else ui_put("CHORD");
    } else { ui_note(n.notes[0]); if (l->kind == LN_DRUMS) { ui_put(" "); ui_number(n.notes[0]); } }
}
/* 1.7.0: the held step's field and its value (an off step: what it gets when changed). */
static void step_field(const tc_controls *c, const ln_step *s) {
    ln_step t = *s;
    if (!t.on) { t = c->brush[c->lane % LN_LANES]; t.on = true; }
    uint8_t field = (uint8_t)(c->step_field % TC_FIELDS);
    ui_start(); ui_put(ui_field_names[field]); ui_put(" ");
    switch (field) {
    case TC_FIELD_VELOCITY: ui_number(t.velocity ? t.velocity : LN_VELOCITY_DEFAULT); break;
    case TC_FIELD_GATE: ui_number(ln_gate_pct(&t)); ui_put("%"); break;
    case TC_FIELD_TIE: ui_put(ui_off_on[t.tie ? 1u : 0u]); break;
    case TC_FIELD_PROBABILITY: if (t.probability) { ui_number(ln_probability_pct(t.probability)); ui_put("%"); } else ui_put("ALWAYS"); break;
    case TC_FIELD_RATCHET: ui_number(ln_strikes(&t)); ui_put(ln_strikes(&t) > 1u ? " STRIKES" : " STRIKE"); break;
    case TC_FIELD_NUDGE: { /* 2.0.3: sixteenths of a step */
        int32_t n = ln_nudge(t.nudge);
        if (!n) ui_put("ON TIME");
        else { ui_put(n < 0 ? "-" : "+"); ui_number((uint32_t)(n < 0 ? -n : n)); ui_put("/16"); }
        break;
    }
    default: ui_put(ui_condition_names[t.condition < LN_CONDITIONS ? t.condition : 0u]); break;
    }
}
void ui_draw_steps(const tc_controls *c, const tp_engine *e) {
    uint32_t at = c->lane % LN_LANES;
    const ln_lane *l = &ln.lanes[at];
    uint32_t steps = ln_length(l);
    ui_start(); ui_put("L"); ui_number(at + 1u); ui_put(" "); ui_put(ui_kind_names[l->kind % LN_KINDS]); ui_put(" ");
    ui_number(steps); ui_put(" "); ui_put(ui_rate_names[ln_rate(l) % TC_ARP_RATES]); ui_line(0);
    uint8_t ch; tp_notes n; ln_step none; memset(&none, 0, sizeof none);
    (void)tc_step_notes(tc_layout(c), l->kind, l->channel, &none, &n, &ch);
    ui_start(); ui_put(c->step_page ? "PAGE 49-64" : "PAGE 1-48"); ui_put(" CH"); ui_number(ch + 1u);
    if (l->mute) ui_put(" MUTE");
    ui_line(11);
    ui_start();
    uint32_t held = (uint32_t)__builtin_popcountll(c->step_mask);
    if (c->step_held < LN_STEPS) {
        ui_put("STEP "); ui_number(c->step_held + 1u);
        if (held > 1u) { ui_put(" +"); ui_number(held - 1u); }
        ui_put(": ");
        if (l->steps[c->step_held].on) step_value(c, l, &l->steps[c->step_held]); else ui_put("OFF");
    }
    else { ui_put("PICKED: "); step_value(c, l, &c->brush[at]); }
    ui_line(22);
    if (c->step_held < LN_STEPS) { step_field(c, &l->steps[c->step_held]); ui_line(34); } /* 1.7.0: VELOCITY 100 */
    else { ui_last_action(c); ui_line(34); }
    ui_start();
    /* 1.8.2: centre tapped plays or stops, held starts step record; the hint while neither is going on */
    bool hint = !c->step_rec && c->step_held >= LN_STEPS;
    if (e->lane_running && e->lane_step[at] < LN_STEPS) {
        ui_put(hint ? "PLAY " : "PLAYING "); ui_number(e->lane_step[at] + 1u); ui_put("/"); ui_number(steps);
        if (hint) ui_put(" HOLD O:REC");
    } else if (l->kind == LN_OFF) ui_put("LANE OFF");
    else ui_put(hint ? "STOPPED  HOLD O: REC" : "STOPPED");
    ui_line(45);
    ui_start();
    if (tc_shift(c)) ui_put("<>MOVE ^CPY vPST OCLR"); /* 2.0.0: Shift + the buttons (controls_steps.c) */
    else if (c->step_held < LN_STEPS) ui_put("<- >+  UP/DN: FIELD");
    else if (c->step_rec) { ui_put("REC "); ui_number(c->step_cursor % steps + 1u); ui_put("/"); ui_number(steps); ui_put(" >REST O END"); }
    else if (c->step_page) ui_put(c->transport ? "<1-48       O STOP" : "<1-48       O PLAY");
    else ui_put(c->transport ? "<BACK  O STOP  >49-64" : "<BACK  O PLAY  >49-64");
    ui_line(56);
}
/* The step screen's pads (1.5.0): on the right half, steps past the lane's end dark, steps that are
 * on in the color of what they play (beats a little brighter when off), the step playing white, the
 * step record cursor pulsing white; on the left half of a melody or drum lane, each pad the color of
 * its note (the root, or the kick, brightest). Chord lanes keep the chord pads' usual colors. */
bool ui_step_color(const tc_controls *c, const tp_engine *e, uint32_t k, uint32_t max, uint32_t pulse, tl_rgb *out) {
    uint32_t at = c->lane % LN_LANES;
    const ln_lane *l = &ln.lanes[at];
    const tp_key *key = &e->keys[k];
    tp_layout layout = tc_layout(c);
    if (k < 48u && l->kind != LN_MELODY && l->kind != LN_DRUMS) return false;
    if (key->sensed) { *out = ui_scaled(255, 255, 255, max); return true; }
    if (k < 48u) {
        ln_step s; memset(&s, 0, sizeof s); s.value = ln_pad_value(l->kind, k);
        tp_notes n; uint8_t ch;
        if (!tc_step_notes(layout, l->kind, l->channel, &s, &n, &ch)) { *out = (tl_rgb){0, 0, 1}; return true; }
        if (l->kind == LN_DRUMS) { *out = ui_scaled(255, 110, 0, n.notes[0] == TP_DRUM_BASE ? max : max * 45u / 100u); return true; }
        tl_rgb h = ui_hues[n.notes[0] % 12u];
        *out = ui_scaled(h.r, h.g, h.b, n.notes[0] % 12u == c->tonic % 12u ? max : max * 60u / 100u);
        return true;
    }
    uint32_t pad = k - 48u, step = c->step_page * 48u + (pad % 4u) * 12u + pad / 4u;
    if (step >= ln_length(l)) { *out = (tl_rgb){0, 0, 1}; return true; }
    if (e->lane_running && e->lane_step[at] == step) { *out = ui_scaled(255, 255, 255, max * 70u / 100u); return true; }
    if (c->step_rec && c->step_cursor % ln_length(l) == step) { *out = ui_scaled(255, 255, 255, max * pulse / 100u); return true; }
    const ln_step *s = &l->steps[step];
    tp_notes n; uint8_t ch;
    if (!s->on) { *out = step % 4u ? ui_scaled(14, 14, 18, max) : ui_scaled(40, 40, 50, max); return true; }
    if (l->kind == LN_DRUMS || !tc_step_notes(layout, l->kind, l->channel, s, &n, &ch)) { *out = ui_scaled(255, 110, 0, max); return true; }
    uint32_t pc = l->kind == LN_CHORDS ? tc_pad_root(layout, s->value) % 12u : n.notes[0] % 12u;
    *out = ui_scaled(ui_hues[pc].r, ui_hues[pc].g, ui_hues[pc].b, max);
    return true;
}
