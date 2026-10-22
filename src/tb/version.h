/* SPDX-License-Identifier: GPL-3.0-or-later - the firmware version: start-up screen, the menu's
 * About page, SysEx IDENTIFY, the USB device release number, and a tag the web installer
 * finds in the image ("TBOSS 0.3.0"). */
#ifndef TB_VERSION_H
#define TB_VERSION_H
#define TB_VERSION_MAJOR 1
#define TB_VERSION_MINOR 0
#define TB_VERSION_PATCH 0
#define TB_STR2(x) #x
#define TB_STR(x) TB_STR2(x)
#define TB_VERSION TB_STR(TB_VERSION_MAJOR) "." TB_STR(TB_VERSION_MINOR) "." TB_STR(TB_VERSION_PATCH)
#define TB_VERSION_TAG "TBOSS " TB_VERSION
/* What the screens show: the version, and BETA while 1.0 is in public testing (the tag, IDENTIFY
 * and USB keep the plain number). */
#define TB_VERSION_LABEL TB_VERSION " BETA"
#define TB_USB_BCD (TB_VERSION_MAJOR << 8 | TB_VERSION_MINOR << 4 | TB_VERSION_PATCH)
#endif
