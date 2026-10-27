/* SPDX-License-Identifier: GPL-3.0-or-later - capacitive controls: decode, debounce, dispatch, publish. */
#ifndef TC_CONTROLS_H
#define TC_CONTROLS_H
#include "i2c.h"
#include "engine.h"
#include "rhythm.h"
#define TC_MAGIC 0x54423068u
#define TC_WORDS 256u
#define TC_POLL_MS 10u
#define TC_DEBOUNCE_MS 20u
enum { TC_NAV=1, TC_FUNCTION=2, TC_ROOT=3, TC_SCALE=4 };
enum tc_action { TC_NONE, TC_TONIC, TC_MODE, TC_CANCEL, TC_PREVIOUS,
    TC_COMMIT, TC_NEXT, TC_MENU, TC_LOWER_TONE,
    TC_TODO_FUNCTION, TC_TODO_CONTEXT, TC_CONFLICT, TC_REJECT,
    TC_OCTAVE, TC_CHORD_EDIT, TC_SETTING, TC_HOLD, TC_ARP, TC_PRESET_LOADED,
    TC_PRESET_SAVED, TC_PRESET_EMPTY, TC_REMOTE, TC_PANEL, TC_TRANSPORT, TC_RESET,
    TC_SCALE_STEP, TC_TEMPO_STEP, TC_FAVOURITE_STEP,
    /* 0.11.0: progressions */
    TC_SEQ_RECORD, TC_SEQ_TAP, TC_SEQ_FULL, TC_SEQ_SAVED, TC_SEQ_CLEARED, TC_SEQ_EMPTY, TC_SEQ_UNDO,
    TC_SEQ_KEPT, TC_SAVE_FAILED,
    /* 1.0.0: centre-button actions, Shift, chord sets */
    TC_BUTTON, TC_PAD_CHOSEN, TC_SET_NO_EDITS, TC_SET_EMPTY, TC_SHIFT_UNUSED, TC_SEQ_BUSY,
    /* 1.4.0: pivot chords, chord capture */
    TC_PIVOT_SET, TC_PIVOT_ARRIVED, TC_CAPTURE_START, TC_CAPTURE_ADDED, TC_CAPTURE_UNDO, TC_CAPTURE_FULL,
    TC_CAPTURE_SAVED, TC_CAPTURE_EMPTY,
    /* 1.4.0: resets, each confirmed first */
    TC_OPTIONS_RESET, TC_FACTORY_RESET, TC_RESET_KEPT,
    /* 1.5.0: the step screen: a step on/off, a step's value set while held, a value picked (no step
     * held), step record on, a step filled, a rest, the last one taken back, step record off, another
     * lane, a step past the lane's length */
    TC_STEP_ON, TC_STEP_OFF, TC_STEP_SET, TC_STEP_PICKED, TC_STEP_RECORD, TC_STEP_FILLED, TC_STEP_REST,
    TC_STEP_BACK, TC_STEP_STOPPED, TC_STEP_LANE, TC_STEP_PAST,
    /* 1.7.0: a held step's field chosen or changed, a lane filled by Euclid, the pads' resting levels
     * measured again */
    TC_STEP_FIELD, TC_EUCLID_FILLED, TC_PADS_RESCANNED };
/* UI: play screen, a settings page's items, editing one item's value, the list of pages, and (1.5.0)
 * the step screen (Settings > Sequence > Steps): the right half's pads are the selected lane's
 * steps, 12 to a row in reading order (1-12 on the top row .. 37-48 on the bottom; page 2: 49-64),
 * the left half picks what a step plays (chord pads; a melody lane: the right half's Scale layout;
 * drums: notes 36.. up each column). Tap a step: on (with the value picked last) or off. Hold a step
 * and touch a left-half pad: that step plays it. Up/down: the lane; left/right: the page (left on
 * the first page goes back); centre: step record (each left-half pad fills the step at the cursor
 * and moves on, right enters a rest, left takes the last one back). */
enum { TC_UI_PLAY, TC_UI_MENU, TC_UI_EDIT, TC_UI_GROUPS, TC_UI_STEPS };
/* 1.7.0: what left/right change on the held steps (up/down choose it): velocity, gate, tie,
 * probability, ratchet, condition (lanes.h). */
enum { TC_FIELD_VELOCITY, TC_FIELD_GATE, TC_FIELD_TIE, TC_FIELD_PROBABILITY, TC_FIELD_RATCHET,
       TC_FIELD_CONDITION, TC_FIELDS };
/* Navigation buttons below the screen, left to right. Left goes back one level (home
 * only from the page list), up/down move or change a value, centre enters (on the play
 * screen: opens the settings), right goes deeper. Holding left and pressing centre goes
 * straight home. Playing: up/down do what Settings > Play > Arrows says (next/previous
 * scale, melody octave, preset or tempo), hold left + up/down step through presets,
 * right toggles hold. */
