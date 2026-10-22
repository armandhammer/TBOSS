# TBOSS firmware architecture

A guide for anyone changing the firmware. Hardware details are in
[hardware.md](hardware.md), the SysEx protocol in [tb-sysex.md](tb-sysex.md).

## Principles

- **Bare metal, one loop, no heap.** C11 on the STM32F407 at 48 MHz, CMSIS register
  access, TinyUSB for USB. Everything runs in one foreground loop; interrupts only
  queue USB events and count milliseconds.
- **Every wait is bounded.** No loop can hang forever on hardware; a missing display,
  LED chain, touch controller or memory chip disables only that part.
- **Safety rules are checked by the build.** `scripts/firmware_safety.py` rejects any
  build that touches the SWD pins (PA13/PA14), option bytes, the debug MCU block,
  low-power Stop/Standby, writes flash anywhere but `flash_store.c` (sector 11), or
  sends the external memory anything but read commands.
- **Tested off the board.** All musical and protocol logic is plain C compiled natively
  by `scripts/run_tb_tests.py`; hardware drivers are thin.

## Main loop (`main.c`)

Each pass:
1. USB housekeeping (`tud_task`).
2. One pad-scan position (`scan.c`; a full 96-pad frame every 16 passes).
3. Touch buttons every 10 ms (`controls.c` via `input.c`/`i2c.c`).
4. The MIDI engine (`engine.c`): new pad frame in, at most one USB-MIDI packet out.
5. Preset requests and the settings autosave (`settings.c`).
6. MIDI input (`midi_in.c`) and SysEx replies (`sysex.c`).
7. Display and lights (`ui.c` → `display.c`, `leds.c`): at most one display page and
   one LED frame per pass.

## Modules

| File | Role |
| --- | --- |
| `startup.S`, `reset.c`, `board.c`, `clock.h`, `flash.ld` | Vector table, reset-state restore after the loader, clocks, USB pins, memory layout (image at `0x08040000`) |
| `scan.c`, `scan_hw.c` | Pad multiplexers and ADC: one position per call, validated frames |
| `input.c`, `i2c.c`, `touch_hw.c` | Touch-controller sampling, bounded I2C transactions, bus clear and recovery |
| `controls.c` | Button decoding, debounce, menu, shortcuts, chord edits, octaves |
| `layout.c` | Scales (built-in and user) and the note(s) of each pad |
| `chord_name.c` | Chord symbols from the notes of a chord pad |
| `engine.c` | Pads, arpeggiator and progressions → MIDI: note ownership, strum, hold, pressure, sustain (CC64) |
| `sequence.c` | Chord progressions: recording rules, the record format (played by `engine.c`) |
| `midi_in.c`, `sysex.c` | USB-MIDI input (notes, clock, SysEx assembly); the SysEx protocol |
| `settings.c`, `flash_store.c` | Record log in sector 11; the only flash writer |
| `nor.c` | Read-only external memory |
| `serial_midi.c`, `jack_hw.c` | TRS MIDI out (packets to MIDI bytes, a queue the serial interrupt empties) and the pedal input; `controls.c` has the pedal logic |
| `ui.c`, `display.c`, `font.c`, `leds.c` | Screens, OLED driver, 5x7 font (`scripts/gen_font.py`), LED drivers |
| `usb_descriptors.c`, `tusb_config.h` | USB MIDI device identity |

## The MIDI engine

The engine turns pad state into MIDI without ever losing a Note Off:

- **Sources** are the 96 pads plus one extra source for the arpeggiator. A pressed pad
  is *admitted*: it captures its notes (from `layout.c`, with the chord edits), its
  MIDI channel and its velocity at that moment, so later changes of key or scale
  never change what a held pad releases.
- **Voices** are (channel, pitch) pairs with a reference count. A voice's Note On goes
  out once while referenced; its Note Off goes out once nothing references it any more.
  Two pads sharing a pitch share one voice.
- **One packet in flight.** Each packet gets a token; the next is sent only after
  TinyUSB reports that transfer complete (via the `tb_midi_tx_result` hook). Every
  Note On reserves its Note Off before it is sent; on USB loss or a stall the session
  stops and a fresh one starts 500 ms later once all pads are released.
- **Strum** gives each captured note a start delay; **hold** keeps a released chord
  pad admitted (*latched*) until another chord pad is touched; the **arpeggiator**
  makes chord pads silent inputs and moves its own single-note source through their
  notes on a microsecond schedule or on incoming MIDI clock.
- **Pressure** is set per half: poly aftertouch for every note a pad has started that no
  other held pad shares (each note tracked and rate-limited on its own), or one
  channel-pressure or controller value per half (merged when both halves use the same
  channel and setting).

## SysEx replies: transfer mode

Replies can be long (a 2 KiB memory chunk is ~2.4 KB of SysEx). Rather than interleave
them with notes, `main.c` first drains the engine (every owed Note Off sent and
confirmed), then streams the reply through TinyUSB's FIFO, and lets the engine re-arm
one second after the last request. Messages without a reply (scale preview) never
interrupt playing.

## Settings

`settings.c` appends 44-word records (magic `TBS3`, sequence, kind/slot, 40 payload
words, CRC-32) to sector 11. Kinds: live settings, presets 0-7, user scales 0-11,
favorite scales (0.7.0), progressions 0-7 (0.11.0), chord sets 0-7 (1.0.0). The first 20 payload words are the 0.5 layout; 0.6/0.7/0.8 settings
are in words 20..30, valid
when word 39 holds the 0.6.0 marker (a 20-word record from older firmware still loads
and keeps the current values of the new settings). The newest
valid record of each kind/slot wins; damaged or invalid ones are skipped. Runtime saves
only program empty space; the sector is erased only at boot (before USB starts), when
it is half full (so every session has room for at least 372 saves), after copying the current records out.

## Diagnostics over SWD

A block at `0x20018000` (`diagnostic.h`, `scan.h`, `controls.h`) holds counters and
state (loop count, USB events, engine state, scan frames, touch-controller status) that
a debugger can read without stopping the board.

## Adding a feature

1. Keep musical logic in plain C modules with no register access, and add a native test
   (`tests/tb/`, registered in `scripts/run_tb_tests.py`).
2. New settings: add the field in `controls.h`, its default in `tc_init`, a menu item
   if it is user-facing (`TC_MENU_*`, its value count in `tc_menu_values`, its page in
   `tc_group_items`, its label and value text in `ui.c`), and its encoding/validation in
   `settings.c` (payload words
   5..16 hold the chord edits; word 32 holds the pad thresholds, word 33 the progression slot; words 18..19 and 34..38 are free and must stay 0; word 39 marks the format). A new setting should be written as 0 while it has its default, so older firmware keeps reading the record.
3. Run all tests and `python scripts/tboss_build.py`; the safety check must pass.
4. Update the [manual](manual.md).
