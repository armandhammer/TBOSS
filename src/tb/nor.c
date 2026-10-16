/* SPDX-License-Identifier: GPL-3.0-or-later - read-only W25Q128 driver (Winbond W25Q128JV datasheet:
 * standard SPI, mode 0, instructions 0xAB, 0x9F and 0x03 only). */
#include "nor.h"
#include "stm32f4xx.h"
enum { NR_WAKE = 0xAB, NR_JEDEC = 0x9F, NR_READ = 0x03, NR_DUMMY = 0xFF };
static uint8_t id[3];
static bool present;
static bool wait(volatile uint32_t *reg, uint32_t mask, uint32_t value) {
    for (uint32_t n = 0; n < 20000u; ++n) if ((*reg & mask) == value) return true;
    return false;
}
static void chip(bool on) { GPIOA->BSRR = on ? (1u << 31) : (1u << 15); } /* PA15 low = selected */
static bool exchange(uint8_t out, uint8_t *in) {
    if (!wait(&SPI3->SR, SPI_SR_TXE, SPI_SR_TXE)) return false;
    SPI3->DR = out;
    if (!wait(&SPI3->SR, SPI_SR_RXNE, SPI_SR_RXNE)) return false;
    uint8_t v = (uint8_t)SPI3->DR;
    if (in) *in = v;
    return true;
}
/* The display only transmits, so stale receive data and an overrun flag are
 * normal on entry: read DR then SR to clear them. Bus idle before selecting. */
static bool begin(void) {
    if (!wait(&SPI3->SR, SPI_SR_BSY, 0u)) return false;
    (void)SPI3->DR; (void)SPI3->SR;
    chip(true);
    return true;
}
static bool end(bool ok) {
    ok = wait(&SPI3->SR, SPI_SR_BSY, 0u) && ok;
    chip(false);
    return ok;
}
static bool command(uint8_t op, uint8_t *in, uint32_t count) {
    if (!begin()) return false;
    bool ok = exchange(op, 0);
    for (uint32_t i = 0; i < count && ok; ++i) ok = exchange(NR_DUMMY, &in[i]);
    return end(ok);
}
bool nr_init(void) {
    RCC->AHB1ENR |= RCC_AHB1ENR_GPIOBEN;
    (void)RCC->AHB1ENR;
    /* MISO PB4: SPI3 AF6 input path (MOSI/SCK are the display's, already set up). */
    GPIOB->AFR[0] = (GPIOB->AFR[0] & ~(15u << 16)) | (6u << 16);
    GPIOB->MODER = (GPIOB->MODER & ~(3u << 8)) | (2u << 8);
    present = false;
    if (!(SPI3->CR1 & SPI_CR1_SPE)) return false; /* display init did not run */
    if (!command(NR_WAKE, 0, 0)) return false;
    for (volatile uint32_t n = 0; n < 500u; ++n) {} /* tRES1 3 us */
    if (!command(NR_JEDEC, id, 3)) return false;
    /* Any real JEDEC ID: manufacturer not 0x00/0xFF, capacity code 2^16..2^25 bytes. */
    present = id[0] && id[0] != NR_DUMMY && id[2] >= 16u && id[2] <= 25u;
    return present;
}
bool nr_present(void) { return present; }
void nr_id(uint8_t out[3]) { out[0] = id[0]; out[1] = id[1]; out[2] = id[2]; }
uint32_t nr_size(void) { return present ? 1u << id[2] : 0u; }
bool nr_read(uint32_t address, uint8_t *out, uint32_t len) {
    if (!present || !len || len > NR_MAX_READ || address >= nr_size() || len > nr_size() - address) return false;
    if (!begin()) return false;
    bool ok = exchange(NR_READ, 0) && exchange((uint8_t)(address >> 16), 0) &&
              exchange((uint8_t)(address >> 8), 0) && exchange((uint8_t)address, 0);
    for (uint32_t i = 0; i < len && ok; ++i) ok = exchange(NR_DUMMY, &out[i]);
    return end(ok);
}
