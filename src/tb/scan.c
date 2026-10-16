/* SPDX-License-Identifier: GPL-3.0-or-later - original bounded scan and capture logic. */
#include "scan.h"
#include <stdatomic.h>
const ts_pin ts_selectors[6][4] = {
    {{2,1},{2,2},{0,1},{0,2}}, {{0,7},{2,5},{2,4},{1,1}},
    {{4,7},{4,8},{4,9},{4,10}}, {{4,11},{4,12},{4,13},{4,14}},
    {{4,15},{1,10},{1,11},{1,12}}, {{1,13},{3,8},{3,9},{3,10}}
};
const ts_pin ts_inputs[6] = {{2,0},{0,3},{0,4},{0,5},{0,6},{1,0}};
const uint8_t ts_channels[6] = {10,3,4,5,6,8};
void ts_masks(uint8_t p, uint32_t reset[5], uint32_t set[5]) {
    for (uint32_t port = 0; port < 5u; ++port) reset[port] = set[port] = 0u;
    for (uint32_t bank = 0; bank < 6u; ++bank) {
        for (uint32_t bit = 0; bit < 4u; ++bit) {
            ts_pin pin = ts_selectors[bank][bit];
            if (((uint32_t)p & (1u << bit)) != 0u) set[pin.port] |= 1u << pin.bit;
            else reset[pin.port] |= 1u << pin.bit;
        }
    }
}
static bool reject(ts_scan *s) {
    s->valid = false;
    s->completed_positions = 0u;
    s->position = 0u;
    ts_status.invalid++;
    return false;
}
bool ts_step(ts_scan *s, ts_ops ops, uint32_t settle_us, uint32_t timeout_us, uint32_t now_ms) {
    if (settle_us < 10u || settle_us > 1000u || timeout_us < 100u || timeout_us > 2000u)
        return reject(s);
    uint8_t p = s->position;
    if (p == 0u) { s->start_us = ops.micros(); s->valid = false; }
    ts_status.selector = p;
    ops.select(p);
    if (!ops.settle(settle_us)) { ts_status.timeouts++; return reject(s); }
    uint32_t sums[6] = {0};
    uint32_t selected = ops.micros(), previous = selected;
    for (uint32_t round = 0; round < 3u; ++round) {
        for (uint8_t bank = 0; bank < 6u; ++bank) {
            uint16_t raw;
            if (!ops.sample(bank, timeout_us, &raw)) { ts_status.timeouts++; return reject(s); }
            uint32_t stamp = ops.micros();
            if (raw > 4095u || stamp - previous == 0u || stamp - previous > timeout_us + 1000u ||
                stamp - selected > 60000u) return reject(s);
            previous = stamp;
            sums[bank] += raw;
        }
    }
    for (uint32_t bank = 0; bank < 6u; ++bank) s->values[16u * bank + p] = sums[bank] / 3u;
    s->position_us[p] = previous;
    s->completed_positions++;
    s->position = (uint8_t)((p + 1u) & 15u);
    if (s->position != 0u) return false;
    s->end_us = ops.micros();
    s->duration_us = s->end_us - s->start_us;
    if (s->completed_positions != 16u || s->duration_us > 100000u || s->duration_us == 0u)
        return reject(s);
    s->completed_positions = 0u;
    if (s->frame == UINT32_MAX) return reject(s); /* no generation wrap */
    s->frame++;
    s->last_ms = now_ms;
    s->valid = true;
    ts_status.frames = s->frame;
    ts_status.duration_us = s->duration_us;
    ts_status.last_frame_ms = now_ms;
    return true;
}
void ts_capture(const ts_scan *s) {
    uint32_t request = ts_status.capture_request;
    if (!s->valid || !request || request == ts_status.capture_ack) return;
    volatile ts_snapshot *out = &ts_status.capture;
    out->sequence++; /* odd: publication in progress */
    atomic_thread_fence(memory_order_seq_cst);
    out->request = request; out->frame = s->frame;
    out->start_us = s->start_us; out->end_us = s->end_us;
    out->duration_us = s->duration_us; out->count = TS_KEYS; out->valid = 1u;
    for (uint32_t k = 0; k < TS_KEYS; ++k) out->values[k] = s->values[k];
    for (uint32_t p = 0; p < TS_POSITIONS; ++p) out->position_us[p] = s->position_us[p];
    atomic_thread_fence(memory_order_seq_cst);
    out->sequence++; /* even: frozen until a different explicit request */
    atomic_thread_fence(memory_order_seq_cst);
    ts_status.capture_ack = request;
}
