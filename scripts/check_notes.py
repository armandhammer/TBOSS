# SPDX-License-Identifier: GPL-3.0-or-later
"""Check every pad note of the firmware against an independent music-theory model.

Compiles the firmware's own layout code (src/tb) natively and compares all 96 pads
for 12 roots x 12 built-in scales, the chord-lowering edits and every sus/6th/7th/9th
chord edit with notes derived from textbook step patterns and triad qualities.
Run where GCC exists (Linux, macOS, WSL):  python3 scripts/check_notes.py
"""
import shutil
import subprocess
import sys
import tempfile
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
SOURCES = ('src/tb/controls.c', 'src/tb/layout.c', 'src/tb/engine.c', 'src/tb/input.c', 'src/tb/scan.c',
           'src/tb/midi_in.c', 'src/tb/sequence.c', 'src/tb/rhythm.c',
           'src/tb/chord_name.c', 'src/tb/chord_detect.c', 'src/tb/lanes.c')

HARNESS = r'''
#include <stdio.h>
#include "controls.h"
volatile ts_abi ts_status;  /* firmware main.c owns this; the engine links to it */
static tp_engine engine;
static tc_controls state;
int main(void) {
#ifdef TP_DEFAULT_LAYOUT
    const tp_layout demo = TP_DEFAULT_LAYOUT;
#else
    const tp_layout demo = TP_DEMO_LAYOUT;
#endif
    printf("B %u %u\n", demo.melody_base, demo.chord_base);
    for (unsigned b = 0; b < 12u; b++) printf("M %u %u\n", b, tc_mode_buttons[b]);
    for (unsigned mode = 0; mode < TC_SCALES; mode++)
    for (unsigned tonic = 0; tonic < 12u; tonic++) {
        tp_layout layout = demo;
        layout.tonic = (uint8_t)tonic; layout.mode = (__typeof__(layout.mode))mode;
        tc_init(&state);
        printf("A %u %u %d\n", mode, tonic, tc_set_layout(&state, &engine, layout));
        for (unsigned lower = 0; lower < 8u; lower++)
        for (unsigned k = 0; k < 96u; k++) {
            if (lower && k >= 48u) continue;
            tp_notes n = {0};
#ifdef TP_MAX_NOTES
            tc_chord edit = {(uint8_t)lower, 0, 0};
            int ok = tc_notes(layout, k, edit, &n);
#else
            int ok = tc_notes(layout, k, (uint8_t)lower, &n);
#endif
            printf("N %u %u %u %u %d %u", mode, tonic, lower, k, ok, n.count);
            for (unsigned i = 0; i < n.count; i++) printf(" %u", n.notes[i]);
            printf("\n");
        }
#ifdef TP_MAX_NOTES
        /* Chord edits: sus 0/2/4 x extensions 0..7 on every chord pad. */
        for (unsigned sus = 0; sus <= 4u; sus += 2u)
        for (unsigned add = 0; add < 8u; add++)
        for (unsigned k = 0; k < 48u; k++) {
            tp_notes n = {0};
            tc_chord edit = {0, (uint8_t)sus, (uint8_t)add};
            int ok = tc_notes(layout, k, edit, &n);
            printf("E %u %u %u %u %u %d %u", mode, tonic, sus, add, k, ok, n.count);
            for (unsigned i = 0; i < n.count; i++) printf(" %u", n.notes[i]);
            printf("\n");
        }
#endif
    }
    return 0;
}
'''

# Independent specification. W=2, H=1, 3=augmented second / minor-third gap.
STEPS = {
    'Major': 'WWHWWWH', 'Natural minor': 'WHWWHWW', 'Dorian': 'WHWWWHW',
    'Phrygian': 'HWWWHWW', 'Lydian': 'WWWHWWH', 'Mixolydian': 'WWHWWHW',
    'Locrian': 'HWWHWWW', 'Harmonic minor': 'WHWWH3H', 'Melodic minor': 'WHWWWWH',
    'Major pentatonic': 'WW3W3', 'Minor pentatonic': '3WW3W', 'Chromatic': 'H' * 12,
}
ORDER = list(STEPS)  # the firmware's scale ids 0..11
# Textbook diatonic triad qualities per degree (M major, m minor, d dim, A aug).
QUALITIES = {
    'Major': 'MmmMMmd', 'Natural minor': 'mdMmmMM', 'Dorian': 'mmMMmdM',
    'Phrygian': 'mMMmdMm', 'Lydian': 'MMmdMmm', 'Mixolydian': 'MmdMmmM',
    'Locrian': 'dMmmMMm', 'Harmonic minor': 'mdAmMMd', 'Melodic minor': 'mmAMMdd',
}
# Printed mode-button labels left to right. Mk = k-th mode of the major
# scale (major step pattern rotated by k-1); 5 = major pentatonic; 12 = chromatic.
BUTTONS = ('5', '12', 'M1', 'M2', 'M3', 'M4', 'M5', 'M6', 'M7', 'U1', 'U2', 'U3')
EMPTY = 0xFF


