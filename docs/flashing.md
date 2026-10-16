# Installing TBOSS (and going back)

Installing TBOSS today needs an **SWD debug adapter** wired to the board's J2 header.
A browser installer that needs only a USB cable is being researched
([roadmap](roadmap.md)).

> **Read this first.** You open the case and connect to the board's electronics, at
> your own risk; it may void a warranty. TBOSS only ever writes flash sectors 6-11 and
> never touches the board's loader, so a failed install can always be retried or
> undone, **provided you made the backup in step 4**. Your backup is the only way back
> to your board's original firmware: keep it safe.

## What you need

- **An SWD adapter**: an ST-Link V2 or V3, the ST-Link built into an STM32 Discovery or
  Nucleo board, or a CMSIS-DAP probe. So far `tboss_flash.py` has been run on a real
  board with the ST-Link of an STM32F4 Discovery (`--interface stlink-dap`) for `check`
  and `backup`; `install` and `restore` use the same OpenOCD operations that installed
  TBOSS on that board and are tested against a simulator. Please report how it goes
  with your adapter.
- **Four connections** to J2 (see [hardware.md](hardware.md#j2-the-swd-debug-header)):
  thin wires with hooks, test clips, or a 6-pin 2.54 mm header soldered into J2.
- **OpenOCD 0.12 or newer**, e.g. [xPack OpenOCD](https://xpack-dev-tools.github.io/openocd-xpack/)
  (Windows, macOS, Linux). Note where `openocd` (`openocd.exe`) ends up.
- **Python 3.9 or newer.**
- **TBOSS**: this repository (green *Code* button → *Download ZIP*, or `git clone`) and
  the firmware image `theoryboard-usb.bin` from the latest
  [release](../../releases), or one you [built yourself](building.md).

## Step by step

### 1. Open the case and find J2
J2 is a row of six plated holes labelled **J2** on the main board. Hole 1 is the one
next to the label.

### 2. Wire the adapter (board and adapter both unplugged)

| J2 hole | Adapter pin |
| --- | --- |
| 1 | VTref / target voltage (**sense only**: the adapter must not power the board) |
| 2 | SWCLK |
| 3 | GND |
| 4 | SWDIO |
| 5, 6 | **nothing** |

*Using an STM32 Discovery or Nucleo board as the adapter:* remove both **CN2** jumpers
(this disconnects the adapter from the Discovery's own microcontroller), then use its
6-pin SWD connector: pin 1 VDD_TARGET, 2 SWCLK, 3 GND, 4 SWDIO; leave 5 and 6 free.

### 3. Power up and check
Plug the TheoryBoard into USB as usual, then plug in the adapter. In a terminal, in the
TBOSS folder:

```
python scripts/tboss_flash.py check
```

Add `--openocd <path to openocd>` if OpenOCD is not on your PATH, and
`--interface stlink-dap` or `--interface cmsis-dap` for those adapters (the default is
`stlink`). Expected:

```
Connecting...
  STM32F40x/41x rev ..., 1 MiB flash, readable (RDP level 0), no write protection
OK: nothing was written.
```

`check` only reads three ID registers; the board keeps running.

### 4. Back up your board (do not skip)

```
python scripts/tboss_flash.py backup
```

This reads the whole 1 MiB internal flash twice, compares the two reads, and saves
`backups/thy333-flash-<date>.bin` plus a `.sha256` file. The board restarts afterwards.
Copy both files somewhere safe (cloud drive, USB stick).

### 5. Install

```
python scripts/tboss_flash.py install theoryboard-usb.bin --backup backups/thy333-flash-<date>.bin
```

The tool reads the board again and refuses to continue unless the loader sectors 0-5
match your backup exactly (proof that the backup belongs to this board). It then shows
what it will do; type `GO`. It erases sectors 6-11, writes the image, verifies it, and
reads the whole flash back to confirm that sectors 0-5 are unchanged.

### 6. Start TBOSS
Unplug the board and the adapter for a moment, then plug the board in normally. After
about 1.5 s the display shows the TBOSS play screen and a USB MIDI device
**TheoryBoard MIDI** appears. Continue with the [manual](manual.md).

## Going back to the original firmware

```
python scripts/tboss_flash.py restore --backup backups/thy333-flash-<date>.bin
```

Same checks, then sectors 6-11 are rewritten from your backup and the whole flash is
compared with the backup byte for byte.

## Updating TBOSS
Run step 5 again with the new image (your backup stays valid). Saved settings are
cleared by an install: back them up first with the [web editor](manual.md#settings-backup).

## If something goes wrong

| Message / situation | What to do |
| --- | --- |
| `OpenOCD not found` | Install OpenOCD or pass `--openocd` with its full path. |
| OpenOCD reports a problem when connecting | Check the four wires (especially GND), that the board is powered over USB, and the `--interface`. Try `--speed 400`. |
| `the two reads differ` | Unreliable connection: shorten or fix the wires and run `backup` again. |
| `sectors [...] differ from your backup` | The backup is from another board or the loader differs. Use the backup of *this* board. |
| `read-protected` | This board's flash is protected; TBOSS cannot be installed with this tool. (Removing protection erases the loader. Do not do it.) |
| Interrupted during erase or write | The loader is untouched: power-cycle and run `install` or `restore` again. |
| SWD stops answering entirely | Should not happen (TBOSS never touches the debug pins). Last resort: BOOT0 high at power-up starts ST's ROM bootloader ([hardware.md](hardware.md#j2-the-swd-debug-header)). |

## What the tool will and will not do
It only ever sends these OpenOCD operations: read ID registers, read the whole flash,
reset (in software), erase sectors 6-11, write and verify from `0x08040000`. Anything
else is refused by the tool itself, and `scripts/test_tboss_flash.py` checks this against
a simulated OpenOCD.
