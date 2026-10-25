/* SPDX-License-Identifier: GPL-3.0-or-later - pad -> MIDI engine: per-pad note ownership,
 * one USB packet in flight with a receipt token, standalone (re)arming.
 * Stage 2: per-side MIDI channels, strum (timed note starts), chord hold, an
 * arpeggiator (one extra source, TP_ARP, that plays the held chords' notes in
 * turn), and pressure as channel aftertouch, poly aftertouch or a controller.
 * 0.4.0: the sustain pedal as CC64 per side; a sent "on" is owed an "off" like a note.
 * 0.6.0: strum repeat and cutoff, arpeggiator rhythm/swing/sync/chords/latch on a
 * 24-per-beat clock (MIDI clock in, or the tempo), pitch bend from pressure, the pedal's
 * controller choice and note (one more source, TP_PEDAL), MIDI clock out and transport,
 * live update of held pads after a layout change.
 * 1.5.0: the step sequencer's lanes (lanes.h), one source each (TP_LANE0..), playing with the
 * transport or a DAW alongside the progression. */
#ifndef TB_ENGINE_H
#define TB_ENGINE_H
#include "scan.h"
#include "layout.h"
#include "lanes.h"
typedef enum { TK_IDLE, TK_ACTIVE, TK_DRAINING, TK_DONE, TK_STOPPED } tk_state;
/* USB-MIDI code index numbers: note off/on, poly pressure, controller, channel pressure,
 * pitch bend, single byte (MIDI clock, Start, Stop). */
typedef enum { TK_ON = 9, TK_OFF = 8, TK_PRESSURE = 10, TK_CC = 11, TK_CHANNEL = 13, TK_BEND = 14,
               TK_CLOCK = 15 } tk_kind;
typedef struct {
    uint32_t polarity, press, release, full_scale, deadband, pressure_ms;
} tk_config;
/* Pad thresholds: 12-bit ADC, higher = pressed harder. The defaults of the Pads settings
 * (0.10.0; before, fixed at 2500/1500/4000); the engine plays with tc_pad_config. */
#define TK_DEFAULT_CONFIG {1u, 1200u, 600u, 3500u, 4u, 50u}
/* Writer copies all 4 bytes atomically; the caller assigns this exact token to
 * the eventual deferred completion. Called from the main loop only. */
typedef bool (*tk_writer)(const uint8_t packet[4], uint32_t token);
/* The 96 pads, the arpeggiator, the pedal's note, the progression, the scale lock's notes, the lanes,
 * the echoes. */
#define TP_ARP TS_KEYS
#define TP_PEDAL (TS_KEYS + 1u)
#define TP_SEQ (TS_KEYS + 2u) /* the progression's current chord (0.11.0, sequence.h) */
#define TP_LOCK (TS_KEYS + 3u) /* scale lock (1.0.0): one source per note arriving, up to TP_LOCKS */
#define TP_LOCKS 16u
#define TP_LANE0 (TP_LOCK + TP_LOCKS) /* the step sequencer's lanes (1.5.0): one source per lane */
#define TP_ECHO0 (TP_LANE0 + LN_LANES) /* echo (1.6.0): one source per echoing strike, up to TP_ECHOES */
#define TP_ECHOES 8u
#define TP_SOURCES (TP_ECHO0 + TP_ECHOES)
#define TP_ARP_POOL 32u
typedef struct {
    tp_notes captured;
    uint32_t admitted_at, order;
    uint32_t note_at[TP_MAX_NOTES];      /* poly aftertouch: each note's last pressure (or Note On) */
    uint16_t due[TP_MAX_NOTES];          /* strum: ms after admission before each captured note starts */
    int8_t feel[TP_MAX_NOTES];           /* strum feel: velocity offset of each captured note (0.7.0) */
    uint8_t note_pressure[TP_MAX_NOTES]; /* poly aftertouch last sent for each note */
    uint8_t pressure, velocity, channel;
    uint8_t input, strike; /* scale lock: the note that arrived, and which strike of it */
    bool sensed, admitted, release_needed, inhibited;
    bool latched; /* hold: released by the hand, still sounding */
    bool silent;  /* arpeggiator input: owns no notes, the arpeggiator plays them */
    bool readmit; /* strike again right after this release (strum repeat, live update) */
    bool cut;     /* ended by the strum cutoff while still touched */
    /* Note repeat (1.4.0, on the clock's grid): repeating while held; the step of the next strike
     * and the clock position (rhythm.h) where the sounding strike ends. */
    bool repeating;
    uint32_t rep_next, rep_off;
    /* Faster pads (1.5.0): a touch first read within TC_PAD_VELOCITY_MS of low_at (the pad's last
     * reading below Trigger) is rising: sensed at the next reading, with the higher pressure (peak). */
    bool rising;
    uint8_t peak;
    uint32_t low_at;
    bool hidden; /* Mono (1.6.0): a melody pad still held under a newer one: silent, owns no notes */
} tp_key;
typedef struct { uint8_t refs; bool exposed; } tp_voice;
/* Echo (1.6.0): a strike's notes, channel and velocity, how many echoes in all and still to come, the
 * echo time and the clock positions (rhythm.h) of the next echo and the end of the sounding one, and
 * the strike's order (the oldest gives way when all are busy). */
