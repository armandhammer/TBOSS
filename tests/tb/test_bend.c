/* SPDX-License-Identifier: GPL-3.0-or-later - 2.4.12 (roadmap C): pitch bend per step on the real code (engine, controls,
 * settings.c on a fake sector 11, the screens): a step's bend in eighths of a semitone of Settings > Steps > Bend range,
 * only on a lane with a MIDI channel to itself, sent on the lane's channel just before the step's note (same transfer)
 * and back to centre at a rest, stop, mute, a shared or changed channel, all notes off and in a new session; the step
 * screen (Shift + M7, left/right, Shift + left/right), storage. Harness as test_v24.c. */
#include "../../src/tb/settings.h"
#include "../../src/tb/sequence.h"
#include "../../src/tb/midi_in.h"
#include "../../src/tb/flash_store.h"
#include "../../src/tb/display.h"
#include "../../src/tb/leds.h"
#include "../../src/tb/jack.h"
#include "../../src/tb/nor.h"
#include "../../src/tb/ui.h"
#include "../../src/tb/controls_internal.h"
#include "../../src/tb/ui_internal.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
volatile ts_abi ts_status;
static uint32_t flash[FS_SIZE / 4u];
const uint8_t *fs_base(void) { return (const uint8_t *)flash; }
bool fs_erase(void) { memset(flash, 0xFF, sizeof flash); return true; }
bool fs_program(uint32_t offset, const uint32_t *words, uint32_t count) {
    for (uint32_t i = 0; i < count; ++i) flash[offset / 4u + i] &= words[i];
    return true;
}
bool nr_present(void) { return false; }
void nr_id(uint8_t id[3]) { id[0] = id[1] = id[2] = 0u; }
uint32_t nr_size(void) { return 0u; }
bool nr_read(uint32_t a, uint8_t *out, uint32_t n) { (void)a; (void)out; (void)n; return false; }
static tl_rgb pads[TL_PADS];
static uint16_t buttons[TL_BUTTONS];
bool tl_init(void) { return true; }
void tl_set_pad(uint32_t pad, tl_rgb c) { if (pad < TL_PADS) pads[pad] = c; }
void tl_set_button(uint32_t channel, uint16_t level) { if (channel < TL_BUTTONS) buttons[channel] = level; }
void tl_service(void) {}
uint32_t jh_levels(void) { return 5u; }
bool td_init(void) { return true; }
void td_pixel(uint32_t x, uint32_t y, bool on) { (void)x; (void)y; (void)on; }
void td_invert(uint32_t x0, uint32_t y0, uint32_t x1, uint32_t y1) { (void)x0; (void)y0; (void)x1; (void)y1; }
void td_service(void) {}
static char lines[64][32];
void td_clear(void) { memset(lines, 0, sizeof lines); }
uint32_t td_text(uint32_t x, uint32_t y, const char *text, uint32_t scale) {
    if (y < 64u && !x) snprintf(lines[y], sizeof lines[y], "%s", text);
    return x + 6u * scale * (uint32_t)strlen(text);
}

