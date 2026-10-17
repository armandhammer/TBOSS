/* SPDX-License-Identifier: GPL-3.0-or-later - Stage 2 engine features on the real engine:
 * per-side channels, strum timing, chord hold, arpeggiator (internal tempo and
 * MIDI clock, patterns, octaves, gate, latch, role changes), pressure as CC.
 * 1 ms ticks, a fresh scan frame every tick, every packet receipted at once. */
#include "../../src/tb/controls.h"
#include "../../src/tb/midi_in.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
volatile ts_abi ts_status;
static tp_engine e;
static ts_scan scan;
static uint32_t now;
typedef struct { uint32_t at; uint8_t status, a, b; } event;
static event log_[4096];
static uint32_t logged;
static int32_t sounding[16][128]; /* ons - offs per channel/pitch */
static bool writer(const uint8_t p[4], uint32_t token) {
    assert(token == e.issued + 1u && logged < 4096u);
    uint8_t kind = p[0], status = p[1], ch = status & 15u;
    assert((status >> 4) == kind && p[2] < 128u && p[3] < 128u);
    if (kind == TK_ON) { assert(sounding[ch][p[2]] == 0 && p[3] >= 1u); sounding[ch][p[2]]++; }
    if (kind == TK_OFF) { assert(sounding[ch][p[2]] == 1 && !p[3]); sounding[ch][p[2]]--; }
    log_[logged++] = (event){now, status, p[2], p[3]};
    return true;
}
static void tick(void) {
    now++; scan.frame++; scan.last_ms = now; scan.valid = true;
    if (e.state == TK_IDLE) (void)tc_arm(&e, &scan, now, true);
    else (void)tc_frame(&e, &scan, now);
    tc_step(&e, now, true, writer);
    if (e.pending_id) tp_complete(&e, e.pending_id, 4, true, now);
}
static void run(uint32_t ms) { while (ms--) tick(); }
static void clear_log(void) { logged = 0; }
static uint32_t count(uint8_t status) {
    uint32_t n = 0; for (uint32_t i = 0; i < logged; ++i) n += log_[i].status == status; return n;
}
static const event *nth(uint8_t status, uint32_t n) {
    for (uint32_t i = 0; i < logged; ++i) if (log_[i].status == status && !n--) return &log_[i];
    return NULL;
}
static void quiet(void) { /* nothing sounding, nothing owed */
    for (uint32_t ch = 0; ch < 16u; ++ch) for (uint32_t p = 0; p < 128u; ++p) assert(!sounding[ch][p]);
    assert(!tp_obligations(&e) && !e.errors && e.state == TK_ACTIVE);
}
static void defaults(void) {
    uint8_t brightness = tc_state.brightness;
    tc_init(&tc_state); tc_state.brightness = brightness;
    tc_state.at_chord = tc_state.at_melody = TC_AT_OFF; tc_state.vel_mode = TC_VEL_FIXED;
}
static void pad(uint32_t k, bool down) { scan.values[k] = down ? 3000u : 0u; }
/* Pad 3 = C major (C2 E2 G2 = 36 40 43); pad 7 = D minor (38 41 45); pad 51 = C3. */
static void channels(void) {
    defaults(); tc_state.chord_ch = 2u; tc_state.melody_ch = 5u; clear_log();
    pad(3, true); pad(51, true); run(20);
    assert(count(0x92) == 3u && count(0x95) == 1u && sounding[2][36] && sounding[5][48]);
    /* The same pitch on both channels is two voices. */
    tc_state.melody_ch = 2u; pad(52, true); run(20); /* C3 on the chord channel... */
    pad(52, false); run(20);
    tc_state.melody_ch = 5u;
    pad(3, false); pad(51, false); run(20);
    assert(count(0x82) == 4u && count(0x85) == 1u); quiet();
}
static void strum_timing(void) {
    defaults(); tc_state.strum = TC_STRUM_UP; tc_state.strum_speed = 2u; /* 20 ms */
    clear_log(); pad(3, true); uint32_t t0 = now + 1u; run(100);
    assert(count(0x90) == 3u);
    assert(nth(0x90, 0)->a == 36 && nth(0x90, 1)->a == 40 && nth(0x90, 2)->a == 43);
    assert(nth(0x90, 0)->at - t0 <= 1u && nth(0x90, 1)->at - nth(0x90, 0)->at == 20u &&
           nth(0x90, 2)->at - nth(0x90, 1)->at == 20u);
    pad(3, false); run(20); quiet();
    tc_state.strum = TC_STRUM_DOWN; clear_log(); pad(3, true); run(100);
    assert(nth(0x90, 0)->a == 43 && nth(0x90, 2)->a == 36);
    pad(3, false); run(20); quiet();
    /* Alternate: down then up then down. */
    tc_state.strum = TC_STRUM_ALTERNATE; uint8_t first[3];
    for (uint32_t i = 0; i < 3u; ++i) {
        clear_log(); pad(3, true); run(100); first[i] = nth(0x90, 0)->a; pad(3, false); run(20);
    }
    assert(first[0] != first[1] && first[0] == first[2]); quiet();
    /* Released mid-strum: notes not yet started never start. */
    tc_state.strum = TC_STRUM_UP; tc_state.strum_speed = 7u; /* 120 ms */
    clear_log(); pad(3, true); run(50); pad(3, false); run(300);
    assert(count(0x90) == 1u && count(0x80) == 1u); quiet();
}
static void hold(void) {
    defaults(); tc_state.hold = 1u; clear_log();
    pad(3, true); run(20); pad(3, false); run(50);
    assert(count(0x90) == 3u && !count(0x80) && e.keys[3].latched);
    /* Another chord replaces it; its own latch follows. */
    pad(7, true); run(20); assert(count(0x80) == 3u && count(0x90) == 6u);
    pad(7, false); run(50); assert(count(0x80) == 3u && sounding[0][38]);
    /* Touching the latched chord silences it, and it stays silent while held. */
    pad(7, true); run(50); assert(!sounding[0][38] && !sounding[0][41]);
    pad(7, false); run(50); assert(count(0x90) == 6u); quiet();
    /* Melody pads never latch. */
    pad(51, true); run(20); pad(51, false); run(20); assert(!sounding[0][48]);
    /* Hold off releases a latched chord. */
    pad(3, true); run(20); pad(3, false); run(20); assert(sounding[0][36]);
    tc_state.hold = 0u; run(20); quiet();
}
static uint8_t arp_notes[64];
static uint32_t arp_capture(uint32_t ms) {
    clear_log(); run(ms);
    uint32_t n = 0;
    for (uint32_t i = 0; i < logged && n < 64u; ++i) if ((log_[i].status & 0xF0u) == 0x90u) arp_notes[n++] = log_[i].a;
    return n;
}
static void arpeggiator(void) {
    defaults(); tc_state.arp = TC_ARP_UP; tc_state.arp_rate = 3u; tc_state.tempo = 16u; /* 1/16 at 120 = 125 ms */
    tc_state.arp_gate = 1u; clear_log();
    pad(3, true);
    uint32_t n = arp_capture(1000);
    assert(n == 8u || n == 9u);
    static const uint8_t up[] = {36, 40, 43, 36, 40, 43, 36, 40};
    for (uint32_t i = 0; i < 8u; ++i) assert(arp_notes[i] == up[i]);
    /* Step 125 ms apart, gate 50%: each Off 62 or 63 ms after its On. */
    assert(nth(0x90, 1)->at - nth(0x90, 0)->at == 125u);
    uint32_t gate = nth(0x80, 0)->at - nth(0x90, 0)->at; assert(gate >= 62u && gate <= 63u);
    pad(3, false); run(200); quiet();
    /* Patterns and octaves. */
    tc_state.arp = TC_ARP_DOWN; pad(3, true); n = arp_capture(400);
    assert(arp_notes[0] == 43 && arp_notes[1] == 40 && arp_notes[2] == 36); pad(3, false); run(200); quiet();
    tc_state.arp = TC_ARP_UPDOWN; pad(3, true); n = arp_capture(800);
    static const uint8_t updown[] = {36, 40, 43, 40, 36, 40};
    for (uint32_t i = 0; i < 6u; ++i) assert(arp_notes[i] == updown[i]);
    pad(3, false); run(200); quiet();
    tc_state.arp = TC_ARP_UP; tc_state.arp_octaves = 1u; pad(3, true); n = arp_capture(800);
    static const uint8_t two[] = {36, 40, 43, 48, 52, 55};
    for (uint32_t i = 0; i < 6u; ++i) assert(arp_notes[i] == two[i]);
    pad(3, false); run(200); quiet(); tc_state.arp_octaves = 0u;
    /* As played: D minor struck first, then C major. */
    tc_state.arp = TC_ARP_PLAYED; pad(7, true); run(5); pad(3, true); run(1);
    uint8_t pool[TP_ARP_POOL]; uint32_t m = tp_arp_pool(&e, pool);
    static const uint8_t played[] = {38, 41, 45, 36, 40, 43};
    assert(m == 6u); for (uint32_t i = 0; i < 6u; ++i) assert(pool[i] == played[i]);
    pad(7, false); pad(3, false); run(200); quiet();
    /* Random stays inside the pool. */
    tc_state.arp = TC_ARP_RANDOM; pad(3, true); n = arp_capture(2000);
    for (uint32_t i = 0; i < n; ++i) assert(arp_notes[i] == 36 || arp_notes[i] == 40 || arp_notes[i] == 43);
    pad(3, false); run(200); quiet();
    /* Gate 100%: legato, the next On follows the previous Off at the step. */
    tc_state.arp = TC_ARP_UP; tc_state.arp_gate = 3u; pad(3, true); n = arp_capture(500);
    assert(n >= 4u && !e.errors); pad(3, false); run(200); quiet();
    tc_state.arp_gate = 1u;
    /* Latch: with hold on the arpeggio continues after the hand lifts. */
    tc_state.hold = 1u; pad(3, true); run(20); pad(3, false); n = arp_capture(500);
    assert(n >= 3u);
    tc_state.hold = 0u; run(200); quiet();
    /* Switching the arpeggiator while a chord is held changes its role. */
    tc_state.arp = TC_ARP_OFF; pad(3, true); run(20); assert(sounding[0][36] && sounding[0][40] && sounding[0][43]);
    tc_state.arp = TC_ARP_UP; run(30); assert(e.keys[3].silent);
    { uint32_t on = 0; for (uint32_t p = 0; p < 128u; ++p) on += (uint32_t)sounding[0][p]; assert(on == 1u); }
    tc_state.arp = TC_ARP_OFF; run(30); assert(sounding[0][36] && sounding[0][40] && sounding[0][43]);
    pad(3, false); run(200); quiet();
}
static void clock_in(uint8_t status) { uint8_t p[4] = {0x0F, status, 0, 0}; tm_packet(p, now); }
static void clocked_arp(void) {
    defaults(); tm_reset(); tc_state.arp = TC_ARP_UP; tc_state.arp_rate = 3u; /* 6 ticks */
    tc_state.arp_gate = 1u;
    clock_in(0xFA);
    /* 100 BPM external clock: a tick every 25 ms -> a step every 150 ms. */
    for (uint32_t i = 0; i < 20u; ++i) { clock_in(0xF8); run(25); }
    assert(tm_clock_active(now) && tm.tick_us >= 24000u && tm.tick_us <= 26000u);
    pad(3, true); clear_log();
    for (uint32_t i = 0; i < 48u; ++i) { clock_in(0xF8); run(25); }
    uint32_t ons = count(0x90);
    assert(ons == 8u || ons == 9u);
    for (uint32_t i = 1; i + 1u < ons; ++i) assert(nth(0x90, i + 1u)->at - nth(0x90, i)->at == 150u);
    /* The clock stops: after 250 ms the internal tempo (120 BPM, 125 ms) takes over. */
    clear_log(); run(1500); assert(!tm_clock_active(now) && count(0x90) >= 9u);
    pad(3, false); run(200); quiet();
}
static void pressure_cc(void) {
    defaults(); tc_state.at_chord = tc_state.at_melody = TC_AT_CC74; tc_state.chord_ch = 1u; tc_state.melody_ch = 4u; clear_log();
    scan.values[3] = 3000u; scan.values[51] = 4000u; run(200);
    const event *a = nth(0xB1, 0), *b = nth(0xB4, 0);
    assert(a && a->a == 74u && a->b == 76u && b && b->a == 74u && b->b == 127u);
    pad(3, false); pad(51, false); run(200);
    assert(nth(0xB1, count(0xB1) - 1u)->b == 0u && nth(0xB4, count(0xB4) - 1u)->b == 0u);
    /* Same channel: one value, the strongest pad on either side. */
    tc_state.melody_ch = 1u; clear_log(); scan.values[3] = 3000u; scan.values[51] = 4000u; run(200);
    assert(!count(0xB4) && nth(0xB1, count(0xB1) - 1u)->b == 127u);
    pad(3, false); pad(51, false); run(200);
    /* Channel aftertouch on the melody channel; mod wheel CC1. */
    tc_state.at_chord = tc_state.at_melody = TC_AT_CC1; tc_state.melody_ch = 4u; clear_log(); scan.values[51] = 4000u; run(200);
    assert(nth(0xB4, 0)->a == 1u); pad(51, false); run(200);
    tc_state.at_chord = tc_state.at_melody = TC_AT_CHANNEL; clear_log(); scan.values[51] = 4000u; run(200);
    assert(nth(0xD4, 0)->a == 127u); pad(51, false); run(200);
    quiet();
}
/* Pressure per side: poly aftertouch on every unshared note of a chord pad; shared notes
 * skipped; each side its own setting; off per side. Pad 3 = C E G (36 40 43). */
