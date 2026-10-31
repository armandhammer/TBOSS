/* SPDX-License-Identifier: GPL-3.0-or-later - the screens (ui.h): Settings. The pages, a page's items with their
 * values, and a reset's confirmation. */
#include "ui_internal.h"
/* Four rows; the list scrolls so the selection stays visible. */
#define ROWS 4u
static uint32_t first_row(uint32_t selected, uint32_t count) {
    uint32_t first = selected >= ROWS - 1u ? selected - (ROWS - 2u) : 0u;
    return count <= ROWS ? 0u : first > count - ROWS ? count - ROWS : first;
}
static void position_text(uint32_t at, uint32_t count) {
    ui_start(); ui_number(at + 1u); ui_put("/"); ui_number(count);
    td_text(128u - 6u * ui_length, 0, ui_buffer, 1);
}
/* A reset's confirmation (1.4.0): what it does, then CANCEL (where it opens) and RESET, reached
 * with up. The chosen one is drawn first and inverted. */
static void draw_confirm(const tc_controls *c) {
    const char *text[3] = {"", "", ""};
    ui_start();
    switch (c->menu) {
    case TC_MENU_RESET_CHORD:
        ui_put("RESET CHORD EDITS?"); text[0] = "THE LAST CHORD PLAYED"; text[1] = "GOES BACK TO PLAIN."; break;
    case TC_MENU_RESET_ALL:
        ui_put("RESET ALL EDITS?"); text[0] = "EVERY CHORD PAD GOES"; text[1] = "BACK TO PLAIN."; break;
    case TC_MENU_BUTTONS_RESET:
        ui_put("RESET ALL BUTTONS?"); text[0] = "EVERY CENTER BUTTON"; text[1] = "BACK TO ITS PRINTED"; text[2] = "FUNCTION."; break;
    case TC_MENU_FACTORY:
        ui_put("RESET ALL OPTIONS?"); text[0] = "PRESETS, SCALES, SETS"; text[1] = "AND PROGRESSIONS STAY";
        text[2] = "ALL ELSE TO DEFAULTS"; break;
    default:
        ui_put("RESET "); ui_put(ui_group_short[c->group % TC_GROUPS]); ui_put(" OPTIONS?");
        text[1] = "THEIR DEFAULTS."; text[2] = "YOUR CHANGES ARE LOST"; break;
    }
    td_text(0, 0, ui_buffer, 1);
    if (c->menu == TC_MENU_RESET_PAGE) { ui_start(); ui_number(tc_reset_count(c->group % TC_GROUPS)); ui_put(" SETTINGS BACK TO"); td_text(0, 11, ui_buffer, 1); }
    for (uint32_t i = 0; i < 3u; ++i) if (text[i][0]) td_text(0, 11u + 10u * i, text[i], 1);
    static const char *const choice[2] = {"CANCEL", "RESET"};
    static const uint32_t at[2] = {12u, 86u};
    uint32_t chosen = c->edit_value ? 1u : 0u;
    for (uint32_t k = 0; k < 2u; ++k) {
        uint32_t i = k ? 1u - chosen : chosen;
        uint32_t end = td_text(at[i], 43, choice[i], 1);
        if (!k) td_invert(at[i] - 2u, 42, end + 1u, 51);
    }
    td_text(0, 56, chosen ? "O RESETS   <CANCEL" : "UP THEN O TO RESET", 1);
}
void ui_draw_menu(const tc_controls *c) {
    uint8_t group = c->group % TC_GROUPS;
    if (c->ui == TC_UI_EDIT && tc_confirm_item(c->menu)) { draw_confirm(c); return; }
    if (c->ui == TC_UI_GROUPS) {
        td_text(0, 0, "SETTINGS", 1);
        position_text(group, TC_GROUPS);
        uint32_t first = first_row(group, TC_GROUPS);
        for (uint32_t row = 0; row < ROWS && first + row < TC_GROUPS; ++row) {
            uint32_t i = first + row, y = 11u + 10u * row;
            td_text(0, y, i == group ? ">" : " ", 1);
            td_text(8, y, ui_group_names[i], 1);
            td_text(122, y, ">", 1);
        }
    } else {
        uint8_t items[TC_GROUP_MAX];
        uint32_t count = tc_page_items(c, group, items), at = 0;
        for (uint32_t i = 0; i < count; ++i) if (items[i] == c->menu) at = i;
        td_text(0, 0, ui_group_names[group], 1);
        position_text(at, count);
        uint32_t first = first_row(at, count);
        for (uint32_t row = 0; row < ROWS && first + row < count; ++row) {
            uint8_t item = items[first + row];
            uint32_t y = 11u + 10u * row;
            bool editing = item == c->menu && c->ui == TC_UI_EDIT;
            td_text(0, y, item == c->menu ? ">" : " ", 1);
            ui_start(); ui_put(ui_menu_items[item]);
            uint32_t label = ui_length; /* (1.9.3) */
            ui_value_name(item, editing ? c->edit_value : tc_menu_value(c, item));
            if (!editing && c->mpe && (item == TC_MENU_CHORD_CH || item == TC_MENU_MELODY_CH)) { /* 1.8.0: MPE's */
                ui_start(); ui_put("MPE "); ui_number(tc_half_ch(c, item == TC_MENU_MELODY_CH) + 1u);
            }
            /* 1.9.3: a preset's name too long for beside the item: choosing, the whole row; else cut short */
            uint32_t room = (128u - 8u - 6u * label - 6u) / 6u;
            bool wide = (item == TC_MENU_LOAD || item == TC_MENU_SAVE || item == TC_MENU_CLEAR_PRESET) && ui_length > room;
            if (wide && !editing) { ui_length = room; ui_buffer[room] = 0; }
            if (!(wide && editing)) td_text(8, y, ui_menu_items[item], 1);
            uint32_t x = 128u - 6u * ui_length;
            td_text(x, y, ui_buffer, 1);
            if (editing) td_invert(x - 1u, y - 1u, 128u, y + 8u);
        }
    }
    /* Bottom line: what the buttons do here, or what the last action did. */
    /* 2.0.0: Shift on: what Shift + the buttons do here (controls_menu.c tc_shift_nav), whatever happened last */
    bool shifted = tc_shift(c) && (c->ui != TC_UI_EDIT || (!tc_confirm_item(c->menu) && c->menu != TC_MENU_IO_TEST &&
                                                          c->menu != TC_MENU_PAD_TEST));
    switch (shifted ? TC_NONE : c->last_action) {
    case TC_NONE: case TC_PREVIOUS: case TC_NEXT: case TC_MENU: case TC_COMMIT: case TC_CANCEL:
        ui_start();
        if (shifted && c->ui == TC_UI_GROUPS) ui_put("<HOME ^vEND");
        else if (shifted && c->ui == TC_UI_MENU) ui_put("<HOME ^vEND ODEFAULT");
        else if (shifted) ui_put("<HOME ^v10 ODEFAULT");
        else if (c->ui == TC_UI_EDIT && c->menu == TC_MENU_LANE_MUTATE) ui_put("<CANCEL   O MUTATE"); /* 2.0.5 */
        else if (c->ui == TC_UI_EDIT) ui_put("<CANCEL   O SET");
        else if (c->ui == TC_UI_MENU && c->menu == TC_MENU_VERSION) { ui_put("<BACK   "); ui_put(TB_VERSION_TAG); } /* the installer finds this tag */
        else if (c->ui == TC_UI_MENU && (c->menu == TC_MENU_LED_TEST || c->menu == TC_MENU_IO_TEST ||
                                         c->menu == TC_MENU_PAD_TEST)) ui_put("<BACK   O START");
        else if (c->ui == TC_UI_MENU && tc_confirm_item(c->menu)) ui_put("<BACK   O RESET");
        else if (c->ui == TC_UI_MENU && c->menu == TC_MENU_SEQ_RECORD) ui_put("<BACK   O RECORD");
        else if (c->ui == TC_UI_MENU && c->menu == TC_MENU_SEQ_CLEAR) ui_put("<BACK   O CLEAR");
        else if (c->ui == TC_UI_MENU && (c->menu == TC_MENU_TRANSPORT || c->menu == TC_MENU_SEQ_PLAY))
            ui_put(c->transport ? "<BACK   O STOP" : "<BACK   O PLAY");
        else if (c->ui == TC_UI_MENU && c->menu == TC_MENU_STEPS) ui_put("<BACK   O OPEN");
        else if (c->ui == TC_UI_MENU && c->menu == TC_MENU_LANE_MUTATE) ui_put("<BACK   O CHOOSE"); /* 2.0.5 */
        else if (c->ui == TC_UI_MENU) ui_put("<BACK   O CHANGE");
        else ui_put("<BACK   O OPEN");
        break;
    case TC_TODO_CONTEXT: ui_start(); ui_put("< TO LEAVE THE MENU"); break;
    default: ui_last_action(c); break;
    }
    td_text(0, 56, ui_buffer, 1);
}
