/*
 * SPDX-License-Identifier: Apache-2.0
 * SPDX-FileCopyrightText: 2025-2026 Tenstorrent USA, Inc.
 */

#ifndef __BARRIER_H__
#define __BARRIER_H__

#define RISCV_FENCE_ASM(p, s)           "\tfence " #p "," #s "\n"
#define RISCV_FENCE(p, s) \
        ({ __asm__ __volatile__ (RISCV_FENCE_ASM(p, s) : : : "memory"); })

#define mb()    RISCV_FENCE(iorw, iorw)

#endif /* __BARRIER_H__ */
