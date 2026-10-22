/* SPDX-License-Identifier: GPL-3.0-or-later - 1.3.0 Full chord layout against a fake external
 * memory chip in the original firmware's record format (two scales: major, and minor pentatonic
 * numbered by Aeolian): the records' chords and voicings on the pads (an octave lower for C2),
 * roots from the run's scale step, a typing slip in one key played as the other keys have it,
 * one copied to five neighboring keys that leaves the scale likewise,
 * records in the wrong key or a missing chip refused, built layouts for every other scale (48
 * chords, scale notes only, runs in scale order), the panel's voicings, and the cache. */
#include "../../src/tb/controls.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
volatile ts_abi ts_status;
static const uint8_t ionian[7] = {0, 2, 4, 5, 7, 9, 11}, aeolian[7] = {0, 2, 3, 5, 7, 8, 10};
static const uint8_t slot_key[12] = {9, 8, 11, 10, 0, 2, 1, 4, 3, 5, 7, 6}; /* the chip's key order */
#define MAJOR_RECORD 21u     /* major modes, Ionian */
#define MIN_PENT_RECORD 60u  /* minor pentatonic, chords numbered by Aeolian */
static uint32_t reads, wrong_key;
static bool slip = true;
/* The fake's chord for entry i in key k: step i*7/48 + 1, the parent mode's triad there, a 7th
 * on odd entries; voiced in G#3..G4 with every third one's lowest note an octave up. */
