/* SPDX-License-Identifier: GPL-3.0-or-later - TBOSS 2.1.0 on the real code (engine, controls, settings.c on a fake
 * sector 11, SysEx, the screens): external control, the MIDI map. Controllers on the Control channel only (120..127
 * never), the newest value of each; which settings can be mapped; learning in Settings (a new controller, a mapped one
 * only after Shift + right, Map button's action, the map full); a setting from lo to hi (backwards too), every value
 * reachable, a knob between two steps kept still; pickup after the value changed some other way, and which way to
 * turn; actions pressed and let go (toggles, while held, Shift); the Control channel off lets go; stored 3 s after,
 * back after a restart, kept by presets, cleared by Factory settings and Clear map; the record over SysEx, refused
 * when it doesn't make sense; the Control page. Every constant from the firmware's headers. */
#include "../../src/tb/settings.h"
#include "../../src/tb/sequence.h"
#include "../../src/tb/midi_in.h"
#include "../../src/tb/flash_store.h"
#include "../../src/tb/display.h"
#include "../../src/tb/leds.h"
#include "../../src/tb/jack.h"
#include "../../src/tb/nor.h"
#include "../../src/tb/ui.h"
#include "../../src/tb/sysex.h"
#include "../../src/tb/controls_internal.h"
#include "../../src/tb/ui_internal.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
volatile ts_abi ts_status;
static uint32_t flash[FS_SIZE / 4u];
const uint8_t *fs_base(void) { return (const uint8_t *)flash; }
bool fs_erase(void) { memset(flash, 0xFF, sizeof flash); return true; }
static uint32_t writes;
bool fs_program(uint32_t offset, const uint32_t *words, uint32_t count) {
    for (uint32_t i = 0; i < count; ++i) flash[offset / 4u + i] &= words[i];
    writes++;
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
static bool writer(const uint8_t p[4], uint32_t token) { (void)p; (void)token; return true; }
static const uint8_t addresses[4] = {0x35, 0x36, 0x38, 0x39};
static uint32_t raw[4];
static uint32_t client(uint8_t a) { for (uint32_t i = 0; i < 4u; ++i) if (addresses[i] == a) return i; assert(0); return 0; }
static struct bi_reply reader(void *ctx, uint8_t a, uint8_t reg, uint32_t gen, uint32_t stamp, uint32_t budget) {
    (void)ctx; (void)reg; (void)budget;
    uint32_t i = client(a);
    return (struct bi_reply){.status = 0, .count = 2, .generation = gen, .timestamp_ms = stamp, .elapsed_us = 500,
                             .data = {(uint8_t)raw[i], (uint8_t)(raw[i] >> 8)}};
}
/* main.c's loop, a pass a millisecond. */
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
        tc_program_in(&tc_state);
        tc_cc_in(&tc_state, &e, now);
    }
}
static void screen(void) { ui_service(&tc_state, &e, &scan, true, now); }
/* A Control Change arriving on channel ch (1..16), then a pass. */
static void cc(uint8_t ch, uint8_t n, uint8_t v) {
    uint8_t p[4] = {0x0Bu, (uint8_t)(0xB0u | ((ch - 1u) & 15u)), n, v};
    tm_packet(p, now);
    run(1);
}
static const uint8_t fn_bit[TC_FUNCTIONS] = {6, 5, 0, 1, 7, 4, 3, 8, 9, 10, 11, 12, 14, 2};
static void centre(uint32_t i, bool down) {
    if (down) raw[client(0x35)] |= 1u << fn_bit[i]; else raw[client(0x35)] &= ~(1u << fn_bit[i]);
    run(30);
}
static void nav(uint32_t index) { tc_navigate(&tc_state, &e, index); run(1); }
static void boot(bool erase) {
    tc_init(&tc_state); memset(&e, 0, sizeof e); memset(&scan, 0, sizeof scan); memset(raw, 0, sizeof raw);
    tm_reset(); ln_clear(&ln);
    sq_clear(&sq, SQ_UNIT_BAR, 0u);
    if (erase) fs_erase();
    (void)st_load(&tc_state);
    run(600); assert(e.state == TK_ACTIVE);
}
static void control(uint8_t ch) { assert(tc_set_item(&tc_state, &e, TC_MENU_CONTROL_CH, ch)); run(1); }
static void editing(uint8_t item) {
    for (uint8_t g = 0; g < TC_GROUPS; ++g) for (uint8_t i = 0; i < tc_group_size(g); ++i)
        if (tc_group_items[g][i] == item) tc_state.group = g;
    tc_state.menu = item; tc_state.ui = TC_UI_EDIT; tc_state.edit_value = tc_menu_value(&tc_state, item);
}
static void slot(uint32_t i, uint8_t kind, uint8_t n, uint8_t target, uint8_t lo, uint8_t hi, uint8_t jump) {
    tc_cc_restart(&tc_state, i);
    tc_state.cc[i] = (tc_cc_slot){kind, n, target, lo, hi, jump, 0, 0};
    tc_cc_watch(&tc_state); /* 2.4.13: midi_in.c listens for it at once */
}

/* --- 1. what arrives --- */
static void input(void) {
    boot(true);
    uint8_t v;
    cc(1, 21, 100); assert(!tm_cc_take(21, &v)); /* Control channel off: nothing */
    control(3);
    cc(1, 21, 100); assert(!tm_cc_take(21, &v)); /* another channel */
    tm.cc_channel = 3u; /* (tc_cc_in would take them) */
    uint8_t p[4] = {0x0Bu, 0xB2u, 21u, 5u}; tm_packet(p, now);
    p[3] = 9u; tm_packet(p, now); p[3] = 77u; tm_packet(p, now); /* a sweep within one pass: the newest only */
    assert(tm_cc_take(21, &v) && v == 77u && !tm_cc_take(21, &v));
    assert(tm.cc_last == 21u && tm.cc_last_value == 77u && tm.cc_last_channel == 3u);
    for (uint8_t n = TM_CONTROLS; n < 128u; ++n) { p[2] = n; tm_packet(p, now); assert(!tm_cc_take(n, &v) && tm.cc_last == 21u); }
    tm.cc_channel = TM_CC_ANY; p[1] = 0xBFu; p[2] = 7u; tm_packet(p, now); assert(tm_cc_take(7, &v) && tm.cc_last_channel == 16u);
    /* A Note On never counts, 123 still ends the MIDI-in notes. */
    uint8_t note[4] = {0x09u, 0x92u, 21u, 100u}; uint32_t seen = tm.cc_seen; tm_packet(note, now); assert(tm.cc_seen == seen);
    p[1] = 0xB2u; p[2] = 123u; p[3] = 0u; tm_packet(p, now); assert(!tm.notes[21]);
    /* Waiting changes from another channel go when the channel changes. */
    p[2] = 30u; p[3] = 1u; tm_packet(p, now); tm.cc_channel = 5u; p[1] = 0xB4u; p[2] = 31u; tm_packet(p, now);
    assert(!tm_cc_take(30, &v) && tm_cc_take(31, &v));
    puts("input: only the Control channel (or all), controllers 0..119, the newest value, the last one for learning");
}

