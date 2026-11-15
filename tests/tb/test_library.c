/* SPDX-License-Identifier: GPL-3.0-or-later - 2.6.6: the progression library and Find key, on the real controls, settings
 * (a fake sector 11) and screens' text. Every library entry loads (a valid record that round-trips, a name that fits and
 * the screen's font has), its chords are the ones named (in C and in other keys), loading one replaces the slot's
 * progression, saves it, survives a restart and puts the board in its scale with the root kept; not while recording.
 * Find key: the notes counted from MIDI in (not the Pad channel's or the MIDI map's), halved when the count is full,
 * forgotten after a pause; the keys found for scales and chords (Krumhansl-Kessler), too few notes, ties; the menu items,
 * their values and names, the messages; Find key sets the key. */
#include "../../src/tb/controls_internal.h"
#include "../../src/tb/settings.h"
#include "../../src/tb/flash_store.h"
#include "../../src/tb/midi_in.h"
#include "../../src/tb/chord_detect.h"
#include "../../src/tb/ui.h"
#include "../../src/tb/ui_internal.h"
#include "../../src/tb/display.h"
#include "../../src/tb/leds.h"
#include "../../src/tb/jack.h"
#include "../../src/tb/nor.h"
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
void td_invert(uint32_t x0, uint32_t y0, uint32_t x1, uint32_t y1) { (void)x0; (void)y0; (void)x1; (void)y1; }
void td_service(void) {}
static char drawn[1024]; /* what the screen shows: each text drawn since it was last cleared, | after each */
void td_clear(void) { drawn[0] = 0; }
uint32_t td_text(uint32_t x, uint32_t y, const char *text, uint32_t scale) {
    (void)y;
    if (strlen(drawn) + strlen(text) + 2u < sizeof drawn) { strcat(drawn, text); strcat(drawn, "|"); }
    return x + 6u * scale * (uint32_t)strlen(text);
}
static tp_engine e;
static uint32_t now = 10000u;
static void service(void) {
    for (uint32_t i = 0; i < 40u; ++i) { now++; st_requests(&tc_state, &e); (void)st_service(&tc_state, now); }
}
static void fresh(void) {
    memset(&e, 0, sizeof e); memset(flash, 0xFF, sizeof flash);
    tm_reset(); tc_init(&tc_state); (void)st_load(&tc_state);
    for (uint32_t p = 0; p < LN_PATTERNS; ++p) ln_clear(&ln_bank[p]);
    e.layout = tc_layout(&tc_state);
}
static const char *ui_text_last(const tc_controls *c) { ui_start(); ui_last_action(c); return ui_buffer; }
static void midi(uint8_t status, uint8_t a, uint8_t b) {
    const uint8_t p[4] = {(uint8_t)(status >> 4), status, a, b}; tm_packet(p, now);
}
static void note(uint8_t ch, uint8_t pitch) { midi((uint8_t)(0x90u | (ch - 1u)), pitch, 100u); midi((uint8_t)(0x80u | (ch - 1u)), pitch, 0u); }
/* The pitch classes step i of p plays with the board at tonic in the board's scale (as engine_play.c plays a step). */
static uint16_t classes(const sq_prog *p, uint32_t i, uint8_t tonic) {
    tp_layout l = tc_layout(&tc_state); l.tonic = tonic;
    l = sq_layout(p, l, &p->entries[i]);
    tp_notes n; assert(tc_notes(l, p->entries[i].pad, p->entries[i].edit, &n) && n.count);
    uint16_t set = 0;
    for (uint32_t k = 0; k < n.count; ++k) set |= (uint16_t)(1u << (n.notes[k] % 12u));
    return set;
}
static uint16_t pcs(const char *list) { /* "0 4 7" */
    uint16_t set = 0; uint32_t v = 0; bool any = false;
    for (const char *s = list;; ++s) {
        if (*s >= '0' && *s <= '9') { v = v * 10u + (uint32_t)(*s - '0'); any = true; continue; }
        if (any) set |= (uint16_t)(1u << (v % 12u));
        v = 0; any = false;
        if (!*s) return set;
    }
}
static uint32_t find(const char *name) {
    for (uint32_t i = 0; i < SQ_LIBRARY; ++i) if (!strcmp(sq_library[i].name, name)) return i;
    assert(0); return 0;
}
static void entries(void) {
    fresh();
    for (uint32_t i = 0; i < SQ_LIBRARY; ++i) {
        const sq_library_entry *l = &sq_library[i];
        assert(strlen(l->name) <= SQ_LIBRARY_NAME && strlen(l->name) > 0u);
        for (const char *c = l->name; *c; ++c) assert(strchr(td_font_chars, *c)); /* the screen can draw it */
        for (uint32_t j = 0; j < i; ++j) assert(strcmp(sq_library[j].name, l->name)); /* each its own name */
        assert(l->mode == 0u || l->mode == 1u || l->mode == 2u || l->mode == 5u);
        sq_prog p, back; uint8_t mode = 99u;
        assert(sq_library_load(i, &p, &mode) && mode == l->mode && p.count >= 2u && p.scale == l->mode && !p.chords);
        uint32_t w[SQ_WORDS]; sq_encode(&p, w);
        assert(sq_decode(w, &back) && back.count == p.count && back.total == p.total && back.unit == p.unit);
        for (uint32_t k = 0; k < p.count; ++k) {
            const sq_entry *a = &p.entries[k], *b = &back.entries[k];
            assert(a->pad == b->pad && a->units == b->units && a->velocity == 100u && b->velocity == 100u && a->scale == b->scale &&
                   a->edit.add == b->edit.add && !a->edit.sus && !a->edit.lower && !a->chance && (a->pad & 3u) == 3u);
            assert(a->scale == SQ_NO_SCALE || (a->scale != l->mode && tc_scale_defined(a->scale)));
        }
    }
    sq_prog p; uint8_t mode = 7u;
    assert(!sq_library_load(SQ_LIBRARY, &p, &mode) && mode == 7u);
    puts("entries: every library progression loads, round-trips as a record, fits the screen in its font");
}
static void chords(void) {
    fresh();
    sq_prog p; uint8_t mode;
    /* The pop progression in C: C G Am F; in A: A E F#m D. */
    assert(sq_library_load(find("I-V-vi-IV"), &p, &mode) && mode == 0u && p.count == 4u && p.total == 4u);
    tc_state.scale = 0u;
    assert(classes(&p, 0, 0) == pcs("0 4 7") && classes(&p, 1, 0) == pcs("7 11 2") && classes(&p, 2, 0) == pcs("9 0 4") &&
           classes(&p, 3, 0) == pcs("5 9 0"));
    assert(classes(&p, 0, 9) == pcs("9 1 4") && classes(&p, 2, 9) == pcs("6 9 1") && classes(&p, 3, 9) == pcs("2 6 9"));
    /* The blues: C7 F7 G7, from Mixolydian, Dorian and the key itself; 12 bars. */
    assert(sq_library_load(find("BLUES 12 BAR"), &p, &mode) && mode == 0u && p.total == 12u && p.count == 7u);
    assert(classes(&p, 0, 0) == pcs("0 4 7 10") && p.entries[0].units == 4u && p.entries[0].scale == 5u);
    assert(classes(&p, 1, 0) == pcs("5 9 0 3") && p.entries[1].units == 2u && p.entries[1].scale == 2u);
    assert(classes(&p, 3, 0) == pcs("7 11 2 5") && p.entries[3].scale == SQ_NO_SCALE);
    /* ii-V-I: Dm7 G7 Cmaj7 (two bars). */
    assert(sq_library_load(find("ii-V-I"), &p, &mode));
    assert(classes(&p, 0, 0) == pcs("2 5 9 0") && classes(&p, 1, 0) == pcs("7 11 2 5") && classes(&p, 2, 0) == pcs("0 4 7 11") &&
           p.entries[2].units == 2u);
    /* In A minor: the Andalusian cadence Am G F E (a major E, from harmonic minor). */
    assert(sq_library_load(find("i-VII-VI-V"), &p, &mode) && mode == 1u);
    tc_state.scale = 1u;
    assert(classes(&p, 0, 9) == pcs("9 0 4") && classes(&p, 1, 9) == pcs("7 11 2") && classes(&p, 2, 9) == pcs("5 9 0") &&
           classes(&p, 3, 9) == pcs("4 8 11") && p.entries[3].scale == 7u);
    /* ii-V-i in C minor: Dm7b5 G7 Cm7. */
    assert(sq_library_load(find("ii-V-i MINOR"), &p, &mode));
    assert(classes(&p, 0, 0) == pcs("2 5 8 0") && classes(&p, 1, 0) == pcs("7 11 2 5") && classes(&p, 2, 0) == pcs("0 3 7 10"));
    /* I-bVI-bVII-I in C: C Ab Bb C, the two borrowed from C minor. */
    assert(sq_library_load(find("I-bVI-bVII-I"), &p, &mode));
    tc_state.scale = 0u;
    assert(classes(&p, 1, 0) == pcs("8 0 3") && classes(&p, 2, 0) == pcs("10 2 5") && p.entries[1].scale == 1u);
    /* Mixolydian rock in G: G F C G. */
    assert(sq_library_load(find("I-bVII-IV-I"), &p, &mode) && mode == 5u);
    tc_state.scale = 5u;
    assert(classes(&p, 0, 7) == pcs("7 11 2") && classes(&p, 1, 7) == pcs("5 9 0") && classes(&p, 2, 7) == pcs("0 4 7"));
    /* Canon: half bars. */
    assert(sq_library_load(find("CANON"), &p, &mode) && p.unit == SQ_UNIT_HALF && p.count == 8u);
    puts("chords: the library's chords as named (pop, blues 7ths, ii-V-I, Andalusian, minor ii-V-i, borrowed, Mixolydian)");
}
static void loading(void) {
    fresh();
    tc_state.scale = 3u; /* Phrygian */
    tp_layout l = tc_layout(&tc_state); l.tonic = 2u; assert(tc_set_layout(&tc_state, &e, l)); /* D Phrygian */
    assert(tc_set_item(&tc_state, &e, TC_MENU_SEQ_SLOT, 2u)); service();
    uint32_t i = find("i-VI-III-VII");
    tc_load_library(&tc_state, &e, (uint8_t)i); service();
    assert(tc_state.last_action == TC_SEQ_LIBRARY && tc_state.library == i);
    assert(tc_state.scale == 1u && tc_layout(&tc_state).tonic == 2u); /* D minor: its scale, the root kept */
    assert(sq.count == 4u && sq.entries[1].pad == 4u * 5u + 3u && (sq_present & (1u << 2)));
    assert(sq_slots[2].count == 4u && sq_slots[2].entries[3].pad == 4u * 6u + 3u);
    assert(!strcmp(ui_text_last(&tc_state), "P3: i-VI-III-VII"));
    assert(tc_menu_value(&tc_state, TC_MENU_SEQ_LIBRARY) == i);
    /* After a restart: the slot keeps it. */
    tm_reset(); tc_init(&tc_state); (void)st_load(&tc_state);
    assert(sq_present & (1u << 2)); assert(sq_slots[2].count == 4u && sq_slots[2].scale == 1u);
    /* A Major one from minor: the board goes to Major; one in the board's scale leaves the scale alone. */
    assert(tc_set_item(&tc_state, &e, TC_MENU_SEQ_SLOT, 0u)); service();
    tc_load_library(&tc_state, &e, (uint8_t)find("I-V-vi-IV")); service();
    assert(tc_state.scale == 0u && sq.count == 4u && (sq_present & 1u));
    /* Not while recording. */
    tc_start_recording(&tc_state);
    assert(tc_state.recording);
    tc_load_library(&tc_state, &e, (uint8_t)find("CANON"));
    assert(tc_state.last_action == TC_SEQ_BUSY);
    tc_state.recording = false;
    /* The menu: the item on the Progressions page, its values, kind, names, the bottom line. */
    uint8_t items[TC_GROUP_MAX], n = tc_page_items(&tc_state, TC_GROUP_PROGRESSIONS, items);
    bool there = false; for (uint32_t k = 0; k < n; ++k) there = there || items[k] == TC_MENU_SEQ_LIBRARY;
    assert(there && tc_menu_values(TC_MENU_SEQ_LIBRARY) == SQ_LIBRARY && tc_item_kind(TC_MENU_SEQ_LIBRARY) == TC_ITEM_BOARD);
    assert(!strcmp(ui_item_name(TC_MENU_SEQ_LIBRARY), "LIBRARY") && !strcmp(ui_value_text(TC_MENU_SEQ_LIBRARY, 0), "I-V-vi-IV"));
    assert(!tc_set_item(&tc_state, &e, TC_MENU_SEQ_LIBRARY, 0u) && !tc_cc_mappable(TC_MENU_SEQ_LIBRARY)); /* the board's own */
    /* Chosen and set from the menu, as the board's buttons do it. */
    tc_state.ui = TC_UI_MENU; tc_state.group = TC_GROUP_PROGRESSIONS; tc_state.menu = TC_MENU_SEQ_LIBRARY;
    tc_navigate(&tc_state, &e, TC_NAV_ENTER); assert(tc_state.ui == TC_UI_EDIT);
    tc_navigate(&tc_state, &e, TC_NAV_UP); tc_navigate(&tc_state, &e, TC_NAV_UP);
    tc_navigate(&tc_state, &e, TC_NAV_ENTER); service();
    assert(tc_state.ui == TC_UI_MENU && tc_state.library == 2u && sq.entries[0].pad == 4u * 5u + 3u); /* vi-IV-I-V */
    puts("loading: into the slot, saved, kept after a restart, the board in its scale (root kept); not while recording; the menu");
}
static void counting(void) {
    fresh();
    for (uint32_t i = 0; i < 5u; ++i) note(1, 60);
    note(3, 64); note(16, 67);
    assert(tm.heard[0] == 5u && tm.heard[4] == 1u && tm.heard[7] == 1u && tm.heard_total == 7u);
    midi(0x90u, 62u, 0u); assert(tm.heard[2] == 0u); /* velocity 0: an end */
    /* The Pad channel's notes press pads: not counted (outside its 48 they count). */
    tc_state.pad_ch = 16u; tm_pad_listen(16u, 36u);
    note(16, 40); assert(tm.heard[4] == 1u);
    note(16, 90); assert(tm.heard[6] == 1u);
    /* A note the MIDI map takes: a control, not counted. */
    tm_watch_set(0, (tm_watch_entry){TM_SRC_NOTE, 2u, 61u});
    note(2, 61); assert(tm.heard[1] == 0u);
    note(5, 61); assert(tm.heard[1] == 1u);
    /* Full: halved, the newest counting most. */
    fresh();
    for (uint32_t i = 0; i < TM_HEARD_MAX - 1u; ++i) note(1, (uint8_t)(i % 2u ? 60u : 67u));
    assert(tm.heard_total == TM_HEARD_MAX - 1u && tm.heard[0] == 127u && tm.heard[7] == 128u);
    note(1, 64);
    assert(tm.heard[0] == 63u && tm.heard[7] == 64u && tm.heard[4] == 0u && tm.heard_total == 127u);
    /* A pause of 20 s: the next note starts afresh; just under, it doesn't. */
    now += TM_HEARD_FORGET_MS - 1u; note(1, 69); assert(tm.heard_total == 128u && tm.heard[9] == 1u);
    now += TM_HEARD_FORGET_MS; note(1, 69); assert(tm.heard_total == 1u && tm.heard[9] == 1u && !tm.heard[0]);
    puts("counting: Note Ons per pitch class from MIDI in, not the Pad channel's or the map's; halved when full; a pause forgets");
}
static uint32_t keys_of(const char *counts, uint8_t *keys) { /* 12 counts, C..B */
    uint16_t h[12]; uint32_t i = 0;
    for (const char *s = counts; *s && i < 12u; ++s) if (*s != ' ') h[i++] = (uint16_t)(*s >= 'a' ? *s - 'a' + 10 : *s - '0');
    assert(i == 12u);
    return cd_keys(h, keys, TC_KEY_GUESSES);
}
static void finding(void) {
    uint8_t k[TC_KEY_GUESSES];
    /*              C D E F G A B with the black keys between */
    assert(keys_of("4 0 2 0 3 2 0 3 0 2 0 2", k) == 3u && k[0] == 1u);           /* C major, its triad strongest */
    assert(keys_of("3 0 2 2 0 2 0 3 0 4 0 1", k) && k[0] == 13u + 9u);          /* A minor: A C E strongest */
    assert(keys_of("0 0 3 0 2 0 2 2 0 3 0 1", k) && k[0] == 3u);                /* D major (F#, C#'s absence aside) */
    assert(keys_of("2 0 2 0 2 0 2 2 0 2 0 2", k) && (k[0] == 1u || k[0] == 8u)); /* a C major scale evenly: C or G major */
    assert(keys_of("0 0 0 0 0 0 0 0 0 0 0 0", k) == 0u);                         /* nothing heard */
    assert(keys_of("5 0 0 0 0 0 0 4 0 0 0 0", k) == 0u);                         /* two pitch classes */
    assert(keys_of("4 0 0 0 4 0 0 4 0 0 0 0", k) >= 1u && k[0] == 1u);          /* 12 notes of C E G: C major */
    assert(keys_of("2 0 0 0 2 0 0 3 0 0 0 0", k) == 0u);                         /* 7 notes: too few */
    /* E harmonic minor's notes with E strong. */
    assert(keys_of("2 0 0 1 4 0 2 2 0 0 0 3", k) && k[0] == 13u + 4u);
    puts("finding: C major, A minor, D major, E minor from their notes; too few notes or classes find nothing");
}
static void find_key_menu(void) {
    fresh();
    tc_state.scale = 4u; /* Lydian */
    assert(!strcmp(ui_value_text(TC_MENU_FIND_KEY, 0), "LISTENING"));
    tc_find_key(&tc_state, &e, 0); assert(tc_state.last_action == TC_KEY_UNHEARD && tc_state.scale == 4u);
    assert(!strcmp(ui_text_last(&tc_state), "NO NOTES HEARD YET"));
    /* A minor arpeggios from a DAW on channel 1 (then a G and a D). */
    for (uint32_t i = 0; i < 4u; ++i) { note(1, 57); note(1, 60); note(1, 64); }
    note(1, 67); note(1, 62);
    assert(!strcmp(ui_value_text(TC_MENU_FIND_KEY, 0), "A MINOR"));
    assert(!strncmp(ui_value_text(TC_MENU_FIND_KEY, 1), "OR ", 3u) && strlen(ui_value_text(TC_MENU_FIND_KEY, 1)) <= 11u);
    tc_find_key(&tc_state, &e, 0);
    assert(tc_state.last_action == TC_KEY_FOUND && tc_state.scale == 1u && tc_layout(&tc_state).tonic == 9u);
    assert(!strcmp(ui_text_last(&tc_state), "KEY: A MINOR"));
    /* The second guess (C major, the relative key), set from the menu's own buttons. */
    uint8_t k[TC_KEY_GUESSES]; assert(cd_keys(tm.heard, k, TC_KEY_GUESSES) >= 2u && k[1] == 1u);
    tc_state.ui = TC_UI_MENU; tc_state.group = TC_GROUP_PLAY; tc_state.menu = TC_MENU_FIND_KEY;
    tc_navigate(&tc_state, &e, TC_NAV_ENTER); assert(tc_state.ui == TC_UI_EDIT && tc_state.edit_value == 0u);
    tc_navigate(&tc_state, &e, TC_NAV_UP); assert(tc_state.edit_value == 1u);
    tc_navigate(&tc_state, &e, TC_NAV_UP); tc_navigate(&tc_state, &e, TC_NAV_UP); assert(tc_state.edit_value == TC_KEY_GUESSES - 1u);
    tc_navigate(&tc_state, &e, TC_NAV_DOWN);
    tc_navigate(&tc_state, &e, TC_NAV_ENTER);
    assert(tc_state.ui == TC_UI_MENU && tc_state.scale == 0u && tc_layout(&tc_state).tonic == 0u && tc_state.found_key == 1u);
    /* The item: on the Play page after Scale, the board's own (not the editor's, not mappable). */
    uint8_t items[TC_GROUP_MAX], n = tc_page_items(&tc_state, TC_GROUP_PLAY, items);
    assert(n >= 2u && items[0] == TC_MENU_SCALE && items[1] == TC_MENU_FIND_KEY);
    assert(tc_menu_values(TC_MENU_FIND_KEY) == TC_KEY_GUESSES && tc_item_kind(TC_MENU_FIND_KEY) == TC_ITEM_BOARD);
    assert(!tc_cc_mappable(TC_MENU_FIND_KEY) && !strcmp(ui_item_name(TC_MENU_FIND_KEY), "FIND KEY"));
    /* Reset options on the Play page leaves it (nothing to reset) and counts as before. */
    assert(tc_reset_count(TC_GROUP_PLAY) == 8u);
    puts("find key: LISTENING, NO NOTES HEARD YET, the best key and the others, set from the menu, the item and its page");
}
/* Bench F3: the Play page shows the key as the notes arrive, no button pressed (the screen is redrawn when what it shows
 * changes, and the guesses were not part of that: LISTENING until a button). */
