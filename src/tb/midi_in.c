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
/* 2.4.13: a message for the MIDI map's entries listening to it. */
static void watched(uint8_t src, uint8_t ch, uint8_t num, uint16_t value, bool down) {
    for (uint32_t i = 0; i < TM_WATCH; ++i) {
        const tm_watch_entry *w = &tm.watch[i];
        if (!w->ch || w->src != src || w->num != num || (w->ch <= 16u && w->ch != ch)) continue;
        tm.watch_value[i] = value; tm.watch_changed |= 1u << i;
        if (down && tm.watch_downs[i] < 255u) tm.watch_downs[i]++;
    }
}
void tm_watch_set(uint32_t i, tm_watch_entry w) {
    if (i >= TM_WATCH) return;
    tm_watch_entry *was = &tm.watch[i];
    if (was->src == w.src && was->ch == w.ch && was->num == w.num) return;
    *was = w; tm.watch_value[i] = 0u; tm.watch_downs[i] = 0u; tm.watch_changed &= ~(1u << i);
}
bool tm_watch_take(uint32_t i, uint16_t *value, uint8_t *downs) {
    if (i >= TM_WATCH || !((tm.watch_changed >> i) & 1u)) return false;
    tm.watch_changed &= ~(1u << i);
    *value = tm.watch_value[i]; *downs = tm.watch_downs[i]; tm.watch_downs[i] = 0u;
    return true;
}
static void note(uint8_t status, uint8_t pitch, uint8_t velocity, bool on) {
    count(&tm.notes[pitch & 127u], on, &tm.notes_changed);
    if (tm.lock_channel && tm.lock_channel == (status & 15u) + 1u) {
        tm.lock_vel[pitch & 127u] = on ? (uint8_t)(velocity & 127u) : 0u;
        if (on) tm.lock_strikes[pitch & 127u]++;
        tm.lock_changed++;
    }
    if (tm.follow_channel != tm.follow_listening) { /* another channel: start afresh */
        memset(tm.follow, 0, sizeof tm.follow); tm.follow_listening = tm.follow_channel; tm.follow_changed++;
    }
    uint32_t ch = (status & 15u) + 1u;
    bool lock = tm.lock_channel && tm.lock_channel == ch, follow = tm.follow_channel && (tm.follow_channel > 16u || tm.follow_channel == ch);
    if (!lock && !follow) watched(TM_SRC_NOTE, (uint8_t)ch, (uint8_t)(pitch & 127u), on ? (uint16_t)(velocity & 127u) : 0u, on); /* 2.4.13 */
    /* Only a new note changes the chord: one released note by note never shows its remains. */
    uint32_t releases = 0;
    if (tm.follow_channel && (tm.follow_channel > 16u || tm.follow_channel == ch))
        count(&tm.follow[pitch & 127u], on, on ? &tm.follow_changed : &releases);
}
static void control(uint8_t status, uint8_t cc, uint8_t value) {
    if (tm.cc_channel != tm.cc_listening) { /* another channel: nothing from the last one is waiting */
        memset(tm.cc_changed, 0, sizeof tm.cc_changed); tm.cc_listening = tm.cc_channel;
    }
    uint8_t ch = (uint8_t)((status & 15u) + 1u);
    if (cc < TM_CONTROLS) watched(TM_SRC_CC, ch, cc, (uint16_t)(value & 127u), false); /* 2.4.13: its own channel */
    if (cc >= TM_CONTROLS || !tm.cc_channel || (tm.cc_channel <= 16u && tm.cc_channel != ch)) return;
    tm.cc_value[cc] = value & 127u; tm.cc_changed[cc / 32u] |= 1u << (cc % 32u);
    tm.cc_last = cc; tm.cc_last_value = value & 127u; tm.cc_last_channel = ch; tm.cc_seen++;
}
bool tm_cc_take(uint8_t cc, uint8_t *value) {
    if (cc >= TM_CONTROLS || !(tm.cc_changed[cc / 32u] >> (cc % 32u) & 1u)) return false;
    tm.cc_changed[cc / 32u] &= ~(1u << (cc % 32u));
    *value = tm.cc_value[cc];
    return true;
}
/* 2.4.4 (review TBR-006): a message unfinished for TM_SYSEX_EXPIRE_MS is given up (dropped, counted). A host that
 * stopped in the middle of one (a browser tab closed during a backup, a cable pulled) left it open, and every SysEx
 * message on the other port (the editor's, or music software's) was dropped until a restart. A real message of 193
 * bytes arrives within a few USB frames. */