static void chord_poly(void) {
    defaults(); tc_state.at_chord = TC_AT_POLY; tc_state.chord_ch = 2u; tc_state.melody_ch = 5u; clear_log();
    scan.values[3] = 2600u; run(20); scan.values[3] = 4000u; run(200);
    assert(count(0xA2) >= 3u);
    bool seen[128] = {false};
    for (uint32_t i = 0; i < logged; ++i) if (log_[i].status == 0xA2) { seen[log_[i].a] = true; assert(log_[i].b >= 100u); }
    assert(seen[36] && seen[40] && seen[43]);
    /* Notes shared with another held chord pad get none: pad 11 = E G B (40 43 47) shares E, G. */
    clear_log(); scan.values[11] = 4000u; run(20); scan.values[3] = 3000u; scan.values[11] = 3200u; run(200);
    for (uint32_t i = 0; i < logged; ++i) if (log_[i].status == 0xA2) assert(log_[i].a != 40u && log_[i].a != 43u);
    scan.values[3] = 0u; scan.values[11] = 0u; run(50); quiet();
    /* Different settings per side: chords channel pressure, melody poly; melody pad 51 = C3. */
    tc_state.at_chord = TC_AT_CHANNEL; tc_state.at_melody = TC_AT_POLY; clear_log();
    scan.values[3] = 4000u; scan.values[51] = 4000u; run(200);
    assert(count(0xD2) >= 1u && count(0xA5) >= 1u && !count(0xA2) && !count(0xD5));
    scan.values[3] = 0u; scan.values[51] = 0u; run(200); quiet();
    /* Off on one side: chords silent in pressure, melody still sends. */
    tc_state.at_chord = TC_AT_OFF; clear_log(); scan.values[3] = 4000u; scan.values[51] = 4000u; run(200);
    assert(!count(0xA2) && !count(0xD2) && count(0xA5) >= 1u);
    scan.values[3] = 0u; scan.values[51] = 0u; run(200); quiet();
    tc_state.at_chord = tc_state.at_melody = TC_AT_OFF;
}
/* Sustain pedal: CC64 per side, owed an "off" like a note. */
static void sustain_pedal(void) {
    defaults(); tc_state.chord_ch = 2u; tc_state.melody_ch = 5u; clear_log();
    tc_state.sustain = true; run(5);
    assert(count(0xB2) == 1u && count(0xB5) == 1u && nth(0xB2, 0)->a == 64u && nth(0xB2, 0)->b == 127u);
    assert(tp_obligations(&e) == 2u);
    pad(3, true); run(20); pad(3, false); run(20); /* notes still end; the synth holds them */
    assert(count(0x92) == 3u && count(0x82) == 3u);
    tc_state.sustain = false; run(5);
    assert(count(0xB2) == 2u && nth(0xB2, 1)->b == 0u && count(0xB5) == 2u && nth(0xB5, 1)->b == 0u); quiet();
    /* Channel change while down: off where it went, on where it goes now. */
    clear_log(); tc_state.sustain = true; run(5); tc_state.chord_ch = 3u; run(5);
    assert(count(0xB2) == 2u && nth(0xB2, 1)->b == 0u && count(0xB3) == 1u && nth(0xB3, 0)->b == 127u);
    /* Both sides on one channel: one controller, the other side's "on" is taken back. */
    tc_state.melody_ch = 3u; run(5);
    assert(count(0xB5) == 2u && nth(0xB5, 1)->b == 0u && count(0xB3) == 1u && tp_obligations(&e) == 1u);
    tc_state.sustain = false; run(5); assert(count(0xB3) == 2u); quiet();
    /* Draining (USB lost, TRS switched off...) sends the owed "off" before finishing. */
    clear_log(); tc_state.sustain = true; run(5); tp_disable(&e); run(5);
    assert(e.state == TK_DONE && !tp_obligations(&e) && count(0xB3) == 2u && nth(0xB3, 1)->b == 0u);
    tc_state.sustain = false; run(1100); assert(e.state == TK_ACTIVE); /* reset after 500 ms, arm 500 ms later */
}
int main(void) {
    tc_init(&tc_state); tm_reset();
    now = 1000u; run(600); assert(e.state == TK_ACTIVE);
    channels(); strum_timing(); hold(); arpeggiator(); clocked_arp(); pressure_cc(); chord_poly(); sustain_pedal();
    puts("PASS: stage 2 engine: per-side channels and per-channel voices, strum up/down/alternate timing and early release, chord hold latch/replace/silence/off, arpeggiator up/down/up-down/random/as-played, octaves, 50%/100% gate, latch, arp role switching, external clock steps/fallback, pressure as CC/channel AT per side and merged, chord-pad poly aftertouch on unshared notes, per-side pressure settings, sustain CC64 per side (channel change, shared channel, owed off on drain)");
    return 0;
}
