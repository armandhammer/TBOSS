/* SPDX-License-Identifier: GPL-3.0-or-later - the lights (ui.h): the pads' colors in each scheme, the buttons'
 * backlights and the LED test. */
#include "ui_internal.h"
/* TLC5947 channel behind each centre button F0..F13 and the five navigation buttons,
 * from the original firmware's light tables (the 0.6.1 octave order). */
static const uint8_t panel_channels[TC_FUNCTIONS] = {7,1,13,12,2,0,14,3,4,5,6,9,10,11};
static const uint8_t nav_channels[5] = {8,15,16,17,18};
#define PANEL_ON 4095u
#define PANEL_DIM 614u /* 15 % (0.6.1: 30 %) */
#define NAV_LEVEL 2867u /* 70 % (0.6.3; before, the dim backlight of every channel) */
/* Brightness 1..8 in sixteenths of the LEDs' steps: 4, 8, 14, 22, 32, 44, 58 and 74 of 255 at full. */
static const uint16_t levels[TC_BRIGHTNESS_LEVELS] = {64,128,224,352,512,704,928,1184};
ui_light ui_scaled(uint32_t r, uint32_t g, uint32_t b, uint32_t max) {
    return (ui_light){(uint16_t)(r * max / 255u), (uint16_t)(g * max / 255u), (uint16_t)(b * max / 255u)};
}
ui_light ui_part(ui_light c, uint32_t pct) {
    return (ui_light){(uint16_t)(c.r * pct / 100u), (uint16_t)(c.g * pct / 100u), (uint16_t)(c.b * pct / 100u)};
}
tl_rgb ui_led(ui_light c) {
    tl_rgb out = {(uint8_t)(c.r / 16u), (uint8_t)(c.g / 16u), (uint8_t)(c.b / 16u)};
    if (out.r || out.g || out.b) return out;
    if (!c.r && !c.g && !c.b) return (tl_rgb){0, 0, 1}; /* dark */
    uint32_t top = c.r > c.g ? c.r : c.g; if (c.b > top) top = c.b;
    out.r = 2u * c.r >= top ? 1u : 0u; out.g = 2u * c.g >= top ? 1u : 0u; out.b = 2u * c.b >= top ? 1u : 0u; /* dim, still lit */
    return out;
}
/* 2.4.0, Out of scale (controls.h TC_OUT_): a note outside the scale (semis above the root); Default's AUTO is the
 * note's own color faint, Root+5th's near-dark, the 1.9.0 schemes' their role's color faint, all at Dim's share
 * (AUTO: 10 %; Root+5th's near-dark from Dim's share as 10 % is to it). */
static tl_rgb note_color(const tc_controls *c, uint32_t semis);
static ui_light out_of_scale(const tc_controls *c, uint32_t semis, uint32_t max) {
    uint32_t level = max * tc_dim(c, 10u) / 100u, percent;
    tl_rgb h;
    switch (c->out_scale) {
    case TC_OUT_WHITE: return ui_scaled(255, 255, 255, level);
    case TC_OUT_WARM: h = ui_palette[13]; return ui_scaled(h.r, h.g, h.b, level);
    case TC_OUT_COLOR: h = note_color(c, semis); return ui_scaled(h.r, h.g, h.b, level);
    case TC_OUT_DARK: case TC_OUT_OFF: return (ui_light){0, 0, 0}; /* dark as an unlit pad; OFF: unlit (ui_pad_colors) */
    default:
        if (c->led_scheme == TC_LEDS_ROOT_FIFTH) return ui_scaled(10, 10, 12, max * tc_dim(c, 10u) / 10u);
        h = c->led_scheme == TC_LEDS_RAINBOW ? note_color(c, semis) : ui_role(c, ui_scheme_role(c->led_scheme, semis, c->tonic + semis, &percent));
        return ui_scaled(h.r, h.g, h.b, level);
    }
}
static uint32_t incoming_classes(void) {
    uint32_t set = 0;
    for (uint32_t p = 0; p < 128u; ++p) if (tm.notes[p]) set |= 1u << (p % 12u);
    return set;
}
/* Rainbow: C red, then every semitone 30 degrees further round the color wheel. */
const tl_rgb ui_hues[12] = {{255,0,0},{255,90,0},{255,190,0},{160,255,0},{0,255,0},{0,255,130},
    {0,220,255},{0,110,255},{0,0,255},{60,0,255},{170,0,255},{255,0,100}};