enum { TC_NAV_LEFT, TC_NAV_UP, TC_NAV_ENTER, TC_NAV_DOWN, TC_NAV_RIGHT };
/* Centre buttons (the chord control panel), in the order of the touch-controller map:
 * the top row's four Voicing buttons (I, II, III, guitar), the three Spread buttons
 * (I, II, III), then the Octave "V" buttons -3 to +3 (checked on a board, 0.6.1). */
enum { TC_FN_VOICING1, TC_FN_VOICING2, TC_FN_VOICING3, TC_FN_GUITAR, TC_FN_SPREAD1, TC_FN_SPREAD2,
       TC_FN_SPREAD3, TC_FN_OCT_M3, TC_FN_OCT_M2, TC_FN_OCT_M1, TC_FN_OCT_0, TC_FN_OCT_P1,
       TC_FN_OCT_P2, TC_FN_OCT_P3, TC_FUNCTIONS };
/* What a centre button does (1.0.0, Settings > Buttons or the web editor). Button n does
 * action n by default (its printed panel function). Toggles light while on; Shift while held
 * or latched; Sustain while on (a toggle: the touch controllers report a button held for
 * about 5 s as released); the "while held" chord shapes and the one-shot actions while held.
 * Shift (held, or latched by a double tap; a tap unlatches): Shift + another centre button =
 * that button's printed panel function; Shift + root button 1..8 = play progression P1..P8
 * (again: stop); Shift + mode button 1..8 = load preset 1..8; Shift + chord pad = choose that
 * chord for Settings > Chords without playing it. */
enum { TC_ACT_SHIFT = TC_FUNCTIONS, TC_ACT_HOLD, TC_ACT_ARP, TC_ACT_STRUM, TC_ACT_VOICE_LEAD, TC_ACT_FOLD,
       TC_ACT_HINTS, TC_ACT_SUSTAIN, TC_ACT_PANIC, TC_ACT_TAP, TC_ACT_PRESET_NEXT, TC_ACT_PRESET_PREV,
       TC_ACT_SCALE_NEXT, TC_ACT_SCALE_PREV, TC_ACT_OCT_UP, TC_ACT_OCT_DOWN, TC_ACT_PLAY, TC_ACT_RECORD,
       TC_ACT_ADD7, TC_ACT_ADD9, TC_ACT_ADD6, TC_ACT_SUS2, TC_ACT_SUS4, TC_ACT_CHORD_SET, TC_ACT_NONE,
       /* 1.4.0 (after NONE: stored actions keep their numbers): hold + root button = pivot to that
        * key (major or minor as the current scale's 3rd), a tap alone = pivot off */
       TC_ACT_PIVOT,
       TC_ACTIONS };
