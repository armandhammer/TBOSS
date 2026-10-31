/* SPDX-License-Identifier: GPL-3.0-or-later - 1.9.0, the board's menu for the editor's Settings tab, on the
 * real sysex.c/settings.c/controls_menu.c/ui_menu.c with a fake sector 11: MENU lists each page as
 * tc_group_items; ITEM gives every item's kind, value, name and every value's name as the screen shows
 * them (continued from first); SET_ITEM leaves exactly the state the menu's O leaves for every value of
 * every item the editor may set, refuses what it may not, never replies; a quiet SET_RECORD applies
 * without a reply. */
#include "../../src/tb/sysex.h"
#include "../../src/tb/ui.h"
#include "../../src/tb/controls_internal.h"
#include "../../src/tb/flash_store.h"
#include "../../src/tb/nor.h"
#include "../../src/tb/lanes.h"
#include "../../src/tb/sequence.h"
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
void nr_id(uint8_t id[3]) { id[0] = id[1] = id[2] = 0; }
uint32_t nr_size(void) { return 0u; }
bool nr_read(uint32_t a, uint8_t *out, uint32_t n) { (void)a; (void)out; (void)n; return false; }
static tp_engine engine;
static uint8_t msg[400];
static uint32_t request(uint8_t cmd, const uint8_t *data, uint32_t n) {
    uint32_t len = 0, sum = cmd;
    msg[len++] = 0xF0; msg[len++] = 0x7D; msg[len++] = 0x54; msg[len++] = 0x42; msg[len++] = cmd;
    for (uint32_t i = 0; i < n; ++i) { msg[len++] = data[i]; sum += data[i]; }
    msg[len++] = (uint8_t)((128u - sum % 128u) % 128u); msg[len++] = 0xF7;
    return sx_handle(msg, len, &tc_state, &engine, 1000u);
}
static const uint8_t *ok(uint32_t len, uint8_t cmd) {
    assert(len >= 8u && sx_out[4] == (cmd | 0x40u) && sx_out[len - 1u] == 0xF7 && sx_out[5] == SX_OK);
    uint32_t sum = 0;
    for (uint32_t i = 4; i < len - 1u; ++i) { assert(sx_out[i] < 0x80u); sum += sx_out[i]; }
    assert(sum % 128u == 0u);
    return &sx_out[6];
}
static uint32_t two(const uint8_t *d) { return d[0] | (uint32_t)d[1] << 7; }
static void fresh(void) {
    memset(flash, 0xFF, sizeof flash); memset(&engine, 0, sizeof engine);
    tc_init(&tc_state); (void)st_load(&tc_state); engine.layout = tc_layout(&tc_state);
}
static void pages(void) {
    for (uint8_t g = 0; g < TC_GROUPS; ++g) {
        uint8_t page[1] = {g};
        const uint8_t *d = ok(request(SX_MENU, page, 1), SX_MENU);
        assert(d[0] == g && d[1] == TC_GROUPS && d[2] == tc_group_size(g));
        for (uint32_t i = 0; i < d[2]; ++i) assert(two(&d[3u + 2u * i]) == tc_group_items[g][i]);
        assert(!memcmp(&d[3u + 2u * d[2]], ui_page_name(g), strlen(ui_page_name(g))));
    }
    uint8_t past[1] = {TC_GROUPS};
    assert(sx_out[5] == SX_OK && request(SX_MENU, past, 1) && sx_out[5] == SX_BAD_VALUE);
    puts("menu: every page's items in order and its name");
}
/* Every item, every value's name as the screen shows it, across as many replies as it takes. */
static void items(void) {
    uint32_t longest = 0, continued = 0;
    for (uint32_t item = 0; item < TC_MENU_ITEMS; ++item) {
        uint32_t values = tc_menu_values((uint8_t)item), first = 0, rounds = 0;
        do {
            uint8_t q[4] = {(uint8_t)(item & 127u), (uint8_t)(item >> 7), (uint8_t)(first & 127u), (uint8_t)(first >> 7)};
            uint32_t len = request(SX_ITEM, q, 4);
            const uint8_t *d = ok(len, SX_ITEM);
            if (len > longest) longest = len;
            assert(len <= SX_OUT_MAX && two(d) == item && d[2] == tc_item_kind((uint8_t)item) && d[3] <= 1u);
            assert(two(&d[4]) == values && two(&d[6]) == tc_menu_value(&tc_state, (uint8_t)item) && two(&d[8]) == first);
            const char *at = (const char *)&d[11];
            assert(!strcmp(at, ui_item_name((uint8_t)item)));
            at += strlen(at) + 1u;
            assert(d[10] > 0u || first >= values);
            for (uint32_t k = 0; k < d[10]; ++k, ++first) {
                char want[40]; strcpy(want, ui_value_text((uint8_t)item, (uint8_t)first));
                assert(!strcmp(at, want)); at += strlen(at) + 1u;
            }
            rounds++;
        } while (first < values);
        if (rounds > 1u) continued++;
    }
    assert(longest <= SX_INLINE_MAX && continued >= 3u); /* 2.0.6: each reply between two notes (board test Z5) */
    uint8_t past[4] = {(uint8_t)(TC_MENU_ITEMS & 127u), (uint8_t)(TC_MENU_ITEMS >> 7), 0, 0};
    assert(request(SX_ITEM, past, 4) && sx_out[5] == SX_BAD_VALUE);
    /* Shown: Pedal's Note only with Mode Note, Glide time only with Glide Time. */
    uint8_t q[4] = {TC_MENU_GLIDE_TIME & 127u, TC_MENU_GLIDE_TIME >> 7, 0, 0};
    assert(!ok(request(SX_ITEM, q, 4), SX_ITEM)[3]);
    tc_state.glide = TC_GLIDE_TIME; assert(ok(request(SX_ITEM, q, 4), SX_ITEM)[3] == 1u);
    fresh();
    printf("items: %u items, every value's name as shown (%u in more than one reply, longest reply %u bytes)\n",
           (unsigned)TC_MENU_ITEMS, (unsigned)continued, (unsigned)longest);
}
/* What a change leaves: the settings record, the lanes and the progression slot, the engine's layout. */
typedef struct { uint32_t live[ST_PAYLOAD]; ln_pattern lanes; uint8_t unit, transport, preset_request; tp_layout layout; } after_t;
static void snapshot(after_t *a) {
    memset(a, 0, sizeof *a);
    assert(st_get(ST_LIVE, 0, &tc_state, a->live) == ST_OK);
    a->lanes = ln; a->unit = sq.unit; a->transport = tc_state.transport ? 1u : 0u; a->layout = engine.layout;
    a->preset_request = tc_state.request;
}
static void set_by_menu(uint8_t item, uint8_t value) {
    tc_state.ui = TC_UI_EDIT; tc_state.menu = item; tc_state.edit_value = value; tc_state.nav_held = 0u;
    tc_navigate(&tc_state, &engine, TC_NAV_ENTER);
}
static uint32_t set_by_editor(uint32_t item, uint32_t value) {
    uint8_t q[4] = {(uint8_t)(item & 127u), (uint8_t)(item >> 7), (uint8_t)(value & 127u), (uint8_t)(value >> 7)};
    return request(SX_SET_ITEM, q, 4);
}
static void setting(void) {
    uint32_t compared = 0;
    for (uint32_t item = 0; item < TC_MENU_ITEMS; ++item) {
        uint8_t kind = tc_item_kind((uint8_t)item);
        if (kind != TC_ITEM_VALUE) continue;
        for (uint32_t v = 0; v < tc_menu_values((uint8_t)item); ++v) {
            after_t menu, editor;
            fresh(); uint32_t changes = tc_state.changes;
            set_by_menu((uint8_t)item, (uint8_t)v); snapshot(&menu);
            bool menu_took = tc_state.last_action == TC_SETTING, menu_changed = tc_state.changes != changes;
            fresh();
            assert(set_by_editor(item, v) == 0u); /* never a reply: the board keeps playing */
            snapshot(&editor);
            assert(!memcmp(&menu, &editor, sizeof menu));
            assert(tc_state.ui == TC_UI_PLAY); /* the board's screen stays where it is */
            if (menu_took) assert(tc_state.last_action == TC_REMOTE);
            assert(menu_changed == (tc_state.changes != changes));
            compared++;
        }
        fresh(); assert(set_by_editor(item, tc_menu_values((uint8_t)item)) == 0u && tc_state.changes == 0u); /* out of range */
    }
    /* On the board only: the resets, tests and actions change nothing from the editor. */
    static const uint8_t board[] = {TC_MENU_RESET_PAGE, TC_MENU_FACTORY, TC_MENU_RESET_ALL, TC_MENU_BUTTONS_RESET,
                                    TC_MENU_PAD_TEST, TC_MENU_IO_TEST, TC_MENU_SEQ_CLEAR, TC_MENU_SEQ_RECORD, TC_MENU_ADD7};
    for (uint32_t i = 0; i < sizeof board; ++i) {
        assert(tc_item_kind(board[i]) == TC_ITEM_BOARD);
        fresh(); tc_state.group = TC_GROUP_PLAY; tc_state.hold = 1u;
        after_t before, later; snapshot(&before);
        assert(set_by_editor(board[i], 1u) == 0u && set_by_editor(board[i], 0u) == 0u);
        snapshot(&later); assert(!memcmp(&before, &later, sizeof before) && tc_state.ui == TC_UI_PLAY);
    }
    /* Transport: 1 starts, 0 stops, the same value again changes nothing. */
    fresh();
    assert(tc_item_kind(TC_MENU_TRANSPORT) == TC_ITEM_TOGGLE && tc_item_kind(TC_MENU_SEQ_PLAY) == TC_ITEM_TOGGLE);
    set_by_editor(TC_MENU_TRANSPORT, 1u); assert(tc_state.transport);
    set_by_editor(TC_MENU_SEQ_PLAY, 1u); assert(tc_state.transport);
    set_by_editor(TC_MENU_SEQ_PLAY, 0u); assert(!tc_state.transport);
    /* Load preset: asks settings.c to load it, as the menu does. */
    fresh();
    assert(tc_item_kind(TC_MENU_LOAD) == TC_ITEM_CHOOSE);
    set_by_editor(TC_MENU_LOAD, 3u); assert(tc_state.request == TC_REQUEST_LOAD && tc_state.request_slot == 3u);
    printf("set item: %u item values set exactly as the menu sets them, out of range and board-only refused, "
           "transport and presets\n", (unsigned)compared);
}
static void quiet(void) {
    fresh();
    uint32_t p[ST_PAYLOAD]; uint8_t bytes[4u * ST_PAYLOAD], data[3u + 200u];
    assert(st_get(ST_LIVE, 0, &tc_state, p) == ST_OK);
    p[0] = (p[0] & ~0xFF0000u) | 40u << 16; /* another chord base, an octave down */
    for (uint32_t i = 0; i < ST_PAYLOAD; ++i) for (uint32_t k = 0; k < 4u; ++k) bytes[4u * i + k] = (uint8_t)(p[i] >> (8u * k));
    data[0] = ST_LIVE; data[1] = 0; data[2] = SX_QUIET;
    uint32_t n = 3u + sx_pack(bytes, sizeof bytes, &data[3]);
    assert(request(SX_SET_RECORD, data, n) == 0u && tc_state.chord_base == 40u && tc_state.last_action == TC_REMOTE);
    data[2] = 2u; assert(request(SX_SET_RECORD, data, n) && sx_out[5] == SX_BAD_VALUE); /* unknown flags */
    data[2] = 0u; p[0] = (p[0] & ~0xFF0000u) | 36u << 16;
    for (uint32_t i = 0; i < ST_PAYLOAD; ++i) for (uint32_t k = 0; k < 4u; ++k) bytes[4u * i + k] = (uint8_t)(p[i] >> (8u * k));
    (void)sx_pack(bytes, sizeof bytes, &data[3]);
    assert(request(SX_SET_RECORD, data, n) && sx_out[5] == SX_OK && tc_state.chord_base == 36u); /* flags 0: a reply */
    puts("quiet set record: applied without a reply; flags 0 replies; unknown flags refused");
}
/* 1.9.4, STATE: the check changes with any value or where an item shows and comes back with them; the counts;
 * the shown bits = ITEM's; which requests main.c may answer between two notes. */
