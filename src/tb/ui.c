/* SPDX-License-Identifier: GPL-3.0-or-later - OLED screens and LED colours.
 * Play screen: root, scale, mode button or preset, the chord name of the last chord
 * pad, the last action (every centre button names its function, so the board
 * documents itself), octaves/hold, then arpeggiator, strum or velocity/pressure.
 * Menu screen: a scrolling list of items with values. Pads: root and fifth
 * coloured, other scale steps dim, edited chords purple, sounding pads white,
 * notes arriving over MIDI green, a scale previewed by the web editor cyan. */
#include "ui.h"
#include "display.h"
#include "leds.h"
#include "chord_name.h"
#include "midi_in.h"
#include "sysex.h"
#include <string.h>
static const char *const roots[12] = {"C","C#","D","D#","E","F","F#","G","G#","A","A#","B"};
static const char *const mode_labels[12] = {"5","12","M1","M2","M3","M4","M5","M6","M7","U1","U2","U3"};
static const char *const functions[14] = {"LOWER ROOT","LOWER 3RD","LOWER 5TH","CHORDS OCT-",
    "CHORDS OCT+","MELODY OCT-","MELODY OCT+","ADD 7TH","ADD 9TH","SUS2","SUS4","ADD 6TH",
    "RESET CHORD","RESET ALL"};
static const char *const menu_items[TC_MENU_ITEMS] = {"SCALE","HOLD","STRUM","STRUM TIME","ARP",
    "ARP RATE","ARP OCTAVES","ARP GATE","TEMPO","CHORD CH","MELODY CH","PRESSURE","VELOCITY",
    "LOAD PRESET","SAVE PRESET","MIDI IN LEDS","BRIGHTNESS","LED TEST"};
static const char *const at_names[TC_AT_MODES] = {"OFF","POLY AT","CHANNEL AT","CC1 MOD","CC2 BREATH",
    "CC11 EXPR","CC74 FILTER"};