/* --- 2. what can be mapped --- */
static void mappable(void) {
    uint32_t n = 0;
    for (uint8_t item = 0; item < TC_MENU_ITEMS; ++item) {
        bool m = tc_cc_mappable(item);
        if (m) { n++; assert(tc_item_kind(item) == TC_ITEM_VALUE && tc_menu_values(item) >= 2u); }
        if (item >= TC_MENU_BUTTON1) assert(!m);
    }
    assert(tc_cc_mappable(TC_MENU_TEMPO) && tc_cc_mappable(TC_MENU_SWING) && tc_cc_mappable(TC_MENU_SCALE) &&
           tc_cc_mappable(TC_MENU_HOLD) && tc_cc_mappable(TC_MENU_SPREAD) && tc_cc_mappable(TC_MENU_LANE_SWING));
    assert(!tc_cc_mappable(TC_MENU_CONTROL_CH) && !tc_cc_mappable(TC_MENU_CC_ACTION) && !tc_cc_mappable(TC_MENU_CC_CLEAR) &&
           !tc_cc_mappable(TC_MENU_SEQ_UNIT) && !tc_cc_mappable(TC_MENU_LANE_EUCLID) && !tc_cc_mappable(TC_MENU_LANE_ROTATE) &&
           !tc_cc_mappable(TC_MENU_U3) && !tc_cc_mappable(TC_MENU_FACTORY) && !tc_cc_mappable(TC_MENU_LOAD) &&
           !tc_cc_mappable(TC_MENU_TRANSPORT) && !tc_cc_mappable(TC_MENU_ADD7) && !tc_cc_mappable(TC_MENU_VERSION));
    assert(n > 60u);
    printf("mappable: %u settings, none of the Control or Buttons pages, actions, per-chord edits, Unit, Euclid, Rotate\n", n);
}

/* --- 3. learning --- */
static void learning(void) {
    boot(true); control(1);
    editing(TC_MENU_TEMPO); uint8_t tempo = tc_state.tempo;
    cc(1, 21, 0);
    assert(tc_state.last_action == TC_CC_LEARNED && tc_cc_count(&tc_state) == 1u);
    assert(tc_state.cc[0].kind == TC_CC_SETTING && tc_state.cc[0].cc == 21u && tc_state.cc[0].target == TC_MENU_TEMPO &&
           tc_state.cc[0].lo == 0u && tc_state.cc[0].hi == TC_TEMPOS - 1u && !tc_state.cc[0].jump);
    assert(tc_state.tempo == tempo); /* the learning move does nothing else */
    screen(); assert(!strcmp(lines[56], "CC21 = TEMPO"));
    /* 2.3.2 (board test C2): a knob sends a stream; the rest of the turn neither sets Tempo nor hides the message while
     * Tempo stays open (each move a pass apart, as a knob's arrive). */
    for (uint8_t v = 1; v < 128u; v = (uint8_t)(v + 9u)) cc(1, 21, v);
    assert(tc_state.tempo == tempo && tc_state.last_action == TC_CC_LEARNED && tc_cc_count(&tc_state) == 1u);
    screen(); assert(!strcmp(lines[56], "CC21 = TEMPO"));
    tc_state.ui = TC_UI_MENU; run(1); /* closed: then it plays (from the bottom: picks up at the tempo) */
    cc(1, 21, 0); cc(1, 21, 127); assert(tc_state.tempo == TC_TEMPOS - 1u && tc_state.cc_hush == 0u);
    cc(1, 21, 0); assert(tc_state.tempo == 0u);
    /* Mapped: moving it while another setting is open plays it, no learning. */
    editing(TC_MENU_SWING);
    cc(1, 21, 30); cc(1, 21, 127);
    assert(tc_cc_count(&tc_state) == 1u && tc_state.cc[0].target == TC_MENU_TEMPO && tc_state.tempo == TC_TEMPOS - 1u);
    /* Shift + right arms it: the next controller moved, mapped or not, is learned. */
    tc_state.shift_latched = true;
    assert(tc_shift_nav(&tc_state, &e, TC_NAV_RIGHT, now) && tc_state.cc_learn && tc_state.last_action == TC_CC_LEARN);
    tc_state.shift_latched = false;
    screen(); assert(!strcmp(lines[56], "MOVE A CONTROL"));
    cc(1, 21, 64);
    assert(tc_cc_count(&tc_state) == 1u && tc_state.cc[0].target == TC_MENU_SWING && tc_state.cc[0].hi == TC_SWINGS - 1u &&
           !tc_state.cc_learn);
    /* Not in an item that can't be mapped, nor with the Control channel off, nor arming there. */
    editing(TC_MENU_FACTORY); cc(1, 40, 1); assert(tc_cc_count(&tc_state) == 1u);
    tc_state.shift_latched = true; assert(!tc_shift_nav(&tc_state, &e, TC_NAV_RIGHT, now)); tc_state.shift_latched = false;
    editing(TC_MENU_ARP_RATE); control(0); tc_state.ui = TC_UI_EDIT; cc(1, 41, 1); assert(tc_cc_count(&tc_state) == 1u);
    control(1); tc_state.ui = TC_UI_EDIT;
    /* Map button: the action chosen there, even for a mapped controller. */
    editing(TC_MENU_CC_ACTION); tc_state.edit_value = TC_ACT_HOLD;
    tc_state.last_action = TC_NONE; screen(); assert(!strcmp(lines[56], "<CANCEL  PRESS A KEY"));
    cc(1, 50, 127);
    assert(tc_state.cc[1].kind == TC_CC_ACTION && tc_state.cc[1].cc == 50u && tc_state.cc[1].target == TC_ACT_HOLD);
    screen(); assert(!strcmp(lines[56], "CC50 = HOLD"));
    cc(1, 50, 0); assert(tc_state.cc[1].target == TC_ACT_HOLD); /* while Map button is open, it learns again */
    tc_state.ui = TC_UI_MENU;
    bool hold = tc_state.hold != 0u;
    cc(1, 50, 127); assert((tc_state.hold != 0u) != hold); /* then it works */
    cc(1, 50, 0); editing(TC_MENU_CC_ACTION); tc_state.edit_value = TC_ACT_ARP; cc(1, 21, 127); assert(tc_state.cc[0].kind == TC_CC_ACTION && tc_state.cc[0].target == TC_ACT_ARP);
    /* A full map. */
    for (uint8_t n = 60; tc_cc_count(&tc_state) < TC_CC_SLOTS; ++n) { editing(TC_MENU_ECHO); cc(1, n, 0); }
    editing(TC_MENU_ECHO); cc(1, 100, 0); assert(tc_state.last_action == TC_CC_FULL);
    screen(); assert(!strcmp(lines[56], "MIDI MAP FULL"));
    /* The edit screen's hint. */
    editing(TC_MENU_ECHOES); tc_state.last_action = TC_NONE; screen(); assert(!strcmp(lines[56], "<CANCEL O SET (LEARN)"));
    control(0); editing(TC_MENU_ECHOES); tc_state.last_action = TC_NONE; screen(); assert(!strcmp(lines[56], "<CANCEL   O SET"));
    puts("learning: a new controller in an open setting (the rest of its stream quiet until the item closes), a mapped one after Shift + right, Map button's action, map full");
}

