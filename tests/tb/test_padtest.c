/* SPDX-License-Identifier: GPL-3.0-or-later - Settings > Pads (0.10.0): Trigger, Release and Full
 * (the pad thresholds, in hundreds; 1.7.0: above each pad's own resting level), each pad's resting
 * level (1.5.0: measured when a session starts, then followed while the pad rests), bleed from a
 * pressed neighbor and Unstick (1.7.0), and the Pad test page's ranking (scan.c ts_strongest). On the
 * real controls/engine/settings code with a fake sector 11. */
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
static void fresh(void) { tc_init(&tc_state); memset(&e, 0, sizeof e); memset(&scan, 0, sizeof scan); ons = offs = 0u; }
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
    static const uint8_t pads[] = {TC_MENU_PAD_TRIGGER, TC_MENU_PAD_RELEASE, TC_MENU_PAD_FULL, TC_MENU_PAD_UNSTICK,
                                   TC_MENU_PAD_TEST, TC_MENU_PAD_RESCAN, TC_MENU_RESET_PAGE}; /* 1.7.0: Unstick, Rescan pads */
    assert(tc_group_size(TC_GROUP_PADS) == sizeof pads);
    for (uint32_t i = 0; i < sizeof pads; ++i) assert(tc_group_items[TC_GROUP_PADS][i] == pads[i]);
    assert(tc_group_size(TC_GROUP_ABOUT) == 3u && tc_group_items[TC_GROUP_ABOUT][1] == TC_MENU_IO_TEST &&
           tc_group_items[TC_GROUP_ABOUT][2] == TC_MENU_FACTORY);
    assert(tc_menu_values(TC_MENU_PAD_TRIGGER) == 16u && tc_menu_values(TC_MENU_PAD_RELEASE) == 15u &&
           tc_menu_values(TC_MENU_PAD_FULL) == 21u && tc_menu_values(TC_MENU_PAD_UNSTICK) == 2u &&
           tc_menu_values(TC_MENU_PAD_TEST) == 1u);
    tc_init(&tc_state);
    assert(tc_menu_value(&tc_state, TC_MENU_PAD_TRIGGER) == 2u); /* +700 = 500 + 2 x 100 */
    assert(tc_menu_value(&tc_state, TC_MENU_PAD_RELEASE) == 2u && tc_menu_value(&tc_state, TC_MENU_PAD_FULL) == 15u);
    assert(tc_menu_value(&tc_state, TC_MENU_PAD_UNSTICK) == 1u && tc_state.unstick == 1u);
}
/* The rules at every setting and resting level: each threshold the setting above the pad's rest (rest
 * capped at 2000), Full at most 4095, always a valid engine configuration. */
static bool same(tk_config a, tk_config b) { return a.press == b.press && a.release == b.release && a.full_scale == b.full_scale; }
static void rules(void) {
    tc_init(&tc_state);
    tk_config set = tc_pad_config(&tc_state), d = set;
    assert(d.press == 700u && d.release == 300u && d.full_scale == 2500u);
    d = tc_pad_raise(set, 0u); assert(same(d, set));
    d = tc_pad_raise(set, 1004u);                                             /* a warped board's pad */
    assert(d.release == 1304u && d.press == 1704u && d.full_scale == 3504u);
    d = tc_pad_raise(set, 4000u);                                             /* capped */
    assert(d.release == 2300u && d.press == 2700u && d.full_scale == 4095u && tk_config_ok(d));
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
                    uint32_t capped = rest < TC_PAD_REST_CAP ? rest : TC_PAD_REST_CAP, full = capped + 100u * f;
                    assert(tk_config_ok(d) && d.press == capped + 100u * t && d.release == capped + 100u * r);
                    assert(d.full_scale == (full > 4095u ? 4095u : full) && d.press < d.full_scale);
                }
                combos++;
            }
    assert(combos > 1000u);
}
/* The engine at the defaults on a stock board: resting pads wander 290..470, a light press plays, a
 * touch below Trigger doesn't, a note ends below Release, velocity 1..127 from Trigger to Full, each
 * measured from that pad's rest. */
