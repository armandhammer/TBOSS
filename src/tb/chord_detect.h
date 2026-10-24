/* SPDX-License-Identifier: GPL-3.0-or-later - chords and keys from what is played (1.4.0, chord
 * capture): the chord a set of held notes is, and the key a list of chords fits best. Pure
 * functions; the chord symbols are chord_name.c's, so what is captured is what the screen names. */
#ifndef TB_CHORD_DETECT_H
#define TB_CHORD_DETECT_H
#include <stdbool.h>
#include <stdint.h>
/* Held pitch classes (bit n = pitch class n) and the lowest note's class: the chord's root and
 * its tones as semitones above the root (bit n), at most 6 (the lowest kept). The root is the
 * bass when the notes are an exact chord symbol on it, else the first other note on which they
 * are one, else the bass. False under two pitch classes. */
bool cd_chord(uint16_t classes, uint8_t bass, uint8_t *root, uint16_t *tones);
/* The key n chords (roots as pitch classes, tones above them) fit best, as a pivot target
 * (layout.h: 1..12 the major key on C..B, 13..24 the minor key): their notes inside its scale,
 * the ones outside it counting against, and a first or last chord on its tonic (of its kind)
 * counting for it. Ties go to the major key, then the lower root. 0 for no chords. */
uint8_t cd_key(const uint8_t *roots, const uint16_t *tones, uint32_t n);
#endif
