/* SPDX-License-Identifier: GPL-3.0-or-later - CRC-32 (IEEE 802.3, reflected, polynomial 0xEDB88320, as zlib),
 * shared by the settings log and the editor's protocol (2.4.6, review TBR-027: was two bit-by-bit copies). */
#ifndef TB_CRC_H
#define TB_CRC_H
#include <stdint.h>
uint32_t tb_crc32_bytes(const uint8_t *bytes, uint32_t count);
/* Each word's bytes, least significant first (the settings records' CRC). */
uint32_t tb_crc32_words(const uint32_t *words, uint32_t count);
#endif
