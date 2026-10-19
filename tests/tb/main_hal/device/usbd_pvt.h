/* SPDX-License-Identifier: GPL-3.0-or-later - test stand-in for TinyUSB's endpoint state. */
#ifndef TB_TEST_MAIN_USBD
#define TB_TEST_MAIN_USBD
#include <stdbool.h>
#include <stdint.h>
bool usbd_edpt_busy(uint8_t rhport, uint8_t ep_addr);
#endif
