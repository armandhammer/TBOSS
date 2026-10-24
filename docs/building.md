# Building and testing TBOSS

## Build the firmware

You need **Python 3.9+**, **git**, and the **Arm GNU Toolchain**
(`arm-none-eabi-gcc`) from [Arm](https://developer.arm.com/downloads/-/arm-gnu-toolchain-downloads).
Version **14.3.Rel1** reproduces the released images byte for byte; other recent
versions work but give a different hash.

```
python scripts/tboss_build.py --fetch      # first time: downloads the pinned dependencies into deps/
python scripts/tboss_build.py              # builds build/tboss/theoryboard-usb.bin
```

If the toolchain is not on your PATH, add `--toolchain "<folder containing arm-none-eabi-gcc>"`.

The script:
1. checks that `deps/` holds exactly the pinned commits of TinyUSB (MIT), ST's CMSIS
   STM32F4 device headers and Arm's CMSIS core (Apache-2.0);
2. makes local, checked copies of four TinyUSB files with small additions (a hook that
   reports USB events and MIDI transmit completions to the firmware, and an upper bound
   on every busy-wait); the downloaded originals stay untouched;
3. compiles `src/tb` with strict warnings (`-Wall -Wextra -Wpedantic -Wconversion
   -Wshadow -Werror`) and links for `0x08040000`;
4. validates the image (vector table, entry point, size below the settings sector,
   RAM layout, diagnostics addresses) and runs `scripts/firmware_safety.py`, which
   rejects any use of the debug pins, option bytes, low-power modes, flash writes
   outside the settings code, or non-read commands to the external memory.

Output: `build/tboss/theoryboard-usb.bin` (to install), `.elf`, `.map`, and
`validation.json` (hashes of every input). The SHA256 is printed at the end.

| Release | Bytes | SHA256 (Arm GNU Toolchain 14.3.Rel1) |
| --- | --- | --- |
| v0.2.0 | 40,224 | `3a6268580496cc1a4347dcd9162e16a4ce7e0a18858984cdac33cb3a691d2f86` |
| v0.3.0 | 41,176 | `a0d17a07d8830b20ba3dd826bfde57f616503cf17de57b798e53b814be7d2d3a` |
| v0.4.0 | 43,084 | `69cf4eaf8184280d7dad68ddd10643b20d52a94a58fc217d6c7b8c877a5f6ef2` |
| v0.5.0 | 44,084 | `1256c8d01bd92879bd92c656b06f51dfd607eb425cc16dc18010127f76e6bff5` |
| v0.5.1 | 44,364 | `95836fad9b3823d225bffb337b1e4572b75b16c36bc753f9742db062b9f4cc7e` |
| v0.6.0 | 50,024 | `968a1340ce88ebb2f88300d147d170fff3809bc984fa35b74747a9f2b664391f` |
| v0.6.1 | 50,616 | `2cf5cfeab588fc76eb9da9f1261a9661316639440d24f9f65f1142f14bad7a7b` |
| v0.6.2 | 50,604 | `dc999f645468acb8b01d8c5346471139786aabb4c094c869882ee21e608c6f9b` |
| v0.6.3 | 50,628 | `967e07dc480f4d6468f638237971896d1516488665583a291fa1a3fa61fc1d92` |
| v0.7.0 | 53,680 | `1886fb4014c25904a18c1eabb60e99bff9f67f04cbd35f3bdea6d34d9c8d1146` |
| v0.9.0 | 57,420 | `da6fe85255d37bc498668df7aea57728402783b79b630ad162e2098ced9b3c85` |
| v0.9.1 | 58,700 | `b2ff14791d69c0a1d8dd1b5c4f06cd93e73535fc3cd056fe3f6800a443a7fbeb` |
| v0.10.0 | 59,476 | `3b0b3fabcdca1749fa3ecd0d40ec38d644a70a74ac9b5d709e911ca5aef0e3f9` |
| v0.11.0 | 64,912 | `d8a145d11648b4ef9a4275db8c58e1043137343289c064d560949c75587a5b3c` |
| v1.0.0-beta.1 | 71,616 | `7ffae3962605818468b20d55cefeeb4b230312fde426fdc0fb355e0b7bff2267` |
| v1.3.0-beta.1 | 74,900 | `f7b623bb2be4f670234eb49aeed96e878e4cbb9ab6f8b52b04ce972ffd9e9829` |
| v1.5.0-beta.1 | 90,380 | `9a3f84c54249d14a01baaf1de054de6e1b89d1807a7c1bb4174da9f359b301df` |

## Tests

Native C tests compile the firmware's own modules for your computer and run them
(Linux, macOS, or WSL on Windows; needs `gcc`):

```
python3 scripts/run_tb_tests.py --label mytest              # optimized
python3 scripts/run_tb_tests.py --label mytest --sanitize   # with address/undefined-behavior sanitizers
python3 scripts/check_notes.py                              # every pad vs. textbook music theory
```

| Suite | Covers |
| --- | --- |
| i2c, mmio | touch-controller bus: transactions, bus clear, recovery, register binding |
| controls | button decoding, debounce, menus, chord edits, layouts |
| engine | note ownership, USB receipts, re-arming, velocity, aftertouch |
| stage2 | channels, strum timing, hold, arpeggiator (internal and MIDI clock), pressure CCs |
| chords | chord names for every pad, scale, root and edit |
| settings | the flash settings log: presets, user scales, corruption, compaction |
| sysex | the SysEx protocol and USB-MIDI input |
| serial | TRS MIDI out bytes and queue, the sustain pedal (debounce, types, modes) |
| stage3 | chord panel, layouts (chromatic, fourths, drums), strum and arpeggiator extras, clock out |
| stage4 | scale library, mode families, guitar voicing and capo, strum feel |
| live | live notes: chord tones from pads and MIDI in, the engine following the live chord |
| voicing | voice leading, fold, note repeat |
| fixes | regression tests for fixed bugs (voice leading without room, notes owed after a lost session, pressure back to rest, transport continue) |
| padtest | the Pad test page's ranking of pads and its place in the menu |
| sequence | chord progressions: recording rules, the record format, playback on the clock, borrowed chords, the buttons, velocity per half, pedal target |
| seq_checks | progressions at the edges: a DAW's Start, Continue and song position, the editor's pauses, every unit, tempo changes, wraps, recording corner cases, the play-screen line, the settings log; a seeded mix of everything |
| screens | every screen on a fake display: every page, item, value and message, the longest chord names; all drawable, on screen, no overlaps |
| main_loop | the firmware's main loop (`main.c`, unchanged) against stand-ins for USB: the editor's pause, a backup's burst of requests, the cable pulled mid-reply |
| fuzz_engine | long seeded random play with realistic timing and injected USB faults; note ownership checked every pass; timing across clock wraps |
| fuzz_layout | every scale, key, octave and layout with random panels, edits and voice leading; textbook cross-check |
| fuzz_controls | the touch-controller bus under random faults, bus clear with stuck lines, debounce, every menu |
| fuzz_parsers | settings records and the flash log (round trips, corruption), SysEx and MIDI-in streams, MIDI clock jitter |

Python and JavaScript tests (any OS; the web editor test also uses `gcc`, through WSL on
Windows; the pages test a headless Chrome or Edge, and skips without one):

```
cd scripts
python -m unittest test_tboss test_tboss_flash
node ../tests/webapp/test_protocol.mjs
node ../tests/webapp/test_installer.mjs
node ../tests/webapp/test_pages.mjs
```

The website (the project page and this manual as one page with a table of contents, next to the
installer and the editor) is built from `docs/manual.md` by `python scripts/build_site.py --out site`, which is
what GitHub Pages publishes; `python scripts/tboss_editor.py 8000 manual.html` shows it while you edit the
manual (every reload renders it again). `test_tboss` builds it and checks that every link into the manual lands.

`test_tboss_flash` runs the flash tool against a simulated OpenOCD
(`tests/tools/fake_openocd.py`) and checks it can only send the allowed commands.
`test_protocol.mjs` checks the web editor's protocol code against the firmware's C
implementation, and every chord the editor can type against the notes the pads play.
`test_pages.mjs` clicks through the editor and installer pages with a stand-in board.

## Layout of the source

See [architecture.md](architecture.md).
