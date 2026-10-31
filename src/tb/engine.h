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
#include "sequence.h"
typedef enum { TK_IDLE, TK_ACTIVE, TK_DRAINING, TK_DONE, TK_STOPPED } tk_state;
/* USB-MIDI code index numbers: note off/on, poly pressure, controller, program change (1.7.0),
 * channel pressure, pitch bend, single byte (MIDI clock, Start, Stop). */
typedef enum { TK_ON = 9, TK_OFF = 8, TK_PRESSURE = 10, TK_CC = 11, TK_PROGRAM = 12, TK_CHANNEL = 13, TK_BEND = 14,
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
#define TP_TURING 16u /* the Turing pattern's loop, at most (controls.h TC_ARP_LOOPS) */
typedef struct {
    tp_notes captured;
    uint32_t admitted_at, order;
    uint32_t note_at[TP_MAX_NOTES];      /* poly aftertouch: each note's last pressure (or Note On) */
    uint16_t due[TP_MAX_NOTES];          /* strum: ms after admission before each captured note starts */
    int8_t feel[TP_MAX_NOTES];           /* strum feel: velocity offset of each captured note (0.7.0) */
    uint8_t note_pressure[TP_MAX_NOTES]; /* poly aftertouch last sent for each note */
    uint8_t pressure, velocity, channel;
    uint8_t at; /* 1.9.0: the pressure that goes out (aftertouch, MPE): pressure through its half's curve */
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
    uint16_t peak_raw, raw; /* 1.8.3: the peak's reading and the reading pressure comes from (velocity) */
    uint16_t settle_ref; /* Unstick: the reading below Trigger the touch has stayed near since settle_at */
    uint32_t settle_at;
    uint32_t low_at;
    bool hidden; /* Mono (1.6.0): a melody pad still held under a newer one: silent, owns no notes */
    /* Glide (1.8.0): a melody pad gliding on a held one: that pad's index + 1 (0 none; it owns no notes).
     * glide_pitch: the pad's own note; glide_offset: semitones from the note its channel sounds. */
    uint8_t glide, glide_pitch;
    int8_t glide_offset;
    /* Glide Time (1.8.5): the bend (from the channel's note, in bend units) this pad's channel moves from
     * and to, starting at glide_at. */
    int16_t glide_from, glide_to;
    uint32_t glide_at;
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
    /* The Turing pattern (2.0.0): each loop step's random number (the note: that modulo the held notes), the held
     * notes it was drawn for (a hash), and the number of the step to sound. */
    uint8_t turing[TP_TURING], turing_pick;
    uint32_t turing_notes;
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
    /* 2.0.0: the chain's entry playing (sequence.h sq_chain; with Chain off 0) and the progression slot it plays
     * (with Chain off the selected one), and whether the step playing sounds as recorded, is a rest or a borrowed
     * chord (its chance; SQ_PLAYS ..). */
    uint8_t seq_link, seq_slot, seq_plays;
    uint32_t seq_chain_changes;
    bool seq_chained;
    /* The step sequencer (1.5.0): the clock position (rhythm.h units) the pattern started at and
     * where it is now (with a DAW, its song position), each lane's steps started (lane_count), the
     * step it plays (lane_step, LN_NONE while stopped), and a step still to sound (pending: an owed
     * Note Off first). Carried into a new session, as the progression. 1.7.0: whether the step plays
     * this time round (lane_sounds: its condition and probability), its ratchet strike, and a tied
     * note held over the steps after it until one plays (lane_tied). */
    /* Each pad's own resting level (1.5.0; 1.7.0: as read, rested once measured; before that the
     * thresholds as set). Measured when
     * the session starts (tc_arm), then following the pad while it rests: down within a few frames,
     * up slowly (a board flexing as it is moved), never from a press. Indexed by physical pad, so
     * trading the halves (Chords on) keeps each sensor's own level. */
    uint16_t rest[TS_KEYS];
    bool rested;
    uint32_t lane_origin, lane_pos, lane_count[LN_LANES];
    uint8_t lane_step[LN_LANES], lane_strike[LN_LANES], lane_tie_step[LN_LANES]; /* the tied step, from count lane_tie_count */
    uint32_t lane_tie_count[LN_LANES];
    bool lane_running, lane_clocked, lane_daw, lane_pending[LN_LANES], lane_sounds[LN_LANES], lane_tied[LN_LANES];
    /* 2.0.3, Stutter: while a Stutter button is held the lanes and the progression play from the window the press
     * fell in, round and round (from, the last position played there); positions in each one's own units. */
    bool lane_stutter, seq_stutter;
    uint32_t lane_stutter_from, lane_stutter_last, seq_stutter_from, seq_stutter_last;
    tp_echo echo[TP_ECHOES]; /* echo (1.6.0): one per source TP_ECHO0.. */
    /* Mono (1.6.0): the notes of the newest melody pad, lifted while an older one is held, kept
     * sounding until the older one's have started again (legato back too). */
    tp_notes mono_hold; uint8_t mono_hold_ch;
    /* Program Change (1.7.0): the program (1..128, 0 none) and channel sent this session, per half;
     * whether they went again TP_PROGRAM_LATE_MS into it; the DAW Starts (tm.starts) seen. */
    uint8_t program_sent[2], program_ch[2];
    bool program_late;
    uint32_t program_starts;
    /* MPE (1.8.0): each channel's pressure, controller and bend (from the centre) as last sent and when,
     * the start count (mpe_uses) when a source last started on it; per zone (0 lower, 1 upper) the member
     * count its configuration message last said, the count going out and that message's step. */
    uint8_t mpe_z[16], mpe_cc[16], mpe_ccn[16]; /* mpe_ccn: the controller mpe_cc went on */
    int16_t mpe_bend[16];
    uint32_t mpe_at[16], mpe_cc_at[16], mpe_bend_at[16], mpe_used[16], mpe_uses; /* each kept apart: pressure and its
                                                                                  * controller go out together */
    uint8_t mpe_sent[2], mpe_count[2], mpe_step[2], mpe_cursor;
} tp_engine;
/* MPE (1.8.0): candidate groups beyond 0/1 (pressure per half) and 2/3 (sustain per half): 4/5 a zone's
 * configuration message, TP_GROUP_MPE + channel a member channel's own pressure, controller or bend. */
#define TP_GROUP_MCM 4u
#define TP_GROUP_MPE 16u
#define TP_MPE_MEMBERS 7u
#define TP_MPE_BEND_MS 10u     /* a bend moves at most every 10 ms (glides want it smooth) ... */
#define TP_MPE_BEND_DEADBAND 16 /* ... by 16 of 8192 at least: under 0.1 semitone at the 48 MPE pads bend */
#define TP_MPE_BEND_SETTLE_MS 40u /* 1.8.5: ... and a smaller difference this long after the last: a glide lands exactly */
#define TP_MPE_BEND_RANGE 48   /* semitones of a member channel's full bend (the MPE default) */
#define TP_GLIDE_BAND 64       /* 1.8.4: a glide is all the way on the harder pad's note this much harder (of 127) */
#define TP_GLIDE_FLOOR 8       /* 1.8.5, Glide Pad: pressure up to this (of 127) past the pad's Trigger doesn't bend */
/* A session starts as the computer finds the board, before a DAW or monitor opens its port: the
 * programs go again this long after. */
#define TP_PROGRAM_LATE_MS 3000u
#define LN_NONE 0xFFu
bool tk_config_ok(tk_config c);
/* Pad k's thresholds: the session's above the pad's own resting level (tc_pad_raise, 1.7.0). */
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
/* 2.0.3: a lane's notes moved by its transpose: a chord lane's by scale steps of layout (with the step's own scale,
 * tc_harmony_note), the others' by semitones; notes out of MIDI range left out, each note once. */
void tc_lane_transpose(tp_layout layout, const ln_lane *l, const ln_step *s, tp_notes *notes);
/* 2.0.3, Stutter: pos looped round the window of `window` units it was in when the loop began (*from, *last kept
 * by the caller; *on whether it is looping); true when the loop came round again (play the step again). */
bool tp_stutter(uint32_t pos, uint32_t window, bool *on, uint32_t *from, uint32_t *last, uint32_t *looped);
/* 2.0.0: the progression playing (or last played): the chain's link e->seq_link while it plays, else the
 * selected one (sequence.h). */
const sq_prog *tp_seq_prog(const tp_engine *e);
uint32_t tp_seq_pos(const tp_engine *e); /* clocks into it (a chain: into the link playing) */
/* The layout step en of p plays with, as it plays this time round (SQ_PLAYS, SQ_BORROWS: the parallel key's). */
tp_layout tp_seq_layout(const sq_prog *p, tp_layout current, const sq_entry *en, uint8_t plays);
/* The arpeggiator's current input notes in play order (diagnostics/tests). */
uint32_t tp_arp_pool(const tp_engine *e, uint8_t pool[TP_ARP_POOL]);
/* Standalone policy: arm on a fresh frame once USB is ready; after a drain or
 * stop, start a fresh session 500 ms later. tc_frame (every pass: tp_frame) also tracks the
 * last chord. */
bool tc_arm(tp_engine *e, const ts_scan *s, uint32_t now, bool ready);
bool tc_frame(tp_engine *e, const ts_scan *s, uint32_t now);
void tc_step(tp_engine *e, uint32_t now, bool ready, tk_writer writer);
#endif