/* 1.9.0: the palette a look's roles pick from: the 12 hues, white, warm white, gray and off. */
const tl_rgb ui_palette[TC_PALETTE] = {{255,0,0},{255,90,0},{255,190,0},{160,255,0},{0,255,0},{0,255,130},
    {0,220,255},{0,110,255},{0,0,255},{60,0,255},{170,0,255},{255,0,100},{255,255,255},{255,245,140},{40,40,50},{0,0,0}};
/* Warm white: 2.4.0 {255,245,140}, chosen on the board from five (board test L3); before {255,150,60}, which the
 * LEDs show pumpkin orange (L1); {255,200,130} looked pink (L2). */
/* Each role's built-in color (controls.h TC_LOOK_): Default's notes are the hues; Root+5th; Rainbow's seven
 * columns (the hues of C mode 12's columns 1, 2, 4, 5, 9, 7 and 12: red, orange, yellow, green, dark blue,
 * light blue, magenta, then again from red across the other five, 1.5.0); then the 1.9.0 schemes. */
const tl_rgb ui_look_builtin[TC_LOOK_ROLES] = {
    {255,0,0},{255,90,0},{255,190,0},{160,255,0},{0,255,0},{0,255,130},{0,220,255},{0,110,255},{0,0,255},{60,0,255},
    {170,0,255},{255,0,100},
    {255,70,0},{0,90,255},{40,40,50},{160,0,255},{0,255,60},                    /* root, 5th, other, edited, MIDI in */
    {255,0,0},{255,90,0},{160,255,0},{0,255,0},{0,0,255},{0,220,255},{255,0,100}, /* Rainbow's columns */
    {200,190,170},{20,20,70},{255,70,0},                                       /* Piano: natural, sharp, root */
    {255,70,0},{255,190,0},{0,90,255},{40,40,50},                              /* Triad: root, 3rd, 5th, other */
    {0,200,80},{255,170,0},{255,30,0},                                         /* Function: tonic, sub, dominant */
    {255,70,0},{0,60,255}};                                                    /* Octaves: root, other */
