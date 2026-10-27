/* SPDX-License-Identifier: GPL-3.0-or-later - 1.0.0 on the real code: chord sets (layout.c, kept by
 * settings.c on a fake sector 11, played by the engine and by progressions), theory hints (which
 * pads, and their lights through ui.c), scale lock (MIDI in -> the melody channel) and the centre
 * buttons' actions with Shift (controls.c through the touch-controller reader). 1.5.0: the step
 * sequencer through the real menus (Settings > Sequence: Lane, Kind, Channel, Length, Rate, Steps),
 * step record on the step screen, playing with the Play button next to a progression, transposed
 * by a root button. */
#include "../../src/tb/settings.h"
#include "../../src/tb/sequence.h"
#include "../../src/tb/midi_in.h"
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
static tl_rgb pads[TL_PADS];
static uint16_t buttons[TL_BUTTONS];
bool tl_init(void) { return true; }
void tl_set_pad(uint32_t pad, tl_rgb c) { if (pad < TL_PADS) pads[pad] = c; }
void tl_set_button(uint32_t channel, uint16_t level) { if (channel < TL_BUTTONS) buttons[channel] = level; }
void tl_service(void) {}
uint32_t jh_levels(void) { return 5u; }
bool td_init(void) { return true; }
void td_pixel(uint32_t x, uint32_t y, bool on) { (void)x; (void)y; (void)on; }
void td_invert(uint32_t x0, uint32_t y0, uint32_t x1, uint32_t y1) { (void)x0; (void)y0; (void)x1; (void)y1; }
void td_service(void) {}
void td_clear(void) {}
uint32_t td_text(uint32_t x, uint32_t y, const char *text, uint32_t scale) { (void)y; return x + 6u * scale * (uint32_t)strlen(text); }

static tp_engine e;
static ts_scan scan;
static uint32_t now = 1000u;
/* What a synth hears. */
static uint8_t sounding[16][128], velocity_of[16][128];
static uint32_t ons, offs, cc64[2]; /* CC64 on, off */
static uint8_t on_note[512], on_ch[512]; /* the Note Ons in order (1.5.0) */
static uint32_t on_at[512], logged;
static bool writer(const uint8_t p[4], uint32_t token) {
    (void)token;
    uint32_t ch = p[1] & 15u;
    if (p[0] == TK_ON) { assert(!sounding[ch][p[2]]); sounding[ch][p[2]] = 1u; velocity_of[ch][p[2]] = p[3]; ons++; }
    if (p[0] == TK_ON && logged < 512u) { on_note[logged] = p[2]; on_ch[logged] = (uint8_t)ch; on_at[logged++] = now; }
    if (p[0] == TK_OFF) { assert(sounding[ch][p[2]]); sounding[ch][p[2]] = 0u; offs++; }
    if (p[0] == TK_CC && p[2] == 64u) cc64[p[3] ? 0 : 1]++;
    return true;
}
static uint32_t heard(uint32_t ch) {
    uint32_t n = 0;
    for (uint32_t p = 0; p < 128u; ++p) n += sounding[ch][p];
    return n;
}
static bool hears(uint32_t ch, const uint8_t *notes, uint32_t n) {
    if (heard(ch) != n) return false;
    for (uint32_t i = 0; i < n; ++i) if (!sounding[ch][notes[i]]) return false;
    return true;
}
/* The touch controllers: a 16-bit status per address, as the board's buttons set it. */
static const uint8_t addresses[4] = {0x35, 0x36, 0x38, 0x39};
static uint32_t raw[4];
static uint32_t client(uint8_t a) { for (uint32_t i = 0; i < 4u; ++i) if (addresses[i] == a) return i; assert(0); return 0; }
static struct bi_reply reader(void *ctx, uint8_t a, uint8_t reg, uint32_t gen, uint32_t stamp, uint32_t budget) {
    (void)ctx; (void)reg; (void)budget;
    uint32_t i = client(a);
    return (struct bi_reply){.status = 0, .count = 2, .generation = gen, .timestamp_ms = stamp, .elapsed_us = 500,
                             .data = {(uint8_t)raw[i], (uint8_t)(raw[i] >> 8)}};
}
/* The loop of usb_main: controls every 10 ms, pad frames every 15, the engine every ms. */
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
/* Touch-controller bits (controls.c): centre buttons, navigation, root and mode buttons. */
static const uint8_t fn_bit[TC_FUNCTIONS] = {6, 5, 0, 1, 7, 4, 3, 8, 9, 10, 11, 12, 14, 2};
static const uint8_t top_bit[12] = {3, 1, 0, 4, 5, 6, 7, 8, 9, 10, 11, 12};
enum { NAV_CENTRE = 6 };
static void hold_bit(uint8_t a, uint8_t bit, bool down) {
    if (down) raw[client(a)] |= 1u << bit; else raw[client(a)] &= ~(1u << bit);
    run(50);
}
static void tap_bit(uint8_t a, uint8_t bit) { hold_bit(a, bit, true); hold_bit(a, bit, false); }
static void hold_button(uint32_t i, bool down) { hold_bit(0x35, fn_bit[i], down); }
static void tap_button(uint32_t i) { tap_bit(0x35, fn_bit[i]); }
static void touch(uint32_t k, bool down) { scan.values[k] = down ? 3000u : 0u; run(60); }
static uint32_t pad(uint32_t column, uint32_t row) { return 4u * column + row; }
static void fresh(void) {
    tc_init(&tc_state); memset(&e, 0, sizeof e); memset(&scan, 0, sizeof scan); memset(raw, 0, sizeof raw);
    memset(sounding, 0, sizeof sounding); ons = offs = 0u; cc64[0] = cc64[1] = 0u;
    tm_reset();
    for (uint8_t s = 0; s < TC_CHORD_SETS; ++s) { tc_chord_set none; memset(&none, 0, sizeof none); assert(tc_chord_set_put(s, &none)); }
    sq_clear(&sq, SQ_UNIT_BAR, 0u);
    fs_erase(); (void)st_load(&tc_state);
    run(600); assert(e.state == TK_ACTIVE);
}
static void quiet(void) { /* nothing sounds, nothing owed */
    run(200);
    for (uint32_t ch = 0; ch < 16u; ++ch) assert(!heard(ch));
}