static tp_engine e;
static ts_scan scan;
static uint32_t now = 1000u;
static uint8_t sounding[16][128];
#define LOG 16384u
static uint8_t on_note[LOG], on_ch[LOG];
static uint32_t on_at[LOG], logged;
/* Every packet out: its time, kind, status, data and transfer (token). */
typedef struct { uint32_t at, token; uint8_t kind, status, a, b; } packet;
static packet out[LOG];
static uint32_t sent;
static bool writer(const uint8_t p[4], uint32_t token) {
    uint32_t ch = p[1] & 15u;
    if (p[0] == TK_ON) {
        assert(!sounding[ch][p[2]]); sounding[ch][p[2]] = 1u;
        if (logged < LOG) { on_note[logged] = p[2]; on_ch[logged] = (uint8_t)ch; on_at[logged++] = now; }
    }
    if (p[0] == TK_OFF) { assert(sounding[ch][p[2]]); sounding[ch][p[2]] = 0u; }
    if (sent < LOG) out[sent++] = (packet){now, token, p[0], p[1], p[2], p[3]};
    return true;
}
/* A DAW's clock: one MIDI clock every daw_us microseconds while daw_us is set. */
static uint32_t daw_us, daw_next_us;
static void midi(uint8_t a, uint8_t b, uint8_t c, uint8_t d) { const uint8_t p[4] = {a, b, c, d}; tm_packet(p, now); }
static const uint8_t addresses[4] = {0x35, 0x36, 0x38, 0x39};
static uint32_t raw[4], missing; /* missing: bit i, client i doesn't answer (NACK) */
static uint32_t client(uint8_t a) { for (uint32_t i = 0; i < 4u; ++i) if (addresses[i] == a) return i; assert(0); return 0; }
static struct bi_reply reader(void *ctx, uint8_t a, uint8_t reg, uint32_t gen, uint32_t stamp, uint32_t budget) {
    (void)ctx; (void)reg; (void)budget;
    uint32_t i = client(a);
    return (struct bi_reply){.status = (missing >> i) & 1u ? BI_NACK : BI_OK, .count = 2, .generation = gen, .timestamp_ms = stamp, .elapsed_us = 500,
                             .data = {(uint8_t)raw[i], (uint8_t)(raw[i] >> 8)}};
}
static void run(uint32_t ms) {
    for (uint32_t i = 0; i < ms; ++i, ++now) {
        while (daw_us && (int32_t)(now * 1000u - daw_next_us) >= 0) { midi(0x0Fu, 0xF8u, 0u, 0u); daw_next_us += daw_us; }
        if (now % 10u == 0u) (void)tc_poll(&tc_state, &e, reader, NULL, now);
        if (now % 15u == 0u) {
            scan.frame++; scan.last_ms = now; scan.valid = true;
            if (e.state == TK_IDLE) (void)tc_arm(&e, &scan, now, true); else (void)tc_frame(&e, &scan, now);
        }
        tc_step(&e, now, true, writer);
        if (e.pending_id) tp_complete(&e, e.pending_id, tp_pending_bytes(&e), true, now);
        st_requests(&tc_state, &e);
        st_service(&tc_state, now);
    }
}
/* Touch-controller bits (controls.c's tables). */
static const uint8_t fn_bit[TC_FUNCTIONS] = {6, 5, 0, 1, 7, 4, 3, 8, 9, 10, 11, 12, 14, 2};
static const uint8_t top_bit[12] = {3, 1, 0, 4, 5, 6, 7, 8, 9, 10, 11, 12};
static const uint8_t nav_bit[5] = {8, 7, 6, 1, 3}; /* left, up, centre, down, right */
static void hold_bit(uint8_t a, uint8_t bit, bool down) {
    if (down) raw[client(a)] |= 1u << bit; else raw[client(a)] &= ~(1u << bit);
    run(50);
}
static void tap_bit(uint8_t a, uint8_t bit) { hold_bit(a, bit, true); hold_bit(a, bit, false); }
static void nav(uint32_t n) { tap_bit(0x36, nav_bit[n]); }
static void mode_button(uint32_t i) { tap_bit(0x38, top_bit[i]); }
static void mode_hold(uint32_t i, bool down) { hold_bit(0x38, top_bit[i], down); }
static void root_button(uint32_t i) { tap_bit(0x39, top_bit[i]); }
static void root_hold(uint32_t i, bool down) { hold_bit(0x39, top_bit[i], down); }
static void centre(uint32_t i, bool down) { hold_bit(0x35, fn_bit[i], down); }
static void hold(uint32_t k) { scan.values[k] = 3000u; run(60); }
static void lift(uint32_t k) { scan.values[k] = 0u; run(60); }
static void touch(uint32_t k) { hold(k); lift(k); }  /* a tap */
static void draw(void) { tc_state.event_sequence++; ui_service(&tc_state, &e, &scan, true, now); }
static uint32_t step_pad(uint32_t s) { /* the right-half pad showing step s on the first page */
    for (uint32_t k = 48u; k < 96u; ++k) if (tc_step_of_pad(&tc_state, k - 48u) == s) return k;
    assert(0); return 0;
}
static void boot_keep(void) { /* a restart on the flash as it is */
    tc_init(&tc_state); memset(&e, 0, sizeof e); memset(&scan, 0, sizeof scan); memset(raw, 0, sizeof raw);
    memset(sounding, 0, sizeof sounding); logged = 0u; daw_us = 0u; missing = 0u;
    tm_reset(); sq_clear(&sq, SQ_UNIT_BAR, 0u);
    (void)st_load(&tc_state);
    run(600); assert(e.state == TK_ACTIVE);
}
static void boot(void) { fs_erase(); boot_keep(); }
static void quiet(void) {
    for (uint32_t ch = 0; ch < 16u; ++ch) for (uint32_t p = 0; p < 128u; ++p) assert(!sounding[ch][p]);
}
static void stop(void) { if (tc_state.transport) tc_toggle_transport(&tc_state); run(400); quiet(); }
static ln_step on_step(uint8_t value) { ln_step s = tc_brush_default(LN_DRUMS); s.value = value; return s; }
/* Lane `lane` of pattern p: drums, n steps of 1/16, step i playing note first + i. */
static void drums(uint32_t p, uint32_t lane, uint32_t n, uint8_t first) {
    ln_lane *l = &ln_bank[p].lanes[lane];
    l->kind = LN_DRUMS; ln_set_length(l, n);
    for (uint32_t i = 0; i < n; ++i) l->steps[i] = on_step((uint8_t)(first + i));
}
static uint32_t drum_ch(void) { return tc_state.drum_ch & 15u; }
/* The time of the k-th Note On of note n (any channel when ch is 16) from log entry `from`; 0 when not heard. */
static uint32_t time_of(uint8_t n, uint32_t k, uint32_t from) {
    for (uint32_t i = from; i < logged; ++i) if (on_ch[i] == drum_ch() && on_note[i] == n && !k--) return on_at[i];
    return 0u;
}
static bool heard(uint8_t n, uint32_t from) { return time_of(n, 0u, from) != 0u; }
static bool near(uint32_t a, uint32_t b, uint32_t within) { return a + within >= b && b + within >= a; }
static void steps_screen(void) {
    tc_state.ui = TC_UI_STEPS; tc_state.step_page = 0u; tc_state.step_held = LN_NONE; tc_state.step_mask = 0u; run(20);
}

