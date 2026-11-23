/* SPDX-License-Identifier: GPL-3.0-or-later - 2.6.8, the Toraiz Squid's sequencer ideas (a user's request): a lane's
 * running directions (six paths over 4 x 4 blocks, Mirror, Reverse, Ping-pong), a step's Lasts and Skip (lanes.h
 * ln_timeline), Stutter 1/32, Triplet, Dotted and a Stutter kept by a double tap, Half and Double speed while held. The
 * real sources (tests/tb/sources.json); every constant from the headers. */
#include "../../src/tb/controls.h"
#include "../../src/tb/engine.h"
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
bool tl_init(void) { return true; }
void tl_set_pad(uint32_t pad, tl_rgb c) { (void)pad; (void)c; }
void tl_set_button(uint32_t channel, uint16_t level) { (void)channel; (void)level; }
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
static uint32_t on_at[LOG], logged, off_at[128];
static bool writer(const uint8_t p[4], uint32_t token) {
    (void)token;
    uint32_t ch = p[1] & 15u;
    if (p[0] == TK_ON) {
        assert(!sounding[ch][p[2]]); sounding[ch][p[2]] = 1u;
        if (logged < LOG) { on_note[logged] = p[2]; on_ch[logged] = (uint8_t)ch; on_at[logged++] = now; }
    }
    if (p[0] == TK_OFF) { assert(sounding[ch][p[2]]); sounding[ch][p[2]] = 0u; off_at[p[2]] = now; }
    return true;
}
static const uint8_t addresses[4] = {0x35, 0x36, 0x38, 0x39};
static uint32_t raw[4];
static uint32_t client(uint8_t a) { for (uint32_t i = 0; i < 4u; ++i) if (addresses[i] == a) return i; assert(0); return 0; }
static struct bi_reply reader(void *ctx, uint8_t a, uint8_t reg, uint32_t gen, uint32_t stamp, uint32_t budget) {
    (void)ctx; (void)reg; (void)budget;
    uint32_t i = client(a);
    return (struct bi_reply){.status = 0, .count = 2, .generation = gen, .timestamp_ms = stamp, .elapsed_us = 500,
                             .data = {(uint8_t)raw[i], (uint8_t)(raw[i] >> 8)}};
}
static void run(uint32_t ms) {
    for (uint32_t i = 0; i < ms; ++i, ++now) {
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
static const uint8_t fn_bit[TC_FUNCTIONS] = {6, 5, 0, 1, 7, 4, 3, 8, 9, 10, 11, 12, 14, 2};
static const uint8_t top_bit[12] = {3, 1, 0, 4, 5, 6, 7, 8, 9, 10, 11, 12};
static const uint8_t nav_bit[5] = {8, 7, 6, 1, 3}; /* left, up, centre, down, right */
static void hold_bit(uint8_t a, uint8_t bit, bool down) {
    if (down) raw[client(a)] |= 1u << bit; else raw[client(a)] &= ~(1u << bit);
    run(50);
}
static void centre(uint32_t i, bool down) { hold_bit(0x35, fn_bit[i], down); }
static void nav(uint32_t n) { hold_bit(0x36, nav_bit[n], true); hold_bit(0x36, nav_bit[n], false); }
static void mode_button(uint32_t i) { hold_bit(0x38, top_bit[i], true); hold_bit(0x38, top_bit[i], false); }
static void hold(uint32_t k) { scan.values[k] = 3000u; run(60); }
static void lift(uint32_t k) { scan.values[k] = 0u; run(60); }
static void touch(uint32_t k) { hold(k); lift(k); }
static void draw(void) { tc_state.event_sequence++; ui_service(&tc_state, &e, &scan, true, now); }
static uint32_t step_pad(uint32_t s) {
    for (uint32_t k = 48u; k < 96u; ++k) if (tc_step_of_pad(&tc_state, k - 48u) == s) return k;
    assert(0); return 0;
}
static void boot(void) {
    tc_init(&tc_state); memset(&e, 0, sizeof e); memset(&scan, 0, sizeof scan); memset(raw, 0, sizeof raw);
    memset(sounding, 0, sizeof sounding); logged = 0u;
    tm_reset(); ln_clear(&ln);
    sq_clear(&sq, SQ_UNIT_BAR, 0u);
    fs_erase(); (void)st_load(&tc_state);
    run(600); assert(e.state == TK_ACTIVE);
}
static void stop(void) {
    if (tc_state.transport) tc_toggle_transport(&tc_state);
    run(400);
    for (uint32_t ch = 0; ch < 16u; ++ch) for (uint32_t p = 0; p < 128u; ++p) assert(!sounding[ch][p]);
}
/* A drum lane of n steps on lane 0, step i playing note 36 + i (marked changed, as an edit is). */
static void drums(uint32_t n) {
    ln_lane *l = &ln.lanes[0];
    l->kind = LN_DRUMS; ln_set_length(l, n);
    for (uint32_t i = 0; i < n; ++i) { l->steps[i] = tc_brush_default(LN_DRUMS); l->steps[i].value = (uint8_t)(36u + i); }
    for (uint32_t s = 0; s < LN_STEPS; s += LN_CHUNK) ln_changed(0, s);
    ln_changed(0, LN_STEPS);
}
static uint32_t drum_ch(void) { return tc_state.drum_ch & 15u; }
/* The drum notes from log entry `from` on, in order (n at most), and their times. */
static uint32_t heard(uint32_t from, uint8_t *notes, uint32_t *at, uint32_t n) {
    uint32_t k = 0;
    for (uint32_t i = from; i < logged && k < n; ++i) if (on_ch[i] == drum_ch()) { notes[k] = on_note[i]; if (at) at[k] = on_at[i]; k++; }
    return k;
}
static void changed(uint32_t lane) { for (uint32_t s = 0; s < LN_STEPS; s += LN_CHUNK) ln_changed(lane, s); ln_changed(lane, LN_STEPS); }
/* The orders a lane of n steps plays from window step first, its direction d (one time round). */
static uint32_t orders(uint32_t n, uint32_t first, uint8_t d, uint8_t *out) {
    ln_lane l; memset(&l, 0, sizeof l); ln_set_length(&l, first + n); l.loop_from = (uint8_t)first; l.direction = d;
    ln_timeline t; ln_timeline_build(&l, 0, 0, 0, &t);
    memcpy(out, t.order, t.visits);
    return t.visits;
}
static bool order_is(uint32_t n, uint32_t first, uint8_t d, const uint8_t *want, uint32_t count) {
    uint8_t got[LN_VISITS];
    return orders(n, first, d, got) == count && !memcmp(got, want, count);
}

/* --- 1. the running directions --- */
static void paths(void) {
    /* The list: 49 directions, each once, every one valid; the first nine as before 2.6.8 and the five new paths. */
    bool seen[64] = {false};
    for (uint32_t v = 0; v < LN_DIRECTION_VALUES; ++v) {
        uint8_t d = ln_direction_at(v);
        assert(ln_direction_ok(d) && !seen[d] && ln_direction_index(d) == v);
        seen[d] = true;
    }
    uint32_t ok = 0;
    for (uint32_t d = 0; d < 256u; ++d) ok += ln_direction_ok((uint8_t)d);
    assert(ok == LN_DIRECTION_VALUES); /* nothing valid left out of the list */
    static const uint8_t first9[9] = {LN_ROWS, LN_ROWS | LN_REVERSE, LN_ROWS | LN_PINGPONG, LN_RANDOM, LN_COLUMNS, LN_SNAKE,
                                      LN_SPIRAL, LN_SPIRAL_DOWN, LN_STAGGER};
    for (uint32_t v = 0; v < 9u; ++v) assert(ln_direction_at(v) == first9[v]);
    assert(!ln_direction_ok(7u) && !ln_direction_ok(LN_RANDOM | LN_MIRROR) && !ln_direction_ok(64u));
    /* The paths over 16 steps, as the Squid's manual numbers its pads (1-4 the top row; here from 0). */
    static const uint8_t columns[16] = {0, 4, 8, 12, 1, 5, 9, 13, 2, 6, 10, 14, 3, 7, 11, 15};
    static const uint8_t snake[16] = {0, 1, 2, 3, 7, 6, 5, 4, 8, 9, 10, 11, 15, 14, 13, 12};
    static const uint8_t spiral[16] = {0, 1, 2, 3, 7, 11, 15, 14, 13, 12, 8, 4, 5, 6, 10, 9};
    static const uint8_t down[16] = {0, 4, 8, 12, 13, 14, 15, 11, 7, 3, 2, 1, 5, 9, 10, 6};
    static const uint8_t stagger[16] = {0, 2, 1, 3, 4, 6, 5, 7, 8, 10, 9, 11, 12, 14, 13, 15};
    static const uint8_t mirrored[16] = {3, 2, 1, 0, 7, 6, 5, 4, 11, 10, 9, 8, 15, 14, 13, 12};
    assert(order_is(16, 0, LN_COLUMNS, columns, 16) && order_is(16, 0, LN_SNAKE, snake, 16) && order_is(16, 0, LN_SPIRAL, spiral, 16));
    assert(order_is(16, 0, LN_SPIRAL_DOWN, down, 16) && order_is(16, 0, LN_STAGGER, stagger, 16) && order_is(16, 0, LN_MIRROR, mirrored, 16));
    /* Mirror: each row right to left (a mirrored spiral starts top right, going left). */
    static const uint8_t spiral_mirror[16] = {3, 2, 1, 0, 4, 8, 12, 13, 14, 15, 11, 7, 6, 5, 9, 10};
    assert(order_is(16, 0, LN_SPIRAL | LN_MIRROR, spiral_mirror, 16));
    /* Reverse: backwards; Ping-pong: there and back, the ends once; both: from the end and back. */
    uint8_t want[LN_VISITS];
    for (uint32_t i = 0; i < 16u; ++i) want[i] = snake[15u - i];
    assert(order_is(16, 0, LN_SNAKE | LN_REVERSE, want, 16));
    for (uint32_t i = 0; i < 16u; ++i) want[i] = columns[i];
    for (uint32_t i = 0; i < 14u; ++i) want[16u + i] = columns[14u - i];
    assert(order_is(16, 0, LN_COLUMNS | LN_PINGPONG, want, 30));
    /* A last block of fewer steps keeps the path's order of those it has; blocks of 16 from the window's first step. */
    static const uint8_t snake8[8] = {0, 1, 2, 3, 7, 6, 5, 4}, spiral8[8] = {0, 1, 2, 3, 7, 4, 5, 6}, columns6[6] = {0, 4, 1, 5, 2, 3};
    assert(order_is(8, 0, LN_SNAKE, snake8, 8) && order_is(8, 0, LN_SPIRAL, spiral8, 8) && order_is(6, 0, LN_COLUMNS, columns6, 6));
    static const uint8_t window[8] = {4, 5, 6, 7, 11, 10, 9, 8};
    assert(order_is(8, 4, LN_SNAKE, window, 8));
    uint8_t got[LN_VISITS];
    assert(orders(20, 0, LN_COLUMNS, got) == 20u && !memcmp(got, columns, 16) && got[16] == 16u && got[19] == 19u);
    static const uint8_t mirror6[6] = {3, 2, 1, 0, 5, 4};
    assert(order_is(6, 0, LN_MIRROR, mirror6, 6));
    /* Every direction over every length visits every window step (ping-pong: the inner ones twice). */
    for (uint32_t v = 0; v < LN_DIRECTION_VALUES; ++v) for (uint32_t n = 1; n <= LN_STEPS; ++n) {
        uint8_t d = ln_direction_at(v);
        uint32_t count = orders(n, 0, d, got), times[LN_STEPS] = {0};
        assert(count == ((d & LN_PINGPONG) && n > 1u ? 2u * n - 2u : n));
        for (uint32_t i = 0; i < count; ++i) { assert(got[i] < n); times[got[i]]++; }
        for (uint32_t s = 0; s < n; ++s) assert(times[s] >= 1u && times[s] <= 2u);
    }
    /* The names, as the board shows them; the step screen's U2 line shortens DIRECTION to DIR when the name is long. */
    static const struct { uint8_t d; const char *name; } names[] = {
        {LN_ROWS, "FORWARD"}, {LN_ROWS | LN_REVERSE, "REVERSE"}, {LN_ROWS | LN_PINGPONG, "PING-PONG"}, {LN_RANDOM, "RANDOM"},
        {LN_ROWS | LN_PINGPONG | LN_REVERSE, "PING-PONG REV"}, {LN_ROWS | LN_MIRROR | LN_REVERSE, "FORWARD MIR REV"},
        {LN_SPIRAL_DOWN | LN_MIRROR | LN_REVERSE | LN_PINGPONG, "SPIRAL DN MIR REV PP"}, {LN_STAGGER, "STAGGER"}};
    for (uint32_t i = 0; i < sizeof names / sizeof names[0]; ++i)
        assert(!strcmp(ui_value_text(TC_MENU_LANE_DIRECTION, (uint8_t)ln_direction_index(names[i].d)), names[i].name));
    boot();
    tc_state.lane = 0u;
    assert(tc_set_item(&tc_state, &e, TC_MENU_LANE_DIRECTION, 5u) && ln.lanes[0].direction == LN_SNAKE);
    assert(tc_menu_values(TC_MENU_LANE_DIRECTION) == LN_DIRECTION_VALUES && tc_menu_value(&tc_state, TC_MENU_LANE_DIRECTION) == 5u);
    ln.lanes[0].direction = LN_SPIRAL_DOWN | LN_MIRROR | LN_REVERSE; /* from the editor */
    ui_lane_setting(&tc_state, TC_MENU_LANE_DIRECTION); assert(!strcmp(ui_buffer, "DIR SPIRAL DN MIR REV"));
    ln.lanes[0].direction = LN_SNAKE;
    ui_lane_setting(&tc_state, TC_MENU_LANE_DIRECTION); assert(!strcmp(ui_buffer, "DIRECTION SNAKE"));
    /* Stored in the lane record; the record refuses a direction that isn't one. */
    ln_pattern back; ln_clear(&back);
    ln.lanes[0].direction = LN_STAGGER | LN_MIRROR | LN_PINGPONG;
    uint32_t w[LN_WORDS]; ln_encode(&ln, 0, w); assert(ln_decode(w, 0, &back) && back.lanes[0].direction == ln.lanes[0].direction);
    w[34] = (w[34] & ~0xFFu) | (LN_RANDOM | LN_REVERSE); assert(!ln_decode(w, 0, &back));
    /* Played: a snake over 8 drum steps (36..43): 36 37 38 39 43 42 41 40, round and round. */
    ln_clear(&ln); drums(8); ln.lanes[0].direction = LN_SNAKE; changed(0);
    tc_toggle_transport(&tc_state); run(2100);
    uint8_t notes[16]; assert(heard(0, notes, NULL, 16) == 16u);
    static const uint8_t snake_notes[16] = {36, 37, 38, 39, 43, 42, 41, 40, 36, 37, 38, 39, 43, 42, 41, 40};
    assert(!memcmp(notes, snake_notes, 16));
    /* Changed while it plays (set straight, as the native suites do; the editor's records bump ln_changes): the spiral. */
    ln.lanes[0].direction = LN_SPIRAL; uint32_t from = logged; run(1100);
    uint8_t later[8]; assert(heard(from, later, NULL, 8) == 8u);
    bool spiral_heard = false; /* 36 37 38 39 43 40 41 42 from wherever it came in */
    for (uint32_t i = 0; i + 2u < 8u; ++i) spiral_heard = spiral_heard || (later[i] == 39 && later[i + 1u] == 43 && later[i + 2u] == 40);
    assert(spiral_heard);
    stop();
    printf("directions: 49 (the first nine as listed), the six paths over 4 x 4 as the Squid's, Mirror, Reverse, Ping-pong, "
           "short last blocks, every one over every length, names (DIR when long), stored, played; ");
}

/* --- 2. a step's Lasts and Skip --- */
static void timeline(void) {
    ln_lane l; memset(&l, 0, sizeof l); ln_set_length(&l, 4u);
    ln_timeline t; ln_timeline_build(&l, 0, 0, 0, &t);
    assert(t.visits == 4u && t.units == 4u && t.at[3] == 3u);
    l.steps[1].time = 2u; /* step 2 lasts 3 steps: the round is 6 steps, step 3 starts at 4 */
    ln_timeline_build(&l, 0, 0, 0, &t);
    assert(t.visits == 4u && t.units == 6u && t.at[0] == 0u && t.at[1] == 1u && t.at[2] == 4u && t.at[3] == 5u && ln_units(&l) == 6u);
    uint32_t lasts;
    assert(ln_unit_in(&t, &l, 0, 0, 5u, &lasts) == 7u && lasts == 3u); /* the second round's step 2 */
    rh_rate rate = tc_arp_rates[LN_RATE_DEFAULT];
    uint32_t len, step = rh_step_start(1u, rate);
    assert(ln_start_in(&t, &l, NULL, 0, 0, 1u, rate, &len) == step && len == 3u * step);
    /* The visit at a position: steps 2-4 of the round are step 2's (visit 1), the 5th step 3's. */
    uint32_t c, at;
    assert(ln_count_at_in(&t, &l, NULL, 0, 0, rh_step_start(3u, rate), rate, &c, &at) && c == 1u && at == step);
    assert(ln_count_at_in(&t, &l, NULL, 0, 0, rh_step_start(4u, rate), rate, &c, &at) && c == 2u);
    assert(ln_count_at_in(&t, &l, NULL, 0, 0, rh_step_start(6u, rate), rate, &c, &at) && c == 4u && ln_order_in(&t, &l, 0, 0, c, NULL) == 0u);
    /* Skip: step 3 taken out: 3 visits, the round 5 steps; every step skipped: nothing to play. */
    l.steps[2].time = LN_SKIP; ln_timeline_build(&l, 0, 0, 0, &t);
    assert(t.visits == 3u && t.units == 5u && t.order[2] == 3u && ln_cycle(&l) == 3u);
    for (uint32_t i = 0; i < 4u; ++i) l.steps[i].time = LN_SKIP;
    ln_timeline_build(&l, 0, 0, 0, &t);
    assert(!t.visits && !t.units && !ln_count_at(&l, NULL, 0, 0, 5000u, rate, &c, &at));
    /* Ping-pong with a step lasting: it lasts each time it is visited; Random: every step not skipped once a round, the
     * round's time the same each round. */
    memset(&l, 0, sizeof l); ln_set_length(&l, 4u); l.direction = LN_PINGPONG; l.steps[1].time = 1u;
    assert(ln_cycle(&l) == 6u && ln_units(&l) == 8u);
    l.direction = LN_RANDOM; ln_set_length(&l, 16u); l.steps[5].time = LN_SKIP; l.steps[9].time = 3u;
    for (uint32_t round = 0; round < 20u; ++round) {
        ln_timeline_build(&l, 3u, 99u, round, &t);
        bool seen[LN_STEPS] = {false};
        assert(t.visits == 15u && t.units == 19u && t.round == round); /* 15 steps, step 10 lasting 4, step 2 still 2 */
        for (uint32_t i = 0; i < t.visits; ++i) { assert(t.order[i] != 5u && !seen[t.order[i]]); seen[t.order[i]] = true; }
        for (uint32_t i = 0; i < t.visits; ++i) assert(ln_order(&l, 3u, 99u, round * 15u + i, NULL) == t.order[i]);
    }
    /* Positions over a random lane's rounds: each the last visit started (the timeline kept for another round). */
    ln_timeline keep; ln_timeline_build(&l, 3u, 99u, 0u, &keep);
    for (uint32_t pos = 0; pos < rh_step_start(200u, rate); pos += 101u) {
        assert(ln_count_at_in(&keep, &l, NULL, 3u, 99u, pos, rate, &c, &at));
        assert((int32_t)(pos - at) >= 0 && (int32_t)(pos - ln_start_in(&keep, &l, NULL, 3u, 99u, c + 1u, rate, NULL)) < 0);
    }
    /* Stored: word 37 the lasts, word 38 the skips; a skip past the chunk's 16 steps, or word 39, refused. */
    ln_pattern p, back; ln_clear(&p); ln_clear(&back);
    p.lanes[2].steps[17].time = 3u; p.lanes[2].steps[18].time = LN_SKIP | 1u; p.lanes[2].steps[31].time = LN_SKIP;
    uint32_t w[LN_WORDS]; ln_encode(&p, 2u * LN_CHUNKS + 1u, w);
    assert(w[37] == (3u << 2 | 1u << 4) && w[38] == (1u << 2 | 1u << 15) && !ln_chunk_default(&p, 2u * LN_CHUNKS + 1u));
    assert(ln_decode(w, 2u * LN_CHUNKS + 1u, &back) && !memcmp(&back.lanes[2], &p.lanes[2], sizeof p.lanes[2]));
    w[38] |= 1u << 16; assert(!ln_decode(w, 2u * LN_CHUNKS + 1u, &back)); w[38] &= 0xFFFFu;
    w[39] = 1u; assert(!ln_decode(w, 2u * LN_CHUNKS + 1u, &back));
    /* The pattern's time round counts them: 4 steps of 1/16 with one lasting 4: 7 steps. */
    ln_clear(&p); p.lanes[0].kind = LN_DRUMS; ln_set_length(&p.lanes[0], 4u);
    for (uint32_t i = 0; i < 4u; ++i) p.lanes[0].steps[i].on = true;
    p.lanes[0].steps[2].time = 3u;
    assert(tp_pattern_length(&p) == rh_step_start(7u, rate));
    for (uint32_t i = 0; i < 4u; ++i) p.lanes[0].steps[i].time = LN_SKIP;
    assert(tp_pattern_length(&p) == 4u * 24u * RH_SUB); /* nothing to play: a bar, as an empty pattern */
    /* Played (1/16 at 120 BPM, 125 ms a step): step 2 lasting 2 steps, step 3 skipped: 36, 37 (250 ms), 39, round. */
    boot(); drums(4); ln.lanes[0].steps[1].time = 1u; ln.lanes[0].steps[2].time = LN_SKIP; changed(0);
    tc_toggle_transport(&tc_state); run(1200);
    uint8_t notes[8]; uint32_t times[8];
    assert(heard(0, notes, times, 8) == 8u);
    static const uint8_t want[8] = {36, 37, 39, 36, 37, 39, 36, 37};
    assert(!memcmp(notes, want, 8));
    int32_t one = (int32_t)(times[1] - times[0]), two = (int32_t)(times[2] - times[1]), round = (int32_t)(times[3] - times[0]);
    assert(one >= 123 && one <= 127 && two >= 248 && two <= 252 && round >= 498 && round <= 502);
    /* Its gate is of its whole length: 50 % of two steps, 125 ms (a note of one step, 62). */
    run(30); uint32_t from = logged; run(600);
    uint32_t n = heard(from, notes, times, 8), k = 0;
    while (k < n && notes[k] != 37u) k++;
    assert(k < n); run(300);
    int32_t held = (int32_t)(off_at[37] - times[k]);
    assert(held >= 120 && held <= 130);
    stop();
    printf("lasts and skips: the timeline (visits, steps a round, where each starts), the visit at a position, ping-pong, "
           "random rounds, stored in words 37-38, the pattern's round, played (37 two steps long, 38 left out, its gate); ");
}

/* --- 3. Lasts and Skip on the step screen, the editor's controller slots and named functions --- */
static void editing(void) {
    boot(); drums(8);
    tc_state.lane = 0u; tc_open_steps(&tc_state);
    /* Shift + M1: LASTS; Shift + M2: SKIP (M1..M7 as before). */
    tc_state.shift_latched = true; mode_button(TC_MODE_M1); assert(tc_state.step_field == TC_FIELD_LASTS);
    tc_state.shift_latched = true; mode_button(TC_MODE_M1 + 1u); assert(tc_state.step_field == TC_FIELD_SKIP);
    tc_state.shift_latched = false;
    /* Held steps' Lasts and Skip: an off step stays off (its place's time); the screen says so. */
    ln_lane *l = &ln.lanes[0];
    memset(&l->steps[6], 0, sizeof l->steps[6]); changed(0);
    tc_step_option(&tc_state, TC_FIELD_LASTS);
    hold(step_pad(6)); nav(TC_NAV_RIGHT); nav(TC_NAV_RIGHT); draw();
    assert(!l->steps[6].on && ln_lasts(&l->steps[6]) == 3u && !strcmp(lines[34], "LASTS 3 STEPS"));
    for (uint32_t i = 0; i < 5u; ++i) nav(TC_NAV_RIGHT);
    assert(ln_lasts(&l->steps[6]) == LN_LASTS);
    tc_step_option(&tc_state, TC_FIELD_SKIP); nav(TC_NAV_RIGHT); draw();
    assert(ln_skipped(&l->steps[6]) && ln_lasts(&l->steps[6]) == LN_LASTS && !strcmp(lines[34], "SKIP ON"));
    lift(step_pad(6));
    /* A tap on or off keeps the place's time. */
    touch(step_pad(6)); assert(l->steps[6].on && ln_skipped(&l->steps[6]) && ln_lasts(&l->steps[6]) == LN_LASTS);
    touch(step_pad(6)); assert(!l->steps[6].on && ln_skipped(&l->steps[6]));
    /* Another field turns an off step on, keeping its time too. */
    tc_step_option(&tc_state, TC_FIELD_VELOCITY);
    hold(step_pad(6)); nav(TC_NAV_RIGHT); lift(step_pad(6));
    assert(l->steps[6].on && ln_skipped(&l->steps[6]) && ln_lasts(&l->steps[6]) == LN_LASTS);
    /* The skipped step lit dim red, on or off. */
    ui_light light; assert(ui_step_color(&tc_state, &e, step_pad(6), 255u, 100u, &light) && light.r && !light.g && !light.b);
    /* A controller slot on the held steps (target 9 Lasts, 10 Skip: the field + 1). */
    hold(step_pad(2));
    tc_steps_set_field(&tc_state, TC_FIELD_LASTS, 127u, 127u); assert(ln_lasts(&l->steps[2]) == LN_LASTS);
    tc_steps_set_field(&tc_state, TC_FIELD_LASTS, 0u, 127u); assert(ln_lasts(&l->steps[2]) == 1u);
    tc_steps_set_field(&tc_state, TC_FIELD_SKIP, 100u, 127u); assert(ln_skipped(&l->steps[2]));
    tc_steps_set_field(&tc_state, TC_FIELD_SKIP, 10u, 127u); assert(!ln_skipped(&l->steps[2]));
    lift(step_pad(2));
    assert(tc_cc_slot_ok(&(tc_cc_slot){TC_CC_STEP, 1, TC_FIELD_SKIP + 1u, 0, 0, 0, 0, 0}) &&
           !tc_cc_slot_ok(&(tc_cc_slot){TC_CC_STEP, 1, TC_FIELDS + 1u, 0, 0, 0, 0, 0}));
    /* The named functions 76 and 77 choose them. */
    assert(TC_NAMED_FIELD_MORE == 76u && tc_cc_slot_ok(&(tc_cc_slot){TC_CC_FUNCTION, 1, TC_NAMED_FIELD_MORE + 1u, 0, 0, 0, 0, 0}));
    assert(!strcmp(ui_field_names[TC_FIELD_LASTS], "LASTS") && !strcmp(ui_field_names[TC_FIELD_SKIP], "SKIP"));
    /* Euclid and Mutate keep each place's time; step record fills keep it too. */
    l->steps[3].time = 2u; ln_euclid_fill(l, 3u, 0u, tc_brush_default(LN_DRUMS)); assert(ln_lasts(&l->steps[3]) == 3u);
    ln_mutate(l, 0, 7u, 100u, 3u); assert(ln_lasts(&l->steps[3]) == 3u && ln_skipped(&l->steps[6]));
    printf("editing: Shift + M1 Lasts, Shift + M2 Skip, an off step kept off, taps keep the time, lit dim red, controller "
           "slots, named functions, Euclid and Mutate keep it; ");
}

/* --- 4. Stutter 1/32, Triplet, Dotted, kept by a double tap --- */
static void stutter(void) {
    boot();
    tc_state.buttons[0] = TC_ACT_STUTTER8; tc_state.buttons[1] = TC_ACT_TRIPLET; tc_state.buttons[2] = TC_ACT_DOTTED;
    tc_state.buttons[3] = TC_ACT_STUTTER32;
    centre(0, true); assert(tc_stutter_units(&tc_state) == 12u * RH_SUB);
    centre(1, true); assert(tc_stutter_units(&tc_state) == 8u * RH_SUB);   /* 1/8 triplet */
    centre(1, false); centre(2, true); assert(tc_stutter_units(&tc_state) == 18u * RH_SUB); /* dotted */
    centre(1, true); assert(tc_stutter_units(&tc_state) == 12u * RH_SUB);  /* both: as written */
    centre(1, false); centre(2, false); centre(0, false); assert(!tc_stutter_units(&tc_state));
    centre(1, true); assert(!tc_stutter_units(&tc_state)); centre(1, false); /* Triplet alone: nothing */
    centre(3, true); assert(tc_stutter_units(&tc_state) == 3u * RH_SUB); centre(3, false);
    /* Tapped twice quickly: kept after it is let go, the button lit; a press lets it go. */
    run(500);
    centre(0, true); centre(0, false); centre(0, true); assert(tc_state.last_action == TC_STUTTER_KEPT); centre(0, false);
    assert(tc_stutter_units(&tc_state) == 12u * RH_SUB && tc_panel_active(&tc_state, 0));
    run(1000); assert(tc_stutter_units(&tc_state) == 12u * RH_SUB);
    centre(3, true); assert(tc_state.last_action == TC_STUTTER_LET_GO); draw(); assert(!strcmp(lines[36], "STUTTER LET GO"));
    assert(tc_stutter_units(&tc_state) == 3u * RH_SUB); /* the press's own while held */
    centre(3, false); assert(!tc_stutter_units(&tc_state) && !tc_panel_active(&tc_state, 0));
    /* Kept with Triplet held: both kept. */
    run(500);
    centre(1, true); centre(0, true); centre(0, false); centre(0, true); centre(0, false); centre(1, false);
    assert(tc_state.stutter_kept == (1u << 1 | 1u << 4) && tc_stutter_units(&tc_state) == 8u * RH_SUB);
    centre(0, true); centre(0, false); assert(!tc_state.stutter_kept);
    /* 2.8.5 (review TBR-081): Panic lets a kept Stutter go, as Shut down does (2.8 kept it: Play again stuttered). */
    run(500);
    centre(0, true); centre(0, false); centre(0, true); centre(0, false); assert(tc_state.stutter_kept);
    tc_state.buttons[4] = TC_ACT_PANIC; centre(4, true); centre(4, false); tc_state.buttons[4] = 0u;
    assert(!tc_state.stutter_kept && !tc_stutter_units(&tc_state));
    /* Two presses too far apart: not kept. */
    run(500); centre(0, true); centre(0, false); run(TC_SHIFT_LATCH_MS); centre(0, true); centre(0, false);
    assert(!tc_state.stutter_kept);
    /* Played: 16 drum steps of 1/16; Stutter 1/32 held: the step it fell in, again every half step. */
    boot(); drums(16);
    tc_state.buttons[3] = TC_ACT_STUTTER32;
    tc_toggle_transport(&tc_state); run(5u * 125u + 20u);
    centre(3, true); uint32_t from = logged; run(500);
    uint8_t notes[16]; uint32_t times[16], n = heard(from, notes, times, 16);
    assert(n >= 6u);
    for (uint32_t i = 1; i < n; ++i) { assert(notes[i] == notes[0]); int32_t g = (int32_t)(times[i] - times[i - 1u]); assert(g >= 60 && g <= 65); }
    centre(3, false); stop();
    /* Pressed in a step's second half (the loop after the step's start): the step still plays each time round, from
     * the loop's start (before 2.6.8's fix it was too late, and silent). */
    boot(); drums(16);
    tc_state.buttons[3] = TC_ACT_STUTTER32;
    tc_toggle_transport(&tc_state); run(5u * 125u + 75u);
    centre(3, true); from = logged; run(500);
    n = heard(from, notes, times, 16);
    assert(n >= 6u);
    for (uint32_t i = 1; i < n; ++i) { assert(notes[i] == notes[0]); int32_t g = (int32_t)(times[i] - times[i - 1u]); assert(g >= 60 && g <= 65); }
    centre(3, false); stop();
    /* Named, stored. */
    assert(!strcmp(ui_value_text(TC_MENU_BUTTON1, TC_ACT_STUTTER32), "STUT 1/32") && !strcmp(ui_value_text(TC_MENU_BUTTON1, TC_ACT_DOUBLE_SPEED), "2X SPEED"));
    tc_controls c = tc_state; c.buttons[5] = TC_ACT_DOUBLE_SPEED; c.buttons[6] = TC_ACT_TRIPLET;
    uint32_t p[ST_PAYLOAD]; assert(st_get(ST_LIVE, 0, &c, p) == ST_OK);
    tc_controls d; tc_init(&d); assert(st_put(ST_LIVE, 0, p, &d, &e) == ST_OK && d.buttons[5] == TC_ACT_DOUBLE_SPEED && d.buttons[6] == TC_ACT_TRIPLET);
    printf("stutter: 1/32, Triplet 2/3, Dotted 3/2, kept by two taps (lit, with those held), let go by a press, played, "
           "named and stored; ");
}

/* --- 5. Half and Double speed --- */
static void speed(void) {
    uint8_t was = 0; uint32_t real = 0, played = 0;
    assert(tp_speed(1000u, TC_SPEED_NONE, &was, &real, &played) == 1000u);
    assert(tp_speed(1000u, TC_SPEED_HALF, &was, &real, &played) == 1000u && was == TC_SPEED_HALF);
    assert(tp_speed(1400u, TC_SPEED_HALF, &was, &real, &played) == 1200u);
    assert(tp_speed(1500u, TC_SPEED_DOUBLE, &was, &real, &played) == 1250u); /* changed: on from where it was */
    assert(tp_speed(1600u, TC_SPEED_DOUBLE, &was, &real, &played) == 1450u);
    assert(tp_speed(1700u, TC_SPEED_NONE, &was, &real, &played) == 1700u && !was); /* let go: back in time */
    /* Played: 16 drum steps of 1/16 (125 ms); Half speed held: a step every 250 ms; Double: every 62; let go: where the
     * real position is. */
    boot(); drums(16);
    tc_state.buttons[0] = TC_ACT_HALF_SPEED; tc_state.buttons[1] = TC_ACT_DOUBLE_SPEED;
    tc_toggle_transport(&tc_state); run(600);
    centre(0, true); assert(tc_speed(&tc_state) == TC_SPEED_HALF);
    uint32_t from = logged; run(1000);
    uint8_t notes[32]; uint32_t times[32], n = heard(from, notes, times, 32);
    assert(n >= 3u);
    for (uint32_t i = 1; i < n; ++i) { int32_t g = (int32_t)(times[i] - times[i - 1u]); assert(g >= 248 && g <= 252); assert(notes[i] == 36u + (notes[i - 1u] - 35u) % 16u); }
    centre(1, true); assert(tc_speed(&tc_state) == TC_SPEED_DOUBLE); /* Double wins */
    from = logged; run(500); n = heard(from, notes, times, 32);
    for (uint32_t i = 2; i < n; ++i) { int32_t g = (int32_t)(times[i] - times[i - 1u]); assert(g >= 60 && g <= 65); }
    centre(1, false); centre(0, false); assert(tc_speed(&tc_state) == TC_SPEED_NONE);
    /* Let go: the lanes back where they would have been (step = the real position's). */
    run(200);
    uint32_t c, at, pos = e.lane_pos;
    assert(ln_count_at(&ln.lanes[0], &ln.groove, 0, ln.seed, pos, tc_arp_rates[LN_RATE_DEFAULT], &c, &at) && e.lane_count[0] == c);
    stop();
    /* The progression too: a chord a bar, Double speed: a chord every half bar. */
    boot();
    assert(sq_tap(&sq, 3u, 100u, 0u, (tc_chord){0, 0, 0}) && sq_tap(&sq, 19u, 100u, 0u, (tc_chord){0, 0, 0}));
    tc_state.buttons[1] = TC_ACT_DOUBLE_SPEED;
    tc_toggle_transport(&tc_state); run(300);
    centre(1, true); uint32_t before = logged, chords = 0, ch = tc_half_ch(&tc_state, 0u), last = 0;
    run(4000);
    for (uint32_t i = before; i < logged; ++i) if (on_ch[i] == ch && on_at[i] != last) { chords++; last = on_at[i]; }
    assert(chords >= 3u && chords <= 5u); /* 4 s at 120 BPM: 2 bars, 4 at double speed: 4 chords */
    centre(1, false); stop();
    printf("speed: half and double from where it began, a change carrying on, let go in time; played on the lanes and "
           "the progression\n");
}
/* 2.7.1 (review TBR-040): Half or Double speed held across a pattern's end: the new pattern starts at its step 1 and
 * plays at the held speed (before: Half left it silent until the slowed lanes caught up; Double started it a round in). */
static void speed_switch(void) {
    for (uint32_t held = 0; held < 2u; ++held) {
        boot();
        for (uint32_t p = 0; p < 2u; ++p) {
            ln_lane *l = &ln_bank[p].lanes[0];
            l->kind = LN_DRUMS; ln_set_length(l, 16u);
            for (uint32_t s = 0; s < 16u; ++s) { l->steps[s] = tc_brush_default(LN_DRUMS); l->steps[s].value = (uint8_t)((p ? 60u : 36u) + s); }
            for (uint32_t s = 0; s <= LN_STEPS; s += LN_CHUNK) ln_changed_in(p, 0u, s);
        }
        tc_state.buttons[0] = held ? TC_ACT_DOUBLE_SPEED : TC_ACT_HALF_SPEED;
        tc_toggle_transport(&tc_state); run(100);
        centre(0, true); ln_queued = 1u; /* pattern 2 at the end of pattern 1's round (2 s) */
        uint32_t from = logged; run(5000);
        assert(ln_current == 1u);
        uint8_t notes[128]; uint32_t times[128], n = heard(from, notes, times, 128), first = n;
        for (uint32_t i = 0; i < n && first == n; ++i) if (notes[i] >= 60u) first = i;
        assert(first + 4u < n && notes[first] == 60u); /* its step 1 (c08a3ce with Double: step 16 first) */
        uint32_t step = held ? 62u : 250u;
        for (uint32_t i = first + 1u; i < n; ++i) {
            int32_t g = (int32_t)(times[i] - times[i - 1u]);
            assert(g >= (int32_t)step - 3 && g <= (int32_t)step + 3); /* c08a3ce with Half: 2 s of silence after step 1 */
            assert(notes[i] == 60u + (notes[i - 1u] - 59u) % 16u);
        }
        centre(0, false); stop();
    }
    printf("speed switch: Half or Double held over a pattern's end starts the new pattern at its step 1, at the held speed\n");
}
/* 2.8.3 (review TBR-049): Stutter held across a pattern's end (the build-up before a change): the new pattern loops its
 * own first window, every gate ending (2.8: the loop stayed in the old pattern's place, before the new one's start, so
 * the lanes stood at step 1: struck once a window, held through it, and the window's other steps never played). Stutter
 * 1/8 (two steps of 1/16) held from 1.7 s of pattern 1's 2 s round, pattern 2 queued; then the same with Half held. */
static void stutter_switch(void) {
    for (uint32_t half = 0; half < 2u; ++half) {
        boot();
        for (uint32_t p = 0; p < 2u; ++p) {
            ln_lane *l = &ln_bank[p].lanes[0];
            l->kind = LN_DRUMS; ln_set_length(l, 16u);
            for (uint32_t s = 0; s < 16u; ++s) { l->steps[s] = tc_brush_default(LN_DRUMS); l->steps[s].value = (uint8_t)((p ? 60u : 36u) + s); }
            for (uint32_t s = 0; s <= LN_STEPS; s += LN_CHUNK) ln_changed_in(p, 0u, s);
        }
        tc_state.buttons[0] = TC_ACT_STUTTER8; tc_state.buttons[1] = TC_ACT_HALF_SPEED;
        tc_toggle_transport(&tc_state); run(100);
        ln_queued = 1u;
        if (half) { centre(1, true); run(1550); } else run(1600); /* 1.7 s into the round */
        centre(0, true);
        uint32_t from = logged, strikes = 0u, silent = 1u, last = 0u;
        for (uint32_t t = 0; t < 1600u; ++t) { /* past the switch at 2 s: each of pattern 2's strikes after a moment of silence
                                                * (the gate ended; the switch itself cuts pattern 1's last note in the pass) */
            uint32_t before = logged;
            run(1);
            for (uint32_t i = before; i < logged; ++i) if (on_ch[i] == drum_ch()) { assert(silent || last < 60u); silent = 0u; strikes++; last = on_note[i]; }
            bool any = false;
            for (uint32_t p = 0; p < 128u; ++p) any = any || sounding[drum_ch()][p];
            if (!any) silent = 1u;
        }
        assert(ln_current == 1u && strikes >= 6u);
        uint8_t notes[64]; uint32_t times[64], n = heard(from, notes, times, 64), first = n;
        for (uint32_t i = 0; i < n && first == n; ++i) if (notes[i] >= 60u) first = i;
        uint32_t step = half ? 250u : 125u;
        assert(first + 4u < n && notes[first] == 60u);
        for (uint32_t i = first + 1u; i < n; ++i) { /* steps 1 and 2 of pattern 2 in turn, a step apart */
            int32_t g = (int32_t)(times[i] - times[i - 1u]);
            assert(notes[i] == (notes[i - 1u] == 60u ? 61u : 60u) && g >= (int32_t)step - 3 && g <= (int32_t)step + 3);
        }
        centre(0, false); if (half) centre(1, false);
        stop();
    }
    /* A DAW's clock (125 BPM, a 1/16 step 120 ms), the switch at 1.92 s with Stutter held, then the DAW's song position
     * moved back to the second sixteenth (a Song Position Pointer, its clock running on): the strikes go on a step apart
     * (the loop set again from the new place; its steps struck afresh). The jump at 40 moments 10 ms apart, to the
     * second, third or fourth sixteenth. */
    for (uint32_t jump = 0; jump < 120u; ++jump) {
        static const uint8_t start[4] = {0x0F, 0xFA, 0, 0}, clock[4] = {0x0F, 0xF8, 0, 0};
        const uint8_t spp[4] = {0x03, 0xF2, (uint8_t)(1u + jump % 3u), 0};
        uint32_t at = 1960u + 10u * (jump / 3u);
        boot();
        for (uint32_t p = 0; p < 2u; ++p) {
            ln_lane *l = &ln_bank[p].lanes[0];
            l->kind = LN_DRUMS; ln_set_length(l, 16u);
            for (uint32_t s = 0; s < 16u; ++s) { l->steps[s] = tc_brush_default(LN_DRUMS); l->steps[s].value = (uint8_t)((p ? 60u : 36u) + s); }
            for (uint32_t s = 0; s <= LN_STEPS; s += LN_CHUNK) ln_changed_in(p, 0u, s);
        }
        tc_state.buttons[0] = TC_ACT_STUTTER8;
        uint32_t t0 = now, from = 0u;
        tm_packet(start, now);
        for (uint32_t t = 0; t < at + 900u; t = now - t0) {
            if (t == 100u) ln_queued = 1u;
            if (t == 1700u) raw[client(0x35)] |= 1u << fn_bit[0]; /* Stutter held from here (the press takes 50 ms) */
            if (t == at) { tm_packet(spp, now); from = logged; }
            if (t % 20u == 0u) tm_packet(clock, now);
            run(1);
        }
        uint8_t notes[64]; uint32_t times[64], n = heard(from, notes, times, 64);
        assert(ln_current == 1u && n >= 6u);
        for (uint32_t i = 1; i < n; ++i) { int32_t g = (int32_t)(times[i] - times[i - 1u]); assert(notes[i] >= 60u && g <= 120 + 25); }
        raw[client(0x35)] &= ~(1u << fn_bit[0]); run(60);
        static const uint8_t stopped[4] = {0x0F, 0xFC, 0, 0};
        tm_packet(stopped, now); run(600);
        for (uint32_t ch = 0; ch < 16u; ++ch) for (uint32_t p = 0; p < 128u; ++p) assert(!sounding[ch][p]);
    }
    printf("stutter switch: Stutter held over a pattern's end loops the new pattern's first window, gates ending; with Half "
           "too; a DAW's jump back after it\n");
}
/* 2.8.1 (review TBR-046): Half or Double speed held while a DAW's song position jumps back (a DAW looping a bar: a Song
 * Position Pointer with its clock running on): the lanes go on from the jump at the held speed (2.8: with Half, the
 * first step and then a second of silence in every loop; with Double, 0.56 s). A drum lane of 16 steps of 1/16 to a
 * clock at 125 BPM (a tick every 20 ms, a bar 1,920 ms), the speed held from 1 s on. */
static void speed_jump(void) {
    static const uint8_t start[4] = {0x0F, 0xFA, 0, 0}, clock[4] = {0x0F, 0xF8, 0, 0}, spp[4] = {0x03, 0xF2, 0, 0};
    for (uint32_t held = 0; held < 2u; ++held) {
        boot(); drums(16);
        tc_state.buttons[0] = held ? TC_ACT_DOUBLE_SPEED : TC_ACT_HALF_SPEED;
        uint32_t t0 = now, from = 0u;
        tm_packet(start, now);
        for (uint32_t t = 0; t < 6000u; t = now - t0) {
            if (t && t % 1920u == 0u) tm_packet(spp, now); /* the loop's jump, its clock as ever */
            if (t % 20u == 0u) tm_packet(clock, now);
            if (t == 1000u) { raw[client(0x35)] |= 1u << fn_bit[0]; from = logged; } /* held from here (centre() runs 50 ms) */
            run(1);
        }
        assert(tc_speed(&tc_state) == (held ? TC_SPEED_DOUBLE : TC_SPEED_HALF));
        uint8_t notes[128]; uint32_t times[128], n = heard(from, notes, times, 128);
        uint32_t step = held ? 60u : 240u; /* a 1/16 at 125 BPM is 120 ms */
        assert(n >= 5000u / step - 4u);
        for (uint32_t i = 2; i < n; ++i) { /* from the first whole step at the held speed on: never a gap */
            int32_t g = (int32_t)(times[i] - times[i - 1u]);
            assert(g <= (int32_t)step + 25); /* a tick of slack at a jump (2.8: 1,000 and 560 ms) */
        }
        raw[client(0x35)] &= ~(1u << fn_bit[0]); run(60);
        static const uint8_t stopped[4] = {0x0F, 0xFC, 0, 0};
        tm_packet(stopped, now); run(600);
        for (uint32_t ch = 0; ch < 16u; ++ch) for (uint32_t p = 0; p < 128u; ++p) assert(!sounding[ch][p]);
    }
    /* The progression too: four chords of a beat each (a beat 480 ms), the same looped bar, Half held from 1 s on: after
     * each jump the first chord for two beats, then the second (2.8: a chord from the far side of the 32-bit circle). */
    boot();
    sq_clear(&sq, SQ_UNIT_BEAT, 0u);
    static const uint8_t chords[4] = {3, 19, 23, 15};
    for (uint32_t i = 0; i < 4u; ++i) assert(sq_tap(&sq, chords[i], 100u, 0u, (tc_chord){0, 0, 0}));
    tc_state.buttons[0] = TC_ACT_HALF_SPEED;
    uint32_t t0 = now;
    tm_packet(start, now);
    for (uint32_t t = 0; t < 6000u; t = now - t0) {
        if (t && t % 1920u == 0u) tm_packet(spp, now);
        if (t % 20u == 0u) tm_packet(clock, now);
        if (t == 1000u) raw[client(0x35)] |= 1u << fn_bit[0];
        run(1);
        if (t > 1920u && t % 1920u == 500u) assert(e.seq_running && e.seq_entry == 0u);
        if (t > 1920u && t % 1920u == 1500u) assert(e.seq_entry == 1u);
    }
    raw[client(0x35)] &= ~(1u << fn_bit[0]); run(60);
    static const uint8_t halt[4] = {0x0F, 0xFC, 0, 0};
    tm_packet(halt, now); run(600);
    for (uint32_t ch = 0; ch < 16u; ++ch) for (uint32_t p = 0; p < 128u; ++p) assert(!sounding[ch][p]);
    printf("speed jump: Half or Double held over a DAW's jump back goes on from there, no silence; the progression too\n");
}
/* 2.7.1 (review TBR-038): a lane with nothing playing at the position (every step skipped; a late first visit) shows
 * no step, not the one it played before (c08a3ce: step 12 of a lane shortened to 4), and its first visit sounds. */
static void waiting(void) {
    boot(); drums(16);
    tc_toggle_transport(&tc_state); run(1550); /* in step 13 */
    assert(e.lane_step[0] == 12u);
    ln_lane *l = &ln.lanes[0];
    ln_set_length(l, 4u);
    for (uint32_t s = 0; s < 4u; ++s) l->steps[s].time |= LN_SKIP;
    changed(0); run(20);
    assert(e.lane_step[0] == LN_NONE && e.lane_count[0] == UINT32_MAX);
    for (uint32_t s = 0; s < 4u; ++s) l->steps[s].time = 0u;
    changed(0); uint32_t from = logged; run(600);
    uint8_t notes[8]; assert(heard(from, notes, NULL, 8) >= 3u && e.lane_step[0] < 4u);
    stop();
    printf("waiting: a lane with nothing to play shows no step, then plays again\n");
}

int main(void) {
    paths(); timeline(); editing(); stutter(); speed(); speed_switch(); waiting(); /* 2.7.1 */
    speed_jump(); /* 2.8.1 */
    stutter_switch(); /* 2.8.3 */
    return 0;
}
