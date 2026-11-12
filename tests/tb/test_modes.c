/* SPDX-License-Identifier: GPL-3.0-or-later - 2.3.0, Modes (modal interchange): each chord row its own scale on the
 * same root, the top row the layout's. C major pad by pad, then every scale, key and row scale against that scale's
 * own chords in root position with the same edits (fold too); borrowed chords;
 * hints by column; the controls (Chord set, Row 2..4, Mel follows, the chord-set button, a held pad + a mode button,
 * per-chord edits, Reset options); retired tuned scales (2.3.3) read back; the melody following the last row through the engine
 * (what a synth hears, scale lock); the lights (borrowed chords stand out) and the screen (Eb MINOR); storage (words
 * 22 and 30, defaults as zeros, presets, a restart). */
#include "../../src/tb/settings.h"
#include "../../src/tb/sequence.h"
#include "../../src/tb/midi_in.h"
#include "../../src/tb/flash_store.h"
#include "../../src/tb/chord_name.h"
#include "../../src/tb/display.h"
#include "../../src/tb/leds.h"
#include "../../src/tb/jack.h"
#include "../../src/tb/nor.h"
#include "../../src/tb/ui_internal.h"
#include "../../src/tb/controls_internal.h"
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
bool tl_init(void) { return true; }
void tl_set_pad(uint32_t pad, tl_rgb c) { if (pad < TL_PADS) pads[pad] = c; }
void tl_set_button(uint32_t channel, uint16_t level) { (void)channel; (void)level; }
void tl_service(void) {}
uint32_t jh_levels(void) { return 5u; }
bool td_init(void) { return true; }
void td_pixel(uint32_t x, uint32_t y, bool on) { (void)x; (void)y; (void)on; }
void td_invert(uint32_t x0, uint32_t y0, uint32_t x1, uint32_t y1) { (void)x0; (void)y0; (void)x1; (void)y1; }
void td_service(void) {}
static char drawn[16][64];
static uint32_t drawn_scale[16], drawn_n;
void td_clear(void) { drawn_n = 0u; }
uint32_t td_text(uint32_t x, uint32_t y, const char *text, uint32_t scale) {
    (void)y;
    uint32_t end = x + 6u * scale * (uint32_t)strlen(text);
    /* 2.3.4 (board test D9): every text inside the screen; a glyph's last column is blank, so the last one may reach past */
    if (end > TD_WIDTH + scale) fprintf(stderr, "off the screen: \"%s\" at x %u, size %u\n", text, x, scale);
    assert(end <= TD_WIDTH + scale);
    if (drawn_n < 16u) { snprintf(drawn[drawn_n], sizeof drawn[0], "%s", text); drawn_scale[drawn_n] = scale; drawn_n++; }
    return end;
}
static uint32_t scale_of(const char *text) { /* the size it was drawn at, 0 if not drawn */
    for (uint32_t i = 0; i < drawn_n; ++i) if (!strcmp(drawn[i], text)) return drawn_scale[i];
    return 0u;
}
static uint32_t sum(tl_rgb c) { return (uint32_t)c.r + c.g + c.b; }
static bool on_screen(const char *text) {
    for (uint32_t i = 0; i < drawn_n; ++i) if (!strcmp(drawn[i], text)) return true;
    return false;
}

static tp_engine e;
static ts_scan scan;
static uint32_t now = 1000u;
static uint8_t sounding[16][128];
static bool writer(const uint8_t p[4], uint32_t token) {
    (void)token;
    uint32_t ch = p[1] & 15u;
    if (p[0] == TK_ON) { assert(!sounding[ch][p[2]]); sounding[ch][p[2]] = 1u; }
    if (p[0] == TK_OFF) { assert(sounding[ch][p[2]]); sounding[ch][p[2]] = 0u; }
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
        if (e.pending_id) tp_complete(&e, e.pending_id, tp_pending_bytes(&e), true, now);
        st_requests(&tc_state, &e);
        st_service(&tc_state, now);
    }
}
/* The mode buttons' touch-controller bits (0x38), by mode-button index (0 the 5, 1 the 12, 2..8 M1..M7). */
static const uint8_t mode_bit[12] = {3, 1, 0, 4, 5, 6, 7, 8, 9, 10, 11, 12};
static void tap_mode(uint32_t i) {
    raw[client(0x38)] |= 1u << mode_bit[i]; run(50);
    raw[client(0x38)] &= ~(1u << mode_bit[i]); run(50);
}
static void touch(uint32_t k, bool down) { scan.values[k] = down ? 3000u : 0u; run(60); }
static uint32_t pad(uint32_t column, uint32_t row) { return 4u * column + row; } /* row 0 the top */
static uint32_t melody(uint32_t column, uint32_t row) { return 48u + pad(column, row); }
static void fresh(void) {
    tc_init(&tc_state); memset(&e, 0, sizeof e); memset(&scan, 0, sizeof scan); memset(raw, 0, sizeof raw);
    memset(sounding, 0, sizeof sounding);
    tm_reset();
    fs_erase(); (void)st_load(&tc_state);
    tc_state.vel_mode = tc_state.melody_vel = TC_VEL_FIXED; tc_state.unstick = 0u;
    run(600); assert(e.state == TK_ACTIVE);
}
static tp_layout modes(uint8_t scale, uint8_t key) {
    tp_layout l = TP_DEFAULT_LAYOUT; l.mode = scale; l.tonic = key; l.chords = (uint8_t)TC_CHORDS_MODES;
    return l;
}
static void notes_are(tp_layout l, uint32_t k, tc_chord edit, const uint8_t *want, uint32_t n) {
    tp_notes got;
    assert(tc_notes(l, k, edit, &got) && got.count == n);
    for (uint32_t i = 0; i < n; ++i) assert(got.notes[i] == want[i]);
}
static const tc_chord none = {0, 0, 0};
static uint32_t seed = 0x2300u;
static uint32_t rnd(uint32_t n) { seed ^= seed << 13; seed ^= seed >> 17; seed ^= seed << 5; return n ? seed % n : 0u; }

