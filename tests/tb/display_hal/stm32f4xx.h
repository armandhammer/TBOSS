/* SPDX-License-Identifier: GPL-3.0-or-later - stand-ins for the registers display.c and nor.c use (test_display.c):
 * plain memory, so the test sets the flags and reads what the drivers wrote. */
#ifndef TEST_DISPLAY_HAL_H
#define TEST_DISPLAY_HAL_H
#include <stdint.h>
typedef struct { volatile uint32_t AHB1ENR, APB1ENR; } test_rcc_t;
typedef struct { volatile uint32_t MODER, OSPEEDR, PUPDR, BSRR, AFR[2]; } test_gpio_t;
typedef struct { volatile uint32_t CR1, CR2, SR, DR; } test_spi_t;
typedef struct { volatile uint32_t HISR, HIFCR; } test_dma_t;
typedef struct { volatile uint32_t CR, NDTR, PAR, M0AR, FCR; } test_stream_t;
extern test_rcc_t test_rcc;
extern test_gpio_t test_gpioa, test_gpiob, test_gpiod;
extern test_spi_t test_spi3;
extern test_dma_t test_dma1;
extern test_stream_t test_stream5;
#define RCC (&test_rcc)
#define GPIOA (&test_gpioa)
#define GPIOB (&test_gpiob)
#define GPIOD (&test_gpiod)
#define SPI3 (&test_spi3)
#define DMA1 (&test_dma1)
#define DMA1_Stream5 (&test_stream5)
#define RCC_AHB1ENR_GPIOAEN (1u << 0)
#define RCC_AHB1ENR_GPIOBEN (1u << 1)
#define RCC_AHB1ENR_GPIODEN (1u << 3)
#define RCC_AHB1ENR_DMA1EN (1u << 21)
#define RCC_APB1ENR_SPI3EN (1u << 15)
#define SPI_CR1_MSTR (1u << 2)
#define SPI_CR1_BR_1 (1u << 4)
#define SPI_CR1_SPE (1u << 6)
#define SPI_CR1_SSI (1u << 8)
#define SPI_CR1_SSM (1u << 9)
#define SPI_CR2_TXDMAEN (1u << 1)
#define SPI_SR_RXNE (1u << 0)
#define SPI_SR_TXE (1u << 1)
#define SPI_SR_BSY (1u << 7)
#define DMA_SxCR_EN (1u << 0)
#define DMA_SxCR_DIR_0 (1u << 6)
#define DMA_SxCR_MINC (1u << 10)
#define DMA_HISR_TEIF5 (1u << 9)
#define DMA_HIFCR_CFEIF5 (1u << 6)
#define DMA_HIFCR_CDMEIF5 (1u << 8)
#define DMA_HIFCR_CTEIF5 (1u << 9)
#define DMA_HIFCR_CHTIF5 (1u << 10)
#define DMA_HIFCR_CTCIF5 (1u << 11)
#endif
