/* SPDX-License-Identifier: GPL-3.0-or-later - 1.9.0, the look (Customize Stage 2) on the real code: the pads'
 * colors through ui_lights.c (built-in roles = the colors before 1.9.0, a role's palette color in its place,
 * the four new schemes, Color by), the look record (settings.c on a fake sector 11: the board's stored only
 * once it says more than the live bits, back after a restart, a preset's own look with Keep look, the
 * board's back after a preset without one, compaction), its checks and the SysEx slot for the look on show,
 * and the menu items (Lights > Color by, Presets > Keep look). */
#include "../../src/tb/settings.h"
#include "../../src/tb/sysex.h"
#include "../../src/tb/flash_store.h"
#include "../../src/tb/nor.h"
#include "../../src/tb/ui_internal.h"
#include "../../src/tb/controls_internal.h"
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
static tp_engine engine;
static uint32_t now = 1000u;
static void boot(bool erase) {
    if (erase) memset(flash, 0xFF, sizeof flash);
    memset(&engine, 0, sizeof engine);
    tc_init(&tc_state); (void)st_load(&tc_state); engine.layout = tc_layout(&tc_state);
}
static void show(void) { ui_pad_colors(&tc_state, &engine, now); }
static void settle(void) { now += 10u; st_service(&tc_state, now); now += ST_SAVE_DELAY_MS + 10u; st_service(&tc_state, now); }
static bool same(tl_rgb a, tl_rgb b) { return a.r == b.r && a.g == b.g && a.b == b.b; }
/* The pad k (its own index, not where it lights) and what it shows. */
static tl_rgb at(uint32_t k) { return pads[tc_physical(&tc_state, k)]; }
static uint32_t find(uint8_t side, uint8_t column, uint8_t row) {
    for (uint32_t k = 0; k < TL_PADS; ++k) { tp_position p; (void)tp_position_of(k, &p); if (p.side == side && p.column == column && p.row == row) return k; }
    assert(0); return 0;
}
static tl_rgb scaled(tl_rgb h, uint32_t percent) {
    uint32_t max = 74u * percent / 100u; /* brightness 8 */
    return ui_scaled(h.r, h.g, h.b, max);
}
/* Built-in roles are the colors before 1.9.0; each role taking the palette color equal to its own changes
 * nothing; a role made Off turns its pads (nearly) dark. */