static void stock_board(void) {
    fresh();
    for (uint32_t k = 0; k < TS_KEYS; ++k) scan.values[k] = 290u + (k * 7u) % 181u;
    uint32_t lo, hi;
    run(1000); assert(e.state == TK_ACTIVE && tp_rest_range(&e, &lo, &hi) && lo >= 290u && hi <= 470u);
    assert(e.config.press == 700u && e.config.release == 300u && e.config.full_scale == 2500u);
    for (uint32_t k = 0; k < TS_KEYS; ++k) assert(same(tp_key_config(&e, k), tc_pad_raise(e.config, e.rest[k])));
    for (uint32_t i = 0; i < 200u; ++i) { /* resting pads wander +-80: never a note */
        for (uint32_t k = 0; k < TS_KEYS; ++k) scan.values[k] = 290u + ((k * 7u + i * 13u) % 181u);
        run(15);
    }
    assert(!ons);
    tp_notes n; assert(tc_pad(e.layout, 51u, &n) && n.count == 1u);
    tk_config d = tp_key_config(&e, 51u);
    scan.values[51] = d.press - 100u; run(100); assert(!ons);         /* below Trigger: silent */
    d = tp_key_config(&e, 51u);                                        /* (its rest followed it up a little) */
    scan.values[51] = d.press + 20u; run(100); assert(ons == 1u && velocity[n.notes[0]] <= 4u); /* just past: very soft */
    scan.values[51] = d.release + 100u; run(100); assert(!offs);      /* relaxed, above Release: still on */
    scan.values[51] = d.release - 10u; run(100); assert(offs == 1u);  /* below Release: off */
    scan.values[51] = tp_key_config(&e, 51u).full_scale; run(100); assert(velocity[n.notes[0]] == 127u); /* Full: loudest */
    scan.values[51] = 300u; run(100); assert(ons == offs);
}
/* A modded board resting at 1100..1250: every pad's thresholds start from its own rest, so the pads
 * play and release as on a stock board; 10 pads held while it starts are silent, start from the
 * board's middle level and learn their own once let go. */
static void modded_board(void) {
    fresh();
    for (uint32_t k = 0; k < TS_KEYS; ++k) scan.values[k] = 1100u + (k * 11u) % 151u;
    for (uint32_t k = 0; k < 10u; ++k) scan.values[60u + k] = 3000u; /* held while starting */
    run(1000); assert(e.state == TK_ACTIVE);
    for (uint32_t k = 0; k < TS_KEYS; ++k) {
        tk_config d = tp_key_config(&e, k);
        assert(d.release == e.rest[k] + 300u && d.press == e.rest[k] + 700u && e.rest[k] >= 1100u && e.rest[k] <= 1250u);
    }
    for (uint32_t k = 0; k < 10u; ++k) assert(e.keys[60u + k].inhibited);
    for (uint32_t k = 0; k < 10u; ++k) scan.values[60u + k] = 1150u;
    run(200); assert(!ons);                                           /* the held pads were silent, now released */
    for (uint32_t k = 0; k < 10u; ++k) assert(!e.keys[60u + k].inhibited && e.rest[60u + k] <= 1250u);
    scan.values[51] = 2500u; run(100); assert(ons == 1u);
    scan.values[51] = 1200u; run(100); assert(offs == 1u);            /* back to rest: off */
}
/* A stock board with 11 pads resting high (900: a dent, a sensitivity modification, a flexing case).
 * Before 1.5.0 one level for the board left them above Release: played once, they never let go;
 * 1.5.0 raised only their Release. Now every pad takes the same press above its own rest. */
static void uneven_board(void) {
    fresh();
    for (uint32_t k = 0; k < TS_KEYS; ++k) scan.values[k] = 300u;
    for (uint32_t k = 0; k < 11u; ++k) scan.values[49u + 4u * k] = 900u;  /* the right half's row 2 */
    run(1000); assert(e.state == TK_ACTIVE && !ons);
    tk_config high = tp_key_config(&e, 49u), low = tp_key_config(&e, 51u);
    assert(high.release == 1200u && high.press == 1600u && low.release == 600u && low.press == 1000u);
    scan.values[49] = 1550u; run(100); assert(!ons);                 /* 650 above its rest: not yet */
    scan.values[49] = 2500u; run(100); assert(ons == 1u);
    scan.values[49] = 900u; run(100); assert(offs == 1u);            /* back to its rest: let go */
    scan.values[49] = 2500u; run(100); assert(ons == 2u);            /* and plays again */
    scan.values[49] = 900u; run(100); assert(offs == 2u);
    scan.values[51] = 1050u; run(100); assert(ons == 3u);            /* the others: 750 above their rest */
    scan.values[51] = 300u; run(100); assert(offs == 3u);
}
/* Chords on trades the halves during a session: each sensor keeps its own resting level. Keyed by
 * logical pad, the melody pads now on the high-resting left half would take the right half's
 * thresholds (Trigger 1000 < their 1300 rest) and play by themselves. */
