# SPDX-License-Identifier: GPL-3.0-or-later
"""Native host tests for the consolidated firmware (src/tb). Needs GCC (WSL).

  python3 scripts/run_tb_tests.py --label dev1 [--sanitize]

Receipts in build/tb/native-{opt|sanitize}-<label>/result.json, including the
source hashes they ran against. No device APIs or ARM execution.
"""
import argparse
import hashlib
import json
from pathlib import Path
import shutil
import subprocess

ROOT = Path(__file__).resolve().parents[1]
CORE = ['src/tb/controls.c', 'src/tb/layout.c', 'src/tb/engine.c', 'src/tb/input.c', 'src/tb/scan.c',
        'src/tb/midi_in.c', 'src/tb/sequence.c', 'src/tb/rhythm.c',
        'src/tb/chord_name.c', 'src/tb/chord_detect.c', 'src/tb/lanes.c']
SUITES = {
    'i2c': ['src/tb/i2c.c', 'tests/tb/test_i2c.c'],
    'mmio': ['-Itests/tb/hal', 'src/tb/touch_hw.c', 'src/tb/i2c.c', 'tests/tb/test_mmio.c'],
    'controls': [*CORE, 'tests/tb/test_controls.c'],
    'engine': [*CORE, 'tests/tb/test_engine.c'],
    'settings': [*CORE, 'src/tb/settings.c', 'tests/tb/test_settings.c'],
    'stage2': [*CORE, 'tests/tb/test_stage2.c'],
    'chords': [*CORE, 'src/tb/font.c', 'tests/tb/test_chords.c'],
    'sysex': [*CORE, 'src/tb/settings.c', 'src/tb/sysex.c', 'tests/tb/test_sysex.c'],
    'serial': [*CORE, 'src/tb/serial_midi.c', 'tests/tb/test_serial.c'],
    'stage3': [*CORE, 'tests/tb/test_stage3.c'],
    'stage4': [*CORE, 'src/tb/font.c', 'tests/tb/test_stage4.c'],
    'live': [*CORE, 'tests/tb/test_live.c'],
    'voicing': [*CORE, 'tests/tb/test_voicing.c'],
    # Seeded stress tests (fixed seeds, so a failure reproduces).
    'fuzz_engine': [*CORE, 'src/tb/serial_midi.c', 'tests/tb/test_fuzz_engine.c'],
    'fuzz_lanes': [*CORE, 'src/tb/settings.c', 'tests/tb/test_fuzz_lanes.c'],  # 1.5.0: the step sequencer
    'fuzz_layout': [*CORE, 'tests/tb/test_fuzz_layout.c'],
    'fuzz_controls': [*CORE, 'src/tb/i2c.c', 'src/tb/settings.c', 'tests/tb/test_fuzz_controls.c'],
    'fuzz_parsers': [*CORE, 'src/tb/settings.c', 'src/tb/sysex.c', 'tests/tb/test_fuzz_parsers.c'],
    'fixes': [*CORE, 'tests/tb/test_fixes.c'],
    'padtest': [*CORE, 'src/tb/settings.c', 'tests/tb/test_padtest.c'],
    'sequence': [*CORE, 'src/tb/settings.c', 'tests/tb/test_sequence.c'],
    'seq_checks': [*CORE, 'src/tb/settings.c', 'src/tb/sysex.c', 'src/tb/ui.c', 'src/tb/font.c',
                   'src/tb/serial_midi.c', 'tests/tb/test_seq_checks.c'],
    # Every screen on a fake display; main.c's loop against stand-ins for USB (tests/tb/main_hal).
    'screens': [*CORE, 'src/tb/settings.c', 'src/tb/sysex.c', 'src/tb/ui.c', 'src/tb/font.c',
                'src/tb/serial_midi.c', 'tests/tb/test_screens.c'],
    # 1.0.0: chord sets, hints, scale lock, centre-button actions and Shift.
    'features': [*CORE, 'src/tb/settings.c', 'src/tb/sysex.c', 'src/tb/ui.c', 'src/tb/font.c',
                 'src/tb/serial_midi.c', 'tests/tb/test_features.c'],
    # 1.3.0: the Full chord layout against a fake external memory chip.
    'full': [*CORE, 'tests/tb/test_full.c'],
    # 1.3.0: the chord grid (a chord type per row).
    'grid': [*CORE, 'tests/tb/test_grid.c'],
    # 1.4.0: rhythm arithmetic, the arpeggiator's new rates and Euclidean rhythms, their settings.
    'rhythm': [*CORE, 'src/tb/settings.c', 'tests/tb/test_rhythm.c'],
    # 1.4.0: pivot chords (shared chords, the button, arriving, lights, screen, storage) and chord
    # capture (detection, keys, pads and MIDI in, saving).
    'pivot': [*CORE, 'src/tb/settings.c', 'src/tb/sysex.c', 'src/tb/ui.c', 'src/tb/font.c',
              'src/tb/serial_midi.c', 'tests/tb/test_pivot.c'],
    'capture': [*CORE, 'src/tb/settings.c', 'src/tb/sysex.c', 'src/tb/ui.c', 'src/tb/font.c',
                'src/tb/serial_midi.c', 'tests/tb/test_capture.c'],
    # 1.4.0: Reset options, Factory settings and the other resets, each confirmed first.
    'lanes': [*CORE, 'src/tb/settings.c', 'src/tb/sysex.c', 'src/tb/ui.c', 'src/tb/font.c',
              'src/tb/serial_midi.c', 'tests/tb/test_lanes.c'],
    'resets': [*CORE, 'src/tb/settings.c', 'src/tb/sysex.c', 'src/tb/ui.c', 'src/tb/font.c',
               'src/tb/serial_midi.c', 'tests/tb/test_resets.c'],
    # 1.5.0: faster pads (each scan position's pads as read, the faster ADC) on the real scanner.
    'fastpads': [*CORE, 'tests/tb/test_fastpads.c'],
    'main_loop': ['-Itests/tb/main_hal', *CORE, 'src/tb/settings.c', 'src/tb/sysex.c', 'src/tb/serial_midi.c',
                  'src/tb/main.c', 'tests/tb/test_main.c'],
}


