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

int clock_lpddr_init(void);

int clock_mm_init(void);

int clock_hsio_init(void);

int clock_pcie_init(void);

/* Enable the RCPU PLL and run the RCPU off it */
int clock_rcpu_init(uint32_t osc_frequency, bool haps);

/* Enable the NOCC PLL and run the NOCC off it.*/
int clock_nocc_init(void);

/* Enable the PLLs of the Ascalon subsystem. */
int clock_asc_init(void);

/* Bring up all devices. This should be done after PLL's are enabled. */
int clock_device_init(void);

/*
 * HAPs models have no PLL. Even if the PLL is enabled and selected they will
 * run off the osc_frequency.
 */

/* Reference clock of the UARTs, the NOCC clock */
uint64_t uart_clock_frequency(uint32_t osc_frequency, bool haps);

/* Reference clock of the QSPI controller: the NOCC clock */
uint64_t qspi_clock_frequency(uint32_t osc_frequency, bool haps);

/* The RCPU can run in bypass off the oscillator or off the PLL at 600MHz */
uint64_t rcpu_clock_frequency(uint32_t osc_frequency, bool haps);


#endif /* __PLATFORM_CLOCK_H__ */
