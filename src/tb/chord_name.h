/* SPDX-License-Identifier: GPL-3.0-or-later - chord symbols for the display (e.g. Dm7, G7sus4, Am/C). */
#ifndef TB_CHORD_NAME_H
#define TB_CHORD_NAME_H
#include "layout.h"
#define TN_MAX 24u
#define TN_SCREEN 21u /* characters across the display at 6 px */
/* Names the notes. root is the pitch class the pad builds on (its scale step); it
 * is tried first, then the other chord tones (bass first) as a slash chord. With
 * no exact symbol the largest known chord inside the notes is named and the rest
 * listed in brackets, e.g. C(9). Returns the length; out is NUL terminated. */
uint32_t tn_name(const tp_notes *notes, uint8_t root, char out[TN_MAX]);
/* The root a set of pitch classes (bit n = pitch class n) is named on with no pad root: the bass
 * if they are an exact symbol on it, else the first other note upward on which they are one,
 * else the bass (1.4.0, chord_detect.c). */
uint8_t tn_root(uint16_t classes, uint8_t bass);
/* The chord on pad k (0..47) with its live edits: the name's length, 0 for a melody pad. */
uint32_t tn_pad(tp_layout layout, uint32_t k, char out[TN_MAX]);
#endif
