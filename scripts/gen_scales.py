# SPDX-License-Identifier: GPL-3.0-or-later
"""The scale library: one list for the firmware and the web editor.

Generates src/tb/scale_library.h (the board's built-in library, ids 37 and up, and the
mode families for the M1..M7 buttons) and webapp/tb-scales.js (every built-in scale with
its id, names and group, for the editor). Run after editing:  python scripts/gen_scales.py
Check that both files match this list:          python scripts/gen_scales.py --check

Scale ids are stored in settings, presets and favorites, so an id never changes
meaning: new scales get new ids at the end. Ids 0..11 and 24..36 are layout.c's own
tables (listed here too, for the editor); 12..23 are the user slots U1..U12.
Steps are semitones above the root. Short names (12 characters at most) show on the
board's display: capitals, digits, space and # - ( ) / and a lowercase b for flat.
"""
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
LIBRARY_FIRST = 37
SHORT_CHARS = set(' #-()/0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZb')

# (id, short name, full name, group, steps). Group order is the editor's order.
SCALES = [
    # Modes of the major scale: the M1..M7 buttons in the Major family.
    (0, 'MAJOR', 'Major (Ionian)', 'Major modes', [0, 2, 4, 5, 7, 9, 11]),
    (2, 'DORIAN', 'Dorian', 'Major modes', [0, 2, 3, 5, 7, 9, 10]),
    (3, 'PHRYGIAN', 'Phrygian', 'Major modes', [0, 1, 3, 5, 7, 8, 10]),
    (4, 'LYDIAN', 'Lydian', 'Major modes', [0, 2, 4, 6, 7, 9, 11]),
    (5, 'MIXOLYDIAN', 'Mixolydian', 'Major modes', [0, 2, 4, 5, 7, 9, 10]),
    (1, 'MINOR', 'Natural minor (Aeolian)', 'Major modes', [0, 2, 3, 5, 7, 8, 10]),
    (6, 'LOCRIAN', 'Locrian', 'Major modes', [0, 1, 3, 5, 6, 8, 10]),
    # Modes of melodic minor.
    (8, 'MEL MINOR', 'Melodic minor (jazz minor)', 'Melodic minor modes', [0, 2, 3, 5, 7, 9, 11]),
    (37, 'DORIAN b2', 'Dorian ♭2 (Phrygian ♮6)', 'Melodic minor modes', [0, 1, 3, 5, 7, 9, 10]),
    (38, 'LYDIAN AUG', 'Lydian augmented', 'Melodic minor modes', [0, 2, 4, 6, 8, 9, 11]),
    (35, 'LYDIAN DOM', 'Lydian dominant (overtone, acoustic)', 'Melodic minor modes', [0, 2, 4, 6, 7, 9, 10]),
    (39, 'MIXOLYD b6', 'Mixolydian ♭6 (Aeolian dominant, Hindu)', 'Melodic minor modes', [0, 2, 4, 5, 7, 8, 10]),
    (40, 'LOCRIAN #2', 'Locrian ♮2 (half-diminished)', 'Melodic minor modes', [0, 2, 3, 5, 6, 8, 10]),
    (36, 'ALTERED', 'Altered (super Locrian)', 'Melodic minor modes', [0, 1, 3, 4, 6, 8, 10]),
    # Modes of harmonic minor.
    (7, 'HARM MINOR', 'Harmonic minor', 'Harmonic minor modes', [0, 2, 3, 5, 7, 8, 11]),
    (41, 'LOCRIAN #6', 'Locrian ♮6', 'Harmonic minor modes', [0, 1, 3, 5, 6, 9, 10]),
    (42, 'IONIAN #5', 'Ionian ♯5 (augmented major)', 'Harmonic minor modes', [0, 2, 4, 5, 8, 9, 11]),
    (43, 'DORIAN #4', 'Dorian ♯4 (Romanian minor, Ukrainian Dorian)', 'Harmonic minor modes', [0, 2, 3, 6, 7, 9, 10]),
    (28, 'PHRYG DOM', 'Phrygian dominant (Spanish, Freygish)', 'Harmonic minor modes', [0, 1, 4, 5, 7, 8, 10]),
    (44, 'LYDIAN #2', 'Lydian ♯2', 'Harmonic minor modes', [0, 3, 4, 6, 7, 9, 11]),
    (45, 'ULTRALOCRIAN', 'Ultralocrian (altered ♭♭7)', 'Harmonic minor modes', [0, 1, 3, 4, 6, 8, 9]),
    # Modes of harmonic major.
    (26, 'HARM MAJOR', 'Harmonic major', 'Harmonic major modes', [0, 2, 4, 5, 7, 8, 11]),
    (46, 'DORIAN b5', 'Dorian ♭5', 'Harmonic major modes', [0, 2, 3, 5, 6, 9, 10]),
    (47, 'PHRYGIAN b4', 'Phrygian ♭4', 'Harmonic major modes', [0, 1, 3, 4, 7, 8, 10]),
    (48, 'LYDIAN b3', 'Lydian ♭3 (melodic minor ♯4)', 'Harmonic major modes', [0, 2, 3, 6, 7, 9, 11]),
    (49, 'MIXOLYD b2', 'Mixolydian ♭2', 'Harmonic major modes', [0, 1, 4, 5, 7, 9, 10]),
    (50, 'LYD AUG #2', 'Lydian augmented ♯2', 'Harmonic major modes', [0, 3, 4, 6, 8, 9, 11]),
    (51, 'LOCRIAN bb7', 'Locrian ♭♭7', 'Harmonic major modes', [0, 1, 3, 5, 6, 8, 9]),
    # Five notes.
    (9, 'MAJ PENT', 'Major pentatonic (Bhupali)', 'Pentatonic', [0, 2, 4, 7, 9]),
    (10, 'MIN PENT', 'Minor pentatonic', 'Pentatonic', [0, 3, 5, 7, 10]),
    (52, 'EGYPTIAN', 'Egyptian (suspended pentatonic)', 'Pentatonic', [0, 2, 5, 7, 10]),
    (53, 'YO', 'Yo (Durga)', 'Pentatonic', [0, 2, 5, 7, 9]),
    (54, 'MAN GONG', 'Man gong (Malkauns)', 'Pentatonic', [0, 3, 5, 8, 10]),
    (55, 'DOM PENT', 'Dominant pentatonic', 'Pentatonic', [0, 2, 4, 7, 10]),
    (56, 'MINOR 6 PENT', 'Minor 6 pentatonic', 'Pentatonic', [0, 3, 5, 7, 9]),
    (33, 'HIRAJOSHI', 'Hirajoshi', 'Pentatonic', [0, 2, 3, 7, 8]),
    (34, 'IN SEN', 'In sen', 'Pentatonic', [0, 1, 5, 7, 10]),
    (57, 'IWATO', 'Iwato', 'Pentatonic', [0, 1, 5, 6, 10]),
    (58, 'KUMOI', 'Kumoi (Shivaranjani)', 'Pentatonic', [0, 2, 3, 7, 9]),
    (59, 'PELOG', 'Pelog (12-note approximation)', 'Pentatonic', [0, 1, 3, 7, 8]),
    (60, 'RYUKYU', 'Ryukyu (Okinawan)', 'Pentatonic', [0, 4, 5, 7, 11]),
    (61, 'HAMSADHWANI', 'Hamsadhwani', 'Pentatonic', [0, 2, 4, 7, 11]),
    (62, 'HINDOL', 'Hindol', 'Pentatonic', [0, 4, 6, 9, 11]),
    (63, 'KALAVATI', 'Kalavati', 'Pentatonic', [0, 4, 7, 9, 10]),
    (64, 'CHANDRAKAUNS', 'Chandrakauns', 'Pentatonic', [0, 3, 5, 8, 11]),
    # Six notes.
    (24, 'BLUES', 'Blues (minor)', 'Six notes', [0, 3, 5, 6, 7, 10]),
    (65, 'MAJOR BLUES', 'Major blues', 'Six notes', [0, 2, 3, 4, 7, 9]),
    (66, 'MAJOR HEXA', 'Major hexatonic', 'Six notes', [0, 2, 4, 5, 7, 9]),
    (67, 'MINOR HEXA', 'Minor hexatonic', 'Six notes', [0, 2, 3, 5, 7, 10]),
    (25, 'WHOLE TONE', 'Whole tone', 'Six notes', [0, 2, 4, 6, 8, 10]),
    (68, 'AUGMENTED', 'Augmented', 'Six notes', [0, 3, 4, 7, 8, 11]),
    (69, 'SIX-TONE SYM', 'Six-tone symmetrical', 'Six notes', [0, 1, 4, 5, 8, 9]),
    (70, 'PROMETHEUS', 'Prometheus', 'Six notes', [0, 2, 4, 6, 9, 10]),
    (71, 'PROMETH NEAP', 'Prometheus Neapolitan', 'Six notes', [0, 1, 4, 6, 9, 10]),
    (72, 'TRITONE', 'Tritone', 'Six notes', [0, 1, 4, 6, 7, 10]),
    (73, 'ISTRIAN', 'Istrian', 'Six notes', [0, 1, 3, 4, 6, 7]),
    (74, 'MESSIAEN 5', 'Messiaen mode 5', 'Six notes', [0, 1, 5, 6, 7, 11]),
    # Other seven-note scales.
    (27, 'HUNG MINOR', 'Hungarian minor', 'Other seven-note scales', [0, 2, 3, 6, 7, 8, 11]),
    (75, 'HUNG MAJOR', 'Hungarian major', 'Other seven-note scales', [0, 3, 4, 6, 7, 9, 10]),
    (76, 'HUNG GYPSY', 'Hungarian gypsy', 'Other seven-note scales', [0, 2, 3, 6, 7, 8, 10]),
    (29, 'DOUBLE HARM', 'Double harmonic (Byzantine, Bhairav)', 'Other seven-note scales', [0, 1, 4, 5, 7, 8, 11]),
    (77, 'NEAP MAJOR', 'Neapolitan major', 'Other seven-note scales', [0, 1, 3, 5, 7, 9, 11]),
    (78, 'NEAP MINOR', 'Neapolitan minor', 'Other seven-note scales', [0, 1, 3, 5, 7, 8, 11]),
    (79, 'PERSIAN', 'Persian', 'Other seven-note scales', [0, 1, 4, 5, 6, 8, 11]),
    (80, 'ORIENTAL', 'Oriental', 'Other seven-note scales', [0, 1, 4, 5, 6, 9, 10]),
    (81, 'MAJ LOCRIAN', 'Major Locrian', 'Other seven-note scales', [0, 2, 4, 5, 6, 8, 10]),
    (82, 'LYDIAN MINOR', 'Lydian minor', 'Other seven-note scales', [0, 2, 4, 6, 7, 8, 10]),
    (83, 'LEADING WT', 'Leading whole tone', 'Other seven-note scales', [0, 2, 4, 6, 8, 10, 11]),
    (84, 'ENIGMATIC', 'Enigmatic', 'Other seven-note scales', [0, 1, 4, 6, 8, 10, 11]),
    (85, 'TODI', 'Todi (thaat)', 'Other seven-note scales', [0, 1, 3, 6, 7, 8, 11]),
    (86, 'POORVI', 'Poorvi (thaat)', 'Other seven-note scales', [0, 1, 4, 6, 7, 8, 11]),
    (87, 'MARWA', 'Marwa (thaat)', 'Other seven-note scales', [0, 1, 4, 6, 7, 9, 11]),
    (88, 'LALIT', 'Lalit', 'Other seven-note scales', [0, 1, 4, 5, 6, 9, 11]),
    # Eight notes and more.
    (32, 'BEBOP DOM', 'Bebop dominant', 'Eight notes and more', [0, 2, 4, 5, 7, 9, 10, 11]),
    (89, 'BEBOP MAJOR', 'Bebop major', 'Eight notes and more', [0, 2, 4, 5, 7, 8, 9, 11]),
    (90, 'BEBOP DORIAN', 'Bebop Dorian', 'Eight notes and more', [0, 2, 3, 4, 5, 7, 9, 10]),
    (91, 'BEB MEL MIN', 'Bebop melodic minor', 'Eight notes and more', [0, 2, 3, 5, 7, 8, 9, 11]),
    (92, 'SPANISH 8', 'Spanish eight-note', 'Eight notes and more', [0, 1, 3, 4, 5, 6, 8, 10]),
    (30, 'DIM W-H', 'Diminished (whole-half)', 'Eight notes and more', [0, 2, 3, 5, 6, 8, 9, 11]),
    (31, 'DIM H-W', 'Diminished (half-whole)', 'Eight notes and more', [0, 1, 3, 4, 6, 7, 9, 10]),
    (93, 'MESSIAEN 3', 'Messiaen mode 3', 'Eight notes and more', [0, 2, 3, 4, 6, 7, 8, 10, 11]),
    (94, 'MESSIAEN 4', 'Messiaen mode 4', 'Eight notes and more', [0, 1, 2, 5, 6, 7, 8, 11]),
    (95, 'MESSIAEN 6', 'Messiaen mode 6', 'Eight notes and more', [0, 2, 4, 5, 6, 8, 10, 11]),
    (96, 'MESSIAEN 7', 'Messiaen mode 7', 'Eight notes and more', [0, 1, 2, 3, 5, 6, 7, 8, 9, 11]),
    (97, 'BLUES 9', 'Composite blues (nine notes)', 'Eight notes and more', [0, 2, 3, 4, 5, 6, 7, 9, 10]),
    (11, 'CHROMATIC', 'Chromatic', 'Eight notes and more', list(range(12))),
]