#define TC_SHIFT_LATCH_MS 400u /* two taps of a Shift button this close latch it */
enum { TC_MENU_SCALE, TC_MENU_HOLD, TC_MENU_STRUM, TC_MENU_STRUM_SPEED, TC_MENU_ARP,
       TC_MENU_ARP_RATE, TC_MENU_ARP_OCTAVES, TC_MENU_ARP_GATE, TC_MENU_TEMPO,
       TC_MENU_CHORD_CH, TC_MENU_MELODY_CH, TC_MENU_CHORD_AT, TC_MENU_VELOCITY,
       TC_MENU_LOAD, TC_MENU_SAVE, TC_MENU_MIDI_LEDS, TC_MENU_BRIGHTNESS, TC_MENU_LED_TEST,
       TC_MENU_VERSION, TC_MENU_TRS_OUT, TC_MENU_PEDAL, TC_MENU_PEDAL_TYPE, TC_MENU_IO_TEST,
       TC_MENU_COLORS, TC_MENU_MELODY_AT,
       /* 0.6.0 */
       TC_MENU_VOICING, TC_MENU_SPREAD, TC_MENU_ADD7, TC_MENU_ADD9, TC_MENU_ADD6, TC_MENU_SUS,
       TC_MENU_PAD_STRUM, TC_MENU_RESET_CHORD, TC_MENU_RESET_ALL, TC_MENU_LAYOUT, TC_MENU_BASS_CH,
       TC_MENU_DRUM_CH, TC_MENU_REPEAT, TC_MENU_CUTOFF, TC_MENU_RHYTHM, TC_MENU_SWING,
       TC_MENU_ARP_SYNC, TC_MENU_ARP_LATCH, TC_MENU_LIVE, TC_MENU_CLOCK_OUT, TC_MENU_TRANSPORT,
       TC_MENU_PEDAL_NOTE,
       /* 0.6.1 */
       TC_MENU_ARROWS, TC_MENU_MELODY_OCT,
       /* 0.7.0 */
       TC_MENU_FAMILY, TC_MENU_CAPO, TC_MENU_FEEL_TIME, TC_MENU_FEEL_VEL,
       /* 0.8.0 */
       TC_MENU_SOUND, TC_MENU_LIVE_CH, TC_MENU_LIVE_IN,
       /* 0.9.0 */
       TC_MENU_FOLD, TC_MENU_VOICE_LEAD, TC_MENU_NOTE_REPEAT,
       /* after 0.9.0: About > Pad test (raw pad readings, to judge sensitivity) */
       TC_MENU_PAD_TEST,
       /* 0.10.0: Pads > Trigger, Release, Full (the pad thresholds, in hundreds) */
       TC_MENU_PAD_TRIGGER, TC_MENU_PAD_RELEASE, TC_MENU_PAD_FULL,
       /* 0.11.0: velocity per half, the pedal's target, progressions */
       TC_MENU_MELODY_VEL, TC_MENU_PEDAL_TARGET, TC_MENU_SEQ_SLOT, TC_MENU_SEQ_UNIT, TC_MENU_SEQ_RECORD,
       TC_MENU_SEQ_CLEAR, TC_MENU_SEQ_GUIDE,
       /* 1.0.0: chord sets, hints, scale lock, the centre buttons' actions (one item per button) */
       TC_MENU_CHORD_SET, TC_MENU_HINTS, TC_MENU_LOCK,
       /* 1.4.0: Euclidean rotation, note repeat's rhythm, pivot chords, chord capture */
       TC_MENU_ARP_ROTATE, TC_MENU_REPEAT_RHYTHM, TC_MENU_PIVOT, TC_MENU_CAPTURE,
       /* 1.4.0: the last item of every page with options (the same item on each: it resets the page it
        * is on), and About's factory settings (every option, saved content kept) */
       TC_MENU_RESET_PAGE, TC_MENU_FACTORY,
       /* 1.5.0: the step sequencer (Sequence page): the step screen, then the lane the next items set */
       TC_MENU_STEPS, TC_MENU_LANE, TC_MENU_LANE_KIND, TC_MENU_LANE_LENGTH, TC_MENU_LANE_RATE,
       TC_MENU_LANE_CHANNEL, TC_MENU_LANE_MUTE,
       /* 1.5.0: which half the chord pads are on (Layout page) */
       TC_MENU_CHORDS_SIDE,
       /* 1.6.0: the harmonizer, echo (Play page), mono melody (Layout page), chance (Arpeggiator page) */
       TC_MENU_HARMONY, TC_MENU_ECHO, TC_MENU_ECHOES, TC_MENU_MONO, TC_MENU_CHANCE,
       /* 1.7.0: a lane's direction and Euclid fill (Sequence page), the pads' resting levels again (Pads) */
       TC_MENU_LANE_DIRECTION, TC_MENU_LANE_EUCLID, TC_MENU_LANE_ROTATE, TC_MENU_PAD_RESCAN,
       /* 1.7.0: a touch that settles back near rest ends (Pads page) */
       TC_MENU_PAD_UNSTICK,
       /* 1.7.0: Program Change in and each half's program out (MIDI page), what Spread spreads (Chords) */
       TC_MENU_PROGRAM_IN, TC_MENU_CHORD_PROG, TC_MENU_MELODY_PROG, TC_MENU_SPREAD_OF,
       TC_MENU_BUTTON1,
       TC_MENU_BUTTONS_RESET = TC_MENU_BUTTON1 + TC_FUNCTIONS, TC_MENU_ITEMS };
/* What up/down do on the play screen (0.6.1). */
/* Sequencer (0.11.0): up starts/stops the progression (the transport), down records. */
enum { TC_ARROWS_SCALE, TC_ARROWS_OCTAVE, TC_ARROWS_PRESET, TC_ARROWS_TEMPO, TC_ARROWS_SEQUENCER, TC_ARROWS_MODES };
/* Settings pages; each lists some of the items above (tc_group_items). */
enum { TC_GROUP_PLAY, TC_GROUP_CHORDS, TC_GROUP_LAYOUT, TC_GROUP_STRUM, TC_GROUP_ARP, TC_GROUP_MIDI,
       TC_GROUP_PEDAL, TC_GROUP_PRESETS, TC_GROUP_LIGHTS, TC_GROUP_SEQUENCE, TC_GROUP_PADS, TC_GROUP_BUTTONS,
       TC_GROUP_ABOUT, TC_GROUPS };
#define TC_GROUP_MAX 18u
extern const uint8_t tc_group_items[TC_GROUPS][TC_GROUP_MAX]; /* TC_MENU_ITEMS pads a short page */
uint8_t tc_group_size(uint8_t group);
/* Pressure output, set per half (chords, melody): off, poly aftertouch (every note a pad
 * plays that no other held pad shares), channel aftertouch, a controller, or pitch bend
 * up (0.6.0). */
