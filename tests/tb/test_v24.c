/* SPDX-License-Identifier: GPL-3.0-or-later - TBOSS 2.4 on the real code (engine, controls, settings.c on a fake
 * sector 11, the screens and lights): Settings > Lights > Out of scale and Dim (worked out in sixteenths of the LEDs'
 * steps, a dim light never rounding away, stored in their own record, kept by presets); "the sequencer grows up": 8
 * lanes and 8 patterns (lanes 5-8 in record kind 20, pattern 1's lanes 1-4 where firmware before 2.4 reads them), 8
 * lanes' notes due at once all going out (burst), mute and solo on the step screen's root buttons, M1..M7 choosing the
 * step option, patterns chosen at the end of the playing one's time round, songs (5 + root buttons, the menu, a DAW's
 * song position), copy and clear, live transpose from the chord pads, each lane's loop window (12 + held steps, the
 * menu), Fill on root button 9, the compaction keeping room with 8 full patterns; 2.4.11: U1..U3 on the step screen
 * the lane's length, direction, swing and rate. Every constant from the headers. */
#include "../../src/tb/settings.h"
#include "../../src/tb/sequence.h"
#include "../../src/tb/midi_in.h"
#include "../../src/tb/flash_store.h"
#include "../../src/tb/display.h"
#include "../../src/tb/leds.h"
#include "../../src/tb/jack.h"
#include "../../src/tb/nor.h"
#include "../../src/tb/ui.h"
#include "../../src/tb/controls_internal.h"
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
static char lines[64][32];
void td_clear(void) { memset(lines, 0, sizeof lines); }
uint32_t td_text(uint32_t x, uint32_t y, const char *text, uint32_t scale) {
    if (y < 64u && !x) snprintf(lines[y], sizeof lines[y], "%s", text);
    return x + 6u * scale * (uint32_t)strlen(text);
}

static tp_engine e;
static ts_scan scan;
static uint32_t now = 1000u;
static uint8_t sounding[16][128];
#define LOG 16384u
static uint8_t on_note[LOG], on_ch[LOG];
static uint32_t on_at[LOG], logged;
static bool writer(const uint8_t p[4], uint32_t token) {
    (void)token;
    uint32_t ch = p[1] & 15u;
    if (p[0] == TK_ON) {
        assert(!sounding[ch][p[2]]); sounding[ch][p[2]] = 1u;
        if (logged < LOG) { on_note[logged] = p[2]; on_ch[logged] = (uint8_t)ch; on_at[logged++] = now; }
    }
    if (p[0] == TK_OFF) { assert(sounding[ch][p[2]]); sounding[ch][p[2]] = 0u; }
    return true;
}
/* A DAW's clock: one MIDI clock every daw_us microseconds while daw_us is set. */
static uint32_t daw_us, daw_next_us;
static void midi(uint8_t a, uint8_t b, uint8_t c, uint8_t d) { const uint8_t p[4] = {a, b, c, d}; tm_packet(p, now); }
static const uint8_t addresses[4] = {0x35, 0x36, 0x38, 0x39};
static uint32_t raw[4], missing; /* missing: bit i, client i doesn't answer (NACK) */
static uint32_t client(uint8_t a) { for (uint32_t i = 0; i < 4u; ++i) if (addresses[i] == a) return i; assert(0); return 0; }
static struct bi_reply reader(void *ctx, uint8_t a, uint8_t reg, uint32_t gen, uint32_t stamp, uint32_t budget) {
    (void)ctx; (void)reg; (void)budget;
    uint32_t i = client(a);
    return (struct bi_reply){.status = (missing >> i) & 1u ? BI_NACK : BI_OK, .count = 2, .generation = gen, .timestamp_ms = stamp, .elapsed_us = 500,
                             .data = {(uint8_t)raw[i], (uint8_t)(raw[i] >> 8)}};
}
static void run(uint32_t ms) {
    for (uint32_t i = 0; i < ms; ++i, ++now) {
        while (daw_us && (int32_t)(now * 1000u - daw_next_us) >= 0) { midi(0x0Fu, 0xF8u, 0u, 0u); daw_next_us += daw_us; }
        if (now % 10u == 0u) (void)tc_poll(&tc_state, &e, reader, NULL, now);
        if (now % 15u == 0u) {
            scan.frame++; scan.last_ms = now; scan.valid = true;
            if (e.state == TK_IDLE) (void)tc_arm(&e, &scan, now, true); else (void)tc_frame(&e, &scan, now);
        }
        tc_step(&e, now, true, writer);
        if (e.pending_id) tp_complete(&e, e.pending_id, tp_pending_bytes(&e), true, now);
        st_requests(&tc_state, &e);
        st_service(&tc_state, now);
    }
}
/* Touch-controller bits (controls.c's tables). */
static const uint8_t fn_bit[TC_FUNCTIONS] = {6, 5, 0, 1, 7, 4, 3, 8, 9, 10, 11, 12, 14, 2};
static const uint8_t top_bit[12] = {3, 1, 0, 4, 5, 6, 7, 8, 9, 10, 11, 12};
static const uint8_t nav_bit[5] = {8, 7, 6, 1, 3}; /* left, up, centre, down, right */
static void hold_bit(uint8_t a, uint8_t bit, bool down) {
    if (down) raw[client(a)] |= 1u << bit; else raw[client(a)] &= ~(1u << bit);
    run(50);
}
static void tap_bit(uint8_t a, uint8_t bit) { hold_bit(a, bit, true); hold_bit(a, bit, false); }
static void nav(uint32_t n) { tap_bit(0x36, nav_bit[n]); }
static void mode_button(uint32_t i) { tap_bit(0x38, top_bit[i]); }
static void mode_hold(uint32_t i, bool down) { hold_bit(0x38, top_bit[i], down); }
static void root_button(uint32_t i) { tap_bit(0x39, top_bit[i]); }
static void root_hold(uint32_t i, bool down) { hold_bit(0x39, top_bit[i], down); }
static void centre(uint32_t i, bool down) { hold_bit(0x35, fn_bit[i], down); }
static void hold(uint32_t k) { scan.values[k] = 3000u; run(60); }
static void lift(uint32_t k) { scan.values[k] = 0u; run(60); }
static void touch(uint32_t k) { hold(k); lift(k); }  /* a tap */
static void draw(void) { tc_state.event_sequence++; ui_service(&tc_state, &e, &scan, true, now); }
static uint32_t step_pad(uint32_t s) { /* the right-half pad showing step s on the first page */
    for (uint32_t k = 48u; k < 96u; ++k) if (tc_step_of_pad(&tc_state, k - 48u) == s) return k;
    assert(0); return 0;
}
static void boot_keep(void) { /* a restart on the flash as it is */
    tc_init(&tc_state); memset(&e, 0, sizeof e); memset(&scan, 0, sizeof scan); memset(raw, 0, sizeof raw);
    memset(sounding, 0, sizeof sounding); logged = 0u; daw_us = 0u; missing = 0u;
    tm_reset(); sq_clear(&sq, SQ_UNIT_BAR, 0u);
    (void)st_load(&tc_state);
    run(600); assert(e.state == TK_ACTIVE);
}
static void boot(void) { fs_erase(); boot_keep(); }
static void quiet(void) {
    for (uint32_t ch = 0; ch < 16u; ++ch) for (uint32_t p = 0; p < 128u; ++p) assert(!sounding[ch][p]);
}
static void stop(void) { if (tc_state.transport) tc_toggle_transport(&tc_state); run(400); quiet(); }
static ln_step on_step(uint8_t value) { ln_step s = tc_brush_default(LN_DRUMS); s.value = value; return s; }
/* Lane `lane` of pattern p: drums, n steps of 1/16, step i playing note first + i. */
static void drums(uint32_t p, uint32_t lane, uint32_t n, uint8_t first) {
    ln_lane *l = &ln_bank[p].lanes[lane];
    l->kind = LN_DRUMS; ln_set_length(l, n);
    for (uint32_t i = 0; i < n; ++i) l->steps[i] = on_step((uint8_t)(first + i));
}
static uint32_t drum_ch(void) { return tc_state.drum_ch & 15u; }
/* The time of the k-th Note On of note n (any channel when ch is 16) from log entry `from`; 0 when not heard. */
static uint32_t time_of(uint8_t n, uint32_t k, uint32_t from) {
    for (uint32_t i = from; i < logged; ++i) if (on_ch[i] == drum_ch() && on_note[i] == n && !k--) return on_at[i];
    return 0u;
}
static bool heard(uint8_t n, uint32_t from) { return time_of(n, 0u, from) != 0u; }
static bool near(uint32_t a, uint32_t b, uint32_t within) { return a + within >= b && b + within >= a; }
static void steps_screen(void) {
    tc_state.ui = TC_UI_STEPS; tc_state.step_page = 0u; tc_state.step_held = LN_NONE; tc_state.step_mask = 0u; run(20);
}

