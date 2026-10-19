/* SPDX-License-Identifier: GPL-3.0-or-later - Stage 4 (0.7.0) on the real engine and layout:
 * the scale library (every built-in scale valid, drawable and distinct, all pads in range
 * and in the scale), the mode families on M1..M7, the guitar voicing on six strings with a
 * capo, and the strum feel (random timing and velocity, order kept).
 * 1 ms ticks, a fresh scan frame every tick, every packet receipted at once. */
#include "../../src/tb/controls.h"
#include "../../src/tb/display.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
volatile ts_abi ts_status;
static tp_engine e;
static ts_scan scan;
static uint32_t now;
typedef struct { uint32_t at; uint8_t status, a, b; } event;
static event log_[8192];
static uint32_t logged;
static int32_t sounding[16][128];
static bool writer(const uint8_t p[4], uint32_t token) {
    assert(token == e.issued + 1u && logged < 8192u);
    uint8_t kind = p[0], status = p[1], ch = status & 15u;
    assert((status >> 4) == kind && p[2] < 128u && p[3] < 128u);
    if (kind == TK_ON) { assert(sounding[ch][p[2]] == 0 && p[3] >= 1u); sounding[ch][p[2]]++; }
    if (kind == TK_OFF) { assert(sounding[ch][p[2]] == 1 && !p[3]); sounding[ch][p[2]]--; }
    log_[logged++] = (event){now, status, p[2], p[3]};
    return true;
}
static void tick(void) {
    now++; scan.frame++; scan.last_ms = now; scan.valid = true;
    if (e.state == TK_IDLE) (void)tc_arm(&e, &scan, now, true);
    else (void)tc_frame(&e, &scan, now);
    tc_step(&e, now, true, writer);
    if (e.pending_id) tp_complete(&e, e.pending_id, 4, true, now);
}
static void run(uint32_t ms) { while (ms--) tick(); }
static void quiet(void) {
    for (uint32_t ch = 0; ch < 16u; ++ch) for (uint32_t p = 0; p < 128u; ++p) assert(!sounding[ch][p]);
    assert(!tp_obligations(&e) && !e.errors && e.state == TK_ACTIVE);
}
static uint8_t key_of(uint32_t column, uint32_t row) { /* chord pads */
    uint8_t k; assert(tp_index((tp_position){0u, (uint8_t)row, (uint8_t)column}, &k)); return k;
}
static bool same_notes(const tp_notes *a, const uint8_t *b, uint32_t n) {
    if (a->count != n) return false;
    for (uint32_t i = 0; i < n; ++i) if (a->notes[i] != b[i]) return false;
    return true;
}
static bool user(uint8_t id) { return id >= TC_SCALES && id < TC_EXTRA_FIRST; }
static uint16_t mask_of(uint8_t id) {
    uint16_t m = 0;
    for (uint32_t d = 0; d < tc_scale_size(id); ++d) m |= (uint16_t)(1u << tc_step_semitones(id, d));
    return m;
}
/* --- the library ------------------------------------------------------------------------ */
static void library(void) {
    tc_chord none = {0, 0, 0};
    uint16_t seen[TC_ALL_SCALES]; uint32_t distinct = 0;
    assert(TC_ALL_SCALES <= 127u); /* SELECT_SCALE and favourites carry a 7-bit id */
    for (uint8_t id = 0; id < TC_ALL_SCALES; ++id) {
        if (user(id)) { assert(!tc_scale_defined(id)); continue; }
        assert(tc_scale_defined(id));
        const char *name = tc_scale_name(id);
        assert(strlen(name) >= 1u && strlen(name) <= 12u);
        for (const char *ch = name; *ch; ++ch) assert(strchr(td_font_chars, *ch)); /* the display can draw it */
        uint32_t n = tc_scale_size(id);
        assert(n >= 5u && n <= 12u && tc_step_semitones(id, 0) == 0u);
        for (uint32_t d = 1; d < n; ++d) assert(tc_step_semitones(id, d) > tc_step_semitones(id, d - 1u) &&
                                                tc_step_semitones(id, d) < 12u);
        uint16_t m = mask_of(id);
        for (uint32_t i = 0; i < distinct; ++i) assert(seen[i] != m); /* no two scales alike */
        seen[distinct++] = m;
        /* Every pad plays in every key; melody pads stay in the scale. */
        for (uint8_t tonic = 0; tonic < 12u; ++tonic) {
            tp_layout l = TP_DEFAULT_LAYOUT; l.mode = id; l.tonic = tonic;
            for (uint32_t k = 0; k < TS_KEYS; ++k) {
                tp_notes notes; assert(tc_notes(l, k, none, &notes) && notes.count >= 1u);
                for (uint32_t i = 0; i < notes.count; ++i)
                    assert(tc_in_scale(id, (uint32_t)(notes.notes[i] + 120u - l.melody_base - tonic) % 12u));
            }
        }
    }
    assert(distinct == TC_ALL_SCALES - TC_USER_SCALES);
    assert(!strcmp(tc_scale_name(TC_LIBRARY_FIRST), "DORIAN b2") && !strcmp(tc_scale_name(TC_ALL_SCALES - 1u), "BLUES 9"));
}
/* --- families: M1..M7 ---------------------------------------------------------------------- */
static void families(void) {
    static const uint8_t major[7] = {0, 2, 3, 4, 5, 1, 6};
    for (uint32_t m = 0; m < 7u; ++m) assert(tc_family_mode(0, m) == major[m] && tc_mode_buttons[2u + m] == major[m]);
    assert(TC_FAMILIES == 4u && !strcmp(tc_family_name(1), "MEL MINOR") && !strcmp(tc_family_name(3), "HARM MAJOR"));
    /* Mode m of a family starts on step m of its first mode (an independent rotation). */
    for (uint8_t f = 0; f < TC_FAMILIES; ++f) {
        uint8_t parent = tc_family_mode(f, 0);
        assert(tc_scale_size(parent) == 7u);
        for (uint32_t m = 0; m < 7u; ++m) {
            uint8_t id = tc_family_mode(f, m);
            uint16_t want = 0;
            for (uint32_t d = 0; d < 7u; ++d)
                want |= (uint16_t)(1u << ((tc_step_semitones(parent, (m + d) % 7u) + 12u - tc_step_semitones(parent, m)) % 12u));
            assert(mask_of(id) == want);
        }
    }
    tc_controls c; tc_init(&c);
    assert(tc_mode_scale(&c, 5) == 4u);                      /* M4 = Lydian */
    c.family = 1u;
    assert(tc_mode_scale(&c, 2) == 8u && tc_mode_scale(&c, 5) == 35u && tc_mode_scale(&c, 8) == 36u); /* MM, Lydian dominant, altered */
    c.family = 2u; assert(tc_mode_scale(&c, 6) == 28u);      /* M5 = Phrygian dominant */
    assert(tc_mode_scale(&c, 0) == 9u && tc_mode_scale(&c, 1) == 11u && tc_mode_scale(&c, 9) == TC_SCALES); /* 5, 12, U1 */
    assert(tc_mode_scale(&c, 12) == TC_EMPTY_SLOT);
}
/* --- guitar voicing ------------------------------------------------------------------------ */
static void guitar(void) {
    tc_chord none = {0, 0, 0};
    tc_panel p = TC_PANEL_NEUTRAL; p.voicing = TC_VOICING_GUITAR;
    tp_layout l = TP_DEFAULT_LAYOUT; tp_notes n;
    /* Open chords, as a guitarist plays them (bottom row = open position). */
    const uint8_t c_open[] = {48, 52, 55, 60, 64},          /* C  x32010 */
                  f_open[] = {41, 45, 53, 57, 60, 65},      /* F  103211 */
                  am_open[] = {45, 52, 57, 60, 64},         /* Am x02210 */
                  em_open[] = {40, 47, 52, 55, 59, 64},     /* Em 022000 */
                  g_open[] = {43, 47, 50, 55, 59, 67},      /* G  320003 */
                  d_open[] = {50, 57, 62, 66};              /* D  xx0232 */
    assert(tc_notes_panel(l, key_of(0, 3), none, p, &n) && same_notes(&n, c_open, 5));
    assert(tc_notes_panel(l, key_of(3, 3), none, p, &n) && same_notes(&n, f_open, 6));
    assert(tc_notes_panel(l, key_of(5, 3), none, p, &n) && same_notes(&n, am_open, 5));
    assert(tc_notes_panel(l, key_of(2, 3), none, p, &n) && same_notes(&n, em_open, 6));
    assert(tc_notes_panel(l, key_of(4, 3), none, p, &n) && same_notes(&n, g_open, 6));
    l.tonic = 2u; assert(tc_notes_panel(l, key_of(0, 3), none, p, &n) && same_notes(&n, d_open, 4));
    /* Capo 2: the same C chord, played two frets up (x3 2 5 5 3): C E C E G. */
    l.tonic = 0u; p.capo = 2u;
    const uint8_t c_capo2[] = {48, 52, 60, 64, 67};
    assert(tc_notes_panel(l, key_of(0, 3), none, p, &n) && same_notes(&n, c_capo2, 5));
    /* Every chord in every classic scale, key, row and capo: 1..6 strings, chord notes only,
     * never below the capo's open low E, at most 4 frets above the hand's position. */
    for (uint8_t mode = 0; mode < TC_SCALES; ++mode) for (uint8_t tonic = 0; tonic < 12u; ++tonic)
        for (uint8_t capo = 0; capo < TC_CAPOS; ++capo) for (uint32_t col = 0; col < 12u; ++col)
            for (uint32_t row = 0; row < 4u; ++row) {
                tp_layout t = TP_DEFAULT_LAYOUT; t.mode = mode; t.tonic = tonic; p.capo = capo;
                tp_notes plain;
                assert(tc_notes(t, key_of(col, row), none, &plain) && tc_notes_panel(t, key_of(col, row), none, p, &n));
                assert(n.count >= 1u && n.count <= 6u && n.notes[0] >= 40u + capo && n.notes[n.count - 1u] <= 64u + capo + 7u + 4u);
                uint32_t classes = 0;
                for (uint32_t i = 0; i < plain.count; ++i) classes |= 1u << (plain.notes[i] % 12u);
                for (uint32_t i = 0; i < n.count; ++i) assert((classes >> (n.notes[i] % 12u)) & 1u);
                assert(n.notes[0] % 12u == plain.notes[0] % 12u || row != 3u || n.count < 3u); /* stands on its root */
            }
}
/* --- strum feel ---------------------------------------------------------------------------- */
static void strike(uint32_t k, uint32_t *start) {
    logged = 0; scan.values[k] = 3000u; *start = now; run(300); scan.values[k] = 0u; run(60); quiet();
}
static void feel(void) {
    tc_init(&tc_state);
    tc_state.at_chord = tc_state.at_melody = TC_AT_OFF; tc_state.vel_mode = tc_state.melody_vel = TC_VEL_FIXED;
    tc_state.strum = TC_STRUM_UP; tc_state.strum_speed = 2u; /* 20 ms */
    e.layout = tc_layout(&tc_state);
    run(600); /* armed */
    uint32_t start;
    /* Off: exactly 20 ms apart, velocity 100. */
    strike(3, &start);
    for (uint32_t r = 0; r < 3u; ++r) {
        const event *on = NULL; uint32_t seen = 0;
        for (uint32_t i = 0; i < logged; ++i) if (log_[i].status == 0x90 && seen++ == r) on = &log_[i];
        assert(on && on->b == 100u && (r == 0u || on->at - log_[0].at == 20u * r));
    }
    /* On: up to 40 ms late and 30 off; the notes still start low to high. */
    tc_state.feel_time = 4u; tc_state.feel_vel = 4u;
    uint32_t spread_time = 0, low = 127, high = 0;
    for (uint32_t s = 0; s < 40u; ++s) {
        strike(3, &start);
        uint32_t previous = 0, found = 0; uint8_t last_note = 0;
        for (uint32_t i = 0; i < logged; ++i) {
            if (log_[i].status != 0x90) continue;
            uint32_t late = log_[i].at - start;
            assert(found == 0u || (log_[i].at >= previous && log_[i].a > last_note));
            assert(late >= 20u * found && late <= 20u * found + 40u + 4u);
            assert(log_[i].b >= 70u && log_[i].b <= 127u);
            if (found && log_[i].at - previous != 20u) spread_time++;
            if (log_[i].b < low) low = log_[i].b;
            if (log_[i].b > high) high = log_[i].b;
            previous = log_[i].at; last_note = log_[i].a; found++;
        }
        assert(found == 3u);
    }
    assert(spread_time > 10u && low < 90u && high > 110u); /* it does vary */
    /* Melody pads never strum, so feel leaves them alone. */
    strike(48u + 3u, &start);
    for (uint32_t i = 0; i < logged; ++i) if (log_[i].status == 0x90) assert(log_[i].b == 100u);
    tc_state.feel_time = tc_state.feel_vel = 0u; tc_state.strum = TC_STRUM_OFF;
}
int main(void) {
    library();
    families();
    guitar();
    feel();
    puts("PASS: stage 4: scale library (every built-in scale defined, drawable, distinct, all pads in range and in the scale in every key), families (M1..M7 rotations, button mapping), guitar voicing (open C F Am Em G D shapes, capo, chord notes only on every chord/key/row/capo), strum feel (random timing and velocity within limits, order kept, off = exact)");
    return 0;
}
