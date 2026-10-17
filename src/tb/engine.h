/* SPDX-License-Identifier: GPL-3.0-or-later - pad -> MIDI engine: per-pad note ownership,
 * one USB packet in flight with a receipt token, standalone (re)arming.
 * Stage 2: per-side MIDI channels, strum (timed note starts), chord hold, an
 * arpeggiator (one extra source, TP_ARP, that plays the held chords' notes in
 * turn), and pressure as channel aftertouch, poly aftertouch or a controller.
 * 0.4.0: the sustain pedal as CC64 per side; a sent "on" is owed an "off" like a note. */
#ifndef TB_ENGINE_H
#define TB_ENGINE_H
#include "scan.h"
#include "layout.h"
typedef enum { TK_IDLE, TK_ACTIVE, TK_DRAINING, TK_DONE, TK_STOPPED } tk_state;
/* USB-MIDI code index numbers: note off/on, poly pressure, controller, channel pressure. */
typedef enum { TK_ON = 9, TK_OFF = 8, TK_PRESSURE = 10, TK_CC = 11, TK_CHANNEL = 13 } tk_kind;
typedef struct {
    uint32_t polarity, press, release, full_scale, deadband, pressure_ms;
} tk_config;
/* Pad thresholds measured on the board: 12-bit ADC, higher = pressed harder. */
#define TK_DEFAULT_CONFIG {1u, 2500u, 1500u, 4000u, 4u, 50u}
/* Writer copies all 4 bytes atomically; the caller assigns this exact token to
 * the eventual deferred completion. Called from the main loop only. */
typedef bool (*tk_writer)(const uint8_t packet[4], uint32_t token);
#define TP_SOURCES (TS_KEYS + 1u) /* the 96 pads, then the arpeggiator */
#define TP_ARP TS_KEYS
#define TP_ARP_POOL 32u
typedef struct {
    tp_notes captured;
    uint32_t admitted_at, order;
    uint32_t note_at[TP_MAX_NOTES];      /* poly aftertouch: each note's last pressure (or Note On) */
    uint16_t due[TP_MAX_NOTES];          /* strum: ms after admission before each captured note starts */
    uint8_t note_pressure[TP_MAX_NOTES]; /* poly aftertouch last sent for each note */
    uint8_t pressure, velocity, channel;
    bool sensed, admitted, release_needed, inhibited;
    bool latched; /* hold: released by the hand, still sounding */
    bool silent;  /* arpeggiator input: owns no notes, the arpeggiator plays them */
} tp_key;
typedef struct { uint8_t refs; bool exposed; } tp_voice;
typedef struct {
    tp_key keys[TP_SOURCES];
    tp_voice voices[16][128]; /* [MIDI channel][pitch] */
    uint16_t exposed[16];     /* exposed voices per channel */
    tk_config config;
    tp_layout layout;
    tk_state state;
    uint32_t frame, frame_at, started, pending_at, wait_at;
    uint32_t issued, pending_id, consumed_arm, accepted, completed, errors;
    uint32_t group_at[2], admissions;
    uint8_t pending_pitch, pending_key, pending_pressure, pending_channel, pending_group, cursor;
    uint8_t group_sent[2]; /* channel pressure / controller value per side (0 chords, 1 melody) */
    uint8_t sustain_ch[2]; /* the channel each side's sustain "on" went to */
    bool sustain_on[2];
    tk_kind pending_kind;
    bool uncertain, waiting, strum_down;
    /* Arpeggiator: step counter, timing in microseconds (wrapping), clock position. */
    uint32_t arp_step, arp_due_us, arp_off_us, arp_ticks, arp_starts, arp_random;
    bool arp_running, arp_clocked;
} tp_engine;
bool tk_config_ok(tk_config c);
/* Arm with the current controls layout. Pads not released at that moment are inhibited:
 * silent until they read released once (a stuck pad must not silence the board). */
bool tp_arm(tp_engine *e, tk_config c, const ts_scan *s, uint32_t request, uint32_t now);
bool tp_frame(tp_engine *e, const ts_scan *s, uint32_t now);
void tp_disable(tp_engine *e);
void tp_loss(tp_engine *e);
void tp_complete(tp_engine *e, uint32_t token, uint32_t bytes, bool success, uint32_t now);
void tp_step(tp_engine *e, uint32_t now, bool ready, tk_writer writer);
uint32_t tp_obligations(const tp_engine *e);
/* The arpeggiator's current input notes in play order (diagnostics/tests). */
uint32_t tp_arp_pool(const tp_engine *e, uint8_t pool[TP_ARP_POOL]);
/* Standalone policy: arm on a fresh frame once USB is ready; after a drain or
 * stop, start a fresh session 500 ms later. tc_frame also tracks the last chord. */
bool tc_arm(tp_engine *e, const ts_scan *s, uint32_t now, bool ready);
bool tc_frame(tp_engine *e, const ts_scan *s, uint32_t now);
void tc_step(tp_engine *e, uint32_t now, bool ready, tk_writer writer);
#endif
