/* SPDX-License-Identifier: GPL-3.0-or-later - concrete STM32F407 I2C1 binding. */
#include "stm32f4xx.h"
#include "i2c.h"
static uint32_t read_reg(void *context, enum tc_reg r) {
    (void)context;
    switch (r) {
    case TC_CR1: return I2C1->CR1;
    case TC_SR1: return I2C1->SR1;
    case TC_SR2: return I2C1->SR2;
    case TC_DR: return I2C1->DR;
    }
    return 0;
}
static void write_reg(void *context, enum tc_reg r, uint32_t value) {
    (void)context;
    switch (r) {
    case TC_CR1: I2C1->CR1=value; break;
    case TC_SR1: I2C1->SR1=value; break;
    case TC_DR: I2C1->DR=value; break;
    case TC_SR2: break;
    }
}
static uint32_t cycles_last, elapsed, fraction;
static uint32_t micros(void *context) {
    (void)context;
    uint32_t cycles=DWT->CYCCNT, delta=cycles-cycles_last;
    cycles_last=cycles; elapsed+=delta/48u; fraction+=delta%48u;
    elapsed+=fraction/48u; fraction%=48u; return elapsed;
}
static uint32_t lock(void *context) {
    (void)context; uint32_t mask=__get_PRIMASK(); __disable_irq(); return mask;
}
static void unlock(void *context, uint32_t mask) { (void)context; __set_PRIMASK(mask); }
/* PB8=SCL, PB9=SDA as open-drain GPIO for the startup bus clear only. */
static void pin(uint32_t bit, bool high) { GPIOB->BSRR=high ? bit : bit<<16; }
static void set_scl(void *context, bool high) { (void)context; pin(1u<<8,high); }
static void set_sda(void *context, bool high) { (void)context; pin(1u<<9,high); }
static bool read_scl(void *context) { (void)context; return (GPIOB->IDR>>8)&1u; }
static bool read_sda(void *context) { (void)context; return (GPIOB->IDR>>9)&1u; }
static void delay_us(void *context, uint32_t us) {
    (void)context;
    /* Iteration ceiling keeps the wait finite even if CYCCNT is not counting. */
    uint32_t start=DWT->CYCCNT, cycles=us*48u;
    for (uint32_t n=0;n<cycles && DWT->CYCCNT-start<cycles;n++) {}
}
bool tc_hardware_recover(tc_bus *b) {
    RCC->APB1RSTR |= RCC_APB1RSTR_I2C1RST; __DSB();
    RCC->APB1RSTR &= ~RCC_APB1RSTR_I2C1RST; __DSB();
    return tc_hardware_init(b);
}
bool tc_hardware_init(tc_bus *b) {
    uint32_t cr2, ccr, trise;
    *b=(tc_bus){0};
    if (!tc_timing(SystemCoreClock,RCC->CFGR,&cr2,&ccr,&trise) ||
        !(DWT->CTRL&DWT_CTRL_CYCCNTENA_Msk)) return false;
    RCC->AHB1ENR |= RCC_AHB1ENR_GPIOBEN;
    RCC->APB1ENR |= RCC_APB1ENR_I2C1EN;
    (void)RCC->APB1ENR;
    /* Dedicated SRAM startup from reset; no inherited-active I2C takeover. */
    if (I2C1->CR1!=0u) return false;
    GPIOB->AFR[1]=(GPIOB->AFR[1]&~0xffu)|0x44u;
    GPIOB->OTYPER |= 0x300u;
    GPIOB->OSPEEDR=(GPIOB->OSPEEDR&~0xf0000u)|0x50000u;
    /* Weak pull-ups match firmware evidence; external bus pull-ups presumed. */
    GPIOB->PUPDR=(GPIOB->PUPDR&~0xf0000u)|0x50000u;
    GPIOB->BSRR=0x300u; /* released (high) before the pins become outputs */
    GPIOB->MODER=(GPIOB->MODER&~0xf0000u)|0x50000u;
    const tc_pins pins={0,set_scl,set_sda,read_scl,read_sda,delay_us};
    uint32_t recovery=tc_bus_clear(&pins);
    GPIOB->MODER=(GPIOB->MODER&~0xf0000u)|0xa0000u;
    I2C1->CR2=cr2; I2C1->CCR=ccr; I2C1->TRISE=trise;
    I2C1->OAR1=0x4000u; /* RM0090: bit14 kept at one, seven-bit mode. */
    I2C1->OAR2=0u;
    I2C1->CR1=TC_PE;
    cycles_last=DWT->CYCCNT; elapsed=0; fraction=0;
    tc_bus_init(b,(tc_io){0,read_reg,write_reg,micros,lock,unlock});
    b->recovery=recovery;
    return true;
}
