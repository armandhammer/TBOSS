/* SPDX-License-Identifier: GPL-3.0-or-later */
#ifndef TB_USB_CLOCK_H
#define TB_USB_CLOCK_H
/* TheoryBoard photo P15 shows 8.000; actual frequency remains to be tested.
 * Independent, lower-speed clock plan, not Discovery's 168 MHz port. */
#define TB_HSE_HZ 8000000u
#define TB_PLL_M 8u
#define TB_PLL_N 192u
#define TB_PLL_P 4u
#define TB_PLL_Q 4u
#define TB_CORE_HZ ((TB_HSE_HZ / TB_PLL_M) * TB_PLL_N / TB_PLL_P)
#define TB_USB_HZ ((TB_HSE_HZ / TB_PLL_M) * TB_PLL_N / TB_PLL_Q)
#define TB_PLL_REGISTER (TB_PLL_M | (TB_PLL_N << 6) | (1u << 16) | (1u << 22) | (TB_PLL_Q << 24))
_Static_assert(TB_CORE_HZ == 48000000u && TB_USB_HZ == 48000000u, "48 MHz clocks");
_Static_assert(TB_HSE_HZ / TB_PLL_M == 1000000u, "PLL input 1 MHz");
_Static_assert((TB_HSE_HZ / TB_PLL_M) * TB_PLL_N == 192000000u, "VCO 192 MHz");
#endif
