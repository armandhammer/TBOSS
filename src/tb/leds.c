/* SPDX-License-Identifier: GPL-3.0-or-later - LED drivers. Pad frame: one 16-bit SPI2 word per
 * LED bit at 12 MHz (83.3 ns per SPI bit): 0 -> 4 high bits (333 ns), 1 -> 9 high
 * bits (750 ns), 1.33 us per LED bit; then 240 zero words (320 us reset low).
 * DMA1 Stream4 channel 0 (SPI2_TX), normal mode: a frame is encoded only while no
 * transfer is running, so the DMA never reads a half-written buffer. */
#include "leds.h"
#include "stm32f4xx.h"
#define TL_ZERO 0xF000u
#define TL_ONE 0xFF80u
#define TL_RESET_WORDS 240u
#define TL_WORDS (TL_PADS * 24u + TL_RESET_WORDS)
static uint16_t frame[TL_WORDS];
static tl_rgb pads[TL_PADS];
static uint16_t buttons[TL_BUTTONS];
static bool pads_dirty, buttons_dirty, started;
bool tl_init(void) {
    RCC->AHB1ENR |= RCC_AHB1ENR_GPIOCEN | RCC_AHB1ENR_GPIOEEN | RCC_AHB1ENR_DMA1EN;
    RCC->APB1ENR |= RCC_APB1ENR_SPI2EN;
    (void)RCC->APB1ENR;
    /* TLC5947: PE3 latch, PE4 clock, PE5 data -> low, push-pull outputs. Only these
     * three bits of GPIOE; the scanner owns PE7..PE15. */
    GPIOE->BSRR = (7u << 3) << 16;
    GPIOE->OSPEEDR |= (2u << 6) | (2u << 8) | (2u << 10);
    GPIOE->MODER = (GPIOE->MODER & ~((3u << 6) | (3u << 8) | (3u << 10))) | (1u << 6) | (1u << 8) | (1u << 10);
    /* PC3 = SPI2 MOSI (AF5); idle low. */
    GPIOC->AFR[0] = (GPIOC->AFR[0] & ~(15u << 12)) | (5u << 12);
    GPIOC->OSPEEDR |= 2u << 6;
    GPIOC->MODER = (GPIOC->MODER & ~(3u << 6)) | (2u << 6);
    /* Master, 16-bit, PCLK1/2 = 12 MHz, software NSS, TX DMA. */
    SPI2->CR1 = SPI_CR1_MSTR | SPI_CR1_DFF | SPI_CR1_SSM | SPI_CR1_SSI;
    SPI2->CR2 = SPI_CR2_TXDMAEN;
    SPI2->CR1 |= SPI_CR1_SPE;
    DMA1_Stream4->CR = 0u;
    for (uint32_t n = 0; n < 10000u && (DMA1_Stream4->CR & DMA_SxCR_EN); ++n) {}
    DMA1_Stream4->PAR = (uint32_t)&SPI2->DR;
    DMA1_Stream4->M0AR = (uint32_t)frame;
    DMA1_Stream4->FCR = 0u;
    for (uint32_t i = 0; i < TL_PADS; ++i) pads[i] = (tl_rgb){0, 0, 0};
    for (uint32_t i = 0; i < TL_BUTTONS; ++i) buttons[i] = 0u;
    pads_dirty = buttons_dirty = true;
    return true;
}
void tl_set_pad(uint32_t pad, tl_rgb c) {
    if (pad >= TL_PADS) return;
    if (pads[pad].r != c.r || pads[pad].g != c.g || pads[pad].b != c.b) { pads[pad] = c; pads_dirty = true; }
}
void tl_set_button(uint32_t channel, uint16_t level) {
    if (channel >= TL_BUTTONS) return;
    if (level > 4095u) level = 4095u;
    if (buttons[channel] != level) { buttons[channel] = level; buttons_dirty = true; }
}
static bool dma_busy(void) {
    if (!started) return false;
    if (DMA1_Stream4->CR & DMA_SxCR_EN) return true;
    return !(SPI2->SR & SPI_SR_TXE) || (SPI2->SR & SPI_SR_BSY);
}
static void send_pads(void) {
    uint32_t w = 0;
    for (uint32_t i = 0; i < TL_PADS; ++i) {
        const uint8_t bytes[3] = {pads[i].g, pads[i].r, pads[i].b}; /* GRB on the wire */
        for (uint32_t b = 0; b < 3u; ++b)
            for (int32_t bit = 7; bit >= 0; --bit) frame[w++] = (bytes[b] >> bit) & 1u ? TL_ONE : TL_ZERO;
    }
    while (w < TL_WORDS) frame[w++] = 0u;
    DMA1->HIFCR = DMA_HIFCR_CTCIF4 | DMA_HIFCR_CHTIF4 | DMA_HIFCR_CTEIF4 | DMA_HIFCR_CDMEIF4 | DMA_HIFCR_CFEIF4;
    DMA1_Stream4->NDTR = TL_WORDS;
    DMA1_Stream4->CR = DMA_SxCR_DIR_0 | DMA_SxCR_MINC | DMA_SxCR_PSIZE_0 | DMA_SxCR_MSIZE_0; /* ch0, 16-bit */
    DMA1_Stream4->CR |= DMA_SxCR_EN;
    started = true;
}
static void send_buttons(void) {
    /* 288 bits, channel 23 first, 12 bits MSB first, then a latch pulse. */
    for (int32_t ch = (int32_t)TL_BUTTONS - 1; ch >= 0; --ch)
        for (int32_t bit = 11; bit >= 0; --bit) {
            GPIOE->BSRR = (buttons[ch] >> bit) & 1u ? (1u << 5) : (1u << (5u + 16u));
            __NOP(); __NOP(); __NOP(); __NOP();
            GPIOE->BSRR = 1u << 4;
            __NOP(); __NOP(); __NOP(); __NOP();
            GPIOE->BSRR = 1u << (4u + 16u);
        }
    __NOP(); __NOP(); __NOP(); __NOP();
    GPIOE->BSRR = 1u << 3;
    __NOP(); __NOP(); __NOP(); __NOP();
    GPIOE->BSRR = 1u << (3u + 16u);
}
void tl_service(void) {
    if (pads_dirty && !dma_busy()) { pads_dirty = false; send_pads(); }
    if (buttons_dirty) { buttons_dirty = false; send_buttons(); }
}
