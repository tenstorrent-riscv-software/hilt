/*
 * SPDX-License-Identifier: Apache-2.0
 * SPDX-FileCopyrightText: 2025-2026 Tenstorrent USA, Inc.
 */

#ifndef __MMIO_H__
#define __MMIO_H__

#include <types.h>
#include <barrier.h>

static inline uint8_t mmio_read8(uintptr_t addr)
{
    uint8_t ret;

    mb();
    ret = *(volatile uint8_t *)addr;
    mb();

    return ret;
}

static inline void mmio_write8(uintptr_t addr, uint8_t value)
{
    mb();
    *(volatile uint8_t *)addr = value;
    mb();
}

static inline uint16_t mmio_read16(uintptr_t addr)
{
    uint16_t ret;

    mb();
    ret = *(volatile uint16_t *)addr;
    mb();

    return ret;
}

static inline void mmio_write16(uintptr_t addr, uint16_t value)
{
    mb();
    *(volatile uint16_t *)addr = value;
    mb();
}

static inline uint32_t mmio_read32(uintptr_t addr)
{
    uint32_t ret;

    mb();
    ret = *(volatile uint32_t *)addr;
    mb();

    return ret;
}

static inline void mmio_write32(uintptr_t addr, uint32_t value)
{
    mb();
    *(volatile uint32_t *)addr = value;
    mb();
}

#endif /* __MMIO_H__ */
