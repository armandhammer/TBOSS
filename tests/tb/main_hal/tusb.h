/* SPDX-License-Identifier: GPL-3.0-or-later - test stand-in for the TinyUSB calls main.c makes
 * (tests/tb/test_main.c plays the USB stack and the computer). */
#ifndef TB_TEST_MAIN_TUSB
#define TB_TEST_MAIN_TUSB
#include <stdbool.h>
#include <stdint.h>
typedef enum { TUSB_ROLE_DEVICE = 1 } tusb_role_t;
typedef enum { TUSB_SPEED_FULL = 0 } tusb_speed_t;
typedef struct { tusb_role_t role; tusb_speed_t speed; } tusb_rhport_init_t;
bool tusb_init(uint8_t rhport, const tusb_rhport_init_t *init);
void tud_task_ext(uint32_t timeout_ms, bool in_isr);
bool tud_mounted(void);
bool tud_suspended(void);
bool tud_midi_packet_write(const uint8_t packet[4]);
bool tud_midi_packet_read(uint8_t packet[4]);
#endif
