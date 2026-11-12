/* SPDX-License-Identifier: GPL-3.0-or-later */
#ifndef TB_USB_DIAGNOSTIC_H
#define TB_USB_DIAGNOSTIC_H
#include <stdint.h>
#include <stdbool.h>
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
/* 2.4.5 (review TBR-007): at 0x20018070 (main.c, cleared at start-up), for SWD (the inspect tool): the longest
 * st_requests call (a preset load or save) and st_service call (the save 3 s after a change, one record a pass since
 * 2.4.5) in microseconds, and the passes in which st_service wrote a record. */
/* 2.4.8 (review TBR-010): its fourth word, stack_low, is the deepest the stack has reached since start-up: the lowest
 * address under it no longer holding TB_PAINT (main.c stack_watch). */
typedef struct { uint32_t requests_us, service_us, service_writes, stack_low; } tb_save_timing;
extern volatile tb_save_timing tb_save_time;
/* 2.4.8 (review TBR-010): startup.S paints the RAM from the end of the diagnostics (0x20018800) to 256 bytes under
 * the top of the stack (TB_PAINT_TOP) with TB_PAINT; flash.ld places tb_stack_paint and checks the size. */
#define TB_PAINT 0xDEADBEEFu
#define TB_PAINT_TOP 0x2001FF00u
#define TB_PAINT_WORDS 7616u
extern uint32_t tb_stack_paint[];
/* 2.4.8 (review TBR-021): the two DMA sources (leds.c frame, display.c shown) in their own section, which flash.ld
 * keeps in the main RAM: the core-coupled RAM is not on the DMA's bus, so a stream from there reads nothing. A stream
 * is started only from an address in the main RAM (tb_dma_ok). */
#ifdef __arm__
#define TB_DMA __attribute__((section(".bss.tb_dma")))
#define tb_dma_ok(p) (((uint32_t)(uintptr_t)(p) >> 28) == 2u)
#else
#define TB_DMA
#define tb_dma_ok(p) ((void)(p), true)
#endif
extern uint32_t SystemCoreClock;
uint32_t tb_millis(void);
uint32_t tb_board_init(void);
void tb_tick_init(void); /* board.c: the cycle counter the millisecond count follows (called by tb_board_init) */
void tb_snapshot(void);
#endif
