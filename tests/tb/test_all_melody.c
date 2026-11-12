/* SPDX-License-Identifier: GPL-3.0-or-later - 2.4.9, All Melody (docs/all-melody.md): the whole board one melody range.
 * Notes: Stacked (the right half four octaves on in Chromatic and Scale, four rows of fourths in Fourths; a set right
 * octave, the same octaves included), Wide rows, Kalimba (the design's row in C major, L12 and R1 the same note);
 * Drums, Bass and Live play as Scale, the stored layout kept and back afterwards. Range: turning it on (and a join or a
 * right octave that reaches higher) brings the melody octave down until all 96 pads fit, the octave can go from C-1 to
 * C9 where they fit, a preset's octave likewise; off again, the usual range. The menu (shown items, values, names), the
 * record (round trip, the device settings a preset keeps). Playing: every pad on the melody channel (Split off) or the
 * left half on the chord channel (Split on); the arpeggiator, Hold and Strum leave the left half alone; Mono one line
 * (or one per half with Split); Glide across the seam in Wide rows only; a melody lane's notes as before.
 * 1 ms ticks, a fresh scan frame every tick, every packet receipted at once. */
#include "../../src/tb/controls_internal.h"
#include "../../src/tb/settings.h"
#include "../../src/tb/flash_store.h"
#include "../../src/tb/ui_internal.h"
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
static tp_engine e;
static ts_scan scan;
static uint32_t now;
typedef struct { uint32_t at; uint8_t status, a, b; } event;
static event log_[16384];
static uint32_t logged;
static int32_t sounding[16][128];
static bool writer(const uint8_t p[4], uint32_t token) {
    assert((token == e.issued + 1u || (e.pending_id && token == e.pending_id)) && logged < 16384u);
    uint8_t kind = p[0], status = p[1], ch = status & 15u;
    if (kind == TK_ON) { assert(sounding[ch][p[2]] == 0); sounding[ch][p[2]]++; }
    if (kind == TK_OFF) { assert(sounding[ch][p[2]] == 1); sounding[ch][p[2]]--; }
    log_[logged++] = (event){now, status, p[2], p[3]};
    return true;
}
static void tick(void) {
    now++; scan.frame++; scan.last_ms = now; scan.valid = true;
    if (e.state == TK_IDLE) (void)tc_arm(&e, &scan, now, true);
    else (void)tc_frame(&e, &scan, now);
    tc_step(&e, now, true, writer);
    if (e.pending_id) tp_complete(&e, e.pending_id, tp_pending_bytes(&e), true, now);
}
static void run(uint32_t ms) { while (ms--) tick(); }
static void press(uint32_t k) { scan.values[k] = 3000u; }
static void lift(uint32_t k) { scan.values[k] = 0u; }
/* Pad at column 0..11 and row from the bottom 0..3: the left half (side 0) or the right one. */
static uint32_t at(uint32_t side, uint32_t column, uint32_t up) { return 48u * side + 4u * column + (3u - up); }
static uint8_t note_of(tp_layout l, uint32_t k) { tp_notes n; assert(tc_notes(l, k, (tc_chord){0, 0, 0}, &n) && n.count == 1u); return n.notes[0]; }
static int on_channel(uint8_t pitch) {
    for (uint32_t j = logged; j-- > 0;) if ((log_[j].status >> 4) == TK_ON && log_[j].a == pitch) return log_[j].status & 15u;
    return -1;
}
static uint32_t ons(void) { uint32_t n = 0; for (uint32_t j = 0; j < logged; ++j) n += (log_[j].status >> 4) == TK_ON; return n; }
static bool heard(uint8_t pitch, uint8_t ch) { return sounding[ch & 15u][pitch & 127u] != 0; }
static void lift_all(void) { memset(scan.values, 0, sizeof scan.values); }
/* A fresh board in All Melody (join, split), a second into its session. */
static void fresh(uint8_t join, uint8_t split) {
    lift_all(); memset(&e, 0, sizeof e); memset(sounding, 0, sizeof sounding);
    memset(flash, 0xFF, sizeof flash); tc_init(&tc_state); (void)st_load(&tc_state);
    for (uint32_t p = 0; p < LN_PATTERNS; ++p) ln_clear(&ln_bank[p]);
    tc_state.unstick = 0u; tc_state.glide = TC_GLIDE_OFF;
    tc_state.vel_mode = tc_state.melody_vel = TC_VEL_FIXED;
    tc_state.pad_trigger = 7u; tc_state.pad_release = 3u; tc_state.pad_full = 25u;
    tc_state.chord_ch = 1u; tc_state.melody_ch = 0u; /* chords on channel 2, melody on 1 */
    e.layout = tc_layout(&tc_state);
    assert(tc_set_item(&tc_state, &e, TC_MENU_CHORDS_SIDE, TC_PADS_ALL_MELODY));
    assert(tc_set_item(&tc_state, &e, TC_MENU_JOIN, join) && tc_set_item(&tc_state, &e, TC_MENU_SPLIT, split));
    run(1000);
    logged = 0;
}
static tp_layout whole(uint8_t layout, uint8_t join, uint8_t right, uint8_t base) {
    tp_layout l = TP_DEFAULT_LAYOUT;
    l.melody_layout = layout; l.whole = 1u; l.join = join; l.right_oct = right; l.melody_base = base;
    return l;
}
static uint32_t distinct(tp_layout l) {
    bool seen[128] = {false}; uint32_t n = 0;
    for (uint32_t k = 0; k < TS_KEYS; ++k) { uint8_t p = note_of(l, k); if (!seen[p]) { seen[p] = true; n++; } }
    return n;
}
/* ---- the notes ---- */
static void notes(void) {
    static const uint8_t major[7] = {0, 2, 4, 5, 7, 9, 11};
    tc_init(&tc_state);
    /* Stacked, Chromatic, C1 at the bottom left: C1..B4 on the left, C5..B8 on the right, 96 different notes */
    tp_layout l = whole(TP_MELODY_CHROMATIC, TP_JOIN_STACKED, TP_RIGHT_AUTO, 24u);
    for (uint32_t c = 0; c < 12u; ++c) for (uint32_t u = 0; u < 4u; ++u) {
        assert(note_of(l, at(0, c, u)) == 24u + c + 12u * u && note_of(l, at(1, c, u)) == 72u + c + 12u * u);
    }
    assert(distinct(l) == 96u);
    /* Stacked, Scale: each half as the melody half plays, the right four octaves higher */
    l = whole(TP_MELODY_SCALE, TP_JOIN_STACKED, TP_RIGHT_AUTO, 24u);
    for (uint32_t c = 0; c < 12u; ++c) for (uint32_t u = 0; u < 4u; ++u) {
        uint32_t want = 24u + 12u * u + 12u * (c / 7u) + major[c % 7u];
        assert(note_of(l, at(0, c, u)) == want && note_of(l, at(1, c, u)) == want + 48u);
    }
    /* Fourths: the right half four rows of fourths on (20 semitones) */
    l = whole(TP_MELODY_FOURTHS, TP_JOIN_STACKED, TP_RIGHT_AUTO, 36u);
    for (uint32_t c = 0; c < 12u; ++c) for (uint32_t u = 0; u < 4u; ++u)
        assert(note_of(l, at(0, c, u)) == 36u + c + 5u * u && note_of(l, at(1, c, u)) == 56u + c + 5u * u);
    /* A set right octave: +0 the same notes, -2 two octaves below the left half, +6 six above */
    for (uint8_t r = 1; r < TP_RIGHT_OCTAVES; ++r) {
        l = whole(TP_MELODY_CHROMATIC, TP_JOIN_STACKED, r, r < 3u ? 48u : 12u);
        int32_t by = 12 * ((int32_t)r - 3);
        for (uint32_t c = 0; c < 12u; ++c) assert((int32_t)note_of(l, at(1, c, 0)) == (int32_t)note_of(l, at(0, c, 0)) + by);
    }
    /* Wide rows: straight across both halves; Chromatic rows two octaves apart, so 96 different notes again */
    l = whole(TP_MELODY_CHROMATIC, TP_JOIN_WIDE, TP_RIGHT_AUTO, 24u);
    for (uint32_t s = 0; s < 2u; ++s) for (uint32_t c = 0; c < 12u; ++c) for (uint32_t u = 0; u < 4u; ++u)
        assert(note_of(l, at(s, c, u)) == 24u + 12u * s + c + 24u * u);
    assert(distinct(l) == 96u);
    l = whole(TP_MELODY_SCALE, TP_JOIN_WIDE, TP_RIGHT_AUTO, 36u);
    assert(note_of(l, at(1, 0, 0)) == 36u + 12u + major[12u % 7u] && note_of(l, at(0, 11, 0)) == 36u + 12u + major[11u % 7u]);
    /* Kalimba, C major: the design's row; L12 and R1 the root, neighbours on a side a third apart */
    l = whole(TP_MELODY_SCALE, TP_JOIN_KALIMBA, TP_RIGHT_AUTO, 48u);
    static const int8_t row[24] = {36, 33, 29, 26, 23, 19, 16, 12, 9, 5, 2, 0,   0, 4, 7, 11, 14, 17, 21, 24, 28, 31, 35, 38};
    for (uint32_t i = 0; i < 24u; ++i) {
        uint32_t k = at(i / 12u, i % 12u, 0);
        assert((int32_t)note_of(l, k) == 48 + row[i]);
        assert(note_of(l, at(i / 12u, i % 12u, 2)) == note_of(l, k) + 24u); /* rows an octave apart */
    }
    /* Drums, Bass, Live: as Scale */
    for (uint8_t m = TP_MELODY_DRUMS; m < TP_MELODY_LAYOUTS; ++m) {
        tp_layout d = whole(m, TP_JOIN_STACKED, TP_RIGHT_AUTO, 24u), s = whole(TP_MELODY_SCALE, TP_JOIN_STACKED, TP_RIGHT_AUTO, 24u);
        for (uint32_t k = 0; k < TS_KEYS; ++k) assert(note_of(d, k) == note_of(s, k));
        assert(!tp_live_pad(d, 60u));
    }
    /* Out of range: refused, not wrapped */
    tp_notes n;
    assert(!tc_notes(whole(TP_MELODY_SCALE, TP_JOIN_STACKED, TP_RIGHT_AUTO, 48u), at(1, 11, 3), (tc_chord){0, 0, 0}, &n));
    l = whole(TP_MELODY_SCALE, TP_JOIN_STACKED, 0u, 24u);
    l.join = TP_JOINS; assert(!tc_layout_ok(l));
    l.join = 0u; l.right_oct = TP_RIGHT_OCTAVES; assert(!tc_layout_ok(l));
    /* The roles: every pad melody, the left half's settings the chord half's only with Split */
    l = whole(TP_MELODY_SCALE, TP_JOIN_STACKED, 0u, 24u);
    assert(tp_melody_pad(l, 0u) && tp_melody_side(l, 0u));
    l.split = 1u; assert(tp_melody_pad(l, 0u) && !tp_melody_side(l, 0u) && tp_melody_side(l, 48u));
    l.whole = 0u; assert(!tp_melody_pad(l, 0u) && tp_melody_pad(l, 48u));
}
/* ---- turning it on, the octave, the menu ---- */
static void range(void) {
    memset(&e, 0, sizeof e); tc_init(&tc_state); e.layout = tc_layout(&tc_state);
    tc_state.melody_layout = TP_MELODY_DRUMS; e.layout = tc_layout(&tc_state);
    uint8_t items[TC_GROUP_MAX];
    assert(tc_page_items(&tc_state, TC_GROUP_LAYOUT, items) == 4u); /* Melody, Pads, Melody oct, Reset */
    /* On: Stacked Scale reaches past the top of MIDI at C3, so the octave comes down to C1 */
    assert(tc_set_item(&tc_state, &e, TC_MENU_CHORDS_SIDE, TC_PADS_ALL_MELODY));
    assert(tc_all_melody(&tc_state) && tc_state.melody_base == 24u && e.layout.whole && e.layout.melody_base == 24u);
    assert(tc_state.melody_layout == TP_MELODY_DRUMS && e.layout.melody_layout == TP_MELODY_SCALE); /* kept, played as Scale */
    assert(tc_page_items(&tc_state, TC_GROUP_LAYOUT, items) == 7u && items[2] == TC_MENU_JOIN && items[3] == TC_MENU_RIGHT_OCT &&
           items[4] == TC_MENU_SPLIT);
    assert(tc_menu_values(TC_MENU_CHORDS_SIDE) == 3u && tc_menu_values(TC_MENU_JOIN) == 3u &&
           tc_menu_values(TC_MENU_RIGHT_OCT) == 10u && tc_menu_values(TC_MENU_SPLIT) == 2u);
    assert(tc_menu_values(TC_MENU_MELODY_OCT) == 11u && tc_menu_value(&tc_state, TC_MENU_MELODY_OCT) == 2u);
    /* A key change keeps the stored layout; D would reach past the top at C1: the octave comes down to C0 */
    tp_layout key = tc_layout(&tc_state); key.tonic = 2u;
    assert(tc_set_layout(&tc_state, &e, key) && tc_state.melody_layout == TP_MELODY_DRUMS && tc_state.tonic == 2u);
    assert(tc_state.melody_base == 12u);
    key = tc_layout(&tc_state); key.tonic = 0u; assert(tc_set_layout(&tc_state, &e, key) && tc_state.melody_base == 12u);
    assert(tc_set_item(&tc_state, &e, TC_MENU_MELODY_OCT, 2u) && tc_state.melody_base == 24u);
    assert(!tc_set_item(&tc_state, &e, TC_MENU_MELODY_OCT, 3u) && tc_state.melody_base == 24u); /* C2 asked for: refused */
    /* Kalimba: the right octave goes (not shown), the octave can go back up */
    assert(tc_set_item(&tc_state, &e, TC_MENU_JOIN, TP_JOIN_KALIMBA));
    assert(tc_page_items(&tc_state, TC_GROUP_LAYOUT, items) == 6u);
    assert(tc_set_item(&tc_state, &e, TC_MENU_MELODY_OCT, 4u) && tc_state.melody_base == 48u); /* C3 fits in Kalimba */
    assert(!tc_set_item(&tc_state, &e, TC_MENU_MELODY_OCT, 10u) && tc_state.melody_base == 48u); /* C9 doesn't: refused */
    /* Stacked again with the right half six octaves up: the octave comes down to C-1 */
    assert(tc_set_item(&tc_state, &e, TC_MENU_JOIN, TP_JOIN_STACKED) && tc_state.melody_base == 24u);
    assert(tc_set_item(&tc_state, &e, TC_MENU_RIGHT_OCT, 9u) && tc_state.melody_base == 0u && tc_state.right_octave == 9u);
    for (uint32_t k = 0; k < TS_KEYS; ++k) { tp_notes n; assert(tc_notes(e.layout, k, (tc_chord){0, 0, 0}, &n)); }
    assert(tc_layout_allowed(&tc_state, e.layout));
    /* Off: the usual range again (C1 at least), the Drums layout back */
    assert(tc_set_item(&tc_state, &e, TC_MENU_CHORDS_SIDE, TC_PADS_RIGHT));
    assert(!tc_all_melody(&tc_state) && !e.layout.whole && tc_state.melody_base == 24u && e.layout.melody_layout == TP_MELODY_DRUMS);
    assert(tc_physical(&tc_state, 0u) == 48u && tc_menu_values(TC_MENU_MELODY_OCT) == 5u);
    assert(tc_set_item(&tc_state, &e, TC_MENU_CHORDS_SIDE, TC_PADS_ALL_MELODY) && tc_physical(&tc_state, 0u) == 0u);
    /* The names on the screen */
    static const struct { uint8_t item, v; const char *name; } names[] = {
        {TC_MENU_CHORDS_SIDE, 0, "CHORDS LEFT"}, {TC_MENU_CHORDS_SIDE, 1, "CHORDS RIGHT"}, {TC_MENU_CHORDS_SIDE, 2, "ALL MELODY"},
        {TC_MENU_JOIN, 0, "STACKED"}, {TC_MENU_JOIN, 1, "WIDE ROWS"}, {TC_MENU_JOIN, 2, "KALIMBA"},
        {TC_MENU_RIGHT_OCT, 0, "AUTO"}, {TC_MENU_RIGHT_OCT, 1, "-2"}, {TC_MENU_RIGHT_OCT, 3, "+0"}, {TC_MENU_RIGHT_OCT, 9, "+6"},
        {TC_MENU_SPLIT, 1, "ON"}, {TC_MENU_MELODY_OCT, 0, "-4"}, {TC_MENU_MELODY_OCT, 4, "0"}, {TC_MENU_MELODY_OCT, 10, "+6"}};
    for (uint32_t i = 0; i < sizeof names / sizeof names[0]; ++i) {
        ui_start(); ui_value_name(names[i].item, names[i].v);
        if (strcmp(ui_buffer, names[i].name)) { printf("item %u value %u: %s\n", names[i].item, names[i].v, ui_buffer); assert(0); }
    }
    assert(!strcmp(ui_item_name(TC_MENU_CHORDS_SIDE), "PADS") && !strcmp(ui_item_name(TC_MENU_RIGHT_OCT), "RIGHT OCTAVE"));
    assert(tc_set_item(&tc_state, &e, TC_MENU_SPLIT, 1u) && tc_state.whole_split == 1u && e.layout.split == 1u);
}
/* ---- the record ---- */
static void stored(void) {
    memset(flash, 0xFF, sizeof flash); memset(&e, 0, sizeof e); tc_init(&tc_state); (void)st_load(&tc_state);
    e.layout = tc_layout(&tc_state);
    assert(tc_set_item(&tc_state, &e, TC_MENU_CHORDS_SIDE, TC_PADS_ALL_MELODY) && tc_set_item(&tc_state, &e, TC_MENU_JOIN, TP_JOIN_WIDE) &&
           tc_set_item(&tc_state, &e, TC_MENU_SPLIT, 1u));
    tc_state.right_octave = 5u; /* kept while not shown */
    uint32_t p[ST_PAYLOAD];
    assert(st_get(ST_LIVE, 0u, &tc_state, p) == ST_OK);
    tc_controls d; tc_init(&d);
    assert(st_put(ST_LIVE, 0u, p, &d, &e) == ST_OK);
    assert(d.chords_right == TC_PADS_ALL_MELODY && d.whole_join == TP_JOIN_WIDE && d.whole_split == 1u && d.right_octave == 5u &&
           d.melody_base == tc_state.melody_base);
    /* The melody base below the usual range: read back in All Melody, refused without it */
    tc_state.melody_base = 12u; assert(tc_layout_allowed(&tc_state, tc_layout(&tc_state)));
    assert(st_get(ST_LIVE, 0u, &tc_state, p) == ST_OK);
    tc_init(&d); assert(st_put(ST_LIVE, 0u, p, &d, &e) == ST_OK && d.melody_base == 12u);
    p[26] &= 0x00FFFFFFu; tc_init(&d); assert(st_put(ST_LIVE, 0u, p, &d, &e) == ST_BAD_VALUE);
}
/* ---- playing ---- */
static void playing(void) {
    /* Split off: every pad on the melody channel, its own note */
    fresh(TP_JOIN_STACKED, 0u);
    uint32_t left = at(0, 0, 0), right = at(1, 0, 0);
    uint8_t nl = note_of(e.layout, left), nr = note_of(e.layout, right);
    assert(nl == 24u && nr == 72u);
    press(left); press(right); run(50);
    assert(heard(nl, 0u) && heard(nr, 0u) && ons() == 2u);
    lift_all(); run(50); assert(!heard(nl, 0u) && !heard(nr, 0u));
    /* Split on: the left half on the chord channel */
    fresh(TP_JOIN_STACKED, 1u);
    press(left); press(right); run(50);
    assert(on_channel(nl) == 1 && on_channel(nr) == 0);
    lift_all(); run(50);
    /* The arpeggiator, Hold and Strum leave the left half playing as a melody pad */
    fresh(TP_JOIN_WIDE, 0u);
    tc_state.arp = TC_ARP_UP; tc_state.hold = 1u; tc_state.strum = TC_STRUM_UP;
    nl = note_of(e.layout, left);
    press(left); run(30); assert(heard(nl, 0u) && !e.keys[left].silent);
    lift(left); run(50); assert(!heard(nl, 0u) && !e.keys[left].latched);
    tc_state.arp = TC_ARP_OFF; tc_state.hold = 0u; tc_state.strum = TC_STRUM_OFF;
    /* Mono: one line across the board */
    fresh(TP_JOIN_STACKED, 0u); tc_state.mono = 1u;
    nl = note_of(e.layout, left); nr = note_of(e.layout, right);
    press(left); run(30); press(right); run(50);
    assert(heard(nr, 0u) && !heard(nl, 0u)); /* the newest only */
    lift(right); run(50); assert(heard(nl, 0u) && !heard(nr, 0u)); /* the older one back */
    lift_all(); run(50);
    /* With Split: a line per half, both sound */
    fresh(TP_JOIN_STACKED, 1u); tc_state.mono = 1u;
    uint32_t left2 = at(0, 1, 0), right2 = at(1, 1, 0);
    press(left); press(right); run(30); press(left2); press(right2); run(50);
    assert(heard(note_of(e.layout, left2), 1u) && heard(note_of(e.layout, right2), 0u));
    assert(!heard(note_of(e.layout, left), 1u) && !heard(note_of(e.layout, right), 0u));
    lift(left2); run(50); assert(heard(note_of(e.layout, left), 1u) && heard(note_of(e.layout, right2), 0u));
    lift_all(); run(50);
    tc_state.mono = 0u;
    /* Glide (MPE on the melody pads): across the seam in Wide rows, not in Stacked */
    for (uint32_t join = TP_JOIN_STACKED; join <= TP_JOIN_WIDE; ++join) {
        fresh((uint8_t)join, 0u);
        tc_state.mpe = TC_MPE_MELODY; tc_state.glide = 1u; tc_state.at_melody = TC_AT_CHANNEL;
        assert(tc_set_layout(&tc_state, &e, tc_layout(&tc_state)));
        lift_all(); run(1000); logged = 0;
        uint32_t l12 = at(0, 11, 1), r1 = at(1, 0, 1);
        press(l12); run(50);
        uint32_t before = ons();
        press(r1); run(50);
        if (join == TP_JOIN_WIDE) assert(ons() == before && e.keys[r1].glide == l12 + 1u);
        else assert(ons() == before + 1u && !e.keys[r1].glide);
        lift_all(); run(100);
    }
    /* A melody lane plays what it did (the right half's Scale layout), All Melody or not */
    fresh(TP_JOIN_STACKED, 0u);
    ln_step s; memset(&s, 0, sizeof s); s.on = true; s.value = 0u;
    tp_notes n; uint8_t ch;
    tp_layout plain = e.layout; plain.whole = 0u;
    assert(tc_step_notes(e.layout, LN_MELODY, 0u, &s, &n, &ch) && n.count == 1u && n.notes[0] == note_of(plain, 48u));
}
/* 2.4.10: the lights' tc_melody_note gives every melody pad's note as tc_pad does (any layout, join, right octave, octave,
 * key, scale; All Melody or not), and nothing for a chord pad, a Live area pad or a note past MIDI. */
