/* SPDX-License-Identifier: GPL-3.0-or-later - the firmware version: start-up screen, the menu's
 * About page, SysEx IDENTIFY, the USB device release number, and a tag the web installer
 * finds in the image ("TBOSS 0.3.0"). */
#ifndef TB_VERSION_H
#define TB_VERSION_H
#define TB_VERSION_MAJOR 2
#define TB_VERSION_MINOR 3
#define TB_VERSION_PATCH 6 /* 2.3: Modes (modal interchange: each chord row its own mode on the key's root, Row 2..4, Mel follows) with 2.2.1 and 2.1 in it; 2.3.1: the screen's changes in one DMA transfer (no top-down drawing); 2.3.2: a knob just learned stays quiet while its item is open (CCnn = shows); 2.3.3: the microtonal tunings dropped (2.2.0's; out of scope), a full 64-byte USB transfer no longer ends the session; 2.3.4: Modes' chord line names the mode in three letters (Edim MIX, D# AEO), the line under the key fits (MODE M1 P2 MODES); 2.3.5: Harm minor and Mel minor rows as HMIN, MMIN (G HMIN); 2.3.6: the pads' defaults +1000/+600/+3200 (were +700/+300/+2500). 2.2.1: notes due together in one USB transfer, MIDI in read before the engine, Step grid (4x4 blocks). 2.1: external MIDI control (the MIDI map, Settings > Control), Layout's first item named Melody. 2.0: the Turing pattern, chained progressions with each chord's chance, presets' own chord sets, U3 as Shift, Sleep and Shut down, the menu rebalanced and reordered, Spread of Bass, Fill, nudge and swing, Mutate, Stutter, lane transpose (1.9: the editor's tabs); every build to test has its own number (2.1.0, 2.1.1 ...) */
#define TB_STR2(x) #x
#define TB_STR(x) TB_STR2(x)
#define TB_VERSION TB_STR(TB_VERSION_MAJOR) "." TB_STR(TB_VERSION_MINOR) "." TB_STR(TB_VERSION_PATCH)
#define TB_VERSION_TAG "TBOSS " TB_VERSION
/* What the screens show. A public release (TB_RELEASE 1) is named by its major and minor version only:
 * "1.7" (released 2026-10-26: build 1.7.10, the step sequencer's second round, the 2020 promises, the
 * pads from each one's rest, tested on a board). A build being tested shows its whole number and
 * ALPHA ("1.7.11 ALPHA"). The tag, IDENTIFY and USB always carry the whole number (the installer
 * checks it). Before 1.7 releases were "x.y.0 BETA". */
#define TB_RELEASE 1
#if TB_RELEASE
#define TB_VERSION_LABEL TB_STR(TB_VERSION_MAJOR) "." TB_STR(TB_VERSION_MINOR)
#else
#define TB_VERSION_LABEL TB_VERSION " ALPHA"
#endif
/* The USB device release number is BCD, one digit for the patch: 9 stands for 9 and up there. */
#define TB_USB_BCD (TB_VERSION_MAJOR << 8 | TB_VERSION_MINOR << 4 | (TB_VERSION_PATCH > 9 ? 9 : TB_VERSION_PATCH))
#endif
