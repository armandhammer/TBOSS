/* SPDX-License-Identifier: GPL-3.0-or-later - 1.4.0 rhythm core (rhythm.c) and the arpeggiator on
 * it: steps as fractions of a clock (exact, never drifting), Euclidean patterns and rotation, the
 * menu's list of them; on the real engine, the new rates (5 and 7 per beat, dotted, triplets) at
 * the tempo and from a DAW's clock, Euclidean rhythms with rotation, a rate change while playing,
 * sync on a fractional grid, and the stored settings (word 27, the wider ranges).
 * 1 ms ticks, a fresh scan frame every tick, every packet receipted at once. */
#include "../../src/tb/controls.h"
#include "../../src/tb/midi_in.h"
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
static uint32_t now;
typedef struct { uint32_t at; uint8_t status, a, b; } event;
static event log_[8192];
static uint32_t logged;
static int32_t sounding[16][128];
static bool writer(const uint8_t p[4], uint32_t token) {
    assert(token == e.issued + 1u && logged < 8192u);
    uint8_t kind = p[0], status = p[1], ch = status & 15u;
    assert((status >> 4) == kind && p[2] < 128u && p[3] < 128u);
    if (kind == TK_ON) { assert(sounding[ch][p[2]] == 0 && p[3] >= 1u); sounding[ch][p[2]]++; }
    if (kind == TK_OFF) { assert(sounding[ch][p[2]] == 1 && !p[3]); sounding[ch][p[2]]--; }
    log_[logged++] = (event){now, status, p[2], p[3]};
    return true;
}
/* A DAW's clock: one MIDI clock every daw_us microseconds while daw_us is set. */
static uint32_t daw_us, daw_next_us;
static void tick(void) {
    now++; scan.frame++; scan.last_ms = now; scan.valid = true;
    while (daw_us && (int32_t)(now * 1000u - daw_next_us) >= 0) {
        static const uint8_t clock[4] = {0x0Fu, 0xF8u, 0u, 0u};
        tm_packet(clock, now); daw_next_us += daw_us;
    }
    if (e.state == TK_IDLE) (void)tc_arm(&e, &scan, now, true);
    else (void)tc_frame(&e, &scan, now);
    tc_step(&e, now, true, writer);
    if (e.pending_id) tp_complete(&e, e.pending_id, 4, true, now);
}
static void run(uint32_t ms) { while (ms--) tick(); }
static void quiet(void) {
    for (uint32_t ch = 0; ch < 16u; ++ch) for (uint32_t p = 0; p < 128u; ++p) assert(!sounding[ch][p]);
    assert(!tp_obligations(&e) && !e.errors && e.state == TK_ACTIVE);
}
static void defaults(void) {
    tc_init(&tc_state);
    tc_state.at_chord = tc_state.at_melody = TC_AT_OFF; tc_state.vel_mode = tc_state.melody_vel = TC_VEL_FIXED;
    e.layout = tc_layout(&tc_state);
}
/* Note On times while chord pad 3 is held for ms. */
static uint32_t ons[1024];
static uint32_t arp_ons(uint32_t ms) {
    logged = 0; scan.values[3] = 3000u; run(ms); scan.values[3] = 0u; run(300); quiet();
    uint32_t n = 0;
    for (uint32_t i = 0; i < logged && n < 1024u; ++i) if ((log_[i].status & 0xF0u) == 0x90u) ons[n++] = log_[i].at;
    return n;
}
static uint8_t euclid_value(uint32_t hits, uint32_t steps) {
    uint32_t h = 0, s = 0;
    for (uint32_t i = 0; rh_euclid_at(i, &h, &s); ++i) if (h == hits && s == steps) return (uint8_t)(TC_RHYTHMS + i);
    assert(0); return 0;
}
/* --- pure ------------------------------------------------------------------------------- */
static void arithmetic(void) {
    /* Whole clocks: 1/16 = 6 clocks. */
    rh_rate sixteenth = {6, 1}, five = {24, 5}, seven = {24, 7};
    assert(rh_step_start(1, sixteenth) == 6u * RH_SUB && rh_step_at(6u * RH_SUB - 1u, sixteenth) == 0u &&
           rh_step_at(6u * RH_SUB, sixteenth) == 1u);
    /* Fractions: 5 and 7 per beat land exactly on the beat every 5 and 7 steps, for ever. */
    for (uint32_t beat = 0; beat < 100000u; beat += 997u) {
        assert(rh_step_start(5u * beat, five) == 24u * RH_SUB * beat);
        assert(rh_step_start(7u * beat, seven) == 24u * RH_SUB * beat);
    }
    /* Every step's start falls in that step; lengths differ by at most one sub-clock. */
    for (uint32_t i = 0; i < 5000u; ++i) {
        assert(rh_step_at(rh_step_start(i, five), five) == i && rh_step_at(rh_step_start(i + 1u, five) - 1u, five) == i);
        uint32_t len = rh_step_start(i + 1u, seven) - rh_step_start(i, seven);
        assert(len == 24u * RH_SUB / 7u || len == 24u * RH_SUB / 7u + 1u);
    }
    /* Euclidean: E(3,8) = X..X..X., E(5,8) has 5 hits, gaps of 1 or 2; rotation; edges. */
    assert(rh_euclid(3, 8, 0) == 0x49u);
    uint64_t five8 = rh_euclid(5, 8, 0); assert(five8 & 1u);
    uint32_t hits = 0; for (uint32_t i = 0; i < 8u; ++i) hits += (uint32_t)((five8 >> i) & 1u);
    assert(hits == 5u);
    for (uint32_t steps = 1; steps <= 64u; ++steps) for (uint32_t k = 0; k <= steps; ++k) {
        uint64_t m = rh_euclid(k, steps, 0); uint32_t count = 0, gap_lo = 64, gap_hi = 0, last = UINT32_MAX;
        for (uint32_t i = 0; i < steps; ++i) if ((m >> i) & 1u) {
            if (last != UINT32_MAX) { uint32_t g = i - last; if (g < gap_lo) gap_lo = g; if (g > gap_hi) gap_hi = g; }
            last = i; count++;
        }
        assert(count == k && (steps == 64u || !(m >> steps)));
        if (k >= 2u) assert(gap_hi - gap_lo <= 1u); /* as even as can be */
        for (uint32_t r = 0; r < steps; ++r) { /* rotating moves every hit r steps later */
            uint64_t rotated = rh_euclid(k, steps, r);
            for (uint32_t i = 0; i < steps; ++i) assert(((rotated >> ((i + r) % steps)) & 1u) == ((m >> i) & 1u));
        }
    }
    assert(rh_euclid(3, 8, 8) == rh_euclid(3, 8, 0) && rh_euclid(9, 8, 0) == 0xFFu && !rh_euclid(0, 8, 0) && !rh_euclid(1, 0, 0));
    /* The menu's list: 120 patterns, steps 2..16, hits 1..steps-1, each once, then nothing. */
    uint32_t seen = 0, h, s;
    for (uint32_t i = 0; rh_euclid_at(i, &h, &s); ++i) { assert(s >= 2u && s <= 16u && h >= 1u && h < s); seen++; }
    assert(seen == RH_EUCLIDS && RH_EUCLIDS + TC_RHYTHMS <= 255u);
    /* The old 8-step patterns keep their masks; a rotation turns them too. */
    uint32_t steps;
    for (uint8_t v = 0; v < TC_RHYTHMS; ++v) assert(tc_rhythm_mask(v, 0, &steps) == tc_rhythms[v] && steps == 8u);
    assert(tc_rhythm_mask(1, 1, &steps) == 0xAAu);
    assert(tc_rhythm_mask(euclid_value(5, 16), 0, &steps) == rh_euclid(5, 16, 0) && steps == 16u);
}
/* --- the arpeggiator ---------------------------------------------------------------------- */
static void rates(void) {
    defaults(); run(1000); assert(e.state == TK_ACTIVE); /* the session starts */
    tc_state.arp = TC_ARP_UP; tc_state.arp_gate = 1u; /* 120 BPM: a beat is 500 ms */
    /* The six earlier rates as before: 1/4 1/8 1/8T 1/16 1/16T 1/32. */
    static const uint32_t ms_x3[TC_ARP_RATES] = {1500, 750, 500, 375, 250, 188, 1000, 125, 300, 214, 1125, 563};
    for (uint8_t r = 0; r < TC_ARP_RATES; ++r) {
        tc_state.arp_rate = r;
        uint32_t n = arp_ons(4000u);
        /* Three steps take 3 x the step (to a millisecond), every time, however long it runs. */
        for (uint32_t i = 3; i < n; ++i) {
            uint32_t d = ons[i] - ons[i - 3u];
            assert(d + 2u >= ms_x3[r] && d <= ms_x3[r] + 2u);
        }
        assert(n * ms_x3[r] / 3u + ms_x3[r] >= 3900u && n * ms_x3[r] / 3u <= 4100u + ms_x3[r]);
    }
    /* 5 per beat for 20 s: 200 steps, the 1st, 6th, 11th ... exactly a beat apart (no drift). */
    tc_state.arp_rate = 8u;
    uint32_t n = arp_ons(20000u);
    assert(n >= 199u && n <= 201u);
    for (uint32_t i = 5; i < n; i += 5u) assert(ons[i] - ons[i - 5u] >= 499u && ons[i] - ons[i - 5u] <= 501u);
    assert(ons[195] - ons[0] >= 19499u && ons[195] - ons[0] <= 19501u);
    /* The same from a DAW at 120 BPM (a clock every 20.833 ms): 7 per beat, a beat every 7 steps. */
    daw_us = 20833u; daw_next_us = now * 1000u + 1000u; run(100);
    tc_state.arp_rate = 9u;
    n = arp_ons(10000u);
    assert(n >= 138u && n <= 142u);
    for (uint32_t i = 7; i < n; i += 7u) assert(ons[i] - ons[i - 7u] >= 497u && ons[i] - ons[i - 7u] <= 503u);
    daw_us = 0u; run(400);
    /* A rate change while playing: the next steps follow it, without a stall or a burst. */
    tc_state.arp_rate = 3u; logged = 0; scan.values[3] = 3000u; run(1000);
    tc_state.arp_rate = 0u; run(1600);
    tc_state.arp_rate = 8u; run(1000);
    scan.values[3] = 0u; run(300); quiet();
    n = 0; for (uint32_t i = 0; i < logged; ++i) if ((log_[i].status & 0xF0u) == 0x90u) ons[n++] = log_[i].at;
    for (uint32_t i = 1; i < n; ++i) assert(ons[i] - ons[i - 1u] >= 99u && ons[i] - ons[i - 1u] <= 501u);
    tc_state.arp = TC_ARP_OFF;
}
static void rhythms(void) {
    defaults(); tc_state.arp = TC_ARP_UP; tc_state.arp_rate = 3u; tc_state.arp_gate = 1u; /* 1/16: 125 ms */
    /* E3/8: steps 0, 3 and 6 of every 8 sound: gaps of 375, 375, 250 ms. */
    tc_state.arp_rhythm = euclid_value(3, 8);
    uint32_t n = arp_ons(3000u);
    assert(n >= 8u && n <= 10u);
    for (uint32_t i = 1; i < n; ++i) {
        uint32_t d = ons[i] - ons[i - 1u], expect = i % 3u == 0u ? 250u : 375u;
        assert(d + 1u >= expect && d <= expect + 1u);
    }
    /* Rotated by 1: steps 1, 4 and 7 sound, so the first step is silent. */
    tc_state.arp_rotate = 1u;
    logged = 0; scan.values[3] = 3000u; uint32_t touched = now; run(1000); scan.values[3] = 0u; run(300); quiet();
    uint32_t first = 0; for (uint32_t i = 0; i < logged; ++i) if ((log_[i].status & 0xF0u) == 0x90u) { first = log_[i].at; break; }
    assert(first - touched >= 124u && first - touched <= 127u);
    /* Silent steps still move the pattern on: the first note heard is the arpeggio's 2nd (E). */
    for (uint32_t i = 0; i < logged; ++i) if ((log_[i].status & 0xF0u) == 0x90u) { assert(log_[i].a == 40u); break; }
    tc_state.arp_rotate = 0u;
    /* A 5-step rhythm against the 3-note arpeggio: 15 steps before it repeats (polymeter). */
    tc_state.arp_rhythm = euclid_value(4, 5);
    logged = 0; scan.values[3] = 3000u; run(15u * 125u * 2u); scan.values[3] = 0u; run(300); quiet();
    uint8_t notes[64]; n = 0;
    for (uint32_t i = 0; i < logged && n < 64u; ++i) if ((log_[i].status & 0xF0u) == 0x90u) notes[n++] = log_[i].a;
    assert(n >= 20u);
    for (uint32_t i = 12; i < n; ++i) assert(notes[i] == notes[i - 12u]); /* 12 notes heard per 15 steps */
    bool differs = false; for (uint32_t i = 3; i < 12u; ++i) differs = differs || notes[i] != notes[i - 3u];
    assert(differs);
    tc_state.arp_rhythm = 0u;
    /* Sync at 5 per beat: the first step waits for the rate's grid (a beat), then 100 ms apart. */
    tc_state.arp_rate = 8u; tc_state.arp_sync = 1u; run(137);
    touched = now; n = arp_ons(1000u);
    assert(n >= 6u && ons[0] - touched <= 101u && ons[1] - ons[0] >= 99u && ons[1] - ons[0] <= 101u);
    tc_state.arp_sync = 0u; tc_state.arp = TC_ARP_OFF;
}
/* --- settings ---------------------------------------------------------------------------- */
static void stored(void) {
    tc_controls a; tc_init(&a);
    a.arp_rate = TC_ARP_RATES - 1u; a.arp_rhythm = TC_RHYTHM_VALUES - 1u; a.arp_rotate = TC_ROTATES - 1u;
    a.note_repeat = TC_REPEAT_BY_ROW; a.repeat_rhythm = RH_EUCLIDS;
    uint32_t p[ST_PAYLOAD];
    assert(st_get(ST_LIVE, 0, &a, p) == ST_OK);
    assert((p[27] >> 16 & 0xFFu) == RH_EUCLIDS && p[27] >> 24 == TC_ROTATES - 1u);
    tc_controls b; tc_init(&b);
    assert(st_put(ST_LIVE, 0, p, &b, &e) == ST_OK);
    assert(b.arp_rate == a.arp_rate && b.arp_rhythm == a.arp_rhythm && b.arp_rotate == a.arp_rotate &&
           b.note_repeat == a.note_repeat && b.repeat_rhythm == a.repeat_rhythm);
    /* Out of range: refused, nothing changes. */
    uint32_t bad[ST_PAYLOAD];
    memcpy(bad, p, sizeof bad); bad[27] = (bad[27] & 0x00FFFFFFu) | (uint32_t)TC_ROTATES << 24;
    assert(st_put(ST_LIVE, 0, bad, &b, &e) == ST_BAD_VALUE);
    memcpy(bad, p, sizeof bad); bad[27] = (bad[27] & 0xFF00FFFFu) | (RH_EUCLIDS + 1u) << 16;
    assert(st_put(ST_LIVE, 0, bad, &b, &e) == ST_BAD_VALUE);
    memcpy(bad, p, sizeof bad); bad[31] = (bad[31] & 0xFF00FFFFu) | TC_NOTE_REPEATS << 16;
    assert(st_put(ST_LIVE, 0, bad, &b, &e) == ST_BAD_VALUE);
    memcpy(bad, p, sizeof bad); bad[2] = (bad[2] & 0x00FFFFFFu) | (uint32_t)TC_ARP_RATES << 24;
    assert(st_put(ST_LIVE, 0, bad, &b, &e) == ST_BAD_VALUE);
    memcpy(bad, p, sizeof bad); bad[23] = (bad[23] & 0xFF00FFFFu) | (uint32_t)TC_RHYTHM_VALUES << 16;
    assert(st_put(ST_LIVE, 0, bad, &b, &e) == ST_BAD_VALUE);
    /* A 1.3 record (word 27's top half 0): every step, unrotated. */
    memcpy(bad, p, sizeof bad); bad[27] &= 0xFFFFu;
    assert(st_put(ST_LIVE, 0, bad, &b, &e) == ST_OK && !b.repeat_rhythm && !b.arp_rotate);
}
int main(void) {
    arithmetic();
    rates();
    rhythms();
    stored();
    puts("PASS: 1.4.0 rhythm: steps as fractions of a clock (5 and 7 per beat exact on every beat for "
         "100,000 beats), Euclidean patterns (every k of n up to 64 as even as can be, rotation, the "
         "menu's 120), the 8-step patterns kept; the arpeggiator at all 12 rates (3 steps to a "
         "millisecond, 20 s at 5 per beat without drift, 7 per beat from a DAW's clock), a rate change "
         "while playing, E3/8, rotated (silent steps move the pattern on), 4/5 against 3 notes, sync "
         "on a 5-per-beat grid; settings word 27 and the wider ranges stored, refused out of range, "
         "1.3 records unchanged");
    return 0;
}