/* --- 4. a setting from lo to hi --- */
static void scaling(void) {
    boot(true); control(1);
    slot(0, TC_CC_SETTING, 21, TC_MENU_TEMPO, 0, TC_TEMPOS - 1u, 1);
    bool hit[256] = {false};
    uint8_t last = 0;
    for (uint8_t v = 0; v < 128u; ++v) {
        cc(1, 21, v); hit[tc_state.tempo] = true;
        assert(tc_state.tempo >= last); last = tc_state.tempo; /* never backwards on the way up */
    }
    for (uint32_t i = 0; i < TC_TEMPOS; ++i) assert(hit[i]); /* every value reached */
    cc(1, 21, 0); assert(tc_state.tempo == 0u);
    /* Every mappable setting's every value from a sweep, and the ends exact. */
    uint32_t items = 0;
    for (uint8_t item = 0; item < TC_MENU_ITEMS; ++item) {
        if (!tc_cc_mappable(item)) continue;
        uint8_t n = tc_menu_values(item);
        if (n > 128u) continue; /* more values than a controller has (Scale, the programs): the ends below */
        slot(0, TC_CC_SETTING, 21, item, 0, (uint8_t)(n - 1u), 1);
        bool seen[256] = {false};
        for (uint8_t v = 0; v < 128u; ++v) { cc(1, 21, v); seen[tc_state.cc_pos[0]] = true; }
        for (uint32_t i = 0; i < n; ++i) { if (!seen[i]) fprintf(stderr, "item %u (%s) n %u: %u never\n", item, ui_menu_items[item], n, i); assert(seen[i]); }
        items++;
    }
    /* A narrowed range, and backwards. */
    slot(0, TC_CC_SETTING, 21, TC_MENU_TEMPO, 8, 20, 1); /* 80 .. 140 BPM */
    cc(1, 21, 0); assert(tc_state.tempo == 8u); cc(1, 21, 127); assert(tc_state.tempo == 20u);
    cc(1, 21, 64); assert(tc_state.tempo == 14u);
    slot(0, TC_CC_SETTING, 21, TC_MENU_TEMPO, 20, 8, 1);
    cc(1, 21, 0); assert(tc_state.tempo == 20u); cc(1, 21, 127); assert(tc_state.tempo == 8u);
    screen(); assert(!strcmp(lines[36], "TEMPO 80 BPM")); /* the play screen names it */
    /* lo == hi: any move sets that value. */
    slot(0, TC_CC_SETTING, 21, TC_MENU_TEMPO, 12, 12, 1); cc(1, 21, 99); assert(tc_state.tempo == 12u);
    /* Two slots on one controller. */
    slot(0, TC_CC_SETTING, 21, TC_MENU_TEMPO, 0, TC_TEMPOS - 1u, 1); slot(1, TC_CC_SETTING, 21, TC_MENU_SWING, 0, TC_SWINGS - 1u, 1);
    cc(1, 21, 127); assert(tc_state.tempo == TC_TEMPOS - 1u && tc_state.arp_swing == TC_SWINGS - 1u);
    /* Scale (more values than 127): both ends. */
    tc_cc_clear(&tc_state);
    slot(0, TC_CC_SETTING, 22, TC_MENU_ARP, 0, TC_ARP_MODES - 1u, 1);
    cc(1, 22, 127); assert(tc_state.arp == TC_ARP_MODES - 1u);
    /* While its setting is open, the value being edited follows. */
    editing(TC_MENU_ARP); cc(1, 22, 0); assert(tc_state.arp == 0u && tc_state.edit_value == 0u);
    tc_state.ui = TC_UI_PLAY;
    printf("scaling: lo to hi and backwards, every value of %u settings reached by a sweep, two slots on one controller\n", items);
}

