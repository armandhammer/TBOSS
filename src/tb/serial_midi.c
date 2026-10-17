/* SPDX-License-Identifier: GPL-3.0-or-later - MIDI bytes for the TRS jack (see serial_midi.h). */
#include "serial_midi.h"
sm_ring sm_out;
uint32_t sm_packet_bytes(const uint8_t packet[4], uint8_t out[3]) {
    /* Code index number -> message length (USB MIDI 1.0, table 4-1); 0/1 are reserved. */
    static const uint8_t length[16] = {0, 0, 2, 3, 3, 1, 2, 3, 3, 3, 3, 3, 2, 2, 3, 1};
    uint32_t n = length[packet[0] & 15u];
    for (uint32_t i = 0; i < n; ++i) out[i] = packet[1u + i];
    return n;
}
uint32_t sm_space(const sm_ring *r) {
    return SM_SIZE - 1u - ((r->head - r->tail) & (SM_SIZE - 1u));
}
bool sm_push(sm_ring *r, const uint8_t *bytes, uint32_t n) {
    if (n > sm_space(r)) return false;
    uint32_t head = r->head;
    for (uint32_t i = 0; i < n; ++i) r->bytes[(head + i) & (SM_SIZE - 1u)] = bytes[i];
    __atomic_thread_fence(__ATOMIC_RELEASE); /* bytes before the new head */
    r->head = (head + n) & (SM_SIZE - 1u);
    r->sent += n;
    return true;
}
bool sm_pop(sm_ring *r, uint8_t *byte) {
    uint32_t tail = r->tail;
    if (tail == r->head) return false;
    __atomic_thread_fence(__ATOMIC_ACQUIRE);
    *byte = r->bytes[tail];
    r->tail = (tail + 1u) & (SM_SIZE - 1u);
    return true;
}
