/* SPDX-License-Identifier: GPL-3.0-or-later
 * Original TheoryBoard port: only HSE/PLL, SysTick and PA11/12 USB FS.
 * No keyboard, LED, button, NOR, updater, flash array or option operations. */
#include "stm32f4xx.h"
#include "tusb.h"
#include "diagnostic.h"
#include "clock.h"
uint32_t SystemCoreClock = 16000000u;
static bool wait_bits(volatile uint32_t *reg, uint32_t mask, uint32_t value) {
    /* Bounded loops even before SysTick; generous oscillator settling budget. */
    for (uint32_t n = 0; n < 8000000u; ++n) {
        if ((*reg & mask) == value) return true;
    }
    return false;
}
uint32_t tb_board_init(void) {
    /* Entry contract: reset-caught HSI, Thread mode. Do not inherit PLL setup. */
    if ((RCC->CFGR & RCC_CFGR_SWS) != RCC_CFGR_SWS_HSI ||
        (RCC->CR & (RCC_CR_HSION | RCC_CR_HSIRDY)) != (RCC_CR_HSION | RCC_CR_HSIRDY)) return 0x101u;
    RCC->CR &= ~(RCC_CR_PLLON | RCC_CR_HSEON | RCC_CR_CSSON);
    if (!wait_bits(&RCC->CR, RCC_CR_PLLRDY | RCC_CR_HSERDY, 0)) return 0x102u;
    RCC->CR &= ~RCC_CR_HSEBYP; /* evidence supports crystal, not an external MCO */
    RCC->CR |= RCC_CR_HSEON;
    if (!wait_bits(&RCC->CR, RCC_CR_HSERDY, RCC_CR_HSERDY)) return 0x103u;
    RCC->PLLCFGR = TB_PLL_REGISTER;
    RCC->CR |= RCC_CR_PLLON;
    if (!wait_bits(&RCC->CR, RCC_CR_PLLRDY, RCC_CR_PLLRDY)) return 0x104u;
    /* Volatile access-latency register only: no flash array writes/unlock. */
    FLASH->ACR = (FLASH->ACR & ~FLASH_ACR_LATENCY) | FLASH_ACR_LATENCY_2WS;
    if ((FLASH->ACR & FLASH_ACR_LATENCY) != FLASH_ACR_LATENCY_2WS) return 0x105u;
    RCC->CFGR = (RCC->CFGR & ~(RCC_CFGR_HPRE | RCC_CFGR_PPRE1 | RCC_CFGR_PPRE2 | RCC_CFGR_SW)) |
                RCC_CFGR_PPRE1_DIV2 | RCC_CFGR_SW_PLL;
    if (!wait_bits(&RCC->CFGR, RCC_CFGR_SWS, RCC_CFGR_SWS_PLL)) return 0x106u;
    SystemCoreClock = TB_CORE_HZ; /* HCLK48, PCLK1 24, PCLK2 48 MHz */
    RCC->AHB1ENR |= RCC_AHB1ENR_GPIOAEN;
    (void)RCC->AHB1ENR; /* peripheral clock enable propagation */
    /* PA11 DM / PA12 DP AF10, push-pull, very high speed, no pulls. */
    GPIOA->MODER = (GPIOA->MODER & ~((3u << 22) | (3u << 24))) | (2u << 22) | (2u << 24);
    GPIOA->OTYPER &= ~((1u << 11) | (1u << 12));
    GPIOA->OSPEEDR |= (3u << 22) | (3u << 24);
    GPIOA->PUPDR &= ~((3u << 22) | (3u << 24));
    GPIOA->AFR[1] = (GPIOA->AFR[1] & ~((15u << 12) | (15u << 16))) | (10u << 12) | (10u << 16);
    RCC->AHB2ENR |= RCC_AHB2ENR_OTGFSEN;
    (void)RCC->AHB2ENR;
    RCC->AHB2RSTR |= RCC_AHB2RSTR_OTGFSRST;
    __DSB();
    RCC->AHB2RSTR &= ~RCC_AHB2RSTR_OTGFSRST;
    __DSB();
    /* PA9 routing is unverified. Dedicated powered-device bench build uses
     * TinyUSB's documented no-VBUS-sense device override; PA9/PA10 untouched.
     * No VBUS drive or host mode. Cable presence must be controlled manually. */
    tud_configure_param_t cfg = { .dwc2 = CFG_TUD_CONFIGURE_DWC2_DEFAULT };
    cfg.dwc2.vbus_sensing = false;
    if (!tud_configure(0u, TUD_CFGID_DWC2, &cfg)) return 0x107u;
    NVIC_SetPriority(OTG_FS_IRQn, 2u);
    if (SysTick_Config(SystemCoreClock / 1000u) != 0u) return 0x108u;
    tb_status.core_hz = TB_CORE_HZ;
    tb_status.usb_hz = TB_USB_HZ;
    return 0;
}
void SysTick_Handler(void) { tb_status.millis++; }
void OTG_FS_IRQHandler(void) { tb_status.usb_irqs++; tusb_int_handler(0u, true); }
uint32_t tb_millis(void) { return tb_status.millis; }
uint32_t tusb_time_millis_api(void) { return tb_millis(); }
void tb_snapshot(void) {
    tb_status.rcc_cr = RCC->CR;
    tb_status.rcc_pllcfgr = RCC->PLLCFGR;
    tb_status.rcc_cfgr = RCC->CFGR;
    if (RCC->AHB2ENR & RCC_AHB2ENR_OTGFSEN) {
        tb_status.gccfg = USB_OTG_FS->GCCFG;
        tb_status.gusbcfg = USB_OTG_FS->GUSBCFG;
        tb_status.gintsts = USB_OTG_FS->GINTSTS;
        tb_status.gotgctl = USB_OTG_FS->GOTGCTL;
        tb_status.dcfg = ((USB_OTG_DeviceTypeDef *)(USB_OTG_FS_PERIPH_BASE + USB_OTG_DEVICE_BASE))->DCFG;
    }
}
