/* SPDX-License-Identifier: GPL-3.0-or-later - 1.4.0 resets on the real code: Reset options on every
 * page that has options, About's Factory settings, and the older Reset chord, Reset all edits and
 * Buttons' Reset all, all behind a confirmation that opens on CANCEL. Nothing changes unless RESET
 * is chosen (up, then centre); a page's reset changes that page only; per-chord edits, a
 * progression's unit and saved content (presets, user scales, chord sets, progressions) stay; the
 * confirmation screens and messages. */
#include "../../src/tb/settings.h"
#include "../../src/tb/sequence.h"
#include "../../src/tb/midi_in.h"
#include "../../src/tb/flash_store.h"
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
static uint32_t inverted_y;
void td_invert(uint32_t x0, uint32_t y0, uint32_t x1, uint32_t y1) { (void)x0; (void)x1; (void)y1; inverted_y = y0 + 1u; }
void td_service(void) {}
/* The screen: every piece of text drawn since the last clear. */
typedef struct { uint32_t x, y; char text[40]; } piece;
static piece pieces[64];
static uint32_t count;
void td_clear(void) { count = 0u; inverted_y = UINT32_MAX; }
uint32_t td_text(uint32_t x, uint32_t y, const char *text, uint32_t scale) {
    if (count < 64u && text[0]) { pieces[count].x = x; pieces[count].y = y; snprintf(pieces[count].text, sizeof pieces[count].text, "%s", text); count++; }
    return x + 6u * scale * (uint32_t)strlen(text);
}
static const char *at(uint32_t x, uint32_t y) {
    for (uint32_t i = 0; i < count; ++i) if (pieces[i].x == x && pieces[i].y == y) return pieces[i].text;
    return "";
}

