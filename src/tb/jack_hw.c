/* SPDX-License-Identifier: GPL-3.0-or-later - TRS MIDI out (USART1 TX, PB6) and the pedal input
 * (PD13); see jack.h. The serial interrupt sends sm_out one byte at a time. */
#include "stm32f4xx.h"
#include "jack.h"
void jh_init(void) {
    RCC->AHB1ENR |= RCC_AHB1ENR_GPIOBEN | RCC_AHB1ENR_GPIODEN;
    RCC->APB2ENR |= RCC_APB2ENR_USART1EN;
    (void)RCC->APB2ENR;
    /* Pedal: input with pull-up (a pedal switches the tip to the sleeve). */
    GPIOD->PUPDR = (GPIOD->PUPDR & ~(3u << 26)) | (1u << 26);
    GPIOD->MODER &= ~(3u << 26);
    /* MIDI out: PB6 alternate function 7 (USART1_TX), push-pull, idles high. */
    GPIOB->AFR[0] = (GPIOB->AFR[0] & ~(15u << 24)) | (7u << 24);
    GPIOB->OTYPER &= ~(1u << 6);
    GPIOB->OSPEEDR = (GPIOB->OSPEEDR & ~(3u << 12)) | (1u << 12);
    GPIOB->MODER = (GPIOB->MODER & ~(3u << 12)) | (2u << 12);
    /* PCLK2 48 MHz (board.c): 48,000,000 / 31,250 = 1536, exact. 8N1, transmit only. */
    USART1->CR1 = 0u;
    USART1->CR2 = 0u;
    USART1->CR3 = 0u;
    USART1->BRR = 48000000u / 31250u;
    USART1->CR1 = USART_CR1_UE | USART_CR1_TE;
    NVIC_SetPriority(USART1_IRQn, 6u);
    NVIC_EnableIRQ(USART1_IRQn);
}
void jh_kick(void) { USART1->CR1 |= USART_CR1_TXEIE; }
void USART1_IRQHandler(void) {
    if (!(USART1->SR & USART_SR_TXE)) return;
    uint8_t byte;
    if (sm_pop(&sm_out, &byte)) USART1->DR = byte;
    else USART1->CR1 &= ~USART_CR1_TXEIE; /* empty: the next jh_kick restarts it */
}
uint32_t jh_levels(void) {
    uint32_t d = GPIOD->IDR, a = GPIOA->IDR;
    return ((d >> 13) & 1u) | ((a >> 10) & 1u) << 1 | ((d >> 5) & 1u) << 2;
}