/* --- 1. Out of scale and Dim --- */
static uint32_t sum(tl_rgb c) { return (uint32_t)c.r + c.g + c.b; }
static void lights(void) {
    boot();
    tp_layout l = tc_layout(&tc_state); l.melody_layout = TP_MELODY_CHROMATIC; assert(tc_set_layout(&tc_state, &e, l));
    /* a melody pad outside C major (C#: column 1 of Chromatic), and one in it */
    uint32_t out = 96u, in = 96u;
    for (uint32_t k = 48u; k < 96u && (out == 96u || in == 96u); ++k) {
        tp_notes n; assert(tc_pad(e.layout, k, &n) && n.count);
        if (!tc_in_scale(tc_state.scale, n.notes[0] % 12u)) out = k; else in = k;
    }
    assert(out < 96u && in < 96u);
    ui_pad_colors(&tc_state, &e, now); tl_rgb auto_out = pads[out], auto_in = pads[in];
    assert(sum(auto_out) < sum(auto_in)); /* AUTO: faint */
    tc_state.out_scale = TC_OUT_WHITE; ui_pad_colors(&tc_state, &e, now);
    assert(pads[out].r && pads[out].r == pads[out].g && pads[out].g == pads[out].b && !memcmp(&pads[in], &auto_in, sizeof auto_in));
    tl_rgb white10 = pads[out];
    tc_state.dim = 7u; ui_pad_colors(&tc_state, &e, now); assert(sum(pads[out]) > 3u * sum(white10)); /* 50 % */
    tc_state.out_scale = TC_OUT_DARK; ui_pad_colors(&tc_state, &e, now);
    assert(!pads[out].r && !pads[out].g && pads[out].b == 1u);                           /* dark, as an unlit pad */
    tc_state.out_scale = TC_OUT_OFF; ui_pad_colors(&tc_state, &e, now);
    assert(!pads[out].r && !pads[out].g && !pads[out].b && !memcmp(&pads[in], &auto_in, sizeof auto_in)); /* off */
    tc_state.out_scale = TC_OUT_WARM; ui_pad_colors(&tc_state, &e, now); /* warm white (board test L3's pick): blue lit */
    assert(pads[out].r >= pads[out].g && pads[out].g > pads[out].b && pads[out].b);
    tc_state.out_scale = TC_OUT_COLOR; tc_state.led_scheme = TC_LEDS_ROOT_FIFTH; ui_pad_colors(&tc_state, &e, now);
    assert(!(pads[out].r == pads[out].g && pads[out].g == pads[out].b)); /* the note's own color, in Root+5th too */
    /* Brightness 1, Dim 5 %: still lit (white stays white) */
    tc_state.led_scheme = TC_LEDS_RAINBOW; tc_state.brightness = 0u; tc_state.dim = 1u; tc_state.out_scale = TC_OUT_WHITE;
    ui_pad_colors(&tc_state, &e, now); assert(pads[out].r == 1u && pads[out].g == 1u && pads[out].b == 1u);
    tc_state.out_scale = TC_OUT_COLOR; ui_pad_colors(&tc_state, &e, now); assert(sum(pads[out]) && !(pads[out].b == 1u && !pads[out].r && !pads[out].g));
    /* A silent chord pad follows Dim (AUTO: 15 %), and so do the panel's inactive buttons, gentler: AUTO 15 %, then
     * 2 % a step of Dim, 15 % at Dim 15 % */
    static const uint8_t panel[TC_FUNCTIONS] = {7,1,13,12,2,0,14,3,4,5,6,9,10,11}; /* ui_lights.c panel_channels */
    tc_state.brightness = 7u; tc_state.out_scale = TC_OUT_AUTO; tc_state.dim = 0u; tc_state.chord_mute = 1u;
    ui_pad_colors(&tc_state, &e, now); uint32_t silent15 = sum(pads[tc_physical(&tc_state, 0)]);
    uint16_t lowest = 4095u; for (uint32_t f = 0; f < TC_FUNCTIONS; ++f) if (buttons[panel[f]] < lowest) lowest = buttons[panel[f]];
    assert(lowest == 614u);
    tc_state.dim = 6u; ui_pad_colors(&tc_state, &e, now); /* 40 % */
    assert(sum(pads[tc_physical(&tc_state, 0)]) > 2u * silent15);
    lowest = 4095u; for (uint32_t f = 0; f < TC_FUNCTIONS; ++f) if (buttons[panel[f]] < lowest) lowest = buttons[panel[f]];
    assert(lowest == 4095u * 21u / 100u); /* Dim 40 %: 21 % */
    tc_state.dim = 1u; ui_pad_colors(&tc_state, &e, now); /* 5 %: 11 %, below AUTO's */
    lowest = 4095u; for (uint32_t f = 0; f < TC_FUNCTIONS; ++f) if (buttons[panel[f]] < lowest) lowest = buttons[panel[f]];
    assert(lowest == 4095u * 11u / 100u); tc_state.dim = 6u;
    tc_state.chord_mute = 0u;
    /* The menu, the record, kept over a restart and by presets (board settings) */
    assert(tc_set_item(&tc_state, &e, TC_MENU_OUT_SCALE, TC_OUT_WARM) && tc_set_item(&tc_state, &e, TC_MENU_DIM, 3u));
    assert(!tc_set_item(&tc_state, &e, TC_MENU_OUT_SCALE, TC_OUT_SCALES) && !tc_set_item(&tc_state, &e, TC_MENU_DIM, TC_DIMS));
    assert(!strcmp(ui_value_text(TC_MENU_OUT_SCALE, TC_OUT_WARM), "WARM") && !strcmp(ui_value_text(TC_MENU_DIM, 3u), "15%") &&
           !strcmp(ui_value_text(TC_MENU_DIM, 0u), "AUTO") && !strcmp(ui_item_name(TC_MENU_OUT_SCALE), "OUT OF SCALE"));
    run(ST_SAVE_DELAY_MS + 100u);
    uint32_t w[ST_PAYLOAD]; assert(st_get(ST_LIGHTS, 0, &tc_state, w) == ST_OK && w[0] == (1u | TC_OUT_WARM << 8 | 3u << 16));
    for (uint32_t i = 1; i < ST_PAYLOAD; ++i) assert(!w[i]);
    tc_state.ui = TC_UI_EDIT; tc_state.group = TC_GROUP_PRESETS; tc_state.menu = TC_MENU_SAVE; tc_state.edit_value = 2u; nav(TC_NAV_ENTER); run(50);
    boot_keep(); assert(tc_state.out_scale == TC_OUT_WARM && tc_state.dim == 3u);
    tc_state.dim = 5u; tc_state.out_scale = TC_OUT_OFF;
    tc_state.ui = TC_UI_EDIT; tc_state.group = TC_GROUP_PRESETS; tc_state.menu = TC_MENU_LOAD; tc_state.edit_value = 2u; nav(TC_NAV_ENTER); run(50);
    assert(tc_state.last_action == TC_PRESET_LOADED && tc_state.dim == 5u && tc_state.out_scale == TC_OUT_OFF); /* the board's */
    /* SysEx: written and in use at once; damaged ones refused; AUTO, AUTO is no record */
    w[0] = 1u | TC_OUT_COLOR << 8 | 1u << 16; assert(st_put(ST_LIGHTS, 0, w, &tc_state, &e) == ST_OK && tc_state.out_scale == TC_OUT_COLOR && tc_state.dim == 1u);
    w[0] = 1u | TC_OUT_SCALES << 8; assert(st_put(ST_LIGHTS, 0, w, &tc_state, &e) == ST_BAD_VALUE);
    w[0] = 1u; w[5] = 1u; assert(st_put(ST_LIGHTS, 0, w, &tc_state, &e) == ST_BAD_VALUE); w[5] = 0u;
    assert(st_put(ST_LIGHTS, 1, w, &tc_state, &e) == ST_BAD_VALUE && tc_state.out_scale == TC_OUT_COLOR);
    w[0] = 0u; assert(st_put(ST_LIGHTS, 0, w, &tc_state, &e) == ST_OK && !tc_state.out_scale && !tc_state.dim &&
                      st_get(ST_LIGHTS, 0, &tc_state, w) == ST_ABSENT);
    printf("lights: Out of scale (white, warm, color in any scheme, off) and Dim (5-50 %%) on out-of-scale notes, silent "
           "pads and the panel, lit at Brightness 1 and Dim 5 %%, the record, a restart, presets leave them, SysEx; ");
}

