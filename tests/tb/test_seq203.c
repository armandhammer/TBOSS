/* SPDX-License-Identifier: GPL-3.0-or-later - TBOSS 2.0.3 on the real code (engine, controls, settings.c on a fake
 * sector 11, the screens): the step sequencer's Fill and Not fill steps (a centre button or the pedal), a step's nudge
 * and a lane's swing (where each step starts, every rate, the steps kept in order; heard early and late), a lane's
 * transpose (scale steps for chords, semitones for the others), Mutate (repeatable, in range, the Steps page's item),
 * Stutter (the lanes and the progression loop the window the press fell in, then carry on in time), the Steps page's
 * new items, the new actions and pedal mode stored. Every constant from the firmware's headers. */
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
static char lines[64][32], right[64][32]; /* right: text ending at the screen's right edge (a menu item's value) */
void td_clear(void) { memset(lines, 0, sizeof lines); memset(right, 0, sizeof right); }
uint32_t td_text(uint32_t x, uint32_t y, const char *text, uint32_t scale) {
    if (y < 64u && !x) snprintf(lines[y], sizeof lines[y], "%s", text);
    if (y < 64u && x && x + 6u * scale * (uint32_t)strlen(text) == 128u) snprintf(right[y], sizeof right[y], "%s", text);
    return x + 6u * scale * (uint32_t)strlen(text);
}

