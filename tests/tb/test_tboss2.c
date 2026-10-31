/* SPDX-License-Identifier: GPL-3.0-or-later - TBOSS 2.0 on the real code (engine, controls, settings.c on a fake
 * sector 11, the screens): the Turing arpeggiator pattern (a loop of held notes drawn at random, Lock and Loop,
 * seeded, a new loop for other notes), the step screen's Shift + buttons (move, copy, paste, clear a lane: lanes.c
 * ln_rotate as the editor's), chained progressions (the chain record, playing in turn, Chain on the Sequence page and
 * Shift + root button 9, kept over a restart and compaction) with each chord's chance (rest or borrowed chord, its
 * record, recording again), presets' own chord sets (Keep chords: saved with a preset, brought back by loading it,
 * the board's back for one without, changes saved before another covers them, slot 9, compaction), the U3 button as
 * Shift (once, latched, held) with the buttons under the screen, Sleep after and Shut down, and the I/O test's
 * buttons. Every constant from the firmware's headers. */
#include "../../src/tb/settings.h"
#include "../../src/tb/sequence.h"
#include "../../src/tb/midi_in.h"
#include "../../src/tb/flash_store.h"
#include "../../src/tb/display.h"
#include "../../src/tb/leds.h"
#include "../../src/tb/jack.h"
#include "../../src/tb/nor.h"
#include "../../src/tb/ui.h"
#include "../../src/tb/chord_name.h"
#include "../../src/tb/controls_internal.h"
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
/* The screen: each line of text by where it starts. */
static char lines[64][32];
static uint32_t cleared;
void td_clear(void) { memset(lines, 0, sizeof lines); cleared++; }
uint32_t td_text(uint32_t x, uint32_t y, const char *text, uint32_t scale) {
    if (y < 64u && !x) snprintf(lines[y], sizeof lines[y], "%s", text);
    return x + 6u * scale * (uint32_t)strlen(text);
}

