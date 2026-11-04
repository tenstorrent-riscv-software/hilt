/*
 * SPDX-License-Identifier: Apache-2.0
 * SPDX-FileCopyrightText: 2025-2026 Tenstorrent USA, Inc.
 */

#ifndef __CPU_H__
#define __CPU_H__

static inline void cpu_idle(void)
{
    asm volatile("wfi" ::: "memory");
}

static inline void cpu_relax(void)
{
    asm volatile("" ::: "memory");
}

#endif /* __CPU_H__ */
