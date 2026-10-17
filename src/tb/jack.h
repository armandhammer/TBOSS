/* SPDX-License-Identifier: GPL-3.0-or-later - the two jacks: TRS MIDI out and the sustain pedal.
 * Pins from the board's previous firmware (not yet confirmed on a board): MIDI out is
 * USART1 TX on PB6 (AF7), 31,250 baud 8N1, transmit only; the pedal is PD13 (input,
 * pull-up). PA10 and PD5 are read only, as alternatives for the I/O test page. */
#ifndef TB_JACK_H
#define TB_JACK_H
#include <stdbool.h>
#include <stdint.h>
#include "serial_midi.h"
void jh_init(void);
/* Starts sending sm_out if it is idle (call after sm_push). */
void jh_kick(void);
/* Pin levels: bit 0 PD13 (pedal), bit 1 PA10, bit 2 PD5. */
uint32_t jh_levels(void);
#endif
