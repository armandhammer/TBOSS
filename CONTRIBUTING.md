# Contributing to TBOSS

Thanks for helping. Bug reports, hardware findings, documentation fixes and code are
all welcome.

## Issues
- **Bug report:** what you did, what you expected, what happened, the TBOSS version
  (the release you installed; the web editor shows the firmware stage), and your OS/DAW
  if MIDI is involved.
- **Hardware finding:** measurements, photos or analysis of the board. Say how you
  found it and how sure you are.
- **Feature request:** what you want to play or do, and how you imagine using it.

## Pull requests
1. Keep the safety rules (they protect people's boards):
   - never touch PA13/PA14 (SWD), option bytes, read/write protection, low-power
     Stop/Standby or the debug MCU block;
   - write internal flash only through `src/tb/flash_store.c` (sector 11);
   - never write the touch controllers' configuration or the external memory.
   `python scripts/tboss_build.py` runs `scripts/firmware_safety.py`; it must pass.
2. Add or update tests (`tests/tb/`, registered in `scripts/run_tb_tests.py`) and run
   the suites in [docs/building.md](docs/building.md).
3. Match the surrounding style: C11, fixed-width integer types, no heap, every loop
   bounded, warnings as errors.
4. Update the [manual](docs/manual.md) when behaviour changes.
5. License: contributions are accepted under GPL-3.0-or-later.

## Please don't submit
- Original/vendor firmware, update files, flash dumps (including your backup), or the
  contents of the external memory chip.
- Disassembly listings or code copied from proprietary firmware. Describe hardware
  facts in your own words instead (pins, registers, timings, protocols).
