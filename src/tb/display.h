/* SPDX-License-Identifier: GPL-3.0-or-later - 128x64 SSD1306-family OLED on SPI3. */
#ifndef TB_DISPLAY_H
#define TB_DISPLAY_H
#include <stdbool.h>
#include <stdint.h>
#define TD_WIDTH 128u
#define TD_PAGES 8u
extern const char td_font_chars[];
extern const uint8_t td_font_rows[][7];
/* Pins (from the board's original firmware): SPI3 SCK PB3,
 * MOSI PB5 (AF6); select PD14 (low active), data/command PD11 (high = data),
 * reset PD12 (low active). The external NOR shares SPI3: its select PA15 is driven
 * high before SPI3 is enabled and never changed, so it is never addressed. */
bool td_init(void);
void td_clear(void);
void td_pixel(uint32_t x, uint32_t y, bool on);
/* Draws text with its top-left corner at (x, y); scale 1 = 6x8 cells, 2 = 12x16.
 * Unknown characters draw as '?'. Returns the x after the text. */
uint32_t td_text(uint32_t x, uint32_t y, const char *text, uint32_t scale);
void td_invert(uint32_t x0, uint32_t y0, uint32_t x1, uint32_t y1);
/* Sends at most one changed page (about 0.4 ms); call every main-loop pass. */
void td_service(void);
#endif
