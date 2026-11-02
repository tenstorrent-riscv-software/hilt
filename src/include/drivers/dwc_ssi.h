/*
 * SPDX-License-Identifier: Apache-2.0
 * SPDX-FileCopyrightText: 2025-2026 Tenstorrent USA, Inc.
 */

#ifndef __DRIVERS_DWC_SSI_H__
#define __DRIVERS_DWC_SSI_H__

#include <types.h>

struct dwc_ssi {
    uintptr_t base;
    unsigned int fifo_depth; /* entries in each of the TX and RX FIFOs */
    uint32_t baudr;          /* serial clock divider, even, >= 2 */
    uint32_t sclk_hz;        /* resulting serial clock */
};

struct dwc_ssi_cmd {
    uint8_t opcode;
    uint8_t addr_bytes; /* 0, 3, or 4 */
    uint32_t addr;
    uint8_t *rx;
    size_t rx_len;
};

/* One-time setup of the spi controller */
int dwc_ssi_init(struct dwc_ssi *ssi, uintptr_t base, uint32_t ref_hz,
                 uint32_t max_sclk_hz, unsigned int fifo_depth);

int dwc_ssi_cmd(const struct dwc_ssi *ssi, const struct dwc_ssi_cmd *cmd);

/*  Shutdown the controller for handoff to the next stage */
int dwc_ssi_shutdown(const struct dwc_ssi *ssi);

uint32_t dwc_ssi_version(const struct dwc_ssi *ssi);

#endif /* __DRIVERS_DWC_SSI_H__ */
