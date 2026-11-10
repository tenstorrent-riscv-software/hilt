/*
 * SPDX-License-Identifier: Apache-2.0
 * SPDX-FileCopyrightText: 2025-2026 Tenstorrent USA, Inc.
 *
 * cpu_handoff(entry, arg0, arg1): transfer control to the next boot stage.
 *
 */

    .section .text.cpu_handoff, "ax", %progbits
    .globl  cpu_handoff
    .type   cpu_handoff, @function
cpu_handoff:
    /* Interrupts off; the next stage decides when to enable them. */
    csrci   mstatus, 0x8

    /* a0 = entry, a1 = arg0, a2 = arg1 */
    mv      t0, a0
    mv      a0, a1
    mv      a1, a2

    /* Make the loaded image visible to instruction fetch. */
    fence   rw, rw
    fence.i

    /* Hand over a clean register file, as the ZSBL does for us. */
    li      x1, 0
    li      x2, 0
    li      x3, 0
    li      x4, 0
    li      x6, 0
    li      x7, 0
    li      x8, 0
    li      x9, 0
    li      x12, 0
    li      x13, 0
    li      x14, 0
    li      x15, 0
    li      x16, 0
    li      x17, 0
    li      x18, 0
    li      x19, 0
    li      x20, 0
    li      x21, 0
    li      x22, 0
    li      x23, 0
    li      x24, 0
    li      x25, 0
    li      x26, 0
    li      x27, 0
    li      x28, 0
    li      x29, 0
    li      x30, 0
    li      x31, 0

    jr      t0

    /* Not reached: the next stage never returns. */
1:  wfi
    j       1b
    .size   cpu_handoff, . - cpu_handoff
