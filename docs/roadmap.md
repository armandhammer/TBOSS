# Roadmap

Ideas and agreed next steps, newest decisions first. Nothing here is built
unless marked. (Written so it can also go into the public TBOSS repository.)

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
- [ ] Own USB product ID (free open-source PID from pid.codes) instead of the test ID `cafe:4069`.

## Install from Chrome, no debugger (research first)

The board's resident loader has a built-in USB update mode: holding two pads at
power-up keeps it in the loader, which then talks to the vendor's update utility
over USB HID (vendor/product 0333:0333, 64-byte reports). The protocol and the file
format (header with firmware/scale flags, check values, plaintext or encrypted
payload) are partly understood from static analysis of the vendor utility and the
loader. If the loader accepts a plaintext image, a web page using **WebHID** could
install TBOSS with only a USB cable and Chrome/Edge on any OS: no drivers, no
debugger, no separate Windows/Mac installers. Going back would mean running the
vendor's own update utility with the vendor's update file.

Open questions, in order:
1. Which two pads enter update mode, and does it work on a board running TBOSS?
   (Static analysis says the loader runs before any application.)
2. The exact request/response sequence, what each check value covers, and where
   the loader writes the payload (must be sectors 6+ only; never the loader).
3. The header's "initialise storage" flag: does it erase the external NOR? The
   installer must leave the NOR untouched.
4. What happens if the transfer is interrupted (can update mode always be
   re-entered?). Must be proven on hardware with the SWD restore as the safety net.

The SWD tools stay as the expert and recovery path (they are also the only way to
back up a board's original internal flash).

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