enum { TC_AT_OFF, TC_AT_POLY, TC_AT_CHANNEL, TC_AT_CC1, TC_AT_CC2, TC_AT_CC11, TC_AT_CC74,
       TC_AT_BEND, TC_AT_MODES };
/* Velocity: from the strike (normal, light = more velocity for a soft touch, firm = less)
 * or fixed at 100. Values 0/1 are the earlier dynamic/fixed. */
enum { TC_VEL_DYNAMIC, TC_VEL_FIXED, TC_VEL_LIGHT, TC_VEL_FIRM, TC_VEL_MODES };
enum { TC_STRUM_OFF, TC_STRUM_UP, TC_STRUM_DOWN, TC_STRUM_ALTERNATE, TC_STRUM_MODES };
/* Chords (0.6.0): every step plays the whole held chord, strummed when strum is on. */
enum { TC_ARP_OFF, TC_ARP_UP, TC_ARP_DOWN, TC_ARP_UPDOWN, TC_ARP_RANDOM, TC_ARP_PLAYED,
       TC_ARP_CHORDS, TC_ARP_MODES };
/* Pedal jack: what the pedal does (sustain-type controllers, the board's hold, the
 * transport, or a note), and which level means "pressed" (auto: whatever it reads at
 * power-up is "released"; normal: pressed pulls the input low). */
enum { TC_PEDAL_OFF, TC_PEDAL_SUSTAIN, TC_PEDAL_HOLD, TC_PEDAL_SOSTENUTO, TC_PEDAL_SOFT,
       TC_PEDAL_PORTAMENTO, TC_PEDAL_TRANSPORT, TC_PEDAL_NOTE, TC_PEDAL_MODES };
enum { TC_PEDAL_AUTO, TC_PEDAL_NORMAL, TC_PEDAL_INVERTED, TC_PEDAL_TYPES };
/* Which half the sustain-type pedal controllers go to (0.11.0). */
enum { TC_PEDAL_BOTH, TC_PEDAL_CHORDS, TC_PEDAL_MELODY, TC_PEDAL_TARGETS };
#define TC_PEDAL_DEBOUNCE_MS 10u
/* Pad colors: Default (each pad the rainbow color of its note, root columns brightest), Root+5th
 * (root orange, fifth blue, other steps dim) or (1.5.0) Rainbow: both halves always seven colors
 * across the columns, red orange yellow green dark blue light blue magenta, then red to dark blue
 * again, whatever plays (touched and sounding pads still white). */
enum { TC_LEDS_RAINBOW, TC_LEDS_ROOT_FIFTH, TC_LEDS_ALL_RAINBOW, TC_LED_SCHEMES };
#define TC_BRIGHTNESS_LEVELS 8u
#define TC_STRUM_MS 8u       /* strum times 0..7 in milliseconds ... */
#define TC_STRUM_SPEEDS 13u  /* ... 8..12 in MIDI clocks at the tempo (0.6.0) */
#define TC_REPEATS 5u        /* strum repeat: off, 1/4, 1/8, 1/16, 1/32 */
/* Note repeat: off, 1/4, 1/8, 1/16, 1/32, then (1.4.0) 1/8T, 1/16T, 5 per beat, and By row: from the
 * bottom row up 2, 3, 4 and 5 per beat (hold pads on two rows for 2:3, 3:4 ...). */
#define TC_NOTE_REPEATS 9u
#define TC_REPEAT_BY_ROW 8u
#define TC_CUTOFFS 7u        /* off, 1/32, 1/16, 1/8, 1/4, 1/2, 1/1 */
#define TC_RHYTHMS 8u        /* the arpeggiator's 8-step patterns (tc_rhythms) ... */
#define TC_RHYTHM_VALUES (TC_RHYTHMS + RH_EUCLIDS) /* ... then (1.4.0) Euclidean k/n (rh_euclid_at) */
#define TC_ROTATES 16u
/* Pivot to (1.4.0): 0 off, 1..12 the major key on C..B, 13..24 the minor key on C..B (layout.h). */
#define TC_PIVOTS TC_PIVOT_KEYS
#define TC_SWINGS 6u         /* 0..50 % */
#define TC_ARP_RATES 12u     /* 1/4 1/8 1/8T 1/16 1/16T 1/32, then (1.4.0) 1/4T 1/32T 5/beat 7/beat 1/8. 1/16. */
#define TC_ARP_OCTAVES 3u
#define TC_ARP_GATES 4u
#define TC_TEMPOS 41u        /* 40..240 BPM in steps of 5 */
#define TC_PRESETS 8u
#define TC_FEEL_TIMES 5u     /* strum feel: each note up to 0, 5, 10, 20, 40 ms late at random */
#define TC_FEEL_VELS 5u      /* ... and its velocity up to 0, 5, 10, 20, 30 off at random */
#define TC_FAVOURITES 128u   /* favourite scales (0.7.0): the arrows step through them */
/* Harmonizer (1.6.0): each melody pad also plays notes of the scale this many scale steps away: off,
 * a 3rd, 4th, 5th or 6th above, the octave above, a 3rd and 5th above, a 3rd or 6th below. */