/* --- 5. a knob between two steps stays put --- */
static void steady(void) {
    boot(true); control(1);
    slot(0, TC_CC_SETTING, 21, TC_MENU_SPREAD, 0, TC_SPREADS - 1u, 1); /* 4 steps: edges at 21, 63, 106 */
    cc(1, 21, 42); uint8_t at = tc_state.chord_spread; assert(at == 1u);
    uint32_t changes = tc_state.changes;
    for (uint32_t k = 0; k < 40u; ++k) { cc(1, 21, (uint8_t)(k & 1u ? 63u : 64u)); } /* jitter across the edge */
    assert(tc_state.chord_spread == 2u || tc_state.chord_spread == 1u);
    uint8_t settled = tc_state.chord_spread;
    for (uint32_t k = 0; k < 40u; ++k) { cc(1, 21, (uint8_t)(k & 1u ? 62u : 65u)); assert(tc_state.chord_spread == settled); }
    assert(tc_state.changes - changes <= 1u);
    cc(1, 21, 75); assert(tc_state.chord_spread == 2u); /* a quarter of a step past the edge: the next one */
    cc(1, 21, 55); assert(tc_state.chord_spread == 2u); /* back within a quarter of a step: kept */
    cc(1, 21, 52); assert(tc_state.chord_spread == 1u);
    /* Hold (two values) on a button: 0 off, 127 on. */
    slot(1, TC_CC_SETTING, 22, TC_MENU_HOLD, 0, 1, 1);
    cc(1, 22, 127); assert(tc_state.hold == 1u); cc(1, 22, 0); assert(tc_state.hold == 0u);
    puts("steady: a knob between two steps doesn't flicker, a quarter of a step past the edge moves on; Hold on a button");
}

/* --- 6. pickup --- */
static void pickup(void) {
    boot(true); control(1);
    slot(0, TC_CC_SETTING, 21, TC_MENU_TEMPO, 0, TC_TEMPOS - 1u, 0);
    uint8_t tempo = tc_state.tempo; /* 120 BPM, step 16 of 41: the knob about 51 */
    cc(1, 21, 0); assert(tc_state.tempo == tempo && tc_state.cc_turn == 1u);
    screen(); assert(!strcmp(lines[36], "TEMPO 120 BPM ^")); /* turn it up */
    cc(1, 21, 20); assert(tc_state.tempo == tempo);
    cc(1, 21, 60); assert(tc_state.cc_picked & 1u); /* passed it: picked up, and from here it follows */
    assert(tc_state.tempo == (60u * (TC_TEMPOS - 1u) + 63u) / 127u);
    cc(1, 21, 0); assert(tc_state.tempo == 0u);
    /* Changed some other way: the menu. */
    assert(tc_set_item(&tc_state, &e, TC_MENU_TEMPO, 30)); run(1);
    cc(1, 21, 3); assert(tc_state.tempo == 30u && tc_state.cc_turn == 1u);
    cc(1, 21, 127); assert(tc_state.tempo == TC_TEMPOS - 1u); /* swept past 30 in one move */
    /* Backwards: lower values up the knob, so "up" means turning it down. */
    slot(0, TC_CC_SETTING, 21, TC_MENU_TEMPO, TC_TEMPOS - 1u, 0, 0);
    assert(tc_set_item(&tc_state, &e, TC_MENU_TEMPO, 10)); run(1);
    cc(1, 21, 0); assert(tc_state.tempo == 10u && tc_state.cc_turn == 1u);
    screen(); assert(!strcmp(lines[36], "TEMPO 90 BPM ^"));
    cc(1, 21, 127); assert(tc_state.tempo == 0u);
    /* A preset: loading one with another tempo needs picking up again. */
    slot(0, TC_CC_SETTING, 21, TC_MENU_TEMPO, 0, TC_TEMPOS - 1u, 0);
    tc_state.tempo = 4u; tc_request(&tc_state, TC_REQUEST_SAVE, 0); run(1);
    cc(1, 21, 0); cc(1, 21, 127); assert(tc_state.tempo == TC_TEMPOS - 1u);
    tc_request(&tc_state, TC_REQUEST_LOAD, 0); run(1); assert(tc_state.last_action == TC_PRESET_LOADED && tc_state.tempo == 4u);
    cc(1, 21, 120); assert(tc_state.tempo == 4u && tc_state.cc_turn == 2u);
    screen(); assert(!strcmp(lines[36], "TEMPO 60 BPM v"));
    cc(1, 21, 2); assert(tc_state.tempo == 4u || tc_state.tempo < 4u); cc(1, 21, 30); assert(tc_state.tempo == (30u * 40u + 63u) / 127u);
    /* Jump: at once. */
    tc_state.cc[0].jump = 1u; assert(tc_set_item(&tc_state, &e, TC_MENU_TEMPO, 33)); run(1);
    cc(1, 21, 0); assert(tc_state.tempo == 0u);
    puts("pickup: a controller waits until it reaches the value (after the menu, a preset), showing which way to turn");
}

/* --- 7. actions --- */
static void actions(void) {
    boot(true); control(1);
    slot(0, TC_CC_ACTION, 30, TC_ACT_HOLD, 0, 0, 0);
    slot(1, TC_CC_ACTION, 31, TC_ACT_ADD7, 0, 0, 0);
    slot(2, TC_CC_ACTION, 32, TC_ACT_SHIFT, 0, 0, 0);
    slot(3, TC_CC_ACTION, 33, TC_ACT_ARP, 0, 0, 0);
    /* A toggle acts on the press only. */
    cc(1, 30, 127); assert(tc_state.hold == 1u && tc_state.last_action == TC_HOLD);
    cc(1, 30, 100); cc(1, 30, 0); assert(tc_state.hold == 1u);
    cc(1, 30, 64); assert(tc_state.hold == 0u); cc(1, 30, 63); assert(tc_state.hold == 0u);
    cc(1, 33, 127); assert(tc_state.arp != TC_ARP_OFF); cc(1, 33, 0); cc(1, 33, 127); assert(tc_state.arp == TC_ARP_OFF); cc(1, 33, 0);
    /* While held: Add 7th shapes the chord pads as a centre button's does, longer than a touch can be held. */
    tp_layout layout = tc_layout(&tc_state);
    cc(1, 31, 127); assert(tc_button_held(&tc_state, TC_ACT_ADD7) && (tc_pad_edit(&tc_state, layout, 0).add & TC_ADD7));
    screen(); assert(!strcmp(lines[36], "ADD 7TH WHILE HELD"));
    run(8000); assert(tc_button_held(&tc_state, TC_ACT_ADD7));
    cc(1, 31, 0); assert(!tc_button_held(&tc_state, TC_ACT_ADD7) && !(tc_pad_edit(&tc_state, layout, 0).add & TC_ADD7));
    /* Shift while held: a centre button then does its printed panel function. */
    tc_state.buttons[TC_FN_OCT_P3] = TC_ACT_HOLD;
    cc(1, 32, 127); assert(tc_shift(&tc_state));
    uint8_t voicing = tc_state.chord_voicing, hold = tc_state.hold;
    centre(TC_FN_OCT_P3, true); centre(TC_FN_OCT_P3, false);
    assert(tc_state.hold == hold && tc_state.chord_octaves != TC_OCTAVES_DEFAULT && tc_state.chord_voicing == voicing);
    cc(1, 32, 0); assert(!tc_shift(&tc_state));
    /* The Control channel off lets go of everything held. */
    cc(1, 31, 127); assert(tc_button_held(&tc_state, TC_ACT_ADD7));
    control(0); assert(!tc_button_held(&tc_state, TC_ACT_ADD7) && !tc_state.cc_held && !tc_state.cc_down);
    /* So does a slot changed (the editor), or cleared. */
    control(1); cc(1, 31, 127); assert(tc_button_held(&tc_state, TC_ACT_ADD7));
    tc_cc_restart(&tc_state, 1); assert(!tc_button_held(&tc_state, TC_ACT_ADD7));
    cc(1, 31, 0); cc(1, 31, 127); assert(tc_button_held(&tc_state, TC_ACT_ADD7));
    tc_cc_clear(&tc_state); assert(!tc_state.cc_held && !tc_cc_count(&tc_state));
    /* Shut down: nothing until centre wakes the board; Sleep: a controller wakes it. */
    slot(0, TC_CC_SETTING, 21, TC_MENU_TEMPO, 0, TC_TEMPOS - 1u, 1);
    tc_shut_down(&tc_state); cc(1, 21, 0); assert(tc_state.tempo != 0u && tc_state.dark == TC_SHUT_DOWN);
    tc_wake(&tc_state); tc_state.dark = TC_DARK; cc(1, 21, 0); assert(tc_state.tempo == 0u && tc_state.dark == TC_AWAKE);
    puts("actions: toggles on the press, while held as long as held (Add 7th, Shift), let go when off, changed or cleared");
}

