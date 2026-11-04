/*
 * SPDX-License-Identifier: Apache-2.0
 * SPDX-FileCopyrightText: 2025-2026 Tenstorrent USA, Inc.
 */

#ifndef __TIMER_H__
#define __TIMER_H__

#include <types.h>
#include <cpu.h>
#include <clock.h>

static inline void delay(uint64_t ticks)
{
    uint64_t t0 = get_clock();

    while (get_clock() - t0 < ticks)
        cpu_relax();
}

extern uint64_t timebase_frequency;

static inline void udelay(uint64_t us)
{
    delay(us * timebase_frequency / 1000000);
}

void time_init(uint64_t clock_frequency);

void timeout_start(uint64_t *timeout, uint64_t us);
bool timeout_expired(const uint64_t *timeout);

#endif /* __TIMER_H__ */
