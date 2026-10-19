/* SPDX-License-Identifier: GPL-3.0-or-later - Settings > Pads (0.10.0): Trigger, Release and Full
 * (the pad thresholds, in hundreds), the resting level measured when a session starts, and
 * the Pad test page's ranking (scan.c ts_strongest). On the real controls/engine/settings code
 * with a fake sector 11. */
#include "../../src/tb/settings.h"
#include "../../src/tb/flash_store.h"
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
static tp_engine e;
static ts_scan scan;
static uint32_t now = 1000u, ons, offs;
static uint8_t velocity[128];
static bool writer(const uint8_t p[4], uint32_t token) {
    (void)token;
    if (p[0] == TK_ON) { ons++; velocity[p[2]] = p[3]; }
    if (p[0] == TK_OFF) offs++;
    return true;
}
static void run(uint32_t ms) {
    for (uint32_t i = 0; i < ms; ++i, ++now) {
        if (i % 15u == 0u) {
            scan.frame++; scan.last_ms = now; scan.valid = true;
            if (e.state == TK_IDLE) (void)tc_arm(&e, &scan, now, true); else (void)tc_frame(&e, &scan, now);
        }
        tc_step(&e, now, true, writer);
        if (e.pending_id) tp_complete(&e, e.pending_id, 4u, true, now);
    }
}
static void ranking(void) {
    uint32_t v[TS_KEYS] = {0};
    uint8_t top[3];
    ts_strongest(v, top, 3u); assert(top[0] == 0u && top[1] == 1u && top[2] == 2u); /* all equal: lowest pads */
    v[50] = 2734u; v[54] = 812u; v[46] = 640u; v[90] = 300u;               /* a press, two neighbours */
    ts_strongest(v, top, 3u); assert(top[0] == 50u && top[1] == 54u && top[2] == 46u);
    v[95] = 4095u; v[10] = 812u;                                            /* ties: the lower pad first */
    ts_strongest(v, top, 3u); assert(top[0] == 95u && top[1] == 50u && top[2] == 10u);
    uint8_t all[TS_KEYS]; bool seen[TS_KEYS] = {false};
    for (uint32_t k = 0; k < TS_KEYS; ++k) v[k] = (k * 37u) % 4096u;
    ts_strongest(v, all, TS_KEYS);
    for (uint32_t i = 0; i < TS_KEYS; ++i) {
        assert(!seen[all[i]]); seen[all[i]] = true;
        if (i) assert(v[all[i - 1u]] >= v[all[i]]);
    }
}
static void pages(void) {
    assert(tc_group_size(TC_GROUP_PADS) == 4u && tc_group_items[TC_GROUP_PADS][0] == TC_MENU_PAD_TRIGGER &&
           tc_group_items[TC_GROUP_PADS][1] == TC_MENU_PAD_RELEASE && tc_group_items[TC_GROUP_PADS][2] == TC_MENU_PAD_FULL &&
           tc_group_items[TC_GROUP_PADS][3] == TC_MENU_PAD_TEST);
    assert(tc_group_size(TC_GROUP_ABOUT) == 2u && tc_group_items[TC_GROUP_ABOUT][1] == TC_MENU_IO_TEST);
    assert(tc_menu_values(TC_MENU_PAD_TRIGGER) == 26u && tc_menu_values(TC_MENU_PAD_RELEASE) == 23u &&
           tc_menu_values(TC_MENU_PAD_FULL) == 26u && tc_menu_values(TC_MENU_PAD_TEST) == 1u);
    tc_init(&tc_state);
    assert(tc_menu_value(&tc_state, TC_MENU_PAD_TRIGGER) == 7u); /* 1200 = 500 + 7 x 100 */
    assert(tc_menu_value(&tc_state, TC_MENU_PAD_RELEASE) == 3u && tc_menu_value(&tc_state, TC_MENU_PAD_FULL) == 20u);
}
/* The rules at every setting and resting level: Release at least rest + 130, Trigger 200 above
 * it, Full 500 above that; nothing below its setting; always a valid engine configuration. */
