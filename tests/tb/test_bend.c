/* SPDX-License-Identifier: GPL-3.0-or-later - 2.4.12 (roadmap C): pitch bend per step on the real code (engine, controls,
 * settings.c on a fake sector 11, the screens): a step's bend in tenths (2.6.3) of a semitone of Settings > Steps > Bend range,
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
/* The bends on ch that change its value, from packet `from` on, in order, into v (at most n); how many. 2.6.0: the
 * old value sent once more before a change is not one of them (flat counts and checks those); 2.6.2: nor a point
 * 1 off it. */
static uint32_t bends(uint32_t ch, uint32_t from, int32_t *v, uint32_t n) {
    uint32_t k = 0; int32_t last = 0;
    for (uint32_t i = 0; i < sent; ++i) {
        if (!is_bend(&out[i], ch)) continue;
        int32_t d = bend_value(&out[i]) - last; /* 2.6.2: a point 1 off is the old step's closing, not a change */
        if ((d > 1 || d < -1) && i >= from && k < n) v[k++] = bend_value(&out[i]);
        last = bend_value(&out[i]);
    }
    return k;
}
static uint32_t count_bends(uint32_t ch, uint32_t from) { int32_t v[LOG / 4]; return bends(ch, from, v, LOG / 4); }
static uint32_t every_bend(uint32_t ch) { /* every pitch bend message on ch, the repeated ones too */
    uint32_t k = 0;
    for (uint32_t i = 0; i < sent; ++i) if (is_bend(&out[i], ch)) k++;
    return k;
}
/* 2.6.0, clean steps in a recording, over the whole log of ch: a bend that changes the value comes less than
 * TP_LANE_FLAT_MS after the channel's last bend message (so after 20 ms of nothing the old value went once more first),
 * and the old value again comes only after that long, with the new value right behind it in the same transfer.
 * How many times the old value went again. */
/* 2.6.2: the old value's point is 1 toward the new value (or the old value itself on a channel's first bend), and
 * mostly a step's closing: TP_LANE_LEAD_MS or so before the change, in a transfer of its own (closings counts those);
 * the rest (a chance step mispredicted, a mute, a stop) are 2.6.0's repeat, right before the change in its transfer.
 * Every change of more than 1 comes less than TP_LANE_FLAT_MS after the channel's last bend message. How many points. */
static uint32_t closings;
static uint32_t flat(uint32_t ch) {
    uint32_t again = 0, last_at = 0; int32_t last = 0; bool seen = false;
    for (uint32_t i = 0; i < sent; ++i) {
        if (!is_bend(&out[i], ch)) continue;
        int32_t b = bend_value(&out[i]), d = b - last;
        if (d >= -1 && d <= 1) { /* the old step's point (the log's first one too: from centre) */
            uint32_t j = i + 1u;
            while (j < sent && !is_bend(&out[j], ch)) j++;
            if (j < sent && bend_value(&out[j]) != b) {
                /* (toward the new value: checked in playing; at random a mute or a chance step may change the next
                 * value after the closing went, 1 the other way: not heard) */
                assert(out[j].at - out[i].at < TP_LANE_FLAT_MS);
                if (out[j].token != out[i].token) { assert(out[j].at > out[i].at); closings++; }
                again++;
            }
        } else if (b != last) assert(seen && out[i].at - last_at < TP_LANE_FLAT_MS);
        last = b; last_at = out[i].at; seen = true;
    }
    return again;
}

/* 2.6.2: lane 1 bent by a step (a closing point, 1 off, is not that) */
static bool bent_away(void) { return e.lane_bent[0] > 1 || e.lane_bent[0] < -1; }

