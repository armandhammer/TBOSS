/* SPDX-License-Identifier: GPL-3.0-or-later - 1.3.0 chord grid: columns are scale steps, rows
 * chord types (bottom up: triad, seventh, sus, ninth), every chord in root position. C major
 * pad by pad, then every scale and key against the scale's own chords with the same edits;
 * the sus row's choice, voicing, fold, hints, held shapes and the live chord. */
#include "../../src/tb/controls.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
volatile ts_abi ts_status;
static const tc_chord none = {0, 0, 0};
static uint32_t pad(uint32_t column, uint32_t row) { return 4u * column + row; }
static tp_layout grid(uint8_t scale, uint8_t key) {
    tp_layout l = TP_DEFAULT_LAYOUT; l.mode = scale; l.tonic = key; l.chords = (uint8_t)TC_CHORDS_GRID;
    return l;
}
static void notes_are(tp_layout l, uint32_t k, tc_chord edit, const uint8_t *want, uint32_t n) {
    tp_notes got;
    assert(tc_notes(l, k, edit, &got) && got.count == n);
    for (uint32_t i = 0; i < n; ++i) assert(got.notes[i] == want[i]);
}
static uint32_t classes(const tp_notes *n) {
    uint32_t m = 0;
    for (uint32_t i = 0; i < n->count; ++i) m |= 1u << (n->notes[i] % 12u);
    return m;
}
/* The sus the grid picks on scale step d: sus4 unless the 4th above isn't perfect and the 2nd
 * above is a whole step. */
