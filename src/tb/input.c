/* SPDX-License-Identifier: GPL-3.0-or-later - touch-controller sampling: one bounded read per client per poll. */
#include "input.h"
#include <stdatomic.h>
#include <stddef.h>

static const uint8_t addresses[BI_CLIENTS] = {0x35, 0x36, 0x38, 0x39};
static uint32_t increment(uint32_t value) {
    return value == UINT32_MAX ? value : value + 1U;
}
bool bi_init(struct bi_sampler *s, uint32_t retries) {
    if (s == NULL) return false;
    *s = (struct bi_sampler){0};
    s->stopped = retries > 1U;
    if (s->stopped) return false;
    s->retries = retries;
    for (uint32_t i = 0; i < BI_CLIENTS; ++i) {
        s->records[i].address = addresses[i];
        s->records[i].status = BI_BLOCKED;
    }
    return true;
}
bool bi_sample(struct bi_sampler *s, bi_read read, void *context, uint32_t now_ms) {
    if (s == NULL || read == NULL) return false;
    if (s->stopped || s->retries > 1U || s->generation == UINT32_MAX ||
        s->sequence >= UINT32_MAX - 2U ||
        (s->generation != 0U && now_ms <= s->timestamp_ms)) {
        s->stopped = true;
        return false;
    }
    ++s->generation;
    s->timestamp_ms = now_ms;
    for (uint32_t i = 0; i < BI_CLIENTS; ++i) {
        const struct bi_record previous = s->records[i];
        struct bi_record r = {0};
        r.address = addresses[i];
        r.generation = s->generation;
        r.timestamp_ms = now_ms;
        r.errors = previous.errors;
        for (uint32_t attempt = 0; attempt <= s->retries; ++attempt) {
            struct bi_reply reply = read(context, addresses[i], 0xAA,
                s->generation, now_ms, BI_BUDGET_US);
            ++r.attempts;
            r.status = reply.status <= BI_BLOCKED ? reply.status : BI_CORRUPT;
            if (r.status == BI_OK) {
                if (reply.count != 2U) r.status = BI_PARTIAL;
                else if (reply.generation != s->generation || reply.timestamp_ms != now_ms)
                    r.status = BI_STALE;
                else if (reply.elapsed_us > BI_BUDGET_US) r.status = BI_TIMEOUT;
            }
            if (r.status == BI_OK) {
                r.valid = 1;
                r.raw = (uint32_t)reply.data[0] | ((uint32_t)reply.data[1] << 8);
                /* A retry or invalid preceding sample breaks edge continuity. */
                r.edges_valid = previous.valid && r.attempts == 1U ? 1U : 0U;
                if (r.edges_valid != 0U) {
                    r.rising = r.raw & ~previous.raw;
                    r.falling = previous.raw & ~r.raw;
                }
                break;
            }
            r.errors = increment(r.errors);
            /* Unsupported/blocked access is never retried. */
            if (r.status == BI_BLOCKED) break;
        }
        s->records[i] = r;
    }
    s->sequence += 2U;
    return true;
}
bool bi_publish(const struct bi_sampler *s, volatile uint32_t words[BI_WORDS]) {
    if (s == NULL || words == NULL || s->stopped || s->generation == 0U) return false;
    words[3] = s->sequence - 1U;
    atomic_thread_fence(memory_order_release);
    words[0] = BI_MAGIC; words[1] = 1; words[2] = BI_WORDS * 4U;
    words[4] = s->generation; words[5] = s->timestamp_ms;
    words[6] = 0; words[7] = BI_CLIENTS;
    for (uint32_t i = 0; i < BI_CLIENTS; ++i) {
        const struct bi_record *r = &s->records[i];
        const uint32_t o = 8U + i * 12U;
        words[o] = r->address; words[o+1U] = r->status;
        words[o+2U] = r->valid; words[o+3U] = r->raw;
        words[o+4U] = r->generation; words[o+5U] = r->timestamp_ms;
        words[o+6U] = r->edges_valid; words[o+7U] = r->rising;
        words[o+8U] = r->falling; words[o+9U] = r->errors;
        words[o+10U] = r->attempts; words[o+11U] = 0;
        if (r->valid == 0U) words[6] |= UINT32_C(1) << i;
    }
    words[56] = s->sequence;
    /* Word-wise FNV-1a detects accidental snapshot corruption, not authenticity.
     * Word 3 is still odd while publishing; checksum uses its final even value. */
    uint32_t checksum = UINT32_C(2166136261);
    for (uint32_t i = 0; i < BI_WORDS - 1U; ++i)
        checksum = (checksum ^ (i == 3U ? s->sequence : words[i])) * UINT32_C(16777619);
    words[57] = checksum;
    atomic_thread_fence(memory_order_release);
    words[3] = s->sequence;
    return true;
}
