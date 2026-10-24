/* SPDX-License-Identifier: GPL-3.0-or-later - Settings > Pads (0.10.0): Trigger, Release and Full
 * (the pad thresholds, in hundreds), each pad's own resting level (1.5.0: measured when a session
 * starts, then followed while the pad rests), and the Pad test page's ranking (scan.c
 * ts_strongest). On the real controls/engine/settings code with a fake sector 11. */
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
    assert(tc_group_size(TC_GROUP_PADS) == 5u && tc_group_items[TC_GROUP_PADS][0] == TC_MENU_PAD_TRIGGER &&
           tc_group_items[TC_GROUP_PADS][1] == TC_MENU_PAD_RELEASE && tc_group_items[TC_GROUP_PADS][2] == TC_MENU_PAD_FULL &&
           tc_group_items[TC_GROUP_PADS][3] == TC_MENU_PAD_TEST && tc_group_items[TC_GROUP_PADS][4] == TC_MENU_RESET_PAGE);
    assert(tc_group_size(TC_GROUP_ABOUT) == 3u && tc_group_items[TC_GROUP_ABOUT][1] == TC_MENU_IO_TEST &&
           tc_group_items[TC_GROUP_ABOUT][2] == TC_MENU_FACTORY);
    assert(tc_menu_values(TC_MENU_PAD_TRIGGER) == 26u && tc_menu_values(TC_MENU_PAD_RELEASE) == 23u &&
           tc_menu_values(TC_MENU_PAD_FULL) == 26u && tc_menu_values(TC_MENU_PAD_TEST) == 1u);
    tc_init(&tc_state);
    assert(tc_menu_value(&tc_state, TC_MENU_PAD_TRIGGER) == 7u); /* 1200 = 500 + 7 x 100 */
    assert(tc_menu_value(&tc_state, TC_MENU_PAD_RELEASE) == 3u && tc_menu_value(&tc_state, TC_MENU_PAD_FULL) == 20u);
}
/* The rules at every setting and resting level: Release at least rest + 130, Trigger 200 above
 * it, Full 500 above that; nothing below its setting; always a valid engine configuration. */