static void traded_halves(void) {
    fresh();
    for (uint32_t k = 0; k < TS_KEYS; ++k) scan.values[k] = k < 48u ? 1300u : 300u;
    run(1000); assert(e.state == TK_ACTIVE && !ons);
    tc_state.chords_right = 1u; run(500); assert(!ons);               /* nothing plays by itself */
    assert(tp_key_config(&e, 49u).release == 1600u && tp_key_config(&e, 1u).release == 600u);
    scan.values[1] = 3000u; run(100); assert(ons == 1u);              /* physical 1 = melody pad 49 */
    scan.values[1] = 1300u; run(100); assert(offs == 1u);             /* lets go at its own rest */
    scan.values[49] = 1250u; run(100); assert(ons > 1u);              /* chord pad 1, right half: from its rest */
    scan.values[49] = 300u; run(100); assert(offs == ons);
    uint32_t n = ons;
    tc_state.chords_right = 0u; run(500); assert(ons == n && offs == n);
}
/* A board that flexes (moved from a desk to a lap): its pads' rest moves while untouched. The levels
 * follow (down within 2 s, up within seconds), so a pad still lets go where it now rests; a press,
 * even a long one, is never taken for rest; a finger resting below Trigger for seconds is. */
static void flexing_board(void) {
    fresh();
    for (uint32_t k = 0; k < TS_KEYS; ++k) scan.values[k] = 400u;
    run(1000); assert(e.state == TK_ACTIVE && e.rest[51] == 400u);
    for (uint32_t k = 0; k < TS_KEYS; ++k) scan.values[k] = 1000u;  /* lifted onto a lap: all rest at 1000 */
    run(100); assert(!ons && e.rest[51] < 1000u);                    /* below Trigger: nothing plays */
    run(5000); assert(!ons && e.rest[51] >= 950u && tp_key_config(&e, 51u).release >= 1250u);
    scan.values[51] = 2500u; run(100); assert(ons == 1u);
    scan.values[51] = 1000u; run(100); assert(offs == 1u);           /* lets go at its new rest */
    uint32_t before = e.rest[51];
    scan.values[51] = 3000u; run(10000); assert(e.rest[51] == before); /* a 10 s press: not rest */
    scan.values[51] = 1000u; run(100); assert(offs == 2u);
    for (uint32_t k = 0; k < TS_KEYS; ++k) scan.values[k] = 300u;   /* back on the desk */
    run(2500); assert(e.rest[51] <= 330u && tp_key_config(&e, 51u).press <= 1030u);
    scan.values[51] = 1250u; run(100); assert(ons == 3u);           /* the light touch is back */
    scan.values[51] = 300u; run(100); assert(offs == 3u);
    scan.values[52] = 950u; run(5000); assert(e.rest[52] >= 850u && ons == 3u); /* a resting finger becomes rest */
    scan.values[52] = 300u; run(2500); assert(e.rest[52] <= 330u);
}
/* Rescan pads (1.7.0): every pad's resting level from the next frame, at once, not followed over seconds;
 * a pad held meanwhile gets the board's middle level, and nothing plays by it. */