static void rules(void) {
    tc_init(&tc_state);
    tk_config d = tc_pad_config(&tc_state);
    assert(d.press == 1200u && d.release == 600u && d.full_scale == 3500u && !tc_pad_limited(&tc_state));
    tc_state.pad_rest = 470u; d = tc_pad_config(&tc_state);                  /* a stock board's highest rest */
    assert(d.press == 1200u && d.release == 600u && !tc_pad_limited(&tc_state));
    tc_state.pad_rest = 1200u; d = tc_pad_config(&tc_state);                 /* a modded board */
    assert(d.release == 1330u && d.press == 1530u && d.full_scale == 3500u && tc_pad_limited(&tc_state));
    tc_state.pad_rest = 4000u; d = tc_pad_config(&tc_state);                 /* pads held at the start: capped */
    assert(d.release == 2130u && d.press == 2330u && tk_config_ok(d));
    uint32_t combos = 0;
    for (uint32_t t = TC_PAD_TRIGGER_MIN; t <= TC_PAD_TRIGGER_MAX; ++t)
        for (uint32_t r = TC_PAD_RELEASE_MIN; r <= TC_PAD_RELEASE_MAX; ++r)
            for (uint32_t f = TC_PAD_FULL_MIN; f <= TC_PAD_FULL_MAX; ++f) {
                if (!tc_pad_settings_ok(t, r, f)) { assert(t < r + 2u || f < t + 5u); continue; }
                tc_state.pad_trigger = (uint8_t)t; tc_state.pad_release = (uint8_t)r; tc_state.pad_full = (uint8_t)f;
                for (uint32_t rest = 0; rest <= 2500u; rest += 50u) {
                    tc_state.pad_rest = (uint16_t)rest;
                    d = tc_pad_config(&tc_state);
                    uint32_t capped = rest < TC_PAD_REST_CAP ? rest : TC_PAD_REST_CAP;
                    assert(tk_config_ok(d) && d.press >= 100u * t && d.release >= 100u * r && d.full_scale >= 100u * f);
                    assert(d.release >= capped + TC_PAD_REST_MARGIN && d.press >= d.release + 200u);
                    assert(d.full_scale >= d.press + 500u || d.full_scale == 4095u);
                    if (rest == 0u) assert(d.press == 100u * t && d.release == 100u * r && d.full_scale == 100u * f);
                }
                combos++;
            }
    assert(combos > 1000u);
}
/* The engine at the defaults on a stock board: resting pads wander 290..470, a light press
 * plays, a touch below Trigger doesn't, a note ends below Release, velocity 1..127 from
 * Trigger to Full. */
static void stock_board(void) {
    tc_init(&tc_state); memset(&e, 0, sizeof e); memset(&scan, 0, sizeof scan); ons = offs = 0u;
    for (uint32_t k = 0; k < TS_KEYS; ++k) scan.values[k] = 290u + (k * 7u) % 181u;
    run(1000); assert(e.state == TK_ACTIVE && tc_state.pad_rest >= 400u && tc_state.pad_rest <= 470u);
    assert(e.config.press == 1200u && e.config.release == 600u && e.config.full_scale == 3500u);
    for (uint32_t i = 0; i < 200u; ++i) { /* resting pads wander +-80: never a note */
        for (uint32_t k = 0; k < TS_KEYS; ++k) scan.values[k] = 290u + ((k * 7u + i * 13u) % 181u);
        run(15);
    }
    assert(!ons);
    tp_notes n; assert(tc_pad(e.layout, 51u, &n) && n.count == 1u);
    scan.values[51] = 1100u; run(100); assert(!ons);                  /* below Trigger: silent */
    scan.values[51] = 1250u; run(100); assert(ons == 1u && velocity[n.notes[0]] <= 4u); /* just past: very soft */
    scan.values[51] = 700u; run(100); assert(!offs);                  /* relaxed, above Release: still on */
    scan.values[51] = 590u; run(100); assert(offs == 1u);             /* below Release: off */
    scan.values[51] = 3500u; run(100); assert(velocity[n.notes[0]] == 127u); /* Full: loudest */
    scan.values[51] = 300u; run(100); assert(ons == offs);
}
/* A modded board resting at 1100..1250: the session raises Release above the rest, so the
 * pads still release; 10 pads held while it starts don't change that. */