static bool same(tk_config a, tk_config b) { return a.press == b.press && a.release == b.release && a.full_scale == b.full_scale; }
static void rules(void) {
    tc_init(&tc_state);
    tk_config set = tc_pad_config(&tc_state), d = set;
    assert(d.press == 1200u && d.release == 600u && d.full_scale == 3500u);
    d = tc_pad_raise(set, 470u);                                              /* a stock board's pad */
    assert(same(d, set));
    d = tc_pad_raise(set, 1200u);                                             /* a modded board's pad */
    assert(d.release == 1330u && d.press == 1530u && d.full_scale == 3500u);
    d = tc_pad_raise(set, 4000u);                                             /* capped */
    assert(d.release == 2130u && d.press == 2330u && tk_config_ok(d));
    tk_config inverted = {2u, 1500u, 2500u, 500u, 4u, 50u};
    assert(same(tc_pad_raise(inverted, 3000u), inverted));                     /* only for higher = harder */
    uint32_t combos = 0;
    for (uint32_t t = TC_PAD_TRIGGER_MIN; t <= TC_PAD_TRIGGER_MAX; ++t)
        for (uint32_t r = TC_PAD_RELEASE_MIN; r <= TC_PAD_RELEASE_MAX; ++r)
            for (uint32_t f = TC_PAD_FULL_MIN; f <= TC_PAD_FULL_MAX; ++f) {
                if (!tc_pad_settings_ok(t, r, f)) { assert(t < r + 2u || f < t + 5u); continue; }
                tc_state.pad_trigger = (uint8_t)t; tc_state.pad_release = (uint8_t)r; tc_state.pad_full = (uint8_t)f;
                for (uint32_t rest = 0; rest <= 2500u; rest += 50u) {
                    d = tc_pad_raise(tc_pad_config(&tc_state), rest);
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
    uint32_t lo, hi;
    run(1000); assert(e.state == TK_ACTIVE && tp_rest_range(&e, &lo, &hi) && lo >= 290u && hi <= 470u);
    assert(e.config.press == 1200u && e.config.release == 600u && e.config.full_scale == 3500u);
    for (uint32_t k = 0; k < TS_KEYS; ++k) assert(same(tp_key_config(&e, k), e.config)); /* every pad as set */
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
/* A modded board resting at 1100..1250: each pad's Release is raised above its own rest, so the
 * pads still release; 10 pads held while it starts are silent, start from the board's middle level
 * and learn their own once let go. */
static void modded_board(void) {
    tc_init(&tc_state); memset(&e, 0, sizeof e); memset(&scan, 0, sizeof scan); ons = offs = 0u;
    for (uint32_t k = 0; k < TS_KEYS; ++k) scan.values[k] = 1100u + (k * 11u) % 151u;
    for (uint32_t k = 0; k < 10u; ++k) scan.values[60u + k] = 3000u; /* held while starting */
    run(1000); assert(e.state == TK_ACTIVE);
    for (uint32_t k = 0; k < TS_KEYS; ++k) {
        tk_config d = tp_key_config(&e, k);
        assert(d.release >= e.rest[k] + TC_PAD_REST_MARGIN && d.press >= d.release + 200u && e.rest[k] >= 1100u && e.rest[k] <= 1250u);
    }
    for (uint32_t k = 0; k < 10u; ++k) assert(e.keys[60u + k].inhibited);
    for (uint32_t k = 0; k < 10u; ++k) scan.values[60u + k] = 1150u;
    run(200); assert(!ons);                                           /* the held pads were silent, now released */
    for (uint32_t k = 0; k < 10u; ++k) assert(!e.keys[60u + k].inhibited && e.rest[60u + k] <= 1200u);
    scan.values[51] = 2500u; run(100); assert(ons == 1u);
    scan.values[51] = 1200u; run(100); assert(offs == 1u);            /* back to rest: off */
}
/* A stock board with 11 pads resting high (900: a dent, a sensitivity modification, a flexing
 * case). Before 1.5.0 one level for the board (the 12th highest pad, 300 here) left them above
 * Release (600): played once, they never let go. Now each is raised for its own rest, and the other
 * pads keep the light touch of the settings. */
static void uneven_board(void) {
    tc_init(&tc_state); memset(&e, 0, sizeof e); memset(&scan, 0, sizeof scan); ons = offs = 0u;
    for (uint32_t k = 0; k < TS_KEYS; ++k) scan.values[k] = 300u;
    for (uint32_t k = 0; k < 11u; ++k) scan.values[49u + 4u * k] = 900u;  /* the right half's row 2 */
    run(1000); assert(e.state == TK_ACTIVE && !ons);
    tk_config high = tp_key_config(&e, 49u), low = tp_key_config(&e, 51u);
    assert(high.release == 1030u && high.press == 1230u && same(low, e.config));
    scan.values[49] = 2500u; run(100); assert(ons == 1u);
    scan.values[49] = 900u; run(100); assert(offs == 1u);            /* back to its rest: let go */
    scan.values[49] = 2500u; run(100); assert(ons == 2u);            /* and plays again */
    scan.values[49] = 900u; run(100); assert(offs == 2u);
    scan.values[51] = 1250u; run(100); assert(ons == 3u);            /* the others: a light touch as set */
    scan.values[51] = 300u; run(100); assert(offs == 3u);
}
/* Chords on trades the halves during a session: each sensor keeps its own resting level. Keyed by
 * logical pad, the melody pads now on the high-resting left half would take the right half's light
 * thresholds (Trigger 1200 < their 1300 rest) and play by themselves. */
static void traded_halves(void) {
    tc_init(&tc_state); memset(&e, 0, sizeof e); memset(&scan, 0, sizeof scan); ons = offs = 0u;
    for (uint32_t k = 0; k < TS_KEYS; ++k) scan.values[k] = k < 48u ? 1300u : 300u;
    run(1000); assert(e.state == TK_ACTIVE && !ons);
    tc_state.chords_right = 1u; run(500); assert(!ons);               /* nothing plays by itself */
    assert(tp_key_config(&e, 49u).release == 1430u && same(tp_key_config(&e, 1u), e.config));
    scan.values[1] = 3000u; run(100); assert(ons == 1u);              /* physical 1 = melody pad 49 */
    scan.values[1] = 1300u; run(100); assert(offs == 1u);             /* lets go at its own rest */
    scan.values[49] = 1250u; run(100); assert(ons > 1u);              /* chord pad 1, right half: as set */
    scan.values[49] = 300u; run(100); assert(offs == ons);
    uint32_t n = ons;
    tc_state.chords_right = 0u; run(500); assert(ons == n && offs == n);
}
/* A board that flexes (moved from a desk to a lap): its pads' rest moves while untouched. The levels
 * follow (down at once, up within seconds), so a pad still lets go where it now rests; a press,
 * even a long one, is never taken for rest; a finger resting below Trigger for seconds is. */
static void flexing_board(void) {
    tc_init(&tc_state); memset(&e, 0, sizeof e); memset(&scan, 0, sizeof scan); ons = offs = 0u;
    for (uint32_t k = 0; k < TS_KEYS; ++k) scan.values[k] = 400u;
    run(1000); assert(e.state == TK_ACTIVE && e.rest[51] == 400u);
    for (uint32_t k = 0; k < TS_KEYS; ++k) scan.values[k] = 1000u;  /* lifted onto a lap: all rest at 1000 */
    run(100); assert(!ons && e.rest[51] < 1000u);                    /* below Trigger: nothing plays */
    run(5000); assert(!ons && e.rest[51] >= 950u && tp_key_config(&e, 51u).release >= 1080u);
    scan.values[51] = 2500u; run(100); assert(ons == 1u);
    scan.values[51] = 1000u; run(100); assert(offs == 1u);           /* lets go at its new rest */
    uint32_t before = e.rest[51];
    scan.values[51] = 3000u; run(10000); assert(e.rest[51] == before); /* a 10 s press: not rest */
    scan.values[51] = 1000u; run(100); assert(offs == 2u);
    for (uint32_t k = 0; k < TS_KEYS; ++k) scan.values[k] = 300u;   /* back on the desk */
    run(200); assert(e.rest[51] <= 330u && same(tp_key_config(&e, 51u), e.config));
    scan.values[51] = 1250u; run(100); assert(ons == 3u);           /* the light touch is back */
    scan.values[51] = 300u; run(100); assert(offs == 3u);
    scan.values[52] = 1100u; run(5000); assert(e.rest[52] >= 1000u && ons == 3u); /* a resting finger becomes rest */
    scan.values[52] = 300u; run(200); assert(e.rest[52] <= 330u);
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
    ranking(); pages(); rules(); stock_board(); modded_board(); uneven_board(); traded_halves(); flexing_board(); saved();
    puts("PASS: pads: Trigger/Release/Full pages and values, the rules at every setting and resting level, "
         "each pad's own resting level (an uneven board's high pads let go, kept by sensor when the halves "
         "trade, a flexing board followed, a press "
         "never taken for rest) "
         "(Release >= rest + 130, Trigger +200, Full +500, capped), a stock board resting 290..470 never "
         "plays by itself and plays from 1200 with velocity 1..127 to 3500, a modded board resting 1100..1250 "
         "with pads held at the start still releases, settings word 32 (0 at the defaults, invalid refused, "
         "presets keep the board's), Pad test ranking");
    return 0;
}