/* --- 1. the units: tenths of a semitone of Bend range (2.6.3; eighths before) --- */
static void units(void) {
    boot();
    for (uint8_t r = 0; r < TC_BEND_RANGES; ++r) {
        tc_state.bend_range = r;
        int32_t most = 10 * tc_bend_semis[r]; /* 2.6.3: tenths */
        for (int32_t x = -most; x <= most; ++x) assert(tc_bend_tenths(&tc_state, tc_bend_of_tenths(&tc_state, x)) == x);
        assert(tc_bend_of_tenths(&tc_state, most) == LN_BEND_MAX && tc_bend_of_tenths(&tc_state, -most) == LN_BEND_MIN);
        assert(tc_bend_of_tenths(&tc_state, most + 50) == LN_BEND_MAX && tc_bend_of_tenths(&tc_state, -most - 50) == LN_BEND_MIN);
        assert(tc_bend_tenths(&tc_state, LN_BEND_MAX) == most && tc_bend_tenths(&tc_state, 0) == 0);
    }
    tc_state.bend_range = 0u;
    assert(tc_bend_of_tenths(&tc_state, 10) == 2048 && tc_bend_of_tenths(&tc_state, 1) == 205);
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
    printf("units: tenths of a semitone round trip in every range (2, 12, 24, 48), clamped, the menu item (mappable, as All "
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
    int32_t was = 0;
    for (uint32_t i = 0; i < sent; ++i) {
        if (!is_bend(&out[i], BEND_CH)) continue;
        int32_t b = bend_value(&out[i]);
        if (b - was >= -1 && b - was <= 1) continue; /* 2.6.2: the old step's closing point */
        /* a new value that isn't centre (a centre at a rest has no note after it): its note right behind it; 2.6.0: and
         * the old value's point right before it (2.6.2: 1 toward the new value; the first step's in the same transfer,
         * every later step's its own closing, an earlier transfer) */
        if (b) assert(i + 1u < sent && is_on(&out[i + 1u], BEND_CH) && out[i + 1u].token == out[i].token);
        uint32_t j = i;
        while (j && !is_bend(&out[j - 1u], BEND_CH)) j--;
        assert(j);
        int32_t point = bend_value(&out[j - 1u]);
        assert(point == was + (b > was ? 1 : -1) || (point == was && out[j - 1u].token == out[i].token)); /* (exactly:
                                                         * the channel's first bend, centre there first) */
        assert(was ? out[j - 1u].token != out[i].token && out[j - 1u].at < out[i].at : out[j - 1u].token == out[i].token || out[j - 1u].at < out[i].at);
        was = b;
    }
    for (uint32_t ch = 0; ch < 16u; ++ch) if (ch != BEND_CH) assert(!every_bend(ch)); /* only its own channel */
    /* stopped: back to centre */
    while (!(e.lane_step[0] == 3u && bent_away())) run(1);
    uint32_t from = sent; tc_toggle_transport(&tc_state); run(50);
    n = bends(BEND_CH, from, v, 16u); assert(n == 1u && v[0] == 0);
    /* muted while bent: centre */
    tc_toggle_transport(&tc_state);
    while (!(e.lane_step[0] == 3u && bent_away())) run(1);
    from = sent; ln.lanes[0].mute = 1u; ln_changed(0u, LN_STEPS); run(30);
    n = bends(BEND_CH, from, v, 16u); assert(n == 1u && v[0] == 0);
    ln.lanes[0].mute = 0u; ln_changed(0u, LN_STEPS);
    /* the channel shared while bent: centre at once, then no bends while it is shared */
    while (!(e.lane_step[0] == 0u && bent_away())) run(1);
    from = sent; assert(tc_set_item(&tc_state, &e, TC_MENU_MELODY_CH, BEND_CH)); run(600);
    n = bends(BEND_CH, from, v, 16u); assert(n == 1u && v[0] == 0);
    assert(tc_set_item(&tc_state, &e, TC_MENU_MELODY_CH, 0u));
    /* the lane moved to another channel while bent: the old one back to centre first */
    while (!(e.lane_step[0] == 3u && bent_away())) run(1);
    from = sent; tc_state.lane = 0u; tc_lane_setting(&tc_state, TC_MENU_LANE_CHANNEL, BEND_CH + 2u); run(600);
    n = bends(BEND_CH, from, v, 16u); assert(n == 1u && v[0] == 0);
    assert(count_bends(BEND_CH + 1u, from) >= 2u);
    tc_lane_setting(&tc_state, TC_MENU_LANE_CHANNEL, BEND_CH + 1u); run(300);
    /* all notes off while bent: centre */
    while (!(e.lane_step[0] == 3u && bent_away())) run(1);
    from = sent; tc_state.panic = true; run(20);
    n = bends(BEND_CH, from, v, 16u); assert(n >= 1u && v[0] == 0);
    /* the session lost while bent: the next one puts it back to centre */
    while (!(e.lane_step[0] == 3u && bent_away())) run(1);
    tc_state.transport = false; tp_loss(&e); from = sent; run(1500);
    assert(e.state == TK_ACTIVE); n = bends(BEND_CH, from, v, 16u); assert(n == 1u && v[0] == 0);
    stop();
    /* 2.6.0: through all of that, every change that came 20 ms or more after the channel's last bend had the old value
     * once more right before it, and the old value never went again otherwise */
    closings = 0u;
    uint32_t again = flat(BEND_CH), changes = count_bends(BEND_CH, 0u);
    assert(again >= 12u && again <= changes && closings * 2u >= again); /* 2.6.2: most a step's own closing */
    printf("(2.6.2: %u of those points a step's closing, earlier and in a transfer of its own) ", (unsigned)closings);
    printf("playing: the step's bend just before its note in the same transfer, centre at a rest, none sent again when "
           "already there, centre when stopped, muted, shared, moved to another channel, all notes off, and in the next session; "
           "clean steps: %u of the %u changes came 20 ms or more after the last bend and each had the old value once more "
           "right before it in the same transfer; ", (unsigned)again, (unsigned)changes);
}

/* --- 3b. 2.6.0, Bend glide: a lane's bend moves to a step's value over a share of the step --- */
static void glide_lane(uint8_t glide) { /* steps: none, +1 semitone (4096), a rest, the bottom of the range (-8192) */
    ln_lane *l = &ln.lanes[0];
    l->kind = LN_MELODY; l->channel = BEND_CH + 1u; ln_set_length(l, 4u); l->glide = glide;
    for (uint32_t i = 0; i < 4u; ++i) { l->steps[i] = tc_brush_default(LN_MELODY); l->steps[i].value = (uint8_t)(3u + 4u * i); }
    l->steps[1].bend = 2048; memset(&l->steps[2], 0, sizeof l->steps[2]); l->steps[3].bend = LN_BEND_MIN;
    for (uint32_t s = 0; s < LN_STEPS; s += LN_CHUNK) ln_changed(0u, s);
    ln_changed(0u, LN_STEPS); ln_extra_changed(ln_current); run(20);
}
/* The packet of the k-th Note On on BEND_CH. */
static uint32_t note_at(uint32_t k) {
    for (uint32_t i = 0; i < sent; ++i) if (is_on(&out[i], BEND_CH) && !k--) return i;
    assert(0); return 0;
}
/* The bends on BEND_CH from packet a up to packet b move one way from `from` to `to`, paced (10 ms apart and 16 of 8192
 * at least, but for the last, which lands exactly and when the glide ends) and done within `within` ms of packet a
 * (short: the whole step, still on its way when the next step takes over, the last bend at most 10 ms of it short);
 * how many. */
static uint32_t glides(uint32_t a, uint32_t b, int32_t from, int32_t to, uint32_t within, bool short_of) {
    uint32_t k = 0, last_at = 0; int32_t last = from;
    for (uint32_t i = a; i < b; ++i) {
        if (!is_bend(&out[i], BEND_CH)) continue;
        int32_t v = bend_value(&out[i]);
        if (!k && v - last >= -1 && v - last <= 1) continue; /* the old value once more (2.6.2: 1 off), before it sets off */
        assert(to > from ? v > last && v <= to : v < last && v >= to);
        if (k && v != to) assert(out[i].at - last_at >= TP_MPE_BEND_MS);
        if (v != to) assert(v - last >= TP_MPE_BEND_DEADBAND || last - v >= TP_MPE_BEND_DEADBAND);
        assert(out[i].at - out[a].at <= within);
        last = v; last_at = out[i].at; k++;
    }
    if (short_of) assert(last - to < 1100 && to - last < 1100); else assert(last == to);
    return k;
}
static void glide(void) {
    /* stored: 4 bits a lane in the pattern record's word 10; firmware before 2.6 has no word 10 (it refuses the record) */
    boot();
    uint32_t w[LN_WORDS]; ln_pattern t;
    for (uint32_t i = 0; i < LN_LANES; ++i) ln.lanes[i].glide = (uint8_t)(i % LN_GLIDES);
    ln_extras_encode(&ln, w); assert(w[0] == 1u && w[2u + LN_LANES] == 0x21043210u);
    ln_clear(&t); assert(ln_extras_decode(w, &t));
    for (uint32_t i = 0; i < LN_LANES; ++i) assert(t.lanes[i].glide == i % LN_GLIDES && ln_glide(&t.lanes[i]) == i % LN_GLIDES);
    w[2u + LN_LANES] = 5u; assert(!ln_extras_decode(w, &t));          /* past the whole step */
    w[2u + LN_LANES] = 1u; w[0] = 0u; assert(!ln_extras_decode(w, &t)); /* not marked as there */
    w[0] = 1u; w[3u + LN_LANES] = 1u; assert(!ln_extras_decode(w, &t)); /* nothing past word 10 */
    ln_clear(&t); ln_extras_encode(&t, w); assert(!w[0] && !w[2u + LN_LANES]); /* off everywhere: as before 2.6 */
    for (uint32_t i = 0; i < LN_LANES; ++i) ln.lanes[i].glide = 0u;
    /* half a step (62 ms of the 125): step 1 (no bend) plays as ever, the first step never glides */
    glide_lane(2u); sent = 0u;
    tc_toggle_transport(&tc_state); run(540); /* a round of 4 steps and the start of the next */
    uint32_t n1 = note_at(0), n2 = note_at(1), n4 = note_at(2), n5 = note_at(3);
    assert(near(out[n2].at - out[n1].at, 125u, 3u) && near(out[n4].at - out[n2].at, 250u, 3u) && near(out[n5].at - out[n4].at, 125u, 3u));
    for (uint32_t i = 0; i < n2; ++i) assert(!is_bend(&out[i], BEND_CH));
    /* step 2: its note first (the bend is still where step 1 left it), then up to +4096 within half a step */
    uint32_t rest = 0;
    for (uint32_t i = n2; i < n4 && !rest; ++i) if (is_bend(&out[i], BEND_CH) && out[i].at - out[n2].at >= 120u) rest = i;
    assert(rest);
    uint32_t up = glides(n2, rest, 0, 4096, 62u + 12u, false);
    assert(up >= 6u && up <= 9u);
    /* the rest: +4096 once more (2.6.2: 1 toward centre), then centre, at the step */
    assert(bend_value(&out[rest]) == 4095 && is_bend(&out[rest + 1u], BEND_CH) && !bend_value(&out[rest + 1u]) &&
           near(out[rest].at - out[n2].at, 125u, 3u));
    /* step 4, after the rest: centre once more, then where the last step was (+4096) and the note, in one transfer;
     * from there down to -8192 within half a step */
    assert(n4 >= 2u && is_bend(&out[n4 - 1u], BEND_CH) && bend_value(&out[n4 - 1u]) == 4096 && out[n4 - 1u].token == out[n4].token);
    assert(is_bend(&out[n4 - 2u], BEND_CH) && bend_value(&out[n4 - 2u]) == 1 && /* 2.6.2: centre 1 toward it */ out[n4 - 2u].token == out[n4].token);
    uint32_t down = glides(n4, n5, 4096, -8192, 62u + 12u, false);
    assert(down >= 6u && down <= 9u);
    /* the next round's step 1 (no bend): its note, then back up to centre */
    run(70);
    (void)glides(n5, sent, -8192, 0, 62u + 12u, false);
    (void)flat(BEND_CH);
    for (uint32_t ch = 0; ch < 16u; ++ch) if (ch != BEND_CH) assert(!every_bend(ch));
    /* stopped on the way: centre at once, the glide over; the first step after it never glides */
    while (!(e.lane_step[0] == 3u && (e.lane_gliding & 1u) && e.lane_bent[0] < 0)) run(1);
    uint32_t from = sent; tc_toggle_transport(&tc_state); run(50);
    int32_t v[16]; uint32_t n = bends(BEND_CH, from, v, 16u);
    assert(n == 1u && v[0] == 0 && !e.lane_gliding && !e.lane_glide_known);
    stop();
    /* the whole step: on its way all step long, there by the next step; Off: a jump, as before 2.6 */
    glide_lane(4u); sent = 0u;
    tc_toggle_transport(&tc_state); run(540);
    n2 = note_at(1); n4 = note_at(2); n5 = note_at(3);
    /* the next step's note takes the bend as it is then with it (not paced: the note starts where the glide is) */
    assert(is_bend(&out[n5 - 1u], BEND_CH) && bend_value(&out[n5 - 1u]) < -8192 + 200 && out[n5 - 1u].token == out[n5].token);
    uint32_t whole = glides(n4, n5 - 1u, 4096, -8192, 125u + 3u, true);
    assert(whole >= 11u && whole <= 15u);
    bool late = false; /* still moving in the step's last quarter */
    for (uint32_t i = n4; i < n5; ++i) if (is_bend(&out[i], BEND_CH) && out[i].at - out[n4].at >= 94u) late = true;
    assert(late);
    stop();
    glide_lane(0u); sent = 0u;
    tc_toggle_transport(&tc_state); run(540);
    n = bends(BEND_CH, 0u, v, 16u); assert(n >= 4u && v[0] == 4096 && v[1] == 0 && v[2] == -8192 && v[3] == 0);
    assert(!e.lane_gliding);
    stop();
    /* a lane on a shared channel doesn't bend, gliding or not */
    glide_lane(2u); assert(tc_set_item(&tc_state, &e, TC_MENU_MELODY_CH, BEND_CH)); sent = 0u;
    tc_toggle_transport(&tc_state); run(540);
    assert(!every_bend(BEND_CH) && !e.lane_gliding);
    stop(); assert(tc_set_item(&tc_state, &e, TC_MENU_MELODY_CH, 0u));
    /* kept over a restart, with the lane's other settings in the pattern's record */
    ln.lanes[3].glide = 4u; ln_extra_changed(ln_current);
    run(4000); boot_keep();
    assert(ln.lanes[0].glide == 2u && ln.lanes[3].glide == 4u && !ln.lanes[1].glide);
    printf("glide: 4 bits a lane in the pattern record's word 10 (0 to 4 quarters of a step; more, or anything past word "
           "10, refused; all off stores as before 2.6); at half a step a step's note plays and its bend moves there in %u "
           "paced bends within 62 ms (10 ms apart, 16 of 8192 at least, the last exact), sets off from the last step's "
           "value after a rest (centre once more, that value, and the note in one transfer), the first step never glides, "
           "centre at once when stopped on the way; the whole step: %u bends, still moving in its last quarter; Off jumps; "
           "nothing on a shared channel; kept over a restart; ", (unsigned)up, (unsigned)whole);
}

/* --- 3c. 2.6.0: three random lanes of bends, glides, rests and ties, each on its own channel, muted, glided
 * differently and silenced (all notes off) at random while they play: the bends stay clean steps (flat), never flood,
 * stay on the lanes' channels, and are back at centre with every note ended once stopped --- */
static uint32_t stress_seed = 20261113u;
static uint32_t rnd(uint32_t n) { stress_seed = stress_seed * 1664525u + 1013904223u; return (stress_seed >> 8) % n; }
static void stress(void) {
    boot();
    uint32_t most = 0, total = 0, again = 0, gliding = 0; closings = 0u;
    for (uint32_t round = 0; round < 60u; ++round) {
        for (uint32_t i = 0; i < 3u; ++i) {
            ln_lane *l = &ln.lanes[i];
            l->kind = LN_MELODY; l->channel = (uint8_t)(BEND_CH + 1u + i); l->mute = 0u; l->glide = (uint8_t)rnd(LN_GLIDES);
            ln_set_length(l, 1u + rnd(8u)); ln_set_rate(l, 2u + rnd(4u));
            for (uint32_t s = 0; s < 8u; ++s) {
                if (!rnd(4u)) { memset(&l->steps[s], 0, sizeof l->steps[s]); continue; } /* a rest */
                l->steps[s] = tc_brush_default(LN_MELODY); l->steps[s].value = (uint8_t)(3u + rnd(20u));
                l->steps[s].bend = rnd(3u) ? (int16_t)((int32_t)rnd(8192u) + LN_BEND_MIN) : 0; l->steps[s].tie = !rnd(5u);
            }
            for (uint32_t s = 0; s < LN_STEPS; s += LN_CHUNK) ln_changed(i, s);
            ln_changed(i, LN_STEPS);
        }
        ln_extra_changed(ln_current); run(20);
        sent = 0u; tc_toggle_transport(&tc_state);
        for (uint32_t k = 0; k < 6u; ++k) {
            run(80u + rnd(260u));
            if (e.lane_gliding) gliding++;
            uint32_t what = rnd(5u), i = rnd(3u);
            if (what == 0u) { ln.lanes[i].mute ^= 1u; ln_changed(i, LN_STEPS); }
            else if (what == 1u) tc_state.panic = true;
            else if (what == 2u) { ln.lanes[i].glide = (uint8_t)rnd(LN_GLIDES); ln_extra_changed(ln_current); }
        }
        tc_toggle_transport(&tc_state); run(400); quiet();
        assert(sent < LOG && !e.lane_gliding);
        for (uint32_t i = 0; i < LN_LANES; ++i) assert(!e.lane_bent[i]);
        for (uint32_t ch = 0; ch < 16u; ++ch) {
            if (ch < BEND_CH || ch > BEND_CH + 2u) { assert(!every_bend(ch)); continue; }
            again += flat(ch); total += every_bend(ch);
            int32_t last = 0;
            for (uint32_t a = 0; a < sent; ++a) { /* at most 16 bends on a channel in any 100 ms (a glide's 10, the old
                                                   * value again, a step's own, the last exact one) */
                if (!is_bend(&out[a], ch)) continue;
                last = bend_value(&out[a]);
                uint32_t n = 0;
                for (uint32_t b = a; b < sent && out[b].at - out[a].at < 100u; ++b) if (is_bend(&out[b], ch)) n++;
                if (n > most) most = n;
            }
            assert(!last); /* centre once stopped */
        }
        assert(most <= 16u);
    }
    assert(gliding >= 20u && again >= 200u && closings >= 150u); /* 2.6.2: closings on the lanes without Bend glide */
    printf("(2.6.2: %u of the old-value points a step's closing) ", (unsigned)closings);
    stop();
    for (uint32_t i = 0; i < 3u; ++i) { ln_lane *l = &ln.lanes[i]; memset(l, 0, sizeof *l); ln_changed(i, LN_STEPS); }
    printf("stress: 60 rounds of three random lanes (bends, rests, ties, Bend glide 0 to 4 quarters, rates and lengths), muted, "
           "glided differently and silenced at random while playing: %u bends, %u of them the old value once more, each "
           "right before its change in the same transfer and never otherwise; at most %u bends on a channel in any 100 ms; "
           "none on another channel; centre and every note ended once stopped; ", (unsigned)total, (unsigned)again, (unsigned)most);
}

/* --- 4. on the step screen --- */
static void editing(void) {
    boot(); bend_lane(); steps_screen();
    ln_lane *l = &ln.lanes[0];
    tc_state.buttons[0] = TC_ACT_SHIFT;
    hold(step_pad(0));
    centre(0, true); mode_button(TC_MODE_M1 + 6u); centre(0, false); /* Shift + M7: BEND */
    assert(tc_state.step_field == TC_FIELD_BEND); draw(); assert(!strcmp(lines[34], "BEND +1"));
    nav(TC_NAV_RIGHT); assert(l->steps[0].bend == 2253); draw(); assert(!strcmp(lines[34], "BEND +1.1"));
    run(TC_SHIFT_LATCH_MS + 50u); /* two Shift taps close together would latch it (2.0.0) */
    centre(0, true); nav(TC_NAV_LEFT); centre(0, false); /* Shift: a semitone */
    assert(l->steps[0].bend == 205); draw(); assert(!strcmp(lines[34], "BEND +0.1"));
    for (uint32_t i = 0; i < 4u; ++i) nav(TC_NAV_LEFT);
    draw(); assert(!strcmp(lines[34], "BEND -0.3") && l->steps[0].bend == -614);
    assert(l->steps[1].on == false);
    lift(step_pad(0)); assert(l->steps[0].on); /* not toggled */
    /* the steps aren't moved (Shift + left alone moves them) */
    assert(l->steps[3].bend == LN_BEND_MIN && l->steps[2].value == 11u);
    /* Bend range 12: the same bend shows in its semitones; an off step goes on with the picked value and the bend */
    assert(tc_set_item(&tc_state, &e, TC_MENU_BEND_RANGE, 1u));
    hold(step_pad(0)); draw(); assert(!strcmp(lines[34], "BEND -1.8")); lift(step_pad(0));
    hold(step_pad(1)); nav(TC_NAV_RIGHT); lift(step_pad(1));
    assert(l->steps[1].on && l->steps[1].bend == tc_bend_of_tenths(&tc_state, 1) && l->steps[1].bend == 34);
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
    printf("editing: Shift + M7 chooses BEND, tenths with left/right, a semitone with Shift, shown in Bend range's "
           "semitones, an off step on with it, clamped, refused on a shared channel, kept over a restart; ");
}

/* --- 5. 2.5.1, slide: a tie into a step that plays overlaps it --- */
static void slide(void) {
    boot(); bend_lane();
    ln_lane *l = &ln.lanes[0];
    for (uint32_t i = 0; i < 4u; ++i) l->steps[i].bend = 0;
    l->steps[1] = l->steps[0]; l->steps[1].value = 7u; /* step 2 another note */
    l->steps[0].tie = true;                           /* step 1 tied into it */
    l->steps[3].value = l->steps[2].value; l->steps[2].tie = true; /* step 3 tied into the same note */
    for (uint32_t s = 0; s < LN_STEPS; s += LN_CHUNK) ln_changed(0u, s);
    ln_changed(0u, LN_STEPS); run(20);
    sent = 0u; tc_toggle_transport(&tc_state); run(480); tc_toggle_transport(&tc_state); run(100); /* four steps of 125 ms */
    /* step 1's note on, step 2's note on, then step 1's note off (the overlap); step 3's note on, and step 4 (the same
     * note, tied into) no new note on */
    int first = -1, second = -1, first_off = -1, ons = 0;
    uint8_t a = 0, b = 0;
    for (uint32_t i = 0; i < sent; ++i) {
        const packet *k = &out[i];
        if ((k->status & 15u) != BEND_CH) continue;
        if (k->kind == TK_ON) {
            if (first < 0) { first = (int)i; a = k->a; } else if (second < 0) { second = (int)i; b = k->a; }
            ons++;
        }
        if (k->kind == TK_OFF && first >= 0 && first_off < 0 && k->a == a) first_off = (int)i;
    }
    assert(first >= 0 && second > first && a != b && first_off > second); /* the new note before the old one's off */
    assert(ons == 3); /* steps 1, 2, 3: step 4 continues step 3's note */
    quiet();
    printf("slide: a tied note overlaps the next note it runs into (its off after the new on), the same note held over; ");
}

/* --- 6. 2.6.3: CC lanes (lanes.h LN_CC): a step's value as a controller on the lane's channel, no notes; nothing before
 * its first step, the value kept over rests, a stop and a new session; Bend glide slews it, paced, landing exactly; the
 * controller from the lane (0: 1, the mod wheel); its record; a bending lane turned into a CC lane centres first; the
 * step screen's left half picks values up each column --- */
static bool is_cc(const packet *p, uint32_t ch, uint8_t num) { return p->kind == TK_CC && p->status == (0xB0u | ch) && p->a == num; }
static uint32_t ccs(uint32_t ch, uint8_t num, uint32_t from, uint8_t *v, uint32_t n) {
    uint32_t k = 0;
    for (uint32_t i = from; i < sent; ++i) if (is_cc(&out[i], ch, num) && k < n) v[k++] = out[i].b;
    return k;
}
static void cc_lanes(void) {
    boot();
    ln_lane *l = &ln.lanes[1];
    l->kind = LN_CC; l->channel = BEND_CH + 1u; l->cc = 74u; ln_set_length(l, 4u);
    for (uint32_t i = 0; i < 4u; ++i) l->steps[i] = tc_brush_default(LN_CC);
    assert(l->steps[0].value == 64u);
    l->steps[0].value = 10u; l->steps[1].value = 64u; memset(&l->steps[2], 0, sizeof l->steps[2]); l->steps[3].value = 127u;
    for (uint32_t s = 0; s < LN_STEPS; s += LN_CHUNK) ln_changed(1u, s);
    ln_changed(1u, LN_STEPS); run(20);
    sent = 0u; run(300); assert(!sent); /* stopped: nothing */
    tc_toggle_transport(&tc_state); run(1010); /* two rounds and a bit */
    uint8_t v[64]; uint32_t n = ccs(BEND_CH, 74u, 0u, v, 64u);
    assert(n >= 6u && v[0] == 10u && v[1] == 64u && v[2] == 127u && v[3] == 10u && v[4] == 64u && v[5] == 127u); /* the rest keeps 64 */
    for (uint32_t i = 0; i < sent; ++i) {
        assert(out[i].kind != TK_ON && out[i].kind != TK_BEND); /* no notes, no bends */
        if (out[i].kind == TK_CC) assert(is_cc(&out[i], BEND_CH, 74u));
    }
    uint32_t first = 0, second = 0;
    for (uint32_t i = 0; i < sent; ++i) if (is_cc(&out[i], BEND_CH, 74u)) { if (!first) first = out[i].at; else if (!second) { second = out[i].at; break; } }
    assert(near(second - first, 125u, 3u)); /* a 1/16 step at 120 BPM */
    /* stopped: the value stays (nothing sent); a new session leaves it too, and the next start goes on from it */
    uint32_t from = sent; tc_toggle_transport(&tc_state); run(400); assert(!ccs(BEND_CH, 74u, from, v, 64u));
    tp_loss(&e); run(1500); assert(e.state == TK_ACTIVE && !ccs(BEND_CH, 74u, from, v, 64u));
    /* the controller: 0 is 1 (the mod wheel); a change sends the value as the new one at the next step */
    l->cc = 0u; ln_changed(1u, LN_STEPS); from = sent; tc_toggle_transport(&tc_state); run(200);
    assert(ccs(BEND_CH, 1u, from, v, 64u) >= 1u && v[0] == 10u && !ccs(BEND_CH, 74u, from, v, 64u));
    stop();
    /* the record: chunk 0's word 35 byte 1; 120 and up refused */
    uint32_t w[LN_WORDS]; ln_pattern t; memcpy(&t, &ln, sizeof t);
    t.lanes[1].cc = 119u; ln_encode(&t, LN_CHUNKS, w); assert(((w[35] >> 8) & 0xFFu) == 119u);
    ln_pattern u; memset(&u, 0, sizeof u); assert(ln_decode(w, LN_CHUNKS, &u) && u.lanes[1].cc == 119u && u.lanes[1].kind == LN_CC);
    w[35] = (w[35] & 0xFFu) | 120u << 8; assert(!ln_decode(w, LN_CHUNKS, &u));
    assert(ln_cc(&t.lanes[1]) == 119u && ln_cc(&ln.lanes[1]) == 1u);
    /* Bend glide on a CC lane: slewed from the last step's value, 10 ms apart at least, ending exactly on each step's */
    l->cc = 74u; l->glide = 2u; ln_changed(1u, LN_STEPS); /* half a step: there before the next */ ln_extra_changed(ln_current); run(20);
    sent = 0u; tc_toggle_transport(&tc_state); run(1010);
    n = ccs(BEND_CH, 74u, 0u, v, 64u);
    assert(n >= 20u && v[0] == 10u);
    uint32_t last_at = 0, k = 0, soon = 0; uint8_t last = 0; bool up = false, down = false, landed127 = false;
    for (uint32_t i = 0; i < sent; ++i) {
        if (!is_cc(&out[i], BEND_CH, 74u)) continue;
        if (k) { soon += out[i].at - last_at < TP_MPE_BEND_MS; up |= out[i].b > last; down |= out[i].b < last; }
        landed127 |= out[i].b == 127u;
        last = out[i].b; last_at = out[i].at; k++;
    }
    assert(up && down && landed127 && soon <= 9u); /* paced, but for a step's first move (as a bend's glide) */
    stop(); l->glide = 0u; ln_extra_changed(ln_current);
    /* a lane that bends turned into a CC lane: its bend back to centre first, then values */
    ln_lane *m = &ln.lanes[0];
    m->kind = LN_MELODY; m->channel = BEND_CH + 2u; ln_set_length(m, 1u); m->steps[0] = tc_brush_default(LN_MELODY); m->steps[0].bend = 2048;
    for (uint32_t s2 = 0; s2 < LN_STEPS; s2 += LN_CHUNK) ln_changed(0u, s2);
    ln_changed(0u, LN_STEPS); l->kind = LN_OFF; ln_changed(1u, LN_STEPS); run(20);
    sent = 0u; tc_toggle_transport(&tc_state); run(100);
    assert(e.lane_bent[0] == 4096);
    from = sent; m->kind = LN_CC; m->cc = 7u; m->steps[0].value = 99u; ln_changed(0u, LN_STEPS); run(300);
    bool centred = false; uint32_t cc_at = 0;
    for (uint32_t i = from; i < sent; ++i) {
        if (out[i].kind == TK_BEND && (out[i].status & 15u) == BEND_CH + 1u && out[i].a == 0u && out[i].b == 64u && !cc_at) centred = true;
        if (is_cc(&out[i], BEND_CH + 1u, 7u) && !cc_at) { cc_at = i; assert(out[i].b == 99u); }
    }
    assert(centred && cc_at);
    stop();
    memset(m, 0, sizeof *m); memset(l, 0, sizeof *l); ln_changed(0u, LN_STEPS); ln_changed(1u, LN_STEPS); run(20);
    /* the step screen: the left half's pads are the values, 0 at the bottom left, up each column, 127 at the top right */
    assert(ln_pad_value(LN_CC, 3u) == 0u && ln_pad_value(LN_CC, 2u) == 2u && ln_pad_value(LN_CC, 44u) == 127u);
    for (uint32_t pad = 1; pad < 48u; ++pad) {
        uint32_t a2 = 4u * (pad / 4u) + 3u - pad % 4u, b2 = a2 - 1u; /* the pad below it in value order */
        uint32_t below = 4u * (b2 / 4u) + 3u - b2 % 4u;
        if (a2) assert(ln_pad_value(LN_CC, pad) > ln_pad_value(LN_CC, below) || ln_pad_value(LN_CC, pad) == ln_pad_value(LN_CC, below) + 0u);
    }
    assert(!strcmp(ui_kind_names[LN_CC], "CC"));
    /* 2.6.4: the Channel item shows what a CC lane sends on: AUTO is the melody channel */
    l->kind = LN_CC; tc_state.lane = 1u; tc_state.melody_ch = 6u;
    assert(!strcmp(ui_value_text(TC_MENU_LANE_CHANNEL, 0u), "AUTO 7") && !strcmp(ui_value_text(TC_MENU_LANE_CHANNEL, 5u), "5"));
    tc_state.melody_ch = 0u; tc_state.lane = 0u; memset(l, 0, sizeof *l); ln_changed(1u, LN_STEPS); run(20);
    printf("cc lanes: values on the lane's channel and controller at each step (no notes, no bends), nothing before the first "
           "step or while stopped, kept over a rest, a stop and a new session, controller 0 as 1, the record (119 kept, 120 "
           "refused), Bend glide slews it paced and lands exactly, a bending lane centred before its values, the step screen's "
           "values up each column; ");
}

/* --- 7. 2.6.3 (bench C3): a fresh lane whose first step is off: its first closing waits until the lane has sounded on its
 * channel (2.6.2 sent it on channel 1, where nothing had bent) --- */
static void first_rest(void) {
    boot();
    ln_lane *l = &ln.lanes[0];
    l->kind = LN_MELODY; l->channel = BEND_CH + 1u; ln_set_length(l, 4u);
    memset(&l->steps[0], 0, sizeof l->steps[0]);
    l->steps[1] = tc_brush_default(LN_MELODY); l->steps[1].bend = 614;
    for (uint32_t s = 0; s < LN_STEPS; s += LN_CHUNK) ln_changed(0u, s);
    ln_changed(0u, LN_STEPS); run(20);
    sent = 0u; tc_toggle_transport(&tc_state); run(1100);
    for (uint32_t ch = 0; ch < 16u; ++ch) if (ch != BEND_CH) assert(!every_bend(ch));
    closings = 0u; (void)flat(BEND_CH); assert(closings >= 2u); /* later rounds close on the lane's own channel */
    stop();
    memset(l, 0, sizeof *l); ln_changed(0u, LN_STEPS); run(20);
    printf("first rest: no bend on another channel before the lane has sounded, closings on its own after; ");
}

/* --- 8. 2.7.1 (review TBR-037): a CC lane whose controller or channel changes (another pattern's CC lane in its place,
 * its controller edited) sends nothing until its next step sounds: the old value never goes out as the new controller
 * or on the old channel --- */
static void cc_switch(void) {
    boot();
    for (uint32_t p = 0; p < 2u; ++p) for (uint32_t i = 0; i < LN_LANES; ++i) ln_bank[p].lanes[i].kind = LN_OFF;
    ln_lane *a = &ln_bank[0].lanes[2], *b = &ln_bank[1].lanes[2];
    a->kind = LN_CC; a->channel = 5u; a->cc = 74u; ln_set_rate(a, 3u); ln_set_length(a, 16u); /* 1/16: a round 2 s */
    for (uint32_t s = 0; s < 16u; ++s) { a->steps[s] = tc_brush_default(LN_CC); a->steps[s].value = 20u; }
    b->kind = LN_CC; b->channel = 2u; b->cc = 7u; ln_set_rate(b, 3u); ln_set_length(b, 16u);
    for (uint32_t s = 0; s < 16u; ++s) memset(&b->steps[s], 0, sizeof b->steps[s]);
    for (uint32_t s = 8u; s < 16u; ++s) { b->steps[s] = tc_brush_default(LN_CC); b->steps[s].value = 127u; }
    for (uint32_t s = 0; s <= LN_STEPS; s += LN_CHUNK) { ln_changed_in(0u, 2u, s); ln_changed_in(1u, 2u, s); }
    ln_select(0u); run(20);
    tc_toggle_transport(&tc_state); run(400);
    uint8_t v[64];
    assert(ccs(4u, 74u, 0u, v, 64u) >= 1u && v[0] == 20u);
    uint32_t from = sent; ln_queued = 1u; run(3000); /* pattern 2 chosen: it plays from the end of pattern 1's round */
    assert(ln_current == 1u);
    uint32_t right = 0u;
    for (uint32_t i = from; i < sent; ++i) {
        if (out[i].kind != TK_CC) continue;
        bool old_lane = is_cc(&out[i], 4u, 74u), new_lane = is_cc(&out[i], 1u, 7u);
        assert(old_lane || new_lane); /* c08a3ce: CC 7 = 20 on channel 5 at the switch */
        if (new_lane) { assert(out[i].b == 127u); right++; }
    }
    assert(right >= 1u);
    /* one pattern, the playing lane's controller edited: nothing on the new controller until a step sounds, then its own value */
    stop(); ln_select(0u); run(20);
    ln_lane *l = &ln_bank[0].lanes[2];
    for (uint32_t s = 0; s < 16u; ++s) l->steps[s].value = 30u;
    for (uint32_t s = 2u; s < 16u; ++s) memset(&l->steps[s], 0, sizeof l->steps[s]); /* steps 1-2 on: 30, then rests */
    for (uint32_t s = 0; s <= LN_STEPS; s += LN_CHUNK) ln_changed_in(0u, 2u, s);
    tc_toggle_transport(&tc_state); run(300); /* in step 3, a rest: 30 sent */
    assert(e.lane_bent[2] == 30);
    l->steps[0].value = 90u; ln_changed_in(0u, 2u, 0u); run(5);
    from = sent; l->cc = 11u; ln_changed_in(0u, 2u, LN_STEPS); run(800); /* to the rests: no step sounds */
    assert(!ccs(4u, 11u, from, v, 64u)); /* c08a3ce: CC 11 = 30 at once */
    run(1400); /* the next round's step 1 */
    assert(ccs(4u, 11u, from, v, 64u) >= 1u && v[0] == 90u);
    stop();
    /* 2.8.1 (review TBR-043): a pattern chosen while the lanes are stopped is taken at once, and nothing is sent: not
     * the old value as the new CC lane's controller (2.8: CC 7 = 90 on channel 5), nor as controller 1 when the new
     * pattern's lane there is no CC lane (2.8: CC 1 = 127 on channel 2) */
    from = sent; tc_choose_pattern(&tc_state, 1u); run(300);
    assert(ln_current == 1u && !tc_state.transport);
    for (uint32_t i = from; i < sent; ++i) assert(out[i].kind != TK_CC);
    tc_toggle_transport(&tc_state); run(1400); assert(e.lane_bent[2] == 127); stop(); /* pattern 2's lane has sounded */
    assert(ln_bank[2].lanes[2].kind == LN_CHORDS);
    from = sent; tc_choose_pattern(&tc_state, 2u); run(300);
    assert(ln_current == 2u);
    for (uint32_t i = from; i < sent; ++i) assert(out[i].kind != TK_CC);
    for (uint32_t p = 0; p < 2u; ++p) { ln_clear(&ln_bank[p]); for (uint32_t s = 0; s <= LN_STEPS; s += LN_CHUNK) ln_changed_in(p, 2u, s); }
    ln_select(0u); run(20);
    printf("cc switch: a CC lane's new controller or channel sends nothing until its first step sounds, nor does a "
           "pattern chosen while stopped; ");
}

int main(void) {
    (void)heard; (void)drums; (void)touch; (void)root_hold; (void)root_button; (void)mode_hold; /* test_v24.c's harness */
    units();
    own_channel();
    playing();
    glide(); /* 2.6.0 */
    stress();
    editing();
    slide(); /* 2.5.1 */
    cc_lanes(); /* 2.6.3 */
    first_rest(); /* 2.6.3 */
    cc_switch(); /* 2.7.1, TBR-037; 2.8.1, TBR-043 */
    printf("bend PASS\n");
    return 0;
}