static const char *const at_short[TC_AT_MODES] = {"OFF","POLY","CHAN","CC1","CC2","CC11","CC74"};
static const char *const vel_names[TC_VEL_MODES] = {"DYNAMIC","FIXED 100"};
static const char *const vel_short[TC_VEL_MODES] = {"DYN","FIX"};
static const char *const strum_names[TC_STRUM_MODES] = {"OFF","UP","DOWN","UP+DOWN"};
static const char *const arp_names[TC_ARP_MODES] = {"OFF","UP","DOWN","UP-DOWN","RANDOM","AS PLAYED"};
static const char *const rate_names[TC_ARP_RATES] = {"1/4","1/8","1/8T","1/16","1/16T","1/32"};
static const char *const off_on[2] = {"OFF","ON"};
static const uint8_t levels[TC_BRIGHTNESS_LEVELS] = {4,8,14,22,32,44,58,74};
typedef struct {
    uint32_t sequence, changes, action, source, index, function;
    uint8_t ui, menu, edit, have_chord, last_chord, usb, test_phase, test_item, clocked, preview;
    tc_chord chord;
} view;
static view shown;
static bool forced = true;
static uint32_t test_start;
static char buffer[40];
static uint32_t length;
static void put(const char *s) { while (*s && length < sizeof buffer - 1u) buffer[length++] = *s++; buffer[length] = 0; }
static void put_number(uint32_t n) {
    char digits[11]; uint32_t i = 0;
    do { digits[i++] = (char)('0' + n % 10u); n /= 10u; } while (n && i < sizeof digits);
    while (i) { char d[2] = {digits[--i], 0}; put(d); }
}
static void start(void) { length = 0; buffer[0] = 0; }
static void note_name(uint32_t midi) {
    put(roots[midi % 12u]);
    if (midi < 12u) put("-1"); else put_number(midi / 12u - 1u);
}
static void line(uint32_t y) { td_text(0, y, buffer, 1); }
static void last_action(const tc_controls *c) {
    start();
    if (c->last_source == TC_FUNCTION && c->last_action != TC_TODO_CONTEXT) {
        put("F"); put_number(c->last_function); put(" ");
        put(c->last_action == TC_TODO_FUNCTION ? "PLAY A CHORD 1ST" :
            c->last_action == TC_REJECT ? "OUT OF RANGE" : functions[c->last_function % 14u]);
        return;
    }
    switch (c->last_action) {
    case TC_TONIC: put("ROOT "); put(roots[c->tonic % 12u]); break;
    case TC_MODE: put("MODE "); put(mode_labels[c->last_index % 12u]); put(" "); put(tc_scale_name(c->scale)); break;
    case TC_TODO_FUNCTION: put(mode_labels[c->last_index % 12u]); put(" IS EMPTY"); break;
    case TC_TODO_CONTEXT: put("CLOSE THE MENU FIRST"); break;
    case TC_REJECT: put("OUT OF RANGE"); break;
    case TC_CONFLICT: put("ONE BUTTON AT A TIME"); break;
    case TC_SETTING: put("SETTING CHANGED"); break;
    case TC_HOLD: put(c->hold ? "HOLD ON" : "HOLD OFF"); break;
    case TC_ARP: put("ARP "); put(arp_names[c->arp % TC_ARP_MODES]); break;
    case TC_PRESET_LOADED: put("PRESET "); put_number(c->preset + 1u); put(" LOADED"); break;
    case TC_PRESET_SAVED: put("PRESET "); put_number(c->preset + 1u); put(" SAVED"); break;
    case TC_PRESET_EMPTY: put("NO PRESET THERE"); break;
    case TC_REMOTE: put("CHANGED BY EDITOR"); break;
    default: put("READY"); break;
    }
}
static void draw_play(const tc_controls *c, bool usb, bool clocked) {
    td_text(0, 0, roots[c->tonic % 12u], 2);
    td_text(30, 0, tc_scale_name(c->scale), 1);
    uint32_t button = 12u;
    for (uint32_t i = 0; i < 12u; ++i) if (tc_mode_buttons[i] == c->scale) { button = i; break; }
    start(); if (button < 12u) { put("MODE "); put(mode_labels[button]); }
    if (c->preset < TC_PRESETS) { put(button < 12u ? "  P" : "PRESET "); put_number(c->preset + 1u); }
    td_text(30, 8, buffer, 1);
    char name[TN_MAX];
    uint32_t n = c->have_chord ? tn_pad(tc_layout(c), c->last_chord % 48u, name) : 0u;
    if (n) {
        td_text(0, n <= 10u ? 18u : 22u, name, n <= 10u ? 2u : 1u);
    } else td_text(0, 22, "PLAY A CHORD", 1);
    last_action(c); line(36);
    start(); put("OCT "); note_name(c->chord_base + c->tonic); put(" "); note_name(c->melody_base + c->tonic);
    if (c->hold) put(" HOLD");
    line(46);
    start();
    if (!usb) put("USB: WAITING");
    else if (c->arp) {
        put("ARP "); put(arp_names[c->arp % TC_ARP_MODES]); put(" "); put(rate_names[c->arp_rate % TC_ARP_RATES]);
        put(" "); if (clocked) put("EXT"); else put_number(tc_tempo_bpm(c->tempo));
    } else if (c->strum) {
        put("STRUM "); put(strum_names[c->strum % TC_STRUM_MODES]); put(" ");
        put_number(tc_strum_ms[c->strum_speed % TC_STRUM_SPEEDS]); put("MS");
    } else {
        put("VEL "); put(vel_short[c->vel_mode % TC_VEL_MODES]); put(" PRESS "); put(at_short[c->at_mode % TC_AT_MODES]);
    }
    line(56);
}
static void value_name(uint8_t item, uint8_t v) {
    start();
    switch (item) {
    case TC_MENU_SCALE: put(tc_scale_name(v)); if (!tc_scale_defined(v)) put(" EMPTY"); break;
    case TC_MENU_HOLD: case TC_MENU_MIDI_LEDS: put(off_on[v & 1u]); break;
    case TC_MENU_STRUM: put(strum_names[v % TC_STRUM_MODES]); break;
    case TC_MENU_STRUM_SPEED: put_number(tc_strum_ms[v % TC_STRUM_SPEEDS]); put(" MS"); break;
    case TC_MENU_ARP: put(arp_names[v % TC_ARP_MODES]); break;
    case TC_MENU_ARP_RATE: put(rate_names[v % TC_ARP_RATES]); break;
    case TC_MENU_ARP_OCTAVES: put_number(v + 1u); break;
    case TC_MENU_ARP_GATE: put_number(tc_arp_gate_pct[v % TC_ARP_GATES]); put("%"); break;
    case TC_MENU_TEMPO: put_number(tc_tempo_bpm(v)); put(" BPM"); break;
    case TC_MENU_CHORD_CH: case TC_MENU_MELODY_CH: put_number(v + 1u); break;
    case TC_MENU_AFTERTOUCH: put(at_names[v % TC_AT_MODES]); break;
    case TC_MENU_VELOCITY: put(vel_names[v % TC_VEL_MODES]); break;
    case TC_MENU_LOAD: case TC_MENU_SAVE:
        put_number(v + 1u); if (!(tc_presets_present & (1u << (v % TC_PRESETS)))) put(" EMPTY");
        break;
    case TC_MENU_BRIGHTNESS: put_number(v + 1u); break;
    default: put("START"); break;
    }
}
static void draw_menu(const tc_controls *c) {
    td_text(0, 0, "MENU", 1);
    td_text(40, 0, c->ui == TC_UI_EDIT ? "< > CHANGE" : "< > MOVE", 1);
    /* Five rows; the selection stays in the middle once the list scrolls. */
    uint32_t first = c->menu < 2u ? 0u : c->menu - 2u;
    if (first > TC_MENU_ITEMS - 5u) first = TC_MENU_ITEMS - 5u;
    for (uint32_t row = 0; row < 5u; ++row) {
        uint8_t i = (uint8_t)(first + row);
        uint32_t y = 12u + 10u * row;
        td_text(0, y, i == c->menu ? ">" : " ", 1);
        td_text(8, y, menu_items[i], 1);
        value_name(i, i == c->menu && c->ui == TC_UI_EDIT ? c->edit_value : tc_menu_value(c, i));
        uint32_t x = 128u - 6u * length;
        td_text(x, y, buffer, 1);
        if (i == c->menu && c->ui == TC_UI_EDIT) td_invert(x - 1u, y - 1u, 128u, y + 8u);
    }
}
static tl_rgb scaled(uint32_t r, uint32_t g, uint32_t b, uint32_t max) {
    return (tl_rgb){(uint8_t)(r * max / 255u), (uint8_t)(g * max / 255u), (uint8_t)(b * max / 255u)};
}
static uint32_t incoming_classes(void) {
    uint32_t set = 0;
    for (uint32_t p = 0; p < 128u; ++p) if (tm.notes[p]) set |= 1u << (p % 12u);
    return set;
}
static void pad_colours(const tc_controls *c, const tp_engine *e, uint32_t now) {
    uint32_t max = levels[c->brightness % TC_BRIGHTNESS_LEVELS];
    tp_layout layout = tc_layout(c);
    uint32_t incoming = c->midi_leds ? incoming_classes() : 0u;
    uint16_t preview = sx_preview(now, NULL);
    for (uint32_t k = 0; k < TL_PADS; ++k) {
        tp_position p; (void)tp_position_of(k, &p);
        uint32_t semis = tc_step_semitones(c->scale, p.column);
        tc_chord edit = c->chord[k % 48u];
        tl_rgb colour;
        bool lit = false;
        if (incoming) {
            /* Melody pads: their note is playing; chord pads: all their notes are. */
            tp_notes n;
            if (tc_pad(layout, k, &n)) {
                uint32_t set = 0;
                for (uint32_t i = 0; i < n.count; ++i) set |= 1u << (n.notes[i] % 12u);
                lit = set && (set & ~incoming) == 0u;
            }
        }
        if (e->keys[k].sensed || e->keys[k].admitted) colour = scaled(255, 255, 255, max);
        else if (preview && p.side) {
            /* The editor's scale as 12 semitone columns from its root; others dark. */
            bool in = (preview >> p.column) & 1u;
            colour = !in ? (tl_rgb){0, 0, 0} : p.column == 0u ? scaled(255, 70, 0, max) : scaled(0, 200, 200, max);
            tl_set_pad(k, colour);
            continue;
        }
        else if (lit) colour = scaled(0, 255, 60, max);
        else if (!p.side && (edit.lower || edit.sus || edit.add)) colour = scaled(160, 0, 255, max);
        else if (semis == 0u) colour = scaled(255, 70, 0, max);
        else if (semis == 7u) colour = scaled(0, 90, 255, max);
        else colour = scaled(40, 40, 50, max);
        if (!colour.r && !colour.g && !colour.b) colour.b = 1u;
        tl_set_pad(k, colour);
    }
    uint16_t backlight = (uint16_t)(48u * (c->brightness + 1u));
    for (uint32_t ch = 0; ch < TL_BUTTONS; ++ch) tl_set_button(ch, backlight);
}
/* LED test: all pads red, green, blue (1 s each: checks byte order), then each pad
 * in chain order (80 ms), then each button channel (400 ms), numbers on screen. */
