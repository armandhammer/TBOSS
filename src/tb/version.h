/* SPDX-License-Identifier: GPL-3.0-or-later - the firmware version: start-up screen, the menu's
 * About page, SysEx IDENTIFY, the USB device release number, and a tag the web installer
 * finds in the image ("TBOSS 0.3.0"). */
#ifndef TB_VERSION_H
#define TB_VERSION_H
#define TB_VERSION_MAJOR 2
#define TB_VERSION_MINOR 0
#define TB_VERSION_PATCH 8 /* 2.0: the Turing pattern, chained progressions with each chord's chance, presets' own chord sets, U3 as Shift, Sleep and Shut down, the menu rebalanced and reordered, Spread of Bass, Fill, nudge and swing, Mutate, Stutter, lane transpose (1.9: the editor's tabs); every build to test has its own number (2.0.0, 2.0.1 ...) */
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
