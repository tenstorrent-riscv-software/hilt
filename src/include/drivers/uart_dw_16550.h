/*
 * SPDX-License-Identifier: Apache-2.0
 * SPDX-FileCopyrightText: 2025-2026 Tenstorrent USA, Inc.
 */

#ifndef __DRIVERS_UART_DW_16550_H__
#define __DRIVERS_UART_DW_16550_H__

#include <types.h>
#include <drivers/uart_8250.h>

/*
 * Initialise a DesignWare DW_apb_uart at base for the given baud rate:
 * waits for the controller to go idle, programs the fractional divisor,
 * then applies the common 16550 setup (uart_8250_init). uart_tx() and
 * uart_transmit_flush() from uart_8250.h are used for I/O.
 */
int uart_dw_16550_init(unsigned long base, uint64_t clock_freq, unsigned int baud);

#endif /* __DRIVERS_UART_DW_16550_H__ */