static void rescan(void) {
    fresh();
    for (uint32_t k = 0; k < TS_KEYS; ++k) scan.values[k] = 400u;
    run(1000); assert(e.state == TK_ACTIVE && e.rest[51] == 400u);
    for (uint32_t k = 0; k < TS_KEYS; ++k) scan.values[k] = 700u + 3u * k; /* moved: every pad rests higher */
    scan.values[60] = 3000u;                                               /* and one is held */
    run(20); assert(e.rest[51] < 700u);
    tc_state.rescan = true; run(20);
    assert(!tc_state.rescan && e.rest[51] == 700u + 3u * 51u && e.rest[95] == 700u + 3u * 95u);
    assert(e.rest[60] >= 700u && e.rest[60] < 1000u);                       /* the middle level, not the press */
    assert(ons == 1u);                                                      /* only the held pad played */
    scan.values[60] = 700u + 180u; run(100); assert(offs == 1u);
    scan.values[51] = 700u + 153u + 1300u; run(100); assert(ons == 2u);     /* plays from its new rest */
    scan.values[51] = 700u + 153u; run(100); assert(offs == 2u);
    tc_state.rescan = true; tc_state.ui = TC_UI_MENU; run(100); assert(!tc_state.rescan && ons == 2u);
}
/* Physical pad at a place on the board: side 0 left, column 0..11, row 0..3 (Pad test names L C1 R1). */
static uint32_t pad_at(uint8_t side, uint8_t column, uint8_t row) {
    tp_position p;
    for (uint32_t k = 0; k < TS_KEYS; ++k) if (tp_position_of(k, &p) && p.side == side && p.column == column && p.row == row) return k;
    assert(0);
    return 0u;
}
/* A warped board (1.7.0, readings from a real board): L C9 R4 rests at 1004, L C11 R4 at 889. A firm
 * press of L C10 R4 (3800) lifts them to 1618 and 1168, and after it L C9 R4 settles at 1078; after a
 * press of its own, at 1228. Before, its Trigger was 1334 (330 above rest): the bleed played it, and
 * settling above its Release (1134, then 1208 as its rest followed) it never let go. */
static void warped_board(void) {
    /* The same places on the right half: one note a pad, so counting notes counts pads. */
    uint32_t c9 = pad_at(1u, 8u, 3u), c10 = pad_at(1u, 9u, 3u), c11 = pad_at(1u, 10u, 3u);
    fresh();
    for (uint32_t k = 0; k < TS_KEYS; ++k) scan.values[k] = 300u;
    scan.values[c9] = 1004u; scan.values[c11] = 889u;
    run(1000); assert(e.state == TK_ACTIVE && !ons);
    assert(tp_key_config(&e, c9).press == 1704u && tp_key_config(&e, c9).release == 1304u);
    /* The bleed as measured: below their Triggers now. */
    scan.values[c10] = 3800u; scan.values[c9] = 1618u; scan.values[c11] = 1168u;
    run(100); assert(ons == 1u && e.keys[c10].admitted && !e.keys[c9].sensed && !e.keys[c11].sensed);
    scan.values[c10] = 300u; scan.values[c9] = 1078u; scan.values[c11] = 935u;
    run(100); assert(offs == 1u && ons == 1u);
    /* Stronger bleed, past L C9 R4's Trigger: still less than half of the press's rise: silent. */
    run(3000); /* rests settled again */
    scan.values[c10] = 3800u; scan.values[c9] = 1900u; run(100); assert(ons == 2u && !e.keys[c9].sensed);
    scan.values[c9] = 3000u; run(100); assert(ons == 3u && e.keys[c9].admitted); /* really pressed: it plays */
    scan.values[c10] = 300u; scan.values[c9] = 1078u; run(100); assert(offs == 3u);
    /* Pressed on its own and settling above its Release: Unstick lets it go. */
    run(3000);
    uint32_t rest = e.rest[c9];
    scan.values[c9] = 3800u; run(100); assert(ons == 4u);
    scan.values[c9] = rest + 400u; run(250); assert(offs == 3u);     /* 400 above rest, Release +300: settling */
    run(100); assert(offs == 4u && e.rest[c9] == rest + 400u);       /* still for 0.3 s: let go, its rest now */
    run(3000); assert(ons == 4u);                                    /* and it stays quiet there */
    /* Unstick off: only Release ends it. */
    scan.values[c9] = e.rest[c9]; run(3000); rest = e.rest[c9];
    tc_state.unstick = 0u;
    scan.values[c9] = 3800u; run(100); assert(ons == 5u);
    scan.values[c9] = rest + 400u; run(500); assert(offs == 4u);     /* above Release: still on */
    scan.values[c9] = rest + 200u; run(100); assert(offs == 5u);
    tc_state.unstick = 1u;
}
/* Unstick keeps aftertouch: a finger wanders (50 to 75 or more), so held notes stay, eased off or not; only a
 * reading within TC_PAD_SETTLE_BAND ends one: after TC_PAD_SETTLE_MS below Trigger, TC_PAD_STILL_MS at or above
 * it, never from TC_PAD_REST_CAP up (a hard press can read a flat 4095). */