/* --- the layout -------------------------------------------------------------------------------- */
static void layout(void) {
    tc_init(&tc_state);
    assert(tc_chords_ok((uint8_t)TC_CHORDS_MODES) && !tc_chords_ok((uint8_t)(TC_CHORDS_MODES + 1u)));
    /* C: the top row Major, then the defaults Minor (Aeolian), Mixolydian, Dorian. */
    tp_layout c = modes(0u, 0u);
    assert(tc_layout_ok(c) && tc_row_scale(c, 0u) == 0u && tc_row_scale(c, 1u) == 1u && tc_row_scale(c, 2u) == 5u &&
           tc_row_scale(c, 3u) == 2u && tc_melody_scale(c) == 0u);
    { const uint8_t C[] = {36, 40, 43}, Em[] = {40, 43, 47}, Bdim[] = {47, 50, 53};
      notes_are(c, pad(0, 0), none, C, 3); notes_are(c, pad(2, 0), none, Em, 3); notes_are(c, pad(6, 0), none, Bdim, 3); }
    { const uint8_t Cm[] = {36, 39, 43}, Eb[] = {39, 43, 46}, Ab[] = {44, 48, 51}, Bb[] = {46, 50, 53};
      notes_are(c, pad(0, 1), none, Cm, 3); notes_are(c, pad(2, 1), none, Eb, 3); notes_are(c, pad(5, 1), none, Ab, 3);
      notes_are(c, pad(6, 1), none, Bb, 3); }
    { const uint8_t Edim[] = {40, 43, 46}, Gm[] = {43, 46, 50}, Bb[] = {46, 50, 53};
      notes_are(c, pad(2, 2), none, Edim, 3); notes_are(c, pad(4, 2), none, Gm, 3); notes_are(c, pad(6, 2), none, Bb, 3); }
    { const uint8_t Dm[] = {38, 41, 45}, Adim[] = {45, 48, 51}, F[] = {41, 45, 48};
      notes_are(c, pad(1, 3), none, Dm, 3); notes_are(c, pad(5, 3), none, Adim, 3); notes_are(c, pad(3, 3), none, F, 3); }
    /* Column 7 on: the next octave, as the scale's chords. */
    { const uint8_t C8[] = {48, 52, 55}, Cm8[] = {48, 51, 55};
      notes_are(c, pad(7, 0), none, C8, 3); notes_are(c, pad(7, 1), none, Cm8, 3); }
    /* The user's example: Dorian and Lydian rows */
    c.rows[0] = 2u; c.rows[1] = 4u;
    { const uint8_t Eb[] = {39, 43, 46}, D[] = {38, 42, 45}, Fsdim[] = {42, 45, 48}, Bm[] = {47, 50, 54};
      notes_are(c, pad(2, 1), none, Eb, 3); notes_are(c, pad(1, 2), none, D, 3); notes_are(c, pad(3, 2), none, Fsdim, 3);
      notes_are(c, pad(6, 2), none, Bm, 3); }
    /* Edits: Eb with its seventh in Minor (D), Cm sus4, a lowered root */
    { const uint8_t Ebmaj7[] = {39, 43, 46, 50}, Csus4[] = {36, 41, 43}, low[] = {27, 31, 34};
      tc_chord seventh = {0, 0, TC_ADD7}, sus = {0, 4, 0}, lowered = {7, 0, 0};
      c = modes(0u, 0u);
      notes_are(c, pad(2, 1), seventh, Ebmaj7, 4); notes_are(c, pad(0, 1), sus, Csus4, 3); notes_are(c, pad(2, 1), lowered, low, 3); }
    /* Every scale, key and row scale: a row plays its scale's own chords in root position, edits and fold as they do. */
    uint32_t checked = 0;
    for (uint32_t i = 0; i < 4000u; ++i) {
        uint8_t main_scale, rows[3];
        do main_scale = (uint8_t)rnd(TC_ALL_SCALES); while (!tc_scale_defined(main_scale));
        for (uint32_t r = 0; r < 3u; ++r) do rows[r] = (uint8_t)rnd(TC_ALL_SCALES); while (!tc_scale_defined(rows[r]));
        tp_layout l = modes(main_scale, (uint8_t)rnd(12u));
        memcpy(l.rows, rows, 3u); l.fold = (uint8_t)rnd(2u);
        l.chord_base = (uint8_t)(TP_CHORD_BASE - TP_OCTAVE_RANGE + 12u * rnd(5u));
        uint32_t k = rnd(48u), row = k % 4u;
        tc_chord edit = rnd(2u) ? none : (tc_chord){(uint8_t)rnd(8u), (uint8_t)(2u * rnd(3u)), (uint8_t)rnd(8u)};
        tp_layout plain = l; plain.chords = 0u; plain.mode = row ? rows[row - 1u] : main_scale;
        tp_notes a, b;
        bool ok_a = tc_notes(l, k, edit, &a), ok_b = tc_notes(plain, 4u * (k / 4u) + 3u, edit, &b); /* its bottom row: root position */
        assert(ok_a == ok_b);
        if (ok_a) { assert(a.count == b.count && !memcmp(a.notes, b.notes, a.count)); checked++; }
        assert(tc_pad_root(l, k) == tc_pad_root(plain, 4u * (k / 4u) + 3u));
    }
    assert(checked > 2000u);
    /* A row scale that isn't there (an empty user slot) plays the top row's. */
    c = modes(0u, 0u); c.rows[0] = TC_SCALES; assert(!tc_scale_defined(TC_SCALES) && tc_row_scale(c, 1u) == 0u);
    /* Outside Modes the rows don't matter. */
    tp_layout g = c; g.chords = 0u; assert(tc_row_scale(g, 2u) == 0u);
    puts("layout: C's rows (Major, Minor, Mixolydian, Dorian; Dorian and Lydian), edits, every scale/key/row against its "
         "own chords in root position");
}
static void borrowed_and_hints(void) {
    tp_layout c = modes(0u, 0u);
    assert(tc_pad_borrowed(c, pad(2, 1), none) && tc_pad_borrowed(c, pad(6, 2), none) && tc_pad_borrowed(c, pad(0, 1), none));
    assert(!tc_pad_borrowed(c, pad(1, 3), none) && !tc_pad_borrowed(c, pad(0, 2), none)); /* Dm in Dorian, C in Mixolydian */
    assert(tc_pad_borrowed(c, pad(3, 1), none) && tc_pad_borrowed(c, pad(4, 3), none)); /* Fm in Minor (Ab), Gm in Dorian (Bb) */
    assert(!tc_pad_borrowed(c, pad(3, 3), none) && !tc_pad_borrowed(c, pad(5, 2), none)); /* F, Am */
    for (uint32_t col = 0; col < 12u; ++col) assert(!tc_pad_borrowed(c, pad(col, 0), none)); /* the top row: never */
    tc_chord seventh = {0, 0, TC_ADD7};
    assert(!tc_pad_borrowed(c, pad(1, 3), seventh)); /* Dm7 in Dorian: D F A C, all C major's */
    assert(tc_pad_borrowed(c, pad(0, 2), seventh)); /* C7 in Mixolydian: Bb */
    tp_layout g = c; g.chords = TC_CHORDS_GRID; assert(!tc_pad_borrowed(g, pad(2, 1), none));
    /* Hints by column, every row: after I, IV strong; V and vi also */
    uint64_t also = 0, strong = tc_hints(c, pad(0, 2), &also);
    for (uint32_t row = 0; row < 4u; ++row) {
        assert((strong >> pad(3, row)) & 1u);
        assert(((also >> pad(4, row)) & 1u) && ((also >> pad(5, row)) & 1u));
    }
    puts("borrowed: a note outside the top row's scale (edits too), never the top row; hints by column on every row");
}

