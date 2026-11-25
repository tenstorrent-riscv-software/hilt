/*
 * SPDX-License-Identifier: Apache-2.0
 * SPDX-FileCopyrightText: 2025-2026 Tenstorrent USA, Inc.
 *
 * Helper functions for accessing reset control on Atlantis. The bus reset is
 * the top level reset and must be released before trying to access some
 * subsystems; mm, hsio, gmac. The rcpu and lsio are not held in reset on boot
 * and do not need to be released from the bus reset.
 */

#ifndef __PLATFORM_RESET_H__
#define __PLATFORM_RESET_H__

#include <types.h>

/* Bus resets, SOC_BUS_RSTN_* */
void reset_bus_release(uint32_t mask);

/* LSIO peripheral resets, SOC_LSIO_BLK_RST_* */
void reset_lsio_release(uint32_t mask);

/* RCPU peripheral resets, SOC_RCPU_BLK_RST_* */
void reset_rcpu_release(uint32_t mask);

/* The APLIC reset */
void reset_aplic_release(void);

/*
 * The mm, hsio, and gmc registers are behind the mm and hsio buses. The bus
 * reset and clocking must be started before accessing tehse register spaces.
 */

/* MM block resets, MM_RSTN_* */
void reset_mm_release(uint32_t mask);

/* HSIO block resets, HSIO_BLK_RST_* */
void reset_hsio_release(uint32_t mask);

/* GMAC resets, HSIO_GMAC_CRG_CFG_GMAC*_XRSTN */
void reset_gmac_release(uint32_t mask);

/* PCIe subsystem resets, PCIE_SUBS_RST_CTL_* */
void reset_pcie_release(uint32_t mask);

#endif /* __PLATFORM_RESET_H__ */