static void unstick_keeps_aftertouch(void) {
    fresh();
    for (uint32_t k = 0; k < TS_KEYS; ++k) scan.values[k] = 300u;
    run(1000);
    scan.values[51] = 4000u; run(100); assert(ons == 1u);             /* a hard strike */
    for (uint32_t i = 0; i < 100u; ++i) { scan.values[51] = i & 1u ? 1460u : 1540u; run(30); }
    assert(!offs && e.keys[51].pressure >= 45u);                      /* eased off, a finger's +-40 for 3 s: kept */
    for (uint32_t i = 0; i < 40u; ++i) { scan.values[51] = i & 1u ? 750u : 950u; run(30); }
    assert(!offs);                                                    /* below Trigger, wobbling +-100: kept */
    scan.values[51] = 950u; run(250); assert(!offs);                  /* still: settling */
    run(100); assert(offs == 1u && e.rest[51] == 950u);               /* still for 0.3 s: ends, its rest now */
    scan.values[51] = 300u; run(3000); assert(e.rest[51] == 300u);
    scan.values[51] = 1300u; run(100); assert(ons == 2u);             /* a light press */
    for (uint32_t i = 0; i < 100u; ++i) { scan.values[51] = 900u - 3u * i; run(15); } /* easing off slowly */
    assert(offs == 1u);                                               /* 60 in 0.3 s: kept, aftertouch falling */
    run(400); assert(offs == 2u);                                     /* stopped at 603, still: ends */
    scan.values[51] = 300u; run(3000);
    scan.values[51] = 1263u; run(100); assert(ons == 3u);             /* above Trigger, held within +-5 */
    for (uint32_t i = 0; i < 27u; ++i) { scan.values[51] = 1263u + (i % 3u) * 4u; run(30); }
    assert(offs == 2u);
    run(300); assert(offs == 3u && e.rest[51] >= 1263u && e.rest[51] <= 1271u); /* still for 1 s: ends, its rest */
    run(2000); assert(ons == 3u);
    scan.values[51] = 300u; run(3000);
    scan.values[51] = 4095u; run(3000); assert(ons == 4u && offs == 3u); /* a flat 4095: a press, kept */
    scan.values[51] = 300u; run(100); assert(offs == 4u);
    tc_state.unstick = 0u;
    scan.values[51] = 300u; run(3000); scan.values[51] = 1300u; run(100); assert(ons == 5u);
    scan.values[51] = 700u; run(3000); assert(offs == 4u);            /* Unstick off: a still hold stays */
    scan.values[51] = 1263u; run(3000); assert(offs == 4u);           /* above Trigger too */
    scan.values[51] = 300u; run(100); assert(offs == 5u);
    tc_state.unstick = 1u;
}
/* L C9 R4 and L C11 R4 (a warped board's readings) were found sounding untouched, reading 1263 and 918
 * within +-5 with their rests at 397 and 15: low readings had taken the rests down (a quarter of the gap
 * every 15 ms then), so their usual level read as a press, and a sounding pad's rest is frozen. Now a
 * burst of low readings moves a rest much less, and a pad held that still lets go after TC_PAD_STILL_MS. */
static void rest_sank(void) {
    uint32_t c9 = pad_at(1u, 8u, 3u), c11 = pad_at(1u, 10u, 3u);
    fresh();
    for (uint32_t k = 0; k < TS_KEYS; ++k) scan.values[k] = 300u;
    scan.values[c9] = 1263u; scan.values[c11] = 918u;
    run(1000); assert(e.rest[c9] == 1263u && e.rest[c11] == 918u);
    scan.values[c9] = scan.values[c11] = 0u; run(150);               /* 0.15 s of low readings */
    assert(e.rest[c9] > 900u && e.rest[c11] > 650u);                 /* (a quarter a step: under 80 and 60) */
    scan.values[c9] = 1263u; scan.values[c11] = 918u; run(3000); assert(!ons); /* their usual level: silent */
    scan.values[c9] = scan.values[c11] = 0u; run(1000);              /* a whole second low */
    assert(e.rest[c9] < 400u && e.rest[c11] < 300u);
    for (uint32_t i = 0; i < 60u; ++i) {                             /* back, held within +-5 for 0.9 s */
        scan.values[c9] = 1263u + (i % 3u) * 4u; scan.values[c11] = 914u + (i % 4u) * 3u; run(15);
    }
    assert(ons == 2u && !offs);                                       /* both play by themselves, as on the board */
    for (uint32_t i = 0; i < 20u; ++i) {
        scan.values[c9] = 1263u + (i % 3u) * 4u; scan.values[c11] = 914u + (i % 4u) * 3u; run(15);
    }
    assert(offs == 2u && e.rest[c9] >= 1263u && e.rest[c11] >= 914u);  /* still for 1 s: let go, their rests */
    for (uint32_t i = 0; i < 200u; ++i) {
        scan.values[c9] = 1263u + (i % 3u) * 4u; scan.values[c11] = 914u + (i % 4u) * 3u; run(15);
    }
    assert(ons == 2u);                                                /* and quiet there */
}
/* L C9 R4 after Rescan pads (a warped board's readings): rescanned while it sat high, it let go for
 * minutes; then, sunk back to 1004 untouched, its rest followed it down, and a press settling 650
 * above (past Release and the first form's 600 span; and a soft press, whose quarter rise is below Release) stuck
 * on with Unstick: a sounding pad's rest is frozen. Now every press lets go TC_PAD_SETTLE_MS after it
 * settles, however high, and that level becomes its rest; a slow creep down settles too. */
