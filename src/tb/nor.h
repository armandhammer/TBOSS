/* SPDX-License-Identifier: GPL-3.0-or-later - READ-ONLY access to the external W25Q128 NOR (16 MiB).
 * Shares SPI3 with the display (display.h): select PA15, MISO PB4. The driver can
 * only send three commands, all reads: release power-down (0xAB), JEDEC ID (0x9F)
 * and read data (0x03). It has no write, erase, status-write or reset command, so
 * the chip's contents (the original firmware's data) cannot change.
 * scripts/firmware_safety.py checks that this file uses no other opcode. */
#ifndef TB_NOR_H
#define TB_NOR_H
#include <stdbool.h>
#include <stdint.h>
#define NR_MAX_READ 2048u
/* After td_init (SPI3 and the PA15 select are set up there). Wakes the chip and
 * reads its JEDEC ID; true when it answers with a plausible ID. */
bool nr_init(void);
bool nr_present(void);
void nr_id(uint8_t id[3]);
uint32_t nr_size(void); /* bytes, from the JEDEC capacity code; 0 if absent */
/* Reads len (1..NR_MAX_READ) bytes; false if absent, out of range or SPI stalls. */
bool nr_read(uint32_t address, uint8_t *out, uint32_t len);
#endif
