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
    TC_PRESET_SAVED, TC_PRESET_EMPTY, TC_REMOTE };
/* UI: play, menu list, editing one menu item's value. */
enum { TC_UI_PLAY, TC_UI_MENU, TC_UI_EDIT };
enum { TC_MENU_SCALE, TC_MENU_HOLD, TC_MENU_STRUM, TC_MENU_STRUM_SPEED, TC_MENU_ARP,
       TC_MENU_ARP_RATE, TC_MENU_ARP_OCTAVES, TC_MENU_ARP_GATE, TC_MENU_TEMPO,
       TC_MENU_CHORD_CH, TC_MENU_MELODY_CH, TC_MENU_AFTERTOUCH, TC_MENU_VELOCITY,
       TC_MENU_LOAD, TC_MENU_SAVE, TC_MENU_MIDI_LEDS, TC_MENU_BRIGHTNESS, TC_MENU_LED_TEST,
       TC_MENU_ITEMS };
/* Pressure output: off, poly aftertouch, channel aftertouch, or a controller. */
enum { TC_AT_OFF, TC_AT_POLY, TC_AT_CHANNEL, TC_AT_CC1, TC_AT_CC2, TC_AT_CC11, TC_AT_CC74,
       TC_AT_MODES };
enum { TC_VEL_DYNAMIC, TC_VEL_FIXED, TC_VEL_MODES };
enum { TC_STRUM_OFF, TC_STRUM_UP, TC_STRUM_DOWN, TC_STRUM_ALTERNATE, TC_STRUM_MODES };
enum { TC_ARP_OFF, TC_ARP_UP, TC_ARP_DOWN, TC_ARP_UPDOWN, TC_ARP_RANDOM, TC_ARP_PLAYED,
       TC_ARP_MODES };
#define TC_BRIGHTNESS_LEVELS 8u
#define TC_STRUM_SPEEDS 8u
#define TC_ARP_RATES 6u
#define TC_ARP_OCTAVES 3u
#define TC_ARP_GATES 4u
#define TC_TEMPOS 41u        /* 40..240 BPM in steps of 5 */
#define TC_PRESETS 8u
extern const uint8_t tc_strum_ms[TC_STRUM_SPEEDS];   /* delay between strummed notes */
extern const uint8_t tc_arp_ticks[TC_ARP_RATES];     /* MIDI clocks (24 per beat) per step */
extern const uint8_t tc_arp_gate_pct[TC_ARP_GATES];
uint32_t tc_tempo_bpm(uint8_t tempo);
uint8_t tc_at_cc(uint8_t at_mode); /* controller number, 0 when not a CC mode */
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
    uint8_t tonic, scale, chord_base, melody_base, last_chord;
    uint8_t ui, menu, edit_value, last_function;
    uint8_t at_mode, vel_mode, brightness;
    uint8_t hold, strum, strum_speed, arp, arp_rate, arp_octaves, arp_gate, tempo;
    uint8_t chord_ch, melody_ch, midi_leds, arp_last;
    uint8_t preset, request, request_slot; /* preset: last loaded/saved, 0xFF none */
    bool started, have_chord, led_test; /* led_test: ui.c runs it, then clears it */
} tc_controls;
#define TC_EMPTY_SLOT 0xFFu
extern const uint8_t tc_mode_buttons[12]; /* mode-button index -> layout scale id */
extern tc_controls tc_state;
extern volatile uint32_t tc_status[TC_WORDS];
void tc_init(tc_controls *c);
/* No client retries; every address serviced once even if another fails. */
bool tc_poll(tc_controls *c, tp_engine *e, bi_read read, void *context, uint32_t now);
bool tc_decode(uint8_t address, uint8_t bit, uint32_t *source,
    uint32_t *index, uint32_t *logical, uint32_t *aliases);
/* The layout the controls currently select (root, scale, octaves). */
tp_layout tc_layout(const tc_controls *c);
bool tc_set_layout(tc_controls *c, tp_engine *e, tp_layout proposed);
/* Presets known to exist (settings.c keeps this current; bit n = slot n). */
extern uint8_t tc_presets_present;
/* Value count and current value of a menu item (ui.c shows them). */
uint8_t tc_menu_values(uint8_t item);
uint8_t tc_menu_value(const tc_controls *c, uint8_t item);
void tc_publish(const tc_controls *c, const tc_bus *b, volatile uint32_t words[TC_WORDS]);
#endif