static void modded_board(void) {
    tc_init(&tc_state); memset(&e, 0, sizeof e); memset(&scan, 0, sizeof scan); ons = offs = 0u;
    for (uint32_t k = 0; k < TS_KEYS; ++k) scan.values[k] = 1100u + (k * 11u) % 151u;
    for (uint32_t k = 0; k < 10u; ++k) scan.values[60u + k] = 3000u; /* held while starting */
    run(1000); assert(e.state == TK_ACTIVE);
    assert(tc_state.pad_rest >= 1200u && tc_state.pad_rest <= 1250u && tc_pad_limited(&tc_state));
    assert(e.config.release >= 1330u && e.config.press == e.config.release + 200u);
    for (uint32_t k = 0; k < 10u; ++k) scan.values[60u + k] = 1150u;
    run(200); assert(!ons);                                           /* the held pads were silent, now released */
    scan.values[51] = 2500u; run(100); assert(ons == 1u);
    scan.values[51] = 1200u; run(100); assert(offs == 1u);            /* back to rest: off */
}
/* Settings word 32: 0 while at the defaults (older firmware keeps reading the record), the
 * values otherwise; invalid values are refused; presets leave the board's thresholds alone. */
static void saved(void) {
    static tp_engine idle;
    fs_erase(); tc_init(&tc_state); (void)st_load(&tc_state);
    uint32_t p[ST_PAYLOAD];
    assert(st_get(ST_LIVE, 0, &tc_state, p) == ST_OK && p[32] == 0u);
    tc_state.pad_trigger = 8u; tc_state.pad_release = 4u; tc_state.pad_full = 30u;
    assert(st_get(ST_LIVE, 0, &tc_state, p) == ST_OK && p[32] == (8u | 4u << 8 | 30u << 16));
    tc_controls back; tc_init(&back);
    assert(st_put(ST_LIVE, 0, p, &back, &idle) == ST_OK && back.pad_trigger == 8u && back.pad_release == 4u &&
           back.pad_full == 30u);
    uint32_t q[ST_PAYLOAD];
    static const uint32_t bad[] = {8u | 7u << 8 | 30u << 16, 4u | 3u << 8 | 30u << 16, 8u | 4u << 8 | 12u << 16,
                                   31u | 4u << 8 | 40u << 16, 8u | 4u << 8 | 30u << 16 | 1u << 24};
    for (uint32_t i = 0; i < sizeof bad / sizeof bad[0]; ++i) {
        memcpy(q, p, sizeof q); q[32] = bad[i];
        tc_controls before = back;
        assert(st_put(ST_LIVE, 0, q, &back, &idle) == ST_BAD_VALUE && !memcmp(&before, &back, sizeof back));
    }
    q[32] = 0u; assert(st_put(ST_LIVE, 0, q, &back, &idle) == ST_OK && back.pad_trigger == TC_PAD_TRIGGER_DEFAULT);
    /* A preset saved with other thresholds loads without touching the board's. */
    assert(st_put(ST_PRESET, 3, p, &tc_state, &idle) == ST_OK);
    tc_state.pad_trigger = 15u; tc_state.pad_release = 6u; tc_state.pad_full = 35u;
    tc_state.request = TC_REQUEST_LOAD; tc_state.request_slot = 3u; st_requests(&tc_state, &idle);
    assert(tc_state.preset == 3u && tc_state.pad_trigger == 15u && tc_state.pad_release == 6u && tc_state.pad_full == 35u);
}
int main(void) {
    ranking(); pages(); rules(); stock_board(); modded_board(); saved();
    puts("PASS: pads: Trigger/Release/Full pages and values, the rules at every setting and resting level "
         "(Release >= rest + 130, Trigger +200, Full +500, capped), a stock board resting 290..470 never "
         "plays by itself and plays from 1200 with velocity 1..127 to 3500, a modded board resting 1100..1250 "
         "with pads held at the start still releases, settings word 32 (0 at the defaults, invalid refused, "
         "presets keep the board's), Pad test ranking");
    return 0;
}
