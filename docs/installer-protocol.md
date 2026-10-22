# The TheoryBoard update mode (USB HID) and the TBOSS installer

The board's resident loader (flash sectors 0-5, see [hardware.md](hardware.md)) can
receive new firmware over USB. The TBOSS web installer (`webapp/install.html`,
`webapp/tb-installer.js`) uses this to install TBOSS with only a USB cable and a
browser. This page describes the parts of the protocol the installer uses. It was
worked out by static analysis of the loader and of the manufacturer's update utility;
the descriptions are our own.

## Entering update mode

Hold the **top-left and bottom-left pads of the left (chord) half** while plugging in
USB. Instead of starting the application after its usual ~1.5 s wait, the loader
connects as a USB HID device, vendor/product ID **`0333:0333`**, with 64-byte input
and output reports (report ID 0). It stays there, answering commands, until told to
finish. On the board the display shows the TheoryBoard logo and the five buttons
below it stay lit; browsers list the device as **TheoryBoard**. Because this happens before any application runs, update mode works whatever
firmware is installed, including a damaged one.

## Message formats

| Kind | 64 bytes |
| --- | --- |
| Command / status | 8 ASCII characters (the name), then a fixed 56-character constant: `sSJFfddSAD$srAffy84JappEKL3$SALgK@3J4hwd5920xg1!8hs7J6Kg` |
| Number | 32-bit little-endian value, then 60 zero bytes |
| Data | 64 bytes of firmware |

The board answers in the same formats. A command it does not recognize is answered
with `SERERRNF`.

## Commands the installer sends

| Command | Effect |
| --- | --- |
| `XCFILNFS` | Opens a small set of commands for the loader's own metadata (on the external memory chip). |
| `DUMPMETA` | Reads 15 metadata values and reports them: `FWMETABG`, 15 numbers, `FWMETADN`. The first is the hardware version (**4** on the boards known so far). Reads only. |
| `DONECMDS` | Closes the metadata commands (answer `FCMDDONE`). |
| `XCSMDIUS` | **Direct install.** The loader answers `FWSETRD1`, then takes 7 numbers: check value, check value compared with the flash after writing (a mismatch only shows a message), byte count, check value, then 1, 0, 1. It erases internal flash sectors 6-11 (`0x08040000`-`0x080FFFFF`) and answers `FWSETRD2`. Every following data report is written at the next address from `0x08040000`; the loader refuses to write past `0x080FFFFF`. |
| `DONE0000` | Ends the data. The loader answers `TRACRCVL` + the CRC of all data received, `EXTCRCVL` + the CRC of the same length of flash read back, then `FWDLYRDY`. |
| `ZCSMCCOZ` | Finish: the loader shows a message, waits about a second and starts the application. |

CRC: polynomial `0x04C11DB7`, most significant bit first, initial value 0, no final
inversion, over whole 64-byte blocks (the installer pads the last block with `0xFF`).
The installer computes the same value itself and only reports success when **both**
loader CRCs match it. The metadata's last three values are the microcontroller's unique ID; the
installer leaves them out of what it shows.

## What the installer never sends

The loader has other commands, including some that change the external memory chip
(writing or erasing its metadata area, erasing the whole chip, or initializing the
original firmware's files) and a staged install that copies through the external chip.
The installer cannot send them: `tb-installer.js` refuses to build any command not in
the table above, and `tests/webapp/test_installer.mjs` runs it against a model of the
loader that flags any other command.

## Going back

The same update mode is what the manufacturer's update utility uses, so the original
firmware can be reinstalled with that utility and its update file.
