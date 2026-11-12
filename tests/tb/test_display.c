/* SPDX-License-Identifier: GPL-3.0-or-later - 2.3.1, the screen by DMA (display.c) on stand-in registers
 * (tests/tb/display_hal): the whole screen first; while a frame goes nothing else starts; then only the pages that
 * changed, first to last, in one transfer (HOLD across pages 5 and 6 at once), nothing for the same picture drawn again;
 * the display deselected and SPI3 back to plain writes after each; the NOR on the same SPI3 waits for a frame (and
 * stops a stuck one; the screen then stays as it is); a transfer error stops the screen.
 * 2.4.3 (review TBR-014): size-1 text is written a glyph column at a time; its pixels, its clipping at the screen's
 * edges and the pages it marks are those of the pixel-by-pixel drawing it replaces, for every character at every
 * position. */
#include "../../src/tb/display.c"
#include "../../src/tb/nor.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
test_rcc_t test_rcc;
test_gpio_t test_gpioa, test_gpiob, test_gpiod;
test_spi_t test_spi3;
test_dma_t test_dma1;
test_stream_t test_stream5;
uint32_t tb_millis(void) { return 0u; }
#define DATA (1u << 11) /* the last pin write before a frame: select (PD14 low), then data mode (PD11 high) */
#define DESELECTED (1u << 14)
static void frame_ends(void) { test_stream5.CR &= ~DMA_SxCR_EN; }
static bool sending(uint32_t first, uint32_t last) {
    return busy && (test_stream5.CR & DMA_SxCR_EN) && (test_spi3.CR2 & SPI_CR2_TXDMAEN) && test_gpiod.BSRR == DATA &&
           test_stream5.NDTR == (last - first + 1u) * TD_WIDTH && test_stream5.M0AR == (uint32_t)(uintptr_t)&shown[first][0] &&
           test_stream5.PAR == (uint32_t)(uintptr_t)&test_spi3.DR && !memcmp(shown, fb, sizeof fb);
}
static bool idle(void) { return !busy && !(test_spi3.CR2 & SPI_CR2_TXDMAEN) && test_gpiod.BSRR == DESELECTED; }
static bool blank(uint32_t page) { for (uint32_t x = 0; x < TD_WIDTH; ++x) if (shown[page][x]) return false; return true; }
/* Text as it was drawn before 2.4.3: td_pixel for each of a cell's 6 x 8 pixels (the reference). */
static uint32_t pixel_text(uint32_t x, uint32_t y, const char *text, uint32_t scale) {
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
static void background(uint32_t kind) {
    for (uint32_t p = 0; p < TD_PAGES; ++p) for (uint32_t x = 0; x < TD_WIDTH; ++x)
        fb[p][x] = (uint8_t)(kind == 0u ? 0u : kind == 1u ? 0xFFu : x * 37u + p * 101u + (x >> 3));
    dirty = 0u;
}
static uint32_t text_pixels(void) {
    static const uint32_t xs[] = {0u, 1u, 61u, 116u, 117u, 121u, 122u, 123u, 127u, 128u, 300u, 0xFFFFFFFDu};
    static uint8_t want[TD_PAGES][TD_WIDTH];
    uint32_t glyphs = (uint32_t)strlen(td_font_chars), compared = 0u;
    for (uint32_t i = 0; i <= glyphs; ++i) { /* each character with its neighbor, and one the font lacks (drawn as ?) */
        const char text[3] = {i < glyphs ? td_font_chars[i] : '~', td_font_chars[(i + 1u) % glyphs], 0};
        for (uint32_t kind = 0; kind < 3u; ++kind) for (uint32_t k = 0; k < sizeof xs / sizeof xs[0]; ++k)
            for (uint32_t row = 0; row < 72u; ++row) { /* every height, past the bottom edge, and three that wrap */
                uint32_t y = row < 69u ? row : 0xFFFFFFFFu - (row - 69u) * 3u, x = xs[k];
                for (uint32_t scale = 1u; scale <= (row % 9u == 0u && kind == 2u ? 2u : 1u); ++scale) {
                    background(kind);
                    uint32_t end = pixel_text(x, y, text, scale);
                    uint8_t marked = dirty;
                    memcpy(want, fb, sizeof fb);
                    background(kind);
                    assert(td_text(x, y, text, scale) == end && !memcmp(want, fb, sizeof fb) && dirty == marked);
                    compared++;
                }
            }
    }
    /* drawn over itself: nothing changes, so no page is marked and nothing is sent */
    background(0u); td_text(7u, 13u, "STEP 12: Cm7", 1u); assert(dirty == 0x06u);
    dirty = 0u; td_text(7u, 13u, "STEP 12: Cm7", 1u); assert(!dirty);
    return compared;
}
int main(void) {
    test_spi3.SR = SPI_SR_TXE | SPI_SR_RXNE; /* the bus never stalls here */
    assert(td_init() && ready && (test_rcc.AHB1ENR & RCC_AHB1ENR_DMA1EN));
    td_service(); assert(sending(0u, 7u)); /* start-up: the whole screen (its RAM is unknown) */
    td_text(0u, 46u, "HOLD", 1u); /* HOLD: pages 5 and 6 */
    td_service(); assert(test_stream5.NDTR == 8u * TD_WIDTH && blank(5u)); /* the frame still going: nothing new starts */
    frame_ends(); td_service(); assert(sending(5u, 6u) && !blank(5u) && !blank(6u)); /* both halves in one transfer */
    frame_ends(); td_service(); assert(idle());
    td_clear(); td_text(0u, 46u, "HOLD", 1u); td_service(); assert(idle()); /* the same picture again: nothing sent */
    td_clear(); td_service(); assert(sending(5u, 6u) && blank(5u) && blank(6u)); /* HOLD off */
    frame_ends(); td_service(); assert(idle());
    td_pixel(0u, 0u, true); td_pixel(127u, 63u, true); td_service(); assert(sending(0u, 7u)); /* first to last page */
    frame_ends(); td_service(); assert(idle());
    td_text(30u, 8u, "MODES", 1u); td_service(); assert(sending(1u, 1u));
    puts("frames: everything at start, then the changed pages first to last in one DMA transfer, none for the same picture");
    /* The NOR waits for a frame that ends: the display deselected first, SPI3 back to plain writes. (The stand-in
     * DR echoes what was written, so no chip answers here; nr_init still takes the bus before it reads the ID.) */
    frame_ends();
    assert(!nr_init() && !nr_present() && idle() && ready);
    td_pixel(9u, 20u, true); td_service(); assert(sending(2u, 2u));
    frame_ends(); td_wait(); assert(idle() && ready);
    /* A frame that never ends: the NOR stops it and goes ahead; the screen stays as it is. */
    td_pixel(9u, 40u, true); td_service(); assert(sending(5u, 5u));
    (void)nr_init(); assert(!busy && !(test_stream5.CR & DMA_SxCR_EN) && idle() && !ready);
    td_pixel(1u, 1u, true); td_service(); assert(idle());
    /* A transfer error stops the screen too. */
    assert(td_init()); td_service(); assert(sending(0u, 7u));
    test_dma1.HISR = DMA_HISR_TEIF5; frame_ends(); td_service(); assert(idle() && !ready);
    puts("shared SPI3: the NOR takes the bus after a frame ends, stops a stuck one; a transfer error stops the screen");
    printf("text: size 1 by glyph columns gives the pixels, the clipping and the marked pages of pixel-by-pixel drawing "
           "(%u drawings compared: every character, every height, three backgrounds, both edges)\n", (unsigned)text_pixels());
    puts("display: all passed");
    return 0;
}