# Mode families: the M1..M7 buttons play these seven scales (mode n starts on step n).
FAMILIES = [
    ('MAJOR', 'Major', [0, 2, 3, 4, 5, 1, 6]),
    ('MEL MINOR', 'Melodic minor', [8, 37, 38, 35, 39, 40, 36]),
    ('HARM MINOR', 'Harmonic minor', [7, 41, 42, 43, 28, 44, 45]),
    ('HARM MAJOR', 'Harmonic major', [26, 46, 47, 48, 49, 50, 51]),
]

# The scales layout.c defines itself (ids 0..11 classic, 24..36 extra), as it has them.
LAYOUT_TABLES = {
    0: [0, 2, 4, 5, 7, 9, 11], 1: [0, 2, 3, 5, 7, 8, 10], 2: [0, 2, 3, 5, 7, 9, 10], 3: [0, 1, 3, 5, 7, 8, 10],
    4: [0, 2, 4, 6, 7, 9, 11], 5: [0, 2, 4, 5, 7, 9, 10], 6: [0, 1, 3, 5, 6, 8, 10], 7: [0, 2, 3, 5, 7, 8, 11],
    8: [0, 2, 3, 5, 7, 9, 11], 9: [0, 2, 4, 7, 9], 10: [0, 3, 5, 7, 10], 11: list(range(12)),
    24: [0, 3, 5, 6, 7, 10], 25: [0, 2, 4, 6, 8, 10], 26: [0, 2, 4, 5, 7, 8, 11], 27: [0, 2, 3, 6, 7, 8, 11],
    28: [0, 1, 4, 5, 7, 8, 10], 29: [0, 1, 4, 5, 7, 8, 11], 30: [0, 2, 3, 5, 6, 8, 9, 11],
    31: [0, 1, 3, 4, 6, 7, 9, 10], 32: [0, 2, 4, 5, 7, 9, 10, 11], 33: [0, 2, 3, 7, 8], 34: [0, 1, 5, 7, 10],
    35: [0, 2, 4, 6, 7, 9, 10], 36: [0, 1, 3, 4, 6, 8, 10],
}


