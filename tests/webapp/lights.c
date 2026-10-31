/* SPDX-License-Identifier: GPL-3.0-or-later - the firmware's pad colors (ui_lights.c ui_pad_colors, 1.9.0 looks)
 * behind stdin/stdout for tests/webapp/test_protocol.mjs, against webapp/tb-lights.js. Each input line:
 * "palette" (the 16 palette colors, then the 36 roles' built-in ones), or "tonic scale layout scheme colorby
 * r0 .. r35" (decimal): the 96 pads at rest, at brightness 8, as rrggbb by pad number (not where they light);
 * "feel b0 .. b39": the curves (1.9.0); "rotate length by s0 .. s63" (2.0.0): a lane's steps (value * 2 + on) moved by
 * lanes.c ln_rotate, the same back; "mutate kind length lane seed pct salt s0 .. s63" (2.0.3): the steps (value |
 * velocity << 7 | on << 14) after lanes.c ln_mutate, the same back; "pads tonic scale chords voicing spread spreadof
 * octaves capo melodylayout melodybase chordbase [mask root] x 12" (the manual's pad explorer, webapp/tb-pads.js): every
 * pad's notes through layout.c tc_notes_panel ("x": not playable) and, on the chord pads, the screen's name (chord_name.c
 * tn_name), "|" between pads; chords 0 the scale's, 1 the 12 (mask root) pairs as a chord set, 9 Full, 10 the grid;
 * "fullpads tonic scale": the Full layout's 48 chord pads as root:notes at chord base 48, then O (the original's, from
 * the chip) or B (built). With TB_NOR_FILE set to an image of the external memory, Full reads its records from it. */
