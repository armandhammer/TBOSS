# Roadmap

Ideas and agreed next steps, newest decisions first. Nothing here is built
unless marked. (Written so it can also go into the public TBOSS repository.)

## Later (1.5): after the 1.0 beta

- [ ] Phrase recorder: melody notes over a progression, locked to its loop and MIDI clock, overdub, clear.
- [ ] Chained progressions (P1 > P1 > P2) and per-step probability (skip, or swap to a borrowed chord).
- [ ] Chord sets captured from the pads or MIDI in (1.0: typed in the web editor); sets stored per preset.
- [ ] Held chord shapes (Add 7th etc. on a center button) change a running arpeggio at once
      (1.0: they apply to the next chord pad touched; found in board testing, 2026-10-21).
- [ ] Full chord layout, the original TheoryBoard's default (requested in
      [discussion #8](https://github.com/armandhammer/TBOSS/discussions/8)): the scale's 7 chords
      in order, each with a run of variations that stay in the scale (C major: C, Cadd2, C6,
      C6(add9), Cmaj7, Cmaj7(9), Csus2, Csus4), poured column by column from the top across the
      48 chord pads (runs in C major: 8, 8, 5, 9, 8, 7, 3). Built from a rule so it works in every
      scale. Still needed: the full variation list for chords like Em, F, Am and B° in C major,
      and their order within a run.

## 1.0.0 beta (built as 0.12.0, released 2026-10-21): chord sets, hints, scale lock, center buttons (approved 2026-10-19)

From a user's request (custom chord sets) and the ideas below (customizable center buttons
and Shift, theory hints, MIDI in scale lock).

- [x] Chord sets: 8 sets of 12 chords (up to 6 notes each, any root, a column may be empty)
      typed in the web editor; Settings → Chords → Chord set; the root buttons transpose a
      set, the rows play its inversions, the panel, voice leading and fold apply; a
      progression keeps the set it was recorded with.
- [x] Theory hints: after a chord, the usual next chords pulse and glow (by scale step in
      seven-note scales, by root movement otherwise).
- [x] Scale lock: notes arriving on one MIDI channel play on the melody channel, moved to the
      nearest note of the key and scale.
- [x] Center buttons: each of the 14 does any of 39 actions (its printed function by
      default; toggles (sustain too), chord shapes while held, tap tempo, presets, scales,
      play/record, next chord set, all notes off); a Shift button (held or latched) with
      Shift + root = progression, Shift + mode = preset, Shift + chord pad = pick for edits.
      Settings → Buttons and the web editor.
- [ ] Checked on a board.

## 0.11.0: chord progressions (approved 2026-10-19)

From user requests: songs whose chords are not all in one scale, velocity set per half,
and the sustain pedal on one half only.

- [x] Progressions: record by tapping chord pads (1 beat to 2 bars per tap, the same chord
      again lengthens it, ◀ takes back), 8 slots; play with ▲ (Arrows: Sequencer), the
      pedal, Transport or a DAW's Start, on the MIDI clock.
- [x] A step keeps its pad, edits and velocity; chords recorded in another mode keep that
      scale (borrowed chords). Root buttons transpose a playing progression; mode buttons
      re-color it.
- [x] Everything that shapes a held chord applies (panel, strum, voice leading, fold,
      arpeggiator). Guide lights: the chord now, the next one, its notes on the right half.
- [x] Web editor: type a progression (`Am D Dm Am C G E7 Am`) in a key; the editor places
      each chord on the pads, borrowing from the closest scale where needed.
- [x] Chord vel and Melody vel (velocity curve per half); pedal Target (both, chords or
      melody).
- [x] Validation before release: a DAW's Continue and Song Position Pointer land on the right
      chord, the editor's pauses keep the place, recording nothing keeps the slot, the play
      screen's line fits; restores into older TBOSS leave out what they don't have.
- [x] Checked on a board and released (2026-10-19).
- [x] Decided: strum *Repeat* and *Cutoff* stay off for progression chords; a
      progression plays its recorded velocities unless *Chord vel* is Fixed 100 (then 100);
      a typed or Fixed 100 plays 100 after a restart (was 101).

Later sequencer ideas (from the Deluge and OXI One comparison) wait for approval: a note
phrase recorder, chained progressions, per-step probability.

## 0.10.0: pad sensitivity (approved 2026-10-18)