static void sysex_bytes(const uint8_t *b, uint32_t count, bool end, uint8_t cable, uint32_t now) {
    if (tm.in_sysex && now - tm.sysex_at >= TM_SYSEX_EXPIRE_MS) {
        tm.in_sysex = false; tm.sysex_overflow = false; tm.sysex_dropped++;
        if (!tm.sysex_ready) tm.sysex_len = 0;
    }
    if (tm.in_sysex && cable != tm.in_cable) return; /* 2.0.6: the other port's message waits its turn (dropped) */
    tm.sysex_at = now;
    for (uint32_t i = 0; i < count; ++i) {
        if (b[i] == 0xF0u) {
            if (tm.in_sysex) tm.sysex_dropped++;          /* unterminated previous message */
            tm.in_sysex = true; tm.sysex_overflow = tm.sysex_ready; /* never touch a ready message */
            tm.in_cable = cable;
            if (!tm.sysex_ready) { tm.sysex_len = 0; tm.sysex_cable = cable; }
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
    uint8_t cable = (uint8_t)(p[0] >> 4);
    if (cable >= TM_CABLES) return;
    if (cable == TM_EDITOR_CABLE) { /* 2.0.6: the editor's port carries SysEx only */
        uint8_t cin = p[0] & 15u;
        if (cin == 0x4u) sysex_bytes(&p[1], 3, false, cable, now);
        else if (cin == 0x5u && p[1] < 0xF8u) sysex_bytes(&p[1], 1, true, cable, now);
        else if (cin == 0x6u || cin == 0x7u) sysex_bytes(&p[1], cin - 4u, true, cable, now);
        return;
    }
    switch (p[0] & 15u) {
    case 0x4: sysex_bytes(&p[1], 3, false, 0u, now); break;      /* SysEx start/continue */
    case 0x5:                                                /* SysEx end (1) or 1-byte common */
        if (p[1] >= 0xF8u) realtime(p[1], now); else sysex_bytes(&p[1], 1, true, 0u, now);
        break;
    case 0x6: sysex_bytes(&p[1], 2, true, 0u, now); break;
    case 0x7: sysex_bytes(&p[1], 3, true, 0u, now); break;
    case 0x3:                                                /* three-byte common: Song Position Pointer */
        if (p[1] == 0xF2u) { tm.position = ((uint32_t)(p[2] & 127u) | (uint32_t)(p[3] & 127u) << 7) * 6u; tm.position_next = true; }
        break;
    case 0xF: realtime(p[1], now); break;                    /* single byte (real time) */
    case 0x8: note(p[1], p[2], 0u, false); break;
    case 0x9: note(p[1], p[2], p[3], p[3] != 0u); break;
    case 0xE:                                                /* 2.4.13: pitch bend, for the MIDI map */
        if ((p[1] & 0xF0u) == 0xE0u) watched(TM_SRC_BEND, (uint8_t)((p[1] & 15u) + 1u), 0u, (uint16_t)((p[2] & 127u) | (uint32_t)(p[3] & 127u) << 7), false);
        break;
    case 0xC:                                                /* Program Change (1.7.0: presets, tc_program_in) */
        if ((p[1] & 0xF0u) == 0xC0u) { tm.program = (uint8_t)(p[2] & 127u); tm.program_ch = (uint8_t)((p[1] & 15u) + 1u); tm.programs++; }
        break;
    case 0xB:
        if ((p[1] & 0xF0u) == 0xB0u) control(p[1], p[2] & 127u, p[3]); /* 2.1.0: the MIDI map (controls_cc.c) */
        if (p[2] == 120u || p[2] == 123u) {                  /* all sound / all notes off */
            memset(tm.notes, 0, sizeof tm.notes); tm.notes_changed++;
            memset(tm.follow, 0, sizeof tm.follow); tm.follow_changed++;
            memset(tm.lock_vel, 0, sizeof tm.lock_vel); tm.lock_changed++;
        }
        break;
    default: break;
    }
}
void tm_sysex_consumed(void) { tm.sysex_ready = false; tm.sysex_len = 0; }
/* 2.4.4 (review TBR-026): the computer went away (an unplug shows as a suspend: VBUS sensing is off) or a host
 * mounted the board afresh. What the last host left running, held or half sent goes: a DAW's Start (bare clock from
 * the next host must not play from the old position), its held notes (pad lights), a SysEx message coming in. A
 * complete message waiting to be answered stays. */
void tm_disconnected(void) {
    tm.running = false; tm.have_tick = false; tm.position_next = false;
    memset(tm.notes, 0, sizeof tm.notes); tm.notes_changed++;
    memset(tm.follow, 0, sizeof tm.follow); tm.follow_changed++;
    memset(tm.lock_vel, 0, sizeof tm.lock_vel); tm.lock_changed++;
    if (tm.in_sysex) {
        tm.in_sysex = false; tm.sysex_overflow = false; tm.sysex_dropped++;
        if (!tm.sysex_ready) tm.sysex_len = 0;
    }
}
bool tm_clock_active(uint32_t now) { return tm.have_tick && now - tm.last_tick_ms < TM_CLOCK_TIMEOUT_MS; }
