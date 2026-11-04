/*
 * SPDX-License-Identifier: Apache-2.0
 * SPDX-FileCopyrightText: 2025-2026 Tenstorrent USA, Inc.
 */

#ifndef __DRIVERS_DWC_APB_UART_H__
#define __DRIVERS_DWC_APB_UART_H__

#include <types.h>

int uart_init(unsigned long base, unsigned long clock_freq, unsigned int baud);

/* Blocking transmit of one byte; dropped if the FIFO does not drain in time */
void uart_tx(unsigned long base, uint8_t byte);

/* Wait for the transmitter to go idle */
int uart_transmit_flush(unsigned long base);

#endif /* __DRIVERS_DWC_APB_UART_H__ */
