/* SPDX-License-Identifier: GPL-3.0-or-later - USB-MIDI 1.0 event packets (USB Device Class
 * Definition for MIDI Devices 1.0, table 4-1): byte 0 = cable << 4 | code index. */
#include "midi_in.h"
#include <string.h>
tm_state tm;
void tm_reset(void) { memset(&tm, 0, sizeof tm); }
static void count(uint8_t *n, bool on, uint32_t *changed) {
    if (on && *n < 255u) { (*n)++; (*changed)++; }
    else if (!on && *n) { (*n)--; (*changed)++; }
}
static void note(uint8_t status, uint8_t pitch, bool on) {
    count(&tm.notes[pitch & 127u], on, &tm.notes_changed);
    if (tm.follow_channel != tm.follow_listening) { /* another channel: start afresh */
        memset(tm.follow, 0, sizeof tm.follow); tm.follow_listening = tm.follow_channel; tm.follow_changed++;
    }
    uint32_t ch = (status & 15u) + 1u;
    /* Only a new note changes the chord: one released note by note never shows its remains. */
    uint32_t releases = 0;
    if (tm.follow_channel && (tm.follow_channel > 16u || tm.follow_channel == ch))
        count(&tm.follow[pitch & 127u], on, on ? &tm.follow_changed : &releases);
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
        if (tm.running) { if (tm.position_next) tm.position_next = false; else tm.position++; }
    } else if (status == 0xFAu) {
        tm.starts++; tm.ticks = 0; tm.running = true; tm.position = 0u; tm.position_next = true;
    }
    else if (status == 0xFBu) tm.running = true;  /* Continue */
    else if (status == 0xFCu) tm.running = false; /* Stop */
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
    case 0x3:                                                /* three-byte common: Song Position Pointer */
        if (p[1] == 0xF2u) { tm.position = ((uint32_t)(p[2] & 127u) | (uint32_t)(p[3] & 127u) << 7) * 6u; tm.position_next = true; }
        break;
    case 0xF: realtime(p[1], now); break;                    /* single byte (real time) */
    case 0x8: note(p[1], p[2], false); break;
    case 0x9: note(p[1], p[2], p[3] != 0u); break;
    case 0xB:
        if (p[2] == 120u || p[2] == 123u) {                  /* all sound / all notes off */
            memset(tm.notes, 0, sizeof tm.notes); tm.notes_changed++;
            memset(tm.follow, 0, sizeof tm.follow); tm.follow_changed++;
        }
        break;
    default: break;
    }
}
void tm_sysex_consumed(void) { tm.sysex_ready = false; tm.sysex_len = 0; }
bool tm_clock_active(uint32_t now) { return tm.have_tick && now - tm.last_tick_ms < TM_CLOCK_TIMEOUT_MS; }
