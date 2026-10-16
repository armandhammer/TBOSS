/* SPDX-License-Identifier: GPL-3.0-or-later - original ADC1/pin subset, no DMA/bus access. */
#include "stm32f4xx.h"
#include "scan.h"
static GPIO_TypeDef *const ports[5] = {GPIOA, GPIOB, GPIOC, GPIOD, GPIOE};
static uint32_t cycles_last, us_total, fraction_cycles;
static uint32_t micros(void) {
    uint32_t now = DWT->CYCCNT;
    uint32_t delta = now - cycles_last;
    cycles_last = now;
    us_total += delta / 48u;
    fraction_cycles += delta % 48u;
    us_total += fraction_cycles / 48u;
    fraction_cycles %= 48u;
    return us_total;
}
static bool settle(uint32_t us) {
    uint32_t start = micros();
    for (uint32_t tries = 0; tries < 200000u; ++tries) {
        if (micros() - start >= us) return true;
    }
    return false;
}
static void select_position(uint8_t p) {
    uint32_t reset[5], set[5];
    ts_masks(p, reset, set);
    /* Per-port zero bits reset, then one bits set, matching F6B writer. */
    for (uint32_t port = 0; port < 5u; ++port) {
        ports[port]->BSRR = reset[port] << 16;
        ports[port]->BSRR = set[port];
    }
    __DSB();
}
static void pin_mode(ts_pin pin, uint32_t mode) {
    GPIO_TypeDef *port = ports[pin.port];
    uint32_t shift = (uint32_t)pin.bit * 2u;
    port->PUPDR &= ~(3u << shift);
    if (mode == 1u) {
        port->OTYPER &= ~(1u << pin.bit);
        port->OSPEEDR &= ~(3u << shift); /* low slew, no unverified fast waveform */
    }
    port->MODER = (port->MODER & ~(3u << shift)) | (mode << shift);
}
static bool sample(uint8_t bank, uint32_t timeout_us, uint16_t *value) {
    /* Each single-rank conversion is separately started and drained. Thus no
     * six-rank overrun race against foreground polling. Three rounds retain order. */
    ADC1->SR = 0u;
    (void)ADC1->DR;
    ADC1->SQR3 = ts_channels[bank];
    ADC1->CR2 = ADC_CR2_ADON | ADC_CR2_SWSTART;
    uint32_t start = micros();
    for (uint32_t tries = 0; tries < 200000u; ++tries) {
        uint32_t sr = ADC1->SR;
        if (sr & ADC_SR_OVR) break;
        if (sr & ADC_SR_EOC) {
            *value = (uint16_t)ADC1->DR;
            ts_status.adc_sr = sr;
            return micros() - start <= timeout_us;
        }
        if (micros() - start >= timeout_us) break;
    }
    ts_status.adc_sr = ADC1->SR;
    ADC1->CR2 = 0u; /* cancel incomplete conversion before reuse */
    ADC1->SR = 0u;
    ADC1->CR2 = ADC_CR2_ADON;
    (void)settle(10u);
    return false;
}
bool ts_hardware_init(void) {
    CoreDebug->DEMCR |= CoreDebug_DEMCR_TRCENA_Msk;
    DWT->CYCCNT = 0u;
    DWT->CTRL |= DWT_CTRL_CYCCNTENA_Msk;
    cycles_last = DWT->CYCCNT;
    if (!settle(10u)) return false;
    RCC->AHB1ENR |= RCC_AHB1ENR_GPIOAEN | RCC_AHB1ENR_GPIOBEN | RCC_AHB1ENR_GPIOCEN |
                   RCC_AHB1ENR_GPIODEN | RCC_AHB1ENR_GPIOEEN;
    (void)RCC->AHB1ENR;
    select_position(0u); /* pre-load output latches before exposing the drivers */
    for (uint32_t bank = 0; bank < 6u; ++bank) {
        pin_mode(ts_inputs[bank], 3u); /* only the six documented analog inputs */
        for (uint32_t bit = 0; bit < 4u; ++bit) pin_mode(ts_selectors[bank][bit], 1u);
    }
    RCC->APB2ENR |= RCC_APB2ENR_ADC1EN;
    (void)RCC->APB2ENR;
    ADC1->CR2 = 0u; ADC1->CR1 = 0u;
    /* Independent ADC1, PCLK2/4=12MHz, no DMA, external trigger, continuous scan
     * or IRQ. Preserve common internal-source controls, configure prescaler only. */
    ADC->CCR = (ADC->CCR & ~(ADC_CCR_ADCPRE | ADC_CCR_MULTI | ADC_CCR_DMA | ADC_CCR_DDS)) |
               ADC_CCR_ADCPRE_0;
    ADC1->SMPR1 = 7u; /* channel10: 480 sample clocks */
    ADC1->SMPR2 = (7u << 9) | (7u << 12) | (7u << 15) | (7u << 18) | (7u << 24);
    ADC1->SQR1 = 0u; ADC1->SQR2 = 0u; ADC1->SQR3 = 10u;
    ADC1->SR = 0u; ADC1->CR2 = ADC_CR2_ADON;
    ts_status.adc_cr2 = ADC1->CR2; ts_status.adc_ccr = ADC->CCR;
    return settle(10u); /* >= ADC stabilization interval; bounded if DWT stops */
}
const ts_ops ts_hardware_ops = {select_position, sample, settle, micros};
