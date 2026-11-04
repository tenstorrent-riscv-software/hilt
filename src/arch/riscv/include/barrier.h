/*
 * SPDX-License-Identifier: Apache-2.0
 * SPDX-FileCopyrightText: 2025-2026 Tenstorrent USA, Inc.
 */

#ifndef __BARRIER_H__
#define __BARRIER_H__

#ifndef __ASSEMBLER__

/* Bits of the fence predecessor/successor sets, as encoded in the instruction. */
enum riscv_fence_set {
    FENCE_W    = 1 << 0,
    FENCE_R    = 1 << 1,
    FENCE_O    = 1 << 2,
    FENCE_I    = 1 << 3,
    FENCE_RW   = FENCE_R | FENCE_W,
    FENCE_IORW = FENCE_I | FENCE_O | FENCE_RW,
};

static inline __attribute__((always_inline)) void riscv_fence(enum riscv_fence_set pred,
                                                              enum riscv_fence_set succ)
{
    __asm__ __volatile__ (".insn i MISC_MEM, 0, x0, x0, %0"
                          : : "i"((pred << 4) | succ) : "memory");
}

static inline __attribute__((always_inline)) void mb(void)
{
    riscv_fence(FENCE_IORW, FENCE_IORW);
}

#endif /* !__ASSEMBLER__ */
#endif /* __BARRIER_H__ */
