/* SPDX-License-Identifier: GPL-3.0-or-later - review TBR-022 (2.4.8): on the board the core-coupled RAM (ln_bank) is
 * garbage until st_load clears it; on the host it would start zeroed and hide a read before st_load. Every suite that
 * links lanes.c links this too (scripts/run_tb_tests.py), so ln_bank starts as 0xA5 bytes before main. */
#include "../../src/tb/lanes.h"
#include "../../src/tb/engine.h"
#include <string.h>

/* The runner's -funsigned-char -fshort-enums (TBR-022) are in force, as on the board. */
_Static_assert((char)-1 > 0, "char must be unsigned, as on ARM");
_Static_assert(sizeof(tk_state) == 1u && sizeof(tk_kind) == 1u, "enums must be short, as on ARM");

__attribute__((constructor)) static void ccm_garbage(void) { memset(ln_bank, 0xA5, sizeof ln_bank); }
