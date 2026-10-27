/* SPDX-License-Identifier: GPL-3.0-or-later - every screen ui.c draws, on a fake 128 x 64 display:
 * the play screen in every scale and key with the longest chord names and every bottom line,
 * every message, the page list, every settings page with every item and every value, the pad
 * test, I/O test, LED test and the web editor's preview, and (1.5.0) the step screen for every
 * lane kind, length, rate, channel, page, held step and value, step record and playhead. Each piece
 * of text must use only
 * characters the font has (src/tb/font.c), stay on the screen, and not overlap other text. */
#include "../../src/tb/settings.h"
#include "../../src/tb/sequence.h"
#include "../../src/tb/midi_in.h"
#include "../../src/tb/sysex.h"
#include "../../src/tb/flash_store.h"
#include "../../src/tb/chord_name.h"
#include "../../src/tb/display.h"
#include "../../src/tb/leds.h"
#include "../../src/tb/jack.h"
#include "../../src/tb/nor.h"
#include "../../src/tb/ui.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
volatile ts_abi ts_status;
static uint32_t flash[FS_SIZE / 4u];
const uint8_t *fs_base(void) { return (const uint8_t *)flash; }
bool fs_erase(void) { memset(flash, 0xFF, sizeof flash); return true; }
bool fs_program(uint32_t offset, const uint32_t *words, uint32_t count) {
    for (uint32_t i = 0; i < count; ++i) flash[offset / 4u + i] &= words[i];
    return true;
}
bool nr_present(void) { return false; }
void nr_id(uint8_t id[3]) { id[0] = id[1] = id[2] = 0u; }
uint32_t nr_size(void) { return 0u; }
bool nr_read(uint32_t a, uint8_t *out, uint32_t n) { (void)a; (void)out; (void)n; return false; }
bool tl_init(void) { return true; }
void tl_set_pad(uint32_t pad, tl_rgb c) { (void)pad; (void)c; }
void tl_set_button(uint32_t channel, uint16_t level) { (void)channel; (void)level; }
void tl_service(void) {}
uint32_t jh_levels(void) { return 5u; }
bool td_init(void) { return true; }
void td_pixel(uint32_t x, uint32_t y, bool on) { (void)x; (void)y; (void)on; }
static uint32_t inverted_y = UINT32_MAX; /* the row being edited (its value is drawn inverted) */
void td_invert(uint32_t x0, uint32_t y0, uint32_t x1, uint32_t y1) { (void)x0; (void)x1; (void)y1; inverted_y = y0 + 1u; }
void td_service(void) {}
/* The fake display keeps every piece of text drawn since the last clear. */
typedef struct { uint32_t x0, x1, y0, y1; char text[48]; } piece;
static piece pieces[64];
static uint32_t count, screens, texts, longest;
static char what[96];
static uint32_t problems;
/* Each problem is reported once (by its text); the sweep goes on and fails at the end. */
static char seen[64][48];
static uint32_t nseen;
static bool first_time(const char *text) {
    for (uint32_t i = 0; i < nseen; ++i) if (!strcmp(seen[i], text)) return false;
    if (nseen < 64u) strncpy(seen[nseen++], text, 47);
    return true;
}
void td_clear(void) { count = 0u; screens++; inverted_y = UINT32_MAX; }
/* The text drawn at the start of row y from column x on (the edited value, a label). */
static const char *text_at(uint32_t y, uint32_t min_x) {
    for (uint32_t i = 0; i < count; ++i) if (pieces[i].y0 == y && pieces[i].x0 >= min_x) return pieces[i].text;
    return "";
}
uint32_t td_text(uint32_t x, uint32_t y, const char *text, uint32_t scale) {
    uint32_t n = (uint32_t)strlen(text);
    texts++;
    if (n > longest && scale == 1u) longest = n;
    for (const char *s = text; *s; ++s)
        if (!strchr(td_font_chars, *s)) { problems++; if (first_time(text)) printf("%s: '%s' has '%c', which the font can't draw\n", what, text, *s); }
    if (!n) return x;
    piece p = {x, x + 6u * scale * n, y, y + 8u * scale, ""};
    strncpy(p.text, text, sizeof p.text - 1u);
    if (p.x1 > TD_WIDTH || p.y1 > 64u) { problems++; if (first_time(text)) printf("%s: '%s' at %u,%u leaves the screen\n", what, text, x, y); }
    for (uint32_t i = 0; i < count; ++i) {
        const piece *q = &pieces[i];
        if (p.x0 < q->x1 && q->x0 < p.x1 && p.y0 < q->y1 && q->y0 < p.y1) {
            problems++; if (first_time(text)) printf("%s: '%s' at %u,%u overlaps '%s' at %u,%u\n", what, text, x, y, q->text, q->x0, q->y0);
        }
    }
    if (count < 64u) pieces[count++] = p;
    return p.x1;
}
static tp_engine e;
static ts_scan scan;
static uint32_t now = 1000u;
static void draw(const char *label) {
    snprintf(what, sizeof what, "%s", label);
    tc_state.event_sequence++; /* something changed: redraw */
    ui_service(&tc_state, &e, &scan, true, now);
}
static void play_screens(void) {
    /* Twelve user scales with names of the full twelve characters. */
    for (uint8_t s = 0; s < TC_USER_SCALES; ++s) {
        char name[13]; snprintf(name, sizeof name, "MMMMMMMMMM%02u", s);
        assert(tc_set_user_scale(s, 0xAB5u, name));
    }
    tc_state.ui = TC_UI_PLAY;
    for (uint8_t pos = 0; pos < TC_ALL_SCALES; ++pos) {
        uint8_t scale = tc_scale_at(pos);
        for (uint8_t tonic = 0; tonic < 12u; tonic += 5u) {
            tp_layout l = {tonic, TP_MELODY_BASE, TP_CHORD_BASE, scale, TP_MELODY_SCALE, 0u, 0u};
            if (!tc_set_layout(&tc_state, &e, l)) continue;
            tc_state.preset = (uint8_t)(pos % 9u); /* 8: none */
            tc_state.have_chord = true;
            for (uint8_t k = 3u; k < 48u; k += 4u) {
                tc_state.last_chord = k;
                tc_state.chord[k] = (tc_chord){(uint8_t)(k % 8u), (uint8_t)(2u * (k % 3u)), (uint8_t)(k % 8u)};
                char label[64]; snprintf(label, sizeof label, "play: scale %u, key %u, pad %u", scale, tonic, k);
                draw(label);
                tc_state.chord[k] = (tc_chord){0, 0, 0};
            }
        }
    }
    /* Every bottom line: arpeggiator, strum, note repeat, velocity and pressure, hold and pedal. */
    tc_set_layout(&tc_state, &e, (tp_layout){11u, TP_MELODY_BASE, TP_CHORD_BASE, 0u, TP_MELODY_SCALE, 0u, 0u});
    for (uint8_t a = 1; a < TC_ARP_MODES; ++a) for (uint8_t r = 0; r < TC_ARP_RATES; ++r) {
        tc_state.arp = a; tc_state.arp_rate = r; tc_state.tempo = (uint8_t)(r * 8u); draw("arp line");
    }
    tc_state.arp = 0u;
    for (uint8_t m = 1; m < TC_STRUM_MODES; ++m) for (uint8_t s = 0; s < TC_STRUM_SPEEDS; ++s) {
        tc_state.strum = m; tc_state.strum_speed = s; draw("strum line");
    }
    tc_state.strum = 0u;
    for (uint8_t r = 1; r < TC_NOTE_REPEATS; ++r) { tc_state.note_repeat = r; draw("note repeat line"); }
    tc_state.note_repeat = 0u;
    /* 1.6.0: harmony, echo and mono in every combination (the short form when the long one is too long). */
    for (uint8_t h = 0; h < TC_HARMONIES; ++h) for (uint8_t t = 0; t < TC_ECHO_TIMES; ++t) for (uint8_t m = 0; m < 2u; ++m) {
        tc_state.harmony = h; tc_state.echo = t; tc_state.mono = m; draw("effects line");
    }
    tc_state.harmony = 0u; tc_state.echo = 0u; tc_state.mono = 0u;
    /* 1.4.0: the pivot line for every target after every chord pad (sharps, minor keys, diminished
     * chords), with hold and sustain; the capture line. */
    for (uint8_t t = 1; t < TC_PIVOTS; ++t) for (uint8_t k = 0; k < 48u; k += 1u) {
        tc_state.pivot = t; tc_state.have_chord = true; tc_state.last_chord = k;
        tc_state.hold = (uint8_t)(k & 1u); tc_state.sustain = (k & 2u) != 0u;
        tm.have_tick = (k & 4u) != 0u; tm.last_tick_ms = now; /* 1.7.0: EXT too */
        char label[48]; snprintf(label, sizeof label, "pivot line: target %u after pad %u", t, k);
        draw(label);
    }
    tc_state.pivot = 0u; tc_state.hold = 0u; tc_state.sustain = false; tm.have_tick = false;
    for (uint8_t n = 0; n < 13u; ++n) {
        tc_state.capturing = true;
        if (n) tc_capture_chord(&tc_state, (uint8_t)(n % 12u), 0x495u); /* x9: the longest names */
        draw("capture line");
    }
    for (uint8_t v = 0; v < TC_VEL_MODES; ++v) for (uint8_t w = 0; w < TC_VEL_MODES; ++w) for (uint8_t at = 0; at < TC_AT_MODES; ++at) {
        tc_state.vel_mode = v; tc_state.melody_vel = w; tc_state.at_chord = at; tc_state.at_melody = (uint8_t)(TC_AT_MODES - 1u - at);
        draw("velocity/pressure line");
    }
    tc_state.hold = 1u; tc_state.sustain = true;
    for (uint32_t clock = 0; clock < 2u; ++clock) { /* 1.7.0: and EXT, a clock coming in (OCT left out when needed) */
        tm.have_tick = clock != 0u; tm.last_tick_ms = now;
        for (uint8_t tonic = 0; tonic < 12u; ++tonic) {
            tc_set_layout(&tc_state, &e, (tp_layout){tonic, TP_MELODY_BASE, TP_CHORD_BASE, 0u, TP_MELODY_SCALE, 0u, 0u});
            tc_state.chord_base = TP_CHORD_BASE + TP_OCTAVE_RANGE; tc_state.melody_base = TP_MELODY_BASE + TP_OCTAVE_RANGE; draw("octaves, hold, sus");
            tc_state.chord_base = TP_CHORD_BASE - TP_OCTAVE_RANGE; tc_state.melody_base = TP_MELODY_BASE - TP_OCTAVE_RANGE; draw("low octaves");
        }
    }
    tm.have_tick = false;
    tc_set_layout(&tc_state, &e, (tp_layout){11u, TP_MELODY_BASE, TP_CHORD_BASE, 0u, TP_MELODY_SCALE, 0u, 0u});
    tc_state.chord_base = TP_CHORD_BASE; tc_state.melody_base = TP_MELODY_BASE; tc_state.hold = 0u; tc_state.sustain = false;
    ui_service(&tc_state, &e, &scan, false, now); /* USB: WAITING */
}
static void messages(void) {
    for (uint32_t ui = 0; ui < 2u; ++ui) {
        tc_state.ui = ui ? TC_UI_MENU : TC_UI_PLAY; tc_state.group = 0u; tc_state.menu = tc_group_items[0][0];
        for (uint8_t a = 0; a <= TC_STEP_PAST; ++a) {
            tc_state.last_action = a; tc_state.preset = 7u; tc_state.prog_slot = 7u; tc_state.tempo = TC_TEMPOS - 1u;
            tc_state.pivot = tc_state.capture_key = 23u; tc_state.capture_slot = 7u; /* 1.4.0: A# MINOR */
            tc_state.last_function = 13u; tc_state.last_index = 11u; tc_state.last_source = 4u;
            sq.total = SQ_UNITS_MAX;
            char label[48]; snprintf(label, sizeof label, "message %u on the %s", a, ui ? "menu" : "play screen");
            draw(label);
            tc_state.chord_set = (uint8_t)TC_CHORDS_FULL; draw(label); /* 1.3.0 */
            tc_state.chord_set = (uint8_t)TC_CHORDS_GRID; draw(label); tc_state.chord_set = 0u;
        }
    }
    /* 1.0.0: what every centre-button action says, with chord sets of full-length names (and
     * the Full layout and the chord grid, 1.3.0). */
    for (uint8_t set = 0; set < TC_CHORD_SETS; ++set) {
        tc_chord_set cs; memset(&cs, 0, sizeof cs);
        cs.column[0] = (tc_set_chord){0x891u, 0u}; snprintf(cs.name, sizeof cs.name, "WWWWWWWWWW%02u", set);
        assert(tc_chord_set_put(set, &cs));
    }
    for (uint32_t ui = 0; ui < 2u; ++ui) for (uint8_t b = 0; b <= TC_ACTIONS; ++b) for (uint8_t on = 0; on < 4u; ++on) {
        tc_state.ui = ui ? TC_UI_MENU : TC_UI_PLAY; tc_state.last_action = TC_BUTTON; tc_state.last_button = b;
        tc_state.shift_latched = on; tc_state.strum = on ? 1u : 0u; tc_state.voice_lead = on ? 1u : 0u;
        tc_state.fold = on ? 1u : 0u; tc_state.hints = on ? 1u : 0u;
        tc_state.chord_set = on == 3u ? TC_CHORDS_GRID : on == 2u ? TC_CHORDS_FULL : on ? TC_CHORD_SETS : 0u; tc_state.preset = 7u; /* 2, 3: Full, grid (1.3.0) */
        char label[48]; snprintf(label, sizeof label, "button action %u (%u) on the %s", b, on, ui ? "menu" : "play screen");
        draw(label);
    }
    tc_state.shift_latched = false; tc_state.strum = 0u; tc_state.voice_lead = 0u; tc_state.fold = 0u; tc_state.hints = 0u;
    tc_state.chord_set = 0u;
    for (uint8_t set = 0; set < TC_CHORD_SETS; ++set) { tc_chord_set none; memset(&none, 0, sizeof none); assert(tc_chord_set_put(set, &none)); }
    sq.total = 0u; tc_state.last_action = TC_NONE; tc_state.pivot = 0u; tc_state.capturing = false;
}
static void settings_pages(void) {
    tc_state.ui = TC_UI_GROUPS;
    for (uint8_t g = 0; g < TC_GROUPS; ++g) { tc_state.group = g; draw("page list"); }
    tc_state.have_chord = true; tc_state.last_chord = 3u;
    /* The pedal's Note shows only while Mode is Note (1.5.0): Note mode shows every item. */
    uint8_t shown[TC_GROUP_MAX];
    assert(tc_page_items(&tc_state, TC_GROUP_PEDAL, shown) == tc_group_size(TC_GROUP_PEDAL) - 1u);
    tc_state.pedal_mode = TC_PEDAL_NOTE;
    assert(tc_page_items(&tc_state, TC_GROUP_PEDAL, shown) == tc_group_size(TC_GROUP_PEDAL));
    for (uint8_t g = 0; g < TC_GROUPS; ++g) for (uint8_t i = 0; i < tc_group_size(g); ++i) {
        uint8_t item = tc_group_items[g][i];
        if (item == TC_MENU_LED_TEST || item == TC_MENU_IO_TEST || item == TC_MENU_PAD_TEST) continue; /* their own pages */
        tc_state.group = g; tc_state.menu = item;
        tc_state.ui = TC_UI_MENU; tc_state.last_action = TC_NONE;
        char label[64]; snprintf(label, sizeof label, "page %u item %u", g, item);
        draw(label);
        /* The page shows as many items as fit (4), also at its end. */
        uint32_t rows = 0, size = tc_group_size(g);
        for (uint32_t k = 0; k < count; ++k) rows += pieces[k].x0 == 8u && pieces[k].y0 >= 11u;
        assert(rows == (size < 4u ? size : 4u));
        /* Every value shows as its own text. */
        static char values[256][48];
        tc_state.ui = TC_UI_EDIT;
        for (uint32_t v = 0; v < tc_menu_values(item); ++v) {
            tc_state.edit_value = (uint8_t)v;
            snprintf(label, sizeof label, "page %u item %u value %u", g, item, v);
            draw(label);
            snprintf(values[v], sizeof values[v], "%s", text_at(inverted_y, 9u));
            assert(values[v][0]);
            for (uint32_t u = 0; u < v; ++u)
                if (!strcmp(values[u], values[v])) { printf("%s: values %u and %u both show '%s'\n", label, u, v, values[v]); assert(0); }
        }
    }
    tc_state.pedal_mode = TC_PEDAL_OFF;
}
/* 1.5.0: the step screen. */
static void step_screens(void) {
    tc_state.ui = TC_UI_STEPS;
    uint32_t drawn = 0;
    for (uint8_t lane = 0; lane < LN_LANES; ++lane) for (uint8_t kind = 0; kind < LN_KINDS; ++kind) {
        ln_lane *l = &ln.lanes[lane];
        tc_state.lane = lane; l->kind = kind;
        for (uint32_t length = 1u; length <= LN_STEPS; length += length < 8u ? 1u : 9u)
            for (uint32_t rate = 0; rate < LN_RATES; ++rate) {
                ln_set_length(l, length); ln_set_rate(l, rate);
                l->channel = (uint8_t)((length + rate) % LN_CHANNELS); l->mute = (uint8_t)(rate & 1u);
                tc_state.step_page = (uint8_t)(rate & 1u); tc_state.step_rec = (rate % 3u) == 0u;
                tc_state.step_cursor = (uint8_t)(LN_STEPS - 1u - rate); tc_state.step_held = LN_NONE;
                e.lane_running = (rate & 2u) != 0u; e.lane_step[lane] = (uint8_t)(length - 1u);
                char label[64]; snprintf(label, sizeof label, "steps: lane %u kind %u length %u rate %u", lane, kind, length, rate);
                draw(label); drawn++;
            }
        /* Every value a step of this kind can hold, held and picked, in a few keys and scales (the
         * longest chord names). */
        ln_set_length(l, LN_STEPS);
        for (uint32_t key = 0; key < 12u; key += 5u) for (uint32_t m = 0; m < 3u; ++m) {
            tp_layout lay = tc_layout(&tc_state); lay.tonic = (uint8_t)key; lay.mode = tc_mode_scale(&tc_state, 2u + 3u * m);
            if (!tc_set_layout(&tc_state, &e, lay)) continue;
            for (uint32_t v = 0; v < 128u; v += kind == LN_DRUMS ? 1u : 3u) {
                ln_step s; memset(&s, 0, sizeof s); s.on = true; s.value = (uint8_t)v; s.edit = (uint8_t)(v % 8u | (v % 3u) << 3 | (v % 7u) << 5);
                l->steps[63] = s; tc_state.brush[lane] = s; tc_state.step_held = (uint8_t)(v & 1u ? 63u : LN_NONE);
                char label[64]; snprintf(label, sizeof label, "steps: lane %u kind %u value %u key %u", lane, kind, v, key);
                draw(label); drawn++;
            }
        }
        tc_state.step_held = 62u; l->steps[62].on = false; draw("steps: an off step held");
        /* 1.7.0: every field of the held steps, every value it stores (and out-of-range ones), 1 to 64 held. */
        for (uint8_t field = 0; field < TC_FIELDS; ++field) for (uint32_t v = 0; v < 128u; ++v) {
            ln_step s; memset(&s, 0, sizeof s); s.on = (v % 5u) != 0u; s.value = (uint8_t)(v % 48u);
            s.velocity = (uint8_t)v; s.gate = (uint8_t)(v & 15u); s.tie = (v & 1u) != 0u; s.probability = (uint8_t)(v & 15u);
            s.ratchet = (uint8_t)(v & 7u); s.condition = (uint8_t)(v & 31u);
            l->steps[40] = s; tc_state.step_held = 40u; tc_state.step_field = field;
            tc_state.step_mask = v < 64u ? ((uint64_t)1u << 40) | ((uint64_t)1u << v) : ~(uint64_t)0;
            char label[64]; snprintf(label, sizeof label, "steps: lane %u kind %u field %u value %u", lane, kind, field, v);
            draw(label); drawn++;
        }
        tc_state.step_mask = 0u; tc_state.step_field = 0u;
    }
    ln_clear(&ln); tc_state.step_held = LN_NONE; tc_state.step_rec = false; tc_state.step_page = 0u; e.lane_running = false;
    tc_set_layout(&tc_state, &e, (tp_layout){0u, TP_MELODY_BASE, TP_CHORD_BASE, 0u, TP_MELODY_SCALE, 0u, 0u});
    assert(drawn > 2000u);
}
static void slot_marks(void) { /* progression and preset slots say EMPTY when they are */
    tc_state.ui = TC_UI_EDIT; tc_state.group = TC_GROUP_SEQUENCE; tc_state.menu = TC_MENU_SEQ_SLOT;
    sq_present = 0x01u;
    tc_state.edit_value = 0u; draw("slot 1"); assert(!strcmp(text_at(inverted_y, 9u), "1"));
    tc_state.edit_value = 1u; draw("slot 2"); assert(!strcmp(text_at(inverted_y, 9u), "2 EMPTY"));
    sq_present = 0u;
}
static void special_pages(void) {
    tc_state.ui = TC_UI_EDIT; tc_state.group = TC_GROUP_PADS;
    tc_state.menu = TC_MENU_PAD_TEST;
    for (uint32_t v = 0; v <= 4095u; v += 455u) {
        for (uint32_t k = 0; k < TS_KEYS; ++k) scan.values[k] = (v + 97u * k) % 4096u;
        scan.valid = true; scan.frame++; now += 100u;
        draw("pad test");
    }
    /* 1.5.0: each pad's own resting level. The strongest pad rests at 1200: its thresholds are the
     * settings above it (1.7.0); the rows show each pad's rest; the bottom line the board's range. */
    e.config = tc_pad_config(&tc_state);
    for (uint32_t k = 0; k < TS_KEYS; ++k) { e.rest[k] = (uint16_t)(300u + k); scan.values[k] = 300u; }
    e.rested = true;
    e.rest[51] = 1200u; scan.values[51] = 2734u; scan.values[55] = 812u; scan.values[47] = 640u;
    now += 100u; draw("pad test, own rests");
    assert(!strcmp(text_at(0u, 0u), "PAD 1900/1500/3700"));
    assert(!strcmp(text_at(11u, 0u), "R C1 R4  2734 * 1200") && !strcmp(text_at(20u, 0u), "R C2 R4   812    355"));
    assert(!strcmp(text_at(56u, 0u), "<BACK  REST 300-1200"));
    scan.values[51] = 300u; scan.values[52] = 1300u; now += 100u; draw("pad test, as set");
    assert(!strcmp(text_at(0u, 0u), "PAD 1052/652/2852") && !strcmp(text_at(11u, 0u), "R C2 R1  1300 *  352"));
    memset(e.rest, 0, sizeof e.rest);
    tc_state.group = TC_GROUP_ABOUT; tc_state.menu = TC_MENU_IO_TEST;
    for (uint32_t k = 0; k < TS_KEYS; k += 7u) { e.keys[k].sensed = true; now += 10u; draw("I/O test"); }
    memset(e.keys, 0, sizeof e.keys);
    tc_state.ui = TC_UI_PLAY; tc_state.led_test = true;
    for (uint32_t i = 0; i < 600000u && tc_state.led_test; i += 50u) { now += 50u; snprintf(what, sizeof what, "LED test"); ui_service(&tc_state, &e, &scan, true, now); }
    assert(!tc_state.led_test);
    /* The web editor's preview of a user scale: every slot, every root, a few masks. */
    static const uint16_t masks[3] = {0xAB5u, 0xFFFu, 0x0A9u};
    for (uint8_t slot = 0; slot < TC_USER_SCALES; ++slot) for (uint8_t root = 0; root < 12u; root += 5u) for (uint32_t m = 0; m < 3u; ++m) {
        uint8_t msg[11] = {0xF0, 0x7D, 0x54, 0x42, SX_PREVIEW, (uint8_t)(masks[m] & 127u), (uint8_t)(masks[m] >> 7), root, slot, 0, 0xF7};
        uint32_t sum = 0; for (uint32_t i = 4; i < 9u; ++i) sum += msg[i];
        msg[9] = (uint8_t)((128u - sum % 128u) % 128u);
        (void)sx_handle(msg, sizeof msg, &tc_state, &e, now);
        char label[32]; snprintf(label, sizeof label, "editor preview U%u", slot + 1u);
        draw(label);
    }
    now += 6000u; draw("after the preview");
}
int main(void) {
    setvbuf(stdout, NULL, _IONBF, 0);
    fs_erase(); tc_init(&tc_state); (void)st_load(&tc_state); tm_reset();
    e.layout = tc_layout(&tc_state); e.state = TK_ACTIVE;
    (void)ui_init();
    play_screens(); messages(); settings_pages(); step_screens(); slot_marks(); special_pages();
    if (problems) { printf("%u screen problems\n", problems); assert(!problems); }
    printf("PASS: screens: %u screens, %u pieces of text (longest %u characters): every scale and key with "
           "chord edits, every bottom line, every message on the play screen and in the menu, every page, item "
           "and value, pad test, I/O test, LED test, the editor's preview, the step screen; all drawable, on screen, no overlaps\n",
           screens, texts, longest);
    return 0;
}
