/* SPDX-License-Identifier: GPL-3.0-or-later - USB-MIDI input: notes for the pad lights, MIDI clock
 * for the arpeggiator, Program Change for the presets, controllers for the MIDI map (2.1.0), and SysEx
 * messages assembled for sysex.c. */
#ifndef TB_MIDI_IN_H
#define TB_MIDI_IN_H
#include <stdbool.h>
#include <stdint.h>
#define TM_SYSEX_MAX 256u /* a 40-word record needs 192 */
#define TM_CLOCK_TIMEOUT_MS 250u
#define TM_SYSEX_EXPIRE_MS 1000u /* 2.4.4 (review TBR-006): a SysEx message unfinished this long is given up */
#define TM_CONTROLS 120u /* controllers 0..119; 120..127 are channel mode messages, never mapped */
#define TM_CC_ANY 17u    /* cc_channel: any channel */
/* 2.4.13: the MIDI map's sources (controls_cc.c): a controller, a note or pitch bend on a channel. */
enum { TM_SRC_CC, TM_SRC_NOTE, TM_SRC_BEND, TM_SRCS };
#define TM_WATCH 32u
typedef struct { uint8_t src, ch, num; } tm_watch_entry; /* ch 1..16 or TM_CC_ANY; 0: unused */
/* 2.0.6: two USB MIDI ports (cables). Cable 0 is the board's port for music software; cable 1,
 * "TheoryBoard Editor", is the editor's: SysEx only, and the reply goes back on it, so music
 * software listening to cable 0 never sees the editor's traffic. */
#define TM_CABLES 2u
#define TM_EDITOR_CABLE 1u
typedef struct {
    uint8_t notes[128];      /* held incoming notes per pitch, all channels (saturating); 2.5.5: not the MIDI map's */
    uint32_t notes_changed;  /* bumps whenever notes[] changes */
    /* Live notes (0.8.0): held notes on follow_channel (1..16, 17 any, 0 none), for the live
     * chord; follow_changed bumps on each new note there. */
    uint8_t follow[128], follow_channel, follow_listening;
    uint32_t follow_changed;
    /* Scale lock (1.0.0): notes held on lock_channel (1..16, 0 none; the engine sets it), each
     * with its velocity (0 = not held) and a count of its strikes; lock_changed bumps on each. */
    uint8_t lock_channel, lock_vel[128], lock_strikes[128];
    uint32_t lock_changed;
    uint32_t ticks, last_tick_ms, tick_us, starts; /* clock: count, time, smoothed period, Starts seen */
    bool have_tick;
    bool running; /* a DAW's Start or Continue, until its Stop (0.11.0: progressions play while it runs) */
    /* The DAW's song position in clocks (0.11.0, for progressions): 0 at Start, set by a Song
     * Position Pointer, one more per clock while running. position_next: the next clock is
     * at position itself (after Start or a Song Position Pointer), not one past it. */
    uint32_t position;
    bool position_next;
    /* Program Change (1.7.0): the last program (0..127) and its channel (1..16); programs counts them. */
    uint8_t program, program_ch;
    uint32_t programs;
    /* External control (2.1.0, controls_cc.c): controllers 0..119 on cc_channel (1..16, TM_CC_ANY, 0 none; the
     * controls set it): the newest value of each, a bit per controller arrived since tm_cc_take, and the last
     * one to arrive with its value and channel (MIDI learn), cc_seen bumping on each. */
    uint8_t cc_channel, cc_listening, cc_value[TM_CONTROLS], cc_last, cc_last_value, cc_last_channel;
    uint32_t cc_changed[(TM_CONTROLS + 31u) / 32u], cc_seen;
    /* 2.4.13: what the MIDI map's slots listen to (controls_cc.c sets watch, slot i entry i): each one's newest value (a
     * CC's 0..127, a note's velocity while it sounds and 0 once it ends, pitch bend 0..16383), the Note Ons since
     * tm_watch_take (watch_downs), a bit per entry that arrived since. Notes on the Scale lock or Live in channel are
     * theirs, never the map's. 2.5.5: a note an entry takes is not in notes[] (it is a control, not a note playing: no
     * pad lights, not in a captured chord); mapped has a bit per pitch whose start was kept out that way. */
    tm_watch_entry watch[TM_WATCH];
    uint32_t mapped[4];
    uint16_t watch_value[TM_WATCH];
    uint8_t watch_downs[TM_WATCH];
    uint32_t watch_changed;
    uint8_t sysex[TM_SYSEX_MAX]; /* complete message F0..F7 once sysex_ready */
    uint32_t sysex_len, sysex_dropped;
    uint32_t sysex_at; /* 2.4.4: when the message coming in last took a byte (TM_SYSEX_EXPIRE_MS) */
    bool sysex_ready, in_sysex, sysex_overflow;
    uint8_t sysex_cable, in_cable; /* 2.0.6: the cable of sysex[] (answer on it), and of the message coming in */
} tm_state;
extern tm_state tm;
void tm_reset(void);
/* One received USB-MIDI event packet (cable 0; SysEx on cable 1 too). A new SysEx message waits
 * (and is dropped, counted) while the previous one is still unconsumed. */
void tm_packet(const uint8_t packet[4], uint32_t now);
void tm_sysex_consumed(void);
/* 2.4.4: a USB mount or suspend (main.c): the transport stopped, held notes and a message coming in forgotten. */
void tm_disconnected(void);
/* 2.1.0: whether controller cc arrived since the last call for it (and clears that), its newest value in *value. */
bool tm_cc_take(uint8_t cc, uint8_t *value);
/* 2.4.13: entry i set to listen to w (a change forgets what the old one had waiting); whether it arrived since the last
 * take (and clears that), with its newest value and the Note Ons meanwhile. */
void tm_watch_set(uint32_t i, tm_watch_entry w);
bool tm_watch_take(uint32_t i, uint16_t *value, uint8_t *downs);
/* External clock: ticks seen within TM_CLOCK_TIMEOUT_MS. */
bool tm_clock_active(uint32_t now);
#endif
