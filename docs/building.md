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

## Tests

Native C tests compile the firmware's own modules for your computer and run them
(Linux, macOS, or WSL on Windows; needs `gcc`):

```
python3 scripts/run_tb_tests.py --label mytest              # optimised
python3 scripts/run_tb_tests.py --label mytest --sanitize   # with address/undefined-behaviour sanitizers
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

Python and JavaScript tests (any OS; the web editor test also uses `gcc`, through WSL on
Windows):

```
cd scripts
python -m unittest test_tboss test_tboss_flash
node ../tests/webapp/test_protocol.mjs
```

`test_tboss_flash` runs the flash tool against a simulated OpenOCD
(`tests/tools/fake_openocd.py`) and checks it can only send the allowed commands.
`test_protocol.mjs` checks the web editor's protocol code against the firmware's C
implementation.

## Layout of the source

See [architecture.md](architecture.md).
