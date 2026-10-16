/* SPDX-License-Identifier: GPL-3.0-or-later - touch-controller sampler interface. */
#ifndef TB_BUTTON_INPUT_H
#define TB_BUTTON_INPUT_H
#include <stdbool.h>
#include <stdint.h>

enum { BI_CLIENTS = 4, BI_WORDS = 58, BI_BUDGET_US = 1000 };
#define BI_MAGIC UINT32_C(0x42543057)
enum bi_status { BI_OK, BI_NACK, BI_TIMEOUT, BI_BUS, BI_PARTIAL, BI_STALE,
                 BI_CORRUPT, BI_BLOCKED };
/* One bounded combined transaction: START W reg repeated-START R two bytes STOP.
 * Adapter owns wire deadlines, ACK/NACK/STOP and error cleanup. No recovery.
 * It must return within budget_us, even on stuck BUSY. No configuration writes.
 * Raw bytes are trusted only after status/count/generation/time checks pass.
 */
struct bi_reply {
    uint32_t status, count, generation, timestamp_ms, elapsed_us;
    uint8_t data[2];
};
typedef struct bi_reply (*bi_read)(void *context, uint8_t address, uint8_t reg,
    uint32_t generation, uint32_t timestamp_ms, uint32_t budget_us);
struct bi_record {
    uint32_t address, status, valid, raw, generation, timestamp_ms;
    uint32_t edges_valid, rising, falling, errors, attempts, reserved;
};
struct bi_sampler {
    struct bi_record records[BI_CLIENTS];
    uint32_t generation, timestamp_ms, sequence, retries;
    bool stopped;
};
bool bi_init(struct bi_sampler *s, uint32_t retries);
/* Main loop only; monotonically increasing caller-supplied ms time. */
bool bi_sample(struct bi_sampler *s, bi_read read, void *context, uint32_t now_ms);
/* Version 1 wire format, fixed little-endian uint32 words. No mapped address yet.
 * Single producer, uncached SRAM only; readers compare seq before/block/after.
 */
bool bi_publish(const struct bi_sampler *s, volatile uint32_t words[BI_WORDS]);
/* Explicitly blocked hardware adapter: no registers, pins, startup or transport. */
struct bi_reply bi_hardware_blocked(void *context, uint8_t address, uint8_t reg,
    uint32_t generation, uint32_t timestamp_ms, uint32_t budget_us);
#endif