/* --- 8. stored --- */
static void stored(void) {
    boot(true); control(5);
    slot(0, TC_CC_SETTING, 21, TC_MENU_TEMPO, 8, 20, 0);
    slot(3, TC_CC_ACTION, 64, TC_ACT_FILL, 0, 0, 0);
    slot(7, TC_CC_SETTING, 119, TC_MENU_SWING, 5, 0, 1);
    tc_state.changes++;
    uint32_t before = writes; run(ST_SAVE_DELAY_MS + 10u); assert(writes > before);
    before = writes; run(ST_SAVE_DELAY_MS + 10u); assert(writes == before); /* once */
    tc_cc_slot map[TC_CC_SLOTS]; memcpy(map, tc_state.cc, sizeof map);
    boot(false);
    assert(tc_state.control_ch == 5u && !memcmp(map, tc_state.cc, sizeof map) && tc_state.cc_pos[0] == TC_CC_NONE);
    /* A preset leaves it. */
    tc_request(&tc_state, TC_REQUEST_SAVE, 2); run(1);
    tc_cc_clear(&tc_state); tc_state.control_ch = 2u;
    tc_request(&tc_state, TC_REQUEST_LOAD, 2); run(1);
    assert(tc_state.last_action == TC_PRESET_LOADED && tc_state.control_ch == 2u && !tc_cc_count(&tc_state));
    /* The record over SysEx: as it is, and the last controller in. */
    memcpy(tc_state.cc, map, sizeof map); tc_state.control_ch = 5u;
    uint32_t p[ST_PAYLOAD];
    assert(st_get(ST_MIDI_MAP, 0, &tc_state, p) == ST_OK);
    assert(p[0] == (1u | 5u << 8) && p[1] == (TC_CC_SETTING | 21u << 8 | (uint32_t)TC_MENU_TEMPO << 16) && p[17] == (8u | 20u << 8));
    assert(p[4] == (TC_CC_ACTION | 64u << 8 | (uint32_t)TC_ACT_FILL << 16) && p[8] == (TC_CC_SETTING | 119u << 8 | (uint32_t)TC_MENU_SWING << 16 | 1u << 24));
    assert(p[24] == 5u && !p[ST_PAYLOAD - 1u]);
    run(1); cc(5, 77, 99);
    assert(st_get(ST_MIDI_MAP, 0, &tc_state, p) == ST_OK && p[ST_PAYLOAD - 1u] == (77u | 99u << 8 | 5u << 16 | (tm.cc_seen & 255u) << 24));
    assert(st_get(ST_MIDI_MAP, 2, &tc_state, p) == ST_BAD_VALUE); /* 2.4.13: slots 17-32 in record 1 */
    assert(st_get(ST_MIDI_MAP, 1, &tc_state, p) == ST_OK && !p[0] && !p[1]);
    /* Written by the editor: in use and stored at once (word 39 ignored). */
    assert(st_get(ST_MIDI_MAP, 0, &tc_state, p) == ST_OK);
    p[2] = TC_CC_SETTING | 22u << 8 | (uint32_t)TC_MENU_ECHO << 16; p[18] = (TC_ECHO_TIMES - 1u) << 8;
    before = writes;
    assert(st_put(ST_MIDI_MAP, 0, p, &tc_state, &e) == ST_OK && writes == before + 1u);
    assert(tc_state.cc[1].kind == TC_CC_SETTING && tc_state.cc[1].target == TC_MENU_ECHO && tc_state.cc[1].hi == TC_ECHO_TIMES - 1u);
    assert(st_put(ST_MIDI_MAP, 0, p, &tc_state, &e) == ST_OK && writes == before + 1u); /* the same again: not written */
    /* Refused: what makes no sense; the map as it was. */
    uint32_t q[ST_PAYLOAD];
    static const struct { uint32_t word, value; } bad[] = {
        {0, 2u}, {0, 1u | (uint32_t)TC_CONTROL_CHANNELS << 8}, {0, 1u | 1u << 16},
        {1, TC_CC_KINDS}, {1, TC_CC_SETTING | 120u << 8 | (uint32_t)TC_MENU_TEMPO << 16},
        {1, TC_CC_SETTING | 1u << 8 | (uint32_t)TC_MENU_SEQ_UNIT << 16}, {1, TC_CC_SETTING | 1u << 8 | (uint32_t)TC_MENU_CONTROL_CH << 16},
        {1, TC_CC_SETTING | 1u << 8 | (uint32_t)TC_MENU_TEMPO << 16 | 31u << 25}, /* 2.4.13: bits 25-29 the channel */ {1, TC_CC_ACTION | 1u << 8 | (uint32_t)TC_ACTIONS << 16},
        {1, TC_CC_EMPTY | 1u << 8}, {17, TC_TEMPOS}, {17, (uint32_t)TC_TEMPOS << 8}, {17, 1u << 16}, {33, 1u},
        {4, TC_CC_ACTION | 64u << 8 | (uint32_t)TC_ACT_FILL << 16 | 1u << 24}, {20, 1u},
        /* 2.4.13: a board button past the 43, a step option past BEND, channel 17, no source 3, pitch bend with a number */
        {1, TC_CC_BUTTON | 1u << 8 | TC_BOARD_BUTTONS << 16}, {1, TC_CC_STEP | 1u << 8 | (TC_FIELDS + 1u) << 16},
        {1, TC_CC_SETTING | 1u << 8 | (uint32_t)TC_MENU_TEMPO << 16 | 17u << 25}, {1, TC_CC_ACTION | 1u << 8 | 3u << 30},
        {1, TC_CC_ACTION | 1u << 8 | (uint32_t)TM_SRC_BEND << 30}};
    tc_cc_slot was[TC_CC_SLOTS]; memcpy(was, tc_state.cc, sizeof was);
    for (uint32_t k = 0; k < sizeof bad / sizeof bad[0]; ++k) {
        memcpy(q, p, sizeof q); q[bad[k].word] = bad[k].value;
        assert(st_put(ST_MIDI_MAP, 0, q, &tc_state, &e) == ST_BAD_VALUE);
        assert(!memcmp(was, tc_state.cc, sizeof was));
    }
    memset(q, 0, sizeof q); q[1] = 1u; assert(st_put(ST_MIDI_MAP, 0, q, &tc_state, &e) == ST_BAD_VALUE); /* slots without word 0 */
    /* All zeros: no map, Control channel off. */
    memset(q, 0, sizeof q); assert(st_put(ST_MIDI_MAP, 0, q, &tc_state, &e) == ST_OK && !tc_cc_count(&tc_state) && !tc_state.control_ch);
    /* Over SysEx itself (GET_RECORD, SET_RECORD). */
    assert(st_put(ST_MIDI_MAP, 0, p, &tc_state, &e) == ST_OK);
    uint8_t get[9] = {0xF0, 0x7D, 0x54, 0x42, SX_GET_RECORD, ST_MIDI_MAP, 0, 0, 0xF7};
    get[7] = (uint8_t)((128u - (SX_GET_RECORD + ST_MIDI_MAP) % 128u) % 128u);
    uint32_t n = sx_handle(get, sizeof get, &tc_state, &e, now);
    assert(n > 8u && sx_out[4] == (SX_GET_RECORD | 0x40u) && sx_out[5] == ST_OK && sx_out[6] == ST_MIDI_MAP);
    /* Factory settings: gone, and the Control channel off; saved so. */
    tc_state.ui = TC_UI_EDIT; tc_state.menu = TC_MENU_FACTORY; tc_state.group = TC_GROUP_ABOUT; tc_state.edit_value = 1u;
    tc_navigate(&tc_state, &e, TC_NAV_ENTER);
    assert(tc_state.last_action == TC_FACTORY_RESET && !tc_cc_count(&tc_state) && !tc_state.control_ch);
    run(ST_SAVE_DELAY_MS + 10u); boot(false); assert(!tc_cc_count(&tc_state) && !tc_state.control_ch);
    /* Compacting the sector keeps it. */
    control(9); memcpy(tc_state.cc, map, sizeof map); tc_state.changes++; run(ST_SAVE_DELAY_MS + 10u);
    for (uint32_t k = 0; k < 400u; ++k) { tc_state.tempo = (uint8_t)(k % TC_TEMPOS); tc_state.changes++; run(ST_SAVE_DELAY_MS + 1u); }
    boot(false); assert(tc_state.control_ch == 9u && !memcmp(map, tc_state.cc, sizeof map));
    puts("stored: 3 s after, once; after a restart and compacting; presets leave it; SysEx reads (last controller) and "
         "writes it, refuses nonsense; Factory settings clears it");
}

