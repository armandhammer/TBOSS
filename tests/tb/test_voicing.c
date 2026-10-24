/* SPDX-License-Identifier: GPL-3.0-or-later - 0.9.0 on the real engine and layout: fold (every
 * chord inside G#..G above the chord octave's C, the backer's F harmonic minor example, rows
 * still inversions, every pad in range), auto voice leading (C F G moves by steps, stays near
 * home over a long progression, off and guitar unchanged) and note repeat (right-half pads
 * strike again at the note value, velocity from the pressure then; chord pads untouched).
 * 1 ms ticks, a fresh scan frame every tick, every packet receipted at once. */
#include "../../src/tb/controls.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
volatile ts_abi ts_status;
static tp_engine e;
static ts_scan scan;
static uint32_t now;
typedef struct { uint32_t at; uint8_t status, a, b; } event;
static event log_[4096];
static uint32_t logged;
static int32_t sounding[16][128];
static bool writer(const uint8_t p[4], uint32_t token) {
    assert(token == e.issued + 1u && logged < 4096u);
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
static uint8_t pad(uint32_t side, uint32_t column, uint32_t row) {
    uint8_t k; assert(tp_index((tp_position){(uint8_t)side, (uint8_t)row, (uint8_t)column}, &k)); return k;
}
/* Strike a chord pad; its notes, sorted. */
static uint32_t chord(uint32_t k, uint8_t notes[16]) {
    logged = 0; scan.values[k] = 3000u; run(40); scan.values[k] = 0u; run(40); quiet();
    uint32_t n = 0;
    for (uint32_t i = 0; i < logged; ++i) if (log_[i].status == 0x90u && n < 16u) notes[n++] = log_[i].a;
    for (uint32_t i = 1; i < n; ++i)
        for (uint32_t j = i; j && notes[j - 1u] > notes[j]; --j) { uint8_t x = notes[j]; notes[j] = notes[j - 1u]; notes[j - 1u] = x; }
    return n;
}
static void expect(uint32_t k, uint8_t a, uint8_t b, uint8_t c) {
    uint8_t n[16]; assert(chord(k, n) == 3u && n[0] == a && n[1] == b && n[2] == c);
}
/* --- fold ----------------------------------------------------------------------------------- */
static void fold(void) {
    tc_chord none = {0, 0, 0};
    /* The backer's report: F harmonic minor, the F chord starts on G# (Ab C F), the C chord on C. */
    tp_layout l = {5u, TP_MELODY_BASE, TP_CHORD_BASE, 7u, TP_MELODY_SCALE, 1u, 0u};
    assert(tc_layout_ok(l));
    tp_notes n;
    assert(tc_notes(l, pad(0, 0, 3), none, &n) && n.count == 3u && n.notes[0] == 44u && n.notes[1] == 48u && n.notes[2] == 53u);
    assert(tc_notes(l, pad(0, 4, 3), none, &n) && n.count == 3u && n.notes[0] == 48u && n.notes[1] == 52u && n.notes[2] == 55u);
    /* Rows above: the lowest note up an octave each time. */
    assert(tc_notes(l, pad(0, 0, 2), none, &n) && n.notes[0] == 48u && n.notes[1] == 53u && n.notes[2] == 56u);
    assert(tc_notes(l, pad(0, 0, 0), none, &n) && n.notes[0] == 56u && n.notes[1] == 60u && n.notes[2] == 65u);
    l.fold = 2u; assert(!tc_layout_ok(l));
    /* Every scale, key, octave, chord and edit: the bottom row inside G#..G, each pitch once,
     * every pad playable; the 8th column plays what the 1st does. */
    static const tc_chord edits[4] = {{0, 0, 0}, {0, 2, TC_ADD7}, {0, 4, TC_ADD6 | TC_ADD9}, {0, 2, TC_ADD6 | TC_ADD7 | TC_ADD9}};
    for (uint8_t scale = 0; scale < TC_ALL_SCALES; ++scale) {
        if (!tc_scale_defined(scale)) continue;
        for (uint8_t t = 0; t < 12u; ++t)
            for (uint8_t base = TP_CHORD_BASE - TP_OCTAVE_RANGE; base <= TP_CHORD_BASE + TP_OCTAVE_RANGE; base += 12u) {
                tp_layout m = {t, TP_MELODY_BASE, base, scale, TP_MELODY_SCALE, 1u, 0u};
                for (uint32_t x = 0; x < 12u; ++x)
                    for (uint32_t ed = 0; ed < 4u; ++ed)
                        for (uint32_t row = 0; row < 4u; ++row) {
                            assert(tc_notes(m, pad(0, x, row), edits[ed], &n) && n.count >= 2u);
                            for (uint32_t i = 1; i < n.count; ++i) assert(n.notes[i] > n.notes[i - 1u]);
                            if (row == 3u) assert(n.notes[0] >= base + 8u && n.notes[n.count - 1u] < base + 20u);
                        }
                tp_notes a, b;
                if (tc_scale_size(scale) == 7u) {
                    assert(tc_notes(m, pad(0, 0, 3), none, &a) && tc_notes(m, pad(0, 7, 3), none, &b));
                    assert(!memcmp(&a, &b, sizeof a));
                }
            }
    }
}
/* --- voice leading -------------------------------------------------------------------------- */
static void lead(void) {
    tc_init(&tc_state);
    tc_state.at_chord = tc_state.at_melody = TC_AT_OFF; tc_state.vel_mode = TC_VEL_FIXED;
    e.layout = tc_layout(&tc_state);
    run(600); /* armed */
    /* Off: every chord as its pad says. */
    expect(pad(0, 0, 3), 36, 40, 43); expect(pad(0, 3, 3), 41, 45, 48); expect(pad(0, 4, 3), 43, 47, 50);
    /* On: from G, C comes as E G C; from nothing, C as it is; then F as C F A, G as D G B
     * and C as E G C: each voice moves a step or stays. */
    tc_state.voice_lead = 1u;
    expect(pad(0, 0, 3), 40, 43, 48);
    tc_state.lead.count = 0u;
    expect(pad(0, 0, 3), 36, 40, 43);
    expect(pad(0, 3, 3), 36, 41, 45);
    expect(pad(0, 4, 3), 38, 43, 47);
    expect(pad(0, 0, 3), 40, 43, 48);
    /* A long progression stays near its pads' register. */
    static const uint8_t cycle[8] = {0, 5, 3, 4, 1, 4, 0, 6};
    for (uint32_t round = 0; round < 25u; ++round)
        for (uint32_t i = 0; i < 8u; ++i) {
            uint8_t n[16]; uint32_t count = chord(pad(0, cycle[i], 3), n);
            assert(count == 3u && n[0] >= 29u && n[2] <= 62u);
        }
    /* A higher row leads from higher up. */
    uint8_t n[16];
    tc_state.lead.count = 0u;
    expect(pad(0, 0, 3), 36, 40, 43);
    assert(chord(pad(0, 3, 0), n) == 3u && n[0] == 45u); /* top-row F: A C F, not C F A */
    /* Guitar voicing ignores it: the open C shape. */
    tc_state.chord_voicing = TC_VOICING_GUITAR;
    assert(chord(pad(0, 0, 3), n) == 5u && n[0] == 48u && n[4] == 64u);
    tc_state.chord_voicing = 0u; tc_state.voice_lead = 0u;
}
/* --- note repeat ---------------------------------------------------------------------------- */
/* Note Ons while pad k is held for ms (harder from soft_until): the first and last velocity, the
 * gap from the touch to the second strike, and the shortest and longest gap after that. */
static uint32_t first_gap;
static uint32_t held_ons(uint32_t k, uint32_t ms, uint32_t soft_until, uint32_t *first, uint32_t *last, uint32_t *gap_lo, uint32_t *gap_hi) {
    logged = 0; scan.values[k] = 2600u;
    for (uint32_t t = 0; t < ms; ++t) { if (t == soft_until) scan.values[k] = 4000u; run(1); }
    scan.values[k] = 0u; run(40); quiet();
    uint32_t n = 0, previous = 0; *gap_lo = UINT32_MAX; *gap_hi = 0; first_gap = 0;
    for (uint32_t i = 0; i < logged; ++i) if ((log_[i].status & 0xF0u) == 0x90u) {
        uint32_t gap = log_[i].at - previous;
        if (n == 1u) first_gap = gap;
        else if (n) { if (gap < *gap_lo) *gap_lo = gap; if (gap > *gap_hi) *gap_hi = gap; }
        if (!n) *first = log_[i].b;
        *last = log_[i].b; previous = log_[i].at; n++;
    }
    return n;
}
/* Every rest before a strike (Note Off to the next Note On) is at least min_rest ms. */
static uint32_t rests_at_least(uint32_t min_rest) {
    uint32_t off_at = 0, rests = 0;
    for (uint32_t i = 0; i < logged; ++i) {
        if ((log_[i].status & 0xF0u) == 0x80u) off_at = log_[i].at;
        else if ((log_[i].status & 0xF0u) == 0x90u && off_at) { assert(log_[i].at - off_at >= min_rest); rests++; }
    }
    return rests;
}
static void repeat(void) {
    tc_init(&tc_state);
    tc_state.at_chord = tc_state.at_melody = TC_AT_OFF;
    e.layout = tc_layout(&tc_state);
    uint32_t first, last, lo, hi;
    /* Off: one strike however long it is held. */
    assert(held_ons(pad(1, 0, 3), 1000, 1000, &first, &last, &lo, &hi) == 1u);
    /* 1/8 at 120 BPM on the clock's grid (1.4.0): the touch strikes at once, the next strike comes
     * on the first eighth at least half an eighth later, then every 250 ms; pressing harder makes
     * the later repeats louder. Each repeat sounds at most half the step and rests at least a
     * quarter: a real gap before every strike. */
    tc_state.note_repeat = 2u;
    for (uint32_t offset = 0; offset < 250u; offset += 37u) { /* touches anywhere between two eighths */
        run(offset);
        uint32_t count = held_ons(pad(1, 0, 3), 1000, 500, &first, &last, &lo, &hi);
        assert(count >= 4u && count <= 5u && first_gap >= 124u && first_gap <= 376u);
        assert(lo >= 249u && hi <= 251u && last > first + 40u);
        assert(rests_at_least(62u) == count - 1u);
    }
    /* 1/32: every 62.5 ms. */
    tc_state.note_repeat = 4u;
    uint32_t count = held_ons(pad(1, 5, 1), 500, 500, &first, &last, &lo, &hi);
    assert(count >= 7u && count <= 9u && lo >= 62u && hi <= 63u);
    /* 5 per beat: every 100 ms, exactly, however long. */
    tc_state.note_repeat = 7u;
    count = held_ons(pad(1, 2, 2), 3000, 3000, &first, &last, &lo, &hi);
    assert(count >= 29u && count <= 31u && lo >= 99u && hi <= 101u);
    /* Repeat rhythm 3/8 at 1/16: three of every eight sixteenths sound (gaps of 2 or 3 steps). */
    uint32_t hits = 0, steps = 0, index = 0;
    while (rh_euclid_at(index, &hits, &steps) && !(hits == 3u && steps == 8u)) index++;
    tc_state.note_repeat = 3u; tc_state.repeat_rhythm = (uint8_t)(index + 1u);
    count = held_ons(pad(1, 3, 3), 2000, 2000, &first, &last, &lo, &hi);
    assert(count >= 6u && count <= 8u && lo >= 249u && hi <= 376u);
    for (uint32_t i = 0, previous = 0, n = 0; i < logged; ++i) if ((log_[i].status & 0xF0u) == 0x90u) {
        uint32_t gap = log_[i].at - previous;
        assert(n < 2u || (gap >= 249u && gap <= 251u) || (gap >= 374u && gap <= 376u));
        previous = log_[i].at; n++;
    }
    tc_state.repeat_rhythm = 0u;
    /* By row: the bottom row 2 per beat, the row above 3: held together, 2 against 3, both on the
     * same beat every half second. */
    tc_state.note_repeat = TC_REPEAT_BY_ROW;
    uint8_t low = pad(1, 0, 3), high = pad(1, 4, 2);
    tp_notes a, b; assert(tc_pad(e.layout, low, &a) && tc_pad(e.layout, high, &b));
    logged = 0; scan.values[low] = scan.values[high] = 3000u;
    run(2000);
    scan.values[low] = scan.values[high] = 0u; run(40); quiet();
    uint32_t n_low = 0, n_high = 0, together = 0, low_at[16] = {0}, high_at[16] = {0};
    for (uint32_t i = 0; i < logged; ++i) if ((log_[i].status & 0xF0u) == 0x90u) {
        if (log_[i].a == a.notes[0] && n_low < 16u) low_at[n_low++] = log_[i].at;
        if (log_[i].a == b.notes[0] && n_high < 16u) high_at[n_high++] = log_[i].at;
    }
    for (uint32_t i = 0; i < n_low; ++i) for (uint32_t j = 0; j < n_high; ++j) /* one packet per ms */
        if ((low_at[i] > high_at[j] ? low_at[i] - high_at[j] : high_at[j] - low_at[i]) <= 3u) together++;
    assert(n_low >= 7u && n_low <= 9u && n_high >= 11u && n_high <= 13u && together >= 3u);
    /* Chord pads are left alone (strum repeat is theirs). */
    assert(held_ons(pad(0, 0, 3), 1000, 1000, &first, &last, &lo, &hi) == 3u);
    tc_state.note_repeat = 0u;
}
int main(void) {
    fold();
    lead();
    repeat();
    puts("PASS: 0.9.0: fold (F harmonic minor: F = Ab C F, C = C E G; rows raise the lowest note; every scale/key/octave/chord/edit inside G#..G, each pitch once, playable), voice leading (off unchanged; C F G = C-E-G, C-F-A, D-G-B; 200 chords stay in range; rows lead from their register; guitar ignores it), note repeat (off once; 1/8 on the clock's grid from any touch, then every 250 ms with a rest before each strike, louder with pressure; 1/32; 5 per beat; Euclidean 3/8; By row 2 against 3, together on the beat; chord pads untouched)");
    return 0;
}
