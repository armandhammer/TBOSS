/* SPDX-License-Identifier: GPL-3.0-or-later
 * Flash-resident F407 entry at 0x08040000, started by the resident loader.
 * The loader leaves VTOR=0x08040000, SysTick off and NVIC cleared, but its clocks
 * and peripherals still running; tb_flash_reset restores reset state first.
 * Same 98-entry vector table as the SRAM entry (theoryboard_usb/startup.S). */
.syntax unified
.cpu cortex-m4
.thumb
.section .isr_vector,"a",%progbits
.balign 512
.global vectors
vectors:
    .word _estack
    .word Reset_Handler
    .word Default_Handler /* NMI */
    .word Default_Handler /* HardFault */
    .word Default_Handler /* MemManage */
    .word Default_Handler /* BusFault */
    .word Default_Handler /* UsageFault */
    .word 0, 0, 0, 0
    .word Default_Handler /* SVCall */
    .word Default_Handler /* DebugMon */
    .word 0
    .word Default_Handler /* PendSV */
    .word SysTick_Handler
    .rept 67
    .word Default_Handler
    .endr
    .word OTG_FS_IRQHandler /* IRQ67 */
    .rept 11
    .word Default_Handler
    .endr
    .word 0 /* external IRQ79 reserved on F407 */
    .word Default_Handler /* RNG, IRQ80 */
    .word Default_Handler /* FPU, IRQ81 */
.section .text.Reset_Handler,"ax",%progbits
.global Reset_Handler
.type Reset_Handler,%function
.thumb_func
Reset_Handler:
    cpsid i
    ldr r0, =_estack
    msr msp, r0
    movs r0, #0
    msr control, r0
    msr basepri, r0
    msr faultmask, r0
    isb
    /* Disable SysTick and every external IRQ (the loader does too; cold reset
     * or a debugger start must not depend on it). */
    ldr r1, =0xe000e010
    str r0, [r1]
    ldr r1, =0xe000e180 /* NVIC ICER0..2: all 82 external IRQs */
    ldr r2, =0xe000e280 /* NVIC ICPR0..2 */
    mvns r3, r0
    .rept 3
    str r3, [r1], #4
    str r3, [r2], #4
    .endr
    ldr r1, =vectors
    ldr r2, =0xe000ed08
    str r1, [r2]
    dsb
    isb
    /* Clocks back to HSI, every loader peripheral reset. Stack only: RAM is
     * not initialized yet, so tb_flash_reset uses no .data/.bss. */
    bl tb_flash_reset
    movs r0, #0
    /* Initialized data: copy from its flash load address. */
    ldr r1, =_sdata
    ldr r2, =_edata
    ldr r3, =_sidata
6:  cmp r1, r2
    bcs 7f
    ldr r4, [r3], #4
    str r4, [r1], #4
    b 6b
7:  ldr r1, =_sbss
    ldr r2, =_ebss
1:  cmp r1, r2
    bcs 2f
    str r0, [r1], #4
    b 1b
2:  ldr r1, =_sdiag
    ldr r2, =_ediag
4:  cmp r1, r2
    bcs 5f
    str r0, [r1], #4
    b 4b
5:  bl usb_main
    b Default_Handler
.size Reset_Handler, .-Reset_Handler
.section .text.Default_Handler,"ax",%progbits
.global Default_Handler
.type Default_Handler,%function
.thumb_func
Default_Handler:
    ldr r0, =tb_status
    mrs r1, ipsr
    str r1, [r0, #12] /* fault exception number; do not resume */
    ldr r2, =0xe000ed28
    ldr r1, [r2]
    str r1, [r0, #96]
    ldr r1, [r2, #4]
    str r1, [r0, #100]
3:  b 3b
.size Default_Handler, .-Default_Handler
.section .note.GNU-stack,"",%progbits
