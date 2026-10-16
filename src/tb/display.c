/* SPDX-License-Identifier: GPL-3.0-or-later - SSD1306 driver: page framebuffer, one page per pass.
 * Init sequence and reset timing as used by the board's original firmware. */
#include "display.h"
#include "diagnostic.h"
#include "stm32f4xx.h"
static uint8_t fb[TD_PAGES][TD_WIDTH];
static uint8_t dirty; /* bit per page */
static bool ready;
static bool wait_flag(volatile uint32_t *reg, uint32_t mask, uint32_t value) {
    for (uint32_t n = 0; n < 20000u; ++n) if ((*reg & mask) == value) return true;
    return false;
}
static void delay_ms(uint32_t ms) {
    uint32_t start = tb_millis();
    for (uint32_t n = 0; n < 2000000u && tb_millis() - start < ms; ++n) {}
}
static bool send(uint8_t byte) {
    if (!wait_flag(&SPI3->SR, SPI_SR_TXE, SPI_SR_TXE)) return false;
    SPI3->DR = byte;
    return true;
}
static bool finish(void) {
    return wait_flag(&SPI3->SR, SPI_SR_TXE, SPI_SR_TXE) && wait_flag(&SPI3->SR, SPI_SR_BSY, 0u);
}
static void cs(bool on) { GPIOD->BSRR = on ? (1u << (14u + 16u)) : (1u << 14); }
static void data_mode(bool data) { GPIOD->BSRR = data ? (1u << 11) : (1u << (11u + 16u)); }
static bool command(const uint8_t *bytes, uint32_t count) {
    cs(true); data_mode(false);
    bool ok = true;
    for (uint32_t i = 0; i < count && ok; ++i) ok = send(bytes[i]);
    ok = finish() && ok;
    cs(false);
    return ok;
}
bool td_init(void) {
    RCC->AHB1ENR |= RCC_AHB1ENR_GPIOAEN | RCC_AHB1ENR_GPIOBEN | RCC_AHB1ENR_GPIODEN;
    (void)RCC->AHB1ENR;
    /* NOR select PA15: high (deselected) first, then push-pull output with pull-up. */
    GPIOA->BSRR = 1u << 15;
    GPIOA->PUPDR = (GPIOA->PUPDR & ~(3u << 30)) | (1u << 30);
    GPIOA->MODER = (GPIOA->MODER & ~(3u << 30)) | (1u << 30);
    /* Display select high, reset low, data/command low; then outputs. */
    GPIOD->BSRR = (1u << 14) | (1u << (12u + 16u)) | (1u << (11u + 16u));
    GPIOD->MODER = (GPIOD->MODER & ~((3u << 22) | (3u << 24) | (3u << 28))) |
                   (1u << 22) | (1u << 24) | (1u << 28);
    /* SPI3 SCK PB3, MOSI PB5: AF6, high speed. MISO (PB4) is not needed. */
    GPIOB->AFR[0] = (GPIOB->AFR[0] & ~((15u << 12) | (15u << 20))) | (6u << 12) | (6u << 20);
    GPIOB->OSPEEDR |= (2u << 6) | (2u << 10);
    GPIOB->MODER = (GPIOB->MODER & ~((3u << 6) | (3u << 10))) | (2u << 6) | (2u << 10);
    RCC->APB1ENR |= RCC_APB1ENR_SPI3EN;
    (void)RCC->APB1ENR;
    /* Master, mode 0, 8-bit, MSB first, PCLK1/8 = 3 MHz, software NSS. */
    SPI3->CR1 = SPI_CR1_MSTR | SPI_CR1_BR_1 | SPI_CR1_SSM | SPI_CR1_SSI;
    SPI3->CR2 = 0u;
    SPI3->CR1 |= SPI_CR1_SPE;
    delay_ms(10u);
    GPIOD->BSRR = 1u << 12; /* release reset */
    delay_ms(10u);
    static const uint8_t init[] = {
        0xAE,             /* display off */
        0x20, 0x02,       /* page addressing */
        0xB0, 0xC0,       /* page 0, COM scan normal */
        0x00, 0x10, 0x40, /* column 0, start line 0 */
        0x81, 0xBB,       /* contrast */
        0xA0, 0xA6,       /* segment normal, normal (not inverted) */
        0xA8, 0x3F,       /* multiplex 64 */
        0xA4,             /* follow RAM */
        0xD3, 0x00,       /* no display offset */
        0xD5, 0xF0,       /* clock */
        0xD9, 0x22,       /* precharge */
        0xDA, 0x12,       /* COM pins */
        0xDB, 0x20,       /* VCOMH */
        0x8D, 0x14,       /* charge pump on */
        0xAF              /* display on */
    };
    ready = command(init, sizeof init);
    td_clear();
    return ready;
}
void td_clear(void) {
    for (uint32_t p = 0; p < TD_PAGES; ++p) for (uint32_t x = 0; x < TD_WIDTH; ++x) fb[p][x] = 0u;
    dirty = 0xFFu;
}
void td_pixel(uint32_t x, uint32_t y, bool on) {
    if (x >= TD_WIDTH || y >= TD_PAGES * 8u) return;
    uint8_t bit = (uint8_t)(1u << (y & 7u)), *b = &fb[y >> 3][x];
    uint8_t v = on ? (uint8_t)(*b | bit) : (uint8_t)(*b & ~bit);
    if (v != *b) { *b = v; dirty |= (uint8_t)(1u << (y >> 3)); }
}
void td_invert(uint32_t x0, uint32_t y0, uint32_t x1, uint32_t y1) {
    for (uint32_t y = y0; y < y1 && y < TD_PAGES * 8u; ++y)
        for (uint32_t x = x0; x < x1 && x < TD_WIDTH; ++x) {
            fb[y >> 3][x] ^= (uint8_t)(1u << (y & 7u)); dirty |= (uint8_t)(1u << (y >> 3));
        }
}
static const uint8_t *glyph(char c) {
    for (uint32_t i = 0; td_font_chars[i]; ++i) if (td_font_chars[i] == c) return td_font_rows[i];
    return c == '?' ? td_font_rows[0] : glyph('?');
}
uint32_t td_text(uint32_t x, uint32_t y, const char *text, uint32_t scale) {
    for (; *text; ++text) {
        const uint8_t *rows = glyph(*text);
        for (uint32_t r = 0; r < 8u; ++r) for (uint32_t c = 0; c < 6u; ++c) {
            bool on = r < 7u && c < 5u && ((rows[r] >> (4u - c)) & 1u);
            for (uint32_t dy = 0; dy < scale; ++dy) for (uint32_t dx = 0; dx < scale; ++dx)
                td_pixel(x + c * scale + dx, y + r * scale + dy, on);
        }
        x += 6u * scale;
    }
    return x;
}
void td_service(void) {
    if (!ready || !dirty) return;
    uint32_t page = 0;
    while (!(dirty & (1u << page))) ++page;
    dirty = (uint8_t)(dirty & ~(1u << page));
    const uint8_t address[3] = {(uint8_t)(0xB0u + page), 0x00u, 0x10u};
    if (!command(address, 3u)) { ready = false; return; }
    cs(true); data_mode(true);
    bool ok = true;
    for (uint32_t x = 0; x < TD_WIDTH && ok; ++x) ok = send(fb[page][x]);
    ok = finish() && ok;
    cs(false);
    if (!ok) ready = false; /* a stuck SPI never blocks the instrument */
}
