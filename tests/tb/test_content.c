/* SPDX-License-Identifier: GPL-3.0-or-later - 1.9.3, the content editors (Customize Stage 4) on the real
 * settings.c, sysex.c and screens: preset names (their record, its checks, Load preset and Save preset showing
 * them, a restart, compaction), a preset cleared by a record of zeros, each preset's own curves with Keep feel
 * (as the looks: loading uses them, a preset without brings the board's back, changes go where they belong, the
 * records' checks), the board's look saved before a preset's covers it, the step grid's quiet lane records
 * (played at once, saved once 3 s after the last), and the SysEx requests VALUES (every item as loading the
 * preset would set it), SET_EDITS (the chord pads' edits, all or nothing, no reply) and CHORD_NAMES. */
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
bool tl_init(void) { return true; }
void tl_set_pad(uint32_t pad, tl_rgb c) { (void)pad; (void)c; }
void tl_set_button(uint32_t channel, uint16_t level) { (void)channel; (void)level; }
void tl_service(void) {}
uint32_t jh_levels(void) { return 5u; }
bool td_init(void) { return true; }
void td_pixel(uint32_t x, uint32_t y, bool on) { (void)x; (void)y; (void)on; }
void td_invert(uint32_t x0, uint32_t y0, uint32_t x1, uint32_t y1) { (void)x0; (void)y0; (void)x1; (void)y1; }
void td_service(void) {}
void td_clear(void) {}
/* The texts drawn on one row of the screen (watch_y), each with where it starts. */
static uint32_t watch_y = 11u, drawn;
static struct { uint32_t x; char text[32]; } row[16];
uint32_t td_text(uint32_t x, uint32_t y, const char *text, uint32_t scale) {
    if (y == watch_y && drawn < 16u) { row[drawn].x = x; strncpy(row[drawn].text, text, 31); row[drawn].text[31] = 0; drawn++; }
    return x + 6u * scale * (uint32_t)strlen(text);
}
static tp_engine engine;
static ts_scan scan;
static uint32_t now = 1000u;
static uint8_t msg[400];
static void boot(bool erase) {
    if (erase) memset(flash, 0xFF, sizeof flash);
    memset(&engine, 0, sizeof engine);
    tc_init(&tc_state); (void)st_load(&tc_state); engine.layout = tc_layout(&tc_state);
}
static void settle(void) { now += 10u; st_service(&tc_state, now); now += ST_SAVE_DELAY_MS + 10u; st_service(&tc_state, now); }
static void save_preset(uint8_t slot) { tc_request(&tc_state, TC_REQUEST_SAVE, slot); st_requests(&tc_state, &engine); }
static void load_preset(uint8_t slot) { tc_request(&tc_state, TC_REQUEST_LOAD, slot); st_requests(&tc_state, &engine); }
static uint32_t request(uint8_t cmd, const uint8_t *data, uint32_t n) {
    uint32_t len = 0, sum = cmd;
    msg[len++] = 0xF0; msg[len++] = 0x7D; msg[len++] = 0x54; msg[len++] = 0x42; msg[len++] = cmd;
    for (uint32_t i = 0; i < n; ++i) { msg[len++] = data[i]; sum += data[i]; }
    msg[len++] = (uint8_t)((128u - sum % 128u) % 128u); msg[len++] = 0xF7;
    return sx_handle(msg, len, &tc_state, &engine, now);
}
static const uint8_t *ok(uint32_t len, uint8_t cmd) {
    assert(len >= 8u && sx_out[4] == (cmd | 0x40u) && sx_out[len - 1u] == 0xF7 && sx_out[5] == SX_OK);
    uint32_t sum = 0;
    for (uint32_t i = 4; i < len - 1u; ++i) { assert(sx_out[i] < 0x80u); sum += sx_out[i]; }
    assert(sum % 128u == 0u);
    return &sx_out[6];
}
static uint32_t two(const uint8_t *d) { return d[0] | (uint32_t)d[1] << 7; }
/* SET_RECORD (quiet: no reply) of kind/slot. */
static uint32_t set_record(uint8_t kind, uint8_t slot, const uint32_t p[ST_PAYLOAD], bool quiet) {
    uint8_t bytes[4u * ST_PAYLOAD], data[3u + 4u * ST_PAYLOAD + 23u] = {kind, slot, quiet ? SX_QUIET : 0u};
    for (uint32_t i = 0; i < ST_PAYLOAD; ++i) for (uint32_t k = 0; k < 4u; ++k) bytes[4u * i + k] = (uint8_t)(p[i] >> (8u * k));
    uint32_t n = sx_pack(bytes, sizeof bytes, &data[3]);
    return request(SX_SET_RECORD, data, 3u + n);
}
static void names_record(const char *const names[TC_PRESETS], uint32_t p[ST_PAYLOAD]) {
    memset(p, 0, ST_PAYLOAD * 4u); p[0] = 1u;
    for (uint32_t n = 0; n < TC_PRESETS; ++n)
        for (uint32_t i = 0; names[n] && names[n][i]; ++i) p[1u + (12u * n + i) / 4u] |= (uint32_t)(uint8_t)names[n][i] << (8u * ((12u * n + i) % 4u));
}
static void draw(void) { drawn = 0; tc_state.event_sequence++; ui_service(&tc_state, &engine, &scan, true, now); }
static void names(void) {
    boot(true);
    uint32_t p[ST_PAYLOAD], q[ST_PAYLOAD];
    assert(st_get(ST_NAMES, 0, &tc_state, p) == ST_ABSENT && p[0] == 1u && !p[1]);
    save_preset(0); save_preset(3);
    const char *const list[TC_PRESETS] = {"MY BALLAD 12", NULL, "EMPTY ONE", "FUNK", NULL, NULL, NULL, "LAST"};
    names_record(list, p);
    uint32_t before = st_records();
    assert(st_put(ST_NAMES, 0, p, &tc_state, &engine) == ST_OK && st_records() == before + 1u);
    assert(!strcmp(tc_preset_names[0], "MY BALLAD 12") && !strcmp(tc_preset_names[3], "FUNK") && !tc_preset_names[1][0]);
    assert(st_get(ST_NAMES, 0, &tc_state, q) == ST_OK && !memcmp(p, q, sizeof p));
    /* Load preset and Save preset name them; an empty one stays EMPTY, named or not. */
    assert(!strcmp(ui_value_text(TC_MENU_LOAD, 0), "1 MY BALLAD 12") && !strcmp(ui_value_text(TC_MENU_SAVE, 3), "4 FUNK"));
    assert(!strcmp(ui_value_text(TC_MENU_LOAD, 2), "3 EMPTY") && !strcmp(ui_value_text(TC_MENU_LOAD, 1), "2 EMPTY"));
    /* Refused, nothing changed: a slot, format 2, a character below space or past ~, one after the name's end, a
     * stray word, characters in a record of zeros' format. */
    uint32_t bad[ST_PAYLOAD];
    memcpy(bad, p, sizeof bad); assert(st_put(ST_NAMES, 1, bad, &tc_state, &engine) == ST_BAD_VALUE);
    memcpy(bad, p, sizeof bad); bad[0] = 2u; assert(st_put(ST_NAMES, 0, bad, &tc_state, &engine) == ST_BAD_VALUE);
    memcpy(bad, p, sizeof bad); bad[1] = (bad[1] & ~0xFFu) | 0x1Fu; assert(st_put(ST_NAMES, 0, bad, &tc_state, &engine) == ST_BAD_VALUE);
    memcpy(bad, p, sizeof bad); bad[1] = (bad[1] & ~0xFFu) | 0x7Fu; assert(st_put(ST_NAMES, 0, bad, &tc_state, &engine) == ST_BAD_VALUE);
    memcpy(bad, p, sizeof bad); bad[4] |= 0x41u << 8; assert(st_put(ST_NAMES, 0, bad, &tc_state, &engine) == ST_BAD_VALUE); /* "B" after none */
    memcpy(bad, p, sizeof bad); bad[25] = 1u; assert(st_put(ST_NAMES, 0, bad, &tc_state, &engine) == ST_BAD_VALUE);
    memcpy(bad, p, sizeof bad); bad[0] = 0u; assert(st_put(ST_NAMES, 0, bad, &tc_state, &engine) == ST_BAD_VALUE);
    assert(!strcmp(tc_preset_names[0], "MY BALLAD 12"));
    /* Over SysEx too; after a restart; through compaction; a record of zeros: none named. */
    assert(set_record(ST_NAMES, 0, p, true) == 0u);
    boot(false); assert(!strcmp(tc_preset_names[7], "LAST") && !strcmp(tc_preset_names[2], "EMPTY ONE"));
    uint32_t live[ST_PAYLOAD]; assert(st_get(ST_LIVE, 0, &tc_state, live) == ST_OK);
    while (st_records() < FS_SIZE / (4u * (4u + ST_PAYLOAD)) / 2u + 2u) assert(st_put(ST_PRESET, 5, live, &tc_state, &engine) == ST_OK);
    boot(false); assert(st_records() < 20u && !strcmp(tc_preset_names[0], "MY BALLAD 12") && !strcmp(tc_preset_names[3], "FUNK"));
    memset(q, 0, sizeof q);
    assert(st_put(ST_NAMES, 0, q, &tc_state, &engine) == ST_OK && !tc_preset_names[0][0] && st_get(ST_NAMES, 0, &tc_state, q) == ST_ABSENT);
    boot(false); assert(!tc_preset_names[7][0]);
    printf("names: stored, read, on Load preset and Save preset, refused when wrong, kept over a restart and compaction, cleared\n");
}
/* The Presets page's Load preset row with a name too long to fit beside it. */
static void screen(void) {
    boot(true); save_preset(0);
    const char *const list[TC_PRESETS] = {"MY BALLAD 12", NULL, NULL, NULL, NULL, NULL, NULL, NULL};
    uint32_t p[ST_PAYLOAD]; names_record(list, p); assert(st_put(ST_NAMES, 0, p, &tc_state, &engine) == ST_OK);
    tc_state.ui = TC_UI_MENU; tc_state.group = TC_GROUP_PRESETS; tc_state.menu = TC_MENU_LOAD; tc_state.preset = 0u;
    watch_y = 11u; draw();
    /* Not choosing: the item and as much of the value as fits beside it. */
    uint32_t label = 99u, value = 99u;
    for (uint32_t i = 0; i < drawn; ++i) {
        if (!strcmp(row[i].text, "LOAD PRESET")) label = i;
        if (row[i].text[0] == '1') value = i;
    }
    assert(label < drawn && value < drawn && row[label].x == 8u);
    assert(!strcmp(row[value].text, "1 MY BAL") && row[value].x >= 8u + 6u * 11u + 6u && row[value].x + 6u * 8u <= 128u);
    /* Choosing: the whole row is the value. */
    tc_state.ui = TC_UI_EDIT; tc_state.edit_value = 0u; draw();
    label = value = 99u;
    for (uint32_t i = 0; i < drawn; ++i) {
        if (!strcmp(row[i].text, "LOAD PRESET")) label = i;
        if (row[i].text[0] == '1') value = i;
    }
    assert(label == 99u && value < drawn && !strcmp(row[value].text, "1 MY BALLAD 12") && row[value].x == 128u - 6u * 14u);
    /* Keep feel is on the Presets page, after Keep look (2.0.0: then Keep chords; 2.0.7: Clear preset after Save). */
    uint8_t items[TC_GROUP_MAX]; uint8_t n = tc_page_items(&tc_state, TC_GROUP_PRESETS, items);
    assert(n == 6u && items[2] == TC_MENU_CLEAR_PRESET && items[3] == TC_MENU_KEEP_LOOK && items[4] == TC_MENU_KEEP_FEEL &&
           tc_menu_values(TC_MENU_KEEP_FEEL) == 2u && items[5] == TC_MENU_KEEP_CHORDS);
    assert(!strcmp(ui_menu_items[TC_MENU_KEEP_FEEL], "KEEP FEEL") && tc_item_kind(TC_MENU_KEEP_FEEL) == TC_ITEM_VALUE);
    assert(tc_set_item(&tc_state, &engine, TC_MENU_KEEP_FEEL, 1u) && tc_state.keep_feel == 1u && tc_menu_value(&tc_state, TC_MENU_KEEP_FEEL) == 1u);
    tc_state.ui = TC_UI_PLAY;
    printf("screen: a long name cut short beside Load preset, whole while choosing; Keep feel on the Presets page\n");
}
static void cleared(void) {
    boot(true);
    save_preset(2); save_preset(4); assert(tc_presets_present == 0x14u);
    uint32_t zero[ST_PAYLOAD] = {0}, p[ST_PAYLOAD];
    assert(st_put(ST_PRESET, 2, zero, &tc_state, &engine) == ST_OK && tc_presets_present == 0x10u);
    assert(st_get(ST_PRESET, 2, &tc_state, p) == ST_ABSENT && st_get(ST_PRESET, 4, &tc_state, p) == ST_OK);
    load_preset(2); assert(tc_state.last_action == TC_PRESET_EMPTY);
    boot(false); assert(tc_presets_present == 0x10u);
    assert(set_record(ST_PRESET, 4, zero, true) == 0u && tc_presets_present == 0u);
    assert(set_record(ST_LIVE, 0, zero, false) && sx_out[5] == SX_BAD_VALUE); /* the live settings can't be cleared */
    /* Compaction leaves cleared presets out. */
    save_preset(6);
    uint32_t live[ST_PAYLOAD]; assert(st_get(ST_LIVE, 0, &tc_state, live) == ST_OK);
    while (st_records() < FS_SIZE / (4u * (4u + ST_PAYLOAD)) / 2u + 2u) assert(st_put(ST_PRESET, 4, zero, &tc_state, &engine) == ST_OK);
    boot(false); assert(tc_presets_present == 0x40u && st_records() < 10u);
    printf("cleared: a record of zeros clears a preset (absent, empty to load), kept over a restart, left out by compaction\n");
}
static bool same_feel(const uint8_t *a, const uint8_t *b) { return !memcmp(a, b, TC_FEEL_BYTES); }
static void feels(void) {
    boot(true);
    uint8_t plain[TC_FEEL_BYTES], mine[TC_FEEL_BYTES], board[TC_FEEL_BYTES];
    tc_feel_default(plain);
    memcpy(mine, plain, sizeof mine); for (uint32_t i = 0; i < TC_CURVE_POINTS; ++i) mine[i] = (uint8_t)(i * i * 127u / 49u);
    memcpy(board, plain, sizeof board); board[TC_FEEL_AT(1) + 2u] = 90u;
    uint32_t p[ST_PAYLOAD], q[ST_PAYLOAD];
    /* The board's curves, then preset 2 saved with Keep feel: its own curves. */
    memcpy(tc_state.feel, board, sizeof board); tc_state.changes++; settle();
    assert(st_get(ST_FEEL, 2, &tc_state, p) == ST_ABSENT);
    tc_state.keep_feel = 1u; memcpy(tc_state.feel, mine, sizeof mine); tc_state.changes++;
    save_preset(1); settle();
    assert(st_get(ST_FEEL, 2, &tc_state, p) == ST_OK && p[0] == 1u && (uint8_t)p[1] == 0u && (uint8_t)(p[1] >> 8) == 2u);
    /* Saved with Keep feel: the board's curves are these now too (they were in use). Back to other board curves. */
    memcpy(tc_state.feel, board, sizeof board); tc_state.changes++; settle();
    save_preset(3); /* Keep feel on: preset 4 gets the board's curves */
    tc_state.keep_feel = 0u; tc_state.changes++; settle();
    save_preset(5); /* without: preset 6 has none */
    assert(st_get(ST_FEEL, 6, &tc_state, p) == ST_ABSENT);
    /* Loading preset 2 uses its curves; preset 6 brings the board's back; preset 4 its own (= the board's). */
    load_preset(1); assert(same_feel(tc_state.feel, mine) && tc_state.feel_slot == 2u);
    load_preset(5); assert(same_feel(tc_state.feel, board) && tc_state.feel_slot == 0u);
    load_preset(1); assert(tc_state.feel_slot == 2u);
    /* While preset 2's show, changes go to them (with the settings), not to the board's; Keep feel to the board's. */
    tc_state.feel[3] = 5u; tc_state.keep_feel = 1u; tc_state.changes++; settle();
    assert(st_get(ST_FEEL, 2, &tc_state, p) == ST_OK && (uint8_t)(p[1] >> 24) == 5u);
    assert(st_get(ST_FEEL, 0, &tc_state, q) == ST_OK && q[0] == (1u | 1u << 8));
    uint8_t got[TC_FEEL_BYTES]; for (uint32_t i = 0; i < TC_FEEL_BYTES; ++i) got[i] = (uint8_t)(q[1u + i / 4u] >> (8u * (i % 4u)));
    assert(same_feel(got, board));
    /* Slot 9: the curves in use, Keep feel, whose. */
    assert(st_get(ST_FEEL, ST_FEEL_NOW, &tc_state, p) == ST_OK && p[0] == (1u | 1u << 8) && p[ST_PAYLOAD - 1u] == 2u && (uint8_t)(p[1] >> 24) == 5u);
    /* After a restart the board's are in use (as the look), Keep feel kept. */
    boot(false); assert(same_feel(tc_state.feel, board) && tc_state.feel_slot == 0u && tc_state.keep_feel == 1u);
    /* A preset's own set over SysEx while in use: in use at once; cleared: the board's again. */
    load_preset(1);
    memset(q, 0, sizeof q); q[0] = 1u;
    for (uint32_t i = 0; i < TC_FEEL_BYTES; ++i) q[1u + i / 4u] |= (uint32_t)plain[i] << (8u * (i % 4u));
    assert(set_record(ST_FEEL, 2, q, true) == 0u && same_feel(tc_state.feel, plain) && tc_state.feel_slot == 2u);
    memset(q, 0, sizeof q);
    assert(st_put(ST_FEEL, 2, q, &tc_state, &engine) == ST_OK && same_feel(tc_state.feel, board) && tc_state.feel_slot == 0u);
    assert(st_get(ST_FEEL, 2, &tc_state, p) == ST_ABSENT);
    /* Slot 9 set: in use (the board's here), Keep feel with it; whose (word 39) only read. */
    memset(q, 0, sizeof q); q[0] = 1u;
    for (uint32_t i = 0; i < TC_FEEL_BYTES; ++i) q[1u + i / 4u] |= (uint32_t)mine[i] << (8u * (i % 4u));
    q[ST_PAYLOAD - 1u] = 4u;
    assert(st_put(ST_FEEL, ST_FEEL_NOW, q, &tc_state, &engine) == ST_OK && same_feel(tc_state.feel, mine) && tc_state.keep_feel == 0u);
    settle(); boot(false); assert(same_feel(tc_state.feel, mine));
    /* Refused: Keep feel in a preset's own, whose past 8, Keep feel 2, a record of zeros for the curves in use. */
    uint32_t bad[ST_PAYLOAD];
    memcpy(bad, q, sizeof bad); bad[ST_PAYLOAD - 1u] = 0u; bad[0] |= 1u << 8;
    assert(st_put(ST_FEEL, 3, bad, &tc_state, &engine) == ST_BAD_VALUE);
    memcpy(bad, q, sizeof bad); bad[ST_PAYLOAD - 1u] = 9u; assert(st_put(ST_FEEL, ST_FEEL_NOW, bad, &tc_state, &engine) == ST_BAD_VALUE);
    memcpy(bad, q, sizeof bad); bad[ST_PAYLOAD - 1u] = 0u; bad[0] |= 2u << 8; assert(st_put(ST_FEEL, 0, bad, &tc_state, &engine) == ST_BAD_VALUE);
    memset(bad, 0, sizeof bad); assert(st_put(ST_FEEL, ST_FEEL_NOW, bad, &tc_state, &engine) == ST_BAD_VALUE);
    /* Saving without Keep feel clears a preset's own; with it, the curves in use go to the preset. */
    tc_state.keep_feel = 0u; save_preset(3); assert(st_get(ST_FEEL, 4, &tc_state, p) == ST_ABSENT);
    /* Compaction keeps every preset's own and the board's. */
    memcpy(tc_state.feel, plain, sizeof plain); tc_state.keep_feel = 1u; tc_state.changes++; save_preset(0);
    tc_state.keep_feel = 0u; memcpy(tc_state.feel, mine, sizeof mine); tc_state.changes++; settle();
    uint32_t live[ST_PAYLOAD]; assert(st_get(ST_LIVE, 0, &tc_state, live) == ST_OK);
    while (st_records() < FS_SIZE / (4u * (4u + ST_PAYLOAD)) / 2u + 2u) assert(st_put(ST_PRESET, 7, live, &tc_state, &engine) == ST_OK);
    boot(false); assert(st_records() < 20u && same_feel(tc_state.feel, mine) && !tc_state.keep_feel);
    load_preset(0); assert(same_feel(tc_state.feel, plain) && tc_state.feel_slot == 1u);
    load_preset(1); assert(same_feel(tc_state.feel, mine) && tc_state.feel_slot == 0u);
    printf("feel: Keep feel saves the curves with a preset; loading uses them, one without brings the board's back; "
           "changes go to the preset's; Keep feel the board's; slot 9; set and cleared while in use; refused when wrong; compaction\n");
}
/* The board's look changed and not saved yet when a preset's own look covers it: saved first (1.9.3). */
static void looks(void) {
    boot(true);
    tc_state.led_scheme = TC_LED_SCHEMES_V1; tc_state.changes++; settle();
    tc_state.keep_look = 1u; tc_state.changes++; save_preset(0); settle();
    tc_state.keep_look = 0u; tc_state.changes++; save_preset(1); settle();
    tc_state.color_by = 1u; tc_state.changes++; /* not saved yet */
    load_preset(0); assert(tc_state.look_slot == 1u && tc_state.color_by == 0u);
    load_preset(1); assert(tc_state.look_slot == 0u && tc_state.color_by == 1u);
    /* A preset's own look changed and another preset loaded at once: the change kept (also for its curves). */
    load_preset(0); tc_state.look_roles[0] = 7u; tc_state.changes++;
    load_preset(1); load_preset(0); assert(tc_state.look_slot == 1u && tc_state.look_roles[0] == 7u);
    tc_state.keep_feel = 1u; tc_state.changes++; save_preset(2); settle();
    load_preset(2); assert(tc_state.feel_slot == 3u);
    tc_state.feel[2] = 99u; tc_state.changes++;
    load_preset(1); assert(tc_state.feel_slot == 0u && tc_state.feel[2] != 99u);
    load_preset(2); assert(tc_state.feel_slot == 3u && tc_state.feel[2] == 99u);
    printf("looks: unsaved changes kept when another preset's look (or curves) covers them, the board's and a preset's\n");
}
static void lanes(void) {
    boot(true);
    uint32_t w[ST_PAYLOAD], base = st_records();
    ln_pattern edit = ln;
    for (uint32_t burst = 0; burst < 20u; ++burst) { /* twenty clicks on lane 2, chunks 0 and 3 */
        edit.lanes[1].steps[burst % 16u].on = true; edit.lanes[1].steps[burst % 16u].value = (uint8_t)burst;
        edit.lanes[1].steps[48u + burst % 16u].on = true; edit.lanes[1].kind = LN_MELODY;
        ln_encode(&edit, 4, w); assert(set_record(ST_LANE, 4, w, true) == 0u);
        ln_encode(&edit, 7, w); assert(set_record(ST_LANE, 7, w, true) == 0u);
        now += 100u; st_service(&tc_state, now);
    }
    assert(!memcmp(&ln.lanes[1], &edit.lanes[1], sizeof ln.lanes[1]) && st_records() == base); /* playing, not stored yet */
    now += ST_SAVE_DELAY_MS + 10u; st_service(&tc_state, now);
    assert(st_records() == base + 2u); /* each chunk once */
    ln_pattern before = ln; ln_clear(&ln); boot(false); assert(!memcmp(&before, &ln, sizeof ln));
    /* A wrong one is refused (nothing changes); the reply form still stores at once. */
    ln_encode(&edit, 4, w); w[37] = 1u;
    assert(set_record(ST_LANE, 4, w, true) == 0u && !memcmp(&before, &ln, sizeof ln));
    edit.lanes[2].steps[2].on = true; ln_encode(&edit, 8, w); base = st_records();
    assert(set_record(ST_LANE, 8, w, false) && sx_out[5] == SX_OK && st_records() == base + 1u && ln.lanes[2].steps[2].on);
    assert(st_put_quiet(ST_LANE, LN_RECORDS, w, &tc_state, &engine) == ST_BAD_VALUE);
    printf("lanes: quiet records play at once and are stored once, 3 s after the last; wrong ones refused\n");
}
static void values(void) {
    boot(true);
    uint8_t ks[2] = {ST_LIVE, 0};
    const uint8_t *d = ok(request(SX_VALUES, ks, 2), SX_VALUES);
    assert(d[0] == ST_LIVE && d[1] == 0u && two(&d[2]) == TC_MENU_ITEMS);
    for (uint32_t i = 0; i < TC_MENU_ITEMS; ++i) assert(two(&d[4u + 2u * i]) == tc_menu_value(&tc_state, (uint8_t)i));
    /* A preset with other settings: VALUES = what loading it sets (the board's own as they are). */
    tc_state.arp = 2u; tc_state.strum = 1u; tc_state.hold = 1u; tc_state.harmony = 2u; tc_state.chord_prog = 9u; save_preset(5);
    tc_state.arp = 0u; tc_state.strum = 0u; tc_state.hold = 0u; tc_state.harmony = 0u; tc_state.chord_prog = 0u; tc_state.brightness = 2u;
    uint8_t got[2u * TC_MENU_ITEMS];
    ks[0] = ST_PRESET; ks[1] = 5;
    d = ok(request(SX_VALUES, ks, 2), SX_VALUES); memcpy(got, &d[4], sizeof got);
    load_preset(5); assert(tc_state.last_action == TC_PRESET_LOADED && tc_state.arp == 2u && tc_state.brightness == 2u);
    for (uint32_t i = 0; i < TC_MENU_ITEMS; ++i)
        if (i != TC_MENU_LOAD && i != TC_MENU_SAVE && i != TC_MENU_CLEAR_PRESET) assert(two(&got[2u * i]) == tc_menu_value(&tc_state, (uint8_t)i));
    ks[1] = 6; assert(request(SX_VALUES, ks, 2) && sx_out[5] == SX_ABSENT);
    ks[1] = 8; assert(request(SX_VALUES, ks, 2) && sx_out[5] == SX_BAD_VALUE);
    ks[0] = ST_SCALE; ks[1] = 0; assert(request(SX_VALUES, ks, 2) && sx_out[5] == SX_BAD_VALUE);
    assert(request(SX_VALUES, ks, 1) && sx_out[5] == SX_LENGTH);
    printf("values: every item of the live settings and of a preset as loading it sets them; empty, wrong and short refused\n");
}
static uint32_t set_edits(const uint8_t b[SX_EDIT_BYTES]) {
    uint8_t data[SX_EDIT_BYTES + 9u];
    uint32_t n = sx_pack(b, SX_EDIT_BYTES, data);
    return request(SX_SET_EDITS, data, n);
}
static void edits(void) {
    boot(true);
    uint8_t b[SX_EDIT_BYTES] = {0};
    b[5] = (uint8_t)(TC_ADD7 << 5); b[9] = (uint8_t)(2u << 3 | 1u); b[47] = (uint8_t)(7u << 5);
    b[48] = 0x01u; b[53] = 0x80u; /* strum off: pads 0 and 47 */
    b[55] = 0x02u;                /* sound off: pad 9 */
    uint32_t changes = tc_state.changes;
    char before[TN_MAX]; (void)tn_pad(tc_layout(&tc_state), 5, before);
    assert(set_edits(b) == 0u && tc_state.changes == changes + 1u && tc_state.last_action == TC_REMOTE);
    assert(tc_state.chord[5].add == TC_ADD7 && tc_state.chord[9].sus == 4u && tc_state.chord[9].lower == 1u && tc_state.chord[47].add == 7u);
    assert(tc_state.strum_off == (1u | (uint64_t)1u << 47) && tc_state.chord_mute == (uint64_t)1u << 9);
    /* The names as the play screen shows them, the edits in them. */
    const uint8_t *d = ok(request(SX_CHORD_NAMES, NULL, 0), SX_CHORD_NAMES);
    assert(d[0] == 48u);
    const char *t = (const char *)&d[1];
    for (uint32_t k = 0; k < 48u; ++k) {
        char name[TN_MAX]; (void)tn_pad(tc_layout(&tc_state), k, name);
        assert(!strcmp(t, name) && name[0]);
        if (k == 5u) assert(strcmp(name, before) && strchr(name, '7'));
        t += strlen(t) + 1u;
    }
    assert(request(SX_CHORD_NAMES, b, 1) && sx_out[5] == SX_LENGTH);
    /* Refused, nothing changed: sus 3, a short message, a chord that would leave MIDI range. */
    tc_controls kept = tc_state; uint8_t bad[SX_EDIT_BYTES]; memcpy(bad, b, sizeof bad);
    bad[3] = 3u << 3; assert(set_edits(bad) == 0u && !memcmp(kept.chord, tc_state.chord, sizeof kept.chord));
    uint8_t data[SX_EDIT_BYTES + 9u]; uint32_t n = sx_pack(b, SX_EDIT_BYTES, data);
    b[1] = TC_ADD9 << 5; (void)sx_pack(b, SX_EDIT_BYTES, data);
    assert(request(SX_SET_EDITS, data, n - 1u) == 0u && tc_state.chord[1].add == 0u);
    tp_layout high = tc_layout(&tc_state); high.chord_base = (uint8_t)(TP_CHORD_BASE + TP_OCTAVE_RANGE); high.tonic = 11u;
    memset(tc_state.chord, 0, sizeof tc_state.chord);
    assert(tc_set_layout(&tc_state, &engine, high)); /* B, the chords two octaves up: the top pads near 127 */
    /* Even there every pad takes every edit (the range check, as the live record's, never refuses one): all at once. */
    for (uint32_t k = 0; k < 48u; ++k)
        for (uint32_t e = 0; e < 256u; ++e) {
            tp_notes notes; tc_chord ch = ln_edit((uint8_t)e);
            assert(ch.sus == 6u || tc_notes(tc_layout(&tc_state), k, ch, &notes));
        }
    memset(bad, 0, sizeof bad); for (uint32_t k = 0; k < 48u; ++k) bad[k] = (uint8_t)(k % 3u << 3 | 7u << 5 | (k & 4u ? 1u : 0u));
    assert(set_edits(bad) == 0u && tc_state.chord[47].add == 7u && tc_state.chord[47].sus == 4u && tc_state.chord[46].sus == 2u);
    printf("edits: every chord pad's edits, strum off and sound off at once without a reply; names as the screen's; "
           "refused whole: sus 3, short; every edit fits at the highest chords\n");
}
int main(void) {
    names(); screen(); cleared(); feels(); looks(); lanes(); values(); edits();
    printf("PASS: the content editors (1.9.3): preset names, clearing presets, presets' own curves, quiet lanes, VALUES, "
           "SET_EDITS, CHORD_NAMES\n");
    return 0;
}
