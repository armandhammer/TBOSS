/* SPDX-License-Identifier: GPL-3.0-or-later - CRC-32 a nibble at a time (crc.h): a 64-byte table, the same result as
 * the bit-by-bit loop it replaces (tests/tb/test_settings.c checks both over random data), two table steps a byte
 * instead of eight shifts: a settings record's CRC was 0.23 ms (emulated), computed three times per record at every
 * start before 2.4.6. */
#include "crc.h"
static const uint32_t nibble[16] = {
    0x00000000u, 0x1DB71064u, 0x3B6E20C8u, 0x26D930ACu, 0x76DC4190u, 0x6B6B51F4u, 0x4DB26158u, 0x5005713Cu,
    0xEDB88320u, 0xF00F9344u, 0xD6D6A3E8u, 0xCB61B38Cu, 0x9B64C2B0u, 0x86D3D2D4u, 0xA00AE278u, 0xBDBDF21Cu,
};
static uint32_t byte_in(uint32_t crc, uint32_t byte) {
    crc ^= byte;
    crc = (crc >> 4) ^ nibble[crc & 15u];
    return (crc >> 4) ^ nibble[crc & 15u];
}
uint32_t tb_crc32_bytes(const uint8_t *bytes, uint32_t count) {
    uint32_t crc = 0xFFFFFFFFu;
    for (uint32_t i = 0; i < count; ++i) crc = byte_in(crc, bytes[i]);
    return ~crc;
}
uint32_t tb_crc32_words(const uint32_t *words, uint32_t count) {
    uint32_t crc = 0xFFFFFFFFu;
    for (uint32_t i = 0; i < count; ++i)
        for (uint32_t b = 0; b < 4u; ++b) crc = byte_in(crc, (words[i] >> (8u * b)) & 0xFFu);
    return ~crc;
}