/* A set in C: C, Fmaj7, (none), Bbm, G7 an octave up; the rest empty. */
static tc_chord_set pop(void) {
    tc_chord_set s; memset(&s, 0, sizeof s);
    s.column[0] = (tc_set_chord){0x091u, 0u};
    s.column[1] = (tc_set_chord){0x891u, 5u};
    s.column[3] = (tc_set_chord){0x089u, 10u};
    s.column[4] = (tc_set_chord){0x491u, 19u};
    snprintf(s.name, sizeof s.name, "pop");
    return s;
}
static void notes_are(tp_layout l, uint32_t k, const uint8_t *want, uint32_t n) {
    tp_notes got; tc_chord none = {0, 0, 0};
    assert(tc_notes(l, k, none, &got) && got.count == n);
    for (uint32_t i = 0; i < n; ++i) assert(got.notes[i] == want[i]);
}
static void chord_sets(void) {
    fresh();
    tc_chord_set s = pop(), bad;
    assert(tc_chord_set_put(0, &s) && tc_chord_set_defined(0) && !tc_chord_set_defined(1));
    assert(!strcmp(tc_chord_set_get(0)->name, "POP"));
    /* Refused: no root bit, seven notes, a root two octaves up, an empty column with a root. */
    bad = s; bad.column[5] = (tc_set_chord){0x090u, 0u}; assert(!tc_chord_set_put(1, &bad));
    bad = s; bad.column[5] = (tc_set_chord){0x0FFu, 0u}; assert(!tc_chord_set_put(1, &bad));
    bad = s; bad.column[5] = (tc_set_chord){0x091u, 24u}; assert(!tc_chord_set_put(1, &bad));
    bad = s; bad.column[5] = (tc_set_chord){0u, 3u}; assert(!tc_chord_set_put(1, &bad));
    assert(!tc_chord_set_defined(1));
    /* In D: the rows climb the inversions, the root buttons transpose. */
    tp_layout l = TP_DEFAULT_LAYOUT; l.tonic = 2u; l.chords = 1u;
    assert(tc_layout_ok(l));
    static const uint8_t d0[3] = {38, 42, 45}, d1[3] = {42, 45, 50}, d2[3] = {45, 50, 54}, d3[3] = {50, 54, 57};
    notes_are(l, pad(0, 3), d0, 3); notes_are(l, pad(0, 2), d1, 3); notes_are(l, pad(0, 1), d2, 3); notes_are(l, pad(0, 0), d3, 3);
    static const uint8_t g0[4] = {43, 47, 50, 54}, g3[4] = {54, 55, 59, 62}, a7[4] = {57, 61, 64, 67};
    notes_are(l, pad(1, 3), g0, 4); notes_are(l, pad(1, 0), g3, 4); notes_are(l, pad(4, 3), a7, 4);
    tp_notes none_heard; tc_chord plain = {0, 0, 0};
    assert(tc_notes(l, pad(2, 3), plain, &none_heard) && !none_heard.count && tc_pad_empty(l, pad(2, 0)));
    assert(tc_pad_root(l, pad(1, 0)) == 7u && tc_pad_root(l, pad(3, 1)) == 0u && tc_pad_root(l, pad(4, 2)) == 9u);
    char name[TN_MAX]; tp_notes n;
    assert(tc_notes(l, pad(1, 3), plain, &n) && tn_name(&n, tc_pad_root(l, pad(1, 3)), name) && !strcmp(name, "Gmaj7"));
    assert(tc_notes(l, pad(3, 3), plain, &n) && tn_name(&n, tc_pad_root(l, pad(3, 3)), name) && !strcmp(name, "Cm"));
    assert(tc_notes(l, pad(4, 2), plain, &n) && tn_name(&n, tc_pad_root(l, pad(4, 2)), name) && !strcmp(name, "A7/C#"));
    /* Edits: sus and added notes don't apply to a set's chords; a lowered root does. */
    tc_chord shaped = {0u, 4u, TC_ADD7}, lowered = {1u, 0u, 0u};
    assert(tc_notes(l, pad(0, 3), shaped, &n) && n.count == 3u && n.notes[0] == 38u && n.notes[1] == 42u);
    assert(tc_notes(l, pad(0, 3), lowered, &n) && n.notes[0] == 26u);
    /* Fold: everything inside G#..G; guitar stands on the set's root; spread drops that root. */
    tp_layout f = l; f.fold = 1u;
    for (uint32_t k = 3u; k < 48u; k += 4u) { /* the bottom row: the folded chord itself */
        assert(tc_notes(f, k, plain, &n));
        for (uint32_t i = 0; i < n.count; ++i) assert(n.notes[i] >= 44u && n.notes[i] <= 55u);
    }
    assert(tc_notes(f, pad(1, 3), plain, &n) && n.count == 4u && n.notes[0] == 47u && n.notes[3] == 55u);
    tc_panel guitar = TC_PANEL_NEUTRAL, spread = TC_PANEL_NEUTRAL; guitar.voicing = TC_VOICING_GUITAR; spread.spread = 1u;
    assert(tc_notes_panel(l, pad(1, 3), plain, guitar, &n) && n.count);
    uint32_t low = 127u; for (uint32_t i = 0; i < n.count; ++i) if (n.notes[i] < low) low = n.notes[i];
    assert(low % 12u == 7u);
    assert(tc_notes_panel(l, pad(1, 3), plain, spread, &n) && n.notes[0] == 31u);
    /* Every key and row stays inside MIDI range at the default octaves; the top octave refuses. */
    for (uint8_t t = 0; t < 12u; ++t) {
        tp_layout k = l; k.tonic = t;
        for (uint32_t p = 0; p < 96u; ++p) assert(tc_notes(k, p, plain, &n));
    }
    tc_set_layout(&tc_state, &e, l);
    assert(tc_state.chord_set == 1u && e.layout.chords == 1u);
    tp_layout high = l; high.chord_base = TP_CHORD_BASE + TP_OCTAVE_RANGE;
    assert(tc_set_layout(&tc_state, &e, high) && tc_state.chord_base == high.chord_base); /* G7 up there still fits */
    assert(tc_set_layout(&tc_state, &e, l));
    /* On the pads: the empty column is silent and the board goes on; the others sound as typed. */
    touch(pad(2, 3), true); assert(!heard(0) && e.state == TK_ACTIVE && !e.errors); touch(pad(2, 3), false);
    touch(pad(1, 3), true); assert(hears(0, g0, 4)); touch(pad(1, 3), false);
    quiet();
    /* The Chords page: no edits on a set's chord; an empty set can't be picked. */
    tc_state.ui = TC_UI_EDIT; tc_state.group = TC_GROUP_CHORDS; tc_state.menu = TC_MENU_ADD7; tc_state.edit_value = 1u;
    tap_bit(0x36, NAV_CENTRE);
    assert(tc_state.last_action == TC_SET_NO_EDITS && !tc_state.chord[pad(1, 3)].add);
    tc_state.ui = TC_UI_EDIT; tc_state.menu = TC_MENU_CHORD_SET; tc_state.edit_value = 3u;
    tap_bit(0x36, NAV_CENTRE);
    assert(tc_state.last_action == TC_SET_EMPTY && tc_state.chord_set == 1u);
    tc_state.ui = TC_UI_EDIT; tc_state.menu = TC_MENU_CHORD_SET; tc_state.edit_value = 0u;
    tap_bit(0x36, NAV_CENTRE);
    assert(tc_state.chord_set == 0u && tc_state.last_action == TC_SETTING);
    tc_state.ui = TC_UI_PLAY;
    /* Stored: the record round-trips, a set in use can't be cleared, the next start has it all. */
    uint32_t p[ST_PAYLOAD], q[ST_PAYLOAD];
    assert(st_get(ST_CHORDS, 0, &tc_state, p) == ST_OK && p[0] == 1u && p[2] == (0x891u | 5u << 12));
    assert(st_get(ST_CHORDS, 1, &tc_state, q) == ST_ABSENT);
    assert(st_put(ST_CHORDS, 0, p, &tc_state, &e) == ST_OK); /* into flash */
    assert(st_put(ST_CHORDS, 1, p, &tc_state, &e) == ST_OK && tc_chord_set_defined(1));
    memcpy(q, p, sizeof q); q[5] = 0x090u; assert(st_put(ST_CHORDS, 2, q, &tc_state, &e) == ST_BAD_VALUE);
    memcpy(q, p, sizeof q); q[20] = 1u; assert(st_put(ST_CHORDS, 2, q, &tc_state, &e) == ST_BAD_VALUE);
    assert(!tc_chord_set_defined(2));
    assert(tc_set_layout(&tc_state, &e, l));
    uint32_t empty[ST_PAYLOAD] = {1u};
    assert(st_put(ST_CHORDS, 0, empty, &tc_state, &e) == ST_BAD_VALUE && tc_chord_set_defined(0)); /* in use */
    assert(st_put(ST_CHORDS, 1, empty, &tc_state, &e) == ST_OK && !tc_chord_set_defined(1));
    tc_state.changes++; run(ST_SAVE_DELAY_MS + 100u);
    tc_controls back; tc_init(&back);
    tc_chord_set none; memset(&none, 0, sizeof none); assert(tc_chord_set_put(0, &none));
    assert(st_load(&back) && back.chord_set == 1u && tc_chord_set_defined(0) && !tc_chord_set_defined(1));
    assert(!memcmp(tc_chord_set_get(0), &s, sizeof s) || !strcmp(tc_chord_set_get(0)->name, "POP"));
    assert(tc_chord_set_get(0)->column[4].root == 19u);
    printf("chord sets: C Fmaj7 - Bbm G7 in D = D Gmaj7 - Cm A7, inversions by row, fold, guitar, spread, stored\n");
}
static void progressions_keep_their_set(void) {
    fresh();
    tc_chord_set s = pop();
    assert(tc_chord_set_put(0, &s));
    tp_layout l = TP_DEFAULT_LAYOUT; l.chords = 1u;
    assert(tc_set_layout(&tc_state, &e, l));
    /* Recorded with the set by the buttons' Record (centre button 13 set to it). */
    tc_state.buttons[13] = TC_ACT_RECORD;
    tap_button(13); assert(tc_state.recording && sq.chords == 1u);
    touch(pad(1, 3), true); touch(pad(1, 3), false);
    tap_button(13); assert(!tc_state.recording && sq.count == 1u);
    run(100);
    uint32_t w[SQ_WORDS]; sq_prog back;
    sq_encode(&sq, w); assert(w[SQ_WORDS - 1u] == 1u && sq_decode(w, &back) && back.chords == 1u);
    /* Played with the scale's chords on the pads: still Fmaj7 (in C). */
    l.chords = 0u; assert(tc_set_layout(&tc_state, &e, l));
    static const uint8_t fmaj7[4] = {41, 45, 48, 52}, dm[3] = {38, 41, 45};
    tc_state.transport = true; run(300);
    assert(hears(0, fmaj7, 4));
    tc_state.transport = false; quiet();
    /* Its set cleared: the pad's scale chord (Dm). */
    uint32_t empty[ST_PAYLOAD] = {1u};
    assert(st_put(ST_CHORDS, 0, empty, &tc_state, &e) == ST_OK);
    sq_changes++; tc_state.transport = true; run(300);
    assert(hears(0, dm, 3));
    tc_state.transport = false; quiet();
    printf("progressions: keep the chord set they were recorded with; its scale chords once it is cleared\n");
}
/* A progression by the buttons: a set chosen on the Chords page, recorded with the
 * arrows (Sequencer) into slot 2, the pads back on the scale's chords, played with up. */
