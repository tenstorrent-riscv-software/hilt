/*
 * HILT entry point.
 *
 * HILT entry point for bootrom and SRAM images. For SRAM images VMA
 * and LMA's are the same.
 */

    .section .text._start, "ax", %progbits
    .globl  _start
    .type   _start, @function
_start:
    /* Interrupts stay off; park traps on our own spinner instead of the
       ZSBL's stale vector until a real handler exists. */
    csrci   mstatus, 0x8
    la      t0, trap_spin
    csrw    mtvec, t0

    /* Enable the FPU: the ilp32d ABI permits FP code anywhere. */
    li      t0, 0x2000              /* mstatus.FS = Initial */
    csrs    mstatus, t0
    csrw    fcsr, zero

    .option push
    .option norelax
    la      gp, __global_pointer$
    .option pop
    la      sp, __stack_top

    /* Load data from LMA to VMA */
    la      t0, __data_start
    la      t1, __data_end
    la      t2, __data_lma
0:  bgeu    t0, t1, 1f
    lw      t3, 0(t2)
    sw      t3, 0(t0)
    addi    t0, t0, 4
    addi    t2, t2, 4
    j       0b
1:

    /* Zero .bss */
    la      t0, __bss_start
    la      t1, __bss_end
0:  bgeu    t0, t1, 1f
    sw      zero, 0(t0)
    addi    t0, t0, 4
    j       0b
1:
    call    main

    /* main must not return; if it does, park. */
hang:
    wfi
    j       hang

    .balign 4
trap_spin:
    j       trap_spin
    .size   _start, . - _start
