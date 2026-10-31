/* SPDX-License-Identifier: GPL-3.0-or-later - the firmware's pad colors (ui_lights.c ui_pad_colors, 1.9.0 looks)
 * behind stdin/stdout for tests/webapp/test_protocol.mjs, against webapp/tb-lights.js. Each input line:
 * "palette" (the 16 palette colors, then the 36 roles' built-in ones), or "tonic scale layout scheme colorby
 * r0 .. r35" (decimal): the 96 pads at rest, at brightness 8, as rrggbb by pad number (not where they light);
 * "feel b0 .. b39": the curves (1.9.0); "rotate length by s0 .. s63" (2.0.0): a lane's steps (value * 2 + on) moved by
 * lanes.c ln_rotate, the same back; "mutate kind length lane seed pct salt s0 .. s63" (2.0.3): the steps (value |
 * velocity << 7 | on << 14) after lanes.c ln_mutate, the same back. */
#include "../../src/tb/settings.h"
#include "../../src/tb/flash_store.h"
#include "../../src/tb/nor.h"
#include "../../src/tb/ui_internal.h"
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
void td_clear(void) {}
uint32_t td_text(uint32_t x, uint32_t y, const char *text, uint32_t scale) { (void)y; return x + 6u * scale * (uint32_t)strlen(text); }
static void rgb(tl_rgb c) { printf("%02x%02x%02x ", c.r, c.g, c.b); }
int main(void) {
    static tp_engine engine;
    static char line[1024];
    memset(flash, 0xFF, sizeof flash);
    while (fgets(line, sizeof line, stdin)) {
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
