/* SPDX-License-Identifier: GPL-3.0-or-later - the jacks' logic: USB-MIDI packets as MIDI bytes
 * for the TRS jack, the queue the serial interrupt empties, and the pedal (debounce,
 * auto/normal/inverted types, sustain and hold modes). */
#include "../../src/tb/controls.h"
#include "../../src/tb/serial_midi.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
volatile ts_abi ts_status;
static void packets(void) {
    uint8_t out[3];
    const uint8_t on[4] = {0x09, 0x92, 60, 100}, off[4] = {0x08, 0x82, 60, 0}, cc[4] = {0x0B, 0xB0, 64, 127};
    const uint8_t chan[4] = {0x0D, 0xD3, 90, 0}, poly[4] = {0x0A, 0xA0, 61, 5}, clock[4] = {0x0F, 0xF8, 0, 0};
    const uint8_t program[4] = {0x0C, 0xC1, 7, 0}, reserved[4] = {0x01, 0x90, 1, 1};
    assert(sm_packet_bytes(on, out) == 3u && out[0] == 0x92 && out[1] == 60 && out[2] == 100);
    assert(sm_packet_bytes(off, out) == 3u && out[0] == 0x82);
    assert(sm_packet_bytes(cc, out) == 3u && out[1] == 64 && out[2] == 127);
    assert(sm_packet_bytes(poly, out) == 3u);
    assert(sm_packet_bytes(chan, out) == 2u && out[0] == 0xD3 && out[1] == 90); /* channel pressure: 2 bytes */
    assert(sm_packet_bytes(program, out) == 2u);
    assert(sm_packet_bytes(clock, out) == 1u && out[0] == 0xF8);
    assert(sm_packet_bytes(reserved, out) == 0u);
    for (uint8_t cin = 0; cin < 16u; ++cin) {
        uint8_t p[4] = {cin, 0x90, 1, 2};
        assert(sm_packet_bytes(p, out) <= 3u);
    }
}
static void ring(void) {
    sm_ring r; memset(&r, 0, sizeof r);
    uint8_t b, msg[3] = {0x90, 60, 100};
    assert(sm_space(&r) == SM_SIZE - 1u && !sm_pop(&r, &b));
    /* Fill with whole messages; the one that doesn't fit is refused entirely. */
    uint32_t pushed = 0;
    while (sm_push(&r, msg, 3u)) pushed += 3u;
    assert(pushed == (SM_SIZE - 1u) / 3u * 3u && sm_space(&r) < 3u && r.sent == pushed);
    uint32_t space = sm_space(&r);
    assert(!sm_push(&r, msg, 3u) && sm_space(&r) == space);
    /* Drain in order, then wrap around many times. */
    for (uint32_t i = 0; i < pushed; ++i) { assert(sm_pop(&r, &b)); assert(b == msg[i % 3u]); }
    assert(!sm_pop(&r, &b));
    for (uint32_t round = 0; round < 1000u; ++round) {
        uint8_t m[3] = {(uint8_t)(0x80u | (round & 15u)), (uint8_t)(round & 127u), (uint8_t)((round * 7u) & 127u)};
        assert(sm_push(&r, m, 3u));
        for (uint32_t i = 0; i < 3u; ++i) { assert(sm_pop(&r, &b) && b == m[i]); }
    }
    assert(sm_space(&r) == SM_SIZE - 1u);
}
static void pedal(void) {
    tc_controls c; tc_init(&c);
    assert(c.pedal_mode == TC_PEDAL_OFF && c.pedal_type == TC_PEDAL_AUTO && c.trs_out == 1u);
    c.pedal_mode = TC_PEDAL_SUSTAIN;
    /* Auto: high at start-up is "released" (a normally-open pedal on a pulled-up input). */
    tc_pedal(&c, true, 100); assert(!c.pedal_pressed && !c.sustain);
    tc_pedal(&c, false, 200); assert(!c.pedal_pressed);           /* not yet stable */
    tc_pedal(&c, true, 203); tc_pedal(&c, false, 205);             /* contact bounce */
    tc_pedal(&c, false, 214); assert(!c.pedal_pressed);
    tc_pedal(&c, false, 215); assert(c.pedal_pressed && c.sustain); /* 10 ms stable */
    tc_pedal(&c, true, 300); tc_pedal(&c, true, 310); assert(!c.pedal_pressed && !c.sustain);
    /* Auto with a normally-closed pedal: low at start-up is "released". */
    tc_init(&c); c.pedal_mode = TC_PEDAL_SUSTAIN;
    tc_pedal(&c, false, 50); tc_pedal(&c, false, 70); assert(!c.pedal_pressed);
    tc_pedal(&c, true, 80); tc_pedal(&c, true, 90); assert(c.pedal_pressed && c.sustain);
    /* Normal: low is pressed, whatever it read at start-up; inverted: high is pressed. */
    c.pedal_type = TC_PEDAL_NORMAL; tc_pedal(&c, true, 100); tc_pedal(&c, true, 110); assert(!c.pedal_pressed);
    tc_pedal(&c, false, 111); tc_pedal(&c, false, 121); assert(c.pedal_pressed);
    c.pedal_type = TC_PEDAL_INVERTED; tc_pedal(&c, false, 122); assert(!c.pedal_pressed);
    /* Hold mode: hold follows the pedal, without counting as a saved change. */
    tc_init(&c); c.pedal_mode = TC_PEDAL_HOLD;
    tc_pedal(&c, true, 0); tc_pedal(&c, true, 20);
    uint32_t changes = c.changes, events = c.event_sequence;
    tc_pedal(&c, false, 30); tc_pedal(&c, false, 40);
    assert(c.hold == 1u && !c.sustain && c.last_action == TC_HOLD && c.changes == changes && c.event_sequence == events + 1u);
    tc_pedal(&c, true, 50); tc_pedal(&c, true, 60); assert(c.hold == 0u);
    /* Off (the default): nothing follows the pedal. */
    tc_init(&c);
    tc_pedal(&c, true, 0); tc_pedal(&c, false, 20); tc_pedal(&c, false, 40);
    assert(c.pedal_pressed && !c.sustain && c.hold == 0u);
}
int main(void) {
    packets(); ring(); pedal();
    puts("PASS: jacks: USB-MIDI packets to MIDI bytes for every code index, whole-message queue (full, refuse, wrap, order), pedal debounce, auto/normal/inverted types, sustain/hold/off modes, hold not saved");
    return 0;
}
