/* SPDX-License-Identifier: GPL-3.0-or-later - 2.3.1, the screen by DMA (display.c) on stand-in registers
 * (tests/tb/display_hal): the whole screen first; while a frame goes nothing else starts; then only the pages that
 * changed, first to last, in one transfer (HOLD across pages 5 and 6 at once), nothing for the same picture drawn again;
 * the display deselected and SPI3 back to plain writes after each; the NOR on the same SPI3 waits for a frame (and
 * stops a stuck one; the screen then stays as it is); a transfer error stops the screen. */
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
    puts("display: all passed");
    return 0;
}
