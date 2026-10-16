/* SPDX-License-Identifier: GPL-3.0-or-later - USB-MIDI 1.0 event packets (USB Device Class
 * Definition for MIDI Devices 1.0, table 4-1): byte 0 = cable << 4 | code index. */
#include "midi_in.h"
#include <string.h>
tm_state tm;
void tm_reset(void) { memset(&tm, 0, sizeof tm); }
static void note(uint8_t pitch, bool on) {
    uint8_t *n = &tm.notes[pitch & 127u];
    if (on && *n < 255u) { (*n)++; tm.notes_changed++; }
    else if (!on && *n) { (*n)--; tm.notes_changed++; }
}
static void sysex_bytes(const uint8_t *b, uint32_t count, bool end) {
    for (uint32_t i = 0; i < count; ++i) {
        if (b[i] == 0xF0u) {
            if (tm.in_sysex) tm.sysex_dropped++;          /* unterminated previous message */
            tm.in_sysex = true; tm.sysex_overflow = tm.sysex_ready; /* never touch a ready message */
            if (!tm.sysex_ready) tm.sysex_len = 0;
        }
        if (!tm.in_sysex) continue;
        if (!tm.sysex_overflow && tm.sysex_len < TM_SYSEX_MAX) tm.sysex[tm.sysex_len++] = b[i];
        else tm.sysex_overflow = true;
        if (b[i] == 0xF7u) {
            tm.in_sysex = false;
            if (tm.sysex_overflow) { tm.sysex_dropped++; tm.sysex_overflow = false; if (!tm.sysex_ready) tm.sysex_len = 0; }
            else tm.sysex_ready = true;
        }
    }
    if (end && tm.in_sysex) { tm.in_sysex = false; tm.sysex_dropped++; if (!tm.sysex_ready) tm.sysex_len = 0; }
}
static void realtime(uint8_t status, uint32_t now) {
    if (status == 0xF8u) {
        if (tm.have_tick && now - tm.last_tick_ms < TM_CLOCK_TIMEOUT_MS) {
            /* Smoothed period: USB delivers ticks in 1 ms frames, so average. */
            int32_t sample = (int32_t)((now - tm.last_tick_ms) * 1000u);
            if (!tm.tick_us) tm.tick_us = (uint32_t)sample;
            else tm.tick_us = (uint32_t)((int32_t)tm.tick_us + (sample - (int32_t)tm.tick_us) / 8);
        }
        tm.have_tick = true; tm.last_tick_ms = now; tm.ticks++;
    } else if (status == 0xFAu) { tm.starts++; tm.ticks = 0; }
}
void tm_packet(const uint8_t p[4], uint32_t now) {
    if (p[0] >> 4) return; /* one cable */
    switch (p[0] & 15u) {
    case 0x4: sysex_bytes(&p[1], 3, false); break;          /* SysEx start/continue */
    case 0x5:                                                /* SysEx end (1) or 1-byte common */
        if (p[1] >= 0xF8u) realtime(p[1], now); else sysex_bytes(&p[1], 1, true);
        break;
    case 0x6: sysex_bytes(&p[1], 2, true); break;
    case 0x7: sysex_bytes(&p[1], 3, true); break;
    case 0xF: realtime(p[1], now); break;                    /* single byte (real time) */
    case 0x8: note(p[2], false); break;
    case 0x9: note(p[2], p[3] != 0u); break;
    case 0xB:
        if (p[2] == 120u || p[2] == 123u) {                  /* all sound / all notes off */
            memset(tm.notes, 0, sizeof tm.notes); tm.notes_changed++;
        }
        break;
    default: break;
    }
}
void tm_sysex_consumed(void) { tm.sysex_ready = false; tm.sysex_len = 0; }
bool tm_clock_active(uint32_t now) { return tm.have_tick && now - tm.last_tick_ms < TM_CLOCK_TIMEOUT_MS; }
