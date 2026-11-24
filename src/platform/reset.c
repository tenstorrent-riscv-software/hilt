/*
 * SPDX-License-Identifier: Apache-2.0
 * SPDX-FileCopyrightText: 2025-2026 Tenstorrent USA, Inc.
 */

#include <types.h>
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