static void settled_high(void) {
    uint32_t c9 = pad_at(1u, 8u, 3u);
    fresh();
    for (uint32_t k = 0; k < TS_KEYS; ++k) scan.values[k] = 300u;
    scan.values[c9] = 1004u;
    run(1000); assert(e.state == TK_ACTIVE && e.rest[c9] == 1004u);
    scan.values[c9] = 1228u; tc_state.rescan = true; run(100); assert(e.rest[c9] == 1228u && !ons);
    scan.values[c9] = 1004u; run(3000); assert(e.rest[c9] == 1004u && !ons); /* sank back: its rest followed */
    static const uint32_t peaks[] = {2000u, 3800u, 2000u, 3800u};
    for (uint32_t i = 0; i < 4u; ++i) {
        uint32_t rest = e.rest[c9];
        scan.values[c9] = peaks[i]; run(100); assert(ons == i + 1u);
        scan.values[c9] = rest + 650u; run(250); assert(offs == i);          /* settling */
        run(100); assert(offs == i + 1u && e.rest[c9] == rest + 650u);       /* let go: its rest now */
        scan.values[c9] = 1004u; run(3000); assert(e.rest[c9] == 1004u && ons == i + 1u);
    }
    scan.values[c9] = 3800u; run(100); assert(ons == 5u);
    for (uint32_t v = 1690u; v > 1300u; v -= 30u) { scan.values[c9] = v; run(15); } /* creeping down */
    assert(offs == 4u);
    scan.values[c9] = 1330u; run(400); assert(offs == 5u && e.rest[c9] == 1330u); /* the creep stopped */
    scan.values[c9] = 1004u; run(3000);
    tc_state.unstick = 0u;
    scan.values[c9] = 3800u; run(100); assert(ons == 6u);
    scan.values[c9] = 1654u; run(3000); assert(offs == 5u);                 /* Unstick off: stuck, as with the first form */
    scan.values[c9] = 1200u; run(100); assert(offs == 6u);                  /* until below Release */
    tc_state.unstick = 1u;
}
/* Settings word 32: 0 while at the defaults (older firmware keeps reading the record), otherwise the
 * thresholds as levels (each setting + its offset), as 0.10.0 to 1.6.0 store them; invalid levels are
 * refused, 1.6 levels convert; Unstick off in word 19 bit 8; presets leave the board's settings alone. */