/* --- 2. 8 lanes x 8 patterns stored --- */
static void storage(void) {
    boot();
    ln_lane *l = &ln_bank[2].lanes[5]; /* pattern 3, lane 6 */
    l->kind = LN_MELODY; ln_set_length(l, 40u);
    for (uint32_t s = 0; s < 40u; s += 3u) { l->steps[s] = on_step((uint8_t)s); ln_changed_in(2, 5, s); }
    ln_bank[0].lanes[1].steps[20] = on_step(44u); ln_changed_in(0, 1, 20); /* pattern 1, lane 2: where 2.3 has it */
    ln_bank[2].lanes[5].loop_from = 3u; ln_bank[2].lanes[5].loop_to = 30u; ln_bank[2].lanes[5].follow = 1u; ln_extra_changed(2);
    ln_bank[0].lanes[6].solo = 1u; ln_extra_changed(0);
    uint32_t base = st_records();
    run(ST_SAVE_DELAY_MS + 100u);
    assert(st_records() == base + 3u + 1u + 2u); /* lane 6 chunks 0..2, pattern 1's lane 2 chunk 1, two pattern records */
    uint32_t w[ST_PAYLOAD];
    /* where they are: lanes 5-8 in kind 20, 1-4 in kind 7, slot = pattern * 16 + lane % 4 * 4 + chunk */
    for (uint32_t chunk = 0; chunk < 3u; ++chunk) assert(st_get(ST_LANE_HI, (uint8_t)(2u * 16u + 1u * 4u + chunk), &tc_state, w) == ST_OK);
    assert(st_get(ST_LANE_HI, 2u * 16u + 1u * 4u + 3u, &tc_state, w) == ST_ABSENT);
    assert(st_get(ST_LANE, 5u, &tc_state, w) == ST_OK); /* pattern 1, lane 2, chunk 1: slot 5, as before 2.4 */
    assert(st_get(ST_LANE, 2u * 16u + 1u * 4u, &tc_state, w) == ST_ABSENT && st_get(ST_LANE, 128u, &tc_state, w) == ST_BAD_VALUE);
    assert(st_get(ST_PATTERN, 2, &tc_state, w) == ST_OK && w[0] == 1u && w[1u + 5u] == (3u | 30u << 8 | 1u << 24));
    assert(st_get(ST_PATTERN, 1, &tc_state, w) == ST_ABSENT && st_get(ST_PATTERN, LN_PATTERNS, &tc_state, w) == ST_BAD_VALUE);
    static ln_pattern before[LN_PATTERNS]; memcpy(before, ln_bank, sizeof before);
    boot_keep(); assert(!memcmp(before, ln_bank, sizeof before));
    assert(ln_window(&ln_bank[2].lanes[5], NULL) == 27u && ln_plays(&ln_bank[0], 6u) && !ln_plays(&ln_bank[0], 0u));
    /* The song record: the pattern playing and the song */
    ln_song.count = 3u; ln_song.links[0] = 0u; ln_song.links[1] = 2u; ln_song.links[2] = 2u; ln_song.on = 1u; ln_song_changed();
    ln_select(2u); run(ST_SAVE_DELAY_MS + 100u);
    assert(st_get(ST_SONG, 0, &tc_state, w) == ST_OK && w[0] == (1u | 2u << 8 | 1u << 16) && w[1] == (1u | 3u << 8 | 3u << 16));
    boot_keep(); assert(ln_current == 2u && ln_song.on && ln_song.count == 3u && ln_song.links[1] == 2u);
    w[1] = 1u | 0u << 8 | 3u << 16; assert(st_put(ST_SONG, 0, w, &tc_state, &e) == ST_BAD_VALUE); /* a gap */
    w[1] = 9u; assert(st_put(ST_SONG, 0, w, &tc_state, &e) == ST_BAD_VALUE);                     /* no pattern 9 */
    /* A log from 2.3 (pattern 1's four lanes): read as pattern 1, lanes 1-4 */
    fs_erase(); tc_init(&tc_state); (void)st_load(&tc_state);
    ln_pattern old; ln_clear(&old); old.lanes[3].kind = LN_DRUMS; old.lanes[3].steps[50] = on_step(60u); old.seed = 77u;
    for (uint32_t slot = 0; slot < LN_LANES_V1 * LN_CHUNKS; ++slot) { ln_encode(&old, slot, w); assert(st_put(ST_LANE, (uint8_t)slot, w, &tc_state, &e) == ST_OK); }
    for (uint32_t i = 0; i < LN_PATTERNS; ++i) ln_clear(&ln_bank[i]);
    boot_keep(); assert(ln_current == 0u && ln_bank[0].seed == 77u && ln_bank[0].lanes[3].steps[50].on && ln_bank[0].lanes[3].kind == LN_DRUMS);
    printf("storage: lanes 5-8 in kind 20, pattern 1's lanes 1-4 where 2.3 reads them, windows, solo, follow and the song "
           "in their records, a restart, damaged songs refused, a 2.3 log read; ");
}
/* 8 patterns of 8 full lanes: every lane chunk stored (256 records); the compaction then waits until it can reclaim a
 * quarter of the sector, and keeps every pattern. */
