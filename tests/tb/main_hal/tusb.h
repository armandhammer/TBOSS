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
uint32_t tud_midi_n_packet_write_n(uint8_t itf, const uint8_t packets[], uint32_t n_packets);
bool tud_midi_packet_read(uint8_t packet[4]);
/* board.c (tests/tb/test_board.c) */
typedef struct { bool vbus_sensing; } tb_test_dwc2_config;
typedef struct { tb_test_dwc2_config dwc2; } tud_configure_param_t;
#define CFG_TUD_CONFIGURE_DWC2_DEFAULT { .vbus_sensing = true }
enum { TUD_CFGID_DWC2 = 1 };
bool tud_configure(uint8_t rhport, uint32_t cfg_id, const void *cfg_param);
void tusb_int_handler(uint8_t rhport, bool in_isr);
#endif