static void builtin(void) {
    for (uint8_t scheme = 0; scheme < TC_LED_SCHEMES; ++scheme) {
        boot(true); tc_state.brightness = 7u; tc_state.led_scheme = scheme; tc_state.tonic = 2u;
        engine.layout = tc_layout(&tc_state); show();
        tl_rgb before[TL_PADS]; memcpy(before, pads, sizeof pads);
        for (uint32_t r = tc_look_first[scheme]; r < (uint32_t)tc_look_first[scheme] + tc_look_count[scheme]; ++r)
            for (uint8_t v = 1; v <= TC_PALETTE; ++v) if (same(ui_palette[v - 1u], ui_look_builtin[r])) tc_state.look_roles[r] = v;
        show(); assert(!memcmp(before, pads, sizeof pads));
    }
    /* The three schemes before 1.9.0, by hand: D major, Default, Root+5th, Rainbow. */
    boot(true); tc_state.brightness = 7u; tc_state.tonic = 2u; engine.layout = tc_layout(&tc_state);
    uint32_t root = find(0, 0, 3), second = find(0, 1, 3), fifth = find(0, 4, 3);
    show(); assert(same(at(root), scaled(ui_hues[2], 100)) && same(at(second), scaled(ui_hues[4], 60)));
    tc_state.led_scheme = TC_LEDS_ROOT_FIFTH; show();
    assert(same(at(root), scaled((tl_rgb){255, 70, 0}, 100)) && same(at(fifth), scaled((tl_rgb){0, 90, 255}, 100)) &&
           same(at(second), scaled((tl_rgb){40, 40, 50}, 100)));
    tc_state.led_scheme = TC_LEDS_ALL_RAINBOW; show();
    assert(same(at(root), scaled(ui_hues[0], 100)) && same(at(fifth), scaled(ui_hues[8], 60)));
    /* A role recolored: Root+5th's root Off (dark, never quite off: blue 1), Rainbow's column 1 white. */
    tc_state.led_scheme = TC_LEDS_ROOT_FIFTH; tc_state.look_roles[TC_LOOK_R5 + TC_R5_ROOT] = TC_PALETTE; show();
    assert(at(root).r == 0u && at(root).g == 0u && at(root).b == 1u);
    tc_state.led_scheme = TC_LEDS_ALL_RAINBOW; tc_state.look_roles[TC_LOOK_COLUMNS] = 13u; show();
    assert(same(at(root), scaled((tl_rgb){255, 255, 255}, 100)) && same(at(find(0, 7, 3)), scaled((tl_rgb){255, 255, 255}, 60)));
    puts("built-in: each scheme's built-in roles are its colors before 1.9.0; a role takes its palette color");
}
/* Color by: Default in D, by note D is D's hue (orange-yellow), by degree the root is the first hue (red). */
static void color_by(void) {
    boot(true); tc_state.brightness = 7u; tc_state.tonic = 2u; engine.layout = tc_layout(&tc_state);
    uint32_t root = find(0, 0, 3);
    show(); assert(same(at(root), scaled(ui_hues[2], 100)));
    tc_state.color_by = TC_COLOR_BY_DEGREE; show(); assert(same(at(root), scaled(ui_hues[0], 100)));
    tc_state.look_roles[TC_LOOK_NOTES] = 13u; show(); assert(same(at(root), scaled((tl_rgb){255, 255, 255}, 100)));
    puts("color by: note name or scale degree; the note roles");
}
/* The new schemes on C major with the chromatic melody layout: every pad's role. */
static void schemes(void) {
    boot(true); tc_state.brightness = 7u; tc_state.melody_layout = TP_MELODY_CHROMATIC; engine.layout = tc_layout(&tc_state);
    uint32_t c_chord = find(0, 0, 3), e_chord = find(0, 2, 3), g_chord = find(0, 4, 3), f_chord = find(0, 3, 3);
    uint32_t c_sharp = find(1, 1, 3), d = find(1, 2, 3);
    tc_state.led_scheme = TC_LEDS_PIANO; show();
    assert(same(at(c_chord), scaled(ui_look_builtin[TC_LOOK_PIANO + TC_PIANO_ROOT], 100)));
    assert(same(at(d), scaled(ui_look_builtin[TC_LOOK_PIANO + TC_PIANO_NATURAL], 100)));
    assert(same(at(c_sharp), scaled(ui_look_builtin[TC_LOOK_PIANO + TC_PIANO_SHARP], 10))); /* not in C major: 10 % */
    tc_state.led_scheme = TC_LEDS_TRIAD; show();
    assert(same(at(c_chord), scaled(ui_look_builtin[TC_LOOK_TRIAD + TC_TRIAD_ROOT], 100)) &&
           same(at(e_chord), scaled(ui_look_builtin[TC_LOOK_TRIAD + TC_TRIAD_THIRD], 100)) &&
           same(at(g_chord), scaled(ui_look_builtin[TC_LOOK_TRIAD + TC_TRIAD_FIFTH], 100)) &&
           same(at(f_chord), scaled(ui_look_builtin[TC_LOOK_TRIAD + TC_TRIAD_OTHER], 35)));
    tc_state.led_scheme = TC_LEDS_FUNCTION; show();
    assert(same(at(c_chord), scaled(ui_look_builtin[TC_LOOK_FUNCTION + TC_FUNCTION_TONIC], 100)) &&
           same(at(f_chord), scaled(ui_look_builtin[TC_LOOK_FUNCTION + TC_FUNCTION_SUB], 70)) &&
           same(at(g_chord), scaled(ui_look_builtin[TC_LOOK_FUNCTION + TC_FUNCTION_DOMINANT], 70)) &&
           same(at(e_chord), scaled(ui_look_builtin[TC_LOOK_FUNCTION + TC_FUNCTION_TONIC], 70)));
    tc_state.led_scheme = TC_LEDS_OCTAVES; show();
    assert(same(at(c_chord), scaled(ui_look_builtin[TC_LOOK_OCTAVES + TC_OCTAVES_ROOT], 100)) &&
           same(at(d), scaled(ui_look_builtin[TC_LOOK_OCTAVES + TC_OCTAVES_OTHER], 30)));
    /* Every scheme, key and both color-bys: every pad lit (never black) and inside the brightness. */
    for (uint8_t scheme = 0; scheme < TC_LED_SCHEMES; ++scheme) for (uint8_t key = 0; key < 12u; ++key) for (uint8_t by = 0; by < 2u; ++by) {
        tc_state.led_scheme = scheme; tc_state.tonic = key; tc_state.color_by = by; engine.layout = tc_layout(&tc_state); show();
        for (uint32_t k = 0; k < TL_PADS; ++k) assert((pads[k].r || pads[k].g || pads[k].b) && pads[k].r <= 74u && pads[k].g <= 74u && pads[k].b <= 74u);
    }
    puts("schemes: Piano, Triad, Function, Octaves give each note its role; every pad lit in every scheme and key");
}
static bool stored(uint8_t slot) { uint32_t p[ST_PAYLOAD]; return st_get(ST_LOOK, slot, &tc_state, p) == ST_OK; }
static void set_menu(uint8_t item, uint8_t value) {
    tc_state.ui = TC_UI_EDIT; tc_state.menu = item; tc_state.edit_value = value; tc_state.nav_held = 0u;
    tc_navigate(&tc_state, &engine, TC_NAV_ENTER); tc_state.ui = TC_UI_PLAY;
}
static void storage(void) {
    /* A scheme the live bits hold: nothing more stored; a restart keeps it. */
    boot(true); set_menu(TC_MENU_COLORS, TC_LEDS_ROOT_FIFTH); settle();
    assert(!stored(0)); boot(false); assert(tc_state.led_scheme == TC_LEDS_ROOT_FIFTH);
    /* Piano, Color by Degree, a role: the board's look stored; the live bits say Default; a restart keeps it. */
    set_menu(TC_MENU_COLORS, TC_LEDS_PIANO); set_menu(TC_MENU_COLOR_BY, TC_COLOR_BY_DEGREE);
    tc_state.look_roles[TC_LOOK_PIANO] = 5u; tc_state.changes++; settle();
    assert(stored(0));
    uint32_t live[ST_PAYLOAD]; assert(st_get(ST_LIVE, 0, &tc_state, live) == ST_OK && !((live[4] >> 29) & 3u));
    boot(false);
    assert(tc_state.led_scheme == TC_LEDS_PIANO && tc_state.color_by == TC_COLOR_BY_DEGREE && tc_state.look_roles[TC_LOOK_PIANO] == 5u);
    /* The editor writes the live settings back (its bits Default): the scheme stays Piano. */
    assert(st_put(ST_LIVE, 0, live, &tc_state, &engine) == ST_OK && tc_state.led_scheme == TC_LEDS_PIANO);
    /* Bits that do change it change it. */
    live[4] = (live[4] & ~(3u << 29)) | (uint32_t)TC_LEDS_ALL_RAINBOW << 29;
    assert(st_put(ST_LIVE, 0, live, &tc_state, &engine) == ST_OK && tc_state.led_scheme == TC_LEDS_ALL_RAINBOW);
    set_menu(TC_MENU_COLORS, TC_LEDS_PIANO); settle();
    puts("storage: the board's look only when the live bits can't say it, kept over a restart and a live record");
}
static void presets(void) {
    boot(true); set_menu(TC_MENU_COLORS, TC_LEDS_TRIAD); settle();
    /* Keep look: preset 3 saved with Triad; then the board goes Function. */
    set_menu(TC_MENU_KEEP_LOOK, 1u); tc_request(&tc_state, TC_REQUEST_SAVE, 2u); st_requests(&tc_state, &engine);
    assert(stored(3) && tc_state.last_action == TC_PRESET_SAVED);
    set_menu(TC_MENU_KEEP_LOOK, 0u); tc_request(&tc_state, TC_REQUEST_SAVE, 0u); st_requests(&tc_state, &engine); assert(!stored(1));
    set_menu(TC_MENU_COLORS, TC_LEDS_FUNCTION); settle();
    /* Loading preset 3 shows its look; preset 1 has none: the board's (Function) comes back. */
    tc_request(&tc_state, TC_REQUEST_LOAD, 2u); st_requests(&tc_state, &engine);
    assert(tc_state.led_scheme == TC_LEDS_TRIAD && tc_state.look_slot == 3u);
    /* Changed while it shows: preset 3's look changes, not the board's. */
    tc_state.look_roles[TC_LOOK_TRIAD] = 7u; tc_state.changes++; settle();
    uint32_t p[ST_PAYLOAD]; assert(st_get(ST_LOOK, 3, &tc_state, p) == ST_OK && (p[7] >> 24) == 7u);
    assert(st_get(ST_LOOK, 0, &tc_state, p) == ST_OK && ((p[0] >> 8) & 0xFFu) == TC_LEDS_FUNCTION);
    tc_request(&tc_state, TC_REQUEST_LOAD, 0u); st_requests(&tc_state, &engine);
    assert(tc_state.led_scheme == TC_LEDS_FUNCTION && tc_state.look_slot == 0u && !tc_state.look_roles[TC_LOOK_TRIAD]);
    /* Saving preset 3 again without Keep look clears its look. */
    tc_request(&tc_state, TC_REQUEST_LOAD, 2u); st_requests(&tc_state, &engine); assert(tc_state.look_slot == 3u);
    tc_request(&tc_state, TC_REQUEST_SAVE, 2u); st_requests(&tc_state, &engine);
    assert(!stored(3) && tc_state.look_slot == 0u && tc_state.led_scheme == TC_LEDS_FUNCTION);
    /* A restart: the board's look. */
    boot(false); assert(tc_state.led_scheme == TC_LEDS_FUNCTION && tc_state.look_slot == 0u);
    /* Even when the board's said nothing more than the live bits before a preset's look showed. */
    boot(true); set_menu(TC_MENU_COLORS, TC_LEDS_ROOT_FIFTH); set_menu(TC_MENU_KEEP_LOOK, 1u); settle();
    uint32_t own[ST_PAYLOAD] = {1u | (uint32_t)TC_LEDS_OCTAVES << 8};
    assert(st_put(ST_LOOK, 5, own, &tc_state, &engine) == ST_OK && stored(0));
    tc_request(&tc_state, TC_REQUEST_SAVE, 4u); st_requests(&tc_state, &engine); /* preset 5, Keep look: Root+5th */
    assert(st_put(ST_LOOK, 5, own, &tc_state, &engine) == ST_OK);
    tc_request(&tc_state, TC_REQUEST_LOAD, 4u); st_requests(&tc_state, &engine); assert(tc_state.led_scheme == TC_LEDS_OCTAVES);
    settle(); boot(false); assert(tc_state.led_scheme == TC_LEDS_ROOT_FIFTH && tc_state.keep_look == 1u);
    puts("presets: Keep look saves the look with a preset; loading shows it, one without brings the board's back; "
         "changes while it shows go to the preset's; saving without Keep look clears it");
}
static void records(void) {
    boot(true);
    uint32_t p[ST_PAYLOAD];
    /* The look on show (slot 9): read, changed live, refused when wrong. */
    assert(st_get(ST_LOOK, ST_LOOK_NOW, &tc_state, p) == ST_OK && p[0] == 1u && !p[ST_PAYLOAD - 1u]);
    p[0] = 1u | (uint32_t)TC_LEDS_TRIAD << 8 | 1u << 16 | 1u << 24; p[1] = 16u;
    assert(st_put(ST_LOOK, ST_LOOK_NOW, p, &tc_state, &engine) == ST_OK);
    assert(tc_state.led_scheme == TC_LEDS_TRIAD && tc_state.color_by == 1u && tc_state.keep_look == 1u && tc_state.look_roles[0] == 16u);
    static const uint32_t bad[][2] = {{0, 0}, {0, 1u | 7u << 8}, {0, 1u | 2u << 16}, {0, 2u}, {1, 17u}, {10, 1u}, {20, 1u}, {39, 9u}};
    for (uint32_t i = 0; i < sizeof bad / sizeof bad[0]; ++i) {
        uint32_t q[ST_PAYLOAD] = {1u}; q[bad[i][0]] = bad[i][1];
        tc_controls before = tc_state;
        assert(st_put(ST_LOOK, ST_LOOK_NOW, q, &tc_state, &engine) == ST_BAD_VALUE && !memcmp(&before, &tc_state, sizeof before));
    }
    uint32_t keep[ST_PAYLOAD] = {1u | 1u << 24};
    assert(st_put(ST_LOOK, 1, keep, &tc_state, &engine) == ST_BAD_VALUE); /* Keep look: the board's only */
    assert(st_put(ST_LOOK, 0, keep, &tc_state, &engine) == ST_OK);
    assert(st_put(ST_LOOK, TC_LOOKS + 1u, keep, &tc_state, &engine) == ST_BAD_VALUE && st_get(ST_LOOK, TC_LOOKS + 1u, &tc_state, p) == ST_BAD_VALUE);
    /* Cleared: absent again. */
    uint32_t zero[ST_PAYLOAD] = {0}; assert(st_put(ST_LOOK, 0, zero, &tc_state, &engine) == ST_OK && !stored(0));
    /* Through SysEx: a quiet record to slot 9 changes the pads without a reply. */
    uint8_t bytes[4u * ST_PAYLOAD], data[3u + 200u];
    uint32_t q[ST_PAYLOAD] = {1u | (uint32_t)TC_LEDS_OCTAVES << 8};
    for (uint32_t i = 0; i < ST_PAYLOAD; ++i) for (uint32_t k = 0; k < 4u; ++k) bytes[4u * i + k] = (uint8_t)(q[i] >> (8u * k));
    data[0] = ST_LOOK; data[1] = ST_LOOK_NOW; data[2] = SX_QUIET;
    uint32_t n = 3u + sx_pack(bytes, sizeof bytes, &data[3]);
    uint8_t msg[300]; uint32_t len = 0, sum = SX_SET_RECORD;
    msg[len++] = 0xF0; msg[len++] = 0x7D; msg[len++] = 0x54; msg[len++] = 0x42; msg[len++] = SX_SET_RECORD;
    for (uint32_t i = 0; i < n; ++i) { msg[len++] = data[i]; sum += data[i]; }
    msg[len++] = (uint8_t)((128u - sum % 128u) % 128u); msg[len++] = 0xF7;
    assert(sx_handle(msg, len, &tc_state, &engine, now) == 0u && tc_state.led_scheme == TC_LEDS_OCTAVES);
    puts("records: the look on show read and changed (also quietly over SysEx), wrong ones refused, Keep look only the board's, cleared");
}
/* Compaction keeps every look. */
static void compaction(void) {
    boot(true);
    uint32_t p[ST_PAYLOAD] = {1u | (uint32_t)TC_LEDS_PIANO << 8, 3u};
    for (uint8_t s = 0; s < TC_LOOKS; ++s) assert(st_put(ST_LOOK, s, p, &tc_state, &engine) == ST_OK);
    uint32_t live[ST_PAYLOAD]; assert(st_get(ST_LIVE, 0, &tc_state, live) == ST_OK);
    while (st_records() < FS_SIZE / (4u * (4u + ST_PAYLOAD)) / 2u + 2u) assert(st_put(ST_PRESET, 0, live, &tc_state, &engine) == ST_OK);
    uint32_t before = st_records();
    boot(false);
    assert(st_records() < before);
    for (uint8_t s = 0; s < TC_LOOKS; ++s) { uint32_t q[ST_PAYLOAD]; assert(st_get(ST_LOOK, s, &tc_state, q) == ST_OK && !memcmp(p, q, sizeof p)); }
    assert(tc_state.led_scheme == TC_LEDS_PIANO && tc_state.look_roles[0] == 3u);
    puts("compaction: every look kept");
}
static void menu(void) {
    boot(true);
    assert(tc_menu_values(TC_MENU_COLORS) == TC_LED_SCHEMES && tc_menu_values(TC_MENU_COLOR_BY) == 2u && tc_menu_values(TC_MENU_KEEP_LOOK) == 2u);
    assert(!strcmp(ui_value_text(TC_MENU_COLORS, TC_LEDS_FUNCTION), "FUNCTION") && !strcmp(ui_value_text(TC_MENU_COLOR_BY, 1u), "DEGREE"));
    tc_state.color_by = 1u; tc_state.led_scheme = TC_LEDS_PIANO; tc_state.group = TC_GROUP_LIGHTS;
    tc_state.look_roles[3] = 4u;
    tc_state.ui = TC_UI_EDIT; tc_state.menu = TC_MENU_RESET_PAGE; tc_state.edit_value = 1u;
    tc_navigate(&tc_state, &engine, TC_NAV_ENTER);
    assert(tc_state.led_scheme == 0u && tc_state.color_by == 0u && tc_state.look_roles[3] == 4u); /* the roles stay */
    puts("menu: Colors' seven schemes, Color by, Keep look; Reset options on Lights leaves the roles");
}
int main(void) {
    builtin(); color_by(); schemes(); storage(); presets(); records(); compaction(); menu();
    puts("PASS: the look (1.9.0): built-in = before, roles, schemes, Color by, stored, presets' own, records, compaction, menu");
    return 0;
}