static void full_patterns(void) {
    boot();
    for (uint32_t p = 0; p < LN_PATTERNS; ++p) for (uint32_t i = 0; i < LN_LANES; ++i) {
        ln_lane *l = &ln_bank[p].lanes[i]; ln_set_length(l, 64u); l->kind = LN_DRUMS;
        for (uint32_t s = 0; s < LN_STEPS; ++s) { l->steps[s] = on_step((uint8_t)(36u + (p * 7u + i * 3u + s) % 40u)); ln_changed_in(p, i, s); }
    }
    run(ST_SAVE_DELAY_MS + 400u); /* 2.4.5 (TBR-007): one record a pass, a pass a ms here */
    uint32_t stored = st_records(); assert(stored >= LN_RECORDS);
    static ln_pattern before[LN_PATTERNS]; memcpy(before, ln_bank, sizeof before);
    /* edits until past half the sector: a restart doesn't compact while less than a quarter can be reclaimed */
    for (uint32_t n = 0; st_records() < 400u; ++n) {
        ln_bank[0].lanes[0].steps[0].velocity = (uint8_t)(1u + n % 100u); ln_changed_in(0, 0, 0); run(ST_SAVE_DELAY_MS + 50u);
    }
    memcpy(before, ln_bank, sizeof before);
    uint32_t used = st_records(); boot_keep(); assert(st_records() == used && !memcmp(before, ln_bank, sizeof before));
    for (uint32_t n = 0; st_records() < 600u; ++n) {
        ln_bank[1].lanes[7].steps[63].velocity = (uint8_t)(1u + n % 100u); ln_changed_in(1, 7, 63); run(ST_SAVE_DELAY_MS + 50u);
    }
    memcpy(before, ln_bank, sizeof before);
    boot_keep(); assert(st_records() < 400u && !memcmp(before, ln_bank, sizeof before)); /* compacted: every pattern kept */
    printf("8 full patterns: %u records, no compaction at every start, compacted later with every pattern kept; ", stored);
}

/* --- 3. 8 lanes due at once --- */
static void burst(void) {
    boot();
    tc_state.at_chord = tc_state.at_melody = TC_AT_OFF;
    uint32_t want = 0;
    for (uint32_t i = 0; i < LN_LANES; ++i) { /* each lane a chord on step 1 and 3 of 4, on its own channel */
        ln_lane *l = &ln.lanes[i];
        l->kind = LN_CHORDS; l->channel = (uint8_t)(i + 1u); ln_set_length(l, 4u);
        ln_step s = tc_brush_default(LN_CHORDS); s.value = (uint8_t)(4u * i + 3u); /* bottom row, column i */
        l->steps[0] = s; l->steps[2] = s;
        tp_notes n; uint8_t ch; assert(tc_step_notes(e.layout, l->kind, l->channel, &s, &n, &ch) && ch == i); want += n.count;
    }
    assert(want >= 3u * LN_LANES);
    tc_toggle_transport(&tc_state); uint32_t start = now;
    run(2000); /* 8 strikes of every lane */
    uint32_t counts[16] = {0}, late = 0;
    for (uint32_t i = 0; i < logged; ++i) {
        counts[on_ch[i]]++;
        uint32_t into = (on_at[i] - start) % 250u; /* steps 1 and 3 of 1/16 */
        if (into > 8u && into < 242u) late++;
    }
    uint32_t total = 0; for (uint32_t ch = 0; ch < LN_LANES; ++ch) total += counts[ch];
    assert(total == 8u * want && !late); /* every note of every strike, each within a few ms of its step */
    stop();
    printf("burst: 8 chord lanes on one step, %u notes a strike, every one out within 8 ms, 8 times over; ", want);
}

