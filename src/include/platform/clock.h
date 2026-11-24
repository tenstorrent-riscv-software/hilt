/*
 * SPDX-License-Identifier: Apache-2.0
 * SPDX-FileCopyrightText: 2025-2026 Tenstorrent USA, Inc.
 *
 * Clock rates on the Atlantis SoC, in Hz, given the frequency of the
 * oscillator it runs from.
 */

#ifndef __PLATFORM_CLOCK_H__
#define __PLATFORM_CLOCK_H__

#include <types.h>

/* Release clock gating for different subsystems */
void clock_lsio_set_cg_enable(uint32_t mask, bool enable);

void clock_bus_set_cg_enable(uint32_t mask, bool enable);

void clock_rcpu_set_cg_enable(uint32_t mask, bool enable);

void clock_mm_set_cg_enable(uint32_t mask, bool enable);

void clock_hsio_set_cg_enable(uint32_t mask, bool enable);

void clock_gmac_set_cg_enable(uint32_t mask, bool enable);

void clock_pcie_set_cg_enable(uint32_t mask, bool enable);

/* Reference clock of the UARTs, the NOCC clock */
uint64_t uart_clock_frequency(uint32_t osc_frequency);

/* Reference clock of the QSPI controller: the NOCC clock */
uint64_t qspi_clock_frequency(uint32_t osc_frequency);

/* Clock the RCPU runs at, which its cycle counter counts */
uint64_t rcpu_clock_frequency(uint32_t osc_frequency);


#endif /* __PLATFORM_CLOCK_H__ */