static tp_engine e;
static ts_scan scan;
static uint32_t now = 1000u;
static bool writer(const uint8_t p[4], uint32_t token) { (void)p; (void)token; return true; }
static const uint8_t addresses[4] = {0x35, 0x36, 0x38, 0x39};
static uint32_t raw[4];
static uint32_t client(uint8_t a) { for (uint32_t i = 0; i < 4u; ++i) if (addresses[i] == a) return i; assert(0); return 0; }
static struct bi_reply reader(void *ctx, uint8_t a, uint8_t reg, uint32_t gen, uint32_t stamp, uint32_t budget) {
    (void)ctx; (void)reg; (void)budget;
    uint32_t i = client(a);
    return (struct bi_reply){.status = 0, .count = 2, .generation = gen, .timestamp_ms = stamp, .elapsed_us = 500,
                             .data = {(uint8_t)raw[i], (uint8_t)(raw[i] >> 8)}};
}
static void run(uint32_t ms) {
    for (uint32_t i = 0; i < ms; ++i, ++now) {
        if (now % 10u == 0u) (void)tc_poll(&tc_state, &e, reader, NULL, now);
        if (now % 15u == 0u) {
            scan.frame++; scan.last_ms = now; scan.valid = true;
            if (e.state == TK_IDLE) (void)tc_arm(&e, &scan, now, true); else (void)tc_frame(&e, &scan, now);
        }
        tc_step(&e, now, true, writer);
        if (e.pending_id) tp_complete(&e, e.pending_id, 4u, true, now);
        st_requests(&tc_state, &e);
        st_service(&tc_state, now);
    }
}
enum { NAV_LEFT = 8, NAV_UP = 7, NAV_CENTRE = 6, NAV_DOWN = 1, NAV_RIGHT = 3 };
static void nav(uint8_t bit) { raw[1] |= 1u << bit; run(50); raw[1] &= ~(1u << bit); run(50); }
static void touch(uint32_t k) { scan.values[k] = 3000u; run(60); scan.values[k] = 0u; run(60); }
static void fresh(void) {
    tc_init(&tc_state); memset(&e, 0, sizeof e); memset(&scan, 0, sizeof scan); memset(raw, 0, sizeof raw);
    tm_reset();
    for (uint8_t s = 0; s < TC_CHORD_SETS; ++s) { tc_chord_set none; memset(&none, 0, sizeof none); assert(tc_chord_set_put(s, &none)); }
    sq_clear(&sq, SQ_UNIT_BAR, 0u);
    fs_erase(); (void)st_load(&tc_state);
    run(600); assert(e.state == TK_ACTIVE);
}
static const char *screen(void) {
    tc_state.event_sequence++; run(20); ui_service(&tc_state, &e, &scan, true, now);
    return at(0, 56);
}
/* Every item's value as the menu shows it (the per-chord items for one chord pad), and the defaults. */
static uint8_t values[TC_MENU_ITEMS], plain[TC_MENU_ITEMS];
static void snapshot(const tc_controls *c, uint8_t out[TC_MENU_ITEMS]) {
    for (uint8_t i = 0; i < TC_MENU_ITEMS; ++i) out[i] = tc_menu_value(c, i);
}
static bool on_page(uint8_t g, uint8_t item) {
    for (uint8_t i = 0; i < tc_group_size(g); ++i) if (tc_group_items[g][i] == item) return true;
    return false;
}
/* Items a page's reset changes: those whose value differs on a fresh board, excluding the per-chord edits. */
static bool per_chord(uint8_t item) {
    return item == TC_MENU_ADD7 || item == TC_MENU_ADD9 || item == TC_MENU_ADD6 || item == TC_MENU_SUS ||
           item == TC_MENU_PAD_STRUM || item == TC_MENU_SOUND;
}
/* The step sequencer's items (1.5.0): the pattern is saved content, which no reset touches. */
static bool pattern_item(uint8_t item) {
    return (item >= TC_MENU_STEPS && item <= TC_MENU_LANE_MUTE) || item == TC_MENU_LANE_DIRECTION || item == TC_MENU_LANE_EUCLID ||
           item == TC_MENU_LANE_ROTATE;
}
/* Open an item, choose RESET, confirm. */
static void reset_item(uint8_t group, uint8_t item) {
    tc_state.ui = TC_UI_MENU; tc_state.group = group; tc_state.menu = item;
    nav(NAV_CENTRE); assert(tc_state.ui == TC_UI_EDIT && tc_state.edit_value == 0u);
    nav(NAV_UP); assert(tc_state.edit_value == 1u);
    nav(NAV_CENTRE); assert(tc_state.ui == TC_UI_MENU);
}
/* Every option away from its default, through the menu as a player would set it. */
static uint32_t seed = 0x5EEDu;
static uint32_t rnd(uint32_t n) { seed ^= seed << 13; seed ^= seed >> 17; seed ^= seed << 5; return n ? seed % n : 0u; }
static void dirty(void) {
    for (uint8_t g = 0; g < TC_GROUPS; ++g) for (uint8_t i = 0; i < tc_group_size(g); ++i) {
        uint8_t item = tc_group_items[g][i], n = tc_menu_values(item);
        if (tc_confirm_item(item) || per_chord(item) || n < 2u || item == TC_MENU_LOAD || item == TC_MENU_SAVE ||
            item == TC_MENU_CAPTURE || item == TC_MENU_TRANSPORT || item == TC_MENU_SEQ_RECORD || item == TC_MENU_SEQ_CLEAR ||
            item == TC_MENU_LED_TEST || item == TC_MENU_IO_TEST || item == TC_MENU_PAD_TEST || item == TC_MENU_VERSION) continue;
        uint8_t now_v = tc_menu_value(&tc_state, item);
        for (uint32_t tries = 0; tries < 8u && tc_menu_value(&tc_state, item) == now_v; ++tries) {
            tc_state.ui = TC_UI_EDIT; tc_state.group = g; tc_state.menu = item;
            uint8_t v = (uint8_t)((now_v + 1u + rnd(n - 1u)) % n);
            if (item == TC_MENU_PAD_TRIGGER) v = 9u;  /* 1300: the gaps to Release and Full kept */
            if (item == TC_MENU_PAD_RELEASE) v = 4u;  /* 700 */
            if (item == TC_MENU_PAD_FULL) v = 18u;    /* 3300 */
            if (item == TC_MENU_SCALE) v = 1u;
            if (item == TC_MENU_CHORD_SET) v = TC_CHORDS_GRID;
            tc_state.edit_value = v; nav(NAV_CENTRE);
        }
        tc_state.ui = TC_UI_MENU;
    }
    sq.unit = SQ_UNIT_BEAT; /* not reset: a progression's own */
}

