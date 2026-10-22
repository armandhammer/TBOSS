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
| `layout.c` | Scales (built-in and user), the note(s) of each pad, and the Full chord layout (read from the external memory, see below) |
| `chord_name.c` | Chord symbols from the notes of a chord pad |
| `engine.c` | Pads, arpeggiator and progressions → MIDI: note ownership, strum, hold, pressure, sustain (CC64) |
| `sequence.c` | Chord progressions: recording rules, the record format (played by `engine.c`) |
| `midi_in.c`, `sysex.c` | USB-MIDI input (notes, clock, SysEx assembly); the SysEx protocol |
| `settings.c`, `flash_store.c` | Record log in sector 11; the only flash writer |
| `nor.c` | Read-only external memory |
| `serial_midi.c`, `jack_hw.c` | TRS MIDI out (packets to MIDI bytes, a queue the serial interrupt empties) and the pedal input; `controls.c` has the pedal logic |
| `ui.c`, `display.c`, `font.c`, `leds.c` | Screens, OLED driver, 5x7 font (`scripts/gen_font.py`), LED drivers |
| `usb_descriptors.c`, `tusb_config.h` | USB MIDI device identity |

## The chord grid (`layout.c`)

`tp_layout.chords == TC_CHORDS_GRID` (10) plays the scale's chords with the row's chord type
added to the pad's edit (`grid_shape`): from the top 7th and 9th, sus (sus4, or sus2 where the
4th above isn't perfect), 7th, nothing, all in root position (`home_step` 0). Everything else
(fold, the panel, voice leading, hints by scale step) is the scale's chords' own code. The
pads' stored edits are left out on the grid (`own_edit` in `controls.c`); held chord-shape
buttons still add to it.

## The Full chord layout (`layout.c`)

The original firmware keeps its Full layout on the external memory chip, and TBOSS reads it
there (read only, through the function `main.c` hands to `tc_full_reader`). From `0x70000`:
70 scales, each as 12 records of 2,560 bytes, one per key (A, G♯, B, A♯, C, D, C♯, E, D♯, F,
G, F♯; `full_key_slot`). A record is 80 entries of 32 bytes: 16 ASCII digits (the chord's MIDI
notes in pairs, `00` ends them, the last pair is the scale step 1..7 whose run it belongs to),
then a 16-byte name. Entries 0..47 are the chord pads, top to bottom down each column; entry
48 is the right half's first note, the key's root (TBOSS checks it before trusting a record).
`full_scales` lists each record's scale and the 7-note mode whose steps number its runs.

Some entries have a wrong note in some keys. Each pad's entry is compared with the same entry
in other keys, far apart first: a chord with a note outside the mode is a slip and has no say;
the key's own entry agreeing with the key a tritone away settles it, otherwise the first chord
three keys agree on wins. The pad keeps the original voicing when its own entry is that chord,
else the chord is voiced as a built one (every tone in G♯3–G4, no half step at the bottom).
About 2 KB is read per scale and key, a few milliseconds; `main.c` reads it when the layout
or the progression changes (`tc_full_prepare`), never while a pad or step plays, and four
layouts stay cached. A scale with no record, or a chip that doesn't answer, gets a layout built
from `full_types` (common chord types, the original's most used first).

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