enum { NAV_LEFT = 8, NAV_UP = 7, NAV_DOWN = 1 };
static void set_item(uint8_t group, uint8_t item, uint8_t value) {
    tc_state.ui = TC_UI_MENU; tc_state.group = group; tc_state.menu = item;
    tap_bit(0x36, NAV_CENTRE); assert(tc_state.ui == TC_UI_EDIT);
    tc_state.edit_value = value; tap_bit(0x36, NAV_CENTRE);
    tap_bit(0x36, NAV_LEFT); tap_bit(0x36, NAV_LEFT); assert(tc_state.ui == TC_UI_PLAY);
}
/* 1.3.0 chord grid from the Chords page: rows are chord types, the pad's own edits stay out
 * (kept for the scale's chords), edits refused, held shapes add, a progression keeps it. */
static void chord_grid(void) {
    fresh();
    tc_state.chord[pad(0, 2)] = (tc_chord){0, 4, TC_ADD6}; /* an edit for the scale's chords */
    set_item(TC_GROUP_CHORDS, TC_MENU_CHORD_SET, TC_CHORDS_GRID); assert(tc_state.chord_set == TC_CHORDS_GRID);
    static const uint8_t c[3] = {36, 40, 43}, c7[4] = {36, 40, 43, 47}, csus[3] = {36, 41, 43}, c9[5] = {36, 40, 43, 47, 50};
    touch(pad(0, 3), true); assert(hears(0, c, 3)); touch(pad(0, 3), false);
    touch(pad(0, 2), true); assert(hears(0, c7, 4)); touch(pad(0, 2), false);
    touch(pad(0, 1), true); assert(hears(0, csus, 3)); touch(pad(0, 1), false);
    touch(pad(0, 0), true); assert(hears(0, c9, 5)); touch(pad(0, 0), false);
    /* Edits refused on the grid; the scale's chords keep theirs. */
    tc_state.ui = TC_UI_EDIT; tc_state.group = TC_GROUP_CHORDS; tc_state.menu = TC_MENU_ADD9; tc_state.edit_value = 1u;
    tap_bit(0x36, NAV_CENTRE);
    assert(tc_state.last_action == TC_SET_NO_EDITS && !(tc_state.chord[pad(0, 0)].add & TC_ADD9));
    tap_bit(0x36, NAV_LEFT); tap_bit(0x36, NAV_LEFT); assert(tc_state.ui == TC_UI_PLAY);
    /* Add 9th while held: the triad row plays Cadd9. */
    tc_state.buttons[2] = TC_ACT_ADD9;
    static const uint8_t cadd9[4] = {36, 40, 43, 50};
    hold_button(2, true); touch(pad(0, 3), true); assert(hears(0, cadd9, 4)); touch(pad(0, 3), false); hold_button(2, false);
    tc_buttons_default(&tc_state);
    set_item(TC_GROUP_CHORDS, TC_MENU_CHORD_SET, 0u); assert(tc_state.chord_set == 0u);
    tp_notes own; assert(tc_pad(tc_layout(&tc_state), pad(0, 2), &own) && own.count == 4u); /* its sus4 and 6th again */
    quiet();
    printf("chord grid: triad, 7th, sus, 9th up a column, edits refused and kept, held shapes add\n");
}
static void progression_by_the_buttons(void) {
    fresh();
    tc_chord_set s; memset(&s, 0, sizeof s);
    s.column[6] = (tc_set_chord){0x091u, 10u}; /* Bb */
    s.column[9] = (tc_set_chord){0x491u, 2u}; /* D7 */
    assert(tc_chord_set_put(0, &s));
    set_item(TC_GROUP_CHORDS, TC_MENU_CHORD_SET, 1u); assert(tc_state.chord_set == 1u);
    set_item(TC_GROUP_PLAY, TC_MENU_ARROWS, TC_ARROWS_SEQUENCER);
    set_item(TC_GROUP_SEQUENCE, TC_MENU_SEQ_SLOT, 1u); run(100);
    tp_notes bb; tc_chord none = {0, 0, 0};
    assert(tc_notes(tc_layout(&tc_state), pad(6, 3), none, &bb));
    tap_bit(0x36, NAV_DOWN); assert(tc_state.recording);
    touch(pad(6, 3), true); touch(pad(6, 3), false); touch(pad(9, 3), true); touch(pad(9, 3), false);
    tap_bit(0x36, NAV_DOWN); assert(!tc_state.recording && sq.count == 2u && sq.chords == 1u); run(100);
    set_item(TC_GROUP_CHORDS, TC_MENU_CHORD_SET, 0u); assert(tc_state.chord_set == 0u);
    tap_bit(0x36, NAV_UP); run(200);
    assert(hears(0, bb.notes, bb.count));
    tc_state.transport = false; quiet();
    printf("progressions: recorded with a set by the arrows, played on the scale's chords: the set's chords\n");
}
static uint32_t sum(tl_rgb c) { return (uint32_t)c.r + c.g + c.b; }
static uint64_t columns(const uint32_t *cols, uint32_t n) {
    uint64_t m = 0;
    for (uint32_t i = 0; i < n; ++i) for (uint32_t r = 0; r < 4u; ++r) m |= (uint64_t)1u << pad(cols[i], r);
    return m;
}
static void hints(void) {
    tp_layout l = TP_DEFAULT_LAYOUT; uint64_t also;
    /* C major: after C, F (strong); G and Am (also), in both octaves of the pads. */
    static const uint32_t f[2] = {3, 10}, g_am[3] = {4, 5, 11}, g[2] = {4, 11}, bdim[1] = {6};
    assert(tc_hints(l, pad(0, 3), &also) == columns(f, 2) && also == columns(g_am, 3));
    assert(tc_hints(l, pad(1, 0), &also) == columns(g, 2) && also == columns(bdim, 1)); /* Dm -> G, Bdim */
    assert(tc_hints(l, pad(7, 3), &also) == columns(f, 2) && also == columns(g_am, 3)); /* C an octave up: the same */
    /* A minor (M6) by scale step too: Am -> Dm (iv), Em, F. */
    l.mode = 1u; l.tonic = 9u;
    static const uint32_t dm[2] = {3, 10}, em_f[3] = {4, 5, 11};
    assert(tc_hints(l, pad(0, 3), &also) == columns(dm, 2) && also == columns(em_f, 3));
    /* C major pentatonic: no chord a fifth down from C, so G and A lead. */
    l.mode = 9u; l.tonic = 0u;
    static const uint32_t g_a[4] = {3, 4, 8, 9};
    assert(tc_hints(l, pad(0, 3), &also) == columns(g_a, 4) && !also);
    /* A chord set by root movement: from C, F a fifth down; G a fifth up, A a third down. */
    tc_chord_set s = pop(); s.column[5] = (tc_set_chord){0x089u, 9u}; /* Am */
    assert(tc_chord_set_put(0, &s));
    l = (tp_layout)TP_DEFAULT_LAYOUT; l.chords = 1u;
    static const uint32_t set_f[1] = {1}, set_g_am[2] = {4, 5};
    assert(tc_hints(l, pad(0, 3), &also) == columns(set_f, 1) && also == columns(set_g_am, 2));
    /* Lights: hint pads stand out, the other chord pads dim; nothing changes with hints off. */
    fresh();
    tc_state.have_chord = true; tc_state.last_chord = (uint8_t)pad(0, 3);
    ui_service(&tc_state, &e, &scan, true, now);
    tl_rgb off_f = pads[pad(3, 3)], off_g = pads[pad(4, 3)], off_d = pads[pad(1, 3)];
    tc_state.hints = 1u;
    uint32_t f_low = 255u, f_high = 0u;
    for (uint32_t t = 0; t < 1000u; t += 50u) {
        ui_service(&tc_state, &e, &scan, true, now + t);
        uint32_t level = sum(pads[pad(3, 3)]);
        if (level < f_low) f_low = level;
        if (level > f_high) f_high = level;
    }
    assert(f_high > f_low);                                                                  /* F pulses */
    assert(f_high >= sum(off_f));
    assert(sum(pads[pad(4, 3)]) >= sum(off_g));            /* G steady */
    assert(4u * sum(pads[pad(1, 3)]) <= sum(off_d) + 12u); /* Dm dim */
    for (uint32_t k = 48u; k < 96u; k += 5u) { tl_rgb before = pads[k]; tc_state.hints = 0u; ui_service(&tc_state, &e, &scan, true, now); assert(!memcmp(&before, &pads[k], sizeof before)); tc_state.hints = 1u; }
    tc_state.hints = 0u;
    printf("hints: C -> F (G Am), Dm -> G (Bdim), Am -> Dm (Em F), pentatonic C -> G A, sets by root; F pulses, others dim\n");
}
static void midi_note(uint8_t ch, uint8_t pitch, uint8_t velocity) {
    uint8_t packet[4] = {velocity ? 0x09u : 0x08u, (uint8_t)((velocity ? 0x90u : 0x80u) | ((ch - 1u) & 15u)), pitch, velocity};
    tm_packet(packet, now); run(20);
}
static void scale_lock(void) {
    fresh();
    tc_state.lock = 5u; tc_state.melody_ch = 1u; /* lock on channel 5, melody on 2 */
    run(20);
    midi_note(5, 61, 90);                          /* C# -> C (between two, the lower) */
    assert(sounding[1][60] && velocity_of[1][60] == 90u && heard(1) == 1u);
    midi_note(5, 66, 70); assert(sounding[1][65]);  /* F# -> F */
    midi_note(5, 64, 50); assert(sounding[1][64]);  /* E stays */
    midi_note(6, 62, 50); assert(heard(1) == 3u);   /* another channel: ignored */
    midi_note(5, 61, 0); assert(!sounding[1][60]);  /* Note On velocity 0 ends it */
    midi_note(5, 66, 0); midi_note(5, 64, 0); assert(!heard(1));
    /* A note keeps its pitch until it ends, whatever the key does meanwhile. */
    midi_note(5, 62, 80); assert(sounding[1][62]);
    tp_layout l = tc_layout(&tc_state); l.tonic = 1u; assert(tc_set_layout(&tc_state, &e, l)); run(50);
    assert(sounding[1][62]);
    midi_note(5, 62, 0); assert(!heard(1));
    midi_note(5, 62, 80); assert(sounding[1][61]);  /* D in C# major: C# (the lower of C# and D#) */
    midi_note(5, 62, 0);
    l.tonic = 0u; assert(tc_set_layout(&tc_state, &e, l));
    /* Striking again restarts the note. */
    uint32_t before = ons;
    midi_note(5, 64, 60); midi_note(5, 64, 100); assert(ons == before + 2u && velocity_of[1][64] == 100u);
    midi_note(5, 64, 0);
    /* Sixteen at once; the seventeenth waits for a free one. */
    for (uint8_t i = 0; i < 17u; ++i) midi_note(5, (uint8_t)(36u + 12u * (i / 7u) + (uint8_t[]){0, 2, 4, 5, 7, 9, 11}[i % 7u]), 64);
    assert(heard(1) == 16u);
    midi_note(5, 36, 0); assert(heard(1) == 16u && sounding[1][36u + 24u + 4u]);
    /* All notes off arriving ends them; turning the lock off ends them. */
    uint8_t all_off[4] = {0x0B, 0xB4, 123, 0}; tm_packet(all_off, now); run(30); assert(!heard(1));
    midi_note(5, 72, 64); assert(sounding[1][72]);
    tc_state.lock = 0u; run(30); assert(!heard(1));
    midi_note(5, 72, 0);
    /* Shared with a melody pad on the same note: one Note On, the Off when both have ended. */
    tc_state.lock = 5u; tc_state.melody_ch = 0u; run(20);
    midi_note(5, 48, 64); assert(sounding[0][48]);
    touch(48u + pad(0, 3), true); assert(sounding[0][48]);
    midi_note(5, 48, 0); assert(sounding[0][48]);
    touch(48u + pad(0, 3), false); assert(!sounding[0][48]);
    quiet();
    printf("scale lock: C# -> C, F# -> F on the melody channel with its velocity, other channels ignored, held pitch kept, "
           "re-strike, 16 at once, all-notes-off, shared notes\n");
}
static void centre_buttons(void) {
    fresh();
    /* Default: the printed panel functions. */
    tap_button(0); assert(tc_state.chord_voicing == 1u && tc_panel_active(&tc_state, 0)); tap_button(0);
    /* Arpeggiator on/off, lit while on. */
    tc_state.buttons[0] = TC_ACT_ARP;
    tap_button(0); assert(tc_state.arp == TC_ARP_UP && tc_panel_active(&tc_state, 0));
    tap_button(0); assert(tc_state.arp == TC_ARP_OFF && !tc_panel_active(&tc_state, 0));
    /* Toggles. */
    tc_state.buttons[1] = TC_ACT_HINTS; tap_button(1); assert(tc_state.hints); tap_button(1); assert(!tc_state.hints);
    tc_state.buttons[1] = TC_ACT_STRUM; tap_button(1); assert(tc_state.strum == TC_STRUM_UP); tap_button(1); assert(!tc_state.strum);
    tc_state.buttons[1] = TC_ACT_FOLD; tap_button(1); assert(tc_state.fold && e.layout.fold); tap_button(1); assert(!tc_state.fold);
    tc_state.buttons[1] = TC_ACT_VOICE_LEAD; tap_button(1); assert(tc_state.voice_lead); tap_button(1);
    tc_state.buttons[1] = TC_ACT_HOLD; tap_button(1); assert(tc_state.hold); tap_button(1); assert(!tc_state.hold);
    tc_state.buttons[1] = TC_ACT_OCT_UP; tap_button(1); assert(tc_state.melody_base == TP_MELODY_BASE + 12u);
    tc_state.buttons[1] = TC_ACT_OCT_DOWN; tap_button(1); assert(tc_state.melody_base == TP_MELODY_BASE);
    tc_state.buttons[1] = TC_ACT_SCALE_NEXT; tap_button(1); assert(tc_state.scale == 1u);
    tc_state.buttons[1] = TC_ACT_SCALE_PREV; tap_button(1); assert(tc_state.scale == 0u);
    /* Chord set: the chord grid, the Full layout, the sets that have chords, then back to the scale's. */
    tc_chord_set s = pop(); assert(tc_chord_set_put(2, &s));
    tc_state.buttons[1] = TC_ACT_CHORD_SET;
    tap_button(1); assert(tc_state.chord_set == TC_CHORDS_GRID && tc_panel_active(&tc_state, 1));
    tap_button(1); assert(tc_state.chord_set == TC_CHORDS_FULL && tc_panel_active(&tc_state, 1));
    tap_button(1); assert(tc_state.chord_set == 3u && tc_panel_active(&tc_state, 1));
    tap_button(1); assert(tc_state.chord_set == 0u);
    /* Tap tempo: half a second apart = 120 BPM. */
    tc_state.buttons[1] = TC_ACT_TAP; tc_state.tempo = 0u;
    tap_button(1); run(400); tap_button(1);
    assert(tc_tempo_bpm(tc_state.tempo) == 120u);
    /* Add 7th while held: the chord with it, then without; recorded with it. */
    tc_state.buttons[2] = TC_ACT_ADD7;
    static const uint8_t c7[4] = {36, 40, 43, 47}, c[3] = {36, 40, 43};
    hold_button(2, true); assert(tc_panel_active(&tc_state, 2));
    touch(pad(0, 3), true); assert(hears(0, c7, 4)); touch(pad(0, 3), false);
    hold_button(2, false); assert(!tc_panel_active(&tc_state, 2));
    touch(pad(0, 3), true); assert(hears(0, c, 3)); touch(pad(0, 3), false);
    assert(!tc_state.chord[pad(0, 3)].add);
    tc_state.buttons[3] = TC_ACT_SUS4; hold_button(3, true);
    static const uint8_t csus4[3] = {36, 41, 43};
    touch(pad(0, 3), true); assert(hears(0, csus4, 3)); touch(pad(0, 3), false); hold_button(3, false);
    /* Sustain: a tap turns CC64 on (lit), and it stays on with the button up (the touch
     * controllers let go of a long touch); a tap turns it off; All notes off ends it too. */
    tc_state.buttons[4] = TC_ACT_SUSTAIN;
    tap_button(4); assert(cc64[0] == 1u && !cc64[1] && tc_panel_active(&tc_state, 4));
    run(6000); assert(cc64[0] == 1u && !cc64[1]);
    tap_button(4); assert(cc64[0] == 1u && cc64[1] == 1u && !tc_panel_active(&tc_state, 4));
    tc_state.buttons[5] = TC_ACT_PANIC;
    tap_button(4); tap_button(5); run(50); assert(cc64[0] == 2u && cc64[1] == 2u && !tc_state.button_sustain);
    /* Off when no button is Sustain any more. */
    tap_button(4); assert(cc64[0] == 3u); tc_state.buttons[4] = 4u; run(50); assert(cc64[1] == 3u);
    /* All notes off: a held (latched) chord ends. */
    tc_state.buttons[5] = TC_ACT_PANIC; tc_state.hold = 1u;
    touch(pad(0, 3), true); touch(pad(0, 3), false); assert(hears(0, c, 3));
    tap_button(5); assert(!heard(0));
    tc_state.hold = 0u;
    quiet();
    /* Play / record buttons. */
    tc_state.buttons[6] = TC_ACT_PLAY; tap_button(6); assert(tc_state.transport); tap_button(6); assert(!tc_state.transport);
    printf("buttons: panel default, arp/hints/strum/fold/lead/hold toggles lit, octave, scale, chord set, tap 120 BPM, "
           "add 7th and sus4 while held, sustain on/off CC64, all notes off, play\n");
}
static void shift(void) {
    fresh();
    tc_state.buttons[13] = TC_ACT_SHIFT; tc_state.buttons[0] = TC_ACT_ARP;
    /* Progression P3 and preset 2 to play and load. */
    sq_clear(&sq, SQ_UNIT_BAR, 0u); assert(sq_tap(&sq, (uint8_t)pad(4, 3), 100u, 0u, (tc_chord){0, 0, 0}));
    uint32_t w[SQ_WORDS]; sq_encode(&sq, w);
    assert(st_put(ST_PROG, 2, w, &tc_state, &e) == ST_OK && (sq_present & 4u));
    sq_clear(&sq, SQ_UNIT_BAR, 0u);
    tc_state.tempo = 30u; tc_state.request = TC_REQUEST_SAVE; tc_state.request_slot = 1u; run(20);
    assert(tc_presets_present & 2u);
    tc_state.tempo = 16u;
    /* Shift + root button 3: P3 plays (G); again: it stops. */
    hold_button(13, true); assert(tc_shift(&tc_state) && tc_panel_active(&tc_state, 13));
    tap_bit(0x39, top_bit[2]);
    assert(tc_state.prog_slot == 2u && tc_state.transport && tc_state.tonic == 0u);
    run(300);
    static const uint8_t g[3] = {43, 47, 50};
    assert(hears(0, g, 3));
    tap_bit(0x39, top_bit[2]); assert(!tc_state.transport);
    run(100); assert(!heard(0));
    /* Shift + mode button 2: preset 2 (tempo 30); the scale stays. */
    tap_bit(0x38, top_bit[1]); assert(tc_state.tempo == 30u && tc_state.preset == 1u && tc_state.scale == 0u);
    tap_bit(0x38, top_bit[5]); assert(tc_state.last_action == TC_PRESET_EMPTY);
    tap_bit(0x38, top_bit[10]); assert(tc_state.last_action == TC_SHIFT_UNUSED);
    /* Shift + chord pad: chosen for the chord edits, silent. */
    touch(pad(5, 2), true); assert(!heard(0) && tc_state.last_chord == pad(5, 2) && tc_state.last_action == TC_PAD_CHOSEN);
    touch(pad(5, 2), false);
    /* Shift + a centre button: its printed panel function (Voicing I), not its action (arp). */
    tap_button(0); assert(tc_state.chord_voicing == 1u && tc_state.arp == TC_ARP_OFF);
    tap_button(0); assert(tc_state.chord_voicing == 0u);
    hold_button(13, false); assert(!tc_shift(&tc_state));
    touch(pad(5, 2), true); assert(heard(0) == 3u); touch(pad(5, 2), false);
    /* Latched by two taps, a tap lets go. */
    tap_button(13); run(100); tap_button(13); assert(tc_state.shift_latched && tc_shift(&tc_state));
    tap_bit(0x39, top_bit[0]); assert(tc_state.tonic == 0u && tc_state.prog_slot == 0u);
    /* Recording (▼ with Arrows Sequencer) with Shift latched: chord pads record, not choose. */
    tc_state.arrows = TC_ARROWS_SEQUENCER; tap_bit(0x36, 1u); assert(tc_state.recording);
    touch(pad(5, 3), true); touch(pad(5, 3), false); assert(sq.count == 1u);
    tap_bit(0x36, 1u); assert(!tc_state.recording && tc_state.last_action == TC_SEQ_SAVED);
    tap_button(13); assert(!tc_shift(&tc_state));
    tap_button(13); run(600); tap_button(13); assert(!tc_shift(&tc_state)); /* too slow */
    quiet();
    printf("shift: + root plays P3 and stops it, + mode loads preset 2, + chord pad chooses it silently, "
           "+ centre button = its panel function, double tap latches, recording with it latched records\n");
}
static void buttons_kept(void) {
    fresh();
    tc_state.buttons[0] = TC_ACT_SHIFT; tc_state.buttons[7] = TC_ACT_NONE; tc_state.hints = 1u; tc_state.lock = 16u;
    uint32_t p[ST_PAYLOAD];
    assert(st_get(ST_LIVE, 0, &tc_state, p) == ST_OK);
    assert(p[34] == (1u << 8 | 16u << 16) && p[35] == TC_ACT_SHIFT + 1u && p[36] == (uint32_t)(TC_ACT_NONE + 1u) << 24);
    tc_controls back; tc_init(&back);
    assert(st_put(ST_LIVE, 0, p, &back, &e) == ST_OK);
    assert(!memcmp(back.buttons, tc_state.buttons, sizeof back.buttons) && back.hints == 1u && back.lock == 16u);
    /* All defaults: the words are 0 (older firmware reads such a record). */
    tc_controls plain; tc_init(&plain);
    assert(st_get(ST_LIVE, 0, &plain, p) == ST_OK);
    for (uint32_t i = 34u; i < 39u; ++i) assert(!p[i]);
    /* Refused: an action that doesn't exist, the bytes after the 14th, hints 2, lock 17. */
    assert(st_get(ST_LIVE, 0, &tc_state, p) == ST_OK);
    uint32_t q[ST_PAYLOAD];
    memcpy(q, p, sizeof q); q[35] = TC_ACTIONS + 1u; assert(st_put(ST_LIVE, 0, q, &back, &e) == ST_BAD_VALUE);
    memcpy(q, p, sizeof q); q[38] |= 1u << 16; assert(st_put(ST_LIVE, 0, q, &back, &e) == ST_BAD_VALUE);
    memcpy(q, p, sizeof q); q[34] = 2u << 8; assert(st_put(ST_LIVE, 0, q, &back, &e) == ST_BAD_VALUE);
    memcpy(q, p, sizeof q); q[34] = 17u << 16; assert(st_put(ST_LIVE, 0, q, &back, &e) == ST_BAD_VALUE);
    memcpy(q, p, sizeof q); q[34] = 4u; assert(st_put(ST_LIVE, 0, q, &back, &e) == ST_BAD_VALUE); /* set 4 is empty */
    /* A preset keeps the board's buttons and lock, and brings its hints. */
    tc_state.request = TC_REQUEST_SAVE; tc_state.request_slot = 0u; run(20);
    tc_buttons_default(&tc_state); tc_state.lock = 3u; tc_state.hints = 0u;
    tc_state.request = TC_REQUEST_LOAD; tc_state.request_slot = 0u; run(20);
    assert(tc_state.preset == 0u && tc_state.buttons[0] == 0u && tc_state.lock == 3u && tc_state.hints == 1u);
    /* Settings > Buttons: pressing a centre button goes to its item, doing nothing else. */
    tc_state.ui = TC_UI_MENU; tc_state.group = TC_GROUP_BUTTONS; tc_state.menu = TC_MENU_BUTTON1;
    tap_button(5); assert(tc_state.menu == TC_MENU_BUTTON1 + 5u && !tc_state.chord_spread);
    tap_bit(0x36, NAV_CENTRE); assert(tc_state.ui == TC_UI_EDIT && tc_state.edit_value == 5u);
    tc_state.edit_value = TC_ACT_HOLD; tap_bit(0x36, NAV_CENTRE); assert(tc_state.buttons[5] == TC_ACT_HOLD);
    tc_state.menu = TC_MENU_BUTTONS_RESET; tap_bit(0x36, NAV_CENTRE); /* 1.4.0: asks first, on CANCEL */
    assert(tc_state.ui == TC_UI_EDIT && !tc_state.edit_value);
    tap_bit(0x36, NAV_CENTRE); assert(tc_state.buttons[5] == TC_ACT_HOLD && tc_state.last_action == TC_RESET_KEPT);
    tap_bit(0x36, NAV_CENTRE); tap_bit(0x36, NAV_UP); tap_bit(0x36, NAV_CENTRE);
    for (uint32_t i = 0; i < TC_FUNCTIONS; ++i) assert(tc_state.buttons[i] == i);
    assert(tc_state.last_action == TC_BUTTON && tc_state.last_button == TC_ACTIONS);
    printf("kept: buttons, hints and lock in words 34..38 (0 at defaults), invalid refused, presets keep the board's "
           "buttons and lock; Buttons page\n");
}
/* Random sets in every key, octave, fold, voicing, spread and octave stack: each pad either
 * refused as a whole layout or playing up to 16 notes inside MIDI range, named on screen; hints
 * never point at a set's empty column. */