#define TC_HARMONIES 9u
/* Echo (1.6.0): melody pad strikes come back after off, 1/16, 1/8T, 1/8, 1/8., 1/4, 1/4. or 1/2 (MIDI
 * clocks: tc_echo_ticks), 1..8 times (Echoes, stored as count - 1; 3 by default), each quieter. */
#define TC_ECHO_TIMES 8u
#define TC_ECHOES 8u
#define TC_ECHOES_DEFAULT 2u
/* Chance (1.6.0): each arpeggiator step and each note repeat strike after the touch plays with this
 * probability: 100 % (always) down to 10 % (tc_chance_pct). */
#define TC_CHANCES 10u
/* Live notes (0.8.0): live_ch 0 = the melody channel, 1..16 = that channel; live_in 0 = off,
 * 1..16 = chords arriving on that MIDI channel set the live chord, 17 = on any channel. */
#define TC_LIVE_CHANNELS 17u
#define TC_LIVE_INPUTS 18u
/* Program Change (1.7.0): program_in 0 = off, 1..16 = programs 1..8 arriving on that MIDI channel load
 * presets 1..8, 17 = on any channel (a device setting); chord_prog and melody_prog 0 = none, n = program
 * n (1..128) sent on that half's channel when a session starts, the value is set and a preset loads. */
#define TC_PROGRAM_INPUTS 18u
#define TC_PROGRAMS 129u
/* Pad thresholds (0.10.0), in hundreds of the pad reading (0..4095, higher = harder) above each
 * pad's own resting level (1.7.0; before, levels the same for every pad): Trigger starts a note,
 * Release ends it (and is where pressure output starts), Full is velocity 127 and full pressure.
 * Trigger >= Release + 2, Full >= Trigger + 5. Stored as levels, as before 1.7.0 (settings word 32:
 * each setting + its TC_PAD_*_STORED; 1.6's defaults 12/6/35 are 1.7's 7/3/25), so a record reads
 * back in either version with about the same feel. */
#define TC_PAD_TRIGGER_MIN 5u /* noise up to 500 above rest never plays (as before 1.7.0) */
#define TC_PAD_TRIGGER_MAX 20u
#define TC_PAD_TRIGGER_DEFAULT 7u
#define TC_PAD_RELEASE_MIN 1u
#define TC_PAD_RELEASE_MAX 15u
#define TC_PAD_RELEASE_DEFAULT 3u
#define TC_PAD_FULL_MIN 10u
#define TC_PAD_FULL_MAX 30u
#define TC_PAD_FULL_DEFAULT 25u
#define TC_PAD_TRIGGER_STORED 5u
#define TC_PAD_RELEASE_STORED 3u
#define TC_PAD_FULL_STORED 10u
/* Unstick (1.7.0): a touch also ends once its reading has stayed within this much (a stuck pad holds
 * within 5, a finger wanders 50 or more) for TC_PAD_SETTLE_MS below Trigger, TC_PAD_STILL_MS at Trigger
 * or above (its first 1.7.0 form: a quarter of its rise, within 600 of rest). */
#define TC_PAD_SETTLE_BAND 30u
#define TC_PAD_SETTLE_MS 300u
#define TC_PAD_STILL_MS 1000u
#define TC_PAD_REST_CAP 2000u   /* a higher "resting" level is a held pad, not rest */
#define TC_PAD_REST_RISE 128u   /* 1.5.0: a resting level rises 1/128 of the way to the reading ... */
#define TC_PAD_REST_FALL 32u    /* ... and falls 1/32 of the way (1.7.0; before, a quarter) ... */
#define TC_PAD_REST_MS 15u      /* ... every 15 ms (about 2 s; once a frame when frames took 15 ms) */
#define TC_PAD_VELOCITY_MS 8u   /* 1.5.0: a touch read within 8 ms of the pad's last reading below Trigger
                                 * waits one more reading: its velocity is the higher of the two */
extern const uint8_t tc_strum_ms[TC_STRUM_MS];       /* delay between strummed notes */
extern const uint8_t tc_strum_ticks[TC_STRUM_SPEEDS - TC_STRUM_MS]; /* ... or MIDI clocks */
extern const uint8_t tc_repeat_ticks[TC_REPEATS];    /* strum repeat while held, MIDI clocks */
extern const uint8_t tc_cutoff_ticks[TC_CUTOFFS];    /* strummed chords end after, MIDI clocks */
extern const uint8_t tc_rhythms[TC_RHYTHMS];         /* arpeggiator: bit n = step n of 8 sounds */
/* A rhythm value (TC_RHYTHM_VALUES) as a mask of `steps` steps, rotated: an 8-step pattern or a
 * Euclidean one. */