static tp_engine e;
static ts_scan scan;
static uint32_t now = 1000u;
static uint8_t sounding[16][128];
static uint8_t on_note[4096], on_ch[4096];
static uint32_t ons, logged;
static bool writer(const uint8_t p[4], uint32_t token) {
    (void)token;
    uint32_t ch = p[1] & 15u;
    if (p[0] == TK_ON) { assert(!sounding[ch][p[2]]); sounding[ch][p[2]] = 1u; ons++; }
    if (p[0] == TK_ON && logged < 4096u) { on_note[logged] = p[2]; on_ch[logged++] = (uint8_t)ch; }
    if (p[0] == TK_OFF) { assert(sounding[ch][p[2]]); sounding[ch][p[2]] = 0u; }
    return true;
}
static uint32_t heard(uint32_t ch) {
    uint32_t n = 0;
    for (uint32_t p = 0; p < 128u; ++p) n += sounding[ch][p];
    return n;
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
static void root_button(uint32_t i) { tap_bit(0x39, top_bit[i]); }
static void u3(bool down) { hold_bit(0x38, top_bit[TC_U3], down); }
static void touch(uint32_t k, bool down) { scan.values[k] = down ? 3000u : 0u; run(60); }
static uint8_t pad(uint32_t column, uint32_t row) { return (uint8_t)(4u * column + row); }
static void boot(bool erase) {
    tc_init(&tc_state); memset(&e, 0, sizeof e); memset(&scan, 0, sizeof scan); memset(raw, 0, sizeof raw);
    memset(sounding, 0, sizeof sounding); ons = 0u; logged = 0u;
    tm_reset();
    for (uint8_t s = 0; s < TC_CHORD_SETS; ++s) { tc_chord_set none; memset(&none, 0, sizeof none); assert(tc_chord_set_put(s, &none)); }
    sq_clear(&sq, SQ_UNIT_BAR, 0u);
    if (erase) fs_erase();
    (void)st_load(&tc_state);
    run(600); assert(e.state == TK_ACTIVE);
}
static void quiet(void) { run(300); for (uint32_t ch = 0; ch < 16u; ++ch) assert(!heard(ch)); }

/* --- 1. the Turing pattern --- */
/* The arpeggio's notes (Note Ons on the chord channel) while pad k is held for ms. */
static uint32_t arpeggio(uint32_t k, uint32_t ms, uint8_t *out, uint32_t max) {
    logged = 0u;
    touch(k, true); run(ms); touch(k, false); quiet();
    uint32_t n = 0;
    for (uint32_t i = 0; i < logged && n < max; ++i) if (on_ch[i] == tc_half_ch(&tc_state, 0u)) out[n++] = on_note[i];
    return n;
}
static void turing(void) {
    assert(TC_ARP_TURING + 1u == TC_ARP_MODES && TP_TURING == TC_ARP_LOOP_MIN + TC_ARP_LOOPS - 1u);
    /* Stored as 0 while at the defaults (80 %, 8 steps), so older records read them. */
    boot(true);
    assert(tc_state.arp_lock == TC_ARP_LOCK_DEFAULT && tc_state.arp_loop == TC_ARP_LOOP_DEFAULT);
    uint32_t p[ST_PAYLOAD]; assert(st_get(ST_LIVE, 0, &tc_state, p) == ST_OK && !(p[17] >> 8));
    for (uint8_t lock = 0; lock < TC_ARP_LOCKS; ++lock) for (uint8_t loop = 0; loop < TC_ARP_LOOPS; ++loop) {
        tc_controls c = tc_state; c.arp = TC_ARP_TURING; c.arp_lock = lock; c.arp_loop = loop;
        assert(st_get(ST_LIVE, 0, &c, p) == ST_OK);
        tc_controls d; tc_init(&d); assert(st_put(ST_LIVE, 0, p, &d, &e) == ST_OK);
        assert(d.arp == TC_ARP_TURING && d.arp_lock == lock && d.arp_loop == loop);
    }
    assert(st_put(ST_LIVE, 0, p, &tc_state, &e) == ST_OK);
    /* Lock 100 %: the loop repeats exactly, Loop steps long, every note a held one. */
    uint8_t a[256], b[256], held[TP_ARP_POOL];
    for (uint8_t loop = 0; loop < TC_ARP_LOOPS; loop += 3u) {
        boot(true);
        tc_state.arp = TC_ARP_TURING; tc_state.arp_lock = TC_ARP_LOCKS - 1u; tc_state.arp_loop = loop; tc_state.arp_octaves = 1u;
        uint32_t length = TC_ARP_LOOP_MIN + loop, n = arpeggio(pad(1, 1), 125u * 4u * length + 1000u, a, sizeof a);
        assert(n >= 3u * length);
        for (uint32_t i = length; i < n; ++i) assert(a[i] == a[i - length]);
        /* not a shorter loop by chance for every length: the notes of a loop are more than one */
        touch(pad(1, 1), true); run(200);
        uint32_t m = tp_arp_pool(&e, held); touch(pad(1, 1), false); quiet();
        assert(m >= 6u); /* the chord over two octaves */
        for (uint32_t i = 0; i < n; ++i) { bool in = false; for (uint32_t j = 0; j < m; ++j) in = in || held[j] == a[i]; assert(in); }
    }
    /* Seeded: the same start plays the same loop; Lock 0 %: new every time round. */
    now = 1000u; boot(true); tc_state.arp = TC_ARP_TURING; tc_state.arp_lock = TC_ARP_LOCKS - 1u;
    uint32_t n1 = arpeggio(pad(1, 1), 2000u, a, sizeof a);
    now = 1000u; boot(true); tc_state.arp = TC_ARP_TURING; tc_state.arp_lock = TC_ARP_LOCKS - 1u;
    uint32_t n2 = arpeggio(pad(1, 1), 2000u, b, sizeof b);
    assert(n1 == n2 && !memcmp(a, b, n1));
    boot(true); tc_state.arp = TC_ARP_TURING; tc_state.arp_lock = 0u; tc_state.arp_octaves = 2u;
    uint32_t n = arpeggio(pad(1, 1), 6000u, a, sizeof a), length = TC_ARP_LOOP_MIN + TC_ARP_LOOP_DEFAULT, same = 0;
    for (uint32_t i = length; i < n; ++i) same += a[i] == a[i - length];
    assert(n > 4u * length && same < (n - length) * 2u / 3u); /* about a third by chance with 9 notes, never all */
    /* Lock 80 % in between: mostly kept, sometimes drawn again. */
    boot(true); tc_state.arp = TC_ARP_TURING; tc_state.arp_octaves = 2u;
    n = arpeggio(pad(1, 1), 8000u, a, sizeof a); same = 0;
    for (uint32_t i = length; i < n; ++i) same += a[i] == a[i - length];
    assert(same > (n - length) / 2u && same < n - length);
    /* Other held notes start a new loop, from the new notes. */
    boot(true); tc_state.arp = TC_ARP_TURING; tc_state.arp_lock = TC_ARP_LOCKS - 1u;
    touch(pad(1, 1), true); run(1500);
    uint32_t first = e.turing_notes; uint8_t was[TP_TURING]; memcpy(was, e.turing, sizeof was);
    touch(pad(4, 1), true); run(1500);
    assert(e.turing_notes != first && memcmp(was, e.turing, sizeof was));
    touch(pad(1, 1), false); touch(pad(4, 1), false); quiet();
    /* The page shows Lock and Loop with Turing only; their names. */
    uint8_t items[TC_GROUP_MAX];
    tc_state.arp = TC_ARP_UP; uint8_t without = tc_page_items(&tc_state, TC_GROUP_ARP, items);
    tc_state.arp = TC_ARP_TURING; assert(tc_page_items(&tc_state, TC_GROUP_ARP, items) == without + 2u);
    assert(items[1] == TC_MENU_ARP_LOCK && items[2] == TC_MENU_ARP_LOOP);
    assert(!strcmp(ui_value_text(TC_MENU_ARP, TC_ARP_TURING), "TURING"));
    assert(!strcmp(ui_value_text(TC_MENU_ARP_LOCK, TC_ARP_LOCKS - 1u), "100%") && !strcmp(ui_value_text(TC_MENU_ARP_LOCK, 0), "0%"));
    assert(!strcmp(ui_value_text(TC_MENU_ARP_LOOP, TC_ARP_LOOP_DEFAULT), "8 STEPS"));
    assert(tc_set_item(&tc_state, &e, TC_MENU_ARP_LOCK, 5u) && tc_state.arp_lock == 5u);
    assert(!tc_set_item(&tc_state, &e, TC_MENU_ARP_LOOP, TC_ARP_LOOPS));
    tc_state.arp = TC_ARP_OFF;
    puts("turing: Lock 100 % repeats the loop exactly at every Loop, held notes only, seeded; Lock 0 % new each time round; "
         "80 % mostly kept; new notes, new loop; stored as 0 at the defaults; the page and names");
}

/* --- 2. the step screen's Shift + buttons (the editor's copy, paste and shift do the same, tb-lanes.js) --- */
static void lanes_shift(void) {
    boot(true);
    ln_lane *l = &ln.lanes[0];
    ln_set_length(l, 5u);
    for (uint32_t i = 0; i < LN_STEPS; ++i) { l->steps[i].on = i % 3u == 0u; l->steps[i].value = (uint8_t)i; }
    ln_lane before = *l;
    ln_rotate(l, 1);
    for (uint32_t i = 0; i < 5u; ++i) assert(l->steps[(i + 1u) % 5u].value == i);
    for (uint32_t i = 5u; i < LN_STEPS; ++i) assert(l->steps[i].value == i); /* past the length: where they were */
    ln_rotate(l, -1); assert(!memcmp(l, &before, sizeof before));
    for (uint32_t k = 0; k < 5u; ++k) ln_rotate(l, 1);
    assert(!memcmp(l, &before, sizeof before)); /* round its length: as it was */
    /* On the step screen: Shift (U3 tapped) + the buttons under the screen. */
    tc_state.u3_shift = 1u; tc_state.ui = TC_UI_STEPS; tc_state.lane = 0u;
    uint32_t changes = ln_changes;
    u3(true); u3(false); nav(TC_NAV_RIGHT);
    assert(tc_state.last_action == TC_LANE_SHIFTED && l->steps[1].value == 0u && ln_changes > changes && !tc_state.shift_once);
    u3(true); u3(false); nav(TC_NAV_LEFT); assert(!memcmp(l, &before, sizeof before));
    tc_state.lane = 1u; u3(true); u3(false); nav(TC_NAV_DOWN); assert(tc_state.last_action == TC_LANE_NO_COPY);
    tc_state.lane = 0u; u3(true); u3(false); nav(TC_NAV_UP); assert(tc_state.last_action == TC_LANE_COPIED);
    tc_state.lane = 2u; u3(true); u3(false); nav(TC_NAV_DOWN);
    assert(tc_state.last_action == TC_LANE_PASTED && !memcmp(&ln.lanes[2], &before, sizeof before)); /* steps and settings */
    /* Clear asks first: Shift + centre again within the time clears the steps, the settings stay. */
    u3(true); u3(false); nav(TC_NAV_ENTER); assert(tc_state.last_action == TC_LANE_CLEAR_ASK && ln.lanes[2].steps[0].on);
    u3(true); u3(false); nav(TC_NAV_ENTER);
    assert(tc_state.last_action == TC_LANE_CLEARED && ln_length(&ln.lanes[2]) == 5u);
    for (uint32_t i = 0; i < LN_STEPS; ++i) assert(!ln.lanes[2].steps[i].on && !ln.lanes[2].steps[i].value);
    u3(true); u3(false); nav(TC_NAV_ENTER); run(TC_CLEAR_ASK_MS + 100u); u3(true); u3(false); nav(TC_NAV_ENTER);
    assert(tc_state.last_action == TC_LANE_CLEAR_ASK); /* too late: asks again */
    /* The bottom line says what they do while Shift is on. */
    u3(true); screen(); assert(!strcmp(lines[56], "<>MOVE ^CPY vPST OCLR")); u3(false); u3(true); u3(false);
    run(3000); assert(!ln_dirty); /* saved, as the step screen's edits are */
    tc_state.ui = TC_UI_PLAY;
    puts("lanes: moved one later and earlier round the length (the rest stay), copied, pasted with its settings, "
         "cleared after asking; on the step screen with Shift, its bottom line; saved");
}

/* --- 3. chained progressions and each chord's chance --- */
static void put_progression(uint8_t slot, const uint8_t *pads_of, uint32_t n) {
    sq_prog p; sq_clear(&p, SQ_UNIT_BEAT, 0u);
    for (uint32_t i = 0; i < n; ++i) assert(sq_tap(&p, pads_of[i], 100u, 0u, (tc_chord){0, 0, 0}));
    uint32_t w[ST_PAYLOAD]; sq_encode(&p, w);
    assert(st_put(ST_PROG, slot, w, &tc_state, &e) == ST_OK);
}
static void put_chain(const uint8_t *links, uint32_t n) {
    sq_chain_list c; memset(&c, 0, sizeof c); c.count = (uint8_t)n; memcpy(c.links, links, n);
    uint32_t w[ST_PAYLOAD]; sq_chain_encode(&c, w);
    assert(st_put(ST_CHAIN, 0, w, &tc_state, &e) == ST_OK);
}
/* Which progression and chord play over ms: each change of (slot, chord) as slot * 64 + chord. */
static uint32_t follow(uint32_t ms, uint32_t *out, uint32_t max) {
    uint32_t n = 0, last = UINT32_MAX;
    for (uint32_t t = 0; t < ms; ++t) {
        run(1);
        uint32_t at = e.seq_running ? e.seq_slot * 64u + e.seq_entry : UINT32_MAX;
        if (at != last && at != UINT32_MAX && n < max) out[n++] = at;
        last = at;
    }
    return n;
}
static void chains(void) {
    boot(true);
    /* The record: refused when it isn't one; all zero is no chain. */
    uint32_t w[ST_PAYLOAD];
    sq_chain_list c = {3u, {0, 0, 1}}, d;
    sq_chain_encode(&c, w); assert(w[0] == (1u | 3u << 8) && w[1] == 0x100u);
    assert(sq_chain_decode(w, &d) && !memcmp(&c, &d, sizeof c));
    w[1] |= 8u; assert(!sq_chain_decode(w, &d)); w[1] &= ~8u;      /* slot 9 */
    w[1] |= 1u << 12; assert(!sq_chain_decode(w, &d)); w[1] = 0x100u; /* past the count */
    w[0] = 1u | (SQ_CHAIN + 1u) << 8; assert(!sq_chain_decode(w, &d)); w[0] = 1u | 3u << 8;
    w[5] = 1u; assert(st_put(ST_CHAIN, 0, w, &tc_state, &e) == ST_BAD_VALUE); w[5] = 0u;
    assert(st_get(ST_CHAIN, 0, &tc_state, w) == ST_ABSENT && st_get(ST_CHAIN, 1, &tc_state, w) == ST_BAD_VALUE);
    /* P1: two chords, P2: one, the chain P1 > P1 > P2 (a beat each). */
    const uint8_t one[2] = {pad(0, 3), pad(3, 3)}, two[1] = {pad(4, 3)};
    put_progression(0, one, 2); put_progression(1, two, 1);
    const uint8_t links[3] = {0, 0, 1}; put_chain(links, 3);
    assert(st_get(ST_CHAIN, 0, &tc_state, w) == ST_OK && sq_chain_decode(w, &d) && d.count == 3u);
    tc_state.chain = 1u; tc_state.transport = true;
    uint32_t seen[64], n = follow(10000u, seen, 64);
    const uint32_t order[5] = {0, 1, 0, 1, 64};
    assert(n >= 15u);
    for (uint32_t i = 0; i < n; ++i) assert(seen[i] == order[i % 5u]);
    assert(e.seq_chained);
    /* The play screen says where: C3/3 P2 ... */
    while (e.seq_link != 2u) run(1);
    screen(); assert(!strncmp(lines[56], "C3/3 P2 ", 8u));
    tc_state.transport = false; quiet();
    /* Shift + root button 9: Chain on and off; Shift + root 1..8 plays that one, Chain off. */
    tc_state.u3_shift = 1u;
    u3(true); u3(false); root_button(SQ_SLOTS); assert(!tc_state.chain && tc_state.last_action == TC_CHAIN);
    screen(); assert(!strcmp(lines[36], "CHAIN OFF"));
    u3(true); u3(false); root_button(SQ_SLOTS); assert(tc_state.chain);
    screen(); assert(!strcmp(lines[36], "CHAIN ON: 112"));
    u3(true); u3(false); root_button(1u); assert(!tc_state.chain && tc_state.transport && tc_state.prog_slot == 1u);
    n = follow(2000u, seen, 64); for (uint32_t i = 0; i < n; ++i) assert(seen[i] == 64u); /* P2 alone */
    tc_state.transport = false; quiet();
    /* Chain on the Sequence page (a board setting: presets keep it), stored with the settings. */
    assert(tc_set_item(&tc_state, &e, TC_MENU_CHAIN, 1u) && tc_state.chain == 1u);
    run(ST_SAVE_DELAY_MS + 100u);
    boot(false); assert(tc_state.chain == 1u && sq_chain.count == 3u && sq_slots[1].count == 1u);
    tc_state.request = TC_REQUEST_SAVE; tc_state.request_slot = 0u; run(20);
    tc_state.chain = 0u; tc_state.request = TC_REQUEST_LOAD; tc_state.request_slot = 0u; run(20);
    assert(tc_state.last_action == TC_PRESET_LOADED && tc_state.chain == 0u);
    /* A chain of empty progressions plays nothing (the selected one instead, here none). */
    const uint8_t empties[2] = {5, 6}; put_chain(empties, 2); tc_state.chain = 1u; tc_state.prog_slot = 7u; sq_clear(&sq, SQ_UNIT_BEAT, 0u);
    tc_state.transport = true; run(1000); assert(!e.seq_running); tc_state.transport = false;
    /* Cleared: zeros. */
    memset(w, 0, sizeof w); assert(st_put(ST_CHAIN, 0, w, &tc_state, &e) == ST_OK && st_get(ST_CHAIN, 0, &tc_state, w) == ST_ABSENT);
    puts("chain: the record and its checks, P1 > P1 > P2 in turn and round again, C3/3 P2 on screen, Shift + root 9 on and "
         "off (CHAIN ON: 112), Shift + root 2 plays P2 alone, Chain stored and the board's, empty links skipped, cleared");
}
static void chances(void) {
    boot(true);
    uint32_t w[ST_PAYLOAD];
    /* The record: a byte per chord; refused when wrong; zeros are every chord at 100 %. */
    sq_prog p; sq_clear(&p, SQ_UNIT_BEAT, 0u);
    for (uint32_t i = 0; i < SQ_ENTRIES; ++i) p.entries[i].chance = (uint8_t)(i % SQ_CHANCES | (i & 1u ? SQ_BORROW : 0u));
    p.count = SQ_ENTRIES; sq_chances_encode(&p, w);
    sq_prog q; memset(&q, 0, sizeof q); assert(sq_chances_decode(w, &q));
    for (uint32_t i = 0; i < SQ_ENTRIES; ++i) assert(q.entries[i].chance == p.entries[i].chance);
    w[1] |= SQ_CHANCES; assert(!sq_chances_decode(w, &q)); w[1] &= ~0xFFu;
    w[1] |= 0x20u; assert(!sq_chances_decode(w, &q)); w[1] &= ~0xFFu;
    w[10] |= 1u << 16; assert(!sq_chances_decode(w, &q)); w[10] &= 0xFFFFu; /* past the 38th */
    w[0] = 0u; assert(!sq_chances_decode(w, &q));
    memset(w, 0, sizeof w); assert(sq_chances_decode(w, &q) && !q.entries[5].chance);
    /* P1 = C (pad 0) then F; the C at 10 % with a rest otherwise: mostly silent, the F always. */
    const uint8_t two[2] = {pad(0, 3), pad(3, 3)}; put_progression(0, two, 2);
    sq_prog odds = sq_slots[0]; odds.entries[0].chance = SQ_CHANCES - 1u; sq_chances_encode(&odds, w);
    assert(st_put(ST_CHANCE, 0, w, &tc_state, &e) == ST_OK && sq.entries[0].chance == SQ_CHANCES - 1u);
    assert(st_get(ST_CHANCE, 0, &tc_state, w) == ST_OK && st_get(ST_CHANCE, 1, &tc_state, w) == ST_ABSENT);
    tc_state.prog_slot = 0u; tc_state.transport = true;
    uint32_t plays = 0, rests = 0, others = 0;
    for (uint32_t loop = 0; loop < 60u; ++loop) {
        while (e.seq_entry != 0u) run(1);
        run(20);
        if (e.seq_plays == SQ_PLAYS) plays++; else if (e.seq_plays == SQ_RESTS) rests++; else others++;
        if (e.seq_plays == SQ_RESTS) assert(!heard(tc_half_ch(&tc_state, 0u)) && !e.keys[TP_SEQ].captured.count);
        while (e.seq_entry == 0u) run(1);
        run(20); assert(e.seq_plays == SQ_PLAYS && heard(tc_half_ch(&tc_state, 0u)));
    }
    assert(!others && rests > 40u && plays > 0u);
    tc_state.transport = false; quiet();
    /* Otherwise borrowed: C major's I as the parallel minor's i. */
    odds.entries[0].chance = (uint8_t)(SQ_CHANCES - 1u) | SQ_BORROW; sq_chances_encode(&odds, w);
    assert(st_put(ST_CHANCE, 0, w, &tc_state, &e) == ST_OK);
    tp_layout minor = tc_layout(&tc_state); minor.mode = tc_mode_buttons[7];
    tp_notes want; assert(tc_pad_with(minor, pad(0, 3), (tc_chord){0, 0, 0}, &want));
    tc_state.transport = true; uint32_t borrowed = 0;
    for (uint32_t loop = 0; loop < 30u; ++loop) {
        while (e.seq_entry != 0u) run(1);
        run(20);
        if (e.seq_plays != SQ_BORROWS) { while (e.seq_entry == 0u) run(1); continue; }
        borrowed++;
        uint32_t ch = tc_half_ch(&tc_state, 0u);
        assert(heard(ch) == want.count);
        for (uint32_t i = 0; i < want.count; ++i) assert(sounding[ch][want.notes[i]]);
        char name[TN_MAX]; (void)tn_name(&want, tc_pad_root(minor, pad(0, 3)), name);
        screen(); assert(!strcmp(lines[strlen(name) <= 10u ? 18u : 22u], name));
        while (e.seq_entry == 0u) run(1);
    }
    assert(borrowed > 20u);
    tc_state.transport = false; quiet();
    /* Recording P1 again: every chord plays (its chances back to 100 %), stored so. */
    tc_state.recording = true; sq_clear(&sq, SQ_UNIT_BEAT, 0u); assert(sq_tap(&sq, pad(1, 3), 100u, 0u, (tc_chord){0, 0, 0}));
    tc_finish_recording(&tc_state); run(20);
    assert(st_get(ST_CHANCE, 0, &tc_state, w) == ST_ABSENT && !sq_slots[0].entries[0].chance);
    /* Kept over a restart and a compaction, with the chain and presets' own sets. */
    odds = sq_slots[0]; odds.entries[0].chance = 3u; sq_chances_encode(&odds, w); assert(st_put(ST_CHANCE, 0, w, &tc_state, &e) == ST_OK);
    const uint8_t links[2] = {0, 0}; put_chain(links, 2);
    uint32_t names[ST_PAYLOAD]; memset(names, 0, sizeof names); names[0] = 1u; names[1] = 'A';
    while (st_records() < 400u) { names[1] ^= 3u; assert(st_put(ST_NAMES, 0, names, &tc_state, &e) == ST_OK); }
    boot(false);
    assert(st_records() < 40u && sq_slots[0].entries[0].chance == 3u && sq.entries[0].chance == 3u && sq_chain.count == 2u);
    puts("chances: the record and its checks; a chord at 10 % mostly a rest (nothing sounds), the next always; borrowed: "
         "the parallel minor's chord, named so; recording again plays every chord; kept over a restart and compaction");
}

/* --- 4. presets' own chord sets (Keep chords) --- */
static tc_chord_set named(const char *name, uint16_t mask) {
    tc_chord_set s; memset(&s, 0, sizeof s);
    for (uint32_t i = 0; i < 12u; ++i) s.column[i] = (tc_set_chord){i < 4u ? mask : 0u, (uint8_t)(i < 4u ? 2u * i : 0u)};
    snprintf(s.name, sizeof s.name, "%s", name);
    return s;
}
static void put_set(uint8_t slot, tc_chord_set s) { /* as the editor's Chords tab: the board's own set */
    uint32_t w[ST_PAYLOAD];
    memset(w, 0, sizeof w); w[0] = 1u;
    for (uint32_t i = 0; i < 12u; ++i) w[1u + i] = s.column[i].mask | (uint32_t)s.column[i].root << 12;
    for (uint32_t i = 0; i < TC_USER_NAME; ++i) w[13u + i / 4u] |= (uint32_t)(uint8_t)s.name[i] << (8u * (i % 4u));
    assert(st_put(ST_CHORDS, slot, w, &tc_state, &e) == ST_OK);
}
static bool set_is(uint8_t slot, const char *name) { return !strcmp(tc_chord_set_get(slot)->name, name); }
static void save_preset(uint8_t slot) { tc_state.request = TC_REQUEST_SAVE; tc_state.request_slot = slot; run(20); assert(tc_state.last_action == TC_PRESET_SAVED); }
static void load_preset(uint8_t slot) { tc_state.request = TC_REQUEST_LOAD; tc_state.request_slot = slot; run(20); assert(tc_state.last_action == TC_PRESET_LOADED); }
static void keep_chords(void) {
    boot(true);
    uint32_t w[ST_PAYLOAD];
    put_set(0, named("FUNK", 0x091u)); put_set(1, named("JAZZ", 0x891u));
    tp_layout l = tc_layout(&tc_state); l.chords = 1u; assert(tc_set_layout(&tc_state, &e, l));
    /* Keep chords on: preset 1 gets its own copy of set 1. Off: preset 2 none. */
    assert(tc_set_item(&tc_state, &e, TC_MENU_KEEP_CHORDS, 1u) && tc_state.keep_chords);
    save_preset(0);
    assert(st_get(ST_OWN_SET, 1, &tc_state, w) == ST_OK && w[0] == 1u && st_get(ST_OWN_SET, 2, &tc_state, w) == ST_ABSENT);
    tc_state.keep_chords = 0u; save_preset(1); assert(st_get(ST_OWN_SET, 2, &tc_state, w) == ST_ABSENT);
    /* The board's set 1 changes; preset 1 brings its own back, preset 2 the board's. */
    put_set(0, named("FUNK 2", 0x089u));
    load_preset(0); assert(set_is(0, "FUNK") && tc_state.chords_slot == 1u && tc_state.chords_cover == 0u);
    assert(st_get(ST_CHORDS, 0, &tc_state, w) == ST_OK && (w[13] & 0xFFu) == 'F' && (w[14] & 0xFFu) == ' '); /* the board's: FUNK 2 */
    tp_notes n; assert(tc_pad(e.layout, pad(0, 3), &n)); /* the pads play it */
    load_preset(1); assert(set_is(0, "FUNK 2") && !tc_state.chords_slot);
    /* Changes to the covered set go to the preset's own, saved before another covers them. */
    load_preset(0);
    tc_chord_set edited = named("FUNK 3", 0x091u); assert(tc_chord_set_put(0, &edited)); /* not saved yet */
    load_preset(1); assert(set_is(0, "FUNK 2"));
    load_preset(0); assert(set_is(0, "FUNK 3"));
    /* ... and through the editor's slot 9 (the set as it plays, whose), at once. */
    assert(st_get(ST_OWN_SET, ST_OWN_SET_NOW, &tc_state, w) == ST_OK && w[ST_PAYLOAD - 1u] == 1u && ((w[0] >> 8) & 0xFFu) == 0u);
    w[13] = 'S' | 'O' << 8 | 'U' << 16 | 'L' << 24; w[14] = 0u; w[ST_PAYLOAD - 1u] = 1u;
    assert(st_put(ST_OWN_SET, ST_OWN_SET_NOW, w, &tc_state, &e) == ST_OK && set_is(0, "SOUL"));
    assert(st_get(ST_OWN_SET, 1, &tc_state, w) == ST_OK && w[13] == ('S' | 'O' << 8 | 'U' << 16 | 'L' << 24));
    w[0] = 1u | 1u << 8; assert(st_put(ST_OWN_SET, ST_OWN_SET_NOW, w, &tc_state, &e) == ST_BAD_VALUE); /* not the covered set */
    /* The board's set 1 written while covered: stored, the pads keep the preset's. */
    put_set(0, named("FUNK 4", 0x089u)); assert(set_is(0, "SOUL"));
    assert(st_get(ST_CHORDS, 0, &tc_state, w) == ST_OK && (w[13] & 0xFFu) == 'F');
    load_preset(1); assert(set_is(0, "FUNK 4"));
    /* Keep chords off saving into preset 1 takes its own away; in use: the board's again. */
    load_preset(0); tc_state.keep_chords = 0u; save_preset(0);
    assert(st_get(ST_OWN_SET, 1, &tc_state, w) == ST_ABSENT && set_is(0, "FUNK 4") && !tc_state.chords_slot);
    /* Keep chords is the board's (slot 0), stored with the settings; slot 0 and 9 say it. */
    assert(tc_set_item(&tc_state, &e, TC_MENU_KEEP_CHORDS, 1u)); run(ST_SAVE_DELAY_MS + 100u);
    assert(st_get(ST_OWN_SET, 0, &tc_state, w) == ST_OK && w[0] == (1u | 1u << 16));
    save_preset(2); tc_state.request = TC_REQUEST_LOAD; tc_state.request_slot = 0u; run(20);
    assert(tc_state.keep_chords == 1u); /* a preset leaves it */
    /* Refused: a set slot past 8, Keep chords in a preset's own, a broken chord. */
    assert(st_get(ST_OWN_SET, 3, &tc_state, w) == ST_OK);
    uint32_t bad[ST_PAYLOAD]; memcpy(bad, w, sizeof bad); bad[0] = 1u | 8u << 8;
    assert(st_put(ST_OWN_SET, 3, bad, &tc_state, &e) == ST_BAD_VALUE);
    memcpy(bad, w, sizeof bad); bad[0] |= 1u << 16; assert(st_put(ST_OWN_SET, 3, bad, &tc_state, &e) == ST_BAD_VALUE);
    memcpy(bad, w, sizeof bad); bad[1] = 0x002u; assert(st_put(ST_OWN_SET, 3, bad, &tc_state, &e) == ST_BAD_VALUE);
    assert(st_put(ST_OWN_SET, TC_LOOKS + 1u, w, &tc_state, &e) == ST_BAD_VALUE);
    /* Set and cleared while in use (the editor's Presets tab). */
    load_preset(2); assert(tc_state.chords_slot == 3u);
    uint32_t ow[ST_PAYLOAD];
    memcpy(ow, w, sizeof ow); ow[13] = 'O' | 'T' << 8 | 'H' << 16 | 'E' << 24; ow[14] = 'R';
    assert(st_put(ST_OWN_SET, 3, ow, &tc_state, &e) == ST_OK && set_is(0, "OTHER"));
    memset(ow, 0, sizeof ow); assert(st_put(ST_OWN_SET, 3, ow, &tc_state, &e) == ST_OK && set_is(0, "FUNK 4") && !tc_state.chords_slot);
    /* Kept over a restart (the board's sets as they are) and a compaction. */
    load_preset(0); save_preset(5); /* Keep chords on: preset 6 has its own */
    boot(false); assert(set_is(0, "FUNK 4") && tc_state.keep_chords == 1u && !tc_state.chords_slot);
    assert(st_get(ST_OWN_SET, 6, &tc_state, w) == ST_OK);
    puts("keep chords: saved with a preset, brought back by loading it, the board's for one without; changes saved where "
         "they belong before another covers them; slot 9; the board's set stored while covered; Keep chords off takes it "
         "away; the board's setting; refused when wrong; set and cleared in use; kept over a restart");
}

/* --- 5. the U3 button as Shift, the buttons under the screen --- */
static void u3_shift(void) {
    boot(true);
    /* U3 as its scale (the default): U3 IS EMPTY until it has one. */
    assert(!tc_state.u3_shift);
    mode_button(TC_U3); assert(tc_state.last_action == TC_TODO_FUNCTION && !tc_shift(&tc_state));
    assert(tc_set_item(&tc_state, &e, TC_MENU_U3, 1u));
    assert(!strcmp(ui_value_text(TC_MENU_U3, 0), "U3 SCALE") && !strcmp(ui_value_text(TC_MENU_U3, 1), "SHIFT"));
    /* A tap: Shift for the next press only; Shift + mode button 3 loads preset 3. */
    tc_state.tempo = 20u; save_preset(2); tc_state.tempo = 16u;
    u3(true); u3(false); assert(tc_state.shift_once && tc_shift(&tc_state) && tc_state.last_action == TC_SHIFT_ONCE);
    screen(); assert(!strcmp(lines[36], "SHIFT: NEXT BUTTON") && !strcmp(lines[56], "<PLAY ^v120 >ARP OTAP"));
    mode_button(2u); run(20); assert(tc_state.tempo == 20u && !tc_shift(&tc_state));
    mode_button(2u); assert(tc_state.last_action == TC_MODE); /* Shift was for one press */
    /* Two taps close together latch it; a tap lets go. Held: Shift while held. */
    hold_bit(0x38, top_bit[TC_U3], true); raw[client(0x38)] = 0u; run(30);
    hold_bit(0x38, top_bit[TC_U3], true); raw[client(0x38)] = 0u; run(30);
    assert(tc_state.shift_latched && !tc_state.shift_once);
    root_button(0u); assert(tc_state.last_action == TC_SEQ_EMPTY && tc_state.shift_latched); /* Shift + root 1: P1 */
    u3(true); u3(false); assert(!tc_shift(&tc_state));
    run(500); u3(true); assert(tc_shift(&tc_state)); root_button(3u); assert(tc_state.prog_slot == 3u && tc_state.last_action == TC_SEQ_EMPTY);
    u3(false); assert(!tc_shift(&tc_state) || tc_state.shift_once); /* let go: still the next press's, if none came */
    tc_state.shift_once = false;
    /* A chord pad with Shift once: chosen, silent, and Shift ends. */
    u3(true); u3(false); touch(pad(2, 3), true); assert(!heard(0) && tc_state.last_chord == pad(2, 3) && !tc_state.shift_once);
    touch(pad(2, 3), false);
    /* The play screen: Shift + up/down the tempo, right the arpeggiator, left play/stop, centre taps the tempo. */
    tc_state.tempo = 16u; /* preset 3 brought 20 */
    u3(true); u3(false); nav(TC_NAV_UP); assert(tc_state.tempo == 17u && tc_state.last_action == TC_TEMPO_STEP && tc_state.ui == TC_UI_PLAY);
    u3(true); u3(false); nav(TC_NAV_DOWN); assert(tc_state.tempo == 16u);
    u3(true); u3(false); nav(TC_NAV_RIGHT); assert(tc_state.arp == TC_ARP_UP); u3(true); u3(false); nav(TC_NAV_RIGHT); assert(!tc_state.arp);
    u3(true); u3(false); nav(TC_NAV_LEFT); assert(tc_state.transport); u3(true); u3(false); nav(TC_NAV_LEFT); assert(!tc_state.transport);
    tc_state.arrows = TC_ARROWS_TEMPO; uint8_t scale = tc_state.scale;
    u3(true); u3(false); screen(); assert(!strcmp(lines[56], "<PLAY ^vSCL >ARP OTAP"));
    nav(TC_NAV_UP); assert(tc_state.scale != scale && tc_state.tempo == 16u); /* the arrows already do tempo */
    tc_state.arrows = TC_ARROWS_SCALE;
    /* Settings: home, first and last, 10 at a time, the default. */
    nav(TC_NAV_ENTER); assert(tc_state.ui == TC_UI_GROUPS);
    u3(true); u3(false); screen(); assert(!strcmp(lines[56], "<HOME ^vEND"));
    nav(TC_NAV_DOWN); assert(tc_state.group == TC_GROUPS - 1u);
    u3(true); u3(false); nav(TC_NAV_UP); assert(tc_state.group == 0u);
    nav(TC_NAV_ENTER); assert(tc_state.ui == TC_UI_MENU);
    u3(true); u3(false); nav(TC_NAV_DOWN); { uint8_t items[TC_GROUP_MAX], n = tc_page_items(&tc_state, 0, items); assert(tc_state.menu == items[n - 1u]); }
    while (tc_state.menu != TC_MENU_TEMPO) nav(TC_NAV_UP);
    nav(TC_NAV_ENTER); assert(tc_state.ui == TC_UI_EDIT);
    u3(true); u3(false); screen(); assert(!strcmp(lines[56], "<HOME ^v10 ODEFAULT"));
    nav(TC_NAV_UP); assert(tc_state.edit_value == 26u); nav(TC_NAV_ENTER); assert(tc_state.tempo == 26u && tc_state.ui == TC_UI_MENU);
    u3(true); u3(false); screen(); assert(!strcmp(lines[56], "<HOME ^vEND ODEFAULT"));
    nav(TC_NAV_ENTER); assert(tc_state.tempo == 16u); /* its default */
    while (tc_state.menu != TC_MENU_LOAD) { tc_state.group = TC_GROUP_PRESETS; tc_state.menu = TC_MENU_LOAD; }
    u3(true); u3(false); nav(TC_NAV_ENTER); assert(tc_state.last_action == TC_NO_DEFAULT);
    u3(true); u3(false); nav(TC_NAV_LEFT); assert(tc_state.ui == TC_UI_PLAY);
    /* Stored with the settings, the board's (presets leave it); Reset all on the Buttons page sets it back. */
    run(ST_SAVE_DELAY_MS + 100u); boot(false); assert(tc_state.u3_shift == 1u);
    tc_state.ui = TC_UI_MENU; tc_state.group = TC_GROUP_BUTTONS; tc_state.menu = TC_MENU_BUTTONS_RESET;
    nav(TC_NAV_ENTER); nav(TC_NAV_UP); nav(TC_NAV_ENTER); assert(!tc_state.u3_shift);
    tc_state.ui = TC_UI_PLAY;
    puts("u3: its scale by default; as Shift a tap is Shift for the next press (preset 3 loaded, a chord pad chosen), two "
         "taps latch, held while held; play screen tempo, arp, play/stop, tap; Settings home, ends, 10 at a time, "
         "defaults; the bottom lines; stored; Reset all");
}

/* --- 6. Sleep after and Shut down; the I/O test's buttons --- */
static bool lights_off(void) {
    for (uint32_t k = 0; k < TL_PADS; ++k) if (pads[k].r || pads[k].g || pads[k].b) return false;
    for (uint32_t ch = 0; ch < TL_BUTTONS; ++ch) if (buttons[ch]) return false;
    return true;
}
static void sleep_and_shut_down(void) {
    boot(true);
    assert(!tc_state.sleep && tc_menu_values(TC_MENU_SLEEP) == TC_SLEEPS);
    assert(!strcmp(ui_value_text(TC_MENU_SLEEP, 0), "OFF") && !strcmp(ui_value_text(TC_MENU_SLEEP, 1), "5 MIN"));
    screen(); assert(!lights_off());
    /* Sleep after 5 minutes: dark after that long without a touch; the board plays on. */
    assert(tc_set_item(&tc_state, &e, TC_MENU_SLEEP, 1u)); tc_state.touched_at = now;
    run(60000u * tc_sleep_minutes[1] - 1000u); assert(tc_state.dark == TC_AWAKE);
    tc_state.transport = false; run(2000u); assert(tc_state.dark == TC_DARK);
    uint32_t was = cleared; screen(); screen(); assert(lights_off() && cleared == was + 1u);
    /* A pad wakes it and plays; a button wakes it and does nothing else. */
    touch(pad(0, 3), true); assert(tc_state.dark == TC_AWAKE && heard(0) == 3u); touch(pad(0, 3), false);
    screen(); assert(!lights_off());
    tc_state.dark = TC_DARK; uint8_t scale = tc_state.scale;
    nav(TC_NAV_UP); assert(tc_state.dark == TC_AWAKE && tc_state.scale == scale && tc_state.last_action == TC_AWOKE);
    /* Shut down (About): every note off, the transport stopped, dark; pads and buttons do nothing but centre. */
    touch(pad(1, 3), true); assert(heard(0)); tc_state.transport = true;
    tc_state.ui = TC_UI_MENU; tc_state.group = TC_GROUP_ABOUT; tc_state.menu = TC_MENU_SHUT_DOWN;
    nav(TC_NAV_ENTER); run(100);
    assert(tc_state.dark == TC_SHUT_DOWN && !tc_state.transport && !heard(0) && tc_state.ui == TC_UI_PLAY);
    touch(pad(1, 3), false); touch(pad(2, 3), true); run(200); assert(!heard(0) && !heard(1)); touch(pad(2, 3), false);
    touch(48u + pad(3, 1), true); run(200); for (uint32_t ch = 0; ch < 16u; ++ch) assert(!heard(ch)); touch(48u + pad(3, 1), false);
    nav(TC_NAV_UP); root_button(4u); mode_button(5u); assert(tc_state.dark == TC_SHUT_DOWN && tc_state.scale == scale && tc_state.tonic == 0u);
    run(60000u * tc_sleep_minutes[1] + 1000u); assert(tc_state.dark == TC_SHUT_DOWN); /* Sleep after doesn't change it */
    screen(); assert(lights_off());
    nav(TC_NAV_ENTER); assert(tc_state.dark == TC_AWAKE && tc_state.ui == TC_UI_PLAY); /* centre only wakes */
    screen(); assert(!lights_off());
    touch(pad(2, 3), true); assert(heard(0) == 3u); touch(pad(2, 3), false); quiet();
    /* Sleep after is the board's, stored with the settings. */
    tc_state.sleep = 3u; save_preset(4); tc_state.sleep = 2u; load_preset(4); assert(tc_state.sleep == 2u);
    run(ST_SAVE_DELAY_MS + 100u); boot(false); assert(tc_state.sleep == 2u);
    /* The I/O test lists the buttons held: U3 and M1 here, a root button, centre button 2. */
    tc_state.ui = TC_UI_EDIT; tc_state.menu = TC_MENU_IO_TEST;
    raw[client(0x38)] = 1u << top_bit[TC_U3] | 1u << top_bit[2]; raw[client(0x39)] = 1u << top_bit[1];
    raw[client(0x35)] = 1u << fn_bit[1]; run(60); screen();
    assert(strstr(lines[37], "BTN") && strstr(lines[37], " U3") && strstr(lines[37], " M1") && strstr(lines[37], " C#") &&
           strstr(lines[37], " F2"));
    assert(!strncmp(lines[28], "TRS: C4/S, SENT ", 16u));
    raw[client(0x38)] = raw[client(0x39)] = raw[client(0x35)] = 0u; run(60);
    tc_state.ui = TC_UI_PLAY;
    puts("sleep: dark after Sleep after without a touch (the screen cleared once, every light off), a pad wakes it and "
         "plays, a button only wakes it; shut down: notes off, stopped, dark, pads and buttons ignored, centre wakes it; "
         "the board's setting; the I/O test lists the buttons held");
}

int main(void) {
    turing();
    lanes_shift();
    chains();
    chances();
    keep_chords();
    u3_shift();
    sleep_and_shut_down();
    puts("PASS: TBOSS 2.0: the Turing pattern, the step screen's Shift + buttons, chained progressions and each chord's "
         "chance, presets' own chord sets, U3 as Shift, Sleep after and Shut down");
    return 0;
}
