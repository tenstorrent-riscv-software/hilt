/*
 * SPDX-License-Identifier: Apache-2.0
 * SPDX-FileCopyrightText: 2025-2026 Tenstorrent USA, Inc.
 */

#include <types.h>
#include <mmio.h>
#include <timer.h>
#include <platform/clock.h>
#include <platform/soc_mmr.h>

/* A set bit opens the gate, i.e. the clock runs. */
static void clock_set_cg_enable(uintptr_t addr, uint32_t mask, bool enable)
{
    uint32_t reg = mmio_read32(addr);

    if (enable) {
        reg |= mask;
    } else {
        reg &= ~mask;
    }
    mmio_write32(addr, reg);
    udelay(CLOCK_GATE_DELAY_US);
}

void clock_lsio_set_cg_enable(uint32_t mask, bool enable)
{
    clock_set_cg_enable(SOC_MMR_BASE + MMR_SOC_LSIO_BLK_CG, mask, enable);
}

void clock_bus_set_cg_enable(uint32_t mask, bool enable)
{
    clock_set_cg_enable(SOC_MMR_BASE + MMR_SOC_BUS_CG, mask, enable);
}

void clock_rcpu_set_cg_enable(uint32_t mask, bool enable)
{
    clock_set_cg_enable(SOC_MMR_BASE + MMR_SOC_RCPU_BLK_CG, mask, enable);
}

void clock_mm_set_cg_enable(uint32_t mask, bool enable)
{
    clock_set_cg_enable(MM_CTRL_BASE + MMR_MM_CLK_GATE_CFG, mask, enable);
}

void clock_hsio_set_cg_enable(uint32_t mask, bool enable)
{
    clock_set_cg_enable(HSIO_CTRL_BASE + MMR_HSIO_BLK_CG, mask, enable);
}

void clock_gmac_set_cg_enable(uint32_t mask, bool enable)
{
    clock_set_cg_enable(HSIO_CTRL_BASE + MMR_HSIO_GMAC_CRG_CFG, mask, enable);
}

void clock_pcie_set_cg_enable(uint32_t mask, bool enable)
{
    clock_set_cg_enable(PCIE_MMR_BASE + MMR_PCIE_SUBS_CG_CTL, mask, enable);
}

uint64_t uart_clock_frequency(uint32_t osc_frequency)
{
    return osc_frequency;
}

uint64_t qspi_clock_frequency(uint32_t osc_frequency)
{
    return osc_frequency;
}

uint64_t rcpu_clock_frequency(uint32_t osc_frequency)
{
    return osc_frequency;
}
