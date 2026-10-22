/* SPDX-License-Identifier: GPL-3.0-or-later - pad RGB LEDs (SPI2/DMA) and centre-button LEDs (TLC5947). */
#ifndef TB_LEDS_H
#define TB_LEDS_H
#include <stdbool.h>
#include <stdint.h>
#define TL_PADS 96u
#define TL_BUTTONS 24u
typedef struct { uint8_t r, g, b; } tl_rgb;
/* Pad chain (as driven by the board's original firmware: SPI2 MOSI PC3, WS2812-style,
 * three bytes per LED, MSB first): chain position i is scanner pad i. Byte order
 * on the wire is TL_ORDER_GRB unless the board shows otherwise. */
bool tl_init(void);
void tl_set_pad(uint32_t pad, tl_rgb color);
/* TLC5947 on PE3 (latch), PE4 (clock), PE5 (data): 24 channels, 12-bit. */
void tl_set_button(uint32_t channel, uint16_t level);
/* Starts a pad frame if changed and the previous one finished; shifts and latches
 * the button frame if changed. Call every main-loop pass. */
void tl_service(void);
#endif
