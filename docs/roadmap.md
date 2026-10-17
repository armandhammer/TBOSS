# Roadmap

Ideas and agreed next steps, newest decisions first. Nothing here is built
unless marked. (Written so it can also go into the public TBOSS repository.)

## 0.5.x: lights, a robust start, aftertouch per half (2026-10-17)

- [x] A pad that reads pressed when the board starts no longer silences the whole board
      (tester report: no pad reaction, no MIDI); it stays silent until released once.
- [x] Rainbow pad colours by note (default), touched pads white with pressure; the 0.4
      look stays as Settings → Lights → Colours → Root+5th.
- [x] Web editor preview: the whole board shows the scale; the display names the slot and notes.
- [x] I/O test page shows pads that read as held.
- [x] Aftertouch per half (Chord AT, Melody AT), and poly aftertouch from chord pads on each
      of their notes that no other held pad shares.
- [x] Editor keys in the board's colours.
- [x] 0.5.1 checked on a board and released as the default (2026-10-17).

## 0.4.0: the jacks (2026-10-16)

- [x] TRS MIDI out (Type A): everything sent over USB also goes to the jack; plays the jack
      alone when no computer is attached. Settings → MIDI → TRS out.
- [x] Sustain pedal: CC64 per side, or the board's hold; auto/normal/inverted pedal types.
- [x] I/O test page (Settings → About) showing the pedal input live and sending a test note.
- [x] 0.4.0 installed and checked on a board without the jacks (USB playing, menu, settings,
      editor, no-computer start); released as the default 2026-10-16.
- [ ] Confirm both on a board (pins from the previous firmware: PB6 USART1 TX, PD13).

## Alpha launch: 0.3.0 (agreed 2026-10-16)

Launch with a basic feature set; users test and report what breaks.

- [x] Browser installer through the board's own update mode (WebHID); verified on hardware.
- [x] Settings, presets and user scales carried across an install (saved over MIDI first,
      restored after the restart).
- [x] Hosted installer and editor on GitHub Pages, with the newest release's firmware.
- [x] Firmware version on the start-up screen, in Settings → About, reported to the editor
      and the installer, and in the USB device release number.
- [x] Settings menu as a tree driven by the five buttons below the display: left back,
      up/down move, centre enter, right deeper; only left leaves (hold left + centre: home).
- [x] Manual: browser install first, real button names, known issues; SWD guide kept as advanced.
- [x] Alpha disclaimer; Discussions for feedback.
- [x] Installed on hardware with the browser installer, settings kept (2026-10-16); released as v0.3.0.
- [x] Going back with the manufacturer's update utility after TBOSS: confirmed (the utility
      hangs at "finalizing" although the board has finished; close it and replug).
- [ ] Own USB product ID 1209:B333 from pid.codes (requested 2026-10-16, pidcodes PR #1319),
      replacing the test ID `cafe:4069`.

Not in TBOSS yet (candidates for the next stages): sequencer/phrase recorder, drum mode,
a second arpeggiator, more user scale slots, an arpeggiator on/off button, per-pad
sensitivity, customisable centre buttons (below).

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

## Customisable centre buttons and a Shift layer (idea, 2026-10-16)

Let users choose, in the web editor, what each of the 14 centre buttons does, with
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
- Phrase recorder / step sequencer, synced to MIDI clock.
- MIDI in "scale lock": quantise incoming notes to the current scale (MIDI thru).
- Use the external memory (after it is backed up): many more presets and scales.
- MPE output (per-note pitch bend/pressure on the melody half).