/* --- 4. mute and solo on the root buttons --- */
static void mute_solo(void) {
    boot();
    drums(0, 0, 4, 36); drums(0, 1, 4, 48);
    steps_screen();
    root_button(1); assert(ln.lanes[1].mute && tc_state.last_action == TC_LANE_MUTE && (ln_dirty[0] & (1u << 4)));
    draw(); assert(!strcmp(lines[11], "1-48 CH10 1-345678") && !strcmp(lines[34], "LANE 2 MUTED"));
    tc_toggle_transport(&tc_state); run(600); uint32_t from = 0;
    assert(heard(36, from) && !heard(48, from));
    root_button(1); assert(!ln.lanes[1].mute); from = logged; run(600); assert(heard(48, from));
    /* Shift + root button 2: solo; only soloed lanes play */
    tc_state.buttons[0] = TC_ACT_SHIFT; centre(0, true); root_button(1); centre(0, false);
    assert(ln.lanes[1].solo && !ln_plays(&ln, 0) && ln_plays(&ln, 1) && (ln_extra_dirty & 1u));
    draw(); assert(!strcmp(lines[11], "1-48 CH10 S-2------") && !strcmp(lines[34], "LANE 2 SOLO"));
    from = logged; run(600); assert(!heard(36, from) && heard(48, from));
    root_button(0); /* muting another lane changes nothing heard */
    root_button(TC_ROOT_LANES_ON); assert(!ln.lanes[0].mute && !ln.lanes[1].solo && tc_state.last_action == TC_LANES_ON);
    from = logged; run(600); assert(heard(36, from) && heard(48, from));
    stop();
    /* root buttons on the step screen don't change the key */
    assert(tc_state.tonic == 0u);
    printf("mute and solo: root buttons 1-8 mute, Shift solos, 10 every lane again, heard and on the screen; ");
}

/* --- 5. M1..M7 choose the step option --- */
static void fields(void) {
    boot(); steps_screen();
    hold(step_pad(0)); mode_button(TC_MODE_M1 + 2u); assert(tc_state.step_field == TC_FIELD_PROBABILITY);
    nav(TC_NAV_LEFT); lift(step_pad(0));
    assert(ln.lanes[0].steps[0].on && ln.lanes[0].steps[0].probability == 1u); /* on, 90 %, not toggled off */
    for (uint32_t m = 0; m < 7u; ++m) { mode_button(TC_MODE_M1 + m); assert(tc_state.step_field == tc_mode_fields[m]); }
    assert(tc_state.step_field == TC_FIELD_TIE); draw(); assert(!strcmp(lines[34], "FIELD: TIE"));
    hold(step_pad(0)); draw(); assert(!strncmp(lines[34], "TIE ", 4u)); lift(step_pad(0));
    mode_button(TC_MODE_M1 + 3u); assert(tc_state.step_field == TC_FIELD_RATCHET);
    assert(tc_state.scale == 0u); /* the mode buttons don't change the scale here */
    printf("fields: M1-M7 velocity, gate, chance, ratchet, condition, nudge, tie, a held step not toggled; ");
}

/* --- 6. patterns, songs, copy, clear --- */
static void patterns(void) {
    boot();
    drums(0, 0, 4, 36); drums(1, 0, 4, 50); drums(2, 0, 2, 70);
    /* stopped: at once */
    assert(tc_set_item(&tc_state, &e, TC_MENU_PATTERN, 1u)); run(20); assert(ln_current == 1u && ln_queued == LN_NO_PATTERN);
    assert(tc_set_item(&tc_state, &e, TC_MENU_PATTERN, 0u)); run(20); assert(ln_current == 0u);
    /* playing: at the end of the playing pattern's time round (4 steps of 1/16: 500 ms) */
    tc_toggle_transport(&tc_state); run(5); uint32_t start = time_of(36, 0, 0); assert(start);
    run(250); assert(tc_set_item(&tc_state, &e, TC_MENU_PATTERN, 1u)); assert(ln_queued == 1u && ln_current == 0u);
    steps_screen(); draw(); assert(strstr(lines[45], ">P2"));
    run(400);
    assert(ln_current == 1u && near(time_of(50, 0, 0), start + 500u, 25u) && time_of(39, 0, 0) && near(time_of(39, 0, 0), start + 375u, 25u));
    draw(); assert(!strncmp(lines[0], "P2 L1 DRUMS", 11u));
    /* root buttons 11 and 12: the previous and next pattern */
    root_button(TC_ROOT_PATTERN_UP); assert(ln_queued == 2u); root_button(TC_ROOT_PATTERN_DOWN); root_button(TC_ROOT_PATTERN_DOWN);
    assert(ln_queued == 0u); root_button(TC_ROOT_PATTERN_UP); assert(ln_queued == LN_NO_PATTERN); /* the one playing */
    stop();
    /* 5 + root buttons: a song (patterns 1, 2, 2, 3 in turn), from the top */
    tc_state.ui = TC_UI_STEPS; mode_hold(TC_MODE_PATTERN, true); root_button(0); root_button(1); root_button(1); root_button(2);
    draw(); assert(!strcmp(lines[34], "SONG 1223"));
    mode_hold(TC_MODE_PATTERN, false);
    assert(ln_song.on && ln_song.count == 4u && ln_song.links[3] == 2u && tc_state.last_action == TC_SONG_SET);
    uint32_t from = logged; tc_toggle_transport(&tc_state); run(1900);
    uint32_t t0 = time_of(36, 0, from); assert(t0);
    assert(near(time_of(50, 0, from), t0 + 500u, 25u) && near(time_of(50, 1, from), t0 + 1000u, 25u) &&
           near(time_of(70, 0, from), t0 + 1500u, 25u) && near(time_of(36, 1, from), t0 + 1750u, 25u)); /* pattern 3: 2 steps */
    /* choosing a pattern ends the song */
    assert(tc_set_item(&tc_state, &e, TC_MENU_PATTERN, 1u) && !ln_song.on);
    stop();
    assert(tc_set_item(&tc_state, &e, TC_MENU_SONG, 1u) && ln_song.on); assert(tc_set_item(&tc_state, &e, TC_MENU_SONG, 0u));
    /* a DAW's song position lands in the song: Song 1, 2; the 5th sixteenth is pattern 2's first step */
    ln_song.count = 2u; ln_song.links[0] = 0u; ln_song.links[1] = 1u; ln_song.on = 1u;
    daw_us = 20833u; daw_next_us = now * 1000u; run(200);
    midi(0x03u, 0xF2u, 4u, 0u); run(30); from = logged; midi(0x0Fu, 0xFBu, 0u, 0u); run(120);
    assert(logged > from && on_note[from] == 50u);
    midi(0x0Fu, 0xFCu, 0u, 0u); daw_us = 0u; run(400); quiet(); ln_song.on = 0u;
    /* no song, a DAW: pattern 2 chosen takes over at the end of pattern 1; the playhead then moved back to the start
     * while playing: pattern 2's first step at once (not stuck until the old switch point comes round again) */
    assert(tc_set_item(&tc_state, &e, TC_MENU_PATTERN, 0u)); run(20); assert(ln_current == 0u);
    daw_us = 20833u; daw_next_us = now * 1000u; run(100); midi(0x0Fu, 0xFAu, 0u, 0u); run(200);
    assert(tc_set_item(&tc_state, &e, TC_MENU_PATTERN, 1u)); run(500); assert(ln_current == 1u);
    midi(0x03u, 0xF2u, 0u, 0u); from = logged; run(420);
    assert(logged >= from + 4u && on_note[from] == 50u && on_note[from + 1u] == 51u && on_note[from + 3u] == 53u); /* on, not stuck */
    midi(0x0Fu, 0xFCu, 0u, 0u); daw_us = 0u; run(400); quiet();
    /* Copy to and Clear pattern (on the board) */
    ln_select(1u); tc_state.ui = TC_UI_EDIT; tc_state.group = TC_GROUP_PATTERNS; tc_state.menu = TC_MENU_PATTERN_COPY;
    tc_state.edit_value = 5u; nav(TC_NAV_ENTER);
    assert(tc_state.last_action == TC_PATTERN_COPIED && !memcmp(&ln_bank[5], &ln_bank[1], sizeof ln_bank[1]) && ln_dirty[5]);
    assert(!tc_set_item(&tc_state, &e, TC_MENU_PATTERN_COPY, 6u)); /* the editor copies on its own */
    ln_select(5u); tc_state.ui = TC_UI_EDIT; tc_state.menu = TC_MENU_PATTERN_CLEAR; tc_state.edit_value = 1u; nav(TC_NAV_ENTER);
    assert(tc_state.last_action == TC_PATTERN_CLEARED && !ln_any(&ln_bank[5]) && ln_bank[1].lanes[0].steps[0].on);
    assert(!strcmp(ui_value_text(TC_MENU_PATTERN, 5u), "P6 EMPTY") && !strcmp(ui_value_text(TC_MENU_PATTERN, 1u), "P2"));
    printf("patterns: chosen at once while stopped, at the end of the time round while playing, 11/12 step, 5 + roots "
           "make a song played in turn, a pattern chosen ends it, a DAW's song position, copy, clear; ");
}