/* --- helpers --- */
#define BEND_CH 4u /* the lane's channel (MIDI channel 5): nothing else plays there */
static int32_t bend_value(const packet *p) { return (int32_t)(p->a | (uint32_t)p->b << 7) - 8192; }
static bool is_bend(const packet *p, uint32_t ch) { return p->kind == TK_BEND && (p->status & 15u) == ch && (p->status >> 4) == 0xEu; }
static bool is_on(const packet *p, uint32_t ch) { return p->kind == TK_ON && (p->status & 15u) == ch; }
/* Lane 1: melody on channel 5, 4 steps of 1/16 (125 ms at 120 BPM): +1 semitone (2048 of 4096 at Bend range 2), a rest,
 * none, -2 semitones (the bottom of the range). */
static void bend_lane(void) {
    ln_lane *l = &ln.lanes[0];
    l->kind = LN_MELODY; l->channel = BEND_CH + 1u; ln_set_length(l, 4u);
    for (uint32_t i = 0; i < 4u; ++i) { l->steps[i] = tc_brush_default(LN_MELODY); l->steps[i].value = (uint8_t)(3u + 4u * i); }
    l->steps[0].bend = 2048; memset(&l->steps[1], 0, sizeof l->steps[1]); l->steps[3].bend = LN_BEND_MIN;
    for (uint32_t s = 0; s < LN_STEPS; s += LN_CHUNK) ln_changed(0u, s);
    ln_changed(0u, LN_STEPS); run(20);
}
/* The bends on ch from packet `from` on, in order, into v (at most n); how many. */
static uint32_t bends(uint32_t ch, uint32_t from, int32_t *v, uint32_t n) {
    uint32_t k = 0;
    for (uint32_t i = from; i < sent; ++i) if (is_bend(&out[i], ch) && k < n) v[k++] = bend_value(&out[i]);
    return k;
}
static uint32_t count_bends(uint32_t ch, uint32_t from) { int32_t v[LOG / 4]; return bends(ch, from, v, LOG / 4); }

