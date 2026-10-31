/* SPDX-License-Identifier: GPL-3.0-or-later - 1.9.0, the feel (Customize Stage 3) on the real engine, settings.c
 * and sysex.c: the curves (straight between their points, the default pressure curve exactly as before), the
 * pressure that goes out through each half's curve and range (channel pressure here) while velocity and the
 * pad's own pressure stay as they were, the Custom velocity curve with its lowest and highest velocity, the
 * feel record (checks, the defaults, saved 3 s after the last change, back after a restart, compaction) and
 * the PADS request (each pad's resting level and reading, by pad number). */
#include "../../src/tb/settings.h"
#include "../../src/tb/sysex.h"
#include "../../src/tb/flash_store.h"
#include "../../src/tb/nor.h"
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
static tp_engine e;
static ts_scan scan;
static uint32_t now = 1000u, channel_packets;
static uint8_t velocity[128], channel_value;
static bool writer(const uint8_t p[4], uint32_t token) {
    (void)token;
    if (p[0] == TK_ON) velocity[p[2]] = p[3];
    if (p[0] == TK_CHANNEL) { channel_packets++; channel_value = p[2]; }
    return true;
}
static void tick(void) {
    now += 10u; scan.frame++; scan.last_ms = now; scan.valid = true;
    if (e.state == TK_IDLE) (void)tc_arm(&e, &scan, now, true);
    else (void)tc_frame(&e, &scan, now);
    tc_step(&e, now, true, writer);
    if (e.pending_id) tp_complete(&e, e.pending_id, 4, true, now);
}
static void run(uint32_t ms) { for (uint32_t t = 0; t < ms; t += 10u) tick(); }
static void boot(bool erase) {
    if (erase) memset(flash, 0xFF, sizeof flash);
    tc_init(&tc_state); (void)st_load(&tc_state);
}
static void curves(void) {
    uint8_t f[TC_FEEL_BYTES]; tc_feel_default(f); assert(tc_feel_ok(f));
    for (uint32_t part = 0; part < 4u; ++part) {
        const uint8_t *p = &f[part * TC_FEEL_PART];
        assert(tc_feel_curve(p, 0) == 0u && tc_feel_curve(p, 127) == 127u);
        for (uint32_t x = 0; x <= 127u; ++x) { int32_t d = (int32_t)tc_feel_curve(p, x) - (int32_t)x; assert(d >= -1 && d <= 1); }
    }
    for (uint32_t level = 0; level <= 127u; ++level) /* the default pressure: exactly as before */
        assert(tc_feel_pressure(f, false, (uint8_t)level) == level && tc_feel_pressure(f, true, (uint8_t)level) == level);
    /* Straight between points; any shape (also falling) is kept. */
    uint8_t g[TC_CURVE_POINTS] = {127, 0, 127, 0, 127, 0, 127, 0};
    for (uint32_t x = 0; x < 127u; ++x) assert(tc_feel_curve(g, x) <= 127u);
    assert(tc_feel_curve(g, 0) == 127u && tc_feel_curve(g, 127) == 0u);
    /* A range: below its start nothing, past its end full (with a curve straight to 127). */
    uint8_t *at = &f[TC_FEEL_AT(1)]; at[TC_CURVE_POINTS] = 32u; at[TC_CURVE_POINTS + 1u] = 96u;
    assert(tc_feel_ok(f));
    assert(tc_feel_pressure(f, true, 20u) == 0u && tc_feel_pressure(f, true, 100u) == 127u && tc_feel_pressure(f, true, 64u) == tc_feel_curve(at, 63u));
    assert(tc_feel_pressure(f, false, 64u) == 64u); /* the other half: still its own (default) */
    /* Refused: a velocity of 0, lowest above highest, a range that doesn't rise, a point past 127. */
    uint8_t bad[TC_FEEL_BYTES];
    memcpy(bad, f, sizeof bad); bad[TC_FEEL_VEL(0) + TC_CURVE_POINTS] = 0u; assert(!tc_feel_ok(bad));
    memcpy(bad, f, sizeof bad); bad[TC_FEEL_VEL(1) + TC_CURVE_POINTS] = 100u; bad[TC_FEEL_VEL(1) + TC_CURVE_POINTS + 1u] = 90u; assert(!tc_feel_ok(bad));
    memcpy(bad, f, sizeof bad); bad[TC_FEEL_AT(0) + TC_CURVE_POINTS] = 50u; bad[TC_FEEL_AT(0) + TC_CURVE_POINTS + 1u] = 50u; assert(!tc_feel_ok(bad));
    memcpy(bad, f, sizeof bad); bad[3] = 128u; assert(!tc_feel_ok(bad));
    puts("curves: straight between points, the defaults (pressure exactly as before), a range, the checks");
}
static void engine(void) {
    boot(true); tc_state.unstick = 0u;
    tc_state.pad_trigger = 25u; tc_state.pad_release = 15u; tc_state.pad_full = 40u; /* test_engine's readings */
    run(1200u); assert(e.state == TK_ACTIVE);
    /* Channel pressure: the default curve gives 1.8's value; a steeper chord curve more, the melody half not. */
    tc_state.at_chord = tc_state.at_melody = TC_AT_CHANNEL;
    scan.values[3] = 3000u; run(200u); assert(channel_value == 76u && e.keys[3].pressure == 76u);
    scan.values[3] = 0u; run(200u);
    static const uint8_t steep[TC_CURVE_POINTS] = {0, 40, 80, 110, 127, 127, 127, 127};
    memcpy(&tc_state.feel[TC_FEEL_AT(0)], steep, TC_CURVE_POINTS);
    scan.values[3] = 3000u; run(200u);
    assert(e.keys[3].pressure == 76u && channel_value == tc_feel_curve(steep, 76u) && channel_value > 76u);
    scan.values[3] = 0u; run(200u); assert(channel_value == 0u);
    scan.values[51] = 3000u; run(200u); assert(channel_value == 76u); /* the melody half: its own, the default */
    scan.values[51] = 0u; run(200u);
    /* Velocity is the strike's, whatever the pressure curve: Normal as before. */
    tp_notes v; assert(tc_pad(e.layout, 51u, &v) && v.count == 1u);
    scan.values[51] = 2600u; run(100u); assert(velocity[v.notes[0]] == 9u); scan.values[51] = 0u; run(100u);
    /* Custom: the half's curve between its lowest and highest. A flat curve at 64: velocity 64; at the top with
     * the range 40..60: 60. */
    tc_state.melody_vel = TC_VEL_CUSTOM;
    uint8_t *mv = &tc_state.feel[TC_FEEL_VEL(1)];
    memset(mv, 64, TC_CURVE_POINTS);
    scan.values[51] = 2600u; run(100u); assert(velocity[v.notes[0]] == 1u + 126u * 64u / 127u); scan.values[51] = 0u; run(100u);
    memset(mv, 127, TC_CURVE_POINTS); mv[TC_CURVE_POINTS] = 40u; mv[TC_CURVE_POINTS + 1u] = 60u;
    scan.values[51] = 2600u; run(100u); assert(velocity[v.notes[0]] == 60u); scan.values[51] = 0u; run(100u);
    memset(mv, 0, TC_CURVE_POINTS);
    scan.values[51] = 4000u; run(100u); assert(velocity[v.notes[0]] == 40u); scan.values[51] = 0u; run(100u);
    /* The default Custom curve is Normal give or take two (two roundings). */
    tc_feel_default(tc_state.feel);
    for (uint32_t reading = 2600u; reading <= 4000u; reading += 200u) {
        tc_state.melody_vel = TC_VEL_DYNAMIC; scan.values[51] = (uint16_t)reading; run(100u);
        uint8_t normal = velocity[v.notes[0]]; scan.values[51] = 0u; run(100u);
        tc_state.melody_vel = TC_VEL_CUSTOM; scan.values[51] = (uint16_t)reading; run(100u);
        int32_t d = (int32_t)velocity[v.notes[0]] - (int32_t)normal; assert(d >= -2 && d <= 2);
        scan.values[51] = 0u; run(100u);
    }
    tc_state.melody_vel = TC_VEL_DYNAMIC; tc_state.at_chord = tc_state.at_melody = TC_AT_POLY;
    assert(tc_menu_values(TC_MENU_VELOCITY) == TC_VEL_MODES && TC_VEL_CUSTOM == 6u);
    puts("engine: pressure out through each half's curve (pressure and velocity themselves unchanged); Custom velocity, "
         "its lowest and highest, the default curve = Normal within 2");
}
static void settle(void) { now += 10u; st_service(&tc_state, now); now += ST_SAVE_DELAY_MS + 10u; st_service(&tc_state, now); }
static void record(void) {
    boot(true);
    uint32_t p[ST_PAYLOAD], q[ST_PAYLOAD];
    /* The defaults read back; nothing stored for them. */
    assert(st_get(ST_FEEL, 0, &tc_state, p) == ST_OK && p[0] == 1u);
    uint32_t before = st_records(); tc_state.changes++; settle(); assert(st_records() == before + 1u); /* the live settings only */
    /* A curve set (as the editor's quiet record): in use at once, stored 3 s later, back after a restart. */
    p[1] = 0x7F7F7F7Fu; p[2] = 0x7F7F7F7Fu;
    assert(st_put(ST_FEEL, 0, p, &tc_state, &e) == ST_OK && tc_state.feel[0] == 127u);
    settle(); boot(false); assert(tc_state.feel[0] == 127u && tc_state.feel[7] == 127u);
    assert(st_get(ST_FEEL, 0, &tc_state, q) == ST_OK && !memcmp(p, q, sizeof p));
    /* Refused, nothing changed: slot 10, format 2, a stray word, a velocity of 0. */
    uint32_t bad[ST_PAYLOAD];
    memcpy(bad, p, sizeof bad); assert(st_put(ST_FEEL, 10, bad, &tc_state, &e) == ST_BAD_VALUE);
    memcpy(bad, p, sizeof bad); bad[0] = 2u; assert(st_put(ST_FEEL, 0, bad, &tc_state, &e) == ST_BAD_VALUE);
    memcpy(bad, p, sizeof bad); bad[20] = 1u; assert(st_put(ST_FEEL, 0, bad, &tc_state, &e) == ST_BAD_VALUE);
    memcpy(bad, p, sizeof bad); bad[3] &= ~0xFFu; assert(st_put(ST_FEEL, 0, bad, &tc_state, &e) == ST_BAD_VALUE); /* byte 8: lowest */
    assert(tc_state.feel[0] == 127u);
    /* All zero: the defaults again. */
    memset(bad, 0, sizeof bad); assert(st_put(ST_FEEL, 0, bad, &tc_state, &e) == ST_OK && tc_state.feel[0] == 0u && tc_state.feel[1] == 18u);
    settle(); boot(false); assert(tc_state.feel[1] == 18u);
    /* Compaction keeps a custom feel. */
    assert(st_put(ST_FEEL, 0, p, &tc_state, &e) == ST_OK); settle();
    uint32_t live[ST_PAYLOAD]; assert(st_get(ST_LIVE, 0, &tc_state, live) == ST_OK);
    while (st_records() < FS_SIZE / (4u * (4u + ST_PAYLOAD)) / 2u + 2u) assert(st_put(ST_PRESET, 0, live, &tc_state, &e) == ST_OK);
    before = st_records(); boot(false); assert(st_records() < before && tc_state.feel[0] == 127u);
    /* Presets leave the feel alone. */
    assert(st_put(ST_PRESET, 1, live, &tc_state, &e) == ST_OK);
    tc_feel_default(tc_state.feel); tc_request(&tc_state, TC_REQUEST_LOAD, 1u); st_requests(&tc_state, &e);
    assert(tc_state.last_action == TC_PRESET_LOADED && tc_state.feel[0] == 0u);
    puts("record: the defaults, set and in use at once, saved with the settings, kept over a restart and compaction, refused when wrong, presets leave it");
}
static void pads(void) {
    boot(true); memset(&e, 0, sizeof e); memset(&scan, 0, sizeof scan);
    tc_state.unstick = 0u; run(1200u); assert(e.rested);
    sx_scanner = &scan;
    scan.values[0] = 1234u; scan.values[95] = 4095u;
    uint8_t msg[16] = {0xF0, 0x7D, 0x54, 0x42, SX_PADS, (uint8_t)((128u - SX_PADS) % 128u), 0xF7};
    uint32_t len = sx_handle(msg, 7, &tc_state, &e, now);
    assert(len > 8u && sx_out[4] == (SX_PADS | 0x40u) && sx_out[5] == SX_OK);
    const uint8_t *d = &sx_out[6];
    assert(d[0] == TS_KEYS && d[1] == 3u && d[2] == tc_state.pad_trigger && d[3] == tc_state.pad_release && d[4] == tc_state.pad_full);
    assert(len == 6u + 6u + 4u * TS_KEYS + 2u);
    uint32_t v0 = d[8] | (uint32_t)d[9] << 7, v95 = d[6 + 4u * 95u + 2u] | (uint32_t)d[6 + 4u * 95u + 3u] << 7;
    assert(v0 == 1234u && v95 == 4095u && (d[6] | (uint32_t)d[7] << 7) == e.rest[0]);
    /* Chords on the right: pad 0 (a chord pad) is read where it now is. */
    tc_state.chords_right = 1u; scan.values[tc_physical(&tc_state, 0)] = 777u;
    (void)sx_handle(msg, 7, &tc_state, &e, now); assert((d[8] | (uint32_t)d[9] << 7) == 777u);
    sx_scanner = NULL; (void)sx_handle(msg, 7, &tc_state, &e, now); assert(d[1] == 1u && !(d[8] | d[9]));
    puts("pads: each pad's resting level and reading now, by pad number, with the thresholds; none without a scanner");
}
int main(void) {
    curves(); engine(); record(); pads();
    puts("PASS: the feel (1.9.0): curves, pressure out, Custom velocity, its record, the PADS request");
    return 0;
}