/* --- 7. live transpose, loop window, Fill on root 9 --- */
static void follow_loop_fill(void) {
    boot();
    tc_state.at_chord = tc_state.at_melody = TC_AT_OFF;
    ln_lane *m = &ln.lanes[0]; m->kind = LN_MELODY; ln_set_length(m, 1u); m->follow = 1u; m->channel = 5u;
    m->steps[0] = tc_brush_default(LN_MELODY); m->steps[0].value = 3u; /* the Scale layout's bottom left: C3 */
    ln_lane *d = &ln.lanes[1]; d->kind = LN_DRUMS; ln_set_length(d, 1u); d->follow = 1u; d->steps[0] = on_step(40u);
    tc_toggle_transport(&tc_state); run(300);
    uint8_t first = 0; for (uint32_t i = 0; i < logged; ++i) if (on_ch[i] == 4u) { first = on_note[i]; break; }
    assert(first);
    /* strike the chord pad on degree IV (column 3 of the scale's chords): the melody lane moves up 3 scale steps */
    assert(tp_follow_steps(e.layout, (uint8_t)(4u * 3u + 3u)) == 3 && tp_follow_steps(e.layout, (uint8_t)(4u * 4u + 3u)) == -3);
    touch(4u * 3u + 3u); uint32_t from = logged; run(300);
    uint8_t moved = 0; for (uint32_t i = from; i < logged; ++i) if (on_ch[i] == 4u) { moved = on_note[i]; break; }
    assert(moved == tc_harmony_note(e.layout, first, 3) && heard(40, from)); /* the drums stay */
    m->follow = 0u; from = logged; run(300);
    moved = 0; for (uint32_t i = from; i < logged; ++i) if (on_ch[i] == 4u) { moved = on_note[i]; break; }
    assert(moved == first);
    stop();
    /* Board test S8: on the step screen Shift (U3 held) + a left-half pad moves the lanes that follow, silent, nothing
     * picked; the moved lane's steps light as they sound, the screen says by how much; the move stays after Shift */
    m->follow = 1u; tc_state.u3_shift = 1u; touch(4u * 0u + 3u); /* back to I (the play screen) */
    steps_screen(); tc_state.lane = 0u;
    ln_step brush = tc_state.brush[0]; from = logged;
    mode_hold(TC_U3, true); touch(4u * 3u + 3u); mode_hold(TC_U3, false); /* the melody pad F3: IV */
    assert(logged == from && tc_state.last_action == TC_FOLLOW_SET && !memcmp(&brush, &tc_state.brush[0], sizeof brush));
    assert(tp_follow_lift(e.layout) == 3);
    draw(); assert(!strcmp(lines[34], "FOLLOW +3 (F)") && (strstr(lines[22], "FOLLOW+3") || strstr(lines[22], " F+3")));
    ui_light lit; assert(ui_step_color(&tc_state, &e, step_pad(0), 1000u, 0u, &lit));
    { ui_light f = ui_scaled(ui_hues[5].r, ui_hues[5].g, ui_hues[5].b, 1000u); assert(!memcmp(&lit, &f, sizeof f)); } /* F, not C */
    tc_toggle_transport(&tc_state); from = logged; run(300);
    moved = 0; for (uint32_t i = from; i < logged; ++i) if (on_ch[i] == 4u) { moved = on_note[i]; break; }
    assert(moved == tc_harmony_note(e.layout, first, 3));
    stop();
    from = logged; touch(4u * 3u + 3u); /* no Shift: picks it and sounds, the lanes stay */
    assert(logged > from && tc_state.last_action == TC_STEP_PICKED && tp_follow_lift(e.layout) == 3);
    m->kind = LN_CHORDS; mode_hold(TC_U3, true); touch(4u * 4u + 3u); mode_hold(TC_U3, false); /* a chord lane: chord pad V */
    assert(tc_state.follow_pad == 4u * 4u + 3u && tp_follow_lift(e.layout) == -3);
    /* a melody lane: a pad a row up, an octave up too (the second row's F: IV + 7 steps) */
    m->kind = LN_MELODY; mode_hold(TC_U3, true); touch(4u * 3u + 2u); mode_hold(TC_U3, false);
    assert(tp_follow_lift(e.layout) == 10); draw(); assert(!strcmp(lines[34], "FOLLOW +10 (F)"));
    mode_hold(TC_U3, true); touch(4u * 4u + 0u); mode_hold(TC_U3, false); assert(tp_follow_lift(e.layout) == 3 * 7 - 3); /* top row's G */
    /* the loop window: off steps outside it dark (as past the end), inside lit, the beat brighter */
    ln_set_length(m, 16u); m->loop_from = 2u; m->loop_to = 6u;
    ui_light a, b, c;
    assert(ui_step_color(&tc_state, &e, step_pad(1), 1000u, 0u, &a) && !a.r && !a.g && !a.b);
    assert(ui_step_color(&tc_state, &e, step_pad(2), 1000u, 0u, &b) && b.r && ui_step_color(&tc_state, &e, step_pad(4), 1000u, 0u, &c) && c.r > b.r);
    m->loop_from = m->loop_to = 0u;
    tc_state.ui = TC_UI_PLAY; run(20);
    /* Loop window: steps 3-6 of 8 */
    boot(); drums(0, 0, 8, 36); steps_screen();
    hold(step_pad(2)); hold(step_pad(5)); mode_button(TC_MODE_LOOP); lift(step_pad(5)); lift(step_pad(2));
    assert(ln.lanes[0].steps[2].on && ln.lanes[0].steps[5].on && ln_window(&ln.lanes[0], &(uint32_t){0}) == 4u);
    draw(); assert(!strcmp(lines[34], "LOOP 3-6"));
    assert(tc_menu_value(&tc_state, TC_MENU_LOOP_FROM) == 2u && tc_menu_value(&tc_state, TC_MENU_LOOP_TO) == 5u);
    tc_toggle_transport(&tc_state); uint32_t start = now; run(1050);
    for (uint8_t n = 36; n < 44; ++n) assert(heard(n, 0) == (n >= 38 && n <= 41));
    assert(near(time_of(38, 1, 0), start + 500u, 25u)); /* four steps a time round */
    assert(tc_set_item(&tc_state, &e, TC_MENU_LOOP_TO, 7u) && ln_window(&ln.lanes[0], NULL) == 6u);
    assert(tc_set_item(&tc_state, &e, TC_MENU_LOOP_FROM, 7u) && ln_window(&ln.lanes[0], NULL) == 1u);
    mode_button(TC_MODE_LOOP); assert(ln_window(&ln.lanes[0], NULL) == 8u); /* none held: the whole lane */
    draw(); assert(!strcmp(lines[34], "LOOP: WHOLE LANE"));
    stop();
    /* Fill on root button 9 */
    ln.lanes[0].steps[1].condition = LN_FILL; from = logged; tc_toggle_transport(&tc_state); run(1100);
    assert(!heard(37, from));
    root_hold(TC_ROOT_FILL, true); assert(tc_fill(&tc_state)); from = logged; run(1000); assert(heard(37, from));
    root_hold(TC_ROOT_FILL, false); assert(!tc_fill(&tc_state));
    /* Board test S10: held through readings of the root buttons missed now and then (every 4th poll), Fill stays;
     * let go during a missed reading, it ends at the next one; a controller lost for good lets go of it */
    root_hold(TC_ROOT_FILL, true);
    for (uint32_t i = 0; i < 100u; ++i) { missing = i % 4u == 0u ? 8u : 0u; run(10); assert(tc_fill(&tc_state)); }
    from = logged; run(1000); assert(heard(37, from) && tc_fill(&tc_state));
    missing = 8u; run(30); raw[client(0x39)] = 0u; run(30); assert(tc_fill(&tc_state)); /* not seen yet */
    missing = 0u; run(30); assert(!tc_fill(&tc_state));
    root_hold(TC_ROOT_FILL, true); missing = 8u; run(10u * TC_LOST_POLLS + 50u); assert(!tc_fill(&tc_state));
    missing = 0u; run(50); assert(!tc_fill(&tc_state)); /* back, still held: a baseline, not a press */
    root_hold(TC_ROOT_FILL, false);
    stop();
    /* Board test S2: on the step screen ▲ goes to the next lane, ▼ back (round) */
    steps_screen(); tc_state.lane = 0u;
    nav(1); assert(tc_state.lane == 1u); nav(3); nav(3); assert(tc_state.lane == LN_LANES - 1u); nav(1); assert(!tc_state.lane);
    printf("follow: a melody lane moves with the chord pad's degree (the nearer way), drums and lanes not following stay; "
           "loop window: 12 + held steps, played alone, the menu, the whole lane again; Fill on root button 9, held through "
           "missed readings; ▲ the next lane; Shift + a left-half pad on the step screen moves the lanes that follow, silent; ");
}