uint64_t tc_rhythm_mask(uint8_t value, uint8_t rotate, uint32_t *steps);
extern const rh_rate tc_note_repeat_rates[TC_NOTE_REPEATS]; /* {0,0} for off and By row */
/* The note repeat rate of melody pad k (By row: its row's). */
rh_rate tc_note_repeat_rate(uint8_t repeat, uint32_t k);
extern const uint8_t tc_swing_pct[TC_SWINGS];
extern const rh_rate tc_arp_rates[TC_ARP_RATES];     /* MIDI clocks (24 per beat) per step, as fractions */
extern const uint8_t tc_arp_gate_pct[TC_ARP_GATES];
extern const uint8_t tc_feel_ms[TC_FEEL_TIMES];
extern const uint8_t tc_feel_vel[TC_FEEL_VELS];
extern const uint8_t tc_echo_ticks[TC_ECHO_TIMES];
extern const uint8_t tc_chance_pct[TC_CHANCES];
/* The harmonizer (1.6.0): each of these notes gets its harmony notes (in the layout's scale and key)
 * added, unless already there or out of MIDI range; at most TP_MAX_NOTES in all. */
void tc_harmonize(tp_layout layout, uint8_t harmony, tp_notes *notes);
uint32_t tc_tempo_bpm(uint8_t tempo);
uint8_t tc_at_cc(uint8_t at_mode); /* controller number, 0 when not a CC mode */
uint8_t tc_pedal_cc(uint8_t pedal_mode); /* 64/66/67/65 for the controller modes, else 0 */
enum { TC_REQUEST_NONE, TC_REQUEST_LOAD, TC_REQUEST_SAVE, TC_REQUEST_PROG_LOAD, TC_REQUEST_PROG_SAVE,
       TC_REQUEST_SET_SAVE /* 1.4.0: store chord set request_slot as it is now */ };
