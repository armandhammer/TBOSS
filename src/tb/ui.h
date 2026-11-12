/* SPDX-License-Identifier: GPL-3.0-or-later - what the OLED and the LEDs show. */
#ifndef TB_UI_H
#define TB_UI_H
#include "controls.h"
bool ui_init(void);
/* 2.3 review: the screen set up and the compaction warning sent to it (st_load's erase follows; ui_init clears it). */
void ui_saving_settings(void);
/* Every main-loop pass: redraw if anything visible changed, then let the display
 * and LED drivers send at most one page / one frame. s: the pad scanner (pad test page). */
void ui_service(tc_controls *c, const tp_engine *e, const ts_scan *s, bool usb_ready, uint32_t now);
/* 2.4.2 (review TBR-013 step 1): the 96 pad colors are worked out at most this often (they were every pass, about
 * two thirds of it), and at once when everything is drawn again (awake, the LED test over). The LED chain takes
 * 3.4 ms to send a frame and the pulse changes every 50 ms: 60 updates a second lose nothing visible. */
#define UI_LIGHTS_MS 16u
/* 2.4.3 (review TBR-013 step 2): a frame's 96 colors are worked out this many pads a pass (6 passes a frame) and
 * shown together when the last are done; all 96 in one pass was a 2.6 ms stall of the loop every 16 ms. */
#define UI_LIGHTS_PADS 16u
/* 1.9.0 (sysex.c MENU, ITEM): a page's, an item's and an item's value's names as the screens show
 * them. ui_value_text's is built in a shared buffer: copy it before the next call. */
const char *ui_page_name(uint8_t group);
const char *ui_item_name(uint8_t item);
const char *ui_value_text(uint8_t item, uint8_t value);
#define UI_TEXT_MAX 39u /* the longest of them */
#endif
