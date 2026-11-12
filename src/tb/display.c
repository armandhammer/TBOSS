/* SPDX-License-Identifier: GPL-3.0-or-later - SSD1306 driver: a page framebuffer; the pages that changed go to the
 * screen together in one DMA transfer (2.3.1; before, one page per main-loop pass, so a change drew top down over
 * several passes and a word across two pages came in two halves). Init sequence and reset timing as used by the board's
 * original firmware, in horizontal addressing (2.3.1; was page addressing). */
#include "display.h"
#include "diagnostic.h"
#include "stm32f4xx.h"
static uint8_t fb[TD_PAGES][TD_WIDTH];
/* What the screen shows: the DMA sends from here, so it changes only between transfers. */
static uint8_t shown[TD_PAGES][TD_WIDTH] TB_DMA;
static uint8_t dirty; /* bit per page: drawn on since the last transfer began */
static bool ready, everything = true; /* everything: the screen's RAM is unknown (after start-up), send it all */
static bool busy; /* a transfer runs: the display selected, SPI3 TX by DMA1 Stream 5 channel 0 */
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
        0x20, 0x00,       /* horizontal addressing: columns, then the next page (2.3.1) */
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
    RCC->AHB1ENR |= RCC_AHB1ENR_DMA1EN; /* 2.3.1: the frame by DMA (the LEDs' stream 4 is SPI2's) */
    (void)RCC->AHB1ENR;
    DMA1_Stream5->CR = 0u;
    busy = false; everything = true;
    td_clear();
    return ready;
}
/* A transfer that has ended: the last byte out, the display deselected, SPI3 back to plain writes (the NOR reads by
 * hand). False while it still runs. */
static bool settle(void) {
    if (!busy) return true;
    if (DMA1_Stream5->CR & DMA_SxCR_EN) return false;
    bool ok = finish();
    SPI3->CR2 &= ~SPI_CR2_TXDMAEN;
    cs(false);
    busy = false;
    if (!ok || (DMA1->HISR & DMA_HISR_TEIF5)) ready = false;
    return true;
}
void td_wait(void) {
    for (uint32_t n = 0; n < 2000000u && !settle(); ++n) {}
    if (!busy) return;
    DMA1_Stream5->CR &= ~DMA_SxCR_EN; /* stuck: stop it, the screen stays as it is */
    for (uint32_t n = 0; n < 20000u && (DMA1_Stream5->CR & DMA_SxCR_EN); ++n) {}
    SPI3->CR2 &= ~SPI_CR2_TXDMAEN;
    cs(false);
    busy = false; ready = false;
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
    /* 2.4.3: each character is searched for once; then its place is remembered (1 + its place in the font; 0: not
     * looked up yet). The search was a third of what drawing a character cost. */
    static uint8_t place[128];
    uint32_t code = (uint8_t)c;
    if (code < 128u && place[code]) return td_font_rows[place[code] - 1u];
    for (uint32_t i = 0; td_font_chars[i]; ++i) if (td_font_chars[i] == c) {
        if (code < 128u && i < 255u) place[code] = (uint8_t)(i + 1u);
        return td_font_rows[i];
    }
    return c == '?' ? td_font_rows[0] : glyph('?');
}
/* Eight pixels of column x, from row y down (bits: bit 0 the top one), straight into the page bytes: what eight
 * td_pixel calls did, clipped at the screen's edges alike, a page marked only when one of its bytes changes. */