def main():
    p = argparse.ArgumentParser(description=__doc__)
    p.add_argument('--sanitize', action='store_true')
    p.add_argument('--label', required=True)
    args = p.parse_args()
    if not all(c.isalnum() or c in '-_' for c in args.label):
        raise ValueError('simple label required')
    cc = shutil.which('gcc')
    if not cc:
        raise SystemExit('BLOCKED: gcc missing (run under WSL)')
    out = ROOT/'build/tb'/('native-'+('sanitize-' if args.sanitize else 'opt-')+args.label)
    out.mkdir(parents=True, exist_ok=False)
    flags = ['-std=c11', '-Wall', '-Wextra', '-Wpedantic', '-Wconversion', '-Wshadow', '-Werror',
             '-O1' if args.sanitize else '-O2']
    if args.sanitize:
        flags += ['-g', '-fsanitize=address,undefined', '-fno-omit-frame-pointer',
                  '-fno-sanitize-recover=all']
    inputs = sorted({*(ROOT/'src/tb').glob('*'), *(ROOT/'tests/tb').rglob('*'), ROOT/'scripts/run_tb_tests.py'})
    receipt = dict(compiler=subprocess.check_output([cc, '--version'], text=True),
                   source_sha256={p.relative_to(ROOT).as_posix(): hashlib.sha256(p.read_bytes()).hexdigest()
                                  for p in inputs if p.is_file()}, suites={})
    failed = False
    for name, sources in SUITES.items():
        command = [cc, *flags, *sources, '-o', str(out/name)]
        built = subprocess.run(command, cwd=ROOT, capture_output=True, text=True, timeout=180)
        result = dict(command=command, compile_returncode=built.returncode, compile_stderr=built.stderr)
        if not built.returncode:
            run = subprocess.run([str(out/name)], cwd=ROOT, capture_output=True, text=True, timeout=600)  # fuzz_engine takes ~3.5 min sanitized
            result.update(returncode=run.returncode, stdout=run.stdout, stderr=run.stderr)
        receipt['suites'][name] = result
        ok = not built.returncode and not result.get('returncode') and not result.get('stderr')
        failed |= not ok
        print(name, 'PASS' if ok else 'FAIL', (result.get('stdout') or built.stderr or result.get('stderr', '')).strip()[:300])
    (out/'result.json').write_text(json.dumps(receipt, indent=2)+'\n', encoding='utf-8')
    raise SystemExit(1 if failed else 0)


if __name__ == '__main__':
    main()