/* --- 1. the units: eighths of a semitone of Bend range --- */
static void units(void) {
    boot();
    for (uint8_t r = 0; r < TC_BEND_RANGES; ++r) {
        tc_state.bend_range = r;
        int32_t most = 8 * tc_bend_semis[r];
        for (int32_t x = -most; x <= most; ++x) assert(tc_bend_eighths(&tc_state, tc_bend_of_eighths(&tc_state, x)) == x);
        assert(tc_bend_of_eighths(&tc_state, most) == LN_BEND_MAX && tc_bend_of_eighths(&tc_state, -most) == LN_BEND_MIN);
        assert(tc_bend_of_eighths(&tc_state, most + 50) == LN_BEND_MAX && tc_bend_of_eighths(&tc_state, -most - 50) == LN_BEND_MIN);
        assert(tc_bend_eighths(&tc_state, LN_BEND_MAX) == most && tc_bend_eighths(&tc_state, 0) == 0);
    }
    tc_state.bend_range = 0u;
    assert(tc_bend_of_eighths(&tc_state, 8) == 2048 && tc_bend_of_eighths(&tc_state, 1) == 256);
    /* the menu: 2, 12, 24, 48 semitones, a board setting */
    assert(tc_menu_values(TC_MENU_BEND_RANGE) == TC_BEND_RANGES);
    assert(!strcmp(ui_value_text(TC_MENU_BEND_RANGE, 0u), "2 SEMIS") && !strcmp(ui_value_text(TC_MENU_BEND_RANGE, 3u), "48 SEMIS"));
    assert(!strcmp(ui_item_name(TC_MENU_BEND_RANGE), "BEND RANGE"));
    uint8_t items[TC_GROUP_MAX]; uint8_t n = tc_page_items(&tc_state, TC_GROUP_STEPS, items);
    assert(n && items[n - 1u] == TC_MENU_BEND_RANGE);
    /* a controller's knob can take the 2.4.9-2.4.12 settings too (Settings > Control > Map button: the MIDI map) */
    static const uint8_t mapped[] = {TC_MENU_BEND_RANGE, TC_MENU_CHORDS_SIDE, TC_MENU_JOIN, TC_MENU_RIGHT_OCT, TC_MENU_SPLIT,
        TC_MENU_LANE_LENGTH, TC_MENU_LANE_DIRECTION, TC_MENU_LANE_SWING, TC_MENU_LANE_RATE};
    for (uint32_t i = 0; i < sizeof mapped; ++i) assert(tc_cc_mappable(mapped[i]));
    printf("units: eighths of a semitone round trip in every range (2, 12, 24, 48), clamped, the menu item (mappable, as All "
           "Melody's and the lane settings); ");
}

/* --- 2. only a lane with a channel to itself --- */
static void own_channel(void) {
    boot(); bend_lane();
    assert(tc_lane_bend_ok(&tc_state, 0u));
    assert(!tc_lane_bend_ok(&tc_state, 1u)); /* a chord lane on the chord channel */
    assert(tc_set_item(&tc_state, &e, TC_MENU_MELODY_CH, BEND_CH)); assert(!tc_lane_bend_ok(&tc_state, 0u));
    assert(tc_set_item(&tc_state, &e, TC_MENU_MELODY_CH, 0u)); assert(tc_lane_bend_ok(&tc_state, 0u));
    assert(tc_set_item(&tc_state, &e, TC_MENU_CHORD_CH, BEND_CH)); assert(!tc_lane_bend_ok(&tc_state, 0u));
    assert(tc_set_item(&tc_state, &e, TC_MENU_CHORD_CH, 1u)); assert(tc_lane_bend_ok(&tc_state, 0u));
    tc_state.mpe = TC_MPE_MELODY; tc_state.changes++; assert(!tc_lane_bend_ok(&tc_state, 0u)); /* the lower zone, 1..8 */
    tc_state.mpe = TC_MPE_OFF; tc_state.changes++; assert(tc_lane_bend_ok(&tc_state, 0u));
    ln.lanes[5].kind = LN_DRUMS; ln.lanes[5].channel = BEND_CH + 1u; assert(!tc_lane_bend_ok(&tc_state, 0u)); /* another lane */
    ln.lanes[5].kind = LN_OFF; assert(tc_lane_bend_ok(&tc_state, 0u)); /* an Off lane doesn't count */
    tc_state.bass_ch = BEND_CH; tc_state.melody_layout = TP_MELODY_DRUMS; tc_state.changes++;
    assert(!tc_lane_bend_ok(&tc_state, 0u)); /* the Drums layout's bass pads */
    tc_state.melody_layout = TP_MELODY_SCALE; tc_state.changes++; assert(tc_lane_bend_ok(&tc_state, 0u));
    tc_state.live_ch = BEND_CH + 1u; tc_state.melody_layout = TP_MELODY_LIVE; tc_state.changes++;
    assert(!tc_lane_bend_ok(&tc_state, 0u)); /* live notes */
    tc_state.live_ch = 0u; tc_state.melody_layout = TP_MELODY_SCALE; tc_state.changes++;
    tc_state.pedal_mode = TC_PEDAL_NOTE; tc_state.drum_ch = BEND_CH; tc_state.changes++;
    assert(!tc_lane_bend_ok(&tc_state, 0u)); /* the pedal note */
    printf("own channel: refused on the halves' channels, an MPE zone, another lane, the bass pads, live notes, the pedal note; ");
}