static bool led_test(const tc_controls *c, uint32_t now, view *v) {
    uint32_t t = now - test_start;
    uint32_t max = levels[c->brightness % TC_BRIGHTNESS_LEVELS];
    if (t < 3000u) {
        uint32_t phase = t / 1000u;
        for (uint32_t k = 0; k < TL_PADS; ++k)
            tl_set_pad(k, scaled(phase == 0u ? 255 : 0, phase == 1u ? 255 : 0, phase == 2u ? 255 : 0, max));
        v->test_phase = 1; v->test_item = (uint8_t)phase;
    } else if (t < 3000u + 80u * TL_PADS) {
        uint32_t i = (t - 3000u) / 80u;
        for (uint32_t k = 0; k < TL_PADS; ++k) tl_set_pad(k, k == i ? scaled(255, 255, 255, max) : (tl_rgb){0, 0, 0});
        v->test_phase = 2; v->test_item = (uint8_t)i;
    } else if (t < 3000u + 80u * TL_PADS + 400u * TL_BUTTONS) {
        uint32_t i = (t - 3000u - 80u * TL_PADS) / 400u;
        for (uint32_t k = 0; k < TL_PADS; ++k) tl_set_pad(k, (tl_rgb){0, 0, 0});
        for (uint32_t ch = 0; ch < TL_BUTTONS; ++ch) tl_set_button(ch, ch == i ? 4095u : 0u);
        v->test_phase = 3; v->test_item = (uint8_t)i;
    } else return false;
    return true;
}
bool ui_init(void) {
    bool display = td_init();
    (void)tl_init();
    td_text(0, 0, "THEORYBOARD", 2);
    td_text(0, 24, "STARTING...", 1);
    return display;
}
void ui_service(tc_controls *c, const tp_engine *e, bool usb, uint32_t now) {
    view v;
    memset(&v, 0, sizeof v); /* padding too: the redraw test compares bytes */
    v.sequence = c->event_sequence; v.changes = c->changes; v.action = c->last_action;
    v.source = c->last_source; v.index = c->last_index; v.function = c->last_function;
    v.ui = c->ui; v.menu = c->menu; v.edit = c->edit_value; v.have_chord = c->have_chord;
    v.last_chord = c->last_chord; v.usb = usb; v.chord = c->chord[c->last_chord % 48u];
    v.clocked = tm_clock_active(now); v.preview = sx_preview(now, NULL) != 0u;
    if (c->led_test) {
        if (!test_start) test_start = now ? now : 1u;
        if (!led_test(c, now, &v)) {
            c->led_test = false; test_start = 0; forced = true;
            for (uint32_t ch = 0; ch < TL_BUTTONS; ++ch) tl_set_button(ch, 0u);
        }
    }
    if (!c->led_test) pad_colours(c, e, now);
    if (forced || memcmp(&v, &shown, sizeof v)) {
        forced = false; shown = v;
        td_clear();
        if (c->led_test) {
            static const char *const colours[3] = {"ALL RED","ALL GREEN","ALL BLUE"};
            td_text(0, 0, "LED TEST", 2);
            start();
            if (v.test_phase == 1u) put(colours[v.test_item % 3u]);
            else { put(v.test_phase == 2u ? "PAD LED " : "BUTTON LED "); put_number(v.test_item); }
            td_text(0, 28, buffer, v.test_phase == 1u ? 1u : 2u);
        } else if (c->ui == TC_UI_PLAY) draw_play(c, usb, v.clocked);
        else draw_menu(c);
    }
    td_service();
    tl_service();
}