/* --- the confirmation ------------------------------------------------------------------- */
static void confirmation(void) {
    fresh();
    static const uint8_t items[5] = {TC_MENU_RESET_PAGE, TC_MENU_FACTORY, TC_MENU_RESET_ALL, TC_MENU_RESET_CHORD, TC_MENU_BUTTONS_RESET};
    static const uint8_t groups[5] = {TC_GROUP_ARP, TC_GROUP_ABOUT, TC_GROUP_CHORDS, TC_GROUP_CHORDS, TC_GROUP_BUTTONS};
    for (uint32_t k = 0; k < 5u; ++k) {
        assert(tc_confirm_item(items[k]) && tc_menu_values(items[k]) == 2u && tc_menu_value(&tc_state, items[k]) == 0u);
        assert(on_page(groups[k], items[k]));
    }
    tc_state.arp = TC_ARP_UP; tc_state.buttons[3] = TC_ACT_HOLD; tc_state.have_chord = true; tc_state.last_chord = 3u;
    tc_state.chord[3].add = TC_ADD7; tc_state.chord[7].add = TC_ADD9;
    uint32_t p[ST_PAYLOAD], q[ST_PAYLOAD];
    assert(st_get(ST_LIVE, 0, &tc_state, p) == ST_OK);
    /* Every way out without choosing RESET changes nothing. */
    for (uint32_t k = 0; k < 5u; ++k) {
        static const uint8_t ways[6][4] = {{NAV_CENTRE}, {NAV_RIGHT}, {NAV_LEFT}, {NAV_DOWN, NAV_CENTRE},
                                           {NAV_UP, NAV_LEFT}, {NAV_UP, NAV_DOWN, NAV_CENTRE}};
        for (uint32_t w = 0; w < 6u; ++w) {
            tc_state.ui = TC_UI_MENU; tc_state.group = groups[k]; tc_state.menu = items[k];
            nav(NAV_CENTRE); assert(tc_state.ui == TC_UI_EDIT && tc_state.edit_value == 0u); /* opens on CANCEL */
            for (uint32_t i = 0; i < 4u && ways[w][i]; ++i) nav(ways[w][i]);
            assert(tc_state.ui == TC_UI_MENU);
            assert(tc_state.last_action == (ways[w][0] == NAV_LEFT || ways[w][1] == NAV_LEFT ? TC_CANCEL : TC_RESET_KEPT));
            assert(st_get(ST_LIVE, 0, &tc_state, q) == ST_OK && !memcmp(p, q, sizeof p));
            assert(tc_state.chord[3].add == TC_ADD7 && tc_state.chord[7].add == TC_ADD9);
        }
    }
    /* Up past RESET stays on RESET (no wrap back to CANCEL), and down past CANCEL on CANCEL. */
    tc_state.ui = TC_UI_MENU; tc_state.group = TC_GROUP_ARP; tc_state.menu = TC_MENU_RESET_PAGE;
    nav(NAV_CENTRE); nav(NAV_DOWN); assert(tc_state.edit_value == 0u);
    nav(NAV_UP); nav(NAV_UP); assert(tc_state.edit_value == 1u);
    nav(NAV_LEFT);
    /* Chosen: each does its own reset only. */
    reset_item(TC_GROUP_CHORDS, TC_MENU_RESET_CHORD);
    assert(tc_state.last_action == TC_RESET && !tc_state.chord[3].add && tc_state.chord[7].add == TC_ADD9);
    reset_item(TC_GROUP_CHORDS, TC_MENU_RESET_ALL);
    assert(tc_state.last_action == TC_RESET && !tc_state.chord[7].add && tc_state.arp == TC_ARP_UP);
    reset_item(TC_GROUP_BUTTONS, TC_MENU_BUTTONS_RESET);
    assert(tc_state.buttons[3] == 3u && tc_state.arp == TC_ARP_UP && tc_state.last_action == TC_BUTTON);
    reset_item(TC_GROUP_ARP, TC_MENU_RESET_PAGE);
    assert(tc_state.arp == TC_ARP_OFF && tc_state.last_action == TC_OPTIONS_RESET);
    /* Reset chord with no chord played: nothing to reset. */
    tc_state.have_chord = false; reset_item(TC_GROUP_CHORDS, TC_MENU_RESET_CHORD); assert(tc_state.last_action == TC_TODO_FUNCTION);
    printf("confirmation: all 5 resets open on CANCEL; centre, right, left, down, up+left, up+down+centre change "
           "nothing; up then centre resets that one thing only\n");
}
/* --- Reset options, page by page ------------------------------------------------------------ */
static void pages(void) {
    tc_controls d; tc_init(&d); snapshot(&d, plain);
    uint32_t pages_with = 0, reset_items = 0;
    for (uint8_t g = 0; g < TC_GROUPS; ++g) {
        bool has = on_page(g, TC_MENU_RESET_PAGE);
        assert(has == (g != TC_GROUP_PRESETS && g != TC_GROUP_BUTTONS && g != TC_GROUP_ABOUT));
        if (!has) continue;
        pages_with++;
        fresh();
        tc_state.chord[5].sus = 4u; /* a per-chord edit: kept */
        dirty();
        snapshot(&tc_state, values);
        uint32_t changed = 0;
        for (uint8_t i = 0; i < tc_group_size(g); ++i)
            changed += !pattern_item(tc_group_items[g][i]) && values[tc_group_items[g][i]] != plain[tc_group_items[g][i]];
        assert(changed >= 1u);
        reset_item(g, TC_MENU_RESET_PAGE);
        assert(tc_state.last_action == TC_OPTIONS_RESET);
        uint8_t after[TC_MENU_ITEMS]; snapshot(&tc_state, after);
        uint32_t back = 0;
        for (uint8_t item = 0; item < TC_MENU_ITEMS; ++item) {
            bool mine = on_page(g, item) && !per_chord(item) && item != TC_MENU_SEQ_UNIT && item != TC_MENU_CAPTURE &&
                        item != TC_MENU_LOAD && item != TC_MENU_SAVE && !pattern_item(item);
            if (mine) { assert(after[item] == plain[item]); back += values[item] != plain[item]; }
            else if (item != TC_MENU_CAPTURE && !(g == TC_GROUP_SEQUENCE && item == TC_MENU_SEQ_UNIT)) {
                /* (Sequence: the slot back to 1 loads that progression, with its own unit) */
                if (after[item] != values[item]) printf("page %u reset: item %u %u -> %u\n", g, item, values[item], after[item]);
                assert(after[item] == values[item]); /* other pages untouched */
            }
        }
        assert(back == changed - (on_page(g, TC_MENU_SEQ_UNIT) && values[TC_MENU_SEQ_UNIT] != plain[TC_MENU_SEQ_UNIT] ? 1u : 0u));
        assert(tc_state.chord[5].sus == 4u && (g == TC_GROUP_SEQUENCE || sq.unit == SQ_UNIT_BEAT));
        assert(tc_reset_count(g) >= 2u);
        reset_items += tc_reset_count(g);
        /* The engine plays what the settings now say. */
        tp_layout l = tc_layout(&tc_state); assert(!memcmp(&l, &e.layout, sizeof l));
    }
    assert(pages_with == 10u);
    assert(tc_reset_count(TC_GROUP_PLAY) == 16u && tc_reset_count(TC_GROUP_CHORDS) == 8u && tc_reset_count(TC_GROUP_ARP) == 10u &&
           tc_reset_count(TC_GROUP_LAYOUT) == 7u && /* 1.6.0: harmony, echo, echoes; chance; mono */
           tc_reset_count(TC_GROUP_SEQUENCE) == 2u && tc_reset_count(TC_GROUP_PADS) == 4u &&
           tc_reset_count(TC_GROUP_MIDI) == 8u); /* 1.7.0: Spread of; Chord prog, Melody prog, Program in */
    /* Not while recording a progression (it keeps its chord set). */
    fresh();
    tc_state.chord_voicing = 2u; tc_state.recording = true;
    reset_item(TC_GROUP_CHORDS, TC_MENU_RESET_PAGE);
    assert(tc_state.last_action == TC_SEQ_BUSY && tc_state.chord_voicing == 2u);
    tc_state.recording = false;
    printf("pages: Reset options on the %u pages with options (%u items): that page back to the defaults, every "
           "other page, per-chord edits and the progression's unit kept, the engine following; not while recording\n",
           pages_with, reset_items);
}
/* --- Factory settings ---------------------------------------------------------------------- */
static void factory(void) {
    fresh();
    /* Saved content: a preset, a user scale, a chord set, a progression. */
    assert(tc_set_user_scale(2, 0x6B5u, "MINE"));
    tc_chord_set cs; memset(&cs, 0, sizeof cs); cs.column[0] = (tc_set_chord){0x091u, 0u}; snprintf(cs.name, sizeof cs.name, "SET");
    assert(tc_chord_set_put(3, &cs));
    uint32_t w[ST_PAYLOAD]; assert(st_get(ST_CHORDS, 3, &tc_state, w) == ST_OK && st_put(ST_CHORDS, 3, w, &tc_state, &e) == ST_OK);
    tc_state.ui = TC_UI_MENU; tc_state.group = TC_GROUP_SEQUENCE; tc_state.menu = TC_MENU_SEQ_RECORD; nav(NAV_CENTRE);
    assert(tc_state.recording); touch(3u); touch(7u); nav(NAV_CENTRE); assert(!tc_state.recording); run(100);
    tc_state.ui = TC_UI_EDIT; tc_state.group = TC_GROUP_PRESETS; tc_state.menu = TC_MENU_SAVE; tc_state.edit_value = 4u; nav(NAV_CENTRE); run(100);
    uint8_t presets = tc_presets_present, progs = sq_present;
    assert(presets && progs);
    /* Everything else away from its default: options, key, panel octaves, chord edits, buttons. */
    dirty();
    tp_layout l = tc_layout(&tc_state); l.tonic = 5u; assert(tc_set_layout(&tc_state, &e, l));
    tc_state.chord_octaves = 0x30u; tc_state.chord[9].add = TC_ADD6; tc_state.buttons[0] = TC_ACT_SHIFT;
    uint8_t kept[TC_MENU_ITEMS]; snapshot(&tc_state, kept);
    reset_item(TC_GROUP_ABOUT, TC_MENU_FACTORY);
    assert(tc_state.last_action == TC_FACTORY_RESET);
    uint8_t after[TC_MENU_ITEMS]; snapshot(&tc_state, after);
    for (uint8_t item = 0; item < TC_MENU_ITEMS; ++item)
        if (pattern_item(item)) assert(after[item] == kept[item]); /* the step pattern stays (1.5.0) */
        else if (item != TC_MENU_SEQ_UNIT && item != TC_MENU_LOAD && item != TC_MENU_SAVE && item != TC_MENU_CAPTURE &&
            item != TC_MENU_SEQ_SLOT) assert(after[item] == plain[item]);
    assert(tc_state.tonic == 0u && tc_state.chord_octaves == TC_OCTAVES_DEFAULT && !tc_state.chord[9].add &&
           tc_state.buttons[0] == 0u && tc_state.prog_slot == 0u);
    run(ST_SAVE_DELAY_MS + 200u);
    assert(tc_presets_present == presets && sq_present == progs && tc_chord_set_defined(3) && tc_scale_defined(TC_SCALES + 2u));
    /* Stored: the next start has the defaults and the saved content. */
    tc_controls back; tc_init(&back);
    tc_chord_set none; memset(&none, 0, sizeof none); assert(tc_chord_set_put(3, &none));
    assert(st_load(&back) && back.tonic == 0u && back.arp == TC_ARP_OFF && back.tempo == plain[TC_MENU_TEMPO] && tc_chord_set_defined(3));
    /* While recording: refused. */
    tc_state.tempo = 30u; tc_state.recording = true;
    reset_item(TC_GROUP_ABOUT, TC_MENU_FACTORY); assert(tc_state.last_action == TC_SEQ_BUSY && tc_state.tempo == 30u);
    tc_state.recording = false;
    printf("factory: every option, the key, panel octaves, chord edits and buttons back to the defaults and stored; "
           "presets, user scales, chord sets and progressions kept; not while recording\n");
}
/* --- the screens ---------------------------------------------------------------------------- */
static void screens(void) {
    fresh();
    struct { uint8_t group, item; const char *title, *first; } cases[] = {
        {TC_GROUP_PLAY, TC_MENU_RESET_PAGE, "RESET PLAY OPTIONS?", "16 SETTINGS BACK TO"},
        {TC_GROUP_ARP, TC_MENU_RESET_PAGE, "RESET ARP OPTIONS?", "10 SETTINGS BACK TO"},
        {TC_GROUP_SEQUENCE, TC_MENU_RESET_PAGE, "RESET SEQ OPTIONS?", "2 SETTINGS BACK TO"},
        {TC_GROUP_CHORDS, TC_MENU_RESET_PAGE, "RESET CHORDS OPTIONS?", "8 SETTINGS BACK TO"},
        {TC_GROUP_ABOUT, TC_MENU_FACTORY, "RESET ALL OPTIONS?", "PRESETS, SCALES, SETS"},
        {TC_GROUP_CHORDS, TC_MENU_RESET_ALL, "RESET ALL EDITS?", "EVERY CHORD PAD GOES"},
        {TC_GROUP_CHORDS, TC_MENU_RESET_CHORD, "RESET CHORD EDITS?", "THE LAST CHORD PLAYED"},
        {TC_GROUP_BUTTONS, TC_MENU_BUTTONS_RESET, "RESET ALL BUTTONS?", "EVERY CENTER BUTTON"},
    };
    for (uint32_t k = 0; k < sizeof cases / sizeof cases[0]; ++k) {
        tc_state.ui = TC_UI_MENU; tc_state.group = cases[k].group; tc_state.menu = cases[k].item;
        assert(!strcmp(screen(), "<BACK   O RESET"));
        nav(NAV_CENTRE);
        assert(!strcmp(screen(), "UP THEN O TO RESET"));
        assert(!strcmp(at(0, 0), cases[k].title) && !strcmp(at(0, 11), cases[k].first));
        assert(!strcmp(at(12, 43), "CANCEL") && !strcmp(at(86, 43), "RESET") && inverted_y == 43u);
        nav(NAV_UP);
        assert(!strcmp(screen(), "O RESETS   <CANCEL"));
        nav(NAV_LEFT);
        assert(tc_state.ui == TC_UI_MENU);
    }
    tc_state.ui = TC_UI_MENU; tc_state.group = TC_GROUP_PLAY; tc_state.menu = TC_MENU_RESET_PAGE;
    nav(NAV_CENTRE); nav(NAV_CENTRE); assert(!strcmp(screen(), "NOTHING WAS RESET"));
    reset_item(TC_GROUP_PLAY, TC_MENU_RESET_PAGE); assert(!strcmp(screen(), "PLAY OPTIONS RESET"));
    reset_item(TC_GROUP_ARP, TC_MENU_RESET_PAGE); assert(!strcmp(screen(), "ARP OPTIONS RESET"));
    reset_item(TC_GROUP_SEQUENCE, TC_MENU_RESET_PAGE); assert(!strcmp(screen(), "SEQ OPTIONS RESET"));
    reset_item(TC_GROUP_ABOUT, TC_MENU_FACTORY); assert(!strcmp(screen(), "ALL OPTIONS RESET"));
    printf("screens: each confirmation's title and text, CANCEL chosen and inverted, the hints for each choice; "
           "NOTHING WAS RESET, PLAY/ARP/SEQ OPTIONS RESET, ALL OPTIONS RESET\n");
}
int main(void) {
    setvbuf(stdout, NULL, _IONBF, 0);
    (void)ui_init();
    confirmation();
    pages();
    factory();
    screens();
    printf("PASS: resets: every reset asks first and opens on CANCEL; Reset options per page, Factory settings, "
           "Reset chord, Reset all edits, Reset all buttons; saved content kept; the confirmation screens\n");
    return 0;
}