def mask(steps):
    return sum(1 << s for s in steps)


def rotate(steps, n):
    return sorted((s - steps[n]) % 12 for s in steps)


def validate():
    by_id = {}
    for sid, short, name, group, steps in SCALES:
        assert sid not in by_id, f'id {sid} twice'
        assert not 12 <= sid <= 23, f'id {sid} is a user slot'
        assert steps == sorted(set(steps)) and steps[0] == 0 and steps[-1] < 12, short
        assert 5 <= len(steps) <= 12, short
        assert 1 <= len(short) <= 12 and set(short) <= SHORT_CHARS, short
        by_id[sid] = steps
    masks = [mask(s) for s in by_id.values()]
    assert len(masks) == len(set(masks)), 'two scales with the same notes'
    library = sorted(i for i in by_id if i >= LIBRARY_FIRST)
    assert library == list(range(LIBRARY_FIRST, LIBRARY_FIRST + len(library))), 'library ids must be consecutive'
    assert set(by_id) - set(library) == set(LAYOUT_TABLES), 'every layout.c scale listed'
    for sid, steps in LAYOUT_TABLES.items():
        assert by_id[sid] == steps, f'id {sid} differs from layout.c'
    for short, _, ids in FAMILIES:
        parent = by_id[ids[0]]
        assert len(parent) == 7 and len(ids) == 7, short
        for n, sid in enumerate(ids):
            assert by_id[sid] == rotate(parent, n), f'{short} mode {n + 1} is not a rotation'
    return by_id, library