static uint32_t five(const uint8_t *d) { uint32_t v = 0; for (uint32_t k = 0; k < 5u; ++k) v |= (uint32_t)d[k] << (7u * k); return v; }
typedef struct { uint32_t check, lanes, progs, changes; bool shown[TC_MENU_ITEMS]; } state_t;
static state_t state(void) {
    uint32_t len = request(SX_STATE, NULL, 0);
    const uint8_t *d = ok(len, SX_STATE);
    assert(len == 6u + 20u + 2u + (TC_MENU_ITEMS + 6u) / 7u + 2u && two(d + 20) == TC_MENU_ITEMS);
    state_t t = {five(d), five(d + 5), five(d + 10), five(d + 15), {0}};
    for (uint32_t i = 0; i < TC_MENU_ITEMS; ++i) t.shown[i] = ((uint32_t)d[22u + i / 7u] >> (i % 7u)) & 1u;
    return t;
}
static void states(void) {
    fresh();
    state_t a = state();
    assert(request(SX_STATE, (const uint8_t *)"x", 1) && sx_out[5] == SX_LENGTH);
    for (uint32_t i = 0; i < TC_MENU_ITEMS; ++i) {
        uint8_t q[4] = {(uint8_t)(i & 127u), (uint8_t)(i >> 7), 0, 0};
        assert(ok(request(SX_ITEM, q, 4), SX_ITEM)[3] == a.shown[i]);
    }
    assert(!a.shown[TC_MENU_GLIDE_TIME]);
    tc_state.glide = TC_GLIDE_TIME; /* Glide time shows: the bit and the check */
    state_t b = state(); assert(b.shown[TC_MENU_GLIDE_TIME] && b.check != a.check);
    fresh(); assert(state().check == a.check); /* the same values, the same check */
    set_by_menu(TC_MENU_HOLD, 1u); state_t c = state(); assert(c.check != a.check && c.changes != a.changes);
    set_by_menu(TC_MENU_HOLD, 0u); assert(state().check == a.check);
    ln_changes++; sq_changes += 2u; state_t e2 = state(); assert(e2.lanes == ln_changes && e2.progs == sq_changes + sq_chain_changes);
    /* 2.0.6 (board test X4): a progression saved to its slot counts too; the live one changed before it was saved */
    tc_request(&tc_state, TC_REQUEST_PROG_SAVE, 2u); st_requests(&tc_state, &engine); assert(state().progs != e2.progs);
    static const uint8_t inline_ok[] = {SX_IDENTIFY, SX_GET_SCALE, SX_GET_RECORD, SX_MENU, SX_ITEM, SX_PADS, SX_VALUES,
                                        SX_CHORD_NAMES, SX_STATE};
    for (uint32_t cmd = 0; cmd < 0x40u; ++cmd) {
        uint8_t m[7] = {0xF0, 0x7D, 0x54, 0x42, (uint8_t)cmd, 0, 0xF7};
        bool expect = memchr(inline_ok, (int)cmd, sizeof inline_ok) != NULL;
        assert(sx_inline(m, 7) == expect);
    }
    assert(!sx_inline(msg, 6));
    fresh();
    printf("state: the check follows every value and where items show (= ITEM's), back with them; counts; 9 inline requests\n");
}
/* 2.0.7, Clear preset: the preset and its own look and curves go; in use, the board's come back; busy recording. */
static void clear_preset(void) {
    fresh();
    tc_state.keep_look = 1u; tc_state.keep_feel = 1u;
    tc_state.preset = 2u; uint32_t empty = state().check; /* 2.0.7: the check covers which presets there are (A2) */
    assert(tc_set_item(&tc_state, &engine, TC_MENU_SAVE, 2u)); st_requests(&tc_state, &engine);
    assert(tc_presets_present & 4u && tc_state.last_action == TC_PRESET_SAVED && state().check != empty);
    assert(tc_set_item(&tc_state, &engine, TC_MENU_LOAD, 2u)); st_requests(&tc_state, &engine);
    assert(tc_state.last_action == TC_PRESET_LOADED && tc_state.feel_slot == 3u && tc_state.look_slot == 3u);
    assert(tc_item_kind(TC_MENU_CLEAR_PRESET) == TC_ITEM_CHOOSE && tc_menu_values(TC_MENU_CLEAR_PRESET) == TC_PRESETS);
    tc_state.recording = true;
    assert(tc_set_item(&tc_state, &engine, TC_MENU_CLEAR_PRESET, 2u)); st_requests(&tc_state, &engine);
    assert(tc_state.last_action == TC_SEQ_BUSY && (tc_presets_present & 4u));
    tc_state.recording = false;
    uint32_t changes = tc_state.changes;
    assert(tc_set_item(&tc_state, &engine, TC_MENU_CLEAR_PRESET, 2u)); st_requests(&tc_state, &engine);
    assert(tc_state.last_action == TC_PRESET_CLEARED && tc_state.cleared == 2u && tc_state.changes != changes);
    assert(!(tc_presets_present & 4u) && !tc_state.feel_slot && !tc_state.look_slot && state().check == empty);
    uint8_t q[4] = {TC_MENU_CLEAR_PRESET & 127u, TC_MENU_CLEAR_PRESET >> 7, 2u, 0u};
    assert(!strncmp((const char *)ok(request(SX_ITEM, q, 4), SX_ITEM) + 11, "CLEAR PRESET", 12));
    assert(tc_set_item(&tc_state, &engine, TC_MENU_LOAD, 2u)); st_requests(&tc_state, &engine);
    assert(tc_state.last_action == TC_PRESET_EMPTY);
    fresh();
    puts("clear preset (STATE's check follows the presets there): the preset and its own look and curves gone, the board's back in use, not while recording");
}
int main(void) {
    fresh();
    pages(); items(); setting(); quiet(); states(); clear_preset();
    puts("PASS: the menu for the editor (1.9.0): pages, items and value names as shown, SET_ITEM = the menu's O, quiet records; "
         "1.9.4 STATE");
    return 0;
}