/* --- 9. the Control page --- */
static void page(void) {
    boot(true);
    uint8_t items[TC_GROUP_MAX];
    assert(tc_page_items(&tc_state, TC_GROUP_CONTROL, items) == 1u && items[0] == TC_MENU_CONTROL_CH); /* off: the rest hidden */
    assert(!strcmp(ui_group_names[TC_GROUP_CONTROL], "CONTROL") && !strcmp(ui_group_names[TC_GROUP_MIDI], "MIDI") &&
           TC_GROUP_CONTROL == TC_GROUP_MIDI + 1u);
    assert(tc_menu_values(TC_MENU_CONTROL_CH) == TC_CONTROL_CHANNELS && tc_menu_values(TC_MENU_CC_ACTION) == TC_ACTIONS);
    assert(!strcmp(ui_value_text(TC_MENU_CONTROL_CH, 0), "OFF") && !strcmp(ui_value_text(TC_MENU_CONTROL_CH, 16), "CH 16") &&
           !strcmp(ui_value_text(TC_MENU_CONTROL_CH, TM_CC_ANY), "ALL"));
    assert(tc_item_kind(TC_MENU_CONTROL_CH) == TC_ITEM_VALUE && tc_item_kind(TC_MENU_CC_ACTION) == TC_ITEM_BOARD &&
           tc_item_kind(TC_MENU_CC_CLEAR) == TC_ITEM_BOARD && tc_confirm_item(TC_MENU_CC_CLEAR));
    control(1);
    assert(tc_page_items(&tc_state, TC_GROUP_CONTROL, items) == 3u && items[1] == TC_MENU_CC_ACTION && items[2] == TC_MENU_CC_CLEAR);
    slot(0, TC_CC_SETTING, 21, TC_MENU_TEMPO, 0, 1, 0); slot(5, TC_CC_ACTION, 22, TC_ACT_HOLD, 0, 0, 0);
    assert(!strcmp(ui_value_text(TC_MENU_CC_CLEAR, 0), "2 MAPPED"));
    /* Settings > Control > Clear map: asked first. */
    tc_state.ui = TC_UI_MENU; tc_state.group = TC_GROUP_CONTROL; tc_state.menu = TC_MENU_CC_CLEAR;
    nav(TC_NAV_ENTER); assert(tc_state.ui == TC_UI_EDIT && !tc_state.edit_value);
    screen(); assert(!strcmp(lines[0], "CLEAR THE MIDI MAP?"));
    nav(TC_NAV_ENTER); assert(tc_cc_count(&tc_state) == 2u && tc_state.last_action == TC_RESET_KEPT);
    nav(TC_NAV_ENTER); nav(TC_NAV_UP); nav(TC_NAV_ENTER);
    assert(!tc_cc_count(&tc_state) && tc_state.last_action == TC_CC_CLEARED && tc_state.control_ch == 1u);
    screen(); assert(!strcmp(lines[56], "MIDI MAP CLEARED"));
    /* Map button's value is the board's (not stored, not the editor's). */
    assert(!tc_set_item(&tc_state, &e, TC_MENU_CC_ACTION, TC_ACT_HOLD));
    puts("page: Control after MIDI, its items hidden while off, names and values, Clear map asked first");
}