static void column(uint32_t x, uint32_t y, uint32_t bits) {
    if (x >= TD_WIDTH) return;
    uint32_t page = y >> 3, shift = y & 7u;
    for (uint32_t half = 0; half < (shift ? 2u : 1u); ++half, ++page) {
        if (page >= TD_PAGES) return;
        uint32_t mask = half ? 0xFFu >> (8u - shift) : 0xFFu << shift, part = half ? bits >> (8u - shift) : bits << shift;
        uint8_t v = (uint8_t)((fb[page][x] & ~mask) | (part & mask));
        if (v != fb[page][x]) { fb[page][x] = v; dirty |= (uint8_t)(1u << page); }
    }
}
uint32_t td_text(uint32_t x, uint32_t y, const char *text, uint32_t scale) {
    for (; *text; ++text) {
        const uint8_t *rows = glyph(*text);
        /* Size 1 (review TBR-014): each of the cell's six columns in one or two byte writes, not 48 td_pixel calls a
         * character: a redraw of the step screen took 9 ms of one pass, on every step while a pattern played. The
         * pixels are the same (tests/tb/test_display.c compares every character at every position). */
        if (scale == 1u && y < TD_PAGES * 8u) {
            /* The glyph's 7 rows of 5 pixels turned into its columns: the first column's bits in first, the other four
             * a byte each in rest, the last column lowest (the multiplication copies a row's four low bits to bits 0,
             * 8, 16 and 24: its shifts by 0, 7, 14 and 21 never overlap). The 8th row and the 6th column are the gaps. */
            uint32_t first = 0u, rest = 0u;
            for (uint32_t r = 0; r < 7u; ++r) {
                first |= (((uint32_t)rows[r] >> 4) & 1u) << r;
                rest |= ((((uint32_t)rows[r] & 15u) * 0x00204081u) & 0x01010101u) << r;
            }
            column(x, y, first);
            for (uint32_t c = 1u; c < 5u; ++c) column(x + c, y, (rest >> (8u * (4u - c))) & 0xFFu);
            column(x + 5u, y, 0u);
            x += 6u;
            continue;
        }
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
    if (!ready || !settle() || (!dirty && !everything)) return;
    /* The pages that differ from the screen, first to last, in one transfer (unchanged ones between them too). */
    uint32_t first = TD_PAGES, last = 0;
    for (uint32_t p = 0; p < TD_PAGES; ++p) {
        bool differs = everything;
        for (uint32_t x = 0; x < TD_WIDTH && !differs && (((uint32_t)dirty >> p) & 1u); ++x) differs = fb[p][x] != shown[p][x];
        if (differs) { if (first == TD_PAGES) first = p; last = p; }
    }
    dirty = 0u; everything = false;
    if (first == TD_PAGES) return; /* redrawn the same */
    if (!tb_dma_ok(shown)) { ready = false; return; } /* 2.4.8 (TBR-021): no stream from the core-coupled RAM */
    for (uint32_t p = first; p <= last; ++p) for (uint32_t x = 0; x < TD_WIDTH; ++x) shown[p][x] = fb[p][x];
    const uint8_t window[6] = {0x21u, 0x00u, (uint8_t)(TD_WIDTH - 1u), 0x22u, (uint8_t)first, (uint8_t)last};
    if (!command(window, sizeof window)) { ready = false; return; }
    cs(true); data_mode(true);
    DMA1->HIFCR = DMA_HIFCR_CTCIF5 | DMA_HIFCR_CHTIF5 | DMA_HIFCR_CTEIF5 | DMA_HIFCR_CDMEIF5 | DMA_HIFCR_CFEIF5;
    DMA1_Stream5->PAR = (uint32_t)(uintptr_t)&SPI3->DR;
    DMA1_Stream5->M0AR = (uint32_t)(uintptr_t)&shown[first][0];
    DMA1_Stream5->NDTR = (last - first + 1u) * TD_WIDTH;
    DMA1_Stream5->FCR = 0u;
    DMA1_Stream5->CR = DMA_SxCR_DIR_0 | DMA_SxCR_MINC; /* channel 0, bytes, memory to SPI3 */
    DMA1_Stream5->CR |= DMA_SxCR_EN;
    SPI3->CR2 |= SPI_CR2_TXDMAEN;
    busy = true; /* a stuck SPI never blocks the instrument: settle() and td_wait() give up on it */
}
