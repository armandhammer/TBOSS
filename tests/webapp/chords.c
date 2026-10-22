/* SPDX-License-Identifier: GPL-3.0-or-later - the chord pads' notes (src/tb/layout.c) behind
 * stdin/stdout for tests/webapp/test_protocol.mjs, which checks that every chord the web editor
 * places plays the notes its name says. Each input line "tonic scale pad lower sus add" prints
 * the pad's notes as the firmware plays them (neutral panel: the bottom row is root position,
 * a lowered member an octave down), lowest first, in hex ("-" if the pad can't play);
 * "U slot mask" defines user scale U(slot + 1) first. */
#include "../../src/tb/layout.h"
#include <stdio.h>
#include "../../src/tb/scan.h"
volatile ts_abi ts_status;
int main(void) {
    static char line[128];
    while (fgets(line, sizeof line, stdin)) {
        unsigned a, b, c, d, e2, f;
        if (line[0] == 'U' && sscanf(line + 1, "%u %x", &a, &b) == 2) {
            puts(tc_set_user_scale((uint8_t)a, (uint16_t)b, "TEST") ? "ok" : "bad");
            continue;
        }
        if (sscanf(line, "%u %u %u %u %u %u", &a, &b, &c, &d, &e2, &f) != 6) { puts("?"); continue; }
        tp_layout l = {(uint8_t)a, TP_MELODY_BASE, TP_CHORD_BASE, (uint8_t)b, TP_MELODY_SCALE, 0u, 0u};
        tc_chord edit = {(uint8_t)d, (uint8_t)e2, (uint8_t)f};
        tp_notes n;
        if (!tc_notes(l, c, edit, &n)) { puts("-"); continue; }
        for (uint32_t i = 0; i < n.count; ++i) printf("%s%02x", i ? " " : "", n.notes[i]);
        putchar('\n');
    }
    return 0;
}