/* A role's color in c's look: its palette color, else its built-in one. */
tl_rgb ui_role(const tc_controls *c, uint32_t role) {
    uint8_t v = role < TC_LOOK_ROLES ? c->look_roles[role] : 0u;
    return v && v <= TC_PALETTE ? ui_palette[v - 1u] : ui_look_builtin[role % TC_LOOK_ROLES];
}
/* The color of the note semis above the root (Default's, and the guide's and hints' in every scheme). */
static tl_rgb note_color(const tc_controls *c, uint32_t semis) {
    return ui_role(c, TC_LOOK_NOTES + (c->color_by == TC_COLOR_BY_DEGREE ? semis : c->tonic + semis) % 12u);
}
/* 1.9.0 schemes: the role of the note semis above the root (pitch class pc), and how bright (percent). */
uint32_t ui_scheme_role(uint8_t scheme, uint32_t semis, uint32_t pc, uint32_t *percent) {
    /* I, iii, vi tonic; ii, IV (and b2, #4) subdominant; V, vii (and b7) dominant */
    static const uint8_t function[12] = {0,1,1,0,0,1,1,2,0,0,2,2};
    semis %= 12u;
    *percent = 100u;
    switch (scheme) {
    case TC_LEDS_PIANO:
        return (uint32_t)TC_LOOK_PIANO + (!semis ? TC_PIANO_ROOT : (0x54Au >> pc % 12u) & 1u ? TC_PIANO_SHARP : TC_PIANO_NATURAL);
    case TC_LEDS_TRIAD:
        if (semis == 3u || semis == 4u) return (uint32_t)TC_LOOK_TRIAD + TC_TRIAD_THIRD;
        if (semis == 7u) return (uint32_t)TC_LOOK_TRIAD + TC_TRIAD_FIFTH;
        if (semis) { *percent = 35u; return (uint32_t)TC_LOOK_TRIAD + TC_TRIAD_OTHER; }
        return (uint32_t)TC_LOOK_TRIAD + TC_TRIAD_ROOT;
    case TC_LEDS_FUNCTION: if (semis) *percent = 70u; return (uint32_t)TC_LOOK_FUNCTION + function[semis];
    default: if (semis) *percent = 30u; return (uint32_t)TC_LOOK_OCTAVES + (semis ? TC_OCTAVES_OTHER : TC_OCTAVES_ROOT);
    }
}
void ui_pad_colors(const tc_controls *c, const tp_engine *e, uint32_t now) {
    uint32_t max = levels[c->brightness % TC_BRIGHTNESS_LEVELS];
    tp_layout layout = tc_layout(c);
    tc_live_chord live; tc_live_current(layout, &live);
    uint8_t melody_scale = tc_melody_scale(layout); /* 2.3.0: Melody follows */
    bool modes = layout.chords == TC_CHORDS_MODES;
    uint32_t incoming = c->midi_leds ? incoming_classes() : 0u;
    uint8_t preview_root = 0;
    uint16_t preview = sx_preview(now, &preview_root);
    /* Progression guide (0.11.0): the playing chord's pad white, the next one pulsing (once a
     * second), on the right half the playing chord's notes bright and the others dim. */
    bool fixed = c->led_scheme == TC_LEDS_ALL_RAINBOW; /* Rainbow: nothing else shows */
    bool roles = c->led_scheme >= TC_LEDS_PIANO;     /* 1.9.0: Piano, Triad, Function, Octaves */
    const sq_prog *playing = tp_seq_prog(e); /* 2.0.0: a chain's link */
    bool guide = c->seq_guide && e->seq_running && playing->count && !preview && !fixed;
    uint32_t cur = TL_PADS, next = TL_PADS, tones = 0u;
    /* Up and down once a second in 11 steps of 50 ms: the LEDs change 20 times a second, not every pass. */
    uint32_t rise = now % 1000u < 500u ? now % 500u : 500u - now % 500u, pulse = 30u + 7u * (rise / 50u);
    /* Theory hints (1.0.0): after a chord, the usual next chords: the strongest pulsing, the
     * others steady, the remaining chord pads dim. Not while a progression's guide shows. */
    uint64_t also = 0u, strong = c->hints && c->have_chord && !guide && !preview && !fixed ? tc_hints(layout, c->last_chord, &also) : 0u;
    /* Pivot chords (1.4.0), before the hints: the chords shared with the target key, those leading
     * best to its V pulsing. */
    if (c->pivot && !guide && !preview && !fixed) strong = tc_pivots(layout, c->pivot, &also);
    if (guide) {
        if (e->seq_plays != SQ_RESTS) cur = playing->entries[e->seq_entry % playing->count].pad;
        if (playing->count > 1u) next = playing->entries[(e->seq_entry + 1u) % playing->count].pad;
        for (uint32_t i = 0; i < e->keys[TP_SEQ].captured.count; ++i) tones |= 1u << (e->keys[TP_SEQ].captured.notes[i] % 12u);
    }
    for (uint32_t k = 0; k < TL_PADS; ++k) {
        tp_position p; (void)tp_position_of(k, &p);
        ui_light step;
        if (c->ui == TC_UI_STEPS && !preview && ui_step_color(c, e, k, max, pulse, &step)) {
            tl_set_pad(tc_physical(c, k), ui_led(step));
            continue;
        }
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
                in_scale = live_area || tc_in_scale(melody_scale, semis);
                bright = live_area ? own.notes[0] % 12u == live.root : semis == 0u;
            }
        }
        tc_chord edit = c->chord[k % 48u];
        ui_light color;
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
        bool rainbow = c->led_scheme == TC_LEDS_RAINBOW, unlit = false;
        const tp_key *key = &e->keys[k];
        if (key->sensed) { /* touched: white, 40..100 % with pressure (aftertouch shows too) */
            uint32_t level = max * (40u + 60u * key->pressure / 127u) / 100u;
            color = ui_scaled(255, 255, 255, level ? level : 1u);
        }
        else if (key->admitted) color = ui_scaled(255, 255, 255, rainbow || fixed || roles ? max * 35u / 100u : max);
        else if (preview) {
            /* Editor keyboard on both halves: column n = n semitones above the editor's root. */
            bool in = ((uint32_t)preview >> p.column) & 1u;
            tl_rgb h = ui_hues[(preview_root + p.column) % 12u];
            color = !in ? ui_scaled(30, 30, 30, max) : p.column == 0u ? ui_scaled(255, 255, 255, max) : ui_scaled(h.r, h.g, h.b, max);
            tl_set_pad(tc_physical(c, k), ui_led(color));
            continue;
        }
        else if (fixed) { /* column n: one of the seven colors (the Bass layout: pair n, 1.7.0) */
            uint32_t column = p.side && c->melody_layout == TP_MELODY_BASS ? p.column / 2u : p.column;
            tl_rgb h = ui_role(c, TC_LOOK_COLUMNS + column % 12u % 7u);
            color = ui_scaled(h.r, h.g, h.b, column ? max * 60u / 100u : max);
        }
        else if (drum) { /* the drum pad: amber, the kick (bottom left) brightest */
            color = ui_scaled(255, 110, 0, (k % 4u == 3u && (k % 48u) / 4u == 8u) ? max : max * 45u / 100u);
        }
        else if (empty) color = ui_scaled(6, 6, 8, max);
        else if (!in_scale) { color = out_of_scale(c, semis, max); unlit = c->out_scale == TC_OUT_OFF; } /* 2.4.0: Out of scale, Dim */
        else if (roles) { /* 1.9.0: the note's role; as Default: MIDI in white, silent dim, edited paler */
            uint32_t percent;
            tl_rgb h = ui_role(c, ui_scheme_role(c->led_scheme, semis, c->tonic + semis, &percent));
            bool edited = !p.side && (c->chord_set < TC_CHORDS_FULL || modes) && (edit.lower || edit.sus || edit.add);
            if (lit) color = ui_scaled(255, 255, 255, max * 55u / 100u);
            else if (muted) color = ui_scaled(h.r, h.g, h.b, max * tc_dim(c, 15u) / 100u);
            else if (edited) color = ui_scaled((h.r + 255u) / 2u, (h.g + 255u) / 2u, (h.b + 255u) / 2u, max);
            else color = ui_scaled(h.r, h.g, h.b, max * percent / 100u);
        }
        else if (rainbow) {
            tl_rgb h = note_color(c, semis);
            bool edited = !p.side && (c->chord_set < TC_CHORDS_FULL || modes) && (edit.lower || edit.sus || edit.add);
            if (lit) color = ui_scaled(255, 255, 255, max * 55u / 100u);
            else if (muted) color = ui_scaled(h.r, h.g, h.b, max * tc_dim(c, 15u) / 100u);
            else if (edited) color = ui_scaled((h.r + 255u) / 2u, (h.g + 255u) / 2u, (h.b + 255u) / 2u, max);
            else color = ui_scaled(h.r, h.g, h.b, bright ? max : max * 60u / 100u);
        }
        else if (lit) { tl_rgb h = ui_role(c, TC_LOOK_R5 + TC_R5_MIDI); color = ui_scaled(h.r, h.g, h.b, max); }
        else if (muted) color = ui_scaled(10, 10, 12, max * tc_dim(c, 15u) / 15u);
        else { /* Root+5th: its roles */
            uint32_t role = (uint32_t)(!p.side && (edit.lower || edit.sus || edit.add) ? TC_R5_EDITED :
                            semis == 0u ? TC_R5_ROOT : semis == 7u ? TC_R5_FIFTH : TC_R5_OTHER);
            tl_rgb h = ui_role(c, TC_LOOK_R5 + role);
            color = ui_scaled(h.r, h.g, h.b, max);
        }
        /* 2.3.0, Modes: on rows 2..4 a chord the top row's scale also has is dim; the borrowed ones stand out */
        if (modes && !p.side && p.row && !fixed && !empty && !lit && !key->sensed && !key->admitted &&
            !tc_pad_borrowed(layout, k, edit))
            color = ui_part(color, tc_dim(c, 25u));
        if (guide && !key->sensed && !key->admitted) {
            tl_rgb h = note_color(c, semis);
            if (k == cur) color = ui_scaled(255, 255, 255, max * 60u / 100u);
            else if (k == next) color = ui_scaled(h.r, h.g, h.b, max * pulse / 100u);
            else if (p.side && !drum && tones) {
                if ((tones >> ((c->tonic + semis) % 12u)) & 1u) color = ui_scaled(h.r, h.g, h.b, max);
                else color = ui_part(color, tc_dim(c, 25u));
            }
        }
        if ((strong || also) && !p.side && !empty && !key->sensed && !key->admitted && (!c->have_chord || k != c->last_chord % 48u)) {
            tl_rgb h = note_color(c, semis);
            if ((strong >> k) & 1u) color = ui_scaled(h.r, h.g, h.b, max * pulse / 100u);
            else if ((also >> k) & 1u) color = ui_scaled(h.r, h.g, h.b, max * 70u / 100u);
            else color = ui_part(color, tc_dim(c, 25u));
        }
        /* Chords on Right: each pad's light on the half it plays from; Out of scale OFF: the LEDs off, not dark (2.4.0) */
        tl_set_pad(tc_physical(c, k), unlit && !color.r && !color.g && !color.b ? (tl_rgb){0, 0, 0} : ui_led(color));
    }
    uint16_t level[TL_BUTTONS];
    for (uint32_t ch = 0; ch < TL_BUTTONS; ++ch) level[ch] = (uint16_t)(48u * (c->brightness + 1u));
    /* 2.4.0, Dim: the panel's buttons that are off move 2 % a step of Dim (the overlay already dims them), 15 % at Dim
     * 15 % as at AUTO: Dim 5 % 11 %, 10 % 13 %, 15 % 15 % ... 50 % 23 % */
    uint16_t dim = (uint16_t)(c->dim ? PANEL_ON * (11u + 2u * (c->dim % TC_DIMS - 1u)) / 100u : PANEL_DIM);
    for (uint32_t f = 0; f < TC_FUNCTIONS; ++f) level[panel_channels[f]] = tc_panel_active(c, f) ? PANEL_ON : dim;
    for (uint32_t n = 0; n < 5u; ++n) level[nav_channels[n]] = NAV_LEVEL;
    for (uint32_t ch = 0; ch < TL_BUTTONS; ++ch) tl_set_button(ch, level[ch]);
}
/* LED test: all pads red, green, blue (1 s each: checks byte order), then each pad
 * in chain order (80 ms), then each button channel (400 ms), numbers on screen. */
