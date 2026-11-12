/* SPDX-License-Identifier: GPL-3.0-or-later - firmware resident at 0x08040000 (sectors 6-10).
 * Sectors 0-5 belong to the resident loader and are never part of this image;
 * sector 11 (0x080E0000) is reserved for settings (settings.c).
 * RAM layout and the diagnostic/scan/controls ABI addresses match sram.ld. */
ENTRY(Reset_Handler)
MEMORY {
  FLASH (rx) : ORIGIN = 0x08040000, LENGTH = 640K
  SRAM (rwx) : ORIGIN = 0x20000000, LENGTH = 128K
  CCM (rw) : ORIGIN = 0x10000000, LENGTH = 64K /* 2.4.0: the step patterns (lanes.c); processor only, no DMA */
}
_estack = 0x20020000;
SECTIONS {
  .isr_vector 0x08040000 : { KEEP(*(.isr_vector)) } > FLASH
  .text : { *(.text*) *(.rodata*) . = ALIGN(4); } > FLASH
  .unsupported : { *(.glue_7) *(.glue_7t) *(.vfp11_veneer) *(.v4_bx) *(.iplt) *(.rel.iplt) *(.igot.plt) } > FLASH
  .data 0x20000000 : AT(ALIGN(LOADADDR(.unsupported) + SIZEOF(.unsupported), 4)) {
    . = ALIGN(4); _sdata = .; *(.data*) . = ALIGN(4); _edata = .;
  } > SRAM
  _sidata = LOADADDR(.data);
  _image_end = _sidata + SIZEOF(.data);
  .bss (NOLOAD) : {
    . = ALIGN(4); _sbss = .;
    _sdma = .; *(.bss.tb_dma) _edma = .; /* 2.4.8 (review TBR-021): the DMA sources (diagnostic.h TB_DMA) */
    *(.bss*) *(COMMON) . = ALIGN(4); _ebss = .;
  } > SRAM
  .diagnostic 0x20018000 (NOLOAD) : {
    _sdiag = .; KEEP(*(.diagnostic))
    . = _sdiag + 112; KEEP(*(.savetime)) /* 2.4.5: the longest settings calls (main.c, review TBR-007) */
    . = _sdiag + 128; KEEP(*(.veltrace)) /* 2.4.4: the velocity trace (engine_pads.c, review TBR-032) */
    . = _sdiag + 256; KEEP(*(.scanabi))
    . = _sdiag + 1024; KEEP(*(.controls)) _ediag = .;
  } > SRAM
  .stack 0x2001c000 (NOLOAD) : { . += 16384; } > SRAM
  .ccm (NOLOAD) : { . = ALIGN(4); *(.ccm*) . = ALIGN(4); _eccm = .; } > CCM /* not zeroed: st_load clears it */
  /DISCARD/ : { *(.comment) *(.note.GNU-stack) *(.ARM.attributes) *(.debug_frame) }
}
ASSERT(SIZEOF(.isr_vector) == 392, "98 F407 vectors")
ASSERT(ADDR(.isr_vector) == 0x08040000, "loader jumps to 0x08040000")
ASSERT(_image_end <= 0x080E0000, "image reaches the settings sector")
ASSERT(_ebss <= 0x20018000, "RAM state overlaps diagnostic ABI")
ASSERT(SIZEOF(.diagnostic) == 2048, "scan diagnostic ABI")
ASSERT(tb_save_time == 0x20018070, "save timing origin")
ASSERT(tp_trace == 0x20018080, "velocity trace origin")
ASSERT(ts_status == 0x20018100, "scan control origin")
ASSERT(tc_status == 0x20018400, "controls ABI origin")
ASSERT(_ediag <= 0x2001c000, "diagnostic overlaps stack")
ASSERT(SIZEOF(.unsupported) == 0, "unexpected stubs")
ASSERT(_eccm <= 0x10010000, "the step patterns fit the core-coupled RAM")
ASSERT((_estack & 7) == 0, "MSP alignment")
/* 2.4.8 (review TBR-021): the DMA sources in the main RAM, and both tagged (frame 5,088 bytes, shown 1,024). */
ASSERT(_sdma >= 0x20000000 && _edma <= 0x20018000, "DMA sources outside the main RAM (the CCM is not on the DMA bus)")
ASSERT(_edma - _sdma >= 6112, "a DMA source not tagged TB_DMA")
/* 2.4.8 (review TBR-010): the RAM startup.S paints for the stack's high-water mark (diagnostic.h TB_PAINT_*). */
tb_stack_paint = _ediag;
tb_stack_paint_end = 0x2001FF00;
ASSERT(tb_stack_paint == 0x20018800 && tb_stack_paint_end - tb_stack_paint == 4 * 7616, "the painted words")
