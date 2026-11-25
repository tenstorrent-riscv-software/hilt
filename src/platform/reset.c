/*
 * SPDX-License-Identifier: Apache-2.0
 * SPDX-FileCopyrightText: 2025-2026 Tenstorrent USA, Inc.
 */

#include <types.h>
#include <timer.h>
#include <platform/reset.h>
#include <platform/soc_mmr.h>

/*
 * A cleared bit holds the block in reset and a set bit takes it out of reset,
 * so the bits are cleared to assert the reset.
 */
static uint32_t reset_update(uint32_t reg, uint32_t mask, bool assert)
{
    return assert ? reg & ~mask : reg | mask;
}

static void reset_set(unsigned int reg, uint32_t mask, bool assert)
{
    soc_mmr_write(reg, reset_update(soc_mmr_read(reg), mask, assert));
}

void reset_bus_set(uint32_t mask, bool assert)
{
    reset_set(MMR_SOC_BUS_RSTN, mask, assert);
}

void reset_lsio_set(uint32_t mask, bool assert)
{
    reset_set(MMR_SOC_LSIO_BLK_RST, mask, assert);
}

void reset_rcpu_set(uint32_t mask, bool assert)
{
    reset_set(MMR_SOC_RCPU_BLK_RST, mask, assert);
}

void reset_aplic_set(bool assert)
{
    reset_set(MMR_SOC_APLIC_BLK_RST, SOC_APLIC_BLK_RST_APLIC, assert);
}

void reset_mm_set(uint32_t mask, bool assert)
{
    mm_mmr_write(MMR_MM_RSTN, reset_update(mm_mmr_read(MMR_MM_RSTN), mask, assert));
}

void reset_hsio_set(uint32_t mask, bool assert)
{
    hsio_mmr_write(MMR_HSIO_BLK_RST,
                   reset_update(hsio_mmr_read(MMR_HSIO_BLK_RST), mask, assert));
}

void reset_gmac_set(uint32_t mask, bool assert)
{
    hsio_mmr_write(MMR_HSIO_GMAC_CRG_CFG,
                   reset_update(hsio_mmr_read(MMR_HSIO_GMAC_CRG_CFG), mask, assert));
}

void reset_pcie_set(uint32_t mask, bool assert)
{
    pcie_mmr_write(MMR_PCIE_SUBS_RST_CTL,
                   reset_update(pcie_mmr_read(MMR_PCIE_SUBS_RST_CTL), mask, assert));
}

static void cpl_rstctrl_set(unsigned int reg, uint32_t mask, bool set)
{
    uint32_t val = cpl_rstctrl_mmr_read(reg);

    if (set) {
        val |= mask;
    } else {
        val &= ~mask;
    }
    cpl_rstctrl_mmr_write(reg, val);
}

/* The reference clock may only be forced a while after a reset is released. */
#define CPL_RESET_DELAY_US 1000

void reset_cpl_release(void)
{
    udelay(CPL_RESET_DELAY_US);

    /* Run all CPL clocks from the reference clock while the resets change. */
    cpl_rstctrl_set(MMR_CPL_RSTCTRL_WARM_RESET,
                    CPL_RSTCTRL_WARM_RESET_FORCE_SS_TO_REF_CLOCK_N, false);

    /* Assert the warm reset, then the cold reset. */
    cpl_rstctrl_set(MMR_CPL_RSTCTRL_WARM_RESET, CPL_RSTCTRL_WARM_RESET_CLUSTER_N, false);
    cpl_rstctrl_set(MMR_CPL_RSTCTRL_COLD_RESET, CPL_RSTCTRL_COLD_RESET_CLUSTER_N, false);

    /* Release them in the opposite order. */
    cpl_rstctrl_set(MMR_CPL_RSTCTRL_COLD_RESET, CPL_RSTCTRL_COLD_RESET_CLUSTER_N, true);
    cpl_rstctrl_set(MMR_CPL_RSTCTRL_WARM_RESET, CPL_RSTCTRL_WARM_RESET_CLUSTER_N, true);

    /* Switch the CPL clocks back to their own source. */
    cpl_rstctrl_set(MMR_CPL_RSTCTRL_WARM_RESET,
                    CPL_RSTCTRL_WARM_RESET_FORCE_SS_TO_REF_CLOCK_N, true);
}