typedef struct {
    uint16_t stable, candidate;
    uint32_t changed[16];
    bool qualified;
} tc_debounce;
typedef struct {
    struct bi_sampler sampler;
    tc_debounce debounce[BI_CLIENTS];
    uint32_t last_poll, cycles, event_sequence, rejected, conflicts, deferred;
    uint32_t last_source, last_index, last_pressed, last_aliases, last_action;
    uint32_t presses[43], releases[43], unknown[4], ambiguous[4];
    uint32_t pressed[4], released[4], recoveries;
    uint32_t changes; /* bumps on every persisted change (settings.c) */
    tc_chord chord[48];
    uint64_t strum_off; /* bit k: chord pad k never strums (0.6.0) */
    uint8_t tonic, scale, chord_base, melody_base, last_chord;
    uint8_t ui, menu, edit_value, last_function, group, nav_held; /* nav_held: bit n = nav n down */
    uint16_t fn_held;   /* bit n = centre button n down */
    uint8_t at_chord, at_melody, vel_mode, brightness;
    uint8_t hold, strum, strum_speed, arp, arp_rate, arp_octaves, arp_gate, tempo;
    uint8_t chord_ch, melody_ch, midi_leds, arp_last;
    /* 0.6.0: chord panel, layout, strum/arp extras, live update, clock out */
    uint8_t chord_voicing, chord_spread, chord_octaves, melody_layout, bass_ch, drum_ch;
    uint8_t strum_repeat, strum_cutoff, arp_rhythm, arp_swing, arp_sync, arp_latch, live_update;
    uint8_t clock_out, pedal_note, arrows;
    uint8_t family, capo, feel_time, feel_vel; /* 0.7.0 */
    uint8_t live_ch, live_in; /* 0.8.0 */
    uint8_t fold, voice_lead, note_repeat; /* 0.9.0; note_repeat: right-half pads, TC_NOTE_REPEATS */
    uint8_t arp_rotate;      /* 1.4.0: the arpeggiator's rhythm rotated (TC_ROTATES) */
    uint8_t repeat_rhythm;   /* 1.4.0: note repeat's rhythm: 0 every step, n = Euclidean n - 1 */
    uint8_t pivot;           /* 1.4.0: the key to pivot to (TC_PIVOTS; 0 off): its pivot chords light */
    bool capturing;          /* 1.4.0: capturing a chord set: chords played (pads, MIDI in) are its columns */
    uint8_t capture_slot, capture_key; /* the set captured into; the key it was saved in (TC_PIVOTS) */
    uint8_t pad_trigger, pad_release, pad_full; /* 0.10.0, hundreds; device settings: presets leave them */
    uint8_t unstick;         /* 1.7.0: Settings > Pads > Unstick (1 = on, the default); presets leave it */

    uint8_t melody_vel;      /* 0.11.0: velocity curve of the melody half (vel_mode: the chord half) */
    uint8_t pedal_target;    /* 0.11.0: TC_PEDAL_BOTH/CHORDS/MELODY; device setting */
    uint8_t prog_slot, seq_guide; /* 0.11.0: the selected progression (sequence.h sq), guide lights */
    bool recording;          /* recording a progression: chord pad touches are its steps */
    /* 1.0.0 */
    uint8_t chord_set;       /* 0 = the scale's chords, n = chord set n (tp_layout.chords) */
    uint8_t hints;           /* theory hints: light the usual next chords */
    uint8_t lock;            /* scale lock: 0 off, n = notes arriving on MIDI channel n; device setting */
    uint8_t buttons[TC_FUNCTIONS]; /* each centre button's action (TC_ACT_ / panel function); device setting */
    bool shift_latched;
    bool button_sustain;     /* a Sustain button tapped on (tc_sustain_button) */
    uint32_t shift_tapped, tapped; /* Shift's last tap (latching), tap tempo's last tap */
    uint8_t last_button;     /* the action behind TC_BUTTON */
    bool panic;              /* all notes off: the engine releases everything it plays, then clears it */
    uint64_t chord_mute;      /* bit k: chord pad k makes no sound, still sets the live chord (0.8.0) */
    uint8_t trs_out, pedal_mode, pedal_type, led_scheme; /* device settings: presets leave them alone */
    bool sustain;            /* pedal down in a controller mode: the engine sends pedal_cc */
    bool pedal_down_note;    /* pedal down in note mode: the engine plays pedal_note */
    bool transport;          /* the board's transport runs (MIDI Start sent, clock out) */
    bool relayout;           /* re-strike held: re-capture held pads after a layout change */
    bool relayout_live;      /* re-strike held: the live chord changed (live-area pads only) */
    /* The live chord: pad live_pad's, or live_midi while live_from_midi (layout.c tc_live_current). */
    uint8_t live_pad;
    bool live_from_midi;
    tc_live_chord live_midi;
    tp_notes lead;           /* the last chord struck, before spread/octaves: voice leading starts there */
    bool pedal_known, pedal_level, pedal_rest, pedal_pressed;
    uint32_t pedal_at;
    uint8_t preset, request, request_slot; /* preset: last loaded/saved, 0xFF none */
    /* 1.5.0, the step screen (not stored): the lane shown and set by the Sequence page, the page of
     * steps (0: 1-48, 1: 49-64), step record and its cursor, the step pad held (LN_NONE: none) and
     * whether a value was given to it meanwhile, and per lane the value a step tapped on gets. */
    uint8_t chords_right;    /* 1.5.0: the chord pads on the right half, the melody pads on the left; device setting */
    uint8_t lane, step_page, step_cursor, step_held;
    bool step_rec, step_set;
    /* 1.7.0 (not stored): every step pad held (the edits go to all of them; step_held is the one
     * shown), the field up/down chose, and per lane the Euclid rotation last set. */
    uint64_t step_mask;
    uint8_t step_field, lane_rotate[LN_LANES];
    bool rescan;             /* 1.7.0: measure every pad's resting level again (engine_pads.c, next frame) */
    bool reshape;            /* 1.7.0: a chord shape button went down or up: the arpeggiator's chords take it */
    /* 1.6.0: the harmonizer (TC_HARMONIES), echo time and count (TC_ECHO_TIMES, TC_ECHOES: count - 1), the
     * melody half one note at a time, chance (TC_CHANCES; 0 = 100 %) */
    uint8_t harmony, echo, echoes, mono, chance;
    /* 1.7.0: Program Change (TC_PROGRAM_INPUTS, TC_PROGRAMS); program_again: send again, bit 0 the chord
     * half's, bit 1 the melody half's (a program set, both when a preset loads), the engine clears it;
     * program_seen: tm.programs taken. Spread of Chord. */
    uint8_t program_in, chord_prog, melody_prog, spread_chord, program_again;
    uint32_t program_seen;
    ln_step brush[LN_LANES];
    bool started, have_chord, led_test; /* led_test: ui.c runs it, then clears it */
} tc_controls;
#define TC_EMPTY_SLOT 0xFFu
extern const uint8_t tc_mode_buttons[12]; /* mode-button index -> layout scale id (Major family) */
/* The scale a mode button plays: M1..M7 follow the family (Settings > Play > Family). */
uint8_t tc_mode_scale(const tc_controls *c, uint32_t button);
/* Favourite scales, in order (settings.c stores them; the web editor sets them). */
extern uint8_t tc_favourites[TC_FAVOURITES], tc_favourite_count;
extern tc_controls tc_state;
extern volatile uint32_t tc_status[TC_WORDS];
void tc_init(tc_controls *c);
/* No client retries; every address serviced once even if another fails. */
bool tc_poll(tc_controls *c, tp_engine *e, bi_read read, void *context, uint32_t now);
bool tc_decode(uint8_t address, uint8_t bit, uint32_t *source,
    uint32_t *index, uint32_t *logical, uint32_t *aliases);