static uint32_t test_start;
bool ui_led_test(const tc_controls *c, uint32_t now, ui_view *v) {
    if (!test_start) test_start = now ? now : 1u;
    uint32_t t = now - test_start;
    uint32_t max = levels[c->brightness % TC_BRIGHTNESS_LEVELS];
    if (t < 3000u) {
        uint32_t phase = t / 1000u;
        for (uint32_t k = 0; k < TL_PADS; ++k)
            tl_set_pad(k, ui_led(ui_scaled(phase == 0u ? 255 : 0, phase == 1u ? 255 : 0, phase == 2u ? 255 : 0, max)));
        v->test_phase = 1; v->test_item = (uint8_t)phase;
    } else if (t < 3000u + 80u * TL_PADS) {
        uint32_t i = (t - 3000u) / 80u;
        for (uint32_t k = 0; k < TL_PADS; ++k) tl_set_pad(k, k == i ? ui_led(ui_scaled(255, 255, 255, max)) : (tl_rgb){0, 0, 0});
        v->test_phase = 2; v->test_item = (uint8_t)i;
    } else if (t < 3000u + 80u * TL_PADS + 400u * TL_BUTTONS) {
        uint32_t i = (t - 3000u - 80u * TL_PADS) / 400u;
        for (uint32_t k = 0; k < TL_PADS; ++k) tl_set_pad(k, (tl_rgb){0, 0, 0});
        for (uint32_t ch = 0; ch < TL_BUTTONS; ++ch) tl_set_button(ch, ch == i ? 4095u : 0u);
        v->test_phase = 3; v->test_item = (uint8_t)i;
    } else { test_start = 0; return false; }
    return true;
}
