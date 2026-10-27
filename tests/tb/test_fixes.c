/* SPDX-License-Identifier: GPL-3.0-or-later - regression tests for four engine/layout fixes found by
 * the stress tests (test_fuzz_*.c), each on the real engine with the loop of src/tb/main.c:
 * 1. voice leading that finds no step inside MIDI range plays the row's own chord instead of
 *    stopping the session (which left notes on and then refused every new session).
 * 2. a session that stops with notes on hands their Note Offs, pedal and pressure to the next.
 * 3. pressure output (channel aftertouch, controllers, pitch bend) returns to rest when a
 *    session drains and after a slow release.
 * 4. a session ending with the transport running sends Stop; the next one sends Continue. */
#include "../../src/tb/controls.h"
#include "../../src/tb/midi_in.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
volatile ts_abi ts_status;
static tp_engine e;
static ts_scan scan;
static uint32_t now, ons, offs, packets;
static uint8_t sounding[16][128];
static int32_t bend = 8192, cc74, aftertouch;
static char transport_log[32];
static uint8_t pedal_rx; /* the receiver's sustain (CC64) */
static uint32_t transport_n;
static bool writer(const uint8_t p[4], uint32_t token) {
    (void)token;
    uint32_t ch = p[1] & 15u;
    packets++;
    if (p[0] == TK_BEND) bend = p[2] | p[3] << 7;
    if (p[0] == TK_CC && p[2] == 74u) cc74 = p[3];
    if (p[0] == TK_CC && p[2] == 64u) pedal_rx = p[3];
    if (p[0] == TK_CHANNEL) aftertouch = p[2];
    if (p[0] == 0x0Fu && p[1] != 0xF8u && transport_n < sizeof transport_log - 1u) /* S(tart) C(ontinue) P (stop) */
        transport_log[transport_n++] = p[1] == 0xFAu ? 'S' : p[1] == 0xFBu ? 'C' : 'P';
    if (p[0] == TK_ON) { assert(!sounding[ch][p[2]]); sounding[ch][p[2]] = 1u; ons++; }
    if (p[0] == TK_OFF) { assert(sounding[ch][p[2]]); sounding[ch][p[2]] = 0u; offs++; }
    return true;
}
static uint32_t heard(void) {
    uint32_t n = 0;
    for (uint32_t c = 0; c < 16u; ++c) for (uint32_t p = 0; p < 128u; ++p) n += sounding[c][p];
    return n;
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
static void reset(void) {
    tc_init(&tc_state); tm_reset(); memset(&e, 0, sizeof e); memset(&scan, 0, sizeof scan);
    memset(sounding, 0, sizeof sounding); ons = offs = 0u; now += 1000u;
    bend = 8192; cc74 = aftertouch = 0;
}
/* main.c midi_input: a SysEx request drains the session. */
static void drain(void) {
    tp_disable(&e);
    for (uint32_t i = 0; i < 100u && e.state == TK_DRAINING; ++i) run(1);
    assert(e.state == TK_DONE);
}
/* 1. In Sen in B with the chord octave +2 (settings from 0.3-0.5), Add 9th on pad 45,
 * Voicing III and Voice lead: no ladder step near home fits. */
static void lead_without_room(void) {
    reset();
    tc_state.tonic = 11u; tc_state.scale = 34u; tc_state.chord_base = TP_CHORD_BASE + TP_OCTAVE_RANGE;
    tc_state.chord_voicing = 3u; tc_state.voice_lead = 1u; tc_state.chord[45].add = TC_ADD9;
    tp_layout l = tc_layout(&tc_state);
    tc_state.lead = (tp_notes){{71, 74, 78}, 3};
    tp_notes led, row;
    assert(tc_pad(l, 45u, &led) && tc_notes(l, 45u, tc_state.chord[45], &row));
    assert(led.count == row.count && !memcmp(led.notes, row.notes, row.count)); /* the row's own chord */
    for (uint32_t i = 0; i < led.count; ++i) assert(led.notes[i] <= 127u);
    /* On the engine: the session keeps playing and every note ends. */
    tc_state.lead.count = 0u;
    run(1000); assert(e.state == TK_ACTIVE);
    scan.values[3] = 3000u; run(300);
    scan.values[45] = 3000u; run(300);
    assert(e.state == TK_ACTIVE && !e.errors && heard() == 3u + led.count);
    scan.values[3] = scan.values[45] = 0u; run(300);
    assert(e.state == TK_ACTIVE && !heard() && ons == offs);
    run(2000); assert(e.state == TK_ACTIVE && !e.errors); /* and later sessions still start */
}
/* 3. Channel pressure, controllers and pitch bend go back to rest: when a session drains
 * with pads held (editor SysEx, stale scan), and after a slow release whose last step
 * was smaller than the deadband. */
static void pressure_rest(void) {
    reset();
    tc_state.at_melody = TC_AT_BEND; tc_state.at_chord = TC_AT_CC74; tc_state.chord_ch = 1u;
    run(1000);
    scan.values[51] = 4000u; scan.values[3] = 4000u; run(300);
    assert(bend == 16383 && cc74 == 127);
    drain();
    assert(bend == 8192 && cc74 == 0 && !heard()); /* pads still held: centred anyway */
    scan.values[51] = scan.values[3] = 0u; run(2000); assert(e.state == TK_ACTIVE);
    for (uint32_t mode = 0; mode < 2u; ++mode) {
        tc_state.at_melody = mode ? TC_AT_CHANNEL : TC_AT_BEND;
        scan.values[51] = 4000u; run(300);
        for (uint32_t v = 4000u; v > 1500u; v -= 25u) { scan.values[51] = v; run(15); } /* through the light end */
        scan.values[51] = 0u; run(500);
        assert(mode ? aftertouch == 0 : bend == 8192);
    }
    uint32_t settled = packets; run(1000); assert(packets == settled); /* at rest: no chatter */
}
/* 4. A drain stops a running transport and the next session continues it (no new Start,
 * which made external sequencers jump back to bar one after every editor exchange). */
static void transport_continues(void) {
    reset();
    memset(transport_log, 0, sizeof transport_log); transport_n = 0u;
    tc_state.clock_out = 1u;
    run(1000);
    tc_state.transport = true; run(200);
    assert(!strcmp(transport_log, "S"));
    drain(); assert(!strcmp(transport_log, "SP"));                  /* the drain stops it */
    run(1500); assert(e.state == TK_ACTIVE && !strcmp(transport_log, "SPC")); /* and it continues */
    tc_state.transport = false; run(200); assert(!strcmp(transport_log, "SPCP"));
    drain(); run(1500); assert(!strcmp(transport_log, "SPCP"));      /* stopped stays stopped */
    tc_state.transport = true; run(200); assert(!strcmp(transport_log, "SPCPS")); /* a new Start */
    tp_loss(&e); run(1500);                                           /* a failed session can't stop it */
    assert(e.state == TK_ACTIVE && !strcmp(transport_log, "SPCPSC"));
    tc_state.transport = false; run(200);
    assert(!strcmp(transport_log, "SPCPSCP"));
}
/* 2. A session that stops with notes on (USB lost or suspended, a receipt missing) owes
 * their Note Offs, its pedal "on" and its pressure: the next session sends them first.
 * After a USB loss with TRS out on, that next session is the TRS-only one, so the jack's
 * synth is released too. */
static void owed_after_stop(void) {
    reset();
    tc_state.at_melody = TC_AT_BEND; tc_state.pedal_mode = TC_PEDAL_SUSTAIN; tc_state.sustain = true;
    run(1000);
    scan.values[3] = scan.values[51] = 4000u; run(300);
    uint32_t held = heard();
    assert(held >= 4u && bend == 16383 && e.sustain_on[0]);
    uint32_t sent = packets;
    tp_loss(&e); run(100);                           /* stopped: nothing more goes out */
    assert(e.state == TK_STOPPED && packets == sent && heard() == held);
    scan.values[3] = scan.values[51] = 0u; tc_state.sustain = false;
    run(1500);                                       /* the next session */
    assert(e.state == TK_ACTIVE && !heard() && bend == 8192 && !e.sustain_on[0] && !tp_obligations(&e));
    /* The same pad again: a fresh Note On after its Off, nothing doubled. */
    scan.values[51] = 3000u; run(200); assert(heard() == 1u);
    scan.values[51] = 0u; run(200); assert(!heard() && ons == offs);
    /* 1.7.0 fix: the session is lost while the pedal's "on" is out but not yet confirmed: the receiver may
     * have it, so the next session sends the "off" (before, the pedal stayed down there). */
    reset(); pedal_rx = 0u;
    tc_state.pedal_mode = TC_PEDAL_SUSTAIN; run(1000); assert(e.state == TK_ACTIVE);
    tc_state.sustain = true;
    for (uint32_t i = 0; i < 50u && !pedal_rx; ++i, ++now) tc_step(&e, now, true, writer);
    assert(pedal_rx == 127u && e.pending_id && e.sustain_on[0] && tp_obligations(&e) == 1u);
    tp_loss(&e); tc_state.sustain = false;          /* no receipt */
    run(1500);
    assert(e.state == TK_ACTIVE && pedal_rx == 0u && !e.sustain_on[0] && !tp_obligations(&e));
}
int main(void) {
    lead_without_room();
    owed_after_stop();
    pressure_rest();
    transport_continues();
    puts("PASS: fixes: voice leading with no room near home plays the row's chord, the session keeps going; notes, pedal and "
         "bend a stopped session owed are ended by the next one; "
         "pressure, controllers and bend return to rest on a drain and after a slow release; a running "
         "transport stops with a drain and continues in the next session");
    return 0;
}