def button_steps(label):
    if label[0] == 'M':
        k = int(label[1]) - 1
        return STEPS['Major'][k:] + STEPS['Major'][:k]
    return {'5': STEPS['Major pentatonic'], '12': STEPS['Chromatic']}.get(label)


NOTE = ('C', 'C#', 'D', 'D#', 'E', 'F', 'F#', 'G', 'G#', 'A', 'A#', 'B')


def name(midi):
    return f'{NOTE[midi % 12]}{midi // 12 - 1}'


def scale_notes(start, steps, count):
    """Ascending MIDI notes of the scale from `start`, purely from the step pattern."""
    size = {'W': 2, 'H': 1, '3': 3}
    notes, pitch, i = [], start, 0
    while len(notes) < count:
        notes.append(pitch)
        pitch += size[steps[i % len(steps)]]
        i += 1
    return notes


def quality(a, b, c):
    third, fifth = b - a, c - a
    return {(4, 7): 'M', (3, 7): 'm', (3, 6): 'd', (4, 8): 'A'}.get((third, fifth), '?')


def expected(scale, tonic, melody_base, chord_base):
    """pad -> expected notes. Rows 0..3 top..bottom; columns 0..11 = scale steps 0..11.
    Right half: one note, bottom row starts on the root, each row up an octave.
    Left half: triad on scale step c: bottom root position, then first inversion,
    second inversion, top root position an octave up."""
    steps = STEPS[scale]
    pads = {}
    for k in range(96):
        side, row, col = k // 48, k % 4, (k % 48) // 4
        if side:
            pads[k] = (scale_notes(melody_base + tonic + 12 * (3 - row), steps, 12)[col],)
        else:
            s = scale_notes(chord_base + tonic, steps, 17)
            r, t, f = s[col], s[col + 2], s[col + 4]
            pads[k] = {3: (r, t, f), 2: (t, f, r + 12), 1: (f, r + 12, t + 12),
                       0: (r + 12, t + 12, f + 12)}[row]
    return pads


def expected_edit(scale, tonic, chord_base, sus, add, k):
    """Independent chord-edit model: the third becomes the 2nd/4th scale step for
    sus2/sus4; 6th/7th/9th are scale steps 5/6/8 above the chord root, stacked
    above the root-position chord (top row: an octave up) in that order."""
    row, col = k % 4, (k % 48) // 4
    s = scale_notes(chord_base + tonic, STEPS[scale], 24)
    r, t, f = s[col], s[col + {0: 2, 2: 1, 4: 3}[sus]], s[col + 4]
    up = 12 if row == 0 else 0
    triad = {3: (r, t, f), 2: (t, f, r + 12), 1: (f, r + 12, t + 12), 0: (r + 12, t + 12, f + 12)}[row]
    extra = tuple(s[col + d] + up for bit, d in ((1, 5), (2, 6), (4, 8)) if add & bit)
    return triad + extra


def build_and_run():
    cc = shutil.which('gcc')
    if not cc:
        raise SystemExit('BLOCKED: gcc not found (on Windows run it under WSL)')
    with tempfile.TemporaryDirectory() as tmp:
        harness, exe = Path(tmp) / 'notes.c', Path(tmp) / 'notes'
        harness.write_text(HARNESS)
        command = [cc, '-std=c11', '-Wall', '-Wextra', '-Werror', '-O2',
                   '-I', str(ROOT / 'src/tb'), str(harness),
                   *(str(ROOT / s) for s in SOURCES), '-o', str(exe)]
        subprocess.run(command, check=True, cwd=ROOT)
        return subprocess.run([str(exe)], check=True, capture_output=True, text=True).stdout


