/* SPDX-License-Identifier: GPL-3.0-or-later - the two jacks: TRS MIDI out and the sustain pedal.
 * MIDI out is USART1 TX on PB6 (AF7), 31,250 baud 8N1, transmit only (from the board's previous
 * firmware; confirmed on a board). The pedal is the jack's tip on PA10 (input, pull-up): found
 * with the I/O test on a board (1.5.0), where PA10 followed the tip but floated without a pull-up.
 * PD13 (the earlier guess, pull-up) and PD5 are read only, as alternatives for the I/O test. */
#ifndef TB_JACK_H
#define TB_JACK_H
#include <stdbool.h>
#include <stdint.h>
#include "serial_midi.h"
void jh_init(void);
/* Starts sending sm_out if it is idle (call after sm_push). */
void jh_kick(void);
/* Pin levels: bit 0 PA10 (pedal), bit 1 PD13, bit 2 PD5. */
uint32_t jh_levels(void);
#endif