/* --- 3. what goes out --- */
static void playing(void) {
    boot(); bend_lane(); sent = 0u;
    tc_toggle_transport(&tc_state); run(1010); /* two rounds of 4 steps and a bit */
    /* each round: +4096 before step 1's note, centre at the rest, nothing new before step 3 (centre already), -8192 before
     * step 4's; each bend in the same transfer as the note after it, the bend first */
    int32_t v[16]; uint32_t n = bends(BEND_CH, 0u, v, 16u);
    assert(n >= 6u && v[0] == 4096 && v[1] == 0 && v[2] == -8192 && v[3] == 4096 && v[4] == 0 && v[5] == -8192);
    for (uint32_t i = 0; i < sent; ++i) {
        if (!is_bend(&out[i], BEND_CH) || !bend_value(&out[i])) continue; /* a centre at a rest has no note after it */
        assert(i + 1u < sent && is_on(&out[i + 1u], BEND_CH) && out[i + 1u].token == out[i].token);
    }
    for (uint32_t ch = 0; ch < 16u; ++ch) if (ch != BEND_CH) assert(!count_bends(ch, 0u)); /* only its own channel */
    /* stopped: back to centre */
    while (!(e.lane_step[0] == 3u && e.lane_bent[0])) run(1);
    uint32_t from = sent; tc_toggle_transport(&tc_state); run(50);
    n = bends(BEND_CH, from, v, 16u); assert(n == 1u && v[0] == 0);
    /* muted while bent: centre */
    tc_toggle_transport(&tc_state);
    while (!(e.lane_step[0] == 3u && e.lane_bent[0])) run(1);
    from = sent; ln.lanes[0].mute = 1u; ln_changed(0u, LN_STEPS); run(30);
    n = bends(BEND_CH, from, v, 16u); assert(n == 1u && v[0] == 0);
    ln.lanes[0].mute = 0u; ln_changed(0u, LN_STEPS);
    /* the channel shared while bent: centre at once, then no bends while it is shared */
    while (!(e.lane_step[0] == 0u && e.lane_bent[0])) run(1);
    from = sent; assert(tc_set_item(&tc_state, &e, TC_MENU_MELODY_CH, BEND_CH)); run(600);
    n = bends(BEND_CH, from, v, 16u); assert(n == 1u && v[0] == 0);
    assert(tc_set_item(&tc_state, &e, TC_MENU_MELODY_CH, 0u));
    /* the lane moved to another channel while bent: the old one back to centre first */
    while (!(e.lane_step[0] == 3u && e.lane_bent[0])) run(1);
    from = sent; tc_state.lane = 0u; tc_lane_setting(&tc_state, TC_MENU_LANE_CHANNEL, BEND_CH + 2u); run(600);
    n = bends(BEND_CH, from, v, 16u); assert(n == 1u && v[0] == 0);
    assert(count_bends(BEND_CH + 1u, from) >= 2u);
    tc_lane_setting(&tc_state, TC_MENU_LANE_CHANNEL, BEND_CH + 1u); run(300);
    /* all notes off while bent: centre */
    while (!(e.lane_step[0] == 3u && e.lane_bent[0])) run(1);
    from = sent; tc_state.panic = true; run(20);
    n = bends(BEND_CH, from, v, 16u); assert(n >= 1u && v[0] == 0);
    /* the session lost while bent: the next one puts it back to centre */
    while (!(e.lane_step[0] == 3u && e.lane_bent[0])) run(1);
    tc_state.transport = false; tp_loss(&e); from = sent; run(1500);
    assert(e.state == TK_ACTIVE); n = bends(BEND_CH, from, v, 16u); assert(n == 1u && v[0] == 0);
    stop();
    printf("playing: the step's bend just before its note in the same transfer, centre at a rest, none sent again when "
           "already there, centre when stopped, muted, shared, moved to another channel, all notes off, and in the next session; ");
}