/* 2.4.4 (review TBR-016): Shift + down pastes the copied lane with its loop window and follow, and they are stored: a
 * restart gives the pasted lane the copy's window (before, its old one: only its steps were saved). The lane pasted
 * onto keeps its own solo (decided 2026-11-10), off or on. */
static void paste(void) {
    boot();
    drums(0, 0, 8, 36); drums(0, 1, 8, 50); drums(0, 2, 8, 60);
    ln_lane *a = &ln.lanes[0], *b = &ln.lanes[1], *c = &ln.lanes[2];
    a->loop_from = 2u; a->loop_to = 5u; a->follow = 1u; a->solo = 1u; c->solo = 1u;
    steps_screen();
    tc_state.lane = 0u; tc_steps_shift(&tc_state, TC_NAV_UP, now); assert(tc_state.last_action == TC_LANE_COPIED);
    tc_state.lane = 1u; tc_steps_shift(&tc_state, TC_NAV_DOWN, now); assert(tc_state.last_action == TC_LANE_PASTED);
    tc_state.lane = 2u; tc_steps_shift(&tc_state, TC_NAV_DOWN, now);
    assert(b->loop_from == 2u && b->loop_to == 5u && b->follow == 1u && !b->solo && b->steps[0].value == 36u);
    assert(c->loop_from == 2u && c->loop_to == 5u && c->solo == 1u && c->steps[7].value == 43u);
    run(3500); /* saved 3 s after the last change */
    boot_keep();
    b = &ln.lanes[1]; c = &ln.lanes[2];
    assert(b->loop_from == 2u && b->loop_to == 5u && b->follow == 1u && !b->solo && b->steps[0].value == 36u);
    assert(c->loop_from == 2u && c->loop_to == 5u && c->follow == 1u && c->solo == 1u && c->steps[0].value == 36u);
    assert(ln_window(b, &(uint32_t){0}) == 3u);
    printf("paste: the loop window and follow pasted and kept over a restart, the lane's own solo kept; ");
}