static uint32_t fake_chord(const uint8_t *parent, uint32_t key, uint32_t i, uint8_t notes[6], uint32_t *step) {
    uint32_t s = i * 7u / 48u, n = 0;
    *step = s + 1u;
    for (uint32_t m = 0; m < (i % 2u ? 4u : 3u); ++m) {
        uint32_t d = s + 2u * m, pc = (key + parent[d % 7u]) % 12u;
        notes[n++] = (uint8_t)(56u + (pc + 4u) % 12u);
    }
    for (uint32_t a = 1; a < n; ++a) for (uint32_t b = a; b && notes[b - 1u] > notes[b]; --b) { uint8_t x = notes[b]; notes[b] = notes[b - 1u]; notes[b - 1u] = x; }
    if (i % 3u == 0u) {
        notes[0] = (uint8_t)(notes[0] + 12u);
        for (uint32_t a = 1; a < n; ++a) for (uint32_t b = a; b && notes[b - 1u] > notes[b]; --b) { uint8_t x = notes[b]; notes[b] = notes[b - 1u]; notes[b - 1u] = x; }
    }
    return n;
}
static void entry_bytes(uint32_t record, uint32_t key, uint32_t i, uint8_t out[32]) {
    memset(out, 0xFF, 32);
    const uint8_t *parent = record == MAJOR_RECORD ? ionian : record == MIN_PENT_RECORD ? aeolian : NULL;
    if (!parent || i > 48u) return;
    uint8_t notes[6]; uint32_t step, n;
    if (i == 48u) { notes[0] = (uint8_t)(60u + (key + (wrong_key ? 1u : 0u)) % 12u); n = 1; step = 1; } /* right half: the root */
    else n = fake_chord(parent, key, i, notes, &step);
    if (slip && record == MAJOR_RECORD && key == 2u && i == 5u) notes[n - 1u]++; /* a typing slip in D */
    if (slip && record == MAJOR_RECORD && key >= 4u && key <= 8u && i == 2u) notes[n - 1u]++; /* copied E..G#: a #5 */
    char text[17];
    for (uint32_t m = 0; m < 7u; ++m) { uint32_t v = m < n ? notes[m] : 0u; text[2 * m] = (char)('0' + v / 10u); text[2 * m + 1] = (char)('0' + v % 10u); }
    text[14] = (char)('0' + step / 10u); text[15] = (char)('0' + step % 10u);
    memcpy(out, text, 16); memcpy(out + 16, "CHORD", 6);
}
static bool chip(uint32_t address, uint8_t *out, uint32_t len) {
    reads++;
    for (uint32_t j = 0; j < len; ++j) {
        uint32_t a = address + j, off = a - 0x70000u;
        if (a < 0x70000u) { out[j] = 0xFF; continue; }
        uint8_t e[32];
        entry_bytes(off / 30720u, slot_key[(off % 30720u) / 2560u], (off % 2560u) / 32u, e);
        out[j] = e[off % 32u];
    }
    return true;
}
static tp_layout full(uint8_t scale, uint8_t key) {
    tp_layout l = TP_DEFAULT_LAYOUT; l.mode = scale; l.tonic = key; l.chords = (uint8_t)TC_CHORDS_FULL;
    return l;
}
static const tc_chord none = {0, 0, 0};
static uint32_t classes(const tp_notes *n) {
    uint32_t m = 0;
    for (uint32_t i = 0; i < n->count; ++i) m |= 1u << (n->notes[i] % 12u);
    return m;
}
/* The record's chords, an octave lower (chord base C2), and their roots. */
static void records_play(const uint8_t *parent, uint8_t scale, uint8_t key) {
    tp_layout l = full(scale, key);
    assert(tc_layout_ok(l) && tc_full_original(l));
    for (uint32_t k = 0; k < 48u; ++k) {
        uint8_t want[6]; uint32_t step, n = fake_chord(parent, key, k, want, &step);
        tp_notes got;
        assert(tc_notes(l, k, none, &got) && got.count == n);
        if (!(slip && parent == ionian && ((key == 2u && k == 5u) || (key >= 4u && key <= 8u && k == 2u))))
            for (uint32_t i = 0; i < n; ++i) assert(got.notes[i] == want[i] - 12u);
        assert(tc_pad_root(l, k) == (key + parent[step - 1u]) % 12u && !tc_pad_empty(l, k));
    }
}
int main(void) {
    tc_init(&tc_state);
    /* No chip: every layout is built. */
    tc_full_reader(NULL);
    assert(!tc_full_original(full(0, 0)));
    tc_full_reader(chip);
    /* The records, in every key; about two reads a pad (one per key compared) and the root check. */
    for (uint8_t key = 0; key < 12u; ++key) {
        reads = 0; records_play(ionian, 0, key);
        assert(reads <= 48u * 2u + 1u + 12u); /* the slips cost the keys near them a few more */
        records_play(aeolian, 10, key);
    }
    /* The slip in D: pad 5 plays the chord the other keys have there, voiced as built. */
    {
        tp_notes d, c; uint8_t want[6]; uint32_t step;
        assert(tc_notes(full(0, 2), 5, none, &d) && tc_notes(full(0, 0), 5, none, &c));
        uint32_t n = fake_chord(ionian, 2, 5, want, &step), cd = 0;
        for (uint32_t i = 0; i < n; ++i) cd |= 1u << (want[i] % 12u);
        assert(classes(&d) == cd && d.count == n);
        for (uint32_t i = 0; i < d.count; ++i) assert(d.notes[i] >= 44u && d.notes[i] <= 55u + 12u);
        tc_full_reader(chip); slip = false;
        tp_notes clean; assert(tc_notes(full(0, 2), 5, none, &clean) && classes(&clean) == cd);
        slip = true; tc_full_reader(chip);
        /* The copied slip: E..G# play pad 2 as the other keys do (C's, transposed). */
        tp_notes c2; assert(tc_notes(full(0, 0), 2, none, &c2));
        for (uint8_t key = 4; key <= 8u; ++key) {
            tp_notes k2; assert(tc_notes(full(0, key), 2, none, &k2));
            uint32_t moved = ((classes(&c2) << key) | (classes(&c2) >> (12u - key))) & 0xFFFu;
            assert(classes(&k2) == moved);
        }
    }
    /* Records in the wrong key (the right half's root disagrees): refused, built instead. */
    wrong_key = 1; tc_full_reader(chip);
    assert(!tc_full_original(full(0, 0)));
    wrong_key = 0; tc_full_reader(chip);
    assert(tc_full_original(full(0, 0)));
    /* Every scale and key: a layout of 48 chords (records or built) that fits; built ones use
     * the scale's notes only, in runs by scale step. */
    uint32_t built = 0;
    for (uint8_t scale = 0; scale < TC_ALL_SCALES; ++scale) {
        if (!tc_scale_defined(scale)) continue;
        for (uint8_t key = 0; key < 12u; ++key) {
            tp_layout l = full(scale, key);
            assert(tc_layout_ok(l));
            bool original = tc_full_original(l);
            uint32_t last_root = 0, period = 48u, chord[48];
            for (uint32_t k = 0; k < 48u; ++k) {
                tp_notes n;
                assert(tc_notes(l, k, none, &n) && n.count >= 2u && !tc_pad_empty(l, k));
                if (original) continue;
                chord[k] = classes(&n);
                for (uint32_t i = 0; i < n.count; ++i) assert(n.notes[i] >= 44u && n.notes[i] < 44u + 36u);
                uint32_t root = (tc_pad_root(l, k) + 12u - key) % 12u;
                if (period == 48u && root < last_root) period = k; /* the chords again */
                if (period < 48u) assert(chord[k] == chord[k % period]);
                last_root = root;
            }
            /* Built on the scale's own notes from 7 notes up (5 and 6 may take a parent mode's). */
            for (uint32_t k = 0; !original && tc_scale_size(scale) >= 7u && k < 48u; ++k)
                for (uint32_t b = 0; b < 12u; ++b) if ((chord[k] >> ((b + key) % 12u)) & 1u) assert(tc_in_scale(scale, b));
            built += !original;
        }
    }
    /* The major scale built (no chip): the runs start on each step's triad, C first. */
    tc_full_reader(NULL);
    {
        tp_layout l = full(0, 0); tp_notes n;
        assert(tc_notes(l, 0, none, &n) && classes(&n) == 0x091u && tc_pad_root(l, 0) == 0u);
        uint32_t steps_seen = 0;
        for (uint32_t k = 0; k < 48u; ++k) steps_seen |= 1u << tc_pad_root(l, k);
        assert(steps_seen == 0xAB5u);
    }
    tc_full_reader(chip);
    /* The panel: voicing I raises the lowest note an octave; spread and octaves as anywhere. */
    {
        tp_layout l = full(0, 0); tp_notes plain, up;
        tc_panel one = TC_PANEL_NEUTRAL; one.voicing = 1u;
        assert(tc_notes(l, 1, none, &plain) && tc_notes_panel(l, 1, none, one, &up));
        assert(up.count == plain.count && up.notes[up.count - 1u] == plain.notes[0] + 12u);
        tp_notes prev = plain;
        int32_t step = tc_lead_step(l, 7, none, 0u, &prev);
        assert(step >= -2 && step <= 2);
    }
    /* The cache: six layouts in turn give the same chords each time. */
    {
        tp_notes first[6], again;
        const uint8_t scales[6] = {0, 10, 2, 7, 26, 11};
        for (uint32_t i = 0; i < 6u; ++i) assert(tc_notes(full(scales[i], (uint8_t)i), 9, none, &first[i]));
        for (uint32_t round = 0; round < 3u; ++round)
            for (uint32_t i = 0; i < 6u; ++i) {
                assert(tc_notes(full(scales[i], (uint8_t)i), 9, none, &again));
                assert(!memcmp(&again, &first[i], sizeof again));
            }
    }
    /* Settings: Full is a chords value (the chord grid after it, then nothing). */
    assert(tc_chords_ok((uint8_t)TC_CHORDS_FULL) && !tc_chords_ok((uint8_t)(TC_CHORDS_GRID + 1u)));
    printf("PASS: full layout: records in 12 keys (major, minor pentatonic by Aeolian) on the pads an octave lower, "
           "roots by run, a slip in D played as the other keys have it, the wrong key or no chip refused; "
           "%u built layouts (every other scale and key) in scale notes and step order; voicing, lead, cache\n", built);
    return 0;
}