typedef struct {
    tp_notes notes;
    uint32_t len, next, off, order;
    uint8_t channel, velocity, total, left;
} tp_echo;
typedef struct {
    tp_key keys[TP_SOURCES];
    tp_voice voices[16][128]; /* [MIDI channel][pitch] */
    uint16_t exposed[16];     /* exposed voices per channel */
    tk_config config;
    tp_layout layout;
    tk_state state;
    uint32_t frame, frame_at, started, pending_at, wait_at;
    uint32_t reads;                   /* the scanner's position reads taken (1.5.0, ts_scan.reads) */
    uint32_t rest_at[TS_POSITIONS];   /* when each position's resting levels last followed */
    uint32_t issued, pending_id, consumed_arm, accepted, completed, errors;
    uint32_t group_at[2], admissions;
    uint8_t pending_pitch, pending_key, pending_pressure, pending_channel, pending_group, cursor;
    uint8_t group_sent[2]; /* channel pressure / controller / bend value per side (0 chords, 1 melody) */
    uint8_t sustain_ch[2], sustain_cc[2]; /* where each side's pedal controller "on" went */
    bool sustain_on[2];
    tk_kind pending_kind;
    bool uncertain, waiting, strum_down;
    uint32_t random; /* strum feel: xorshift32 state (never 0) */
    /* Arpeggiator: steps started, the clock position (rhythm.h) its steps count from, where the next
     * step starts and the sounding one ends, the rate and swing the origin was set for (1.4.0). */
    uint32_t arp_step, arp_origin, arp_next_at, arp_off_at, arp_starts, arp_random, arp_play;
    uint8_t arp_rate_at, arp_swing_at;
    bool arp_running, arp_clocked, arp_waiting, arp_pending; /* pending: step arp_play still to sound */
    /* The board's own 24-per-beat clock from the tempo: next tick time, tick count; MIDI
     * clock bytes owed, transport state sent. */
    uint32_t clock_us, clock_ticks, clock_due;
    bool clock_started, transport_sent;
    bool transport_paused; /* a drain stopped the running transport: the next Start is a Continue */
    /* The progression (0.11.0): where it is (clocks from its first chord; with a DAW its song
     * position), the clock count its first chord fell on, the step being played (seq_step
     * counts every step played), DAW Starts and sq changes seen. Carried into a new session. */
    uint32_t seq_origin, seq_pos, seq_step, seq_starts, seq_changes;
    uint8_t seq_entry;
    bool seq_running, seq_pending, seq_clocked, seq_daw;
    /* The step sequencer (1.5.0): the clock position (rhythm.h units) the pattern started at and
     * where it is now (with a DAW, its song position), each lane's steps started (lane_count), the
     * step it plays (lane_step, LN_NONE while stopped), where its gate ends, and a step still to
     * sound (pending: an owed Note Off first). Carried into a new session, as the progression. */
    /* Each pad's own resting level (1.5.0; 0 = none measured: the thresholds as set). Measured when
     * the session starts (tc_arm), then following the pad while it rests: down within a few frames,
     * up slowly (a board flexing as it is moved), never from a press. Indexed by physical pad, so
     * trading the halves (Chords on) keeps each sensor's own level. */
    uint16_t rest[TS_KEYS];
    uint32_t lane_origin, lane_pos, lane_count[LN_LANES], lane_off[LN_LANES];
    uint8_t lane_step[LN_LANES];
    bool lane_running, lane_clocked, lane_daw, lane_pending[LN_LANES];
    tp_echo echo[TP_ECHOES]; /* echo (1.6.0): one per source TP_ECHO0.. */
    /* Mono (1.6.0): the notes of the newest melody pad, lifted while an older one is held, kept
     * sounding until the older one's have started again (legato back too). */
    tp_notes mono_hold; uint8_t mono_hold_ch;
} tp_engine;
#define LN_NONE 0xFFu
bool tk_config_ok(tk_config c);
/* Pad k's thresholds: the session's, raised for the pad's own resting level (tc_pad_raise). */
tk_config tp_key_config(const tp_engine *e, uint32_t k);
/* The lowest and highest resting level of the pads; false before any was measured. */
bool tp_rest_range(const tp_engine *e, uint32_t *lo, uint32_t *hi);
/* Arm with the current controls layout. Pads not released at that moment are inhibited:
 * silent until they read released once (a stuck pad must not silence the board). */
bool tp_arm(tp_engine *e, tk_config c, const ts_scan *s, uint32_t request, uint32_t now);
/* The pads read since the last call (1.5.0): the 6 pads of each position read since (up to 15
 * positions), else (a new frame without position reads, or more) all 96. True when pads were read. */
bool tp_frame(tp_engine *e, const ts_scan *s, uint32_t now);
void tp_disable(tp_engine *e);
void tp_loss(tp_engine *e);
void tp_complete(tp_engine *e, uint32_t token, uint32_t bytes, bool success, uint32_t now);
void tp_step(tp_engine *e, uint32_t now, bool ready, tk_writer writer);
uint32_t tp_obligations(const tp_engine *e);
/* The notes and MIDI channel of a lane step as it plays now in this layout: a chord pad as
 * progressions play one (tc_pad_with: panel, voice leading), a melody step as that position of the
 * right half's Scale layout, a drum step its note. False (no notes) for a value the kind has no pad
 * for or an off lane. */
bool tc_step_notes(tp_layout layout, uint8_t kind, uint8_t channel, const ln_step *s, tp_notes *out, uint8_t *ch);
/* The arpeggiator's current input notes in play order (diagnostics/tests). */
uint32_t tp_arp_pool(const tp_engine *e, uint8_t pool[TP_ARP_POOL]);
/* Standalone policy: arm on a fresh frame once USB is ready; after a drain or
 * stop, start a fresh session 500 ms later. tc_frame (every pass: tp_frame) also tracks the
 * last chord. */
bool tc_arm(tp_engine *e, const ts_scan *s, uint32_t now, bool ready);
bool tc_frame(tp_engine *e, const ts_scan *s, uint32_t now);
void tc_step(tp_engine *e, uint32_t now, bool ready, tk_writer writer);
#endif