#include "../../src/tb/settings.h"
#include "../../src/tb/flash_store.h"
#include "../../src/tb/nor.h"
#include "../../src/tb/ui_internal.h"
#include "../../src/tb/chord_name.h"
#include <stdlib.h>
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
static uint8_t *chip; static uint32_t chip_size; /* TB_NOR_FILE: the external memory's image */
static bool chip_read(uint32_t a, uint8_t *out, uint32_t n) {
    if (!chip || a > chip_size || n > chip_size - a) return false;
    memcpy(out, chip + a, n); return true;
}
static void notes_out(const tp_notes *t) { for (uint32_t i = 0; i < t->count; ++i) printf(i ? ",%u" : "%u", t->notes[i]); }
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
void td_clear(void) {}
uint32_t td_text(uint32_t x, uint32_t y, const char *text, uint32_t scale) { (void)y; return x + 6u * scale * (uint32_t)strlen(text); }
static void rgb(tl_rgb c) { printf("%02x%02x%02x ", c.r, c.g, c.b); }
int main(void) {
    static tp_engine engine;
    static char line[1024];
    memset(flash, 0xFF, sizeof flash);
    const char *image = getenv("TB_NOR_FILE");
    if (image) {
        FILE *f = fopen(image, "rb");
        if (!f) return 2;
        fseek(f, 0, SEEK_END); chip_size = (uint32_t)ftell(f); fseek(f, 0, SEEK_SET);
        chip = malloc(chip_size);
        if (!chip || fread(chip, 1, chip_size, f) != chip_size) return 2;
        fclose(f); tc_full_reader(chip_read);
    }
    while (fgets(line, sizeof line, stdin)) {
        if (!strncmp(line, "fullpads", 8)) {
            unsigned tonic, scale;
            if (sscanf(line + 8, "%u %u", &tonic, &scale) != 2) return 1;
            tp_layout c = {(uint8_t)tonic, TP_MELODY_BASE, 48u, (uint8_t)scale, TP_MELODY_SCALE, 0u, TC_CHORDS_FULL};
            tc_full_prepare(c);
            for (uint32_t k = 0; k < 48u; ++k) {
                tp_notes t; const tc_chord none = {0, 0, 0};
                if (!tc_notes(c, k, none, &t)) return 1;
                printf("%u:", tc_pad_root(c, k)); notes_out(&t); printf("|");
            }
            printf("%s\n", tc_full_original(c) ? "O" : "B"); continue;
        }
        if (!strncmp(line, "pads", 4)) {
            unsigned v[11 + 24] = {0}; uint32_t n = 0; char *q = line + 4; int at;
            while (n < 11u + 24u && sscanf(q, "%u%n", &v[n], &at) == 1) { q += at; n++; }
            if (n < 11u) return 1;
            if (v[2] == 1u) {
                tc_chord_set s; memset(&s, 0, sizeof s);
                for (uint32_t i = 0; i < 12u; ++i) { s.column[i].mask = (uint16_t)v[11u + 2u * i]; s.column[i].root = (uint8_t)v[12u + 2u * i]; }
                if (!tc_chord_set_put(0u, &s)) return 1;
            }
            tp_layout c = {(uint8_t)v[0], (uint8_t)v[9], (uint8_t)v[10], (uint8_t)v[1], (uint8_t)v[8], 0u, (uint8_t)v[2]};
            tc_panel panel = {(uint8_t)v[3], (uint8_t)v[4], (uint8_t)v[6], (uint8_t)v[7], 0u, (uint8_t)v[5]};
            tc_full_prepare(c);
            for (uint32_t k = 0; k < 96u; ++k) {
                tp_notes t; const tc_chord none = {0, 0, 0}; char name[TN_MAX];
                if (!tc_notes_panel(c, k, none, panel, &t)) { printf("x|"); continue; }
                notes_out(&t);
                if (k < 48u) { if (t.count) (void)tn_name(&t, tc_pad_root(c, k), name); else strcpy(name, "-"); printf(";%s", name); }
                printf("|");
            }
            printf("\n"); continue;
        }
        if (!strncmp(line, "palette", 7)) {
            for (uint32_t i = 0; i < TC_PALETTE; ++i) rgb(ui_palette[i]);
            for (uint32_t i = 0; i < TC_LOOK_ROLES; ++i) rgb(ui_look_builtin[i]);
            printf("\n"); continue;
        }
        if (!strncmp(line, "feel", 4)) { /* 1.9.0: "feel b0 .. b39": the two velocity curves at 0..127, then the
                                              * pressure out of each half for 0..127 (controls.c) */
            uint8_t feel[TC_FEEL_BYTES]; char *q = line + 4; int at; unsigned b;
            for (uint32_t i = 0; i < TC_FEEL_BYTES && sscanf(q, "%u%n", &b, &at) == 1; ++i) { feel[i] = (uint8_t)b; q += at; }
            for (uint32_t half = 0; half < 2u; ++half) for (uint32_t x = 0; x <= 127u; ++x) printf("%u ", tc_feel_curve(&feel[TC_FEEL_VEL(half)], x));
            for (uint32_t half = 0; half < 2u; ++half) for (uint32_t x = 0; x <= 127u; ++x) printf("%u ", tc_feel_pressure(feel, half != 0u, (uint8_t)x));
            printf("\n"); continue;
        }
        if (!strncmp(line, "rotate", 6)) {
            static ln_lane l; char *q = line + 6; int at; int value; unsigned length;
            memset(&l, 0, sizeof l);
            if (sscanf(q, "%u%n", &length, &at) != 1) return 1;
            q += at; ln_set_length(&l, length);
            if (sscanf(q, "%d%n", &value, &at) != 1) return 1;
            q += at;
            for (uint32_t i = 0; i < LN_STEPS; ++i) { unsigned s; if (sscanf(q, "%u%n", &s, &at) != 1) return 1; q += at; l.steps[i].on = s & 1u; l.steps[i].value = (uint8_t)(s >> 1); }
            ln_rotate(&l, value);
            for (uint32_t i = 0; i < LN_STEPS; ++i) printf("%u ", (unsigned)l.steps[i].value * 2u + (l.steps[i].on ? 1u : 0u));
            printf("\n"); continue;
        }
        if (!strncmp(line, "mutate", 6)) {
            static ln_lane l; char *q = line + 6; int at; unsigned kind, length, lane, seed, pct, salt;
            memset(&l, 0, sizeof l);
            if (sscanf(q, "%u %u %u %u %u %u%n", &kind, &length, &lane, &seed, &pct, &salt, &at) != 6) return 1;
            q += at; l.kind = (uint8_t)kind; ln_set_length(&l, length);
            for (uint32_t i = 0; i < LN_STEPS; ++i) {
                unsigned s; if (sscanf(q, "%u%n", &s, &at) != 1) return 1;
                q += at; l.steps[i].value = (uint8_t)(s & 127u); l.steps[i].velocity = (uint8_t)((s >> 7) & 127u); l.steps[i].on = (s >> 14) & 1u;
            }
            ln_mutate(&l, lane, seed, pct, salt);
            for (uint32_t i = 0; i < LN_STEPS; ++i)
                printf("%u ", (unsigned)l.steps[i].value | (unsigned)l.steps[i].velocity << 7 | (l.steps[i].on ? 1u : 0u) << 14);
            printf("\n"); continue;
        }
        unsigned v[5 + TC_LOOK_ROLES]; uint32_t n = 0; char *p = line; int used;
        while (n < 5u + TC_LOOK_ROLES && sscanf(p, "%u%n", &v[n], &used) == 1) { p += used; n++; }
        tc_init(&tc_state); memset(&engine, 0, sizeof engine);
        tc_state.brightness = 7u; tc_state.tonic = (uint8_t)v[0]; tc_state.scale = (uint8_t)v[1]; tc_state.melody_layout = (uint8_t)v[2];
        tc_state.led_scheme = (uint8_t)v[3]; tc_state.color_by = (uint8_t)v[4];
        for (uint32_t i = 0; i < TC_LOOK_ROLES; ++i) tc_state.look_roles[i] = (uint8_t)v[5u + i];
        engine.layout = tc_layout(&tc_state);
        ui_pad_colors(&tc_state, &engine, 1000u);
        for (uint32_t k = 0; k < TL_PADS; ++k) rgb(pads[tc_physical(&tc_state, k)]);
        printf("\n");
    }
    return 0;
}
