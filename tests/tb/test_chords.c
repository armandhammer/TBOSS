/* SPDX-License-Identifier: GPL-3.0-or-later - chord symbols: textbook spot checks, then every
 * chord pad x scale x root x edit must get a short, drawable name, and plain
 * diatonic triads/sevenths must get their exact symbol (no brackets). */
#include "../../src/tb/chord_name.h"
#include "../../src/tb/controls.h"
#include "../../src/tb/display.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
volatile ts_abi ts_status;
static void is(const char *want, uint8_t root, uint32_t n, const uint8_t *notes) {
    tp_notes t = {{0}, (uint8_t)n};
    memcpy(t.notes, notes, n);
    char out[TN_MAX];
    uint32_t len = tn_name(&t, root, out);
    if (strcmp(out, want)) { printf("got %s want %s\n", out, want); assert(0); }
    assert(len == strlen(out));
}
#define IS(want, root, ...) do { const uint8_t n_[] = {__VA_ARGS__}; is(want, root, sizeof n_, n_); } while (0)
static bool drawable(const char *s) {
    for (; *s; ++s) if (!strchr(td_font_chars, *s)) return false;
    return true;
}
int main(void) {
    IS("C", 0, 48, 52, 55); IS("C/E", 0, 52, 55, 60); IS("C/G", 0, 55, 60, 64);
    IS("Dm", 2, 50, 53, 57); IS("Bdim", 11, 47, 50, 53); IS("Caug", 0, 48, 52, 56);
    IS("Cmaj7", 0, 48, 52, 55, 59); IS("G7", 7, 43, 47, 50, 53); IS("Dm7", 2, 50, 53, 57, 60);
    IS("Bm7b5", 11, 47, 50, 53, 57); IS("Csus2", 0, 48, 50, 55); IS("Csus4", 0, 48, 53, 55);
    IS("C6", 0, 48, 52, 55, 57); IS("Cmaj9", 0, 48, 52, 55, 59, 62); IS("Cmaj13", 0, 48, 50, 52, 55, 57, 59);
    IS("G7sus4", 7, 43, 48, 50, 53); IS("Cadd9", 0, 48, 52, 55, 62); IS("F#m", 6, 54, 57, 61);
    /* Pentatonic stack C-E-A: an A minor chord over C. Chromatic C-D-E: C with a 9th. */
    IS("Am/C", 0, 48, 52, 57); IS("C(9)", 0, 48, 50, 52);
    /* A lowered root keeps the name; a lowered third becomes the bass. */
    IS("C", 0, 36, 52, 55); IS("C/E", 0, 40, 48, 55);
    tc_init(&tc_state);
    static const char *const major[7] = {"C", "Dm", "Em", "F", "G", "Am", "Bdim"};
    tp_layout layout = TP_DEFAULT_LAYOUT;
    char out[TN_MAX];
    for (uint32_t col = 0; col < 7u; ++col) { /* bottom row = root position */
        assert(tn_pad(layout, 4u * col + 3u, out) && !strcmp(out, major[col]));
    }
    tc_state.chord[3].add = TC_ADD7; assert(tn_pad(layout, 3, out) && !strcmp(out, "Cmaj7"));
    tc_state.chord[19].add = TC_ADD7; assert(tn_pad(layout, 19, out) && !strcmp(out, "G7"));
    tc_state.chord[3] = (tc_chord){0, 4, 0}; assert(tn_pad(layout, 3, out) && !strcmp(out, "Csus4"));
    assert(!tn_pad(layout, 51, out) && !out[0]);
    /* Every pad, built-in scale, root and edit: a name that fits and draws. */
    uint32_t names = 0, exact = 0;
    for (uint8_t mode = 0; mode < TC_SCALES; ++mode)
        for (uint8_t tonic = 0; tonic < 12u; ++tonic)
            for (uint32_t sus = 0; sus <= 4u; sus += 2u)
                for (uint8_t add = 0; add < 8u; ++add)
                    for (uint8_t lower = 0; lower < 8u; lower = (uint8_t)(lower ? (uint32_t)lower << 1 : 1u))
                        for (uint32_t k = 0; k < 48u; ++k) {
                            tp_layout l = {tonic, TP_MELODY_BASE, TP_CHORD_BASE, mode, TP_MELODY_SCALE};
                            tc_state.chord[k] = (tc_chord){lower, (uint8_t)sus, add};
                            uint32_t len = tn_pad(l, k, out);
                            assert(len && len <= 21u && drawable(out)); /* 21 x 6 px fit the 128 px screen */
                            names++;
                            if (mode <= 6u && !sus && !lower && (add == 0u || add == TC_ADD7)) {
                                assert(!strchr(out, '(')); exact++; /* diatonic triads and sevenths */
                            }
                        }
    printf("PASS: chord symbols: %u textbook cases, %u pad names drawable and short, %u diatonic triads/sevenths exact\n",
           22u, names, exact);
    return 0;
}
