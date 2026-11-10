/*
 * SPDX-License-Identifier: Apache-2.0
 * SPDX-FileCopyrightText: 2025-2026 Tenstorrent USA, Inc.
 */

#ifndef __CPU_H__
#define __CPU_H__

#include <types.h>

/* Jump to the next boot stage, never returns */
void cpu_handoff(uintptr_t entry, unsigned long arg0, unsigned long arg1)
    __attribute__((noreturn));

static inline void cpu_idle(void)
{
    asm volatile("wfi" ::: "memory");
}

static inline void cpu_relax(void)
{
    asm volatile("" ::: "memory");
}

#endif /* __CPU_H__ */
