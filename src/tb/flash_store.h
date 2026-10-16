/* SPDX-License-Identifier: GPL-3.0-or-later - the only internal-flash writer: sector 11 alone. */
#ifndef TB_FLASH_STORE_H
#define TB_FLASH_STORE_H
#include <stdbool.h>
#include <stdint.h>
#define FS_BASE 0x080E0000u /* sector 11, 128 KiB; the firmware image ends below it */
#define FS_SIZE 0x20000u
const uint8_t *fs_base(void);
bool fs_erase(void);  /* stalls flash fetch ~1-2 s: settings.c only calls it before USB */
bool fs_program(uint32_t offset, const uint32_t *words, uint32_t count);
#endif