From Pad test readings on a stock board (rest ~290–470, lightest touch ~600, neighbors
under ~500, firm press ~3500) and the same board with a sensitivity modification.

- [x] Settings → Pads: Trigger (500–3000, default 1200), Release (300–2500, default 600),
      Full (1500–4000, default 3500) in steps of 100; Pad test moves to this page.
- [x] Release kept at least 130 above the resting level measured when playing starts, so
      boards whose pads rest high still release.
- [x] Released as the default (2026-10-18).

Later, if needed: per-pad calibration (each pad's own resting level), and ignoring a pad
that only reacts to a much harder-pressed neighbor (for modified boards).

## 0.9.1: pad test and reliability fixes (2026-10-18)

From a code review with long randomized stress tests of the engine, layouts, controls and
settings (now part of the test suites).

- [x] Settings → About → Pad test: raw pad readings, the peak of a touch, the velocity a
      note got, and how much a press spreads to neighboring pads.
- [x] Voice leading never stops the board: a pad with no inversion in range plays its
      row's chord.
- [x] Notes held when a session is lost (computer unplugged or asleep) end in the next
      session, so a synth on the TRS jack doesn't drone.
- [x] Pressure, controllers and pitch bend always return to rest (after a slow release,
      and when the web editor interrupts playing).
- [x] A running transport pauses and continues (Stop, Continue) instead of restarting
      after an interruption.
- [x] Buttons keep working after 49.7 days of continuous power.
- [x] Next: an adjustable pad sensitivity: 0.10.0.

## 0.9.0: voice leading, note repeat, fold (approved 2026-10-17)

Defaults stay those of a regular TheoryBoard: all three are off until turned on.

- [x] Auto voice leading (Settings → Chords → Voice lead): each chord takes the inversion
      that moves least from the chord before, within two inversions of its pad.
- [x] Note repeat for the right half (Settings → Play → Note repeat), MPC-style, velocity
      from pressure.
- [x] Fold (Settings → Chords → Fold): every chord inside one octave, G♯ to G, as the
      original is described; the campaign's "turn off octave folding" is TBOSS's default.
- [x] Manual: every feature with a walk-through, why to use it and when not to.
- [x] Checked on a board and released as the default (2026-10-17).

Still under discussion: bass mode (each note on two pads for fast alternating repeats).

## 0.8.0: the original's promised Live Note Update (approved 2026-10-17)

The 2020 campaign's Live Note Update was a pad area: while a chord is held on the left, the
right half offers that chord's notes (press of the time: 28 scale pads, 20 chord pads). TBOSS
had used the name for re-striking held pads (now *Restrike*).

- [x] Live notes layout: columns 1–7 the scale, columns 8–12 the live chord's notes, rows
      an octave apart; the chord stays after release.
- [x] Live notes on their own MIDI channel (Settings → Layout → Live ch).
- [x] Live notes follow chords arriving over MIDI in (Settings → Layout → Live in).
- [x] Silent chord pads that only set the live chord (Settings → Chords → Sound).
- [x] Checked on a board (2026-10-17; following MIDI in and the own channel not yet tried).

Chords on both sides: not for now.

## 0.7.0: Stage 4, the scale library and strum feel (approved 2026-10-17)

- [x] Scale families on M1–M7: the modes of major, melodic minor, harmonic minor and
      harmonic major (Settings → Play → Family).
- [x] A built-in library: 86 scales in all (61 new), one list for firmware and editor
      (scripts/gen_scales.py).
- [x] Up to 128 favorite scales on the board; ▲/▼ step through them.
- [x] Editor: browse by family or bright to dark, find scales by notes (clicked or played),
      scales one note away, favorites.
- [x] Strum feel: random timing and velocity (user request).
- [x] Capo (user request): the guitar voicing became six real strings in standard tuning,
      with a capo and positions up the neck.
- [x] Checked on a board and released as the default (2026-10-17).

Stage 5 (not approved yet): sequencers/phrase recorder, sets and user colors.

## 0.6.0: Stage 3, the original TheoryBoard's features (approved 2026-10-17)

- [x] Center buttons do what the printed panel says: voicing I/II/III/guitar, spread
      I/II/III, octave V with tap and hold-to-stack; per-pad edits moved to Settings → Chords.
- [x] Drum mode: the right half as scale / bass / General MIDI drum grids, own channels.
- [x] 13 more built-in scales; user slots raised to 12.
- [x] More layouts: chromatic and fourths.
- [x] Strum: repeat while held, cutoff, tempo-based strum times, strum on/off per pad.
- [x] Arpeggiator: 8-step rhythm, swing, sync start, a Chords pattern (arp-to-strum), latch.
- [ ] A second arpeggiator (optional in the proposal; deferred: it needs a second
      source pool and its own menu page, better done with the Stage 5 sequencer work).
- [x] Expression: pitch bend from pressure, Light/Firm velocity curves, live update of
      held pads.
- [x] Pedal: sostenuto/soft/portamento, play/stop, a note on the drum channel.
- [x] MIDI clock out at the board's tempo; transport Start/Stop.
- [x] Violet and pink pad colors moved further apart (A, A#, B).
- [x] Checked on a board: voicing and spread right; the octave V was out of order (fixed in 0.6.1).

## 0.6.1: panel fixes from the board check (2026-10-17)

- [x] Octave V buttons in their real order (−3 … +3).
- [x] 0.6.2: octave buttons toggle (tap on / tap off, lit octaves stack), as the original
      firmware does; quick taps had stacked by accident with hold-to-stack.
- [x] Panel lights: full while a choice is on, dim otherwise (30 %, 15 % from 0.6.2) (channels from the original
      firmware's light tables).
- [x] ▲/▼ on the play screen: Settings → Play → *Arrows* (Scale by default, or melody
      octave, preset, tempo); melody octave also as Settings → Play → *Melody oct*.
- [x] 0.6.2 checked on a board (panel, its lights, arrows) and released as the default (2026-10-17).
- [x] 0.6.3 (navigation buttons at 70 %) checked on a board and released as the default (2026-10-17).

## 0.5.x: lights, a robust start, aftertouch per half (2026-10-17)

- [x] A pad that reads pressed when the board starts no longer silences the whole board
      (tester report: no pad reaction, no MIDI); it stays silent until released once.
- [x] Rainbow pad colors by note (default), touched pads white with pressure; the 0.4
      look stays as Settings → Lights → Colors → Root+5th.
- [x] Web editor preview: the whole board shows the scale; the display names the slot and notes.
- [x] I/O test page shows pads that read as held.
- [x] Aftertouch per half (Chord AT, Melody AT), and poly aftertouch from chord pads on each
      of their notes that no other held pad shares.
- [x] Editor keys in the board's colors.
- [x] 0.5.1 checked on a board and released as the default (2026-10-17).

## 0.4.0: the jacks (2026-10-16)

- [x] TRS MIDI out (Type A): everything sent over USB also goes to the jack; plays the jack
      alone when no computer is attached. Settings → MIDI → TRS out.
- [x] Sustain pedal: CC64 per side, or the board's hold; auto/normal/inverted pedal types.
- [x] I/O test page (Settings → About) showing the pedal input live and sending a test note.
- [x] 0.4.0 installed and checked on a board without the jacks (USB playing, menu, settings,
      editor, no-computer start); released as the default 2026-10-16.
- [x] TRS MIDI out (PB6 USART1 TX, Type A) confirmed on a board: messages read correctly
      by a MIDI interface (2026-10-17).
- [ ] Sustain pedal input (PD13) on a board.

## Alpha launch: 0.3.0 (agreed 2026-10-16)

Launch with a basic feature set; users test and report what breaks.

- [x] Browser installer through the board's own update mode (WebHID); verified on hardware.
- [x] Settings, presets and user scales carried across an install (saved over MIDI first,
      restored after the restart).
- [x] Hosted installer and editor on GitHub Pages, with the newest release's firmware.
- [x] Firmware version on the start-up screen, in Settings → About, reported to the editor
      and the installer, and in the USB device release number.
- [x] Settings menu as a tree driven by the five buttons below the display: left back,
      up/down move, center enter, right deeper; only left leaves (hold left + center: home).
- [x] Manual: browser install first, real button names, known issues; SWD guide kept as advanced.
- [x] Alpha disclaimer; Discussions for feedback.
- [x] Installed on hardware with the browser installer, settings kept (2026-10-16); released as v0.3.0.
- [x] Going back with the manufacturer's update utility after TBOSS: confirmed (the utility
      hangs at "finalizing" although the board has finished; close it and replug).
- [ ] Own USB product ID 1209:B333 from pid.codes (requested 2026-10-16, pidcodes PR #1319),
      replacing the test ID `cafe:4069`.

Not in TBOSS yet (candidates for the next stages): sequencer/phrase recorder (chord
progressions came in 0.11.0),
a second arpeggiator, an arpeggiator on/off button, per-pad sensitivity, customizable
center buttons (below). Drum mode and 12 user slots came in 0.6.0.

## TBOSS public release (agreed 2026-10-16, after Stage 2 passes on hardware)

Public GitHub repository **TBOSS** under `armandhammer`, GPL-3.0-or-later.
Latest known-working firmware, web editor, tools and documentation; tagged releases.

- [x] Source comments neutral; headers GPL-3.0-or-later; standalone build
      `scripts/tboss_build.py` reproduces the Stage 2 image byte for byte.
- [x] Public flash tool (backup / install / restore over SWD) with simulated-OpenOCD tests.
- [x] Documentation: user guide (every feature, step by step), hardware reference
      (MCU, flash map, loader, pads, touch controllers, LEDs, OLED, NOR, USB, SWD),
      flashing, building, firmware architecture, SysEx protocol.
- [x] Issue templates (bug report, feature request, hardware finding), CONTRIBUTING.
- [ ] Own USB product ID (free open-source PID from pid.codes) instead of the test ID `cafe:4069` (see Alpha launch).

## Install from Chrome, no debugger (done in 0.3.0)

The board's resident loader has a built-in USB update mode (hold the chord half's top-left
and bottom-left pads while plugging in). The TBOSS installer page talks to it over WebHID
and installs to flash sectors 6-11 only; the protocol is in
[installer-protocol.md](installer-protocol.md). Verified on hardware. The SWD tools stay as
the expert and recovery path (they are also the only way to back up a board's whole
internal flash).

## Customizable center buttons and a Shift layer (idea, 2026-10-16; built in 1.0.0)

Let users choose, in the web editor, what each of the 14 center buttons does, with
the current functions as the default. One button (or a user-slot button such as U3)
can be made **Shift**: held, it gives every other button and pad a second meaning;
double-tapped, it could latch.

Possible actions to choose from:
- Chord shaping: lower root/3rd/5th; add 6th/7th/9th/11th/13th; sus2/sus4; force
  major/minor/dim/7th while held (quality override); cycle inversion; drop-2 voicing;
  smooth voice leading on/off; add the root an octave down (bass note).
- Performance: hold; arpeggiator on/off; tap tempo; strum on/off and direction;
  sustain pedal (CC64) while held; mod wheel or pitch bend while held; all notes off
  (panic); mute the chord or melody half; octave and semitone transpose.
- Navigation: next/previous preset; save to the current preset; next/previous scale;
  open a menu item directly.
- Looping (needs a recorder): record/play/overdub/clear a phrase.
- MIDI: send a chosen CC or program change; switch the melody half's channel.

Shift ideas: Shift + root button = transpose without changing the layout name;
Shift + mode button = save the current setup to that preset slot; Shift + chord pad =
edit that pad directly instead of "the last chord played"; Shift + melody pad =
choose the arpeggiator's root note or rate.

Button lights could show state (hold, arpeggiator, Shift, a latched function). That
needs the backlight channel of each button, which the LED test can measure.

Stored as a new settings record, so presets and the settings backup carry it.

## Other ideas

- Pad sensitivity: per-pad calibration and velocity curves (soft/normal/hard).
- Chord memory: store any chord (from MIDI in or by pads) on a chord pad.
- Theory hints: light the pads of common next chords (e.g. V after ii).
- Phrase recorder / note step sequencer, synced to MIDI clock (chord progressions: 0.11.0).
- MIDI in "scale lock": quantize incoming notes to the current scale (MIDI thru).
- Use the external memory (after it is backed up): many more presets and scales.
- MPE output (per-note pitch bend/pressure on the melody half).
- Dual boot (low priority): firmware A starts normally, firmware B while two right-half
  pads are held at power-up. Easy with two TBOSS builds; with the original firmware it
  needs a check that the original never writes sectors 10-11 and that the loader does not
  verify its checksum at boot (the original must stay at 0x08040000).