static void lights_note(void) {
    tc_init(&tc_state);
    static const uint8_t bases[4] = {0u, 24u, 48u, 108u}, modes[3] = {0u, 1u, 5u};
    uint32_t checked = 0, refused = 0;
    for (uint8_t w = 0; w < 2u; ++w) for (uint8_t m = 0; m < TP_MELODY_LAYOUTS; ++m) for (uint8_t j = 0; j < TP_JOINS; ++j)
    for (uint8_t r = 0; r < TP_RIGHT_OCTAVES; r = (uint8_t)(r + 3u)) for (uint32_t b = 0; b < 4u; ++b)
    for (uint8_t t = 0; t < 12u; t = (uint8_t)(t + 5u)) for (uint32_t s = 0; s < 3u; ++s) {
        tp_layout l = whole(m, j, r, bases[b]); l.whole = w; l.tonic = t; l.mode = modes[s];
        for (uint32_t k = 0; k < TS_KEYS; ++k) {
            tp_notes n; uint8_t note = 0xEEu;
            bool want = tp_melody_pad(l, k) && !tp_live_pad(l, k) && tc_pad(l, k, &n) && n.count == 1u;
            bool got = tc_melody_note(l, k, &note);
            assert(want == got && (!got || note == n.notes[0]));
            checked++; refused += !got;
        }
    }
    assert(checked > 100000u && refused > 0u && refused < checked);
    uint8_t note; tp_layout bad = whole(TP_MELODY_SCALE, TP_JOINS, 0u, 24u);
    assert(!tc_melody_note(bad, 60u, &note) && !tc_melody_note(whole(TP_MELODY_SCALE, 0u, 0u, 24u), 96u, &note));
}
int main(void) {
    lights_note();
    notes();
    range();
    stored();
    playing();
    printf("PASS: All Melody: Stacked (Chromatic C1..B8 all different, Scale and Fourths on from the left, a set right octave "
           "-2..+6), Wide rows, Kalimba (the design's row, the centre shared), Drums/Bass/Live as Scale; turned on, the "
           "octave down until all 96 fit (a right octave of +6: C-1), C9 refused, off again the usual range and the stored "
           "layout back; menu items, values and names; the record round trip, a base below C1 only in All Melody; every pad "
           "on the melody channel, Split's left half on the chord channel, the arpeggiator, Hold and Strum leave it alone, "
           "Mono one line or one per half, Glide over the seam in Wide rows only, melody lanes as before; the lights' melody note as tc_pad's in every layout\n");
    return 0;
}
