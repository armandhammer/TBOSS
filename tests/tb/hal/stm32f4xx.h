/* SPDX-License-Identifier: GPL-3.0-or-later - minimal native MMIO storage, no real MMIO. */
#ifndef TC_TEST_MMIO
#define TC_TEST_MMIO
#include <stdint.h>
typedef struct { volatile uint32_t CR1,SR1,SR2,DR,CR2,CCR,TRISE,OAR1,OAR2; } tc_test_i2c;
typedef struct { volatile uint32_t CFGR,AHB1ENR,APB1ENR,APB1RSTR; } tc_test_rcc;
typedef struct { volatile uint32_t AFR[2],OTYPER,OSPEEDR,PUPDR,MODER,IDR,BSRR; } tc_test_gpio;
typedef struct { volatile uint32_t CTRL,CYCCNT; } tc_test_dwt;
extern tc_test_i2c test_i2c;
extern tc_test_rcc test_rcc;
extern tc_test_gpio test_gpiob;
extern tc_test_dwt test_dwt;
extern uint32_t SystemCoreClock, test_irq;
#define I2C1 (&test_i2c)
#define RCC (&test_rcc)
#define GPIOB (&test_gpiob)
#define DWT (&test_dwt)
#define DWT_CTRL_CYCCNTENA_Msk 1u
#define RCC_AHB1ENR_GPIOBEN 2u
#define RCC_APB1ENR_I2C1EN (1u<<21)
#define RCC_APB1RSTR_I2C1RST (1u<<21)
static inline void __DSB(void) {}
static inline uint32_t __get_PRIMASK(void) { return test_irq; }
static inline void __disable_irq(void) { test_irq=1; }
static inline void __set_PRIMASK(uint32_t mask) { test_irq=mask; }
#endif