/* --- 2.4.11 (roadmap A): U1..U3 on the step screen, the lane's settings --- */
static uint32_t past_light(void) { /* step 20's pad (past a short lane's end): its blue, 0 when not blue */
    ui_pad_colors(&tc_state, &e, now);
    tl_rgb c = pads[step_pad(20)];
    return c.r || c.g ? 0u : c.b;
}
static void lane_settings(void) {
    boot(); drums(0, 0, 16, 36); steps_screen();
    ln_lane *l = &ln.lanes[0];
    const uint32_t U1 = TC_U3 - 2u, U2 = TC_U3 - 1u;
    uint8_t scale = tc_state.scale, lane = tc_state.lane;
    /* a tap shows the value and changes nothing */
    mode_button(U1); assert(ln_length(l) == 16u && tc_state.scale == scale && !tc_state.lane_hold);
    draw(); assert(!strcmp(lines[34], "LENGTH 16"));
    /* U1 held: left/right -1/+1; a step pad is the last step (past the end too), not toggled; up/down nothing */
    mode_hold(U1, true); draw(); assert(!strcmp(lines[34], "LENGTH 16") && !strcmp(lines[56], "<- >+  PAD: LAST STEP"));
    nav(TC_NAV_RIGHT); assert(ln_length(l) == 17u); nav(TC_NAV_LEFT); nav(TC_NAV_LEFT); assert(ln_length(l) == 15u);
    uint32_t dim = past_light(); /* the steps past the end dim blue while Length is held (dark is 0, 0, 1) */
    assert(dim > 4u);
    nav(TC_NAV_UP); nav(TC_NAV_DOWN); assert(tc_state.lane == lane && tc_state.ui == TC_UI_STEPS);
    touch(step_pad(31)); assert(ln_length(l) == 32u && !l->steps[31].on); draw(); assert(!strcmp(lines[34], "LENGTH 32"));
    touch(step_pad(3)); assert(ln_length(l) == 4u && l->steps[3].on);
    mode_hold(U1, false); assert(!tc_state.lane_hold); draw(); assert(!strcmp(lines[34], "LENGTH 4"));
    assert(past_light() < dim); /* let go: dark again */
    /* let go: the pads and the buttons under the screen as before */
    touch(step_pad(3)); assert(!l->steps[3].on); touch(step_pad(3)); assert(l->steps[3].on);
    nav(TC_NAV_UP); assert(tc_state.lane == (lane + 1u) % LN_LANES); nav(TC_NAV_DOWN); assert(tc_state.lane == lane);
    /* a step held when U1 is pressed: not toggled when let go */
    hold(step_pad(2)); mode_hold(U1, true); lift(step_pad(2)); mode_hold(U1, false);
    assert(l->steps[2].on && !tc_state.step_mask);
    /* 1 .. 64, no wrap */
    mode_hold(U1, true);
    for (uint32_t i = 0; i < LN_STEPS + 4u; ++i) nav(TC_NAV_RIGHT);
    assert(ln_length(l) == LN_STEPS);
    for (uint32_t i = 0; i < LN_STEPS + 4u; ++i) nav(TC_NAV_LEFT);
    assert(ln_length(l) == 1u);
    for (uint32_t i = 1; i < 16u; ++i) nav(TC_NAV_RIGHT);
    mode_hold(U1, false); assert(ln_length(l) == 16u);
    /* U2 held: Direction, a pad changes nothing */
    mode_hold(U2, true); nav(TC_NAV_RIGHT); assert(l->direction == LN_FORWARD + 1u);
    draw(); assert(!strcmp(lines[34], "DIRECTION REVERSE") && !strcmp(lines[56], "<- >+"));
    touch(step_pad(0)); assert(l->steps[0].on && ln_length(l) == 16u);
    for (uint32_t i = 0; i < LN_DIRECTIONS + 1u; ++i) nav(TC_NAV_RIGHT);
    assert(l->direction == LN_DIRECTIONS - 1u);
    for (uint32_t i = 0; i < LN_DIRECTIONS + 1u; ++i) nav(TC_NAV_LEFT);
    mode_hold(U2, false); assert(l->direction == LN_FORWARD);
    /* U3 held (U3 as Scale): Swing in fives; the scale stays */
    mode_hold(TC_U3, true); nav(TC_NAV_RIGHT); nav(TC_NAV_RIGHT); assert(l->swing == 2u * TC_SWING_STEP);
    draw(); assert(!strcmp(lines[34], "SWING 10%"));
    nav(TC_NAV_LEFT); nav(TC_NAV_LEFT); nav(TC_NAV_LEFT); draw(); assert(l->swing == 0u && !strcmp(lines[34], "SWING OFF"));
    mode_hold(TC_U3, false); assert(tc_state.scale == scale);
    /* Shift (a panel button) + U2: Rate (the steps not moved, as Shift + right alone would); + U1: Swing; + U3: free */
    tc_state.buttons[0] = TC_ACT_SHIFT; centre(0, true);
    uint32_t rate = ln_rate(l);
    ln_step before[LN_STEPS]; memcpy(before, l->steps, sizeof before);
    mode_hold(U2, true); nav(TC_NAV_RIGHT); assert(ln_rate(l) == rate + 1u);
    draw(); assert(!strncmp(lines[34], "RATE ", 5u) && !strcmp(lines[34] + 5, ui_rate_names[rate + 1u]));
    mode_hold(U2, false);
    assert(!memcmp(before, l->steps, sizeof before)); /* not rotated */
    mode_hold(U1, true); nav(TC_NAV_RIGHT); assert(l->swing == TC_SWING_STEP); mode_hold(U1, false);
    mode_button(TC_U3); assert(tc_state.last_action == TC_SHIFT_UNUSED && !tc_state.lane_hold);
    centre(0, false);
    /* U3 as Shift: tapped, then U1 held: Swing; U3 held with U1: Swing */
    tc_state.u3_shift = 1u;
    mode_button(TC_U3); mode_hold(U1, true); assert(tc_state.lane_hold == TC_MENU_LANE_SWING);
    nav(TC_NAV_RIGHT); assert(l->swing == 2u * TC_SWING_STEP); mode_hold(U1, false);
    mode_hold(TC_U3, true); mode_hold(U1, true); nav(TC_NAV_LEFT); assert(l->swing == TC_SWING_STEP);
    mode_hold(U1, false); mode_hold(TC_U3, false);
    assert(!tc_state.shift_once && !tc_state.shift_latched);
    tc_state.u3_shift = 0u;
    /* the play screen: U1 is a user scale's button as before */
    tc_state.ui = TC_UI_PLAY; mode_button(U1); assert(!tc_state.lane_hold && tc_state.last_action != TC_LANE_SET);
    /* stored as the Steps page stores them */
    run(4000); boot_keep();
    l = &ln.lanes[0];
    assert(ln_length(l) == 16u && l->direction == LN_FORWARD && l->swing == TC_SWING_STEP && ln_rate(l) == rate + 1u);
    printf("lane settings: U1 length (pads the last step, 1..64), U2 direction, U3 swing, Shift + U1 swing, Shift + U2 rate, "
           "Shift + U3 free, U3 as Shift, taps show, stored; ");
}
int main(void) {
    paste();
    lane_settings();
    lights();
    storage();
    full_patterns();
    burst();
    mute_solo();
    fields();
    patterns();
    follow_loop_fill();
    printf("v24 PASS\n");
    return 0;
}
