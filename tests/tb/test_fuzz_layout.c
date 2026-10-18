/* SPDX-License-Identifier: GPL-3.0-or-later - layout sweep on the real layout/controls code.
 * Every scale (built-in, library and 12 random user scales) x key x fold x chord octave,
 * and every scale x key x right-half layout x melody octave. For each layout the board
 * would accept (tc_set_layout: every pad fits with the chord edits), every pad, with
 * seeded random panels (voicing, spread, octave stacks, capo), voice leading from random
 * earlier chords and random live chords, must give valid notes: tc_pad never fails (a
 * failure stops the engine's session mid-play), 0..16 notes, all 0..127. Cross-check
 * with an independent textbook model (scripts/check_notes.py's, there for the default
 * octaves only): the classic scales' melody pads and unshaped chord pads at every
 * octave offset, and the chromatic, fourths and drums layouts. Fixed seed. */
#include "../../src/tb/controls.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
volatile ts_abi ts_status;
static uint32_t seed = 0x1B873593u;
static uint32_t rnd(uint32_t n) {
    seed ^= seed << 13; seed ^= seed >> 17; seed ^= seed << 5;
    return n ? seed % n : 0u;
}
/* Textbook step patterns of the classic scales (ids 0..11), written out independently. */
static const char *const textbook[12] = {
    "2212221", "2122122", "2122212", "1222122", "2221221", "2212212",  /* major, minor, dorian, phrygian, lydian, mixolydian */
    "1221222", "2122131", "2122221", "22323", "32232", "111111111111"}; /* locrian, harm/mel minor, pentatonics, chromatic */
