/* SPDX-License-Identifier: GPL-3.0-or-later - 1.4.0 pivot chords on the real code: which chord pads
 * a target key shares (layout.c tc_pivots: ii and IV strong, the rest of the shared chords also)
 * in every scale, key and chord layout with edits; the Pivot to setting and its centre-button
 * action (hold + root button, a tap turns it off), arriving at the target (its major or minor,
 * Pivot cleared), the pad lights (guide > pivot > hints), the play screen's line and the stored
 * record (word 33 byte 2). */
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
/* The screen: the text drawn at column 0 of each row, as the play screen's lines are. */
static char rows[64][48];
void td_clear(void) { memset(rows, 0, sizeof rows); }
uint32_t td_text(uint32_t x, uint32_t y, const char *text, uint32_t scale) {
    if (!x && y < 64u) snprintf(rows[y], sizeof rows[y], "%s", text);
    return x + 6u * scale * (uint32_t)strlen(text);
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
enum { NAV_CENTRE = 6, NAV_LEFT = 8 };
static void hold_bit(uint8_t a, uint8_t bit, bool down) {
    if (down) raw[client(a)] |= 1u << bit; else raw[client(a)] &= ~(1u << bit);
    run(50);
}
static void tap_bit(uint8_t a, uint8_t bit) { hold_bit(a, bit, true); hold_bit(a, bit, false); }
static void hold_button(uint32_t i, bool down) { hold_bit(0x35, fn_bit[i], down); }
static void tap_button(uint32_t i) { tap_bit(0x35, fn_bit[i]); }
static void tap_root(uint32_t pc) { tap_bit(0x39, top_bit[pc]); }
static void touch(uint32_t k, bool down) { scan.values[k] = down ? 3000u : 0u; run(60); }
static uint32_t pad(uint32_t column, uint32_t row) { return 4u * column + row; }
static void fresh(void) {
    tc_init(&tc_state); memset(&e, 0, sizeof e); memset(&scan, 0, sizeof scan); memset(raw, 0, sizeof raw);
    memset(sounding, 0, sizeof sounding);
    tm_reset();
    for (uint8_t s = 0; s < TC_CHORD_SETS; ++s) { tc_chord_set none; memset(&none, 0, sizeof none); assert(tc_chord_set_put(s, &none)); }
    sq_clear(&sq, SQ_UNIT_BAR, 0u);
    fs_erase(); (void)st_load(&tc_state);
    run(600); assert(e.state == TK_ACTIVE);
}
static void quiet(void) {
    run(200);
    for (uint32_t ch = 0; ch < 16u; ++ch) assert(!heard(ch));
}
static uint64_t columns(const uint32_t *cols, uint32_t n) {
    uint64_t m = 0;
    for (uint32_t i = 0; i < n; ++i) for (uint32_t r = 0; r < 4u; ++r) m |= (uint64_t)1u << pad(cols[i], r);
    return m;
}
/* Pivot targets: 1..12 the major key on C..B, 13..24 the minor key. */
static uint8_t major(uint32_t pc) { return (uint8_t)(1u + pc); }
static uint8_t minor(uint32_t pc) { return (uint8_t)(13u + pc); }
static uint16_t classes(const uint8_t *pcs, uint32_t n) {
    uint16_t m = 0;
    for (uint32_t i = 0; i < n; ++i) m |= (uint16_t)(1u << (pcs[i] % 12u));
    return m;
}
static uint16_t scale_on(const uint8_t steps[7], uint32_t root) {
    uint16_t m = 0;
    for (uint32_t i = 0; i < 7u; ++i) m |= (uint16_t)(1u << ((steps[i] + root) % 12u));
    return m;
}
static const uint8_t major_steps[7] = {0, 2, 4, 5, 7, 9, 11}, minor_steps[7] = {0, 2, 3, 5, 7, 8, 10};

/* --- the pure part ---------------------------------------------------------------------- */
static void scales_and_classes(void) {
    for (uint32_t pc = 0; pc < 12u; ++pc) {
        assert(tc_pivot_scale(major(pc)) == scale_on(major_steps, pc));
        assert(tc_pivot_scale(minor(pc)) == scale_on(minor_steps, pc));
    }
    assert(!tc_pivot_scale(0u) && !tc_pivot_scale(TC_PIVOT_KEYS) && !tc_pivot_scale(255u));
    assert(TC_PIVOTS == TC_PIVOT_KEYS && TC_PIVOT_KEYS == 25u);
    /* The classes a pad plays, with its edits: C, Cmaj7, Csus4, G7; a set's empty column none. */
    tc_init(&tc_state);
    tp_layout l = TP_DEFAULT_LAYOUT; tc_chord plain = {0, 0, 0}, add7 = {0, 0, TC_ADD7}, sus4 = {0, 4, 0};
    static const uint8_t c[3] = {0, 4, 7}, cmaj7[4] = {0, 4, 7, 11}, csus4[3] = {0, 5, 7}, g7[4] = {7, 11, 2, 5};
    for (uint32_t r = 0; r < 4u; ++r) assert(tc_pad_classes(l, pad(0, r), plain) == classes(c, 3)); /* every inversion */
    assert(tc_pad_classes(l, pad(0, 3), add7) == classes(cmaj7, 4));
    assert(tc_pad_classes(l, pad(0, 3), sus4) == classes(csus4, 3));
    assert(tc_pad_classes(l, pad(4, 3), add7) == classes(g7, 4));
    assert(tc_pad_classes(l, pad(7, 3), plain) == classes(c, 3)); /* an octave up: the same classes */
    assert(tc_pad_classes(l, 48u + pad(7, 3), plain) == classes(c, 3)); /* key numbers wrap like the pads */
    tc_chord_set s; memset(&s, 0, sizeof s); s.column[0] = (tc_set_chord){0x091u, 0u};
    assert(tc_chord_set_put(0, &s));
    l.chords = 1u; assert(!tc_pad_classes(l, pad(1, 3), plain) && tc_pad_classes(l, pad(0, 3), plain) == classes(c, 3));
    memset(&s, 0, sizeof s); assert(tc_chord_set_put(0, &s));
    printf("pure: the 24 target scales, a pad's classes with its edits, in every inversion and octave\n");
}
/* C major's pads: columns 0..6 C Dm Em F G Am Bdim, 7..11 C Dm Em F G. */
static void from_c_major(void) {
    tc_init(&tc_state);
    tp_layout l = TP_DEFAULT_LAYOUT; uint64_t also;
    /* To G: ii (Am) and IV (C) strong; Em and G also; Dm, F, Bdim (they have F) not. */
    static const uint32_t g_strong[3] = {0, 5, 7}, g_also[4] = {2, 4, 9, 11};
    assert(tc_pivots(l, major(7), &also) == columns(g_strong, 3) && also == columns(g_also, 4));
    /* To A minor (the same notes): ii (Bdim) and iv (Dm) strong, every other chord also. */
    static const uint32_t a_strong[3] = {1, 6, 8}, a_also[9] = {0, 2, 3, 4, 5, 7, 9, 10, 11};
    assert(tc_pivots(l, minor(9), &also) == columns(a_strong, 3) && also == columns(a_also, 9));
    /* To E minor: iv (Am) strong; Em, G and C also. */
    static const uint32_t e_strong[1] = {5}, e_also[6] = {0, 2, 4, 7, 9, 11};
    assert(tc_pivots(l, minor(4), &also) == columns(e_strong, 1) && also == columns(e_also, 6));
    /* To F: no Gm or Bb here, so nothing strong; C Dm F Am also. */
    static const uint32_t f_also[7] = {0, 1, 3, 5, 7, 8, 10};
    assert(tc_pivots(l, major(5), &also) == 0u && also == columns(f_also, 7));
    /* To F#: nothing shared. Off and out of range: nothing. */
    assert(!tc_pivots(l, major(6), &also) && !also);
    also = 1u; assert(!tc_pivots(l, 0u, &also) && !also);
    also = 1u; assert(!tc_pivots(l, TC_PIVOT_KEYS, &also) && !also);
    assert(tc_pivots(l, major(7), NULL) == columns(g_strong, 3));
    /* Edits count: G with its 7th (G7, an F) is no longer in G; Cmaj7 still is (B). */
    tc_state.chord[pad(4, 3)].add = TC_ADD7; tc_state.chord[pad(0, 3)].add = TC_ADD7;
    assert(tc_pivots(l, major(7), &also) == columns(g_strong, 3) && also == (columns(g_also, 4) & ~((uint64_t)1u << pad(4, 3))));
    tc_init(&tc_state);
    printf("C major: to G (Am C strong, Em G also), A minor (Bdim Dm), E minor (Am), F (none strong), F# none; edits count\n");
}
/* Every scale family mode, key and target, the chord grid, the Full layout and a set: a pad is a
 * pivot exactly when every note of it is in the target's scale, strong exactly when its root is
 * the target's 2nd or 4th degree. */
static void everywhere(void) {
    tc_init(&tc_state);
    tc_chord_set s; memset(&s, 0, sizeof s);
    s.column[0] = (tc_set_chord){0x091u, 0u}; s.column[1] = (tc_set_chord){0x891u, 5u};
    s.column[3] = (tc_set_chord){0x089u, 10u}; s.column[4] = (tc_set_chord){0x491u, 19u};
    s.column[5] = (tc_set_chord){0x049u, 2u};
    assert(tc_chord_set_put(0, &s));
    static const uint8_t chords[4] = {0u, 1u, TC_CHORDS_GRID, TC_CHORDS_FULL};
    uint32_t layouts = 0, pivots = 0, strongs = 0;
    for (uint32_t m = 0; m < 9u; ++m) for (uint8_t tonic = 0; tonic < 12u; tonic += 1u) for (uint32_t ch = 0; ch < 4u; ++ch) {
        tp_layout l = TP_DEFAULT_LAYOUT; l.mode = tc_mode_buttons[m]; l.tonic = tonic; l.chords = chords[ch];
        if (!tc_layout_ok(l)) continue;
        layouts++;
        for (uint8_t t = 0; t < TC_PIVOT_KEYS + 1u; ++t) {
            uint64_t also, strong = tc_pivots(l, t, &also);
            assert(!(strong & also));
            uint16_t key = tc_pivot_scale(t);
            for (uint32_t k = 0; k < 48u; ++k) {
                uint16_t c = tc_pad_classes(l, k, tc_pad_edit(&tc_state, l, k));
                bool shared = key && c && !(c & ~key);
                uint32_t degree = t ? (tc_pad_root(l, k) + 12u - (t - 1u) % 12u) % 12u : 0u;
                bool lead = degree == 2u || degree == 5u;
                assert((((strong | also) >> k) & 1u) == shared);
                assert(((strong >> k) & 1u) == (shared && lead));
                pivots += shared; strongs += shared && lead;
            }
        }
    }
    assert(layouts >= 9u * 12u * 3u && pivots && strongs);
    memset(&s, 0, sizeof s); assert(tc_chord_set_put(0, &s));
    printf("everywhere: %u layouts (modes, keys, scale chords, a set, grid, Full) x 25 targets: %u pivot pads, %u strong\n",
           layouts, pivots, strongs);
}

/* --- on the board ----------------------------------------------------------------------- */
static void set_item(uint8_t group, uint8_t item, uint8_t value) {
    tc_state.ui = TC_UI_MENU; tc_state.group = group; tc_state.menu = item;
    tap_bit(0x36, NAV_CENTRE); assert(tc_state.ui == TC_UI_EDIT);
    tc_state.edit_value = value; tap_bit(0x36, NAV_CENTRE);
    tap_bit(0x36, NAV_LEFT); tap_bit(0x36, NAV_LEFT); assert(tc_state.ui == TC_UI_PLAY);
}
static bool in_group(uint8_t group, uint8_t item) {
    for (uint32_t i = 0; i < TC_GROUP_MAX; ++i) if (tc_group_items[group][i] == item) return true;
    return false;
}
static void the_button(void) {
    fresh();
    assert(in_group(TC_GROUP_CHORDS, TC_MENU_PIVOT) && tc_menu_values(TC_MENU_PIVOT) == TC_PIVOTS);
    assert(TC_ACT_PIVOT == TC_ACT_NONE + 1u && TC_ACT_PIVOT + 1u == TC_ACTIONS); /* stored actions keep their numbers */
    tc_state.buttons[0] = TC_ACT_PIVOT;
    /* Held, a root button picks the target: a major key from a major scale; the key stays. */
    hold_button(0, true); tap_root(7); hold_button(0, false);
    assert(tc_state.pivot == major(7) && tc_state.last_action == TC_PIVOT_SET && tc_state.tonic == 0u);
    assert(tc_panel_active(&tc_state, 0) && !heard(0));
    /* Held again for another key: that one. */
    hold_button(0, true); tap_root(2); hold_button(0, false);
    assert(tc_state.pivot == major(2) && tc_state.tonic == 0u);
    /* A tap alone: off, unlit. */
    tap_button(0); assert(!tc_state.pivot && !tc_panel_active(&tc_state, 0));
    /* From a minor scale (A minor, M6): a minor key. */
    tp_layout l = tc_layout(&tc_state); l.tonic = 9u; l.mode = tc_mode_buttons[7]; assert(tc_set_layout(&tc_state, &e, l));
    hold_button(0, true); tap_root(4); hold_button(0, false);
    assert(tc_state.pivot == minor(4) && tc_state.tonic == 9u && tc_state.scale == tc_mode_buttons[7]);
    tap_button(0); assert(!tc_state.pivot);
    /* With Shift held the root button is Shift's (no pivot). */
    tc_state.buttons[1] = TC_ACT_SHIFT;
    hold_button(1, true); hold_button(0, true); tap_root(4); hold_button(0, false); hold_button(1, false);
    assert(!tc_state.pivot);
    /* The menu sets it too. */
    set_item(TC_GROUP_CHORDS, TC_MENU_PIVOT, minor(11)); assert(tc_state.pivot == minor(11));
    set_item(TC_GROUP_CHORDS, TC_MENU_PIVOT, 0u); assert(!tc_state.pivot);
    quiet();
    printf("button: hold + G = to G, hold + D = to D, tap = off, from A minor hold + E = to E minor, not with Shift; menu\n");
}
static void arriving(void) {
    fresh();
    uint8_t m1 = tc_mode_buttons[2], m2 = tc_mode_buttons[3], m6 = tc_mode_buttons[7];
    /* Another root button just changes the key; the target's arrives and clears Pivot. */
    tc_state.pivot = major(7);
    tap_root(2); assert(tc_state.tonic == 2u && tc_state.pivot == major(7) && tc_state.last_action == TC_TONIC);
    tap_root(7); assert(tc_state.tonic == 7u && tc_state.scale == m1 && !tc_state.pivot && tc_state.last_action == TC_PIVOT_ARRIVED);
    /* Major to a minor target: the minor (M6) on arrival. */
    tp_layout l = tc_layout(&tc_state); l.tonic = 0u; assert(tc_set_layout(&tc_state, &e, l));
    tc_state.pivot = minor(4);
    tap_root(4); assert(tc_state.tonic == 4u && tc_state.scale == m6 && !tc_state.pivot);
    /* Minor to a major target: M1. */
    tc_state.pivot = major(7);
    tap_root(7); assert(tc_state.tonic == 7u && tc_state.scale == m1 && !tc_state.pivot);
    /* Another mode (Dorian) keeps its mode. */
    l = tc_layout(&tc_state); l.tonic = 2u; l.mode = m2; assert(tc_set_layout(&tc_state, &e, l));
    tc_state.pivot = minor(9);
    tap_root(9); assert(tc_state.tonic == 9u && tc_state.scale == m2 && !tc_state.pivot && tc_state.last_action == TC_PIVOT_ARRIVED);
    /* Arrived, the pads play in the new key. */
    l = tc_layout(&tc_state); l.mode = m1; l.tonic = 0u; assert(tc_set_layout(&tc_state, &e, l));
    tc_state.pivot = major(7); tap_root(7);
    touch(pad(0, 3), true); assert(sounding[0][43] && sounding[0][47] && sounding[0][50]); touch(pad(0, 3), false); /* G */
    quiet();
    printf("arriving: D keeps the target, G arrives (M1, cleared), C -> E minor gives M6, minor -> G gives M1, Dorian kept\n");
}
static uint32_t sum(tl_rgb c) { return (uint32_t)c.r + c.g + c.b; }
/* The lowest and highest brightness of pad k over a second. */
static void range(uint32_t k, uint32_t *low, uint32_t *high) {
    *low = 255u * 3u; *high = 0u;
    for (uint32_t t = 0; t < 1000u; t += 50u) {
        ui_service(&tc_state, &e, &scan, true, now + t);
        uint32_t level = sum(pads[k]);
        if (level < *low) *low = level;
        if (level > *high) *high = level;
    }
}
static void lights(void) {
    fresh();
    /* No chord played yet: every pad shows (pad 0, the top C, too). */
    ui_service(&tc_state, &e, &scan, true, now);
    tl_rgb off_c = pads[pad(0, 0)], off_em = pads[pad(2, 3)], off_f = pads[pad(3, 3)], off_g = pads[pad(4, 3)];
    /* To G: C (IV there) pulses, Em steady, F dim; the melody half is untouched. */
    tl_rgb melody[48]; memcpy(melody, pads + 48, sizeof melody);
    tc_state.pivot = major(7);
    uint32_t low, high;
    range(pad(0, 0), &low, &high); assert(high > low && 2u * high >= sum(off_c));
    range(pad(2, 3), &low, &high); assert(low == high && low >= sum(off_em));
    range(pad(3, 3), &low, &high); assert(4u * high <= sum(off_f) + 12u);
    ui_service(&tc_state, &e, &scan, true, now); assert(!memcmp(melody, pads + 48, sizeof melody));
    /* After Dm, the hints pulse G; the pivot's light comes first (G steady, also there); the
     * chord just played keeps its own light. */
    tc_state.have_chord = true; tc_state.last_chord = (uint8_t)pad(1, 3); tc_state.hints = 1u;
    range(pad(4, 3), &low, &high); assert(low == high && low >= sum(off_g));
    range(pad(1, 3), &low, &high); assert(low == high);
    tc_state.pivot = 0u;
    range(pad(4, 3), &low, &high); assert(high > low);
    /* The progression's guide comes first: a playing progression shows its chords, not the pivots. */
    tc_state.hints = 0u; tc_state.pivot = major(7);
    tc_state.transport = true; run(50); /* an empty progression: no guide */
    range(pad(3, 3), &low, &high); assert(4u * high <= sum(off_f) + 12u);
    tc_state.transport = false; run(50);
    sq_clear(&sq, SQ_UNIT_BAR, 0u);
    tc_state.ui = TC_UI_PLAY; tc_state.buttons[13] = TC_ACT_RECORD;
    tap_button(13); assert(tc_state.recording);
    touch(pad(3, 3), true); touch(pad(3, 3), false); touch(pad(4, 3), true); touch(pad(4, 3), false);
    tap_button(13); assert(!tc_state.recording && sq.count == 2u);
    tc_state.transport = true; run(100); assert(e.seq_running);
    range(pad(3, 3), &low, &high); assert(4u * high > sum(off_f)); /* F is the guide's, lit */
    tc_state.transport = false; quiet();
    tc_state.pivot = 0u;
    printf("lights: to G, C pulses, Em steady, F dim, melody untouched; pivot before hints; the guide before the pivot\n");
}
static const char *play_line(void) {
    tc_state.ui = TC_UI_PLAY; tc_state.event_sequence++;
    run(100); ui_service(&tc_state, &e, &scan, true, now);
    return rows[46];
}
static const char *message(void) { (void)play_line(); return rows[36]; }
static void screen(void) {
    fresh();
    tc_state.pivot = major(7); tc_state.have_chord = false;
    assert(!strcmp(play_line(), "PIVOT TO G MAJOR"));
    tc_state.have_chord = true;
    tc_state.last_chord = (uint8_t)pad(5, 3); assert(!strcmp(play_line(), "ii IN G: D7>G"));
    tc_state.last_chord = (uint8_t)pad(0, 1); assert(!strcmp(play_line(), "IV IN G: D7>G"));
    tc_state.last_chord = (uint8_t)pad(2, 3); assert(!strcmp(play_line(), "vi IN G: D7>G"));
    tc_state.last_chord = (uint8_t)pad(4, 3); assert(!strcmp(play_line(), "I IN G: D7>G"));
    tc_state.last_chord = (uint8_t)pad(3, 3); assert(!strcmp(play_line(), "PIVOT TO G MAJOR")); /* F: not in G */
    tc_state.pivot = minor(9); tc_state.last_chord = (uint8_t)pad(6, 3); assert(!strcmp(play_line(), "iio IN Am: E7>Am"));
    tc_state.last_chord = (uint8_t)pad(4, 3); assert(!strcmp(play_line(), "VII IN Am: E7>Am"));
    tc_state.pivot = minor(4); tc_state.last_chord = (uint8_t)pad(5, 3); assert(!strcmp(play_line(), "iv IN Em: B7>Em"));
    tc_state.hold = 1u; assert(!strcmp(play_line(), "iv IN Em: B7>Em HOLD"));
    /* Short of room beside HOLD and SUS: the way in goes, then PIVOT. */
    tc_state.sustain = true; assert(!strcmp(play_line(), "iv IN Em HOLD SUS"));
    tc_state.pivot = minor(10); tc_state.last_chord = (uint8_t)pad(3, 3); assert(!strcmp(play_line(), "TO A# MINOR HOLD SUS"));
    tc_state.hold = 0u; tc_state.sustain = false; assert(!strcmp(play_line(), "PIVOT TO A# MINOR"));
    tc_state.pivot = 0u; assert(!strncmp(play_line(), "OCT ", 4u));
    /* Messages. */
    tc_state.buttons[0] = TC_ACT_PIVOT;
    hold_button(0, true); tap_root(7); hold_button(0, false); assert(!strcmp(message(), "PIVOT TO G MAJOR"));
    tap_button(0); assert(!strcmp(message(), "PIVOT OFF"));
    tc_state.pivot = major(7); tap_root(7); assert(!strncmp(message(), "NOW IN G ", 9u));
    set_item(TC_GROUP_CHORDS, TC_MENU_PIVOT, minor(1)); assert(tc_state.pivot == minor(1));
    tc_state.pivot = 0u;
    printf("screen: PIVOT TO G MAJOR, ii/IV/vi/I IN G: D7>G, iio and VII IN Am, iv IN Em with HOLD; messages\n");
}
static void stored(void) {
    fresh();
    uint32_t p[ST_PAYLOAD], q[ST_PAYLOAD];
    assert(st_get(ST_LIVE, 0, &tc_state, p) == ST_OK && !(p[33] >> 16)); /* off: as older firmware writes it */
    set_item(TC_GROUP_CHORDS, TC_MENU_PIVOT, minor(9));
    assert(st_get(ST_LIVE, 0, &tc_state, p) == ST_OK && (p[33] >> 16) == minor(9));
    run(ST_SAVE_DELAY_MS + 100u);
    tc_controls back; tc_init(&back);
    assert(st_load(&back) && back.pivot == minor(9));
    for (uint32_t v = 0; v < 256u; ++v) {
        memcpy(q, p, sizeof q); q[33] = (q[33] & 0xFF00FFFFu) | v << 16;
        assert((st_put(ST_LIVE, 0, q, &back, &e) == ST_OK) == (v < TC_PIVOTS));
        if (v < TC_PIVOTS) assert(back.pivot == v);
    }
    memcpy(q, p, sizeof q); q[33] |= (uint32_t)TC_PROGRAM_INPUTS << 24; /* byte 3: Program in (1.7.0), 0..17 */
    assert(st_put(ST_LIVE, 0, q, &back, &e) == ST_BAD_VALUE);
    printf("stored: word 33 byte 2 (0 = off), saved and loaded, 25 and above refused\n");
}
int main(void) {
    setvbuf(stdout, NULL, _IONBF, 0);
    scales_and_classes();
    from_c_major();
    everywhere();
    the_button();
    arriving();
    lights();
    screen();
    stored();
    printf("PASS: pivot: target scales, shared chords (ii IV strong) in every layout with edits, the button "
           "and menu, arriving, lights (guide > pivot > hints), the play screen, stored\n");
    return 0;
}
