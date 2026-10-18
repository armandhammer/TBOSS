/* SPDX-License-Identifier: GPL-3.0-or-later - what the OLED and the LEDs show. */
#ifndef TB_UI_H
#define TB_UI_H
#include "controls.h"
bool ui_init(void);
/* Every main-loop pass: redraw if anything visible changed, then let the display
 * and LED drivers send at most one page / one frame. s: the pad scanner (pad test page). */
void ui_service(tc_controls *c, const tp_engine *e, const ts_scan *s, bool usb_ready, uint32_t now);
#endif