static tp_engine e;
static ts_scan scan;
static uint32_t now = 1000u;
static uint8_t sounding[16][128];
#define LOG 8192u
static uint8_t on_note[LOG], on_ch[LOG];
static uint32_t on_at[LOG], logged;
static bool writer(const uint8_t p[4], uint32_t token) {
    (void)token;
    uint32_t ch = p[1] & 15u;
    if (p[0] == TK_ON) {
        assert(!sounding[ch][p[2]]); sounding[ch][p[2]] = 1u;
        if (logged < LOG) { on_note[logged] = p[2]; on_ch[logged] = (uint8_t)ch; on_at[logged++] = now; }
    }
    if (p[0] == TK_OFF) { assert(sounding[ch][p[2]]); sounding[ch][p[2]] = 0u; }
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
        if (e.pending_id) tp_complete(&e, e.pending_id, 4u, true, now);
        st_requests(&tc_state, &e);
        st_service(&tc_state, now);
    }
}
static void screen(void) { ui_service(&tc_state, &e, &scan, true, now); }
static const uint8_t fn_bit[TC_FUNCTIONS] = {6, 5, 0, 1, 7, 4, 3, 8, 9, 10, 11, 12, 14, 2};
static void centre(uint32_t i, bool down) {
    if (down) raw[client(0x35)] |= 1u << fn_bit[i]; else raw[client(0x35)] &= ~(1u << fn_bit[i]);
    run(30);
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
/* A drum lane of n steps on lane 0, step i playing note 36 + i. */
static void drums(uint32_t n) {
    ln_lane *l = &ln.lanes[0];
    l->kind = LN_DRUMS; ln_set_length(l, n);
    for (uint32_t i = 0; i < n; ++i) { l->steps[i] = tc_brush_default(LN_DRUMS); l->steps[i].value = (uint8_t)(36u + i); }
}
static uint32_t drum_ch(void) { return tc_state.drum_ch & 15u; }
/* The time of the k-th Note On of note n on the drum channel from log entry `from`. */
static uint32_t time_of(uint8_t n, uint32_t k, uint32_t from) {
    for (uint32_t i = from; i < logged; ++i) if (on_ch[i] == drum_ch() && on_note[i] == n && !k--) return on_at[i];
    assert(0); return 0;
}
static bool heard_note(uint8_t n, uint32_t from) {
    for (uint32_t i = from; i < logged; ++i) if (on_ch[i] == drum_ch() && on_note[i] == n) return true;
    return false;
}
static int32_t gap(int32_t a, int32_t b) { return a - b; }

/* --- 1. Fill and Not fill --- */
static void fill(void) {
    ln_step s; memset(&s, 0, sizeof s); s.on = true;
    s.condition = LN_FILL; assert(!ln_sounds(&s, 0, 0, 0, 0, false) && ln_sounds(&s, 0, 0, 0, 0, true));
    s.condition = LN_NOT_FILL; assert(ln_sounds(&s, 0, 0, 0, 0, false) && !ln_sounds(&s, 0, 0, 0, 0, true));
    s.condition = LN_ALWAYS; assert(ln_sounds(&s, 0, 0, 0, 0, false) && ln_sounds(&s, 0, 0, 0, 0, true));
    s.condition = 2u; /* 2:2 still counts the loops, Fill or not */
    assert(!ln_sounds(&s, 0, 0, 0, 0, true) && ln_sounds(&s, 0, 0, 1, 0, true));
    /* Played: step 0 always (36), step 1 Fill (37), step 2 Not fill (38). */
    boot(); drums(4); ln.lanes[0].steps[1].condition = LN_FILL; ln.lanes[0].steps[2].condition = LN_NOT_FILL;
    ln.lanes[0].steps[3].on = false;
    tc_state.buttons[0] = TC_ACT_FILL;
    tc_toggle_transport(&tc_state); run(1100);
    assert(heard_note(36, 0) && heard_note(38, 0) && !heard_note(37, 0));
    centre(0, true); assert(tc_fill(&tc_state) && tc_state.last_action == TC_BUTTON && tc_state.last_button == TC_ACT_FILL);
    screen(); assert(!strcmp(lines[36], "FILL WHILE HELD")); /* the play screen's message line */
    run(600); uint32_t from = logged; run(1000);
    assert(heard_note(36, from) && heard_note(37, from) && !heard_note(38, from));
    centre(0, false); assert(!tc_fill(&tc_state)); run(600); from = logged; run(1000);
    assert(heard_note(38, from) && !heard_note(37, from));
    stop();
    /* The pedal's Fill. */
    tc_state.pedal_mode = TC_PEDAL_FILL; tc_state.pedal_pressed = true; assert(tc_fill(&tc_state));
    tc_state.pedal_pressed = false; assert(!tc_fill(&tc_state));
    assert(!tc_pedal_cc(TC_PEDAL_FILL));
    puts("fill: Fill steps only while Fill is held, Not fill only while it isn't, A:B kept; a centre button (FILL WHILE HELD) "
         "and the pedal");
}

/* --- 2. nudge and swing --- */
static void nudge_and_swing(void) {
    for (uint8_t b = 0; b < LN_NUDGES; ++b) assert(ln_nudge(b) == (b < 8u ? (int32_t)b : (int32_t)b - 16));
    /* Every rate, random nudges and swing: the step at pos is the last that started, and steps keep their order. */
    uint32_t seed = 7u, checked = 0;
    for (uint32_t r = 0; r < TC_ARP_RATES; ++r) for (uint32_t trial = 0; trial < 6u; ++trial) {
        ln_lane l; memset(&l, 0, sizeof l);
        ln_set_length(&l, 1u + (seed = seed * 1103515245u + 12345u) % LN_STEPS);
        l.swing = (uint8_t)(trial ? (seed >> 8) % (LN_SWING_MAX + 1u) : 0u);
        l.direction = (uint8_t)((seed >> 12) % LN_DIRECTIONS);
        for (uint32_t i = 0; i < LN_STEPS; ++i) l.steps[i].nudge = (uint8_t)(trial ? (seed = seed * 1103515245u + 12345u) >> 20 & 15u : 0u);
        rh_rate rate = tc_arp_rates[r];
        for (uint32_t c = 0; c < 200u; ++c) {
            uint32_t length = rh_step_start(c + 1u, rate) - rh_step_start(c, rate);
            int32_t off = ln_offset(&l, &l.steps[ln_order(&l, 0, 3u, c, NULL)], c, length);
            assert(2 * (off < 0 ? -off : off) < (int32_t)length);
            assert(gap((int32_t)ln_start(&l, 0, 3u, c + 1u, rate), (int32_t)ln_start(&l, 0, 3u, c, rate)) > 0);
        }
        for (uint32_t pos = 0; pos < rh_step_start(200u, rate); pos += 37u) {
            uint32_t c;
            if (!ln_count_at(&l, 0, 3u, pos, rate, &c)) { assert(gap((int32_t)pos, (int32_t)ln_start(&l, 0, 3u, 0, rate)) < 0); continue; }
            assert(gap((int32_t)pos, (int32_t)ln_start(&l, 0, 3u, c, rate)) >= 0 && gap((int32_t)pos, (int32_t)ln_start(&l, 0, 3u, c + 1u, rate)) < 0);
            if (!trial) assert(c == rh_step_at(pos, rate));
            checked++;
        }
    }
    /* Heard: 1/16 at 120 BPM is 125 ms; step 1 a quarter of a step late (+4/16), then early, then swing 40 %. */
    int32_t shift[3] = {0, 0, 0};
    for (uint32_t k = 0; k < 3u; ++k) {
        boot(); drums(4);
        if (k == 0u) ln.lanes[0].steps[1].nudge = 4u;
        if (k == 1u) ln.lanes[0].steps[1].nudge = (uint8_t)(-4 & 15);
        if (k == 2u) ln.lanes[0].swing = 40u;
        tc_toggle_transport(&tc_state); run(1500);
        shift[k] = gap((int32_t)time_of(37, 1, 0), (int32_t)time_of(36, 1, 0)) - 125;
        assert(gap((int32_t)time_of(38, 1, 0), (int32_t)time_of(36, 1, 0)) >= 248 && gap((int32_t)time_of(38, 1, 0), (int32_t)time_of(36, 1, 0)) <= 252);
        stop();
    }
    assert(shift[0] >= 29 && shift[0] <= 33 && shift[1] >= -33 && shift[1] <= -29 && shift[2] >= 48 && shift[2] <= 52);
    printf("nudge and swing: %u positions over every rate find the step that started last, steps in order; heard %+d, %+d ms "
           "(nudge +-4/16), %+d ms (swing 40 %%)\n", checked, shift[0], shift[1], shift[2]);
}

/* --- 3. transpose --- */
static void transpose(void) {
    boot();
    tp_layout layout = tc_layout(&tc_state);
    ln_lane l; memset(&l, 0, sizeof l);
    ln_step s = tc_brush_default(LN_CHORDS);
    tp_notes n; uint8_t ch;
    assert(tc_step_notes(layout, LN_CHORDS, 0, &s, &n, &ch) && n.count == 3u); /* C E G */
    tp_notes up = n; l.kind = LN_CHORDS; l.transpose = 1; tc_lane_transpose(layout, &l, &s, &up);
    assert(up.count == 3u);
    for (uint32_t i = 0; i < 3u; ++i) assert(up.notes[i] == tc_harmony_note(layout, n.notes[i], 1)); /* D F A */
    assert(up.notes[0] % 12u == 2u && up.notes[1] % 12u == 5u && up.notes[2] % 12u == 9u);
    l.kind = LN_MELODY; l.transpose = -3; up = n; tc_lane_transpose(layout, &l, &s, &up);
    for (uint32_t i = 0; i < 3u; ++i) assert(up.notes[i] + 3u == n.notes[i]);
    l.kind = LN_DRUMS; l.transpose = LN_TRANSPOSE_MAX; tp_notes hi = {{120u, 30u}, 2u}; tc_lane_transpose(layout, &l, &s, &hi);
    assert(hi.count == 1u && hi.notes[0] == 78u); /* past 127: left out */
    l.transpose = 0; up = n; tc_lane_transpose(layout, &l, &s, &up); assert(!memcmp(&up, &n, sizeof n));
    /* Played: a drum lane up 5 semitones. */
    drums(4); ln.lanes[0].transpose = 5;
    tc_toggle_transport(&tc_state); run(700);
    assert(heard_note(41, 0) && heard_note(44, 0) && !heard_note(36, 0));
    stop();
    puts("transpose: chord lanes by scale steps (C to Dm), the others by semitones, out of range left out, played");
}

/* --- 4. mutate --- */
static void mutate(void) {
    uint32_t seed = 99u, changed = 0, total = 0;
    for (uint32_t trial = 0; trial < 400u; ++trial) {
        ln_lane a; memset(&a, 0, sizeof a);
        a.kind = (uint8_t)(trial % LN_KINDS); ln_set_length(&a, 1u + trial % LN_STEPS);
        for (uint32_t i = 0; i < LN_STEPS; ++i) {
            seed = seed * 1103515245u + 12345u;
            a.steps[i].on = (seed >> 9) & 1u; a.steps[i].value = (uint8_t)(a.kind == LN_DRUMS ? 36u + (seed >> 12) % 40u : (seed >> 12) % 48u);
        }
        uint32_t pct = 10u * (1u + trial % TC_MUTATES);
        ln_lane b = a, c = a, d = a;
        ln_mutate(&b, trial % LN_LANES, trial, pct, 5u); ln_mutate(&c, trial % LN_LANES, trial, pct, 5u);
        assert(!memcmp(&b, &c, sizeof b)); /* repeatable */
        ln_mutate(&d, trial % LN_LANES, trial, pct, 6u);
        for (uint32_t i = 0; i < LN_STEPS; ++i) {
            const ln_step *x = &a.steps[i], *y = &b.steps[i];
            if (i >= ln_length(&a)) { assert(!memcmp(x, y, sizeof *x)); continue; } /* past the length: untouched */
            total++; changed += memcmp(x, y, sizeof *x) != 0;
            if (a.kind == LN_DRUMS) { assert(!y->on || y->value >= 36u); continue; }
            assert(y->on == x->on && y->value < 48u && y->value % 4u == x->value % 4u && y->velocity <= 127u); /* rows kept */
            if (y->value != x->value) {
                int32_t moved = (int32_t)(y->value / 4u) - (int32_t)(x->value / 4u);
                assert(moved && moved >= -2 && moved <= 2);
            }
        }
    }
    assert(changed > total / 20u && changed < total / 2u); /* about the amount, on the steps that can change */
    /* On the board: Settings > Steps > Mutate, ▲▼ the amount, ● mutates the lane once (board only for the editor). */
    boot(); drums(16);
    for (uint32_t i = 0; i < 16u; i += 3u) ln.lanes[0].steps[i].on = false;
    ln_lane before = ln.lanes[0]; uint32_t changes = ln_changes;
    assert(tc_item_kind(TC_MENU_LANE_MUTATE) == TC_ITEM_BOARD && !tc_set_item(&tc_state, &e, TC_MENU_LANE_MUTATE, 4u));
    assert(tc_menu_value(&tc_state, TC_MENU_LANE_MUTATE) == TC_MUTATE_DEFAULT);
    tc_state.ui = TC_UI_EDIT; tc_state.group = TC_GROUP_STEPS; tc_state.menu = TC_MENU_LANE_MUTATE; tc_state.edit_value = 4u;
    tc_navigate(&tc_state, &e, TC_NAV_ENTER);
    assert(tc_state.last_action == TC_LANE_MUTATED && tc_state.mutate == 4u && memcmp(&before, &ln.lanes[0], sizeof before) && ln_changes > changes);
    screen(); assert(!strcmp(lines[56], "LANE 1 MUTATED"));
    /* 2.0.5 (board test W16): it reads as an action, not a setting: `MUTATE 50% OF STEPS` (the amount last used), ● CHOOSE, then ● MUTATE */
    tc_state.ui = TC_UI_MENU; tc_state.last_action = TC_NONE; screen(); assert(!strcmp(lines[56], "<BACK   O CHOOSE"));
    bool shown = false;
    for (uint32_t y = 0; y < 56u; ++y) shown = shown || (!strcmp(lines[y], ">") && !strcmp(right[y], "50% OF STEPS"));
    assert(shown);
    tc_state.ui = TC_UI_EDIT; tc_state.edit_value = 2u; screen(); assert(!strcmp(lines[56], "<CANCEL   O MUTATE"));
    tc_state.ui = TC_UI_MENU;
    ln_lane once = ln.lanes[0];
    tc_state.ui = TC_UI_EDIT; tc_state.menu = TC_MENU_LANE_MUTATE; tc_state.edit_value = 4u; tc_navigate(&tc_state, &e, TC_NAV_ENTER);
    assert(memcmp(&once, &ln.lanes[0], sizeof once)); /* again: different */
    run(ST_SAVE_DELAY_MS + 200u); assert(!ln_dirty); /* saved */
    printf("mutate: 400 lanes, repeatable, %u of %u steps changed, rows kept, one or two pads over, drums on and off, past the "
           "length untouched; Settings > Steps > Mutate (board only), LANE 1 MUTATED, each time different, saved\n", changed, total);
}

/* --- 5. stutter --- */
static void stutter(void) {
    bool on = false; uint32_t from = 0, last = 0, at;
    assert(!tp_stutter(1000u, 0u, &on, &from, &last, &at) && at == 1000u && !on);
    assert(!tp_stutter(1000u, 300u, &on, &from, &last, &at) && on && from == 900u && at == 1000u);
    assert(!tp_stutter(1150u, 300u, &on, &from, &last, &at) && at == 1150u);
    assert(tp_stutter(1210u, 300u, &on, &from, &last, &at) && at == 910u); /* round again */
    assert(!tp_stutter(1250u, 300u, &on, &from, &last, &at) && at == 950u);
    assert(!tp_stutter(1260u, 0u, &on, &from, &last, &at) && at == 1260u && !on); /* let go: in time */
    /* Played: 16 drum steps; Stutter 1/4 held during step 5: steps 4..7 over and over, then on in time. */
    boot(); drums(16);
    tc_state.buttons[2] = TC_ACT_STUTTER4;
    tc_toggle_transport(&tc_state); run(5u * 125u + 60u);
    centre(2, true); assert(tc_stutter_ticks(&tc_state) == 24u);
    screen(); assert(!strcmp(lines[36], "STUTTER 1/4"));
    uint32_t start = logged; run(1500);
    uint32_t looped = 0;
    for (uint32_t i = start; i < logged; ++i) if (on_ch[i] == drum_ch()) { assert(on_note[i] >= 40u && on_note[i] <= 43u); looped++; }
    assert(looped >= 10u);
    centre(2, false); assert(!tc_stutter_ticks(&tc_state));
    uint32_t after = logged; run(600);
    bool on_in_time = false;
    for (uint32_t i = after; i < logged; ++i) on_in_time = on_in_time || (on_ch[i] == drum_ch() && (on_note[i] < 40u || on_note[i] > 43u));
    assert(on_in_time);
    stop();
    /* The progression: a chord a bar; Stutter 1/8 strikes it again every eighth. */
    boot();
    assert(sq_tap(&sq, 3u, 100u, 0u, (tc_chord){0, 0, 0}) && sq_tap(&sq, 19u, 100u, 0u, (tc_chord){0, 0, 0}));
    tc_state.buttons[1] = TC_ACT_STUTTER8;
    tc_toggle_transport(&tc_state); run(300);
    uint32_t ch = tc_half_ch(&tc_state, 0u), before = logged, strikes = 0;
    run(1000); for (uint32_t i = before; i < logged; ++i) strikes += on_ch[i] == ch;
    assert(strikes <= 3u * 1u); /* one chord's notes at most */
    centre(1, true); before = logged; run(1000); strikes = 0;
    for (uint32_t i = before; i < logged; ++i) strikes += on_ch[i] == ch;
    assert(strikes >= 3u * 3u); /* again and again */
    centre(1, false);
    stop();
    puts("stutter: the window the press fell in, round and round, in time again when let go; lanes (STUTTER 1/4) and the "
         "progression (struck again each time round)");
}

/* --- 6. the Steps page, names, storage --- */
static void pages_and_storage(void) {
    boot();
    uint8_t items[TC_GROUP_MAX], n = tc_page_items(&tc_state, TC_GROUP_STEPS, items);
    static const uint8_t want[] = {TC_MENU_STEPS, TC_MENU_LANE, TC_MENU_LANE_KIND, TC_MENU_LANE_LENGTH, TC_MENU_LANE_RATE,
        TC_MENU_LANE_DIRECTION, TC_MENU_LANE_CHANNEL, TC_MENU_LANE_MUTE, TC_MENU_LANE_SWING, TC_MENU_LANE_TRANSPOSE,
        TC_MENU_LANE_EUCLID, TC_MENU_LANE_ROTATE, TC_MENU_LANE_MUTATE, TC_MENU_STEP_PADS};
    assert(n == sizeof want && !memcmp(items, want, sizeof want));
    assert(!strcmp(ui_item_name(TC_MENU_LANE_SWING), "SWING") && !strcmp(ui_item_name(TC_MENU_LANE_TRANSPOSE), "TRANSPOSE") &&
           !strcmp(ui_item_name(TC_MENU_LANE_MUTATE), "MUTATE"));
    assert(!strcmp(ui_value_text(TC_MENU_LANE_SWING, 0), "OFF") && !strcmp(ui_value_text(TC_MENU_LANE_SWING, 5), "25%"));
    assert(!strcmp(ui_value_text(TC_MENU_LANE_MUTATE, 0), "10% OF STEPS")); /* 2.0.5 */
    tc_state.lane = 1u; ln.lanes[1].kind = LN_CHORDS;
    assert(!strcmp(ui_value_text(TC_MENU_LANE_TRANSPOSE, TC_TRANSPOSE_MENU + 2u), "+2 STEPS"));
    ln.lanes[1].kind = LN_MELODY;
    assert(!strcmp(ui_value_text(TC_MENU_LANE_TRANSPOSE, TC_TRANSPOSE_MENU - 3u), "-3 SEMIS") && !strcmp(ui_value_text(TC_MENU_LANE_TRANSPOSE, TC_TRANSPOSE_MENU), "0"));
    assert(tc_set_item(&tc_state, &e, TC_MENU_LANE_SWING, 7u) && ln.lanes[1].swing == 35u && tc_menu_value(&tc_state, TC_MENU_LANE_SWING) == 7u);
    assert(tc_set_item(&tc_state, &e, TC_MENU_LANE_TRANSPOSE, 0u) && ln.lanes[1].transpose == -TC_TRANSPOSE_MENU);
    assert(!tc_set_item(&tc_state, &e, TC_MENU_LANE_SWING, LN_SWING_MAX / TC_SWING_STEP + 1u));
    assert(!tc_set_item(&tc_state, &e, TC_MENU_LANE_TRANSPOSE, 2u * TC_TRANSPOSE_MENU + 1u));
    ln.lanes[1].transpose = 40; assert(tc_menu_value(&tc_state, TC_MENU_LANE_TRANSPOSE) == 2u * TC_TRANSPOSE_MENU); /* from the editor: shown at the end */
    ln.lanes[1].swing = 33u; assert(tc_menu_value(&tc_state, TC_MENU_LANE_SWING) == 6u);
    /* The lane record keeps swing, transpose and nudge. */
    ln.lanes[1].steps[5].nudge = 13u; ln.lanes[1].steps[5].condition = LN_NOT_FILL; ln.lanes[1].steps[5].on = true;
    ln_pattern back; ln_clear(&back);
    for (uint32_t slot = 0; slot < LN_RECORDS; ++slot) { uint32_t w[LN_WORDS]; ln_encode(&ln, slot, w); assert(ln_decode(w, slot, &back)); }
    assert(!memcmp(&back.lanes[1], &ln.lanes[1], sizeof back.lanes[1]));
    /* The step field's name, the actions' and the pedal's names; stored in the live settings. */
    assert(!strcmp(ui_field_names[TC_FIELD_NUDGE], "NUDGE") && !strcmp(ui_condition_names[LN_FILL], "FILL") &&
           !strcmp(ui_condition_names[LN_NOT_FILL], "NOT FILL"));
    assert(!strcmp(ui_value_text(TC_MENU_PEDAL, TC_PEDAL_FILL), "FILL"));
    assert(!strcmp(ui_value_text(TC_MENU_BUTTON1, TC_ACT_FILL), "FILL") && !strcmp(ui_value_text(TC_MENU_BUTTON1, TC_ACT_STUTTER16), "STUT 1/16"));
    assert(TC_ACT_FILL == TC_ACT_PIVOT + 1u && TC_ACT_STUTTER16 + 1u == TC_ACTIONS && TC_PEDAL_FILL + 1u == TC_PEDAL_MODES);
    tc_controls c = tc_state; c.pedal_mode = TC_PEDAL_FILL; c.buttons[3] = TC_ACT_STUTTER16; c.buttons[4] = TC_ACT_FILL;
    uint32_t p[ST_PAYLOAD]; assert(st_get(ST_LIVE, 0, &c, p) == ST_OK);
    tc_controls d; tc_init(&d); assert(st_put(ST_LIVE, 0, p, &d, &e) == ST_OK);
    assert(d.pedal_mode == TC_PEDAL_FILL && d.buttons[3] == TC_ACT_STUTTER16 && d.buttons[4] == TC_ACT_FILL);
    p[35] = (p[35] & ~0xFFu) | (TC_ACTIONS + 1u); assert(st_put(ST_LIVE, 0, p, &d, &e) == ST_BAD_VALUE); /* past the last: refused */
    puts("pages: the Steps page's 14 items, Swing, Transpose (steps or semitones by the lane's kind), Mutate's names and values, "
         "set and refused; nudge and the Fill conditions in the lane record; the actions and the pedal's Fill named and stored");
}

int main(void) {
    fill();
    nudge_and_swing();
    transpose();
    mutate();
    stutter();
    pages_and_storage();
    puts("PASS: seq203: Fill and Not fill steps, nudge and swing, lane transpose, Mutate, Stutter, the Steps page and storage");
    return 0;
}
