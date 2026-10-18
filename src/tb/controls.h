/* SPDX-License-Identifier: GPL-3.0-or-later - capacitive controls: decode, debounce, dispatch, publish. */
#ifndef TC_CONTROLS_H
#define TC_CONTROLS_H
#include "i2c.h"
#include "engine.h"
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
    TC_SCALE_STEP, TC_TEMPO_STEP, TC_FAVOURITE_STEP };
/* UI: play screen, a settings page's items, editing one item's value, the list of pages. */
enum { TC_UI_PLAY, TC_UI_MENU, TC_UI_EDIT, TC_UI_GROUPS };
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
enum { TC_MENU_SCALE, TC_MENU_HOLD, TC_MENU_STRUM, TC_MENU_STRUM_SPEED, TC_MENU_ARP,
       TC_MENU_ARP_RATE, TC_MENU_ARP_OCTAVES, TC_MENU_ARP_GATE, TC_MENU_TEMPO,
       TC_MENU_CHORD_CH, TC_MENU_MELODY_CH, TC_MENU_CHORD_AT, TC_MENU_VELOCITY,
       TC_MENU_LOAD, TC_MENU_SAVE, TC_MENU_MIDI_LEDS, TC_MENU_BRIGHTNESS, TC_MENU_LED_TEST,
       TC_MENU_VERSION, TC_MENU_TRS_OUT, TC_MENU_PEDAL, TC_MENU_PEDAL_TYPE, TC_MENU_IO_TEST,
       TC_MENU_COLOURS, TC_MENU_MELODY_AT,
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
       TC_MENU_PAD_TEST, TC_MENU_ITEMS };
/* What up/down do on the play screen (0.6.1). */
enum { TC_ARROWS_SCALE, TC_ARROWS_OCTAVE, TC_ARROWS_PRESET, TC_ARROWS_TEMPO, TC_ARROWS_MODES };
/* Settings pages; each lists some of the items above (tc_group_items). */
enum { TC_GROUP_PLAY, TC_GROUP_CHORDS, TC_GROUP_LAYOUT, TC_GROUP_STRUM, TC_GROUP_ARP, TC_GROUP_MIDI,
       TC_GROUP_PEDAL, TC_GROUP_PRESETS, TC_GROUP_LIGHTS, TC_GROUP_ABOUT, TC_GROUPS };
#define TC_GROUP_MAX 12u
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
#define TC_PEDAL_DEBOUNCE_MS 10u
/* Pad colours: rainbow (each pad the colour of its note, root columns brightest) or
 * root+5th (root orange, fifth blue, other steps dim). */
enum { TC_LEDS_RAINBOW, TC_LEDS_ROOT_FIFTH, TC_LED_SCHEMES };
#define TC_BRIGHTNESS_LEVELS 8u
#define TC_STRUM_MS 8u       /* strum times 0..7 in milliseconds ... */
#define TC_STRUM_SPEEDS 13u  /* ... 8..12 in MIDI clocks at the tempo (0.6.0) */
#define TC_REPEATS 5u        /* off, 1/4, 1/8, 1/16, 1/32 */
#define TC_CUTOFFS 7u        /* off, 1/32, 1/16, 1/8, 1/4, 1/2, 1/1 */
#define TC_RHYTHMS 8u
#define TC_SWINGS 6u         /* 0..50 % */
#define TC_ARP_RATES 6u
#define TC_ARP_OCTAVES 3u
#define TC_ARP_GATES 4u
#define TC_TEMPOS 41u        /* 40..240 BPM in steps of 5 */
#define TC_PRESETS 8u
#define TC_FEEL_TIMES 5u     /* strum feel: each note up to 0, 5, 10, 20, 40 ms late at random */
#define TC_FEEL_VELS 5u      /* ... and its velocity up to 0, 5, 10, 20, 30 off at random */
#define TC_FAVOURITES 128u   /* favourite scales (0.7.0): the arrows step through them */
/* Live notes (0.8.0): live_ch 0 = the melody channel, 1..16 = that channel; live_in 0 = off,
 * 1..16 = chords arriving on that MIDI channel set the live chord, 17 = on any channel. */
#define TC_LIVE_CHANNELS 17u
#define TC_LIVE_INPUTS 18u
extern const uint8_t tc_strum_ms[TC_STRUM_MS];       /* delay between strummed notes */
extern const uint8_t tc_strum_ticks[TC_STRUM_SPEEDS - TC_STRUM_MS]; /* ... or MIDI clocks */
extern const uint8_t tc_repeat_ticks[TC_REPEATS];    /* strum repeat while held, MIDI clocks */
extern const uint8_t tc_cutoff_ticks[TC_CUTOFFS];    /* strummed chords end after, MIDI clocks */
extern const uint8_t tc_rhythms[TC_RHYTHMS];         /* arpeggiator: bit n = step n of 8 sounds */
extern const uint8_t tc_swing_pct[TC_SWINGS];
extern const uint8_t tc_arp_ticks[TC_ARP_RATES];     /* MIDI clocks (24 per beat) per step */
extern const uint8_t tc_arp_gate_pct[TC_ARP_GATES];
extern const uint8_t tc_feel_ms[TC_FEEL_TIMES];
extern const uint8_t tc_feel_vel[TC_FEEL_VELS];
uint32_t tc_tempo_bpm(uint8_t tempo);
uint8_t tc_at_cc(uint8_t at_mode); /* controller number, 0 when not a CC mode */
uint8_t tc_pedal_cc(uint8_t pedal_mode); /* 64/66/67/65 for the controller modes, else 0 */
enum { TC_REQUEST_NONE, TC_REQUEST_LOAD, TC_REQUEST_SAVE };
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
    uint8_t fold, voice_lead, note_repeat; /* 0.9.0; note_repeat: right-half pads, tc_repeat_ticks */
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
/* Value count and current value of a menu item (ui.c shows them). */
uint8_t tc_menu_values(uint8_t item);
uint8_t tc_menu_value(const tc_controls *c, uint8_t item);
/* Whether centre button n's function is on (its light is then full, otherwise dim). */
bool tc_panel_active(const tc_controls *c, uint32_t index);
void tc_publish(const tc_controls *c, const tc_bus *b, volatile uint32_t words[TC_WORDS]);
/* The pedal input's level (true = high), every main-loop pass: debounces, works out
 * pressed/released for the pedal type, and applies the pedal mode. */
void tc_pedal(tc_controls *c, bool level, uint32_t now);
/* MIDI channel of pad k (chord pads, melody pads, or the drums layout's three pads). */
uint8_t tc_channel_of(const tc_controls *c, uint32_t k);
#endif