static uint32_t seed = 0xC0DE0120u;
static uint32_t rnd(uint32_t n) { seed ^= seed << 13; seed ^= seed >> 17; seed ^= seed << 5; return n ? seed % n : 0u; }
static void random_sets(void) {
    uint32_t layouts = 0, refused = 0, results = 0;
    for (uint32_t round = 0; round < 3000u; ++round) {
        tc_chord_set set; memset(&set, 0, sizeof set);
        for (uint32_t i = 0; i < 12u; ++i) {
            if (!rnd(5u)) continue;
            uint16_t mask = 1u;
            for (uint32_t n = rnd(6u); n; --n) mask |= (uint16_t)(1u << (1u + rnd(11u)));
            set.column[i] = (tc_set_chord){mask, (uint8_t)rnd(TC_SET_ROOTS)};
        }
        assert(tc_chord_set_put(0, &set));
        if (!tc_chord_set_defined(0)) continue;
        tp_layout l = TP_DEFAULT_LAYOUT;
        l.chords = 1u; l.tonic = (uint8_t)rnd(12u); l.fold = (uint8_t)rnd(2u);
        l.chord_base = (uint8_t)(TP_CHORD_BASE - TP_OCTAVE_RANGE + 12u * rnd(5u));
        bool fits = true; tp_notes n; tc_chord plain = {0, 0, 0};
        for (uint32_t k = 0; k < 96u && fits; ++k) fits = tc_notes(l, k, plain, &n);
        if (!fits) { refused++; continue; }
        layouts++;
        tp_notes previous = {{0}, 0};
        for (uint32_t k = 0; k < 48u; ++k) {
            tc_panel panel = {(uint8_t)rnd(TC_VOICINGS), (uint8_t)rnd(TC_SPREADS), (uint8_t)(1u + rnd(127u)), (uint8_t)rnd(TC_CAPOS), 0u,
                              (uint8_t)(k & 1u)}; /* 1.7.0: Spread of Chord on every other pad (no extra draw) */
            tc_chord edit = {(uint8_t)rnd(8u), 0u, 0u};
            if (!tc_notes(l, k, edit, &n)) continue;
            /* Voice leading as the board picks it (layout.c live_panel): from the last chord. */
            if (rnd(2u) && panel.voicing != TC_VOICING_GUITAR && previous.count)
                panel.lead = (uint8_t)(TC_LEAD_ZERO + tc_lead_step(l, k, edit, panel.voicing, &previous));
            assert(tc_notes_panel(l, k, edit, panel, &n) && n.count <= TP_MAX_NOTES);
            for (uint32_t i = 0; i < n.count; ++i) assert(n.notes[i] <= 127u);
            char name[TN_MAX];
            assert(tn_name(&n, tc_pad_root(l, k), name) < TN_MAX);
            if (tc_pad_empty(l, k)) assert(!n.count);
            if (n.count) previous = n;
            results++;
        }
        uint64_t also, strong = tc_hints(l, rnd(48u), &also);
        for (uint32_t k = 0; k < 48u; ++k) if (((strong | also) >> k) & 1u) assert(!tc_pad_empty(l, k));
        assert(!(strong & also));
    }
    tc_chord_set none; memset(&none, 0, sizeof none); assert(tc_chord_set_put(0, &none));
    printf("random sets: %u layouts (%u refused as out of range), %u pad results in range, named, hints on chords only\n",
           layouts, refused, results);
}
/* The 1.0 code review's findings, each on the real code. */
static void review_findings(void) {
    fresh();
    tc_chord_set s = pop();
    assert(tc_chord_set_put(0, &s) && tc_chord_set_put(1, &s));
    /* While recording, the chord set stays: the button, the menu path, the editor, a preset. */
    tc_state.buttons[0] = TC_ACT_RECORD; tc_state.buttons[1] = TC_ACT_CHORD_SET;
    tc_state.tempo = 20u; tc_state.request = TC_REQUEST_SAVE; tc_state.request_slot = 0u; run(20); /* preset 1: scale chords */
    tc_state.tempo = 25u;
    tp_layout l = tc_layout(&tc_state); l.chords = 1u; assert(tc_set_layout(&tc_state, &e, l));
    tap_button(0); assert(tc_state.recording && sq.chords == 1u);
    tap_button(1); assert(tc_state.chord_set == 1u && tc_state.last_action == TC_SEQ_BUSY);
    l.chords = 2u; assert(!tc_set_layout(&tc_state, &e, l) && tc_state.chord_set == 1u);
    uint32_t live[ST_PAYLOAD]; assert(st_get(ST_LIVE, 0, &tc_state, live) == ST_OK);
    live[34] = (live[34] & ~255u) | 2u; assert(st_put(ST_LIVE, 0, live, &tc_state, &e) == ST_BAD_VALUE);
    tc_state.request = TC_REQUEST_LOAD; tc_state.request_slot = 0u; run(20);
    assert(tc_state.chord_set == 1u && tc_state.tempo == 25u && tc_state.last_action == TC_SEQ_BUSY);
    /* An empty column: silent and not a chord (not recorded, not the live or last chord). */
    touch(pad(0, 3), true); touch(pad(0, 3), false); assert(sq.count == 1u && tc_state.last_chord == pad(0, 3));
    touch(pad(2, 3), true); assert(!heard(0)); touch(pad(2, 3), false);
    assert(sq.count == 1u && tc_state.last_chord == pad(0, 3) && tc_state.live_pad == pad(0, 3));
    tap_button(0); assert(!tc_state.recording); run(100);
    l = tc_layout(&tc_state); l.chords = 2u; assert(tc_set_layout(&tc_state, &e, l)); /* free again */
    /* Shift latched while recording: the pads record and sound. */
    l.chords = 0u; assert(tc_set_layout(&tc_state, &e, l));
    tc_state.buttons[2] = TC_ACT_SHIFT; tap_button(2); run(100); tap_button(2); assert(tc_state.shift_latched);
    tc_state.arrows = TC_ARROWS_SEQUENCER; tap_bit(0x36, 1u); assert(tc_state.recording); /* Shift: button 0 is Voicing I */
    touch(pad(0, 3), true); assert(heard(0) == 3u && sq.count == 1u); touch(pad(0, 3), false);
    tap_bit(0x36, 1u); tap_button(2); assert(!tc_state.recording && !tc_shift(&tc_state)); quiet();
    /* Sustain on and off is not a setting: no flash write. */
    tc_state.buttons[3] = TC_ACT_SUSTAIN; tc_state.changes++; run(4000); /* earlier changes saved */
    uint32_t before = st_records(), changes = tc_state.changes;
    tap_button(3); run(4000); tap_button(3); run(4000);
    assert(tc_state.changes == changes && st_records() == before && cc64[0] == 1u && cc64[1] == 1u);
    /* A held Add 7th: the live area offers the 7th the pads play. */
    tc_state.buttons[4] = TC_ACT_ADD7;
    l = tc_layout(&tc_state); l.melody_layout = TP_MELODY_LIVE; assert(tc_set_layout(&tc_state, &e, l));
    hold_button(4, true);
    touch(pad(0, 3), true);
    tc_live_chord ch; tc_live_current(tc_layout(&tc_state), &ch);
    assert(heard(0) == 4u && ch.count == 4u && ch.tones[3] == 11u);
    touch(pad(0, 3), false); hold_button(4, false);
    tc_live_current(tc_layout(&tc_state), &ch); assert(ch.count == 3u);
    quiet();
    printf("review: no chord set change while recording (button, menu, editor, preset), empty columns do nothing, "
           "Shift latched records audibly, sustain writes no settings, the live area follows a held 7th\n");
}
/* 1.5.0: Chords on Right. The halves trade places on the board only: the right half plays the chord
 * pads, the left half the melody pads, the lights follow, everything else is as it was. */