def c_header(by_id, library):
    entries = {sid: (short, steps) for sid, short, _, _, steps in SCALES}
    lines = ['/* SPDX-License-Identifier: GPL-3.0-or-later - the built-in scale library (ids 37 and up) and the',
             ' * mode families, generated by scripts/gen_scales.py from its list; edit that, then rerun it.',
             ' * layout.h includes it for the sizes; layout.c defines TB_SCALE_LIBRARY_DATA for the tables. */',
             '#ifndef TB_SCALE_LIBRARY_H', '#define TB_SCALE_LIBRARY_H',
             f'#define TC_LIBRARY_SCALES {len(library)}u',
             f'#define TC_FAMILIES {len(FAMILIES)}u',
             '#endif',
             '#if defined(TB_SCALE_LIBRARY_DATA) && !defined(TB_SCALE_LIBRARY_TABLES)',
             '#define TB_SCALE_LIBRARY_TABLES',
             'static const uint8_t library_steps[TC_LIBRARY_SCALES][12]={']
    for sid in library:
        lines.append('    {' + ','.join(map(str, entries[sid][1])) + '}, /* ' + str(sid) + ' */')
    lines.append('};')
    lines.append('static const uint8_t library_counts[TC_LIBRARY_SCALES]={' +
                 ','.join(str(len(entries[sid][1])) for sid in library) + '};')
    lines.append('static const char *const library_names[TC_LIBRARY_SCALES]={')
    for i in range(0, len(library), 6):
        lines.append('    ' + ','.join(f'"{entries[sid][0]}"' for sid in library[i:i + 6]) + ',')
    lines.append('};')
    lines.append('static const char *const family_names[TC_FAMILIES]={' +
                 ','.join(f'"{short}"' for short, _, _ in FAMILIES) + '};')
    lines.append('/* M1..M7 in each family. */')
    lines.append('static const uint8_t family_modes[TC_FAMILIES][7]={')
    for short, _, ids in FAMILIES:
        lines.append('    {' + ','.join(map(str, ids)) + '}, /* ' + short + ' */')
    lines.append('};')
    lines.append('#endif')
    return '\n'.join(lines) + '\n'