static void find_key_screen(void) {
    static ts_scan scan;
    fresh();
    tc_state.ui = TC_UI_MENU; tc_state.group = TC_GROUP_PLAY; tc_state.menu = TC_MENU_SCALE;
    ui_service(&tc_state, &e, &scan, true, now);
    assert(strstr(drawn, "LISTENING"));
    for (uint32_t i = 0; i < 4u; ++i) { note(1, 57); note(1, 60); note(1, 64); }
    note(1, 67); note(1, 62);
    ui_service(&tc_state, &e, &scan, true, ++now);
    assert(strstr(drawn, "A MINOR") && !strstr(drawn, "LISTENING"));
    tc_state.ui = TC_UI_PLAY; ui_service(&tc_state, &e, &scan, true, ++now); /* elsewhere: not worked out */
    note(1, 64);
    tc_state.ui = TC_UI_MENU; ui_service(&tc_state, &e, &scan, true, ++now);
    assert(strstr(drawn, "A MINOR"));
    puts("find key screen: the Play page shows the key once the notes come, with no button pressed");
}
int main(void) {
    entries();
    chords();
    loading();
    counting();
    finding();
    find_key_menu();
    find_key_screen();
    printf("PASS: 2.6.6: the progression library (every entry, its chords, loading into a slot, the board's scale, the menu) "
           "and Find key (the notes counted, the keys found, the menu, setting the key)\n");
    return 0;
}