static void chords_on_right(void) {
    fresh();
    tc_state.melody_ch = 1u; /* the melody on channel 2, to tell them apart */
    tp_notes c, mel;
    assert(tc_pad_with(e.layout, pad(0, 3), (tc_chord){0, 0, 0}, &c) && tc_notes(e.layout, 48u + pad(0, 3), (tc_chord){0, 0, 0}, &mel));
    /* The lights on the left: chord colors there before. */
    ui_service(&tc_state, &e, &scan, true, now);
    static tl_rgb before[TL_PADS]; memcpy(before, pads, sizeof before);
    set_item(TC_GROUP_LAYOUT, TC_MENU_CHORDS_SIDE, 1u); assert(tc_state.chords_right == 1u);
    /* The bottom-left pad of the right half: the C chord, on the chord channel. */
    touch(48u + pad(0, 3), true); assert(hears(0u, c.notes, c.count) && !heard(1u));
    touch(48u + pad(0, 3), false); quiet();
    /* The bottom-left pad of the left half: the melody's C3, on the melody channel. */
    touch(pad(0, 3), true); assert(hears(1u, mel.notes, 1u) && !heard(0u));
    touch(pad(0, 3), false); quiet();
    /* Each light is where its pad plays now. */
    ui_service(&tc_state, &e, &scan, true, now);
    for (uint32_t k = 0; k < TL_PADS; ++k) assert(!memcmp(&pads[k], &before[(k + 48u) % TL_PADS], sizeof pads[k]));
    /* A held chord pad when the halves change: it ends, nothing is left sounding. */
    touch(48u + pad(0, 3), true); assert(heard(0u));
    tc_state.chords_right = 0u; run(60); assert(!heard(0u));
    touch(48u + pad(0, 3), false); quiet();
    tc_state.chords_right = 1u;
    /* The step screen keeps its roles: the steps are on the left now. */
    tc_state.ui = TC_UI_MENU; tc_state.group = TC_GROUP_SEQUENCE; tc_state.menu = TC_MENU_STEPS;
    tap_bit(0x36, NAV_CENTRE); assert(tc_state.ui == TC_UI_STEPS);
    touch(0u, true); touch(0u, false); assert(ln.lanes[0].steps[0].on && !heard(0u) && !heard(1u));
    touch(0u, true); touch(0u, false); assert(!ln.lanes[0].steps[0].on);
    tap_bit(0x36, NAV_LEFT); tap_bit(0x36, NAV_LEFT); tap_bit(0x36, NAV_LEFT); assert(tc_state.ui == TC_UI_PLAY);
    /* A board setting: stored, and a preset doesn't change it. */
    set_item(TC_GROUP_PRESETS, TC_MENU_SAVE, 0u);
    set_item(TC_GROUP_LAYOUT, TC_MENU_CHORDS_SIDE, 0u);
    set_item(TC_GROUP_PRESETS, TC_MENU_LOAD, 0u); assert(tc_state.chords_right == 0u);
    set_item(TC_GROUP_LAYOUT, TC_MENU_CHORDS_SIDE, 1u);
    run(ST_SAVE_DELAY_MS + 100u);
    tc_controls back; tc_init(&back); assert(st_load(&back) && back.chords_right == 1u);
    /* Reset options on the Layout page puts the chords back on the left. */
    tc_state.ui = TC_UI_MENU; tc_state.group = TC_GROUP_LAYOUT; tc_state.menu = TC_MENU_RESET_PAGE;
    tap_bit(0x36, NAV_CENTRE); tap_bit(0x36, NAV_UP); tap_bit(0x36, NAV_CENTRE);
    assert(tc_state.chords_right == 0u && tc_state.last_action == TC_OPTIONS_RESET);
    printf("chords on right: chords on the right half, melody on the left, lights moved, a held pad ends, the "
           "step screen, a board setting kept by presets, stored, reset; ");
}
/* 1.5.0: the step sequencer as a player meets it. */
static void step_sequencer(void) {
    enum { LEFT = 8, UP = 7, CENTRE = 6, DOWN = 1, RIGHT = 3 };
    fresh();
    /* Lane 2: melody, on channel 2, 8 steps of 1/8, through the Sequence page. */
    set_item(TC_GROUP_SEQUENCE, TC_MENU_LANE, 1u);
    set_item(TC_GROUP_SEQUENCE, TC_MENU_LANE_KIND, LN_MELODY);
    set_item(TC_GROUP_SEQUENCE, TC_MENU_LANE_CHANNEL, 2u);
    set_item(TC_GROUP_SEQUENCE, TC_MENU_LANE_LENGTH, 7u);
    set_item(TC_GROUP_SEQUENCE, TC_MENU_LANE_RATE, 1u);
    const ln_lane *mel = &ln.lanes[1];
    assert(tc_state.lane == 1u && mel->kind == LN_MELODY && mel->channel == 2u && ln_length(mel) == 8u && ln_rate(mel) == 1u);
    /* Into the step screen by the buttons: centre (the pages), down to Sequence, centre, down to Steps,
     * centre. It opens on the lane chosen. */
    tap_bit(0x36, CENTRE); assert(tc_state.ui == TC_UI_GROUPS);
    while (tc_state.group != TC_GROUP_SEQUENCE) tap_bit(0x36, DOWN);
    tap_bit(0x36, CENTRE); assert(tc_state.ui == TC_UI_MENU && tc_state.menu == TC_MENU_SEQ_SLOT);
    while (tc_state.menu != TC_MENU_STEPS) tap_bit(0x36, DOWN);
    tap_bit(0x36, CENTRE); assert(tc_state.ui == TC_UI_STEPS && tc_state.lane == 1u);
    /* Step record: C3, E3, G3 (the left half as the right half's Scale layout), a rest, done. */
    tap_bit(0x36, CENTRE); assert(tc_state.step_rec);
    touch(pad(0, 3), true); touch(pad(0, 3), false);
    touch(pad(2, 3), true); touch(pad(2, 3), false);
    touch(pad(4, 3), true); touch(pad(4, 3), false);
    tap_bit(0x36, RIGHT); tap_bit(0x36, CENTRE); assert(!tc_state.step_rec);
    assert(mel->steps[0].on && mel->steps[1].on && mel->steps[2].on && !mel->steps[3].on && tc_state.step_cursor == 4u);
    quiet();
    /* Lane 1 (chords, 16 of 1/16): the I chord on its first step, by holding the step and touching the pad. */
    tap_bit(0x36, UP); assert(tc_state.lane == 0u);
    touch(48u, true); touch(pad(0, 3), true); touch(pad(0, 3), false); touch(48u, false);
    assert(ln.lanes[0].steps[0].on && ln.lanes[0].steps[0].value == pad(0, 3));
    quiet();
    /* Back out, then play with a centre button set to Play, next to a one-chord progression (Am). */
    tap_bit(0x36, LEFT); tap_bit(0x36, LEFT); tap_bit(0x36, LEFT); assert(tc_state.ui == TC_UI_PLAY);
    sq_clear(&sq, SQ_UNIT_BAR, tc_state.scale); assert(sq_tap(&sq, (uint8_t)pad(5, 3), 100, tc_state.scale, (tc_chord){0, 0, 0})); sq_changes++;
    tc_state.buttons[0] = TC_ACT_PLAY; logged = 0u;
    tap_button(0); assert(tc_state.transport);
    run(2100);
    /* The melody: C3 E3 G3 (rest) then nothing to step 8, again; every 250 ms on channel 2. */
    uint8_t melody[8]; uint32_t at[8], n = 0;
    for (uint32_t i = 0; i < logged && n < 8u; ++i) if (on_ch[i] == 1u) { melody[n] = on_note[i]; at[n++] = on_at[i]; }
    assert(n >= 4u && melody[0] == 48u && melody[1] == 52u && melody[2] == 55u && melody[3] == 48u);
    /* (steps 1 and 9 wait behind the chords' Note Ons, a packet a millisecond) */
    assert(at[2] - at[1] >= 248u && at[2] - at[1] <= 252u && at[3] - at[0] >= 1996u && at[3] - at[0] <= 2004u);
    /* The chord lane's C and the progression's Am together on the chord channel. */
    uint32_t classes = 0;
    for (uint32_t i = 0; i < logged; ++i) if (on_ch[i] == 0u) classes |= 1u << (on_note[i] % 12u);
    assert(classes == (1u << 0 | 1u << 4 | 1u << 7 | 1u << 9)); /* C E G + A */
    /* A root button (D) transposes the pattern: the melody now from D3. */
    logged = 0u; tap_bit(0x39, top_bit[2]); run(2600);
    n = 0;
    for (uint32_t i = 0; i < logged && n < 8u; ++i) if (on_ch[i] == 1u) melody[n++] = on_note[i];
    bool d = false;
    for (uint32_t i = 0; i + 2u < n; ++i) d = d || (melody[i] == 50u && melody[i + 1u] == 54u && melody[i + 2u] == 57u);
    assert(d);
    tap_button(0); assert(!tc_state.transport); quiet();
    printf("step sequencer: Sequence page lane settings, the step screen by the buttons, step record, hold + pad, "
           "Play next to a progression, transposed by a root button; ");
}
/* Colors > Rainbow (1.5.0): seven colors across the columns, the same on every row of both halves,
 * whatever the key, scale, layout or hints: red, orange, yellow, green, dark blue, light blue, magenta,
 * then red, orange, yellow, green, dark blue again; steady; a touched pad white; Chords on Right
 * changes nothing. */