def check(output):
    failures, accepted, notes, bases, buttons, edits = [], {}, {}, None, {}, {}
    for line in output.splitlines():
        kind, *v = line.split()
        v = list(map(int, v))
        if kind == 'B':
            bases = v
        elif kind == 'M':
            buttons[v[0]] = v[1]
        elif kind == 'A':
            accepted[v[0], v[1]] = v[2]
        elif kind == 'E':
            mode, tonic, sus, add, k, ok, count, *n = v
            edits[mode, tonic, sus, add, k] = (ok, tuple(n[:count]))
        else:
            mode, tonic, lower, k, ok, count, *n = v
            notes[mode, tonic, lower, k] = (ok, tuple(n[:count]))
    melody_base, chord_base = bases
    for b, label in enumerate(BUTTONS):
        want, got = button_steps(label), buttons[b]
        if want is None:
            # User slot: user scale id 12 + n (set from the web editor), or empty.
            if got not in (EMPTY, len(ORDER) + BUTTONS.index(label) - BUTTONS.index('U1')):
                failures.append(f'button {label}: expected a user slot, got scale id {got}')
        elif got == EMPTY or STEPS[ORDER[got]] != want:
            failures.append(f'button {label}: scale id {got} is not step pattern {want}')
    for mode, scale in enumerate(ORDER):
        for tonic in range(12):
            label = f'{NOTE[tonic]} {scale}'
            if not accepted[mode, tonic]:
                failures.append(f'{label}: firmware rejects the layout')
            want = expected(scale, tonic, melody_base, chord_base)
            pcs = {(tonic + x) % 12 for x in scale_notes(0, STEPS[scale], len(STEPS[scale]))}
            for k in range(96):
                ok, got = notes[mode, tonic, 0, k]
                if not ok or got != want[k]:
                    failures.append(f'{label} pad {k}: got {got} expected {want[k]}')
                if any(p % 12 not in pcs for p in got):
                    failures.append(f'{label} pad {k}: note outside scale {got}')
            if scale in QUALITIES:
                for col in range(7):
                    q = quality(*notes[mode, tonic, 0, 4 * col + 3][1])
                    if q != QUALITIES[scale][col]:
                        failures.append(f'{label} degree {col + 1}: triad {q}, '
                                        f'textbook {QUALITIES[scale][col]}')
            # Function 0..2 lower the chord's root/third/fifth by one octave.
            for k in range(48):
                base = want[k]
                r = want[k - k % 4 + 3][0]  # this column's root-position root
                member = {r % 12: 0, want[k - k % 4 + 3][1] % 12: 1,
                          want[k - k % 4 + 3][2] % 12: 2}
                for lower in range(1, 8):
                    ok, got = notes[mode, tonic, lower, k]
                    target = tuple(p - 12 if lower >> member[p % 12] & 1 else p for p in base)
                    if not ok or got != target:
                        failures.append(f'{label} pad {k} lower={lower}: got {got} expected {target}')
    for (mode, tonic, sus, add, k), (ok, got) in edits.items():
        want = expected_edit(ORDER[mode], tonic, chord_base, sus, add, k)
        if not ok or got != want:
            failures.append(f'{NOTE[tonic]} {ORDER[mode]} pad {k} sus{sus} add{add}: got {got} expected {want}')
    if edits:
        print(f'Chord edits checked: {len(edits)} (sus2/sus4, 6th/7th/9th on every chord pad)')
    return failures, notes, melody_base, chord_base, buttons


def main():
    failures, notes, melody_base, chord_base, buttons = check(build_and_run())
    combos = len(ORDER) * 12
    print(f'Firmware bases: melody {melody_base} ({name(melody_base)}), chords {chord_base} ({name(chord_base)})')
    print(f'Checked {combos} root/scale layouts x 96 pads, plus 7 lowering masks x 48 chord pads each '
          f'({combos * (96 + 7 * 48)} pad results).')
    major = ORDER.index('Major')
    minor = ORDER.index('Natural minor')
    show = lambda m, t, k: ' '.join(name(p) for p in notes[m, t, 0, k][1])
    def button_name(b):
        got = buttons[b]
        return 'empty' if got == EMPTY else ORDER[got] if got < len(ORDER) else f'user scale {got - len(ORDER) + 1}'
    print('Mode buttons:', ', '.join(f'{label}={button_name(b)}' for b, label in enumerate(BUTTONS)))
    print('Spot checks (bottom row = row 3):')
    print('  C Major   chord pads, bottom row:', ' | '.join(show(major, 0, 4 * c + 3) for c in range(7)))
    print('  C Major   melody, bottom row:    ', ' '.join(show(major, 0, 48 + 4 * c + 3) for c in range(12)))
    print('  A minor   melody, bottom row:    ', ' '.join(show(minor, 9, 48 + 4 * c + 3) for c in range(12)))
    print('  D Major   chord col 0, rows 3..0:', ' | '.join(show(major, 2, r) for r in (3, 2, 1, 0)))
    if failures:
        print(f'FAIL: {len(failures)} mismatches')
        for f in failures[:40]:
            print('  ' + f)
        sys.exit(1)
    print('PASS: every pad matches the independent scale/triad model.')


if __name__ == '__main__':
    main()
