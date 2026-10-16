/* SPDX-License-Identifier: GPL-3.0-or-later - restore reset state after the resident loader.
 * The loader (0x0800BB4C) jumps here with its clocks and peripherals running
 * (likely PLL, USB HID, SPI). Everything downstream assumes a cold reset: board.c
 * requires HSI as SYSCLK, tc_hardware_init refuses an active I2C1. Called from
 * Reset_Handler before RAM init: stack only, no globals.
 * Never touches PWR, FLASH, DBGMCU or option bytes; resetting GPIOA returns
 * PA13/PA14 to their reset-default SWD function. */
#include "stm32f4xx.h"
static void wait_bits(volatile uint32_t *reg, uint32_t mask, uint32_t value) {
    /* Bounded: a missing oscillator must not hang before the debugger can see us. */
    for (uint32_t n = 0; n < 1000000u && (*reg & mask) != value; ++n) {}
}
static void pulse(volatile uint32_t *reset, uint32_t bits) {
    *reset = bits; __DSB();
    *reset = 0u; __DSB();
}
void tb_flash_reset(void) {
    RCC->CR |= RCC_CR_HSION;
    wait_bits(&RCC->CR, RCC_CR_HSIRDY, RCC_CR_HSIRDY);
    RCC->CFGR = 0u; /* SYSCLK=HSI, no prescalers, MCO defaults */
    wait_bits(&RCC->CFGR, RCC_CFGR_SWS, RCC_CFGR_SWS_HSI);
    RCC->CR &= ~(RCC_CR_PLLON | RCC_CR_PLLI2SON | RCC_CR_CSSON | RCC_CR_HSEON);
    wait_bits(&RCC->CR, RCC_CR_PLLRDY | RCC_CR_PLLI2SRDY | RCC_CR_HSERDY, 0u);
    RCC->CR &= ~RCC_CR_HSEBYP;
    RCC->PLLCFGR = 0x24003010u;    /* RM0090 reset values */
    RCC->PLLI2SCFGR = 0x20003000u;
    RCC->CIR = 0x00BF0000u;        /* clear ready/CSS flags, interrupts off */
    pulse(&RCC->AHB1RSTR, RCC_AHB1RSTR_GPIOARST | RCC_AHB1RSTR_GPIOBRST | RCC_AHB1RSTR_GPIOCRST |
        RCC_AHB1RSTR_GPIODRST | RCC_AHB1RSTR_GPIOERST | RCC_AHB1RSTR_GPIOFRST | RCC_AHB1RSTR_GPIOGRST |
        RCC_AHB1RSTR_GPIOHRST | RCC_AHB1RSTR_GPIOIRST | RCC_AHB1RSTR_CRCRST | RCC_AHB1RSTR_DMA1RST |
        RCC_AHB1RSTR_DMA2RST | RCC_AHB1RSTR_ETHMACRST | RCC_AHB1RSTR_OTGHRST);
    pulse(&RCC->AHB2RSTR, RCC_AHB2RSTR_DCMIRST | RCC_AHB2RSTR_RNGRST | RCC_AHB2RSTR_OTGFSRST);
    pulse(&RCC->AHB3RSTR, RCC_AHB3RSTR_FSMCRST);
    pulse(&RCC->APB1RSTR, RCC_APB1RSTR_TIM2RST | RCC_APB1RSTR_TIM3RST | RCC_APB1RSTR_TIM4RST |
        RCC_APB1RSTR_TIM5RST | RCC_APB1RSTR_TIM6RST | RCC_APB1RSTR_TIM7RST | RCC_APB1RSTR_TIM12RST |
        RCC_APB1RSTR_TIM13RST | RCC_APB1RSTR_TIM14RST | RCC_APB1RSTR_WWDGRST | RCC_APB1RSTR_SPI2RST |
        RCC_APB1RSTR_SPI3RST | RCC_APB1RSTR_USART2RST | RCC_APB1RSTR_USART3RST | RCC_APB1RSTR_UART4RST |
        RCC_APB1RSTR_UART5RST | RCC_APB1RSTR_I2C1RST | RCC_APB1RSTR_I2C2RST | RCC_APB1RSTR_I2C3RST |
        RCC_APB1RSTR_CAN1RST | RCC_APB1RSTR_CAN2RST | RCC_APB1RSTR_DACRST);
    pulse(&RCC->APB2RSTR, RCC_APB2RSTR_TIM1RST | RCC_APB2RSTR_TIM8RST | RCC_APB2RSTR_USART1RST |
        RCC_APB2RSTR_USART6RST | RCC_APB2RSTR_ADCRST | RCC_APB2RSTR_SDIORST | RCC_APB2RSTR_SPI1RST |
        RCC_APB2RSTR_SYSCFGRST | RCC_APB2RSTR_TIM9RST | RCC_APB2RSTR_TIM10RST | RCC_APB2RSTR_TIM11RST);
    RCC->AHB1ENR = RCC_AHB1ENR_CCMDATARAMEN; /* reset value */
    RCC->AHB2ENR = 0u;
    RCC->AHB3ENR = 0u;
    RCC->APB1ENR = 0u;
    RCC->APB2ENR = 0u;
    __DSB(); __ISB();
}
