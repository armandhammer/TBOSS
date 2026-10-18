/* SPDX-License-Identifier: GPL-3.0-or-later - Settings > About > Pad test (ui.c draws it from
 * ts_strongest in scan.c): the strongest pads of a frame, in order, and the page's place in
 * the menu. Pads keep playing while the page is open. */
#include "../../src/tb/controls.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
volatile ts_abi ts_status;
int main(void) {
    uint32_t v[TS_KEYS] = {0};
    uint8_t top[3];
    /* All equal: the lowest pads, in order. */
    ts_strongest(v, top, 3u); assert(top[0] == 0u && top[1] == 1u && top[2] == 2u);
    /* A press with bleed onto two neighbours, plus an unrelated light pad. */
    v[50] = 2734u; v[54] = 812u; v[46] = 640u; v[90] = 300u;
    ts_strongest(v, top, 3u); assert(top[0] == 50u && top[1] == 54u && top[2] == 46u);
    /* Ties: the lower pad first; the top end of the range. */
    v[95] = 4095u; v[10] = 812u;
    ts_strongest(v, top, 3u); assert(top[0] == 95u && top[1] == 50u && top[2] == 10u);
    ts_strongest(v, top, 1u); assert(top[0] == 95u);
    /* Every pad can be found: ranking all 96 visits each pad once, strongest first. */
    uint8_t all[TS_KEYS]; bool seen[TS_KEYS] = {false};
    for (uint32_t k = 0; k < TS_KEYS; ++k) v[k] = (k * 37u) % 4096u;
    ts_strongest(v, all, TS_KEYS);
    for (uint32_t i = 0; i < TS_KEYS; ++i) {
        assert(!seen[all[i]]); seen[all[i]] = true;
        if (i) assert(v[all[i - 1u]] >= v[all[i]]);
    }
    /* The page: About lists Version, I/O test, Pad test; it opens like the I/O test (an
     * edit with nothing to change) and centre or left leaves it. */
    assert(tc_group_items[TC_GROUP_ABOUT][2] == TC_MENU_PAD_TEST && tc_group_size(TC_GROUP_ABOUT) == 3u);
    assert(tc_menu_values(TC_MENU_PAD_TEST) == 1u);
    /* Opening and leaving it with the buttons: test_controls.c. Its value stays in range. */
    tc_init(&tc_state);
    tc_state.ui = TC_UI_EDIT; tc_state.menu = TC_MENU_PAD_TEST; tc_state.group = TC_GROUP_ABOUT;
    assert(tc_menu_value(&tc_state, TC_MENU_PAD_TEST) < tc_menu_values(TC_MENU_PAD_TEST));
    puts("PASS: pad test: strongest pads in order (ties: lower pad), all 96 rankable, About page has "
         "Version, I/O test, Pad test");
    return 0;
}
