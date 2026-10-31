<p align="center">
  <img src="docs/assets/tboss-logo.jpg" alt="TBOSS: TheoryBoard Open Source Software" width="360">
</p>

# TBOSS — TheoryBoard Open Source Software

**Open-source firmware for the TheoryBoard THY333** music controller: play chords and
melodies by scale degree, with strum, hold, an arpeggiator, chord names on the display,
presets, custom scales from a web editor, and full MIDI control.

> **Community firmware.** TBOSS is an independent community project, not affiliated with, endorsed or
> supported by the makers of the TheoryBoard. It installs from the browser through the
> board's own update mode and never touches the board's start-up loader, so you can
> always reinstall, or go back to the original firmware with the manufacturer's update
> utility ([how](docs/flashing.md#going-back-to-the-original-firmware); the original files are
> [archived here](https://github.com/armandhammer/TheoryBoard-Original-Firmware)). Use it at your own risk, and please tell us what works and what doesn't.

**Website and manual:** https://tboss.app/

**Install:** https://tboss.app/install (Chrome or Edge)

**Editor:** https://tboss.app/editor

**Current version: 2.0** ([what's new](docs/manual.md#new-in-20); it also brings [1.9's web editor tabs](docs/manual.md#new-in-19)
and [1.8's MPE and Glide](docs/manual.md#new-in-18))

## Getting started

You need the TheoryBoard, its USB cable, and Chrome or Edge on a computer (Windows,
macOS, Linux or ChromeOS). Nothing else to install.

1. Open the [installer](https://tboss.app/install). It offers the
   current version.
2. **Updating from an earlier TBOSS?** Click *Save settings* on the page first (step 2):
   your settings, presets, scales and progressions come back after installing.
3. Start the board in **update mode**: unplug the USB cable (and the power adapter, if one
   is plugged in), hold the **top-left and
   bottom-left pads of the left half**, plug the cable back in and keep holding about
   3 seconds. The screen shows the TheoryBoard logo.
4. Click *Connect*, choose **TheoryBoard**, then *Install TBOSS*. It takes about a minute;
   the board restarts by itself. (Linux needs a one-time permission rule:
   [installation guide](docs/flashing.md).)
5. Play: the computer sees a MIDI device **TheoryBoard MIDI**. Press the center button
   below the screen for the settings; the [manual](docs/manual.md) explains everything.
6. Custom scales and backups: the [web editor](https://tboss.app/editor).

To go back to the original firmware, see
[Going back](docs/flashing.md#going-back-to-the-original-firmware).

## Status

Version 2.0 on 2026-10-30 ([what's new](docs/manual.md#new-in-20)), with 1.8 and 1.9, which were not released on their own. How it compares with the original firmware, including the features
promised in 2020: [TBOSS and the original TheoryBoard](docs/manual.md#tboss-and-the-original-theoryboard).
What's missing so far: [Known issues](docs/manual.md#known-issues) and the [roadmap](docs/roadmap.md).

**Testers wanted:** install it, go through the manual's
[feature check](docs/manual.md#feature-check) and file a
[test report](../../issues/new?template=test_report.yml), even if everything worked.
Especially wanted: anyone using the **pedal** in its other modes (sustain is confirmed on a board;
Settings → About → *I/O test* shows what the board sees), and anyone trying **scale lock** or
progressions started from a DAW.

## Features

- **96 pressure-sensitive pads:** chords by scale degree on the left (four voicings
  per chord), melody on the right; velocity from how hard you press.
- **Any key and scale:** 12 root buttons and **86 built-in scales**: the modes of major,
  melodic minor, harmonic minor and harmonic major (the M1–M7 buttons play the family you
  choose), pentatonics, blues, whole tone, diminished, bebop, Hungarian, Indian thaats,
  Messiaen's modes and more; plus **12 custom scales** made on a piano keyboard. *(0.7.0)*
- **Scale library in the web editor:** browse by family or bright to dark, find scales
  by the notes you play, see the scales one note away, and keep up to **128 favorites**
  on the board for ▲/▼. *(0.7.0)*
- **Chord panel:** the center buttons do what the board prints: inversions, guitar
  voicing, spread (root down 1–3 octaves) and octave shift or stacking for every chord. *(0.6.0)*
- **Chord edits:** add 6th/7th/9th, sus2/sus4, strum on/off, per pad.
- **Melody layouts:** scale, chromatic, fourths, and **drum mode** (scale, bass and
  General MIDI drum grids on their own channels). *(0.6.0)* The original's promised **bass mode**:
  each scale note on two pads side by side for fast two-finger bass lines. *(1.7)*
- **Program Change** each way: each half's synth sound saved with a preset, and a DAW switching
  the board's presets; the Spread buttons can open the **whole chord**. *(1.7)*
- **Step sequencer:** 4 lanes of up to 64 steps (chords, melody or drums) beside the progression, with
  each step's velocity, gate, chance, ratchet and condition, direction and Euclid fills *(1.5, 1.7)*; Fill steps,
  nudge, each lane's swing and transpose, Mutate and Stutter buttons *(2.0)*. **Harmony, echo, mono** and the
  arpeggiator's chance *(1.6)*, and a **Turing pattern** for the arpeggiator: a random loop that changes a little each
  time round *(2.0)*.
- **Chained progressions** (P1 > P1 > P2 …) with each chord's chance of resting or borrowing, **presets with their own
  chord sets**, colors and curves, **U3 as a Shift button**, and Sleep and Shut down. *(2.0)*
- **MPE** (each pad on its own channel: per-note pressure, also inside a chord) and **Glide** (roll from pad to pad to
  slide the pitch). *(1.8)*
- **The board set up from the web editor:** every setting, the pad colors (seven schemes), velocity and pressure
  curves drawn with the mouse and a calibration, the step lanes as a grid, the chord pads' edits, and presets named,
  copied and compared. The editor has a USB port of its own, so your DAW keeps playing while it is connected. *(1.9,
  2.0)*
- **Live notes**, the original's promised Live Note Update: the right half offers the
  notes of the chord you just played, on their own MIDI channel if you like, and can follow
  the chords your DAW plays; chord pads can be set silent to only pick the chord. *(0.8.0)*
- **Voice leading** (each chord takes the inversion closest to the one before, for smooth
  progressions), **note repeat** for the right half (MPC-style, louder as you press), and
  the original's **octave folding**, all optional. *(0.9.0)*
- **Chord progressions:** tap chord pads to record a song's chords (8 slots), then let the
  board play them in time while you play the melody: from ▲, the pedal or your DAW's Start,
  on its clock. Root buttons transpose them live, mode buttons re-color them, and chords
  from other scales (D and E7 in A minor) are kept as recorded. Guide lights show the chord
  now and the next one. Or type `Am D Dm Am C G E7 Am` in the web editor: it finds the
  pads and the borrowed scales. Plus velocity curves per half and the sustain pedal on
  chords, melody or both. *(0.11.0)* Step-by-step walk-throughs:
  [record and play](docs/manual.md#try-progression),
  [a song with borrowed chords](docs/manual.md#try-borrowed),
  [type it in the web editor](docs/manual.md#try-typed).
- **Chord sets:** any 12 chords on the chord pads, typed in the web editor (`C Dm Em F G Am
  Bb Ab Fm D7 E7 G7`): borrowed chords and secondary dominants next to the key's own, or a
  song's chords in order; the root buttons transpose them. **Theory hints** light the
  chords that usually come next. **Scale lock** plays the notes your computer sends in the
  board's key. **Center buttons you assign** (arpeggiator, sustain, a 7th while held, tap
  tempo, next preset …) and a **Shift** button: Shift + a root button plays a progression,
  Shift + a mode button loads a preset. *(1.0)* Step-by-step walk-throughs:
  [a set of borrowed chords](docs/manual.md#try-chord-set),
  [a song's chords, then a progression](docs/manual.md#try-set-song),
  [theory hints](docs/manual.md#try-hints),
  [center buttons and Shift](docs/manual.md#try-buttons),
  [scale lock](docs/manual.md#try-scale-lock).
- **Pad sensitivity:** set how lightly the pads play. Settings → Pads: *Trigger* (how light a
  touch starts a note), *Release* (how far you lift before it ends) and *Full* (maximum
  velocity and pressure), in steps of 100. The defaults play from a light touch, so most
  players no longer need a sensitivity mod; with one, raise *Trigger* (about 1900) so
  neighboring pads stay quiet. *Pad test* shows your pads' readings. *(0.10.0)*
- **Pad test** page: see how hard each touch reads, how much a press
  spreads to neighboring pads, and the velocity each note got. Reliability fixes: notes
  held when the computer goes away now end on a TRS synth, pitch bend and pressure always
  return to rest, a running transport continues instead of restarting after the web
  editor talks to the board, and voice leading always plays. *(0.9.1)*
- Out of the box it plays like a regular TheoryBoard: every extra is off until you turn it
  on, and the [manual](docs/manual.md) walks through each one with an example.
- **Chord names on the display:** `Dm7`, `G7sus4`, `Am/C` ...
- **Hold, strum** (up, down, alternating; in ms or note values; repeat while held,
  cutoff; random feel in timing and velocity *(0.7.0)*), a **guitar voicing** on six real
  strings with a **capo** *(0.7.0)*, and an **arpeggiator** (up, down, up-down, random, as played, chords; 1-3
  octaves; rhythm patterns, swing, latch; synced to your DAW's MIDI clock).
- **MIDI clock out** and Start/Stop; pedal as sustain, sostenuto, soft, portamento,
  hold, play/stop or a drum note. *(0.6.0)*
- **MIDI:** separate channels for chords and melody; pressure set per half as poly
  aftertouch (chord pads too), channel aftertouch, mod wheel, breath, expression,
  filter (CC74) or pitch bend; velocity curves.
- **Rainbow pad lights** that follow your key and scale, white under your fingers
  (brighter as you press harder), and show incoming MIDI notes.
- **8 presets** and saved settings.
- **TRS MIDI out** (Type A, confirmed on a board) to hardware synths, also without a
  computer, and a **sustain pedal** input (sustain confirmed on a board; also hold, play/stop and more).
- **Settings menu** on the display, driven by the five buttons below it (back, up,
  enter, down, deeper).
- **Web editor** (Chrome/Edge): custom scales, typed chord progressions and chord sets, the center buttons, settings backup
  and restore, and a verified backup of the board's 16 MB memory chip.
- **Browser installer:** a USB cable and Chrome or Edge; keeps your settings across updates.
- USB MIDI class compliant: no drivers on Windows, macOS or Linux.

## Documentation

| | |
| --- | --- |
| [User manual](https://tboss.app/manual) ([as Markdown](docs/manual.md)) | Every feature, step by step, and a feature check |
| [Installing](docs/flashing.md) | From the browser; going back; advanced: SWD adapter |
| [Hardware reference](docs/hardware.md) | MCU, memory map, loader, pads, touch controllers, LEDs, display, memory chip, J2 |
| [Building and testing](docs/building.md) | Reproducible build, native tests |
| [Firmware architecture](docs/architecture.md) | How the code is organized, how to add a feature |
| [SysEx protocol](docs/tb-sysex.md) | What the web editor sends |
| [Update mode protocol](docs/installer-protocol.md) | What the browser installer sends |
| [Roadmap](docs/roadmap.md) | Plans and more ideas |

## Issues

Use [Issues](../../issues) for bug reports, feature ideas and hardware findings (there
are templates for each). Please include the TBOSS version (Settings → About on the board)
and what you were doing. Questions, ideas and experiences: [Discussions](../../discussions).
Releases with the firmware file (`theoryboard-usb.bin`) are [here](../../releases).

## Contributing

Pull requests are welcome; see [CONTRIBUTING.md](CONTRIBUTING.md) and the
[code of conduct](CODE_OF_CONDUCT.md). The build must pass its safety check and the tests;
GitHub runs both on every pull request.

## License

TBOSS is free software under the **GNU General Public License v3.0 or later**
([LICENSE](LICENSE)). The build downloads and uses TinyUSB (MIT) and the CMSIS
headers from Arm and ST (Apache-2.0) at pinned versions; they are not part of this
repository. "TheoryBoard" is a trademark of its holder and is used here only to say which
device this firmware runs on.