/* The layout the controls currently select (root, scale, octaves, right-half layout). */
tp_layout tc_layout(const tc_controls *c);
bool tc_set_layout(tc_controls *c, tp_engine *e, tp_layout proposed);
/* Presets known to exist (settings.c keeps this current; bit n = slot n). */
extern uint8_t tc_presets_present;
/* Value count and current value of a menu item (ui_menu.c shows them). */
uint8_t tc_menu_values(uint8_t item);
/* Resets (1.4.0: Reset chord, Reset all edits, the Buttons page's Reset all, Reset options, Factory
 * settings) ask first: their value opens on CANCEL (0) and only RESET (1), chosen with up, then
 * centre, resets. */
bool tc_confirm_item(uint8_t item);
/* How many of a page's items Reset options sets back to their defaults. */
uint8_t tc_reset_count(uint8_t group);
uint8_t tc_menu_value(const tc_controls *c, uint8_t item);
/* Whether centre button n's function is on (its light is then full, otherwise dim). */
bool tc_panel_active(const tc_controls *c, uint32_t index);
/* Shift is held or latched (1.0.0). */
bool tc_shift(const tc_controls *c);
/* A Sustain button is on (1.0.0): CC64 while it is, unless the pedal is down. */
bool tc_sustain_button(const tc_controls *c);
/* A centre button with this action is held. */
bool tc_button_held(const tc_controls *c, uint8_t action);
/* Chord pad k's edits as it plays now: its own, with the chord shapes of held centre buttons
 * (add 7th/9th/6th, sus2/sus4 while held) when they fit the layout. */
tc_chord tc_pad_edit(const tc_controls *c, tp_layout layout, uint32_t k);
/* Chord capture (1.4.0): a chord played while capturing (root a pitch class, tones above it) is
 * the next column, unless it repeats the last one. The chords so far, their count returned. */
void tc_capture_chord(tc_controls *c, uint8_t root, uint16_t tones);
uint32_t tc_captured(uint8_t roots[12], uint16_t tones[12]);
/* The step screen (1.5.0): pad k touched (pressed) or lifted, with a touch's velocity. */
void tc_step_pad(tc_controls *c, tp_layout layout, uint32_t k, bool pressed, uint8_t velocity);
/* 1.7.0: a step's field (TC_FIELD_..) one value on (d > 0) or back; the step screen's left/right on held steps. */
void tc_step_field(ln_step *s, uint8_t field, int32_t d);
/* Every centre button back to its printed panel function. */
void tc_buttons_default(tc_controls *c);
/* Program Change in (1.7.0): a program that arrived since the last call (midi_in.c), on the Program in
 * channel, loads that preset as Shift + mode button does; main.c calls it after MIDI input. */
void tc_program_in(tc_controls *c);
void tc_publish(const tc_controls *c, const tc_bus *b, volatile uint32_t words[TC_WORDS]);
/* The pedal input's level (true = high), every main-loop pass: debounces, works out
 * pressed/released for the pedal type, and applies the pedal mode. */
void tc_pedal(tc_controls *c, bool level, uint32_t now);
/* The items a page shows, in order (1.5.0): the pedal's Note only while Mode is Note (Target
 * stays: the Sustain button uses it too). Returns the count. */
uint8_t tc_page_items(const tc_controls *c, uint8_t group, uint8_t items[TC_GROUP_MAX]);
/* Pads keep their roles whichever side they are on: pads 0..47 are chord pads, 48..95 melody pads
 * everywhere in the firmware. With Chords on Right (1.5.0) the halves trade places only where pads
 * meet the hardware: the pad the scanner reads for pad k, and the pad its light goes to. The top
 * row of root and mode buttons stays as printed. Its own inverse. */
uint32_t tc_physical(const tc_controls *c, uint32_t k);
/* MIDI channel of pad k (chord pads, melody pads, or the drums layout's three pads). */
uint8_t tc_channel_of(const tc_controls *c, uint32_t k);
/* The pad thresholds as set: Trigger/Release/Full. */
tk_config tc_pad_config(const tc_controls *c);
/* A pad's thresholds (1.7.0): the settings above its own resting level (capped at 2000: a higher one
 * is a held pad); Full at most 4095. Only for polarity 1 (higher = harder). Before, the settings
 * were levels, raised only as far as a pad resting high needed to read released. */
tk_config tc_pad_raise(tk_config settings, uint32_t rest);
/* Whether these settings keep the gaps (Trigger >= Release + 200, Full >= Trigger + 500). */
bool tc_pad_settings_ok(uint32_t trigger, uint32_t release, uint32_t full);
#endif
