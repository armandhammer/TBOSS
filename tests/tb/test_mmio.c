/* SPDX-License-Identifier: GPL-3.0-or-later - compile real touch_hw.c against storage-only CMSIS substitute. */
#include "hal/stm32f4xx.h"
#include "../../src/tb/i2c.h"
#include <assert.h>
#include <stdio.h>
tc_test_i2c test_i2c;
tc_test_rcc test_rcc;
tc_test_gpio test_gpiob;
tc_test_dwt test_dwt;
uint32_t SystemCoreClock=48000000u, test_irq;
int main(void) {
    tc_bus b;
    test_rcc=(tc_test_rcc){0x100a,0x10000,0x400,0};
    test_dwt.CTRL=1;
    test_gpiob=(tc_test_gpio){{0x12345678,0xa5a5a5a5},0x5a5a,0x5a5a5a5a,0xa5a5a5a5,0x5a5a5a5a,0x300,0};
    tc_test_gpio before=test_gpiob;
    assert(tc_hardware_init(&b) && b.enabled && !b.latched && b.recovery==0);
    assert(test_gpiob.BSRR==0x100); /* idle bus: both lines released, no pulses */
    assert(test_i2c.CR1==TC_PE && test_i2c.CR2==24 && test_i2c.CCR==(0x8000|20) && test_i2c.TRISE==8);
    assert(test_i2c.OAR1==0x4000 && test_i2c.OAR2==0);
    assert(test_rcc.AHB1ENR==(0x10000|2) && test_rcc.APB1ENR==(0x400|(1u<<21)));
    assert(test_gpiob.AFR[0]==before.AFR[0]);
    assert(test_gpiob.AFR[1]==((before.AFR[1]&~0xffu)|0x44u));
    assert(test_gpiob.OTYPER==(before.OTYPER|0x300u));
    assert(test_gpiob.OSPEEDR==((before.OSPEEDR&~0xf0000u)|0x50000u));
    assert(test_gpiob.PUPDR==((before.PUPDR&~0xf0000u)|0x50000u));
    assert(test_gpiob.MODER==((before.MODER&~0xf0000u)|0xa0000u));
    assert(!test_irq);
    /* No clock progress: actual bound still returns, without interrupt leakage. */
    assert(tc_i2c_read(&b,0x39,0xaa,1,1,1000).status==BI_TIMEOUT && b.latched && !test_irq);
    assert(!tc_hardware_init(&b)); /* refuse inherited-active controller */
    /* Lines read low with a frozen cycle counter: bounded, reported, AF still set. */
    test_i2c.CR1=0; test_gpiob.IDR=0;
    assert(tc_hardware_init(&b) && b.recovery==TC_CLEAR_SCL_STUCK && !b.latched);
    assert(test_gpiob.MODER==((before.MODER&~0xf0000u)|0xa0000u) && test_i2c.CR1==TC_PE);
    /* Mid-session recovery: I2C1 reset pulse, then the full init again. */
    test_i2c.CR1=TC_PE; b.latched=true; test_gpiob.IDR=0x300; test_rcc.APB1RSTR=0;
    test_i2c.CR1=0; /* the RCC reset clears CR1 on hardware; storage-only fake */
    assert(tc_hardware_recover(&b) && !b.latched && b.enabled && b.recovery==0);
    assert(!(test_rcc.APB1RSTR&RCC_APB1RSTR_I2C1RST) && test_i2c.CR1==TC_PE);
    test_i2c.CR1=0; SystemCoreClock=16000000;
    assert(!tc_hardware_init(&b) && !b.enabled);
    puts("PASS: startup bus clear via GPIO (idle and stuck/frozen-clock), I2C1 reset recovery, concrete hardware binding, RCC/pin preservation, 24MHz/400kHz timing, IRQ restoration, frozen-clock bound, clock/inherited-state rejection");
    return 0;
}