static uint8_t sus_on(uint8_t scale, uint32_t d) {
    uint32_t n = tc_scale_size(scale);
    uint32_t root = tc_step_semitones(scale, d) + 12u * (d / n);
    uint32_t second = tc_step_semitones(scale, d + 1u) + 12u * ((d + 1u) / n);
    uint32_t fourth = tc_step_semitones(scale, d + 3u) + 12u * ((d + 3u) / n);
    return fourth - root != 5u && second - root == 2u ? 2u : 4u;
}
int main(void) {
    tc_init(&tc_state);
    assert(tc_chords_ok((uint8_t)TC_CHORDS_GRID) && !tc_chords_ok((uint8_t)(TC_CHORDS_GRID + 1u)));
    /* C major, chords from C2: C, C7 (Cmaj7), Csus4, C9 (Cmaj9) up the first column. */
    tp_layout c = grid(0, 0);
    assert(tc_layout_ok(c));
    static const uint8_t triad[3] = {36, 40, 43}, seventh[4] = {36, 40, 43, 47}, sus4[3] = {36, 41, 43},
                         ninth[5] = {36, 40, 43, 47, 50};
    notes_are(c, pad(0, 3), none, triad, 3);
    notes_are(c, pad(0, 2), none, seventh, 4);
    notes_are(c, pad(0, 1), none, sus4, 3);
    notes_are(c, pad(0, 0), none, ninth, 5);
    /* G: G, G7, Gsus4, G9. */
    static const uint8_t g[3] = {43, 47, 50}, g7[4] = {43, 47, 50, 53}, gsus[3] = {43, 48, 50}, g9[5] = {43, 47, 50, 53, 57};
    notes_are(c, pad(4, 3), none, g, 3); notes_are(c, pad(4, 2), none, g7, 4);
    notes_are(c, pad(4, 1), none, gsus, 3); notes_are(c, pad(4, 0), none, g9, 5);
    /* The sus row: F has no perfect fourth in C major (B), so Fsus2; B keeps its sus4 (E). */
    static const uint8_t fsus2[3] = {41, 43, 48}, bsus4[3] = {47, 52, 53};
    notes_are(c, pad(3, 1), none, fsus2, 3);
    notes_are(c, pad(6, 1), none, bsus4, 3);
    /* Column 7 is C again an octave up; every row's root is its column's scale step. */
    static const uint8_t c_up[3] = {48, 52, 55};
    notes_are(c, pad(7, 3), none, c_up, 3);
    for (uint32_t k = 0; k < 48u; ++k) assert(tc_pad_root(c, k) == tc_step_semitones(0, k / 4u) % 12u && !tc_pad_empty(c, k));
    /* Every scale and key: the bottom row is the scale's own chords, the others the same chord
     * with a 7th, a sus, a 7th and 9th; root position, scale notes, the column's root. */
    uint32_t checked = 0;
    for (uint8_t scale = 0; scale < TC_ALL_SCALES; ++scale) {
        if (!tc_scale_defined(scale)) continue;
        for (uint8_t key = 0; key < 12u; ++key) {
            tp_layout l = grid(scale, key), s = l;
            s.chords = 0u;
            for (uint32_t column = 0; column < 12u; ++column) {
                const tc_chord as_row[4] = {{0, 0, TC_ADD7 | TC_ADD9}, {0, sus_on(scale, column), 0}, {0, 0, TC_ADD7}, {0, 0, 0}};
                for (uint32_t row = 0; row < 4u; ++row) {
                    tp_notes got, want;
                    assert(tc_notes(l, pad(column, row), none, &got));
                    assert(tc_notes(s, pad(column, 3), as_row[row], &want));
                    assert(got.count == want.count && !memcmp(got.notes, want.notes, got.count));
                    assert(got.count == (row == 0u ? 5u : row == 2u ? 4u : 3u));
                    assert(got.notes[0] % 12u == tc_pad_root(l, pad(column, row)));
                    for (uint32_t i = 0; i < got.count; ++i) assert(tc_in_scale(scale, (got.notes[i] + 12u - key) % 12u));
                    ++checked;
                }
            }
        }
    }
    /* Voicing I inverts the row's chord (C: E G C); voice leading stays near it. */
    {
        tc_panel one = TC_PANEL_NEUTRAL; one.voicing = 1u;
        tp_notes inv; assert(tc_notes_panel(c, pad(0, 3), none, one, &inv));
        assert(inv.count == 3u && inv.notes[0] == 40u && inv.notes[1] == 43u && inv.notes[2] == 48u);
        tp_notes prev; assert(tc_notes(c, pad(0, 3), none, &prev));
        int32_t step = tc_lead_step(c, pad(4, 2), none, 0u, &prev);
        assert(step >= -2 && step <= 2);
    }
    /* Fold: the same chords inside one octave. */
    {
        tp_layout f = c; f.fold = 1u;
        for (uint32_t k = 0; k < 48u; ++k) {
            tp_notes a, b; assert(tc_notes(c, k, none, &a) && tc_notes(f, k, none, &b));
            assert(classes(&a) == classes(&b));
        }
    }
    /* Hints go by scale step, as in the scale's chords: after I, every IV pad (columns 3, 10). */
    {
        uint64_t also, strong = tc_hints(c, pad(0, 0), &also), iv = 0;
        for (uint32_t row = 0; row < 4u; ++row) iv |= (uint64_t)1u << pad(3, row) | (uint64_t)1u << pad(10, row);
        assert(strong == iv && also);
    }
    /* Held shapes add to the row's chord; a held sus replaces the row's (Fsus4 with its B). */
    {
        static const uint8_t cadd9[4] = {36, 40, 43, 50}, fsus4[3] = {41, 47, 48};
        const tc_chord add9 = {0, 0, TC_ADD9}, held_sus4 = {0, 4, 0};
        notes_are(c, pad(0, 3), add9, cadd9, 4);
        notes_are(c, pad(0, 0), add9, ninth, 5);
        notes_are(c, pad(3, 1), held_sus4, fsus4, 3);
    }
    /* The live chord of a grid pad: its row's tones (G7: G B D F). */
    {
        tc_live_chord live; tc_live_of_pad(c, pad(4, 2), none, &live);
        assert(live.root == 7u && live.count == 4u && live.tones[1] == 4u && live.tones[3] == 10u);
    }
    printf("PASS: chord grid: C major pad by pad (triad, 7th, sus, 9th; Fsus2, Bsus4), %u pads in every scale and key "
           "against the scale's chords with the row's edits, voicing, fold, hints by step, held shapes, live chord\n", checked);
    return 0;
}