/* --- 2.3.2: the computer stops reading the board's USB data --- */
static void usb_watch(void) {
    boot(true); editing(TC_MENU_TEMPO); tc_state.last_action = TC_NONE;
    uint32_t t = 1000u, seq = tc_state.event_sequence;
    /* Transfers that go: never said, however many. */
    for (uint32_t i = 0; i < 5000u; ++i, ++t) tc_usb_watch(&tc_state, true, (i % 3u) != 0u, t);
    assert(tc_state.last_action == TC_NONE && tc_state.event_sequence == seq && !tc_state.usb_stalled);
    /* Busy 1.5 s: said once, at 1.5 s, and only once however long it stays. */
    tc_usb_watch(&tc_state, true, false, t++);
    uint32_t from = t;
    for (; t - from < TC_USB_STALL_MS; ++t) tc_usb_watch(&tc_state, true, true, t);
    assert(tc_state.last_action == TC_NONE && !tc_state.usb_stalled);
    tc_usb_watch(&tc_state, true, true, t);
    assert(tc_state.last_action == TC_USB_STALLED && tc_state.usb_stalled && tc_state.event_sequence == seq + 1u);
    screen(); assert(!strcmp(lines[56], "USB: PC NOT READING"));
    for (uint32_t i = 0; i < 20000u; ++i) tc_usb_watch(&tc_state, true, true, ++t);
    assert(tc_state.event_sequence == seq + 1u);
    /* Read again: said once. */
    tc_usb_watch(&tc_state, true, false, ++t); tc_usb_watch(&tc_state, true, false, ++t);
    assert(tc_state.last_action == TC_USB_BACK && !tc_state.usb_stalled && tc_state.event_sequence == seq + 2u);
    screen(); assert(!strcmp(lines[56], "USB: PC READING AGAIN"));
    /* Stalled, then the cable goes (or the computer sleeps): nothing said, and it starts over. */
    for (from = t; t - from <= TC_USB_STALL_MS; ++t) tc_usb_watch(&tc_state, true, true, t);
    assert(tc_state.usb_stalled && tc_state.event_sequence == seq + 3u);
    tc_usb_watch(&tc_state, false, false, ++t);
    assert(!tc_state.usb_stalled && tc_state.event_sequence == seq + 3u);
    tc_usb_watch(&tc_state, true, true, ++t); tc_usb_watch(&tc_state, true, true, t + TC_USB_STALL_MS - 1u);
    assert(!tc_state.usb_stalled);
    /* Wrap of the millisecond counter. */
    tc_state.usb_busy = false; tc_state.usb_stalled = false;
    tc_usb_watch(&tc_state, true, true, UINT32_MAX - 100u); tc_usb_watch(&tc_state, true, true, TC_USB_STALL_MS - 101u);
    assert(tc_state.usb_stalled);
    puts("usb: busy 1.5 s says PC NOT READING once, reading again says so once, a cable pulled or sleep says nothing");
}

