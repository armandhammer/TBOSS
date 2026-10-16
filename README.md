# TBOSS — TheoryBoard Open Source Software

**Open-source firmware for the TheoryBoard THY333** music controller: play chords and
melodies by scale degree, with strum, hold, an arpeggiator, chord names on the display,
presets, custom scales from a web editor, and full MIDI control.

> TBOSS is an independent community project. It is not affiliated with, endorsed or
> supported by the makers of the TheoryBoard. Installing it means opening the case and
> using a debug adapter, at your own risk. Keep the backup the installer makes: it is
> your way back to the original firmware.

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
- **Web editor** (Chrome/Edge): custom scales, settings backup and restore, and a
  verified backup of the board's 16 MB memory chip.
- USB MIDI class compliant: no drivers on Windows, macOS or Linux.

## Status

Version 0.2.0: all features above run on a real board. Tested on one unit so far;
reports from other boards are very welcome (see [Issues](#issues)).

## Documentation

| | |
| --- | --- |
| [User manual](docs/manual.md) | Every feature, step by step, and a feature check |
| [Installing](docs/flashing.md) | Backup, install and restore with an SWD adapter |
| [Hardware reference](docs/hardware.md) | MCU, memory map, loader, pads, touch controllers, LEDs, display, memory chip, J2 |
| [Building and testing](docs/building.md) | Reproducible build, native tests |
| [Firmware architecture](docs/architecture.md) | How the code is organised, how to add a feature |
| [SysEx protocol](docs/tb-sysex.md) | What the web editor sends |
| [Roadmap](docs/roadmap.md) | Browser installer, customisable buttons, more ideas |

## Quick start

1. Download this repository and `theoryboard-usb.bin` from the latest
   [release](../../releases).
2. Follow [Installing](docs/flashing.md): wire an SWD adapter to J2, run
   `python scripts/tboss_flash.py backup`, then `install`.
3. Connect the board to your computer and play: see the [manual](docs/manual.md).
4. For custom scales and backups: `python scripts/tboss_editor.py` opens the web editor.

## Issues

Use [Issues](../../issues) for bug reports, feature ideas and hardware findings (there
are templates for each). Please include the TBOSS version and what you were doing.

## Contributing

Pull requests are welcome; see [CONTRIBUTING.md](CONTRIBUTING.md). The build must pass
its safety check and the tests.

## License

TBOSS is free software under the **GNU General Public License v3.0 or later**
([LICENSE](LICENSE)). The build downloads and uses TinyUSB (MIT) and the CMSIS
headers from Arm and ST (Apache-2.0) at pinned versions; they are not part of this
repository. "TheoryBoard" is a trademark of its holder and is used here only to say which
device this firmware runs on.