static void saved(void) {
    static tp_engine idle;
    fs_erase(); tc_init(&tc_state); (void)st_load(&tc_state);
    uint32_t p[ST_PAYLOAD];
    assert(st_get(ST_LIVE, 0, &tc_state, p) == ST_OK && p[32] == 0u && !(p[19] >> 8));
    tc_state.pad_trigger = 8u; tc_state.pad_release = 4u; tc_state.pad_full = 20u; tc_state.unstick = 0u;
    assert(st_get(ST_LIVE, 0, &tc_state, p) == ST_OK && p[32] == (13u | 7u << 8 | 30u << 16) && p[19] >> 8 == 1u);
    tc_controls back; tc_init(&back);
    assert(st_put(ST_LIVE, 0, p, &back, &idle) == ST_OK && back.pad_trigger == 8u && back.pad_release == 4u &&
           back.pad_full == 20u && back.unstick == 0u);
    uint32_t q[ST_PAYLOAD];
    static const uint32_t bad[] = {8u | 7u << 8 | 30u << 16, 4u | 3u << 8 | 30u << 16, 8u | 4u << 8 | 12u << 16,
                                   31u | 4u << 8 | 40u << 16, 8u | 4u << 8 | 30u << 16 | 1u << 24};
    for (uint32_t i = 0; i < sizeof bad / sizeof bad[0]; ++i) {
        memcpy(q, p, sizeof q); q[32] = bad[i];
        tc_controls before = back;
        assert(st_put(ST_LIVE, 0, q, &back, &idle) == ST_BAD_VALUE && !memcmp(&before, &back, sizeof back));
    }
    memcpy(q, p, sizeof q); q[19] |= 1u << 10; /* bit 9: Spread of Chord (1.7.0) */
    { tc_controls before = back; assert(st_put(ST_LIVE, 0, q, &back, &idle) == ST_BAD_VALUE && !memcmp(&before, &back, sizeof back)); }
    /* 1.6's levels: its defaults are 1.7's; a tuned board keeps about its feel, in range. */
    memcpy(q, p, sizeof q); q[19] = 0u;
    q[32] = 12u | 6u << 8 | 35u << 16; assert(st_put(ST_LIVE, 0, q, &back, &idle) == ST_OK &&
        back.pad_trigger == TC_PAD_TRIGGER_DEFAULT && back.pad_release == TC_PAD_RELEASE_DEFAULT && back.pad_full == TC_PAD_FULL_DEFAULT &&
        back.unstick == 1u);
    q[32] = 25u | 15u << 8 | 40u << 16; assert(st_put(ST_LIVE, 0, q, &back, &idle) == ST_OK &&
        back.pad_trigger == 20u && back.pad_release == 12u && back.pad_full == 30u);   /* the old fixed feel */
    q[32] = 5u | 3u << 8 | 15u << 16; assert(st_put(ST_LIVE, 0, q, &back, &idle) == ST_OK &&
        back.pad_trigger == 5u && back.pad_release == 1u && back.pad_full == 10u);     /* the lightest */
    q[32] = 0u; assert(st_put(ST_LIVE, 0, q, &back, &idle) == ST_OK && back.pad_trigger == TC_PAD_TRIGGER_DEFAULT);
    /* A preset saved with other thresholds loads without touching the board's. */
    assert(st_put(ST_PRESET, 3, p, &tc_state, &idle) == ST_OK);
    tc_state.pad_trigger = 12u; tc_state.pad_release = 6u; tc_state.pad_full = 28u; tc_state.unstick = 1u;
    tc_state.request = TC_REQUEST_LOAD; tc_state.request_slot = 3u; st_requests(&tc_state, &idle);
    assert(tc_state.preset == 3u && tc_state.pad_trigger == 12u && tc_state.pad_release == 6u && tc_state.pad_full == 28u &&
           tc_state.unstick == 1u);
}
int main(void) {
    ranking(); pages(); rules(); stock_board(); modded_board(); uneven_board(); traded_halves(); flexing_board(); rescan();
    warped_board(); unstick_keeps_aftertouch(); settled_high(); rest_sank(); saved();
    puts("PASS: pads: Trigger/Release/Full/Unstick pages and values, the thresholds above each pad's own resting "
         "level at every setting and rest (capped), each pad's resting level (an uneven board's high pads play and "
         "let go like the others, kept by sensor when the halves trade, a flexing board followed, a press never "
         "taken for rest; Rescan pads measures them again at once), a stock board resting 290..470 never plays by "
         "itself, velocity 1..127 from Trigger to Full, a modded board resting 1100..1250 with pads held at the "
         "start still releases; a warped board (bleed of a neighbor's press stays silent, a pad settling above its "
         "Release lets go with Unstick, not without); Unstick keeps a moving finger's holds and a flat 4095, ends a "
         "reading still within 30 for 0.3 s below Trigger (L C9 R4 after its rest sank, soft and hard presses, a creep) "
         "or 1 s above it (L C9 R4 and L C11 R4 left sounding) and takes it for rest; low readings move a rest slowly; "
         "settings word 32 "
         "as levels (0 at the defaults, invalid refused, 1.6's converted), Unstick in word 19, presets keep the "
         "board's; Pad test ranking");
    return 0;
}