/* --- 2.4.13: a channel per slot, notes and pitch bend, board buttons, step options (D), 32 slots --- */
static void note_in(uint8_t ch, uint8_t n, uint8_t v) {
    uint8_t p[4] = {(uint8_t)(v ? 0x09u : 0x08u), (uint8_t)((v ? 0x90u : 0x80u) | ((ch - 1u) & 15u)), n, v};
    tm_packet(p, now); run(1);
}
static void bend_in(uint8_t ch, uint16_t v) {
    uint8_t p[4] = {0x0Eu, (uint8_t)(0xE0u | ((ch - 1u) & 15u)), (uint8_t)(v & 127u), (uint8_t)(v >> 7)};
    tm_packet(p, now); run(1);
}
static uint8_t logical_of(uint32_t source, uint32_t index) { /* tc_poll's number of a board button */
    for (uint32_t a = 0; a < 4u; ++a) for (uint8_t bit = 0; bit < 16u; ++bit) {
        uint32_t s, i, l, al;
        if (tc_decode(addresses[a], bit, &s, &i, &l, &al) && s == source && i == index) return (uint8_t)l;
    }
    assert(0); return 0;
}
static void slot_of(uint32_t i, uint8_t kind, uint8_t src, uint8_t ch, uint8_t n, uint8_t target) {
    tc_cc_restart(&tc_state, i);
    tc_state.cc[i] = (tc_cc_slot){kind, n, target, 0, 0, 0, src, ch};
    assert(tc_cc_slot_ok(&tc_state.cc[i]));
    tc_cc_watch(&tc_state);
}
static void controller(void) {
    boot(true); control(1);
    tc_state.ui = TC_UI_PLAY;
    /* a note on its own channel (10) as root button D: the key, on the play screen; another channel nothing */
    uint8_t d = logical_of(TC_ROOT, 2u);
    slot_of(0, TC_CC_BUTTON, TM_SRC_NOTE, 10u, 36u, d);
    note_in(10, 36, 100); assert(tc_state.tonic == 2u); note_in(10, 36, 0);
    assert(tc_set_item(&tc_state, &e, TC_MENU_SCALE, tc_state.scale)); tc_state.tonic = 0u; tc_state.changes++;
    uint32_t presses = tc_state.presses[d];
    note_in(9, 36, 100); note_in(9, 36, 0); assert(tc_state.presses[d] == presses && tc_state.releases[d] == presses);
    /* the Control channel Off: the map ignores the controller, a slot with its own channel too */
    control(0); note_in(10, 36, 100); note_in(10, 36, 0); assert(tc_state.presses[d] == presses); control(1);
    /* notes on the Scale lock channel are Scale lock's */
    tc_state.lock = 10u; run(5); note_in(10, 36, 100); note_in(10, 36, 0); assert(tc_state.presses[d] == presses);
    tc_state.lock = 0u; run(5);
    note_in(10, 36, 100); note_in(10, 36, 0); assert(tc_state.presses[d] == presses + 1u && tc_state.releases[d] == presses + 1u);
    /* on the step screen: a CC on the Control channel as U1, held, and one as right: Length +1; a note as root button 3:
     * lane 3 muted, then playing again */
    tc_state.ui = TC_UI_STEPS; tc_state.step_page = 0u; tc_state.step_held = LN_NONE; tc_state.step_mask = 0u; run(5);
    slot_of(1, TC_CC_BUTTON, TM_SRC_CC, 0u, 20u, logical_of(TC_SCALE, TC_U3 - 2u));
    slot_of(2, TC_CC_BUTTON, TM_SRC_CC, 0u, 21u, logical_of(TC_NAV, TC_NAV_RIGHT));
    uint32_t length = ln_length(&ln.lanes[0]);
    cc(1, 20, 127); assert(tc_state.lane_hold == TC_MENU_LANE_LENGTH);
    cc(1, 21, 127); cc(1, 21, 0); assert(ln_length(&ln.lanes[0]) == length + 1u);
    cc(1, 20, 0); assert(!tc_state.lane_hold);
    slot_of(3, TC_CC_BUTTON, TM_SRC_NOTE, 2u, 40u, logical_of(TC_ROOT, 2u));
    note_in(2, 40, 90); note_in(2, 40, 0); assert(ln.lanes[2].mute);
    note_in(2, 40, 90); note_in(2, 40, 0); assert(!ln.lanes[2].mute);
    /* a slot changed while its button is held lets go of it */
    cc(1, 20, 127); assert(tc_state.lane_hold); tc_cc_restart(&tc_state, 1u); assert(!tc_state.lane_hold);
    cc(1, 20, 0);
    /* D: a step option on the steps held; none held, nothing */
    ln_lane *l = &ln.lanes[0];
    l->kind = LN_MELODY; l->channel = 5u; ln_changed(0u, LN_STEPS); tc_state.changes++; run(2);
    tc_state.step_mask = 1u << 2; tc_state.step_held = 2u;
    slot_of(4, TC_CC_STEP, TM_SRC_CC, 0u, 30u, (uint8_t)(1u + TC_FIELD_VELOCITY));
    cc(1, 30, 127); assert(l->steps[2].on && l->steps[2].velocity == 127u && tc_state.step_field == TC_FIELD_VELOCITY);
    cc(1, 30, 0); assert(l->steps[2].velocity == 1u);
    tc_state.step_field = TC_FIELD_GATE;
    slot_of(5, TC_CC_STEP, TM_SRC_CC, 0u, 31u, 0u); /* the option chosen (M1..M7, Shift + M7) */
    cc(1, 31, 127); assert(l->steps[2].gate == 10u && tc_state.step_field == TC_FIELD_GATE);
    cc(1, 31, 0); assert(l->steps[2].gate == 1u);
    slot_of(6, TC_CC_STEP, TM_SRC_BEND, 3u, 0u, (uint8_t)(1u + TC_FIELD_BEND)); /* the wheel on channel 3: a step's bend 1:1 */
    bend_in(3, 16383); assert(l->steps[2].bend == LN_BEND_MAX);
    bend_in(3, 8192); assert(l->steps[2].bend == 0);
    bend_in(3, 0); assert(l->steps[2].bend == LN_BEND_MIN);
    bend_in(3, 8192 + 2048); assert(l->steps[2].bend == 1024);
    tc_state.melody_ch = 4u; tc_state.changes++; run(2); /* the lane's channel shared: refused */
    bend_in(3, 8192); assert(l->steps[2].bend == 1024 && tc_state.last_action == TC_BEND_SHARED);
    tc_state.melody_ch = 0u; tc_state.changes++;
    tc_state.step_mask = 0u; tc_state.step_held = LN_NONE;
    cc(1, 30, 64); assert(l->steps[2].velocity == 1u); /* no step held */
    /* 32 slots: the last one stored in the second record, back after a restart with its channel and source */
    slot_of(TC_CC_SLOTS - 1u, TC_CC_BUTTON, TM_SRC_NOTE, 16u, 127u, logical_of(TC_NAV, TC_NAV_ENTER));
    tc_state.changes++; run(4000);
    tc_cc_slot was[TC_CC_SLOTS]; memcpy(was, tc_state.cc, sizeof was);
    boot(false);
    assert(!memcmp(was, tc_state.cc, sizeof was) && tc_state.cc[TC_CC_SLOTS - 1u].channel == 16u && tc_state.cc[6].src == TM_SRC_BEND);
    uint32_t q[ST_PAYLOAD];
    assert(st_get(ST_MIDI_MAP, 1, &tc_state, q) == ST_OK && q[0] == 1u && q[16] >> 30 == TM_SRC_NOTE && ((q[16] >> 25) & 31u) == 16u);
    puts("controller (2.4.13): a channel per slot (and the Control channel Off ignoring all), notes (Scale lock's channel theirs) "
         "and pitch bend, board buttons (the key, U1 held + right = Length, root 3 = mute lane 3, let go when the slot changes), "
         "step options on the held steps (one chosen or the one M1..M7 chose, the wheel = a step's bend 1:1, refused on a shared "
         "channel, nothing with no step held), 32 slots in two records kept over a restart");
}
int main(void) {
    input();
    mappable();
    learning();
    scaling();
    steady();
    pickup();
    actions();
    stored();
    page();
    usb_watch();
    controller();
    puts("PASS: cc: the MIDI map (controllers on the Control channel, learning, scaling, pickup, actions, storage, the page)");
    return 0;
}
