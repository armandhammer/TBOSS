/* SPDX-License-Identifier: GPL-3.0-or-later - OLED screens and LED colors.
 * Play screen: root, scale, mode button or preset, the chord name of the last chord
 * pad, the last action (every centre button names its function, so the board
 * documents itself), octaves/hold, then arpeggiator, strum or velocity/pressure.
 * Settings: a list of pages, each a scrolling list of items with values, and a bottom
 * line with the buttons' meaning or the result of the last action.
 * Pads (rainbow, the default): each pad the color of its note's pitch class, root columns
 * brightest; a touched pad white, brighter with pressure; sounding (held, latched, arp
 * input) softer white; edited chords paler; notes arriving over MIDI white.
 * Pads (root+5th): root orange, fifth blue, other steps dim, edited chords purple,
 * sounding pads white, MIDI in green.
 * Web editor preview (either scheme): both halves become 12 semitone columns from the
 * editor's root: root column white, scale notes in their rainbow color at full
 * brightness, other notes nearly dark; the display names the slot and the notes. */
#include "ui.h"
#include "display.h"
#include "leds.h"
#include "chord_name.h"
#include "midi_in.h"
#include "sysex.h"
#include "version.h"
#include "jack.h"
#include "sequence.h"
#include <string.h>
static const char *const roots[12] = {"C","C#","D","D#","E","F","F#","G","G#","A","A#","B"};
static const char *const mode_labels[12] = {"5","12","M1","M2","M3","M4","M5","M6","M7","U1","U2","U3"};
static const char *const menu_items[TC_MENU_ITEMS] = {"SCALE","HOLD","STRUM","STRUM TIME","ARP",
    "ARP RATE","ARP OCTAVES","ARP GATE","TEMPO","CHORD CH","MELODY CH","CHORD AT","CHORD VEL",
    "LOAD PRESET","SAVE PRESET","MIDI IN LEDS","BRIGHTNESS","LED TEST","VERSION","TRS OUT","MODE",
    "TYPE","I/O TEST","COLORS","MELODY AT","VOICING","SPREAD","ADD 7TH","ADD 9TH","ADD 6TH","SUS",
    "STRUM PAD","RESET CHORD","RESET ALL","LAYOUT","BASS CH","DRUM CH","REPEAT","CUTOFF","RHYTHM","SWING",
    "SYNC START","LATCH","RESTRIKE","CLOCK OUT","TRANSPORT","NOTE","ARROWS","MELODY OCT","FAMILY","CAPO",
    "FEEL TIME","FEEL VEL","SOUND","LIVE CH","LIVE IN","FOLD","VOICE LEAD","NOTE REPEAT","PAD TEST",
    "TRIGGER","RELEASE","FULL","MELODY VEL","TARGET","PROGRESSION","UNIT","RECORD","CLEAR","GUIDE",
    "CHORD SET","HINTS","SCALE LOCK","VOICING I","VOICING II","VOICING III","GUITAR","SPREAD I","SPREAD II",
    "SPREAD III","OCTAVE -3","OCTAVE -2","OCTAVE -1","OCTAVE 0","OCTAVE +1","OCTAVE +2","OCTAVE +3","RESET ALL"};
/* What a centre button can do (controls.h TC_ACT_), as Settings > Buttons shows it. */
static const char *const action_names[TC_ACTIONS] = {"VOICING 1","VOICING 2","VOICING 3","GUITAR","SPREAD 1",
    "SPREAD 2","SPREAD 3","OCT -3","OCT -2","OCT -1","OCT 0","OCT +1","OCT +2","OCT +3","SHIFT","HOLD","ARP",
    "STRUM","LEAD","FOLD","HINTS","SUSTAIN","ALL OFF","TAP TEMPO","PRESET >","PRESET <","SCALE >","SCALE <",
    "MEL OCT +","MEL OCT -","PLAY","RECORD","ADD 7TH","ADD 9TH","ADD 6TH","SUS2","SUS4","CHORD SET","NOTHING"};
static const char *const group_names[TC_GROUPS] = {"PLAY","CHORDS","LAYOUT","STRUM","ARPEGGIATOR","MIDI",
    "PEDAL","PRESETS","LIGHTS","SEQUENCE","PADS","BUTTONS","ABOUT"};
static const char *const pedal_modes[TC_PEDAL_MODES] = {"OFF","SUSTAIN","HOLD","SOSTENUTO","SOFT",
    "PORTAMENTO","PLAY/STOP","NOTE"};
static const char *const voicings[TC_VOICINGS] = {"OFF","I","II","III","GUITAR"};
static const char *const romans[TC_SPREADS] = {"OFF","I","II","III"};
static const char *const layouts[TP_MELODY_LAYOUTS] = {"SCALE","CHROMATIC","FOURTHS","DRUMS","LIVE NOTES"};
static const char *const tick_names[] = {"1/96","1/48","1/32","1/24","1/16"};
static const char *const repeat_names[TC_REPEATS] = {"OFF","1/4","1/8","1/16","1/32"};
static const char *const cutoff_names[TC_CUTOFFS] = {"OFF","1/32","1/16","1/8","1/4","1/2","1/1"};
static const char *const sus_names[3] = {"OFF","SUS2","SUS4"};
static const char *const pedal_types[TC_PEDAL_TYPES] = {"AUTO","NORMAL","INVERTED"};
static const char *const led_schemes[TC_LED_SCHEMES] = {"RAINBOW","ROOT+5TH"};
static const char *const at_names[TC_AT_MODES] = {"OFF","POLY AT","CHANNEL AT","CC1 MOD","CC2 BREATH",
    "CC11 EXPR","CC74 FILT","PITCH BEND"};
static const char *const at_short[TC_AT_MODES] = {"OFF","POLY","CHAN","CC1","CC2","CC11","CC74","BEND"};
static const char *const vel_names[TC_VEL_MODES] = {"NORMAL","FIXED 100","LIGHT","FIRM"};
static const char *const vel_short[TC_VEL_MODES] = {"DYN","FIX","LGT","FRM"};
static const char *const strum_names[TC_STRUM_MODES] = {"OFF","UP","DOWN","UP+DOWN"};
static const char *const arp_names[TC_ARP_MODES] = {"OFF","UP","DOWN","UP-DOWN","RANDOM","AS PLAYED",
    "CHORDS"};
