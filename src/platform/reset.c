/*
 * SPDX-License-Identifier: Apache-2.0
 * SPDX-FileCopyrightText: 2025-2026 Tenstorrent USA, Inc.
 */

#include <types.h>
#include <platform/reset.h>
#include <platform/soc_mmr.h>

/* A set bit takes the block out of reset. */
static void reset_release(unsigned int reg, uint32_t mask)
{
    soc_mmr_write(reg, soc_mmr_read(reg) | mask);
}

void reset_bus_release(uint32_t mask)
{
    reset_release(MMR_SOC_BUS_RSTN, mask);
}

void reset_lsio_release(uint32_t mask)
{
    reset_release(MMR_SOC_LSIO_BLK_RST, mask);
}

void reset_rcpu_release(uint32_t mask)
{
    reset_release(MMR_SOC_RCPU_BLK_RST, mask);
}

void reset_aplic_release(void)
{
    reset_release(MMR_SOC_APLIC_BLK_RST, SOC_APLIC_BLK_RST_APLIC);
}

void reset_mm_release(uint32_t mask)
{
    mm_mmr_write(MMR_MM_RSTN, mm_mmr_read(MMR_MM_RSTN) | mask);
}

void reset_hsio_release(uint32_t mask)
{
    hsio_mmr_write(MMR_HSIO_BLK_RST, hsio_mmr_read(MMR_HSIO_BLK_RST) | mask);
}

void reset_gmac_release(uint32_t mask)
{
    hsio_mmr_write(MMR_HSIO_GMAC_CRG_CFG,
                   hsio_mmr_read(MMR_HSIO_GMAC_CRG_CFG) | mask);
}

void reset_pcie_release(uint32_t mask)
{
    pcie_mmr_write(MMR_PCIE_SUBS_RST_CTL, pcie_mmr_read(MMR_PCIE_SUBS_RST_CTL) | mask);
}
