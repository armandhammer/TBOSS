/* SPDX-License-Identifier: GPL-3.0-or-later - board.c's millisecond count (review TBR-001): SysTick adds the
 * milliseconds the core's cycle counter says went by, so a tick held back by a long masked pass loses none.
 * On stand-in registers (tests/tb/main_hal); tb_board_init itself needs the real oscillators. */
#include "stm32f4xx.h"
#include "tusb.h"
#include "../../src/tb/diagnostic.h"
#include "../../src/tb/clock.h"
#include <assert.h>
#include <stdio.h>
volatile tb_diagnostic tb_status;
tb_test_dwt test_dwt;
tb_test_coredebug test_coredebug;
tb_test_rcc test_rcc;
tb_test_flash test_flash;
tb_test_gpio test_gpioa;
tb_test_otg test_otg;
USB_OTG_DeviceTypeDef test_otg_device;
uint32_t test_primask;
void test_halt(void) { assert(!"no halt here"); }
void test_unmasked(void) {}
bool tud_configure(uint8_t rhport, uint32_t cfg_id, const void *cfg_param) {
    (void)rhport; (void)cfg_id; (void)cfg_param; return true;
}
void tusb_int_handler(uint8_t rhport, bool in_isr) { (void)rhport; (void)in_isr; }
void SysTick_Handler(void);
#define MS (TB_CORE_HZ / 1000u)
static uint32_t seed = 2026u;
static uint32_t rnd(uint32_t n) { seed = seed * 1103515245u + 12345u; return (seed >> 8) % n; }
static void start(void) {
    test_dwt.CTRL = 0u; test_dwt.CYCCNT = 0xDEADBEEFu; test_coredebug.DEMCR = 0u; tb_status.millis = 0u;
    tb_tick_init();
    assert(test_dwt.CYCCNT == 0u && (test_dwt.CTRL & DWT_CTRL_CYCCNTENA_Msk) &&
           (test_coredebug.DEMCR & CoreDebug_DEMCR_TRCENA_Msk));
}
int main(void) {
    /* The finding's check: 5 ms of cycles pass with interrupts masked, then one SysTick: 5 ms counted. */
    start();
    uint32_t before = tb_millis();
    test_dwt.CYCCNT += 5u * MS + 100u;
    SysTick_Handler();
    assert(tb_millis() == before + 5u);
    /* Ticks every millisecond, each served a little late (interrupt latency), and now and then a masked
     * pass of up to 120 ms: the count is always the whole milliseconds since the start, never less. */
    start();
    uint64_t cycles = 0u;
    uint32_t late = 0u;
    for (uint32_t k = 1u; k <= 200000u; ++k) {
        uint64_t due = (uint64_t)k * MS;
        if (cycles >= due) continue; /* fell inside the last masked pass: SysTick keeps one pending tick */
        uint32_t held = rnd(50u) == 0u ? rnd(120u) * MS : 0u;
        uint64_t at = due + held + rnd(400u);
        if (held) late++;
        test_dwt.CYCCNT = (uint32_t)at; /* the counter wraps every 89 s: 200,000 ms crosses it twice */
        cycles = at;
        SysTick_Handler();
        assert(tb_millis() == (uint32_t)(at / MS));
    }
    assert(late > 1000u && cycles > 2u * 0x100000000ull);
    /* A stopped counter (a debugger cleared TRCENA): each tick counts 1 ms, as before; counting again, the
     * next difference starts from the last tick, not from the stale reading. */
    start();
    test_dwt.CYCCNT = 10u * MS; SysTick_Handler(); assert(tb_millis() == 10u);
    test_coredebug.DEMCR = 0u;
    for (uint32_t i = 0; i < 7u; ++i) SysTick_Handler();
    assert(tb_millis() == 17u);
    test_coredebug.DEMCR = CoreDebug_DEMCR_TRCENA_Msk;
    test_dwt.CYCCNT += 3u * MS; SysTick_Handler(); assert(tb_millis() == 20u);
    test_dwt.CTRL = 0u; SysTick_Handler(); assert(tb_millis() == 21u);
    printf("board: SysTick counts the milliseconds the cycle counter saw (a 5 ms masked pass: +5), 200,000 ticks "
           "with latency and masked passes up to 120 ms across two counter wraps exact, a stopped counter "
           "counts 1 ms a tick\n");
    return 0;
}
