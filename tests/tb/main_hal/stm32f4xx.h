/* SPDX-License-Identifier: GPL-3.0-or-later - test stand-in for the CMSIS header main.c uses:
 * interrupt masking as a variable, barriers as nothing (tests/tb/test_main.c). */
#ifndef TB_TEST_MAIN_STM32
#define TB_TEST_MAIN_STM32
#include <stdint.h>
extern uint32_t test_primask;
static inline uint32_t __get_PRIMASK(void) { return test_primask; }
static inline void __disable_irq(void) { test_primask = 1u; }
static inline void __enable_irq(void) { test_primask = 0u; }
static inline void __set_PRIMASK(uint32_t mask) { test_primask = mask; }
static inline void __DMB(void) {}
void test_wfi(void); /* fail(): the test stops there */
static inline void __WFI(void) { test_wfi(); }
#endif