/* --- the controls and the engine --------------------------------------------------------------- */
static bool shown(uint8_t group, uint8_t item) {
    uint8_t items[TC_GROUP_MAX];
    for (uint32_t i = 0, n = tc_page_items(&tc_state, group, items); i < n; ++i) if (items[i] == item) return true;
    return false;
}
static void value_is(uint8_t item, const char *text) {
    ui_value_name(item, tc_menu_value(&tc_state, item));
    if (strcmp(ui_buffer, text)) fprintf(stderr, "%s: %s, not %s\n", ui_item_name(item), ui_buffer, text);
    assert(!strcmp(ui_buffer, text));
}
static void controls(void) {
    fresh();
    assert(!shown(TC_GROUP_CHORDS, TC_MENU_ROW2) && !shown(TC_GROUP_CHORDS, TC_MENU_MELODY_FOLLOW));
    assert(tc_set_item(&tc_state, &e, TC_MENU_CHORD_SET, TC_CHORDS_MODES) && tc_state.chord_set == TC_CHORDS_MODES);
    assert(e.layout.chords == TC_CHORDS_MODES);
    value_is(TC_MENU_CHORD_SET, "MODES");
    assert(shown(TC_GROUP_CHORDS, TC_MENU_ROW2) && shown(TC_GROUP_CHORDS, TC_MENU_ROW4) && shown(TC_GROUP_CHORDS, TC_MENU_MELODY_FOLLOW));
    value_is(TC_MENU_ROW2, "MINOR"); value_is(TC_MENU_ROW3, "MIXOLYDIAN"); value_is(TC_MENU_ROW4, "DORIAN");
    value_is(TC_MENU_MELODY_FOLLOW, "MAIN");
    assert(!strcmp(ui_item_name(TC_MENU_ROW2), "ROW 2") && !strcmp(ui_item_name(TC_MENU_MELODY_FOLLOW), "MEL FOLLOWS"));
    assert(tc_menu_values(TC_MENU_ROW2) == tc_menu_values(TC_MENU_SCALE) && tc_cc_mappable(TC_MENU_ROW3));
    /* Row 3 Lydian through the menu: the pads follow */
    assert(tc_set_item(&tc_state, &e, TC_MENU_ROW3, tc_scale_position(4u)) && tc_state.mode_rows[1] == 4u && e.layout.rows[1] == 4u);
    value_is(TC_MENU_ROW3, "LYDIAN");
    /* The chord-set button: Scale, Grid, Full, Modes, (sets), Scale */
    tc_state.chord_set = 0u; e.layout = tc_layout(&tc_state);
    tc_state.buttons[1] = TC_ACT_CHORD_SET;
    for (uint32_t i = 0; i < 3u; ++i) tc_button(&tc_state, &e, 1u, now);
    assert(tc_state.chord_set == TC_CHORDS_MODES);
    tc_button(&tc_state, &e, 1u, now); assert(tc_state.chord_set == 0u);
    tc_button(&tc_state, &e, 1u, now); tc_button(&tc_state, &e, 1u, now); tc_button(&tc_state, &e, 1u, now);
    assert(tc_state.chord_set == TC_CHORDS_MODES);
    /* What a synth hears: Eb on row 2 (Minor), then a pad edit applies (Chord edits page) */
    const uint8_t Eb[] = {39, 43, 46}, Ebmaj7[] = {39, 43, 46, 50};
    touch(pad(2, 1), true); assert(hears(tc_state.chord_ch, Eb, 3)); touch(pad(2, 1), false);
    assert(tc_state.have_chord && tc_state.last_chord == pad(2, 1));
    tc_state.ui = TC_UI_EDIT; tc_state.group = TC_GROUP_CHORD_EDITS; tc_state.menu = TC_MENU_ADD7; tc_state.edit_value = 1u;
    tc_navigate(&tc_state, &e, TC_NAV_ENTER); assert(tc_state.last_action == TC_CHORD_EDIT); tc_state.ui = TC_UI_PLAY;
    touch(pad(2, 1), true); assert(hears(tc_state.chord_ch, Ebmaj7, 4)); touch(pad(2, 1), false);
    /* A held chord-shape button applies too (the grid's rule) */
    tp_layout l = tc_layout(&tc_state); tc_state.fn_held = 0u;
    assert(tc_pad_edit(&tc_state, l, pad(2, 1)).add == TC_ADD7);
    /* The screen: D#maj7 AEO (2.3.4: the mode in three letters, large), MODES on the line under the key */
    td_clear(); ui_draw_play(&tc_state, &e, true, false);
    if (!on_screen("D#maj7 AEO")) for (uint32_t i = 0; i < drawn_n; ++i) fprintf(stderr, "drawn: %s\n", drawn[i]);
    assert(scale_of("D#maj7 AEO") == 2u);
    bool modes_line = false;
    for (uint32_t i = 0; i < drawn_n; ++i) if (strstr(drawn[i], "MODES")) modes_line = true;
    assert(modes_line);
    /* 2.3.4 (board test D2): row 3 Mixolydian, its Edim is Edim MIX, large (was Edim MIXOLYDIAN, small) */
    uint8_t row3 = tc_state.mode_rows[1];
    tc_state.mode_rows[1] = 5u; e.layout = tc_layout(&tc_state); /* MIXOLYDIAN (layout.c's 5) */
    touch(pad(2, 2), true); touch(pad(2, 2), false);
    td_clear(); ui_draw_play(&tc_state, &e, true, false);
    if (scale_of("Edim MIX") != 2u) for (uint32_t i = 0; i < drawn_n; ++i) fprintf(stderr, "drawn: %s (%u)\n", drawn[i], drawn_scale[i]);
    assert(scale_of("Edim MIX") == 2u);
    tc_state.mode_rows[1] = row3; e.layout = tc_layout(&tc_state);
    /* 2.3.5 (board test N1): Harm minor and Mel minor in four letters: row 2's G (column 5) is G HMIN, then G MMIN, large */
    { uint8_t was = tc_state.mode_rows[0];
      tc_state.mode_rows[0] = 7u; e.layout = tc_layout(&tc_state); /* HARM MINOR (layout.c's 7) */
      touch(pad(4, 1), true); touch(pad(4, 1), false);
      td_clear(); ui_draw_play(&tc_state, &e, true, false); assert(scale_of("G HMIN") == 2u);
      tc_state.mode_rows[0] = 8u; e.layout = tc_layout(&tc_state); /* MEL MINOR */
      touch(pad(4, 1), true); touch(pad(4, 1), false);
      td_clear(); ui_draw_play(&tc_state, &e, true, false); assert(scale_of("G MMIN") == 2u);
      /* Another scale, too long with the chord for the large font (MIN PENT): the chord alone */
      tc_state.mode_rows[0] = 10u; e.layout = tc_layout(&tc_state);
      touch(pad(0, 1), true); touch(pad(0, 1), false);
      char alone[TN_MAX]; uint32_t n = tn_pad(tc_layout(&tc_state), pad(0, 1), alone);
      td_clear(); ui_draw_play(&tc_state, &e, true, false);
      if (!scale_of(alone)) for (uint32_t i = 0; i < drawn_n; ++i) fprintf(stderr, "drawn: %s (%u)\n", drawn[i], drawn_scale[i]);
      assert(n && n + 1u + strlen(tc_scale_name(10u)) > UI_BIG_COLUMNS && scale_of(alone) == (n <= UI_BIG_COLUMNS ? 2u : 1u));
      for (uint32_t i = 0; i < drawn_n; ++i) assert(!strstr(drawn[i], "PENT"));
      tc_state.mode_rows[0] = was; e.layout = tc_layout(&tc_state); }
    /* 2.3.4 (board test D9): a preset loaded, the line under the key fits (MODE M1  P2 MODES was 17 characters) */
    { uint8_t was = tc_state.preset; tc_state.preset = 1u;
      td_clear(); ui_draw_play(&tc_state, &e, true, false);
      if (!on_screen("MODE M1 P2 MODES")) for (uint32_t i = 0; i < drawn_n; ++i) fprintf(stderr, "drawn: %s\n", drawn[i]);
      assert(on_screen("MODE M1 P2 MODES"));
      tc_state.preset = was; }
    /* The top row's chord: its name alone */
    touch(pad(0, 0), true); touch(pad(0, 0), false);
    td_clear(); ui_draw_play(&tc_state, &e, true, false); assert(on_screen("C"));
    /* Held pad of row 4 + M3 (the family's third mode, Phrygian): row 4 plays it, the key's scale stays */
    uint8_t phrygian = tc_mode_scale(&tc_state, 4u);
    touch(pad(1, 3), true); tap_mode(4u);
    assert(tc_state.mode_rows[2] == phrygian && tc_state.scale == 0u && tc_state.last_action == TC_ROW_MODE && tc_state.row_set == 3u);
    ui_start(); ui_last_action(&tc_state); assert(!strcmp(ui_buffer, "ROW 4: PHRYGIAN"));
    touch(pad(1, 3), false);
    { const uint8_t Db[] = {37, 41, 44}; touch(pad(1, 3), true); assert(hears(tc_state.chord_ch, Db, 3)); touch(pad(1, 3), false); }
    /* A held top-row pad + a mode button: the key's scale, as always */
    touch(pad(0, 0), true); tap_mode(7u); assert(tc_state.scale == tc_mode_scale(&tc_state, 7u) && tc_state.last_action == TC_MODE);
    touch(pad(0, 0), false); tap_mode(2u); assert(tc_state.scale == 0u);
    /* Reset options (Chords): the rows back to Minor, Mixolydian, Dorian; Mel follows Main */
    assert(tc_set_item(&tc_state, &e, TC_MENU_MELODY_FOLLOW, TC_FOLLOW_ROW));
    tc_state.ui = TC_UI_MENU; tc_state.group = TC_GROUP_CHORDS; tc_state.menu = TC_MENU_RESET_PAGE;
    tc_navigate(&tc_state, &e, TC_NAV_ENTER); assert(tc_state.ui == TC_UI_EDIT && !tc_state.edit_value);
    tc_navigate(&tc_state, &e, TC_NAV_UP); tc_navigate(&tc_state, &e, TC_NAV_ENTER);
    assert(tc_state.last_action == TC_OPTIONS_RESET && tc_state.chord_set == 0u && tc_state.melody_follow == TC_FOLLOW_MAIN);
    assert(tc_state.mode_rows[0] == 1u && tc_state.mode_rows[1] == 5u && tc_state.mode_rows[2] == 2u && !e.layout.melody_row);
    puts("controls: Chord set Modes, its items shown then, values and names, Row 3 set, the chord-set button, a pad and "
         "its edit heard, the screen, a held row-4 pad + M3, a top-row pad + a mode button, Reset options");
}
static void follows(void) {
    fresh();
    assert(tc_set_item(&tc_state, &e, TC_MENU_CHORD_SET, TC_CHORDS_MODES));
    /* Main: the melody stays in C major whatever row plays */
    touch(pad(2, 1), true); touch(pad(2, 1), false);
    assert(!e.layout.melody_row && tc_quantise(e.layout, 63u) == 62u);
    { const uint8_t E[] = {52}; touch(melody(2, 3), true); assert(hears(tc_state.melody_ch, E, 1)); touch(melody(2, 3), false); }
    /* Last row: Eb (row 2, Minor) takes the melody to C minor: its pads, scale lock, the harmonizer, the lights */
    assert(tc_set_item(&tc_state, &e, TC_MENU_MELODY_FOLLOW, TC_FOLLOW_ROW) && tc_state.melody_follow == TC_FOLLOW_ROW);
    value_is(TC_MENU_MELODY_FOLLOW, "LAST ROW");
    touch(pad(2, 1), true); touch(pad(2, 1), false);
    assert(e.layout.melody_row == 1u && tc_layout(&tc_state).melody_row == 1u && tc_melody_scale(e.layout) == 1u);
    { const uint8_t Eb[] = {51}; touch(melody(2, 3), true); assert(hears(tc_state.melody_ch, Eb, 1)); touch(melody(2, 3), false); }
    assert(tc_quantise(e.layout, 64u) == 63u && tc_harmony_note(e.layout, 60u, 2) == 63u);
    /* The lights: in Chromatic, Eb's pad lit as a scale note while the melody follows Minor, dark again with Major */
    tp_layout ch = tc_layout(&tc_state); ch.melody_layout = TP_MELODY_CHROMATIC; assert(tc_set_layout(&tc_state, &e, ch));
    assert(e.layout.melody_row == 1u);
    ui_pad_colors(&tc_state, &e, now); uint32_t eb_minor = sum(pads[melody(3, 3)]);
    touch(pad(0, 0), true); touch(pad(0, 0), false);
    ui_pad_colors(&tc_state, &e, now); uint32_t eb_major = sum(pads[melody(3, 3)]);
    assert(eb_minor > 3u * eb_major);
    ch = tc_layout(&tc_state); ch.melody_layout = TP_MELODY_SCALE; assert(tc_set_layout(&tc_state, &e, ch));
    touch(pad(2, 1), true); touch(pad(2, 1), false); assert(e.layout.melody_row == 1u);
    /* Bb on row 3 (Mixolydian), then the top row: back to C major */
    touch(pad(6, 2), true); touch(pad(6, 2), false); assert(e.layout.melody_row == 2u && tc_melody_scale(e.layout) == 5u);
    { const uint8_t Bb[] = {58}; touch(melody(6, 3), true); assert(hears(tc_state.melody_ch, Bb, 1)); touch(melody(6, 3), false); }
    touch(pad(0, 0), true); touch(pad(0, 0), false); assert(e.layout.melody_row == 0u);
    { const uint8_t B[] = {59}; touch(melody(6, 3), true); assert(hears(tc_state.melody_ch, B, 1)); touch(melody(6, 3), false); }
    /* Back to Main: the top row's, whatever was played */
    touch(pad(2, 1), true); touch(pad(2, 1), false); assert(e.layout.melody_row == 1u);
    assert(tc_set_item(&tc_state, &e, TC_MENU_MELODY_FOLLOW, TC_FOLLOW_MAIN) && !e.layout.melody_row && !tc_state.follow_row);
    /* Another chord layout: no following */
    assert(tc_set_item(&tc_state, &e, TC_MENU_MELODY_FOLLOW, TC_FOLLOW_ROW));
    touch(pad(2, 1), true); touch(pad(2, 1), false); assert(e.layout.melody_row == 1u);
    assert(tc_set_item(&tc_state, &e, TC_MENU_CHORD_SET, 0u) && !e.layout.melody_row && !tc_state.follow_row);
    touch(pad(2, 1), true); touch(pad(2, 1), false); assert(!e.layout.melody_row);
    /* A row scale whose melody leaves MIDI range: the melody stays (the chords still play) */
    assert(tc_set_item(&tc_state, &e, TC_MENU_CHORD_SET, TC_CHORDS_MODES));
    tp_layout high = tc_layout(&tc_state); high.melody_base = TP_MELODY_BASE + TP_OCTAVE_RANGE;
    high.rows[0] = 9u; /* Major pentatonic: its top-right melody pad 2 octaves + a step up, past 127 there */
    assert(tc_set_layout(&tc_state, &e, high));
    touch(pad(0, 1), true); touch(pad(0, 1), false); assert(e.layout.melody_row == 0u && heard(tc_state.chord_ch) == 0u);
    touch(pad(0, 2), true); touch(pad(0, 2), false); assert(e.layout.melody_row == 2u); /* Mixolydian fits */
    puts("follows: Main stays; Last row follows Minor, Mixolydian, the top row (pads heard, scale lock, harmonizer, "
         "lights); Main again; not in another chord layout");
}
static void lights(void) {
    fresh();
    assert(tc_set_item(&tc_state, &e, TC_MENU_CHORD_SET, TC_CHORDS_MODES));
    tc_state.hints = 0u;
    ui_pad_colors(&tc_state, &e, now);
    /* Rows 2..4: a chord the top row also has (Dm on Dorian's row) is dim; a borrowed one (Eb) is not */
    tl_rgb dm = pads[pad(1, 3)], dm_top = pads[pad(1, 0)], eb = pads[pad(2, 1)];
    uint32_t dm_sum = sum(dm), top_sum = sum(dm_top), eb_sum = sum(eb);
    assert(dm_sum * 3u < top_sum && eb_sum * 2u > top_sum);
    /* The scale's chords: nothing dims */
    assert(tc_set_item(&tc_state, &e, TC_MENU_CHORD_SET, 0u));
    ui_pad_colors(&tc_state, &e, now);
    assert(sum(pads[pad(1, 3)]) * 2u > top_sum);
    puts("lights: rows 2..4's chords the key also has dim, borrowed ones bright; nothing dims outside Modes");
}
/* 2.3.3: the tunings dropped. A board from 2.2.0-2.3.2 may have stored a tuned scale (ids TC_ALL_SCALES ..
 * TC_RETIRED_SCALES_END - 1) as the scale, a Modes row or a progression's, and 2.2's Tuning out in word 19 bits 12-13:
 * the scale reads as C Major, a row as its default, a progression's as Major. 2.4.12: bits 12-13 are Bend range now, so an
 * old Tuning out reads as one (a display setting only: the semitones a step's bend shows), written back as it is. */
