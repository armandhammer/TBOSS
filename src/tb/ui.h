/* SPDX-License-Identifier: GPL-3.0-or-later - what the OLED and the LEDs show. */
#ifndef TB_UI_H
#define TB_UI_H
#include "controls.h"
bool ui_init(void);
/* Every main-loop pass: redraw if anything visible changed, then let the display
 * and LED drivers send at most one page / one frame. s: the pad scanner (pad test page). */
void ui_service(tc_controls *c, const tp_engine *e, const ts_scan *s, bool usb_ready, uint32_t now);
/* 1.9.0 (sysex.c MENU, ITEM): a page's, an item's and an item's value's names as the screens show
 * them. ui_value_text's is built in a shared buffer: copy it before the next call. */
const char *ui_page_name(uint8_t group);
const char *ui_item_name(uint8_t item);
const char *ui_value_text(uint8_t item, uint8_t value);
#define UI_TEXT_MAX 39u /* the longest of them */
#endif