/* --- 4. on the step screen --- */
static void editing(void) {
    boot(); bend_lane(); steps_screen();
    ln_lane *l = &ln.lanes[0];
    tc_state.buttons[0] = TC_ACT_SHIFT;
    hold(step_pad(0));
    centre(0, true); mode_button(TC_MODE_M1 + 6u); centre(0, false); /* Shift + M7: BEND */
    assert(tc_state.step_field == TC_FIELD_BEND); draw(); assert(!strcmp(lines[34], "BEND +1"));
    nav(TC_NAV_RIGHT); assert(l->steps[0].bend == 2304); draw(); assert(!strcmp(lines[34], "BEND +1.125"));
    run(TC_SHIFT_LATCH_MS + 50u); /* two Shift taps close together would latch it (2.0.0) */
    centre(0, true); nav(TC_NAV_LEFT); centre(0, false); /* Shift: a semitone */
    assert(l->steps[0].bend == 256); draw(); assert(!strcmp(lines[34], "BEND +0.125"));
    for (uint32_t i = 0; i < 4u; ++i) nav(TC_NAV_LEFT);
    draw(); assert(!strcmp(lines[34], "BEND -0.375") && l->steps[0].bend == -768);
    assert(l->steps[1].on == false);
    lift(step_pad(0)); assert(l->steps[0].on); /* not toggled */
    /* the steps aren't moved (Shift + left alone moves them) */
    assert(l->steps[3].bend == LN_BEND_MIN && l->steps[2].value == 11u);
    /* Bend range 12: the same bend shows in its semitones; an off step goes on with the picked value and the bend */
    assert(tc_set_item(&tc_state, &e, TC_MENU_BEND_RANGE, 1u));
    hold(step_pad(0)); draw(); assert(!strcmp(lines[34], "BEND -2.25")); lift(step_pad(0));
    hold(step_pad(1)); nav(TC_NAV_RIGHT); lift(step_pad(1));
    assert(l->steps[1].on && l->steps[1].bend == tc_bend_of_eighths(&tc_state, 1) && l->steps[1].bend == 43);
    /* clamped at the range */
    hold(step_pad(3)); draw(); assert(!strcmp(lines[34], "BEND -12"));
    run(TC_SHIFT_LATCH_MS + 50u); centre(0, true); nav(TC_NAV_LEFT); centre(0, false); assert(l->steps[3].bend == LN_BEND_MIN);
    lift(step_pad(3));
    /* a shared channel: refused */
    assert(tc_set_item(&tc_state, &e, TC_MENU_MELODY_CH, BEND_CH));
    hold(step_pad(0)); draw(); assert(!strcmp(lines[34], "BEND CH SHARED"));
    int16_t was = l->steps[0].bend;
    nav(TC_NAV_RIGHT); assert(l->steps[0].bend == was && tc_state.last_action == TC_BEND_SHARED);
    draw(); assert(!strcmp(lines[34], "BEND CH SHARED"));
    lift(step_pad(0));
    assert(tc_set_item(&tc_state, &e, TC_MENU_MELODY_CH, 0u));
    /* Shift + M7 with no step held: the field for the next */
    run(TC_SHIFT_LATCH_MS + 50u); centre(0, true); mode_button(TC_MODE_M1); centre(0, false); /* Shift + M1: preset 1 (none there) */
    mode_button(TC_MODE_M1); assert(tc_state.step_field == TC_FIELD_VELOCITY);
    run(TC_SHIFT_LATCH_MS + 50u); centre(0, true); mode_button(TC_MODE_M1 + 6u); centre(0, false); assert(tc_state.step_field == TC_FIELD_BEND);
    /* kept: the bends and Bend range over a restart */
    int16_t b0 = l->steps[0].bend, b1 = l->steps[1].bend;
    run(4000); boot_keep(); l = &ln.lanes[0];
    assert(l->steps[0].bend == b0 && l->steps[1].bend == b1 && l->steps[3].bend == LN_BEND_MIN && tc_state.bend_range == 1u);
    printf("editing: Shift + M7 chooses BEND, eighths with left/right, a semitone with Shift, shown in Bend range's "
           "semitones, an off step on with it, clamped, refused on a shared channel, kept over a restart; ");
}

int main(void) {
    (void)near; (void)heard; (void)drums; (void)touch; (void)root_hold; (void)root_button; (void)mode_hold; /* test_v24.c's harness */
    units();
    own_channel();
    playing();
    editing();
    printf("bend PASS\n");
    return 0;
}
