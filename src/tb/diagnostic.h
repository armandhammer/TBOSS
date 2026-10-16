/* SPDX-License-Identifier: GPL-3.0-or-later */
#ifndef TB_USB_DIAGNOSTIC_H
#define TB_USB_DIAGNOSTIC_H
#include <stdint.h>
/* Fixed SWD ABI at 0x20018000, all words; observations are not atomic. */
typedef struct {
    uint32_t magic, loops, cookie, fault;
    uint32_t stage, millis, usb_irqs, mounts;
    uint32_t resets, suspends, accepted, completed;
    uint32_t tx_errors, demo_state, rcc_cr, rcc_pllcfgr;
    uint32_t rcc_cfgr, core_hz, usb_hz, gccfg;
    uint32_t gusbcfg, gintsts, gotgctl, dcfg;
    uint32_t cfsr, hfsr, mode, initialized_data;
} tb_diagnostic;
extern volatile tb_diagnostic tb_status;
extern uint32_t SystemCoreClock;
uint32_t tb_millis(void);
uint32_t tb_board_init(void);
void tb_snapshot(void);
#endif
