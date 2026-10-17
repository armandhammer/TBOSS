# Installing TBOSS (and going back)

TBOSS installs from **Chrome or Edge** with only a USB cable, using the update mode
built into the board's start-up loader. Nothing to open, solder or install.

> **Alpha software.** TBOSS only ever replaces the firmware area (flash sectors 6-11);
> the board's start-up loader, which provides the update mode, is never touched. So an
> interrupted or failed install can always be repeated, and the original firmware can
> be put back (see [Going back](#going-back-to-the-original-firmware)).

## With a browser (recommended)

1. Open **https://armandhammer.github.io/TBOSS/install.html** in Chrome or Edge on a
   computer (Windows, macOS, Linux or ChromeOS). The page shows the firmware version it
   will install. (Offline or with your own build: `python scripts/tboss_editor.py 8000
   install.html` serves the same page from your copy, with the firmware you built.)
2. **Keep your settings** (only when TBOSS is already installed): with the board plugged
   in and running, click **Save settings**. The page keeps a copy, downloads it as a
   file, and puts it back after installing.
3. **Update mode:** unplug the USB cable; hold the **top-left and bottom-left pads of the
   left (chord) half**; plug the cable back in and keep holding for about 3 seconds.
   The screen shows the TheoryBoard logo and the five buttons below the screen stay lit.
4. **Connect:** click Connect and choose **TheoryBoard** in the list the browser shows.
5. **Install:** click **Install TBOSS** and don't unplug the board until the page says
   `Done` (about a minute). The page checks the loader's own report of what it received
   and wrote against the image, then the board restarts.
6. **Check:** the page asks the board for its version over MIDI and restores the settings
   from step 2. (If you skipped step 2, click **Check the board**.)

The *Details* box on the page records every step; please include it in a bug report.

## Updating TBOSS

Same steps with the newer version; step 2 carries your settings, presets and user
scales over.

## Going back to the original firmware

The original firmware goes back on the same way it is updated (confirmed on a board
running TBOSS):

1. Optional: keep a copy of the board's external memory chip, which holds the original
   firmware's own data and which the utility may re-initialise: web editor →
   [external memory backup](manual.md#external-memory-backup).
2. Put the board in update mode (step 3 above) and run the **manufacturer's update
   utility** with the manufacturer's update file. The manufacturer no longer exists; the
   utility (Windows and macOS) and the version 5 update file are archived, with checksums,
   at [TheoryBoard-Original-Firmware](https://github.com/armandhammer/TheoryBoard-Original-Firmware).
3. The utility may stay on **"Updating firmware... finalizing"** for good, with the board
   showing "Firmware update, do not unplug device". In the confirmed case the update had
   already finished and the board was only waiting for the utility. After about
   **5 minutes** on that message, close the utility, unplug the board and plug it in
   normally: the original firmware starts.
4. If it doesn't start, enter update mode again and repeat step 2 (or install TBOSS
   again from the browser). The loader is never affected.

If you made an SWD backup (below), restoring it puts back exactly what your board had.

## If something goes wrong

| Situation | What to do |
| --- | --- |
| The browser lists no device | The board is not in update mode: unplug, hold the two pads, plug in again. Use Chrome or Edge, not Firefox or Safari. |
| The board starts normally instead of update mode | Hold the pads before plugging in, and keep holding about 3 seconds. |
| `the board did not answer` / `stopped answering` | Unplug, enter update mode again and repeat. The loader is not affected. |
| `verification failed` | Repeat from update mode. If it fails again, report it with the *Details*. |
| Unplugged during the install | Enter update mode again and repeat: update mode starts before any firmware. |
| `hardware version` message | Your board reports a different hardware version than the boards tested so far; please open an issue with the *Details*. |
| Settings not restored | Restore the downloaded file in the [web editor](manual.md#settings-backup). |

## Advanced: installing with an SWD adapter

The SWD route is for developers, and it is the only way to **back up the board's whole
internal flash**, including the start-up loader. It needs the case opened and an SWD
debug adapter wired to the board's J2 header.

> You open the case and connect to the board's electronics, at your own risk; it may
> void a warranty. Keep the backup from step 4: it is an exact copy of your board.

### What you need

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

### Step by step

#### 1. Open the case and find J2
J2 is a row of six plated holes labelled **J2** on the main board. Hole 1 is the one
next to the label.

#### 2. Wire the adapter (board and adapter both unplugged)

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

#### 3. Power up and check
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

#### 4. Back up your board (do not skip)

```
python scripts/tboss_flash.py backup
```

This reads the whole 1 MiB internal flash twice, compares the two reads, and saves
`backups/thy333-flash-<date>.bin` plus a `.sha256` file. The board restarts afterwards.
Copy both files somewhere safe (cloud drive, USB stick).

#### 5. Install

```
python scripts/tboss_flash.py install theoryboard-usb.bin --backup backups/thy333-flash-<date>.bin
```

The tool reads the board again and refuses to continue unless the loader sectors 0-5
match your backup exactly (proof that the backup belongs to this board). It then shows
what it will do; type `GO`. It erases sectors 6-11, writes the image, verifies it, and
reads the whole flash back to confirm that sectors 0-5 are unchanged.

#### 6. Start TBOSS
Unplug the board and the adapter for a moment, then plug the board in normally. After
about 1.5 s the display shows the TBOSS play screen and a USB MIDI device
**TheoryBoard MIDI** appears. Continue with the [manual](manual.md).

### Going back with your SWD backup

```
python scripts/tboss_flash.py restore --backup backups/thy333-flash-<date>.bin
```

Same checks, then sectors 6-11 are rewritten from your backup and the whole flash is
compared with the backup byte for byte.

### Updating with the SWD tool
Run step 5 again with the new image (your backup stays valid). Saved settings are
cleared by an install: back them up first with the [web editor](manual.md#settings-backup).

### If something goes wrong (SWD)

| Message / situation | What to do |
| --- | --- |
| `OpenOCD not found` | Install OpenOCD or pass `--openocd` with its full path. |
| OpenOCD reports a problem when connecting | Check the four wires (especially GND), that the board is powered over USB, and the `--interface`. Try `--speed 400`. |
| `the two reads differ` | Unreliable connection: shorten or fix the wires and run `backup` again. |
| `sectors [...] differ from your backup` | The backup is from another board or the loader differs. Use the backup of *this* board. |
| `read-protected` | This board's flash is protected; TBOSS cannot be installed with this tool. (Removing protection erases the loader. Do not do it.) |
| Interrupted during erase or write | The loader is untouched: power-cycle and run `install` or `restore` again. |
| SWD stops answering entirely | Should not happen (TBOSS never touches the debug pins). Last resort: BOOT0 high at power-up starts ST's ROM bootloader ([hardware.md](hardware.md#j2-the-swd-debug-header)). |

### What the tool will and will not do
It only ever sends these OpenOCD operations: read ID registers, read the whole flash,
reset (in software), erase sectors 6-11, write and verify from `0x08040000`. Anything
else is refused by the tool itself, and `scripts/test_tboss_flash.py` checks this against
a simulated OpenOCD.