static const char *const rate_names[TC_ARP_RATES] = {"1/4","1/8","1/8T","1/16","1/16T","1/32"};
static const char *const off_on[2] = {"OFF","ON"};
static const char *const arrow_names[TC_ARROWS_MODES] = {"SCALE","MELODY OCTAVE","PRESET","TEMPO","SEQUENCER"};
static const char *const targets[TC_PEDAL_TARGETS] = {"BOTH","CHORDS","MELODY"};
static const char *const unit_names[SQ_UNIT_KINDS] = {"1 BEAT","1/2 BAR","1 BAR","2 BARS"};
/* TLC5947 channel behind each centre button F0..F13 and the five navigation buttons,
 * from the original firmware's light tables (the 0.6.1 octave order). */
static const uint8_t panel_channels[TC_FUNCTIONS] = {7,1,13,12,2,0,14,3,4,5,6,9,10,11};
static const uint8_t nav_channels[5] = {8,15,16,17,18};
#define PANEL_ON 4095u
#define PANEL_DIM 614u /* 15 % (0.6.1: 30 %) */
#define NAV_LEVEL 2867u /* 70 % (0.6.3; before, the dim backlight of every channel) */
static const uint8_t levels[TC_BRIGHTNESS_LEVELS] = {4,8,14,22,32,44,58,74};
typedef struct {
    uint32_t sequence, changes, action, source, index, function;
    uint8_t ui, menu, edit, group, have_chord, last_chord, usb, test_phase, test_item, clocked, preview;
    uint8_t sustain, levels, pedal, held, first; /* levels..first only while the I/O test page is open */
    uint8_t preview_root, preview_slot;
    uint16_t preview_mask;
    uint32_t trs_sent;
    uint32_t refresh; /* pad test: redraw every 100 ms */
    uint8_t seq_entry, seq_total, seq_running, recording, seq_unit; /* progression shown on the play screen */
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
#define COLUMNS (TD_WIDTH / 6u) /* characters across the screen at size 1 */
/* The unit of the progression playing now, 0 .. total - 1. */
static uint32_t seq_unit(const tp_engine *e) {
    return sq.total ? e->seq_pos / sq_unit_ticks(sq.unit) % sq.total : 0u;
}
static void button_text(const tc_controls *c);
static void last_action(const tc_controls *c) {
    start();
    switch (c->last_action) {
    case TC_PANEL:
        if (c->last_function <= TC_FN_GUITAR) {
            if (c->chord_voicing == TC_VOICING_GUITAR) put("GUITAR VOICING");
            else { put("VOICING "); put(voicings[c->chord_voicing % TC_VOICINGS]); }
        } else if (c->last_function <= TC_FN_SPREAD3) { put("SPREAD "); put(romans[c->chord_spread % TC_SPREADS]); }
        else {
            put("OCTAVES");
            for (uint32_t o = 0; o < 7u; ++o) if ((c->chord_octaves >> o) & 1u) {
                put(o < 3u ? " -" : o > 3u ? " +" : " "); put_number(o < 3u ? 3u - o : o - 3u);
            }
        }
        break;
    case TC_TRANSPORT: put(c->transport ? "PLAY" : "STOP"); break;
    case TC_RESET: put("CHORD EDITS RESET"); break;
    case TC_SCALE_STEP: put("SCALE "); put(tc_scale_name(c->scale)); break;
    case TC_TEMPO_STEP: put("TEMPO "); put_number(tc_tempo_bpm(c->tempo)); put(" BPM"); break;
    case TC_FAVOURITE_STEP: {
        uint32_t at = 0;
        while (at < tc_favourite_count && tc_favourites[at] != c->scale) at++;
        put("FAV "); put_number(at + 1u); put("/"); put_number(tc_favourite_count); put(" "); put(tc_scale_name(c->scale));
        break;
    }
    case TC_CHORD_EDIT: put("CHORD EDITED"); break;
    case TC_OCTAVE: put("MELODY OCT "); note_name(c->melody_base + c->tonic); break;
    case TC_TONIC: put("ROOT "); put(roots[c->tonic % 12u]); break;
    case TC_MODE: put("MODE "); put(mode_labels[c->last_index % 12u]); put(" "); put(tc_scale_name(c->scale)); break;
    case TC_TODO_FUNCTION:
        if (c->last_source == TC_SCALE) { put(mode_labels[c->last_index % 12u]); put(" IS EMPTY"); }
        else put("PLAY A CHORD 1ST");
        break;
    case TC_TODO_CONTEXT: put("CLOSE THE MENU FIRST"); break;
    case TC_REJECT: put("OUT OF RANGE"); break;
    case TC_CONFLICT: put("ONE BUTTON AT A TIME"); break;
    case TC_SETTING: put("SETTING CHANGED"); break;
    case TC_HOLD: put(c->hold ? "HOLD ON" : "HOLD OFF"); break;
    case TC_ARP: put("ARP "); put(arp_names[c->arp % TC_ARP_MODES]); break;
    case TC_PRESET_LOADED: put("PRESET "); put_number(c->preset + 1u); put(" LOADED"); break;
    case TC_PRESET_SAVED: put("PRESET "); put_number(c->preset + 1u); put(" SAVED"); break;
    case TC_PRESET_EMPTY: put("NO PRESET THERE"); break;
    case TC_SEQ_RECORD: put("REC: TAP CHORD PADS"); break;
    case TC_SEQ_TAP: case TC_SEQ_UNDO: put("REC "); put_number(sq.total); put("/"); put_number(SQ_UNITS_MAX);
        if (c->last_action == TC_SEQ_UNDO) put(" UNDO");
        break;
    case TC_SEQ_FULL: put("PROGRESSION FULL"); break;
    case TC_SEQ_SAVED: put("PROGRESSION "); put_number(c->prog_slot + 1u); put(" SAVED"); break;
    case TC_SEQ_CLEARED: put("PROGRESSION "); put_number(c->prog_slot + 1u); put(" CLEARED"); break;
    case TC_SEQ_EMPTY: put("NO PROGRESSION YET"); break;
    case TC_SEQ_KEPT: put("NOTHING RECORDED"); break;
    case TC_SAVE_FAILED: put("NOT SAVED: RESTART"); break;
    case TC_REMOTE: put("CHANGED BY EDITOR"); break;
    case TC_PAD_CHOSEN: put("CHOSEN FOR EDITING"); break;
    case TC_SET_NO_EDITS: put(c->chord_set == TC_CHORDS_FULL ? "FULL CHORDS: NO EDITS" :
                              c->chord_set == TC_CHORDS_GRID ? "CHORD GRID: NO EDITS" : "SET CHORDS: NO EDITS"); break;
    case TC_SET_EMPTY: put("THAT SET IS EMPTY"); break;
    case TC_SHIFT_UNUSED: put("SHIFT: BUTTONS 1-8"); break;
    case TC_SEQ_BUSY: put("NOT WHILE RECORDING"); break;
    case TC_BUTTON: button_text(c); break;
    default: put("READY"); break;
    }
}
static void chord_set_text(uint8_t set) {
    if (!set) { put("SCALE CHORDS"); return; }
    if (set == TC_CHORDS_FULL) { put("FULL CHORDS"); return; }
    if (set == TC_CHORDS_GRID) { put("CHORD GRID"); return; }
    const tc_chord_set *s = tc_chord_set_get((uint8_t)(set - 1u));
    put("SET "); put_number(set);
    if (s && s->name[0]) { put(" "); put(s->name); }
}
/* What a centre button just did (TC_BUTTON). */
static void button_text(const tc_controls *c) {
    switch (c->last_button) {
    case TC_ACT_SHIFT: put(c->shift_latched ? "SHIFT LATCHED" : "SHIFT (TAP 2X: LATCH)"); break;
    case TC_ACT_STRUM: put(c->strum ? "STRUM ON" : "STRUM OFF"); break;
    case TC_ACT_VOICE_LEAD: put(c->voice_lead ? "VOICE LEAD ON" : "VOICE LEAD OFF"); break;
    case TC_ACT_FOLD: put(c->fold ? "FOLD ON" : "FOLD OFF"); break;
    case TC_ACT_HINTS: put(c->hints ? "HINTS ON" : "HINTS OFF"); break;
    case TC_ACT_CHORD_SET: chord_set_text(c->chord_set); break;
    case TC_ACT_PANIC: put("ALL NOTES OFF"); break;
    case TC_ACT_SUSTAIN: put(c->button_sustain ? "SUSTAIN ON" : "SUSTAIN OFF"); break;
    case TC_ACT_ADD7: case TC_ACT_ADD9: case TC_ACT_ADD6: case TC_ACT_SUS2: case TC_ACT_SUS4:
        put(action_names[c->last_button]); put(" WHILE HELD"); break;
    case TC_ACTIONS: put("BUTTONS RESET"); break;
    default: put(c->last_button < TC_ACTIONS ? action_names[c->last_button] : "?"); break;
    }
}
/* The chord name of progression step i, as it plays in the current key. */
static uint32_t step_name(const tc_controls *c, uint32_t i, char name[TN_MAX]) {
    if (i >= sq.count) { name[0] = 0; return 0u; }
    const sq_entry *en = &sq.entries[i];
    tp_layout l = sq_layout(&sq, tc_layout(c), en);
    tp_notes n;
    if (!tc_pad_with(l, en->pad, en->edit, &n)) { name[0] = 0; return 0u; }
    return tn_name(&n, tc_pad_root(l, en->pad), name);
}
static void draw_play(const tc_controls *c, const tp_engine *e, bool usb, bool clocked) {
    td_text(0, 0, roots[c->tonic % 12u], 2);
    td_text(30, 0, tc_scale_name(c->scale), 1);
    uint32_t button = 12u;
    for (uint32_t i = 0; i < 12u; ++i) if (tc_mode_scale(c, i) == c->scale) { button = i; break; }
    start(); if (button < 12u) { put("MODE "); put(mode_labels[button]); }
    if (c->preset < TC_PRESETS) { put(button < 12u ? "  P" : "PRESET "); put_number(c->preset + 1u); }
    if (c->chord_set == TC_CHORDS_FULL) put(length ? " FULL" : "FULL CHORDS");
    else if (c->chord_set == TC_CHORDS_GRID) put(length ? " GRID" : "CHORD GRID");
    else if (c->chord_set) { put(length ? "  S" : "CHORD SET "); put_number(c->chord_set); }
    td_text(30, 8, buffer, 1);
    char name[TN_MAX];
    uint32_t n = c->have_chord ? tn_pad(tc_layout(c), c->last_chord % 48u, name) : 0u;
    if (c->melody_layout == TP_MELODY_LIVE && c->live_from_midi && c->live_midi.count) {
        /* Live notes following MIDI in: the incoming chord's name. */
        tp_notes in = {{0}, 0};
        for (uint32_t i = 0; i < c->live_midi.count; ++i) in.notes[in.count++] = (uint8_t)(60u + c->live_midi.root + c->live_midi.tones[i]);
        n = tn_name(&in, c->live_midi.root, name);
    }
    if (e->seq_running && sq.count) n = step_name(c, e->seq_entry, name); /* the progression's chord */
    if (n) {
        td_text(0, n <= 10u ? 18u : 22u, name, n <= 10u ? 2u : 1u);
    } else td_text(0, 22, "PLAY A CHORD", 1);
    last_action(c); line(36);
    start(); put("OCT "); note_name(c->chord_base + c->tonic); put(" "); note_name(c->melody_base + c->tonic);
    if (c->hold) put(" HOLD");
    if (c->sustain) put(" SUS");
    line(46);
    start();
    if (!usb) put("USB: WAITING");
    else if (c->recording) { /* REC P1 12/64 Am: the last chord when it fits */
        char last[TN_MAX];
        put("REC P"); put_number(c->prog_slot + 1u); put(" "); put_number(sq.total); put("/"); put_number(SQ_UNITS_MAX);
        uint32_t n_last = sq.count ? step_name(c, sq.count - 1u, last) : 0u;
        if (n_last && length + 1u + n_last <= COLUMNS) { put(" "); put(last); }
    } else if (e->seq_running && sq.count) {
        /* P1 Am > F 3/8: the chord now, the next one, the unit playing of all. Names that
         * don't fit go first (the chord now is also the big name above), then the next. */
        char now_name[TN_MAX], next[TN_MAX];
        (void)step_name(c, e->seq_entry, now_name); (void)step_name(c, (e->seq_entry + 1u) % sq.count, next);
        for (uint32_t form = 0; form < 3u; ++form) {
            start(); put("P"); put_number(c->prog_slot + 1u); put(" ");
            if (form == 0u) { put(now_name); if (sq.count > 1u) { put(" > "); put(next); } put(" "); }
            else if (form == 1u && sq.count > 1u) { put("> "); put(next); put(" "); }
            put_number(seq_unit(e) + 1u); put("/"); put_number(sq.total);
            if (length <= COLUMNS) break;
        }
    } else if (c->arp) { /* ARP UP 1/16 120; without "ARP " when that doesn't fit (AS PLAYED 1/16T 200) */
        for (uint32_t form = 0; form < 2u; ++form) {
            start(); if (!form) put("ARP ");
            put(arp_names[c->arp % TC_ARP_MODES]); put(" "); put(rate_names[c->arp_rate % TC_ARP_RATES]);
            put(" "); if (clocked) put("EXT"); else put_number(tc_tempo_bpm(c->tempo));
            if (length <= COLUMNS) break;
        }
    } else if (c->strum) {
        put("STRUM "); put(strum_names[c->strum % TC_STRUM_MODES]); put(" ");
        if (c->strum_speed < TC_STRUM_MS) { put_number(tc_strum_ms[c->strum_speed]); put("MS"); }
        else put(tick_names[(c->strum_speed - TC_STRUM_MS) % (TC_STRUM_SPEEDS - TC_STRUM_MS)]);
    } else if (c->note_repeat) {
        put("REPEAT "); put(repeat_names[c->note_repeat % TC_REPEATS]); put(" ");
        if (clocked) put("EXT"); else put_number(tc_tempo_bpm(c->tempo));
    } else {
        put("V "); put(vel_short[c->vel_mode % TC_VEL_MODES]); put("/"); put(vel_short[c->melody_vel % TC_VEL_MODES]);
        put(" A "); put(at_short[c->at_chord % TC_AT_MODES]); put("/"); put(at_short[c->at_melody % TC_AT_MODES]);
    }
    line(56);
}
static void value_name(uint8_t item, uint8_t v) {
    start();
    switch (item) {
    case TC_MENU_SCALE: put(tc_scale_name(tc_scale_at(v))); if (!tc_scale_defined(tc_scale_at(v))) put(" EMPTY"); break;
    case TC_MENU_HOLD: case TC_MENU_MIDI_LEDS: case TC_MENU_TRS_OUT: case TC_MENU_ADD7: case TC_MENU_ADD9:
    case TC_MENU_ADD6: case TC_MENU_PAD_STRUM: case TC_MENU_ARP_SYNC: case TC_MENU_ARP_LATCH: case TC_MENU_LIVE:
    case TC_MENU_CLOCK_OUT: case TC_MENU_SOUND: case TC_MENU_FOLD: case TC_MENU_VOICE_LEAD: put(off_on[v & 1u]); break;
    case TC_MENU_NOTE_REPEAT: put(repeat_names[v % TC_REPEATS]); break;
    case TC_MENU_LIVE_CH: if (v) put_number(v); else put("MELODY"); break;
    case TC_MENU_LIVE_IN: if (!v) put("OFF"); else if (v > 16u) put("ALL"); else { put("CH "); put_number(v); } break;
    case TC_MENU_VOICING: put(voicings[v % TC_VOICINGS]); break;
    case TC_MENU_SPREAD: put(romans[v % TC_SPREADS]); break;
    case TC_MENU_SUS: put(sus_names[v % 3u]); break;
    case TC_MENU_RESET_CHORD: case TC_MENU_RESET_ALL: put("RESET"); break;
    case TC_MENU_LAYOUT: put(layouts[v % TP_MELODY_LAYOUTS]); break;
    case TC_MENU_BASS_CH: case TC_MENU_DRUM_CH: put_number(v + 1u); break;
    case TC_MENU_PAD_TRIGGER: put_number(100u * (TC_PAD_TRIGGER_MIN + v)); break;
    case TC_MENU_PAD_RELEASE: put_number(100u * (TC_PAD_RELEASE_MIN + v)); break;
    case TC_MENU_PAD_FULL: put_number(100u * (TC_PAD_FULL_MIN + v)); break;
    case TC_MENU_REPEAT: put(repeat_names[v % TC_REPEATS]); break;
    case TC_MENU_CUTOFF: put(cutoff_names[v % TC_CUTOFFS]); break;
    case TC_MENU_RHYTHM: {
        uint8_t r = tc_rhythms[v % TC_RHYTHMS];
        for (uint32_t i = 0; i < 8u; ++i) put(((uint32_t)r >> i) & 1u ? "X" : ".");
        break;
    }
    case TC_MENU_SWING: put_number(tc_swing_pct[v % TC_SWINGS]); put("%"); break;
    case TC_MENU_TRANSPORT: put(v ? "RUNNING" : "STOPPED"); break;
    case TC_MENU_PEDAL_NOTE: note_name(v); put(" "); put_number(v); break;
    case TC_MENU_PEDAL: put(pedal_modes[v % TC_PEDAL_MODES]); break;
    case TC_MENU_PEDAL_TYPE: put(pedal_types[v % TC_PEDAL_TYPES]); break;
    case TC_MENU_COLORS: put(led_schemes[v % TC_LED_SCHEMES]); break;
    case TC_MENU_STRUM: put(strum_names[v % TC_STRUM_MODES]); break;
    case TC_MENU_STRUM_SPEED:
        if (v < TC_STRUM_MS) { put_number(tc_strum_ms[v]); put(" MS"); }
        else put(tick_names[(v - TC_STRUM_MS) % (TC_STRUM_SPEEDS - TC_STRUM_MS)]);
        break;
    case TC_MENU_ARP: put(arp_names[v % TC_ARP_MODES]); break;
    case TC_MENU_ARP_RATE: put(rate_names[v % TC_ARP_RATES]); break;
    case TC_MENU_ARP_OCTAVES: put_number(v + 1u); break;
    case TC_MENU_ARP_GATE: put_number(tc_arp_gate_pct[v % TC_ARP_GATES]); put("%"); break;
    case TC_MENU_TEMPO: put_number(tc_tempo_bpm(v)); put(" BPM"); break;
    case TC_MENU_CHORD_CH: case TC_MENU_MELODY_CH: put_number(v + 1u); break;
    case TC_MENU_CHORD_AT: case TC_MENU_MELODY_AT: put(at_names[v % TC_AT_MODES]); break;
    case TC_MENU_VELOCITY: put(vel_names[v % TC_VEL_MODES]); break;
    case TC_MENU_LOAD: case TC_MENU_SAVE:
        put_number(v + 1u); if (!(tc_presets_present & (1u << (v % TC_PRESETS)))) put(" EMPTY");
        break;
    case TC_MENU_BRIGHTNESS: put_number(v + 1u); break;
    case TC_MENU_VERSION: put(TB_VERSION_LABEL); break;
    case TC_MENU_ARROWS: put(arrow_names[v % TC_ARROWS_MODES]); break;
    case TC_MENU_MELODY_VEL: put(vel_names[v % TC_VEL_MODES]); break;
    case TC_MENU_PEDAL_TARGET: put(targets[v % TC_PEDAL_TARGETS]); break;
    case TC_MENU_SEQ_SLOT: put_number(v + 1u); if (!(sq_present & (1u << (v % SQ_SLOTS)))) put(" EMPTY"); break;
    case TC_MENU_SEQ_UNIT: put(unit_names[v % SQ_UNIT_KINDS]); break;
    case TC_MENU_SEQ_GUIDE: case TC_MENU_HINTS: put(off_on[v & 1u]); break;
    case TC_MENU_CHORD_SET: {
        const tc_chord_set *s = v ? tc_chord_set_get((uint8_t)(v - 1u)) : 0;
        if (!v) put("SCALE");
        else if (v == TC_CHORDS_FULL) put("FULL");
        else if (v == TC_CHORDS_GRID) put("GRID");
        else if (!tc_chord_set_defined((uint8_t)(v - 1u))) { put_number(v); put(" EMPTY"); }
        else if (s && s->name[0] && !s->name[10]) put(s->name); /* names up to 10 characters fit beside the item */
        else { put("SET "); put_number(v); }
        break;
    }
    case TC_MENU_LOCK: if (v) { put("CH "); put_number(v); } else put("OFF"); break;
    case TC_MENU_BUTTONS_RESET: put("RESET"); break;
    case TC_MENU_SEQ_CLEAR: put("CLEAR"); break;
    case TC_MENU_FAMILY: put(tc_family_name(v)); break;
    case TC_MENU_CAPO: if (v) { put("FRET "); put_number(v); } else put("OFF"); break;
    case TC_MENU_FEEL_TIME: if (v) { put("0-"); put_number(tc_feel_ms[v % TC_FEEL_TIMES]); put(" MS"); } else put("OFF"); break;
    case TC_MENU_FEEL_VEL: if (v) { put("+-"); put_number(tc_feel_vel[v % TC_FEEL_VELS]); } else put("OFF"); break;
    case TC_MENU_MELODY_OCT: {
        uint32_t mid = TP_OCTAVE_RANGE / 12u;
        put(v < mid ? "-" : v > mid ? "+" : ""); put_number(v < mid ? mid - v : v - mid);
        break;
    }
    default:
        if (item >= TC_MENU_BUTTON1 && item < TC_MENU_BUTTON1 + TC_FUNCTIONS) put(v < TC_ACTIONS ? action_names[v] : "?");
        else put("START");
        break;
    }
}
/* Four rows; the list scrolls so the selection stays visible. */
#define ROWS 4u
static uint32_t first_row(uint32_t selected, uint32_t count) {
    uint32_t first = selected >= ROWS - 1u ? selected - (ROWS - 2u) : 0u;
    return count <= ROWS ? 0u : first > count - ROWS ? count - ROWS : first;
}
static void position_text(uint32_t at, uint32_t count) {
    start(); put_number(at + 1u); put("/"); put_number(count);
    td_text(128u - 6u * length, 0, buffer, 1);
}
static void draw_menu(const tc_controls *c) {
    uint8_t group = c->group % TC_GROUPS;
    if (c->ui == TC_UI_GROUPS) {
        td_text(0, 0, "SETTINGS", 1);
        position_text(group, TC_GROUPS);
        uint32_t first = first_row(group, TC_GROUPS);
        for (uint32_t row = 0; row < ROWS && first + row < TC_GROUPS; ++row) {
            uint32_t i = first + row, y = 11u + 10u * row;
            td_text(0, y, i == group ? ">" : " ", 1);
            td_text(8, y, group_names[i], 1);
            td_text(122, y, ">", 1);
        }
    } else {
        uint32_t count = tc_group_size(group), at = 0;
        for (uint32_t i = 0; i < count; ++i) if (tc_group_items[group][i] == c->menu) at = i;
        td_text(0, 0, group_names[group], 1);
        position_text(at, count);
        uint32_t first = first_row(at, count);
        for (uint32_t row = 0; row < ROWS && first + row < count; ++row) {
            uint8_t item = tc_group_items[group][first + row];
            uint32_t y = 11u + 10u * row;
            bool editing = item == c->menu && c->ui == TC_UI_EDIT;
            td_text(0, y, item == c->menu ? ">" : " ", 1);
            td_text(8, y, menu_items[item], 1);
            value_name(item, editing ? c->edit_value : tc_menu_value(c, item));
            uint32_t x = 128u - 6u * length;
            td_text(x, y, buffer, 1);
            if (editing) td_invert(x - 1u, y - 1u, 128u, y + 8u);
        }
    }
    /* Bottom line: what the buttons do here, or what the last action did. */
    switch (c->last_action) {
    case TC_NONE: case TC_PREVIOUS: case TC_NEXT: case TC_MENU: case TC_COMMIT: case TC_CANCEL:
        start();
        if (c->ui == TC_UI_EDIT) put("<CANCEL   O SET");
        else if (c->ui == TC_UI_MENU && c->menu == TC_MENU_VERSION) { put("<BACK   "); put(TB_VERSION_TAG); } /* the installer finds this tag */
        else if (c->ui == TC_UI_MENU && (c->menu == TC_MENU_LED_TEST || c->menu == TC_MENU_IO_TEST ||
                                         c->menu == TC_MENU_PAD_TEST)) put("<BACK   O START");
        else if (c->ui == TC_UI_MENU && (c->menu == TC_MENU_RESET_CHORD || c->menu == TC_MENU_RESET_ALL ||
                                         c->menu == TC_MENU_BUTTONS_RESET)) put("<BACK   O RESET");
        else if (c->ui == TC_UI_MENU && c->menu == TC_MENU_SEQ_RECORD) put("<BACK   O RECORD");
        else if (c->ui == TC_UI_MENU && c->menu == TC_MENU_SEQ_CLEAR) put("<BACK   O CLEAR");
        else if (c->ui == TC_UI_MENU && c->menu == TC_MENU_TRANSPORT) put(c->transport ? "<BACK   O STOP" : "<BACK   O PLAY");
        else if (c->ui == TC_UI_MENU) put("<BACK   O CHANGE");
        else put("<BACK   O OPEN");
        break;
    case TC_TODO_CONTEXT: start(); put("< TO LEAVE THE MENU"); break;
    default: last_action(c); break;
    }
    td_text(0, 56, buffer, 1);
}
/* I/O test: the pedal input and two alternatives, live, the test note on the TRS jack,
 * and the pads that read as pressed (held, or stuck: the engine keeps those silent). */
static void draw_io(const tc_controls *c, uint32_t pins, uint32_t sent, uint32_t held, uint32_t first) {
    static const char *const level[2] = {"LOW","HIGH"};
    td_text(0, 0, "I/O TEST", 1);
    start(); put("PEDAL PD13 "); put(level[pins & 1u]); put(c->pedal_pressed ? " DOWN" : " UP"); line(10);
    start(); put("PA10 "); put(level[(pins >> 1) & 1u]); put("  PD5 "); put(level[(pins >> 2) & 1u]); line(19);
    start(); put("TRS: C4 EVERY SECOND"); line(28);
    start(); put("SENT "); put_number(sent); put(" BYTES"); line(37);
    start(); put("PADS HELD "); put_number(held);
    tp_position p;
    if (held && tp_position_of(first, &p)) {
        put(p.side ? " R C" : " L C"); put_number(p.column + 1u); put(" R"); put_number(p.row + 1u);
    }
    line(46);
    td_text(0, 56, "<BACK", 1);
}
/* Pad test: the raw pad readings (0..4095, higher = harder) against the press and release
 * thresholds, so a player can see how hard a light touch reads and how much a press bleeds
 * onto the neighbouring pads. The peak is taken on every pass, the screen redraws 10x a second. */
static uint32_t pad_peak, pad_peak_pad;
static bool pad_touch;
static void pad_peaks(const tc_controls *c, const ts_scan *s) {
    const tk_config d = tc_pad_config(c);
    uint8_t top;
    ts_strongest(s->values, &top, 1u);
    uint32_t v = s->values[top];
    if (!pad_touch && v > d.release) { pad_touch = true; pad_peak = 0u; } /* a new touch */
    if (pad_touch && v > pad_peak) { pad_peak = v; pad_peak_pad = top; }
    if (v <= d.release) pad_touch = false; /* the peak stays shown until the next touch */
}
static void pad_name(uint32_t k) {
    tp_position p;
    if (!tp_position_of(k, &p)) return;
    put(p.side ? "R C" : "L C"); put_number(p.column + 1u); put(" R"); put_number(p.row + 1u);
}
static void pad_value(uint32_t v, uint32_t at) { /* right-aligned in 4 digits from character `at` */
    while (length < at) put(" ");
    for (uint32_t d = 1000u; d > 1u && v < d; d /= 10u) put(" ");
    put_number(v);
}
static void draw_pads(const tc_controls *c, const tp_engine *e, const ts_scan *s) {
    const tk_config d = tc_pad_config(c);
    /* Trigger / release / full as played (! = raised above the settings for this board's rest). */
    start(); put("PAD "); put_number(d.press); put("/"); put_number(d.release); put("/"); put_number(d.full_scale);
    if (tc_pad_limited(c)) put("!");
    line(0);
    uint8_t top[3];
    ts_strongest(s->values, top, 3u);
    for (uint32_t i = 0; i < 3u; ++i) {
        start(); pad_name(top[i]); pad_value(s->values[top[i]], 9u);
        if (s->values[top[i]] >= d.press) put(" *");
        line(11u + 9u * i);
    }
    start();
    if (pad_peak) { put("PEAK"); pad_value(pad_peak, 5u); put(" "); pad_name(pad_peak_pad); }
    else put("PEAK -");
    line(38);
    start(); put("LAST VEL ");
    uint32_t newest = TS_KEYS;
    for (uint32_t k = 0; k < TS_KEYS; ++k) if (e->admissions && e->keys[k].order == e->admissions) newest = k;
    if (newest < TS_KEYS) { put_number(e->keys[newest].velocity); put(" "); pad_name(newest); } else put("-");
    line(47);
    start(); put("<BACK      REST "); put_number(c->pad_rest); line(56);
}
/* Web editor preview: which slot, the root, the notes. */
static void draw_editor(uint16_t mask, uint8_t root, uint8_t slot) {
    td_text(0, 0, "WEB EDITOR", 1);
    if (slot < TC_USER_SCALES) { start(); put("U"); put_number(slot + 1u); td_text(128u - 12u * length, 0, buffer, 2); }
    uint32_t count = 0;
    for (uint32_t i = 0; i < 12u; ++i) count += ((uint32_t)mask >> i) & 1u;
    start(); put("ROOT "); put(roots[root % 12u]); put("  "); put_number(count); put(" NOTES"); line(18);
    /* The notes, six per line: up to 12 names of up to 2 letters. */
    uint32_t listed = 0, y = 30;
    start();
    for (uint32_t i = 0; i < 12u; ++i) {
        if (!(((uint32_t)mask >> i) & 1u)) continue;
        if (listed && listed % 6u == 0u) { line(y); y += 10u; start(); }
        put(roots[(root + i) % 12u]); put(" "); listed++;
    }
    line(y);
    td_text(0, 56, "PADS SHOW THE SCALE", 1);
}
static tl_rgb scaled(uint32_t r, uint32_t g, uint32_t b, uint32_t max) {
    return (tl_rgb){(uint8_t)(r * max / 255u), (uint8_t)(g * max / 255u), (uint8_t)(b * max / 255u)};
}
static uint32_t incoming_classes(void) {
    uint32_t set = 0;
    for (uint32_t p = 0; p < 128u; ++p) if (tm.notes[p]) set |= 1u << (p % 12u);
    return set;
}
/* Rainbow: C red, then every semitone 30 degrees further round the color wheel. */
static const tl_rgb hues[12] = {{255,0,0},{255,90,0},{255,190,0},{160,255,0},{0,255,0},{0,255,130},
    {0,220,255},{0,110,255},{0,0,255},{60,0,255},{170,0,255},{255,0,100}};
static void pad_colors(const tc_controls *c, const tp_engine *e, uint32_t now) {
    uint32_t max = levels[c->brightness % TC_BRIGHTNESS_LEVELS];
    tp_layout layout = tc_layout(c);
    tc_live_chord live; tc_live_current(layout, &live);
    uint32_t incoming = c->midi_leds ? incoming_classes() : 0u;
    uint8_t preview_root = 0;
    uint16_t preview = sx_preview(now, &preview_root);
    /* Progression guide (0.11.0): the playing chord's pad white, the next one pulsing (once a
     * second), on the right half the playing chord's notes bright and the others dim. */
    bool guide = c->seq_guide && e->seq_running && sq.count && !preview;
    uint32_t cur = TL_PADS, next = TL_PADS, tones = 0u;
    /* Up and down once a second in 11 steps of 50 ms: the LEDs change 20 times a second, not every pass. */
    uint32_t rise = now % 1000u < 500u ? now % 500u : 500u - now % 500u, pulse = 30u + 7u * (rise / 50u);
    /* Theory hints (1.0.0): after a chord, the usual next chords: the strongest pulsing, the
     * others steady, the remaining chord pads dim. Not while a progression's guide shows. */
    uint64_t also = 0u, strong = c->hints && c->have_chord && !guide && !preview ? tc_hints(layout, c->last_chord, &also) : 0u;
    if (guide) {
        cur = sq.entries[e->seq_entry % sq.count].pad;
        if (sq.count > 1u) next = sq.entries[(e->seq_entry + 1u) % sq.count].pad;
        for (uint32_t i = 0; i < e->keys[TP_SEQ].captured.count; ++i) tones |= 1u << (e->keys[TP_SEQ].captured.notes[i] % 12u);
    }
    for (uint32_t k = 0; k < TL_PADS; ++k) {
        tp_position p; (void)tp_position_of(k, &p);
        /* The note's pitch class: a chord pad's root, a melody pad's note (any layout). */
        uint32_t semis = (tc_pad_root(layout, k) + 12u - c->tonic) % 12u;
        bool empty = !p.side && tc_pad_empty(layout, k); /* a chord set's column without a chord */
        bool in_scale = true, drum = p.side && c->melody_layout == TP_MELODY_DRUMS && tp_drum_grid(k) == 2u;
        /* Live area: every pad a chord tone, the chord's root brightest. Silent chord pads dim. */
        bool live_area = tp_live_pad(layout, k), muted = !p.side && ((c->chord_mute >> (k % 48u)) & 1u);
        bool bright = semis == 0u;
        if (p.side) {
            tp_notes own;
            if (tc_pad(layout, k, &own) && own.count) {
                semis = (own.notes[0] + 12u - c->tonic) % 12u;
                in_scale = live_area || tc_in_scale(c->scale, semis);
                bright = live_area ? own.notes[0] % 12u == live.root : semis == 0u;
            }
        }
        tc_chord edit = c->chord[k % 48u];
        tl_rgb color;
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
        bool rainbow = c->led_scheme == TC_LEDS_RAINBOW;
        const tp_key *key = &e->keys[k];
        if (key->sensed) { /* touched: white, 40..100 % with pressure (aftertouch shows too) */
            uint32_t level = max * (40u + 60u * key->pressure / 127u) / 100u;
            color = scaled(255, 255, 255, level ? level : 1u);
        }
        else if (key->admitted) color = scaled(255, 255, 255, rainbow ? max * 35u / 100u : max);
        else if (preview) {
            /* Editor keyboard on both halves: column n = n semitones above the editor's root. */
            bool in = ((uint32_t)preview >> p.column) & 1u;
            tl_rgb h = hues[(preview_root + p.column) % 12u];
            color = !in ? scaled(30, 30, 30, max) : p.column == 0u ? scaled(255, 255, 255, max) : scaled(h.r, h.g, h.b, max);
            tl_set_pad(k, color);
            continue;
        }
        else if (drum) { /* the drum pad: amber, the kick (bottom left) brightest */
            color = scaled(255, 110, 0, (k % 4u == 3u && (k % 48u) / 4u == 8u) ? max : max * 45u / 100u);
        }
        else if (empty) color = scaled(6, 6, 8, max);
        else if (!in_scale) color = rainbow ? scaled(hues[(c->tonic + semis) % 12u].r, hues[(c->tonic + semis) % 12u].g,
                                                      hues[(c->tonic + semis) % 12u].b, max * 10u / 100u)
                                             : scaled(10, 10, 12, max);
        else if (rainbow) {
            tl_rgb h = hues[(c->tonic + semis) % 12u];
            bool edited = !p.side && c->chord_set < TC_CHORDS_FULL && (edit.lower || edit.sus || edit.add);
            if (lit) color = scaled(255, 255, 255, max * 55u / 100u);
            else if (muted) color = scaled(h.r, h.g, h.b, max * 15u / 100u);
            else if (edited) color = scaled((h.r + 255u) / 2u, (h.g + 255u) / 2u, (h.b + 255u) / 2u, max);
            else color = scaled(h.r, h.g, h.b, bright ? max : max * 60u / 100u);
        }
        else if (lit) color = scaled(0, 255, 60, max);
        else if (muted) color = scaled(10, 10, 12, max);
        else if (!p.side && (edit.lower || edit.sus || edit.add)) color = scaled(160, 0, 255, max);
        else if (semis == 0u) color = scaled(255, 70, 0, max);
        else if (semis == 7u) color = scaled(0, 90, 255, max);
        else color = scaled(40, 40, 50, max);
        if (guide && !key->sensed && !key->admitted) {
            tl_rgb h = hues[(c->tonic + semis) % 12u];
            if (k == cur) color = scaled(255, 255, 255, max * 60u / 100u);
            else if (k == next) color = scaled(h.r, h.g, h.b, max * pulse / 100u);
            else if (p.side && !drum && tones) {
                if ((tones >> ((c->tonic + semis) % 12u)) & 1u) color = scaled(h.r, h.g, h.b, max);
                else color = (tl_rgb){(uint8_t)(color.r / 4u), (uint8_t)(color.g / 4u), (uint8_t)(color.b / 4u)};
            }
        }
        if ((strong || also) && !p.side && !empty && !key->sensed && !key->admitted && k != c->last_chord % 48u) {
            tl_rgb h = hues[(c->tonic + semis) % 12u];
            if ((strong >> k) & 1u) color = scaled(h.r, h.g, h.b, max * pulse / 100u);
            else if ((also >> k) & 1u) color = scaled(h.r, h.g, h.b, max * 70u / 100u);
            else color = (tl_rgb){(uint8_t)(color.r / 4u), (uint8_t)(color.g / 4u), (uint8_t)(color.b / 4u)};
        }
        if (!color.r && !color.g && !color.b) color.b = 1u;
        tl_set_pad(k, color);
    }
    uint16_t level[TL_BUTTONS];
    for (uint32_t ch = 0; ch < TL_BUTTONS; ++ch) level[ch] = (uint16_t)(48u * (c->brightness + 1u));
    for (uint32_t f = 0; f < TC_FUNCTIONS; ++f) level[panel_channels[f]] = tc_panel_active(c, f) ? PANEL_ON : PANEL_DIM;
    for (uint32_t n = 0; n < 5u; ++n) level[nav_channels[n]] = NAV_LEVEL;
    for (uint32_t ch = 0; ch < TL_BUTTONS; ++ch) tl_set_button(ch, level[ch]);
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
    td_text(0, 0, "TBOSS", 2);
    td_text(0, 24, "VERSION " TB_VERSION_LABEL, 1);
    td_text(0, 40, "STARTING...", 1);
    return display;
}
void ui_service(tc_controls *c, const tp_engine *e, const ts_scan *s, bool usb, uint32_t now) {
    view v;
    memset(&v, 0, sizeof v); /* padding too: the redraw test compares bytes */
    v.sequence = c->event_sequence; v.changes = c->changes; v.action = c->last_action;
    v.source = c->last_source; v.index = c->last_index; v.function = c->last_function;
    v.ui = c->ui; v.menu = c->menu; v.edit = c->edit_value; v.group = c->group; v.have_chord = c->have_chord;
    v.last_chord = c->last_chord; v.usb = usb; v.chord = c->chord[c->last_chord % 48u];
    v.clocked = tm_clock_active(now); v.preview_mask = sx_preview(now, &v.preview_root);
    v.preview = v.preview_mask != 0u; v.preview_slot = v.preview ? sx_preview_slot() : 0xFFu; v.sustain = c->sustain;
    bool io = c->ui == TC_UI_EDIT && c->menu == TC_MENU_IO_TEST;
    if (io) {
        v.levels = (uint8_t)jh_levels(); v.pedal = c->pedal_pressed; v.trs_sent = sm_out.sent;
        for (uint32_t k = TL_PADS; k-- > 0u;) if (e->keys[k].sensed || e->keys[k].inhibited) { v.held++; v.first = (uint8_t)k; }
    }
    v.seq_entry = e->seq_entry; v.seq_total = sq.total; v.seq_running = e->seq_running; v.recording = c->recording;
    v.seq_unit = (uint8_t)seq_unit(e);
    bool pads = c->ui == TC_UI_EDIT && c->menu == TC_MENU_PAD_TEST;
    if (pads) { pad_peaks(c, s); v.refresh = now / 100u; } /* 10 redraws a second */
    else { pad_peak = 0u; pad_touch = false; }
    if (c->led_test) {
        if (!test_start) test_start = now ? now : 1u;
        if (!led_test(c, now, &v)) {
            c->led_test = false; test_start = 0; forced = true;
            for (uint32_t ch = 0; ch < TL_BUTTONS; ++ch) tl_set_button(ch, 0u);
        }
    }
    if (!c->led_test) pad_colors(c, e, now);
    if (forced || memcmp(&v, &shown, sizeof v)) {
        forced = false; shown = v;
        td_clear();
        if (c->led_test) {
            static const char *const colors[3] = {"ALL RED","ALL GREEN","ALL BLUE"};
            td_text(0, 0, "LED TEST", 2);
            start();
            if (v.test_phase == 1u) put(colors[v.test_item % 3u]);
            else { put(v.test_phase == 2u ? "PAD LED " : "BUTTON "); put_number(v.test_item); } /* fits at size 2 */
            td_text(0, 28, buffer, v.test_phase == 1u ? 1u : 2u);
        } else if (v.preview) draw_editor(v.preview_mask, v.preview_root, v.preview_slot);
        else if (io) draw_io(c, v.levels, v.trs_sent, v.held, v.first);
        else if (pads) draw_pads(c, e, s);
        else if (c->ui == TC_UI_PLAY) draw_play(c, e, usb, v.clocked);
        else draw_menu(c);
    }
    td_service();
    tl_service();
}
