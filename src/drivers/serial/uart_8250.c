/*
 * SPDX-License-Identifier: Apache-2.0
 * SPDX-FileCopyrightText: 2025-2026 Tenstorrent USA, Inc.
 *
 * Generic 8250/16550 UART: polled transmit and basic line setup.
 */

#include <types.h>
#include <errno.h>
#include <mmio.h>
#include <timer.h>
#include <drivers/uart_8250.h>

static int uart_wait_lsr(unsigned long base, uint8_t mask)
{
    uint64_t timeout;

    timeout_start(&timeout, UART_POLL_TIMEOUT_US);
    while (!(mmio_read8(base + UART_LSR) & mask)) {
        if (timeout_expired(&timeout)) {
            return -ETIMEDOUT;
        }
    }

    return 0;
}

void uart_tx(unsigned long base, uint8_t byte)
{
    if (uart_wait_lsr(base, UART_LSR_THRE)) {
        return;
    }
    mmio_write8(base + UART_THR, byte);
}

int uart_transmit_flush(unsigned long base)
{
    return uart_wait_lsr(base, UART_LSR_TEMT);
}

void uart_8250_init(unsigned long base, uint16_t divisor)
{
    const uint8_t lcr = UART_LCR_CS8 | UART_LCR_1_STB | UART_LCR_PDIS;

    mmio_write8(base + UART_LCR, lcr);

    /* Disable interrupts */
    mmio_write8(base + UART_IER, 0x00);

    /* Divisor latch access */
    mmio_write8(base + UART_LCR, lcr | UART_LCR_DLAB);
    mmio_write8(base + UART_BRDL, (uint8_t)divisor);
    mmio_write8(base + UART_BRDH, (divisor >> 8) & 0xFF);
    mmio_write8(base + UART_LCR, lcr);

    mmio_write8(base + UART_FCR, UART_FCR_FE | UART_FCR_RFR | UART_FCR_XFR | UART_FCR_FTL_14);
    mmio_write8(base + UART_MCR, UART_MCR_AFCE);
}
