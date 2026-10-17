/* SPDX-License-Identifier: GPL-3.0-or-later - MIDI on the TRS jack: USB-MIDI packets as
 * MIDI bytes, queued for the serial port (jack_hw.c sends them at 31,250 baud). One
 * producer (the main loop) and one consumer (the serial interrupt); whole messages only. */
#ifndef TB_SERIAL_MIDI_H
#define TB_SERIAL_MIDI_H
#include <stdbool.h>
#include <stdint.h>
#define SM_SIZE 256u /* power of two; about 80 ms of MIDI at 31,250 baud */
typedef struct {
    uint8_t bytes[SM_SIZE];
    volatile uint32_t head, tail; /* head: next write (main loop), tail: next read (interrupt) */
    uint32_t sent;                /* bytes queued since start (diagnostics, I/O test page) */
} sm_ring;
extern sm_ring sm_out;
/* The MIDI bytes a USB-MIDI packet carries (by its code index number); 0 for none. */
uint32_t sm_packet_bytes(const uint8_t packet[4], uint8_t out[3]);
uint32_t sm_space(const sm_ring *r);
/* Queues all n bytes, or none when they don't fit. */
bool sm_push(sm_ring *r, const uint8_t *bytes, uint32_t n);
bool sm_pop(sm_ring *r, uint8_t *byte);
#endif