static void retired(void) {
    fresh();
    uint32_t q[ST_PAYLOAD], p[ST_PAYLOAD];
    assert(st_get(ST_LIVE, 0u, &tc_state, q) == ST_OK);
    memcpy(p, q, sizeof p); p[0] = (p[0] & 0xFFFF0000u) | (uint32_t)(TC_ALL_SCALES + 5u) << 8 | 14u; /* a tuned scale, root step 14 */
    p[19] |= 3u << 12;
    p[22] = (p[22] & 0xFFFFu) | (uint32_t)(TC_ALL_SCALES + 1u) << 16; /* Row 2: the first tuned scale (code id + 1) */
    p[34] = (p[34] & ~0xFFu) | TC_CHORDS_MODES;
    assert(st_put(ST_LIVE, 0u, p, &tc_state, &e) == ST_OK);
    assert(tc_state.scale == 0u && tc_state.tonic == 0u && tc_state.mode_rows[0] == 1u /* Minor, Row 2's default */);
    assert(tc_state.bend_range == 3u);
    assert(st_get(ST_LIVE, 0u, &tc_state, p) == ST_OK && ((p[19] >> 12) & 3u) == 3u && !(p[22] >> 16) && (p[0] & 0xFFFFu) == 0u);
    memcpy(p, q, sizeof p); p[22] = (p[22] & 0xFFFFu) | (uint32_t)(TC_RETIRED_SCALES_END + 1u) << 16; /* never an id */
    assert(st_put(ST_LIVE, 0u, p, &tc_state, &e) == ST_BAD_VALUE);
    memcpy(p, q, sizeof p); p[0] = (p[0] & 0xFFFF00FFu) | (uint32_t)TC_RETIRED_SCALES_END << 8;
    assert(st_put(ST_LIVE, 0u, p, &tc_state, &e) == ST_BAD_VALUE);
    /* A progression recorded in a tuned scale: Major */
    sq_prog prog; sq_clear(&prog, SQ_UNIT_BAR, 0u);
    uint32_t w[SQ_WORDS]; sq_encode(&prog, w); w[0] = (w[0] & ~0xFF0000u) | (uint32_t)(TC_ALL_SCALES + 9u) << 16;
    sq_prog back; assert(sq_decode(w, &back) && back.scale == 0u);
    w[0] = (w[0] & ~0xFF0000u) | (uint32_t)TC_RETIRED_SCALES_END << 16; assert(!sq_decode(w, &back));
    /* No tuning items left on the pages */
    for (uint8_t g = 0; g < TC_GROUPS; ++g) for (uint8_t i = 0; i < tc_group_size(g); ++i) {
        ui_value_name(tc_group_items[g][i], 0u); (void)ui_buffer;
        assert(strncmp(ui_menu_items[tc_group_items[g][i]], "TUNING", 6) && strcmp(ui_menu_items[tc_group_items[g][i]], "SEND TUNING"));
    }
    puts("retired: a stored tuned scale reads as C Major, a row's as its default, a progression's as Major; Tuning out "
         "ignored and not written; ids from 149 refused; no tuning items");
}
static void storage(void) {
    fresh();
    uint32_t p[ST_PAYLOAD];
    assert(st_get(ST_LIVE, 0u, &tc_state, p) == ST_OK && !(p[22] >> 16) && !(p[30] >> 16)); /* defaults: zeros, as before 2.3 */
    assert(tc_set_item(&tc_state, &e, TC_MENU_CHORD_SET, TC_CHORDS_MODES));
    assert(tc_set_item(&tc_state, &e, TC_MENU_ROW2, tc_scale_position(2u)) && tc_set_item(&tc_state, &e, TC_MENU_ROW4, tc_scale_position(7u)));
    assert(tc_set_item(&tc_state, &e, TC_MENU_MELODY_FOLLOW, TC_FOLLOW_ROW));
    assert(st_get(ST_LIVE, 0u, &tc_state, p) == ST_OK && (p[34] & 0xFFu) == TC_CHORDS_MODES);
    assert(((p[22] >> 16) & 0xFFu) == 3u && !(p[22] >> 24) && ((p[30] >> 16) & 0xFFu) == 8u && (p[30] >> 24) == 1u);
    /* Save it as preset 3, change everything, load it back */
    tc_request(&tc_state, TC_REQUEST_SAVE, 2u); run(50);
    assert(tc_set_item(&tc_state, &e, TC_MENU_MELODY_FOLLOW, TC_FOLLOW_MAIN) && tc_set_item(&tc_state, &e, TC_MENU_ROW2, 0u));
    assert(tc_set_item(&tc_state, &e, TC_MENU_CHORD_SET, 0u));
    tc_request(&tc_state, TC_REQUEST_LOAD, 2u); run(50);
    assert(tc_state.chord_set == TC_CHORDS_MODES && tc_state.mode_rows[0] == 2u && tc_state.mode_rows[2] == 7u && tc_state.melody_follow == TC_FOLLOW_ROW);
    assert(e.layout.chords == TC_CHORDS_MODES && e.layout.rows[0] == 2u);
    /* Over a restart */
    tc_state.changes++; run(4000);
    tc_init(&tc_state); assert(st_load(&tc_state));
    assert(tc_state.chord_set == TC_CHORDS_MODES && tc_state.mode_rows[0] == 2u && tc_state.mode_rows[1] == 5u &&
           tc_state.mode_rows[2] == 7u && tc_state.melody_follow == TC_FOLLOW_ROW && !tc_state.follow_row);
    /* Refused: a row past every scale, Mel follows 2; the 48-pad words keep their own 16 bits */
    uint32_t q[ST_PAYLOAD];
    assert(st_get(ST_LIVE, 0u, &tc_state, q) == ST_OK);
    memcpy(p, q, sizeof p); p[22] = (p[22] & 0xFFFFu) | (uint32_t)(TC_RETIRED_SCALES_END + 1u) << 16; assert(st_put(ST_LIVE, 0u, p, &tc_state, &e) == ST_BAD_VALUE);
    memcpy(p, q, sizeof p); p[30] = (p[30] & 0xFFFFFFu) | 3u << 25; /* 2.4.9: Mel follows is bit 24; a join past Kalimba */ assert(st_put(ST_LIVE, 0u, p, &tc_state, &e) == ST_BAD_VALUE);
    memcpy(p, q, sizeof p); p[22] |= 0xFFFFu; p[30] |= 0xFFFFu;
    assert(st_put(ST_LIVE, 0u, p, &tc_state, &e) == ST_OK && tc_state.strum_off >> 32 == 0xFFFFu && tc_state.chord_mute >> 32 == 0xFFFFu);
    assert(tc_state.mode_rows[0] == 2u && tc_state.melody_follow == TC_FOLLOW_ROW);
    /* A progression recorded in Modes keeps it (word 39 = 11) */
    sq_prog prog; sq_clear(&prog, SQ_UNIT_BAR, 0u); prog.chords = TC_CHORDS_MODES;
    uint32_t w[SQ_WORDS]; sq_encode(&prog, w); sq_prog back; assert(sq_decode(w, &back) && back.chords == TC_CHORDS_MODES);
    puts("storage: defaults as zeros; rows and Mel follows in words 22 and 30; a preset; a restart; refused values; "
         "the pads' bits kept; a progression's chord layout");
}
int main(void) {
    layout();
    borrowed_and_hints();
    controls();
    follows();
    lights();
    retired();
    storage();
    puts("modes: all passed");
    return 0;
}
