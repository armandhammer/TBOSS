# TBOSS — TheoryBoard Open Source Software

**Open-source firmware for the TheoryBoard THY333** music controller: play chords and
melodies by scale degree, with strum, hold, an arpeggiator, chord names on the display,
presets, custom scales from a web editor, and full MIDI control.

> **Alpha.** TBOSS is an independent community project, not affiliated with, endorsed or
> supported by the makers of the TheoryBoard. It installs from the browser through the
> board's own update mode and never touches the board's start-up loader, so you can
> always reinstall, or go back to the original firmware with the manufacturer's update
> utility. Use it at your own risk, and please tell us what works and what doesn't.

**Install:** https://armandhammer.github.io/TBOSS/install.html (Chrome or Edge) ·
**Editor:** https://armandhammer.github.io/TBOSS/

## Features

- **96 pressure-sensitive pads:** chords by scale degree on the left (four voicings
  per chord), melody on the right; velocity from strike force.
- **Any key and scale:** 12 root buttons; major, minor, the seven modes, harmonic and
  melodic minor, pentatonics, chromatic; plus **three custom scales** made on a piano
  keyboard in the web editor.
- **Chord edits:** lower any chord tone an octave, add 6th/7th/9th, sus2/sus4, per pad.
- **Chord names on the display:** `Dm7`, `G7sus4`, `Am/C` ...
- **Hold, strum** (up, down, alternating) and an **arpeggiator** (up, down, up-down,
  random, as played; 1-3 octaves; synced to your DAW's MIDI clock).
- **MIDI:** separate channels for chords and melody; pressure as poly or channel
  aftertouch, mod wheel, breath, expression or filter (CC74).
- **8 presets**, saved settings, and pad lights that show incoming MIDI notes.
- **Settings menu** on the display, driven by the five buttons below it (back, up,
  enter, down, deeper).
- **Web editor** (Chrome/Edge): custom scales, settings backup and restore, and a
  verified backup of the board's 16 MB memory chip.
- **Browser installer:** a USB cable and Chrome or Edge; keeps your settings across updates.
- USB MIDI class compliant: no drivers on Windows, macOS or Linux.

## Status

Version 0.3.0, **alpha**: a basic feature set that runs on a real board, tested on one
unit so far. Some features of the original firmware are not in TBOSS yet (sequencer,
drum mode, more user scales…): see [Known issues](docs/manual.md#known-issues) and the
[roadmap](docs/roadmap.md). Reports from other boards are very welcome (see [Issues](#issues)).

## Documentation

| | |
| --- | --- |
| [User manual](docs/manual.md) | Every feature, step by step, and a feature check |
| [Installing](docs/flashing.md) | From the browser; going back; advanced: SWD adapter |
| [Hardware reference](docs/hardware.md) | MCU, memory map, loader, pads, touch controllers, LEDs, display, memory chip, J2 |
| [Building and testing](docs/building.md) | Reproducible build, native tests |
| [Firmware architecture](docs/architecture.md) | How the code is organised, how to add a feature |
| [SysEx protocol](docs/tb-sysex.md) | What the web editor sends |
| [Update mode protocol](docs/installer-protocol.md) | What the browser installer sends |
| [Roadmap](docs/roadmap.md) | Customisable buttons, more ideas |

## Quick start

1. Open the [installer](https://armandhammer.github.io/TBOSS/install.html) in Chrome or
   Edge and follow its steps ([details](docs/flashing.md)).
2. Connect the board to your computer and play: see the [manual](docs/manual.md). The
   five buttons below the screen open and move through the settings.
3. Custom scales and backups: the [web editor](https://armandhammer.github.io/TBOSS/).

Releases with the firmware file (`theoryboard-usb.bin`) are [here](../../releases).

## Issues

Use [Issues](../../issues) for bug reports, feature ideas and hardware findings (there
are templates for each). Please include the TBOSS version (Settings → About on the board)
and what you were doing. Questions, ideas and experiences: [Discussions](../../discussions).

## Contributing

Pull requests are welcome; see [CONTRIBUTING.md](CONTRIBUTING.md). The build must pass
its safety check and the tests.

## License

TBOSS is free software under the **GNU General Public License v3.0 or later**
([LICENSE](LICENSE)). The build downloads and uses TinyUSB (MIT) and the CMSIS
headers from Arm and ST (Apache-2.0) at pinned versions; they are not part of this
repository. "TheoryBoard" is a trademark of its holder and is used here only to say which
device this firmware runs on.