static uint32_t model_steps(uint32_t scale, uint32_t out[12]) {
    uint32_t n = 0, at = 0;
    for (const char *s = textbook[scale]; *s; ++s) { out[n++] = at; at += (uint32_t)(*s - '0'); }
    assert(at == 12u);
    return n;
}
static uint32_t model_note(uint32_t scale, uint32_t base, uint32_t degree) {
    uint32_t s[12], n = model_steps(scale, s);
    return base + 12u * (degree / n) + s[degree % n];
}
static uint32_t accepted_chords, accepted_melody, pads_checked, empty_results, lead_checked, model_checked;
static bool fits(tp_layout l) {
    if (!tc_layout_ok(l)) return false;
    tp_notes n;
    for (uint32_t k = 0; k < TS_KEYS; ++k) if (!tc_notes(l, k, tc_state.chord[k % 48u], &n)) return false;
    return true;
}
static void check_notes_valid(const tp_notes *n) {
    assert(n->count <= TP_MAX_NOTES);
    for (uint32_t i = 0; i < n->count; ++i) assert(n->notes[i] <= 127u);
    if (!n->count) empty_results++;
}
static void random_lead(tp_layout l) {
    tc_state.lead.count = 0u;
    if (rnd(2u)) { /* the core of another pad of this layout */
        (void)tc_pad_core(l, rnd(48u), &tc_state.lead);
    } else {       /* anything that came before (another key, octave, edits) */
        uint32_t n = 1u + rnd(6u);
        for (uint32_t i = 0; i < n; ++i) tc_state.lead.notes[tc_state.lead.count++] = (uint8_t)rnd(128u);
    }
}
static void chord_side(tp_layout l) {
    for (uint32_t k = 0; k < 48u; ++k) {
        for (uint32_t t = 0; t < 6u; ++t) {
            /* t 0: neutral; 1..3: random panel; 4, 5: voice leading on */
            tc_state.chord_voicing = t ? (uint8_t)rnd(TC_VOICINGS) : 0u;
            tc_state.chord_spread = t ? (uint8_t)rnd(TC_SPREADS) : 0u;
            tc_state.chord_octaves = t ? (uint8_t)(rnd(3u) ? 1u + rnd(127u) : TC_OCTAVES_DEFAULT) : TC_OCTAVES_DEFAULT;
            tc_state.capo = (uint8_t)rnd(TC_CAPOS);
            tc_state.voice_lead = t >= 4u;
            if (tc_state.voice_lead) { random_lead(l); lead_checked++; }
            tp_notes n, core;
            if (!tc_pad(l, k, &n)) {
                fprintf(stderr, "tc_pad failed: scale %u key %u fold %u chord_base %u pad %u voicing %u spread %u "
                        "octaves %u lead %u\n", l.mode, l.tonic, l.fold, l.chord_base, k, tc_state.chord_voicing,
                        tc_state.chord_spread, tc_state.chord_octaves, tc_state.voice_lead);
                assert(false);
            }
            check_notes_valid(&n);
            assert(tc_pad_core(l, k, &core)); check_notes_valid(&core);
            pads_checked++;
        }
        tc_state.voice_lead = 0u;
    }
    /* Unshaped chord pads of the classic scales (no edits, no fold): the triad on the
     * column's step, rows from the bottom root position, 1st, 2nd inversion, an octave up. */
    if (l.mode >= TC_SCALES || l.fold) return;
    for (uint32_t k = 0; k < 48u; ++k) {
        if (tc_state.chord[k].lower || tc_state.chord[k].sus || tc_state.chord[k].add) continue;
        uint32_t col = k / 4u, row = k % 4u, step = 3u - row, base = l.chord_base + l.tonic;
        uint32_t t[3] = {model_note(l.mode, base, col), model_note(l.mode, base, col + 2u), model_note(l.mode, base, col + 4u)};
        for (uint32_t i = 0; i < step; ++i) { uint32_t low = t[0]; t[0] = t[1]; t[1] = t[2]; t[2] = low + 12u; }
        tp_notes n; const tc_chord none = {0, 0, 0};
        assert(tc_notes(l, k, none, &n) && n.count == 3u);
        for (uint32_t i = 0; i < 3u; ++i) assert(n.notes[i] == t[i]);
        model_checked++;
    }
}
static void melody_side(tp_layout l) {
    for (uint32_t k = 48u; k < TS_KEYS; ++k) {
        tp_notes n;
        assert(tc_pad(l, k, &n) && n.count == 1u && n.notes[0] <= 127u);
        pads_checked++;
        uint32_t col = (k - 48u) / 4u, row = k % 4u, up = 3u - row, base = l.melody_base + l.tonic, want = 0u;
        bool model = true;
        switch (l.melody_layout) {
        case TP_MELODY_SCALE: model = l.mode < TC_SCALES; if (model) want = model_note(l.mode, base + 12u * up, col); break;
        case TP_MELODY_CHROMATIC: want = base + col + 12u * up; break;
        case TP_MELODY_FOURTHS: want = base + col + 5u * up; break;
        case TP_MELODY_DRUMS: {
            uint32_t grid = col / 4u, d = 4u * up + col % 4u;
            model = grid == 2u || l.mode < TC_SCALES;
            if (grid == 2u) want = TP_DRUM_BASE + d;
            else if (model) want = model_note(l.mode, base, d) - (grid == 1u ? 12u : 0u);
            break;
        }
        default: /* live notes: scale columns, then tones of the live chord, all in range */
            model = col < TP_LIVE_COLUMN && l.mode < TC_SCALES;
            if (model) want = model_note(l.mode, base + 12u * up, col);
            else if (col >= TP_LIVE_COLUMN) {
                tc_live_chord live; tc_live_current(l, &live);
                bool tone = false;
                for (uint32_t i = 0; i < live.count; ++i) tone = tone || (n.notes[0] + 120u - live.root) % 12u == live.tones[i];
                assert(tone);
            }
            break;
        }
        if (model) { assert(n.notes[0] == want); model_checked++; }
        if (l.melody_layout == TP_MELODY_SCALE) assert(tc_in_scale(l.mode, (n.notes[0] + 120u - l.tonic) % 12u));
    }
}
static void random_live_chord(void) {
    tc_state.live_pad = (uint8_t)rnd(48u);
    tc_state.live_from_midi = rnd(2u) != 0u;
    uint8_t held[128] = {0};
    for (uint32_t i = 0, n = 2u + rnd(5u); i < n; ++i) held[rnd(128u)] = 1u;
    if (!tc_live_of_notes(held, &tc_state.live_midi)) tc_state.live_from_midi = false;
}
int main(void) {
    tc_init(&tc_state);
    /* User scales: 5 notes (the fewest allowed) to all 12, root always in. */
    for (uint8_t u = 0; u < TC_USER_SCALES; ++u) {
        uint16_t mask;
        do mask = (uint16_t)(1u | rnd(4096u)); while (!tc_user_mask_ok(mask));
        assert(tc_set_user_scale(u, u == 0u ? 0x0295u /* 5 notes */ : mask, "FUZZ"));
    }
    for (uint32_t scale = 0; scale < TC_ALL_SCALES; ++scale) {
        assert(tc_scale_defined((uint8_t)scale));
        for (uint32_t tonic = 0; tonic < 12u; ++tonic) {
            /* Chord edits for half the layouts, kept only where they fit (as edit_chord). */
            for (uint32_t k = 0; k < 48u; ++k) tc_state.chord[k] = (tc_chord){0, 0, 0};
            for (uint32_t fold = 0; fold < 2u; ++fold)
                for (uint32_t o = 0; o < 5u; ++o) {
                    tp_layout l = {(uint8_t)tonic, TP_MELODY_BASE, (uint8_t)(TP_CHORD_BASE - TP_OCTAVE_RANGE + 12u * o),
                                   (uint8_t)scale, TP_MELODY_SCALE, (uint8_t)fold};
                    for (uint32_t k = 0; k < 48u; ++k) {
                        tc_chord edit = {0, 0, 0};
                        if (rnd(2u)) edit = (tc_chord){(uint8_t)rnd(8u), (uint8_t)(2u * rnd(3u)), (uint8_t)rnd(8u)};
                        tp_notes n;
                        tc_state.chord[k] = tc_notes(l, k, edit, &n) ? edit : (tc_chord){0, 0, 0};
                    }
                    if (!fits(l)) continue;
                    accepted_chords++;
                    chord_side(l);
                }
            for (uint32_t k = 0; k < 48u; ++k) tc_state.chord[k] = (tc_chord){0, 0, 0};
            for (uint32_t layout = 0; layout < TP_MELODY_LAYOUTS; ++layout)
                for (uint32_t o = 0; o < 5u; ++o) {
                    tp_layout l = {(uint8_t)tonic, (uint8_t)(TP_MELODY_BASE - TP_OCTAVE_RANGE + 12u * o), TP_CHORD_BASE,
                                   (uint8_t)scale, (uint8_t)layout, 0u};
                    if (!fits(l)) continue;
                    accepted_melody++;
                    random_live_chord();
                    melody_side(l);
                }
        }
    }
    /* The default layout is always accepted; extreme octaves sometimes are not. */
    assert(accepted_chords >= TC_ALL_SCALES * 12u && accepted_melody >= TC_ALL_SCALES * 12u * TP_MELODY_LAYOUTS);
    printf("PASS: layout sweep: %u scales x 12 keys; %u accepted chord layouts (fold, octaves, random edits) and %u "
           "melody layouts (5 layouts, octaves, random live chords); %u pad results valid with random panels, "
           "%u with voice leading; %u empty (octave copies all outside MIDI); %u pads equal the textbook model\n",
           (unsigned)TC_ALL_SCALES, accepted_chords, accepted_melody, pads_checked, lead_checked, empty_results,
           model_checked);
    return 0;
}
