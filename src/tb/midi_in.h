/* SPDX-License-Identifier: GPL-3.0-or-later - USB-MIDI input: notes for the pad lights, MIDI clock
 * for the arpeggiator, and SysEx messages assembled for sysex.c. */
#ifndef TB_MIDI_IN_H
#define TB_MIDI_IN_H
#include <stdbool.h>
#include <stdint.h>
#define TM_SYSEX_MAX 256u /* a 40-word record needs 192 */
#define TM_CLOCK_TIMEOUT_MS 250u
typedef struct {
    uint8_t notes[128];      /* held incoming notes per pitch, all channels (saturating) */
    uint32_t notes_changed;  /* bumps whenever notes[] changes */
    /* Live notes (0.8.0): held notes on follow_channel (1..16, 17 any, 0 none), for the live
     * chord; follow_changed bumps on each new note there. */
    uint8_t follow[128], follow_channel, follow_listening;
    uint32_t follow_changed;
    uint32_t ticks, last_tick_ms, tick_us, starts; /* clock: count, time, smoothed period, Starts seen */
    bool have_tick;
    uint8_t sysex[TM_SYSEX_MAX]; /* complete message F0..F7 once sysex_ready */
    uint32_t sysex_len, sysex_dropped;
    bool sysex_ready, in_sysex, sysex_overflow;
} tm_state;
extern tm_state tm;
void tm_reset(void);
/* One received USB-MIDI event packet (cable 0 only). A new SysEx message waits
 * (and is dropped, counted) while the previous one is still unconsumed. */
void tm_packet(const uint8_t packet[4], uint32_t now);
void tm_sysex_consumed(void);
/* External clock: ticks seen within TM_CLOCK_TIMEOUT_MS. */
bool tm_clock_active(uint32_t now);
#endif
