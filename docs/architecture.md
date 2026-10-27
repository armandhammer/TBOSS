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
2. Two pad-scan positions (`scan.c`; a full 96-pad frame every 8 passes, about 4 ms).
3. Touch buttons every 10 ms (`controls*.c` via `input.c`/`i2c.c`).
4. The MIDI engine (`engine*.c`): the pads just read in (a session starts on a complete
   frame), at most one USB-MIDI packet out.
5. Preset requests and the settings autosave (`settings.c`).
6. MIDI input (`midi_in.c`) and SysEx replies (`sysex.c`).
7. Display and lights (`ui*.c` → `display.c`, `leds.c`): at most one display page and
   one LED frame per pass.

## Modules

The engine, the controls and the screens are each a group of files, one per feature, so a change
to one feature stays in one file. Each group's `*_internal.h` declares what only its own files
share; the rest of the firmware sees `engine.h`, `controls.h` and `ui.h`. The native tests build
the files listed in `tests/tb/sources.json` (the portable core, and the screens), so a new source
file is added to that list once.

| File | Role |
| --- | --- |
| `startup.S`, `reset.c`, `board.c`, `clock.h`, `flash.ld` | Vector table, reset-state restore after the loader, clocks, USB pins, memory layout (image at `0x08040000`) |
| `scan.c`, `scan_hw.c` | Pad multiplexers and ADC: one position per call, validated frames, positions counted for the engine |
| `input.c`, `i2c.c`, `touch_hw.c` | Touch-controller sampling, bounded I2C transactions, bus clear and recovery |
| `controls.c` | Button decoding and debounce, the board's state (`tc_state`), layouts, pad settings, dispatch |
| `controls_menu.c` | Settings: pages, items and values, per-chord edits, resets and their confirmation, navigation |
| `controls_buttons.c` | The play screen's arrows, the pedal, the chord panel, the centre buttons' actions and Shift |
| `controls_steps.c` | The step screen: lane settings, steps on and off, held steps' fields, step record |
| `controls_record.c` | Recording a progression, chord capture, the transport |
| `layout.c` | Scales (built-in and user), the note(s) of each pad, and the Full chord layout (read from the external memory, see below) |
| `chord_name.c` | Chord symbols from the notes of a chord pad |
| `chord_detect.c` | Chord capture (1.4.0): the chord a set of held notes is (`cd_chord`, on `chord_name.c`'s symbol table, so what is captured is what the screen names) and the key a list of chords fits (`cd_key`) |
| `rhythm.c` | Clock positions, step rates as fractions of a MIDI clock, Euclidean patterns (1.4.0; pure, also used by the step sequencer) |
| `lanes.c` | The step sequencer's core (1.5.0; pure): the pattern of lanes of steps, its record format, which step plays at a position |
| `engine.c` | Sessions, note ownership (`tp_own`, `tp_blocked`, `tp_start`), admitting and releasing pads, what goes out next (notes, pressure, sustain, clock), `tc_frame` |
| `engine_pads.c` | Reading the pads: each pad's resting level, touch and release, velocity, arming a session |
| `engine_effects.c` | Strum and its repeat and cutoff, note repeat, Chance, echo, Mono, the Bass layout's retrigger |
| `engine_arp.c` | The arpeggiator |
| `engine_play.c` | What plays by itself: the board's clock, the pedal's note, progressions, the step sequencer's lanes, scale lock |
| `sequence.c` | Chord progressions: recording rules, the record format (played by `engine_play.c`) |
| `midi_in.c`, `sysex.c` | USB-MIDI input (notes, clock, SysEx assembly); the SysEx protocol |
| `settings.c`, `flash_store.c` | Record log in sector 11; the only flash writer |
| `nor.c` | Read-only external memory |
| `serial_midi.c`, `jack_hw.c` | TRS MIDI out (packets to MIDI bytes, a queue the serial interrupt empties) and the pedal input; `controls_buttons.c` has the pedal logic |
| `ui.c` | What the screens share (text lines, the last action's message), redrawing when something shown changes |
| `ui_play.c`, `ui_menu.c`, `ui_steps.c`, `ui_tests.c` | The play screen, Settings, the step screen, the test pages and the editor's preview |
| `ui_names.c` | Every name a screen shows (values, items, pages, actions, notes) |
| `ui_lights.c` | The pads' colors in each scheme, the buttons' lights, the LED test |
| `display.c`, `font.c`, `leds.c` | OLED driver, 5x7 font (`scripts/gen_font.py`), LED drivers |
| `bits.h` | Bit helpers for pitch-class sets (counting, rotating, the lowest notes) |
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

## Rhythm and positions (`rhythm.c`, 1.4.0)

Timing is counted in **clock positions**: MIDI clocks (24 per beat) in 1/256 of a clock
(`RH_SUB`). `tp_clock_pos()` in `engine_play.c` takes the DAW's clock or the board's own one from the
tempo, plus how far the current clock has run, so a position is finer than a clock. A step
length is a fraction `rh_rate {num, den}` of clocks: 1/16 is {6, 1}, 5 per beat {24, 5}, so
tuplets never drift. `rh_step_at(pos, rate)` is the step at a position and `rh_step_start` the
position a step starts (32-bit arithmetic only: 64-bit division would pull in a libgcc object
the linker script refuses).

- The **arpeggiator** keeps an origin position and starts step n at `origin + arp_boundary(n)`
  (swing applied to pairs); a rate or swing change re-origins at the current step, the clock
  going back plays the next step at once, and it never plays a burst of missed steps.
- **Note repeat** strikes at the touch, then on the first step of its rate that is at least
  half a step later, so repeats on several pads and the DAW's grid line up. By row gives each
  row its own rate (`tc_note_repeat_rate`).
- **Euclidean rhythms** (`rh_euclid`, Bresenham style; `rh_euclid_at` numbers the menu's 120)
  are masks of up to 16 steps; `tc_rhythm_mask` turns a stored rhythm value and rotation into
  one.

## Pivot chords and chord capture (1.4.0)

`tc_pivots` (`layout.c`, next to `tc_hints`) returns the chord pads whose pitch classes
(`tc_pad_classes`, with their edits) all lie in the target's major or natural minor scale,
those on its 2nd or 4th degree as strong; `ui_lights.c` lights them with the hints' rendering (guide
lights first, then pivots, then hints) and `ui_play.c` names the chord's role on the play screen. The
target's root button arrives (`dispatch` in `controls.c`).

Capture keeps up to 12 chords in `controls_record.c` (`tc_capture_chord`): chord pads add theirs in
`tc_frame`, and `capture_midi` in `engine.c` takes MIDI-in chords from `tm.notes` (the
fullest set held, at the first release). Saving runs `cd_key`, stores the set relative to
that key through `tc_chord_set_put`, asks `settings.c` to write the chord set record
(`TC_REQUEST_SET_SAVE`) and moves the board to that key and set.

## The step sequencer (`lanes.c`, 1.5.0)

Built so later features are small additions, not rewrites:

- **Pure core.** `lanes.c` holds the pattern (`ln_pattern`: 4 lanes, a seed, a version), its
  record format and `ln_locate`, with no engine or UI code, so all of it runs in the native tests
  and the fuzzers (`lanes`, `fuzz_lanes`).
- **Steps come from the position.** A lane's step is `rh_step_at(pos, rate) % length`, as a
  progression's chord comes from `seq_locate`: lanes of different lengths and rates are polymeter
  by themselves, and a DAW's Start, Continue and Song Position Pointer land on the right step.
  With a DAW the position is its song position (`tm.position`, plus the fraction of the current
  clock); otherwise the clock position since the pattern started, kept across sessions as the
  progression's is.
- **Zero means default** in every field of a step (64 bits: on, value, velocity, edits, scale,
  gate, probability, ratchet, condition, nudge, tie, spare) and a lane (kind, channel, length,
  rate, direction, swing, mute, transpose, Euclid fill). 1.5.0 plays on, value and velocity and a
  lane's kind, channel, length, rate and mute; 1.7.0 adds gate, probability, ratchet, condition,
  tie, the direction and Euclid fill; nudge, swing and transpose are still stored as they are, so
  later features need no format change.
- **Randomness is a hash** (`ln_hash(seed, lane, loop, step)`), not a stored state, so
  playback repeats and can be located (`fuzz_lanes` checks that the same seed plays the same
  packets): a step's probability (1.7.0) and the random direction's order each time round
  (`ln_order`, a shuffle keyed by the loop).
- **1.7.0 in the engine.** At each new step count the lane takes `ln_order` (direction, loop) and
  `ln_sounds` (condition, probability). A step's n ratchet strikes span `[start + len*j/n,
  start + len*(j+1)/n)`, each sounding its gate's share; joining a step late skips the strikes
  already over. A tied step's last strike holds (`lane_tied`) over steps that don't play until
  one does, or the lane stops, is muted, or All notes off.
- **One playback path.** Each lane is an engine source (`TP_LANE0..3`, after the scale lock's).
  `tp_lanes()` in `engine_play.c` gives a step that is on its notes through `tc_step_notes`: a chord pad
  through `tc_pad_with` (the panel and voice leading apply), a melody step as the right half's
  Scale layout position, a drum step its note; then `tp_blocked`, `tp_start`, `tp_own` and `tp_disown` as for every
  source, so strum, channels and safe Note Offs come with it. The gate is half the step
  (`ln_gate_end`); a step that comes past its gate (an owed Note Off kept it waiting) is skipped.
- **The step screen** is a UI state (`TC_UI_STEPS`): `tc_frame` hands every pad touch and lift
  to `tc_step_pad` (`controls_steps.c`) instead of the usual chord handling, `admit_sources` keeps the
  right half silent and plays a melody or drum lane's values on the left half, and `ui_steps.c` draws
  the screen and the step lights.
- **Storage.** Record kind `ST_LANE` (7): slot = pattern × 16 + lane × 4 + chunk, 16 steps per
  record; chunk 0 also holds the lane's settings, lane 0 chunk 0 the seed. Edits mark their chunk
  in `ln_dirty`; `st_service` writes the changed chunks 3 s after the last edit, and never a chunk
  of defaults that was never stored. Backups, restores and the installer's *Save settings*
  include the pattern through the generic record commands.
- **Flash budget.** Sector 11 holds 744 records and is compacted at boot once half full. The
  current records are at most 12 scales, 8 chord sets, the favorites, 8 progressions, 8 presets,
  the live settings and 16 pattern records: 54, so every session keeps over 300 saves. Eight
  patterns (planned) would be 128 pattern records, 166 in all, still under half the sector; more
  than that would need another sector, which changes the safety rules.

## Chords on the right (1.5.0)

Pads 0-47 are chord pads and 48-95 melody pads everywhere in the firmware. *Chords on* Right
(`tc_state.chords_right`) trades the halves only where pads meet the hardware: `tp_frame` and
`tp_arm` read pad k's value from the other half (`tc_physical`), and `ui_lights.c` sends pad k's light
there. Nothing else knows; the pad test and I/O test name pads by where they are on the board.

## Each pad's resting level (1.5.0)

`tp_engine.rest[96]`, indexed by physical pad (the sensor's level stays put when *Chords on*
trades the halves), replaces the board-wide `pad_rest` (the 12th highest pad), which left a few
high-resting pads above Release on uneven boards: played once, they stuck. `tc_arm` measures each
pad from the starting frame (a pad at 2000 or more is held: it starts at the board's median and is
inhibited until let go). `tp_key_config(e, k)` = `tc_pad_raise(settings, rest)`: since 1.7.0 every
threshold is the setting above the pad's rest (rest capped at 2000, Full at most 4095); in 1.5-1.6
only Release was raised (to rest + 130) where a pad rested high, so those pads set off with less force
than the rest. `tp_frame` uses it for press, release, pressure and velocity, and `follow_rest` moves
the level while the pad rests (not sensed, admitted or inhibited, below its Trigger): down 1/32 of
the gap every 15 ms (`TC_PAD_REST_FALL`, `TC_PAD_REST_MS`, per scan position; a quarter before 1.7.0,
which let a tenth of a second of low readings drop a rest 700 and play the pad at its usual level),
up 1/128 (`TC_PAD_REST_RISE`), so a
board that flexes is followed within seconds and a press is never taken for rest. `rest` holds the
reading itself, `rested` whether it was measured. The Pad test shows each pad's rest and the range.

Two more rules in `read_pad` (1.7.0, `engine_pads.c`), from a warped board's readings:
- **Bleed:** a touch is ignored while a neighbor on the board (same half, a row or column away) reads
  past its own Trigger and has risen more than twice as far above its rest (`bleed`).
- **Unstick** (Settings > Pads, on by default): a touch also ends once its reading has stayed within
  `TC_PAD_SETTLE_BAND` (30) for `TC_PAD_SETTLE_MS` (300 ms) below Trigger or `TC_PAD_STILL_MS` (1 s)
  at or above it, never from `TC_PAD_REST_CAP` (2000) up (a hard press can read a flat 4095):
  `settling` keeps `tp_key.settle_ref`/`settle_at`, restarted by any larger move, and `unstuck`
  checks the time. That reading becomes the pad's rest at once. A stuck pad holds within about 5 and
  a finger wanders 50 to 75 or more (measured on a warped board), so a pad left reading high lets go
  whatever its rest (a sounding pad's rest is frozen), while held notes keep sounding.

Settings word 32 still holds levels, as 0.10-1.6 do (each setting + `TC_PAD_*_STORED`), so either
version reads the other's record with about the same feel; Unstick off is word 19 bit 8, which the
web installer clears for older versions.

## Faster pads (1.5.0)

Before 1.5.0 the engine took whole 96-pad frames (one position a pass; ADC at 12 MHz, 480-cycle
sampling: a frame about every 15 ms), so a note sounded 0-30 ms after the touch, and a lift shorter
than a frame could fall between two readings of the pad (fast repeated hits merged). Now:

- `scan_hw.c`: ADC at 24 MHz (PCLK2/2), 112-cycle sampling, still three conversions averaged
  (fallbacks if a board's readings move: 144 or 480 cycles, one constant). `main.c` reads two
  positions a pass: a frame in about 4 ms.
- `ts_scan.reads` counts positions read and `last` is the latest; `valid` stays true while the
  next frame is read (false only after a rejected read, until a complete frame). `tp_frame` takes
  the 6 pads of each position read since its last call, every pass (whole frames, as the tests
  give them, all 96). A session still arms on a complete frame.
- Velocity: read every few ms, a touch's first reading past Trigger is early in the press. A touch
  first read within `TC_PAD_VELOCITY_MS` (8 ms) of the pad's last reading below Trigger waits one
  more reading and plays the higher pressure; one reading Full, or its half at Fixed 100, plays at
  once. With readings 15 ms apart there is no wait, so the old timing behaves as before.
- Time, not frames: resting levels follow every `TC_PAD_REST_MS`; staleness (100 ms) and pressure
  pacing (`pressure_ms`) were already in milliseconds.
- `test_fastpads` runs the real scanner on a fake ADC with simulated fingers in `main.c`'s loop
  order, old timing beside new: frames 15.8 -> 3.75 ms; Note On after a touch average 16.3 -> 6.3
  ms, worst 28.6 -> 8.3 ms (Fixed 100: 2.5 / 4.5 ms); 20 hits with 10 ms lifts: 15 notes before,
  20 now; a firm strike plays the same velocity at every point of the scan. `fuzz_engine` runs half
  its rounds this way.
- USB still sends one packet per transfer (about 1 ms a note, so a 4-note chord spreads over about
  4 ms); one transfer per chord was left for later, if needed.

## The MIDI engine

The engine turns pad state into MIDI without ever losing a Note Off:

- **Sources** are the 96 pads plus extra sources: the arpeggiator, the pedal's note, the
  progression, 16 for the scale lock and (1.5.0) one per step-sequencer lane. A pressed pad
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
favorite scales (0.7.0), progressions 0-7 (0.11.0), chord sets 0-7 (1.0.0), the step pattern's
chunks 0-15 (1.5.0). The first 20 payload words are the 0.5 layout; 0.6/0.7/0.8 settings
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
   (`tests/tb/`, registered in `scripts/run_tb_tests.py`). Put it in the file of the feature it
   belongs to (see Modules); a new source file goes in `tests/tb/sources.json` too.
2. New settings: add the field in `controls.h`, its default in `tc_init`, a menu item
   if it is user-facing (`TC_MENU_*`, its value count in `tc_menu_values` and its page in
   `tc_group_items` in `controls_menu.c`, its label in `ui_names.c` and its value text in
   `ui_menu.c`), and its encoding/validation in
   `settings.c` (payload words
   5..16 hold the chord edits; word 32 holds the pad thresholds, word 33 the progression slot; words 18..19 and 34..38 are free and must stay 0; word 39 marks the format). A new setting should be written as 0 while it has its default, so older firmware keeps reading the record.
3. Run all tests and `python scripts/tboss_build.py`; the safety check must pass.
4. Update the [manual](manual.md).
