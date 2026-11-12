/* SPDX-License-Identifier: GPL-3.0-or-later - test stand-in for the CMSIS header main.c uses:
 * interrupt masking as a variable, barriers as nothing (tests/tb/test_main.c). */
#ifndef TB_TEST_MAIN_STM32
#define TB_TEST_MAIN_STM32
#include <stdint.h>
extern uint32_t test_primask;
static inline uint32_t __get_PRIMASK(void) { return test_primask; }
static inline void __disable_irq(void) { test_primask = 1u; }
static inline void __enable_irq(void) { test_primask = 0u; }
void test_unmasked(void); /* interrupts on again: test_main.c watches where main.c's masked pass ends (TBR-015) */
static inline void __set_PRIMASK(uint32_t mask) { test_primask = mask; if (!mask) test_unmasked(); }
static inline void __DMB(void) {}
void test_halt(void); /* fail()'s spin (2.4.4, TBR-005: no WFI): the test stops there */
static inline void __NOP(void) { test_halt(); }
static inline void __DSB(void) {}
/* board.c (tests/tb/test_board.c): the cycle counter and SysTick for real; the clock, flash, GPIO and USB
 * registers only so that it compiles (tb_board_init's oscillator waits cannot finish on plain memory). */
typedef struct { volatile uint32_t CTRL, CYCCNT; } tb_test_dwt;
typedef struct { volatile uint32_t DEMCR; } tb_test_coredebug;
typedef struct { volatile uint32_t CR, PLLCFGR, CFGR, AHB1ENR, AHB2ENR, AHB2RSTR; } tb_test_rcc;
typedef struct { volatile uint32_t ACR; } tb_test_flash;
typedef struct { volatile uint32_t MODER, OTYPER, OSPEEDR, PUPDR, AFR[2]; } tb_test_gpio;
typedef struct { volatile uint32_t GOTGCTL, GUSBCFG, GINTSTS, GCCFG; } tb_test_otg;
typedef struct { volatile uint32_t DCFG; } USB_OTG_DeviceTypeDef;
extern tb_test_dwt test_dwt;
extern tb_test_coredebug test_coredebug;
extern tb_test_rcc test_rcc;
extern tb_test_flash test_flash;
extern tb_test_gpio test_gpioa;
extern tb_test_otg test_otg;
extern USB_OTG_DeviceTypeDef test_otg_device;
#define DWT (&test_dwt)
#define CoreDebug (&test_coredebug)
#define RCC (&test_rcc)
#define FLASH (&test_flash)
#define GPIOA (&test_gpioa)
#define USB_OTG_FS (&test_otg)
#define USB_OTG_FS_PERIPH_BASE ((uintptr_t)&test_otg_device)
#define USB_OTG_DEVICE_BASE 0u
#define DWT_CTRL_CYCCNTENA_Msk 1u
#define CoreDebug_DEMCR_TRCENA_Msk (1u << 24)
#define RCC_CR_HSION 1u
#define RCC_CR_HSIRDY 2u
#define RCC_CR_HSEON (1u << 16)
#define RCC_CR_HSERDY (1u << 17)
#define RCC_CR_HSEBYP (1u << 18)
#define RCC_CR_CSSON (1u << 19)
#define RCC_CR_PLLON (1u << 24)
#define RCC_CR_PLLRDY (1u << 25)
#define RCC_CFGR_SW 3u
#define RCC_CFGR_SW_PLL 2u
#define RCC_CFGR_SWS (3u << 2)
#define RCC_CFGR_SWS_HSI 0u
#define RCC_CFGR_SWS_PLL (2u << 2)
#define RCC_CFGR_HPRE (15u << 4)
#define RCC_CFGR_PPRE1 (7u << 10)
#define RCC_CFGR_PPRE1_DIV2 (4u << 10)
#define RCC_CFGR_PPRE2 (7u << 13)
#define RCC_AHB1ENR_GPIOAEN 1u
#define RCC_AHB2ENR_OTGFSEN (1u << 7)
#define RCC_AHB2RSTR_OTGFSRST (1u << 7)
#define FLASH_ACR_LATENCY 15u
#define FLASH_ACR_LATENCY_2WS 2u
#define FLASH_ACR_PRFTEN (1u << 8)
#define FLASH_ACR_ICEN (1u << 9)
#define FLASH_ACR_DCEN (1u << 10)
typedef enum { OTG_FS_IRQn = 67 } IRQn_Type;
static inline void NVIC_SetPriority(IRQn_Type irq, uint32_t priority) { (void)irq; (void)priority; }
static inline uint32_t SysTick_Config(uint32_t ticks) { (void)ticks; return 0u; }
#endif