def js_module():
    lines = ['// SPDX-License-Identifier: GPL-3.0-or-later - every built-in scale on the board, generated by',
             '// scripts/gen_scales.py (edit its list, then rerun it). id: the board\'s scale id (SELECT_SCALE,',
             '// favorites); short: the name on its display; steps: semitones above the root.',
             f'export const LIBRARY_FIRST = {LIBRARY_FIRST};',
             'export const USER_FIRST = 12; // U1..U12 are ids 12..23',
             'export const SCALES = Object.freeze([']
    for sid, short, name, group, steps in SCALES:
        lines.append(f"  {{ id: {sid}, short: '{short}', name: '{name}', group: '{group}', steps: {steps} }},")
    lines.append(']);')
    lines.append('// The M1..M7 buttons in each family (Settings > Play > Family on the board).')
    lines.append('export const FAMILIES = Object.freeze([')
    for short, name, ids in FAMILIES:
        lines.append(f"  {{ short: '{short}', name: '{name}', modes: {ids} }},")
    lines.append(']);')
    return '\n'.join(lines) + '\n'


def main():
    by_id, library = validate()
    outputs = {ROOT / 'src/tb/scale_library.h': c_header(by_id, library), ROOT / 'webapp/tb-scales.js': js_module()}
    if '--check' in sys.argv:
        stale = [p.relative_to(ROOT).as_posix() for p, text in outputs.items()
                 if not p.exists() or p.read_text(encoding='utf-8') != text]
        if stale:
            print('STALE (run python scripts/gen_scales.py): ' + ', '.join(stale))
            return 1
        print(f'PASS: scale library: {len(SCALES)} built-in scales ({len(library)} in the library), '
              f'{len(FAMILIES)} families, generated files current')
        return 0
    for path, text in outputs.items():
        path.write_text(text, encoding='utf-8', newline='\n')
    print(f'wrote {len(SCALES)} scales ({len(library)} library), {len(FAMILIES)} families')
    return 0


if __name__ == '__main__':
    sys.exit(main())