static void rainbow_lights(void) {
    fresh();
    tc_state.tonic = 2u; tc_state.scale = 1u; tc_state.melody_layout = TP_MELODY_DRUMS; /* D minor, drums */
    tc_state.hints = 1u; tc_state.have_chord = true; tc_state.last_chord = (uint8_t)pad(0, 3);
    ui_service(&tc_state, &e, &scan, true, now);
    tl_rgb by_note = pads[pad(0, 0)];
    tc_state.led_scheme = TC_LEDS_ALL_RAINBOW;
    ui_service(&tc_state, &e, &scan, true, now);
    tl_rgb first[TL_PADS]; memcpy(first, pads, sizeof first);
    tl_rgb column[12];
    for (uint32_t i = 0; i < 12u; ++i) column[i] = first[pad(i, 0)];
    assert(column[0].r && !column[0].g && !column[0].b);                                   /* red */
    assert(memcmp(&by_note, &column[0], sizeof by_note));                            /* not D's color */
    assert(column[1].r > column[1].g && column[1].g && !column[1].b);                        /* orange */
    assert(column[2].r && column[2].g > column[2].r && !column[2].b);                        /* yellow (C mode 12's 4th column) */
    assert(!column[3].r && column[3].g && !column[3].b);                                   /* green */
    assert(!column[4].r && !column[4].g && column[4].b);                                   /* dark blue */
    assert(!column[5].r && column[5].g && column[5].b);                                    /* light blue */
    assert(column[6].r > column[6].b && column[6].b && !column[6].g);                        /* magenta */
    assert(column[7].r && !column[7].g && !column[7].b && column[0].r > column[7].r);           /* red again; column 1 brightest */
    for (uint32_t i = 8; i < 12u; ++i) assert(!memcmp(&column[i], &column[i - 7u], sizeof column[i])); /* orange .. dark blue again */
    for (uint32_t i = 1; i < 7u; ++i) for (uint32_t j = i + 1u; j < 7u; ++j) assert(memcmp(&column[i], &column[j], sizeof column[i]));
    for (uint32_t k = 0; k < TL_PADS; ++k) {
        uint32_t col = (k % 48u) / 4u;
        assert(!memcmp(&first[k], &first[pad(col, 0)], sizeof first[k]));          /* every row, both halves */
        if (col) assert(memcmp(&first[k], &first[pad(col - 1u, 0)], sizeof first[k]));
    }
    for (uint32_t t = 50u; t < 1000u; t += 50u) {                                 /* no pulsing hints */
        ui_service(&tc_state, &e, &scan, true, now + t); assert(!memcmp(first, pads, sizeof first));
    }
    scan.values[pad(5, 2)] = 3000u; run(60); ui_service(&tc_state, &e, &scan, true, now);
    tl_rgb w = pads[pad(5, 2)]; assert(w.r && w.r == w.g && w.g == w.b);           /* touched: white */
    touch(pad(5, 2), false); quiet();
    tc_state.chords_right = 1u; ui_service(&tc_state, &e, &scan, true, now);
    assert(!memcmp(first, pads, sizeof first));
    tc_state.chords_right = 0u; tc_state.hints = 0u; tc_state.led_scheme = TC_LEDS_RAINBOW;
    printf("rainbow colors: red, orange, yellow, green, dark blue, light blue, magenta, then red to dark blue again, "
           "on every row of both halves, any key, steady, touched white\n");
}
int main(void) {
    rainbow_lights();
    chords_on_right();
    step_sequencer();
    random_sets();
    chord_sets();
    progressions_keep_their_set();
    progression_by_the_buttons();
    chord_grid();
    hints();
    scale_lock();
    centre_buttons();
    shift();
    buttons_kept();
    review_findings();
    printf("PASS: 1.0.0 features: chord sets (pads, rows, fold, guitar, spread, names, edits refused, stored, kept by "
           "progressions), theory hints (scale steps, root movement, lights), scale lock (nearest note, channel, "
           "velocity, held pitch, 16 voices), centre-button actions and Shift, all stored\n");
    return 0;
}
