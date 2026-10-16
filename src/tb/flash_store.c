/* SPDX-License-Identifier: GPL-3.0-or-later - sector 11 erase/program (RM0090 3.6). The only
 * module allowed to touch FLASH KEYR/CR/SR (scripts/firmware_safety.py). Every
 * address is checked against sector 11; the sector number is a constant; option
 * bytes (OPTCR/OPTKEYR) are never touched. */
#include "flash_store.h"
#include "stm32f4xx.h"
#define FS_SECTOR 11u
#define FS_KEY1 0x45670123u
#define FS_KEY2 0xCDEF89ABu
#define FS_ERRORS (FLASH_SR_OPERR | FLASH_SR_WRPERR | FLASH_SR_PGAERR | FLASH_SR_PGPERR | FLASH_SR_PGSERR)
_Static_assert(FS_BASE == 0x080E0000u && FS_SIZE == 0x20000u, "sector 11 of a 1 MiB STM32F407");
const uint8_t *fs_base(void) { return (const uint8_t *)FS_BASE; }
static bool idle(void) {
    for (uint32_t n = 0; n < 50000000u; ++n) if (!(FLASH->SR & FLASH_SR_BSY)) return true;
    return false;
}
static bool unlock(void) {
    if (!idle()) return false;
    if (FLASH->CR & FLASH_CR_LOCK) { FLASH->KEYR = FS_KEY1; FLASH->KEYR = FS_KEY2; }
    FLASH->SR = FLASH_SR_EOP | FS_ERRORS;
    return !(FLASH->CR & FLASH_CR_LOCK);
}
static bool done(void) {
    bool ok = idle() && !(FLASH->SR & FS_ERRORS);
    FLASH->CR = FLASH_CR_LOCK; /* clears PG/SER/SNB, relocks */
    return ok;
}
bool fs_erase(void) {
    if (!unlock()) return false;
    FLASH->CR = FLASH_CR_PSIZE_1 | FLASH_CR_SER | (FS_SECTOR << FLASH_CR_SNB_Pos);
    FLASH->CR |= FLASH_CR_STRT;
    return done();
}
bool fs_program(uint32_t offset, const uint32_t *words, uint32_t count) {
    if ((offset & 3u) || offset >= FS_SIZE || count > (FS_SIZE - offset) / 4u) return false;
    if (!unlock()) return false;
    bool ok = true;
    for (uint32_t i = 0; i < count && ok; ++i) {
        uint32_t address = FS_BASE + offset + 4u * i;
        if (address < FS_BASE || address >= FS_BASE + FS_SIZE) { ok = false; break; }
        FLASH->CR = FLASH_CR_PSIZE_1 | FLASH_CR_PG;
        *(volatile uint32_t *)address = words[i];
        ok = idle() && !(FLASH->SR & FS_ERRORS);
    }
    return done() && ok;
}
