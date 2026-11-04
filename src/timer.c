/*
 * SPDX-License-Identifier: Apache-2.0
 * SPDX-FileCopyrightText: 2025-2026 Tenstorrent USA, Inc.
 */

#include <types.h>
#include <timer.h>

uint64_t timebase_frequency;

void time_init(uint64_t clock_frequency)
{
    timebase_frequency = clock_frequency;
}

void timeout_start(uint64_t *timeout, uint64_t us)
{
    *timeout = get_clock() + us * timebase_frequency / 1000000;
}

bool timeout_expired(const uint64_t *timeout)
{
    return (int64_t)(get_clock() - *timeout) >= 0;
}
