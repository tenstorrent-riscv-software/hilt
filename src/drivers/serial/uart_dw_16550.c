/*
 * SPDX-License-Identifier: Apache-2.0
 * SPDX-FileCopyrightText: 2025-2026 Tenstorrent USA, Inc.
 *
 * Synopsys DW_apb_uart: a 16550 with 32-bit register stride and DesignWare
 * extensions (busy status, fractional baud divisor). The common 16550
 * setup and I/O live in uart_8250.c.
 */

#include <types.h>
#include <errno.h>
#include <mmio.h>
#include <timer.h>
#include <drivers/uart_8250.h>
#include <drivers/uart_dw_16550.h>

#define UART_DW_USR 0x7cu /* DW UART status register   */
#define UART_DW_DLF 0xc0u /* DW divisor latch fraction */

#define UART_DW_USR_BUSY (1U << 0) /* DW UART busy */

static int wait_busy(unsigned long base)
{
    uint64_t timeout;

    timeout_start(&timeout, UART_POLL_TIMEOUT_US);
    while (mmio_read8(base + UART_DW_USR) & UART_DW_USR_BUSY) {
        if (timeout_expired(&timeout)) {
            return -ETIMEDOUT;
        }
    }

    return 0;
}

int uart_dw_16550_init(unsigned long base, uint64_t clock_freq, unsigned int baud)
{
    uint8_t dlf_bits;
    uint64_t ratio;
    uint16_t divisor;
    uint8_t fract;
    uint64_t timeout;
    int rc;

    if (baud == 0) {
        return -EINVAL;
    }

    /*
     * Designware controller requires UART busy to clear otherwise
     * UART_LCR and divisor writes can be ignored.
     */
    uart_transmit_flush(base);

    /* Detect and set DesignWare DLF */
    rc = wait_busy(base);
    if (rc) {
        return rc;
    }
    mmio_write8(base + UART_DW_DLF, 0xff);
    dlf_bits = mmio_read8(base + UART_DW_DLF);

    /* Use ~round-nearest division instead of truncating */
    if (dlf_bits == 0x3f) {
        /* 6-bit fractional divisor case */
        ratio   = ((clock_freq << 2) + (baud / 2)) / baud;
        divisor = ratio >> 6;
        fract   = ratio & 0x3f;
    } else {
        ratio = (clock_freq + (baud / 2)) / baud;
        if (dlf_bits == 0xf) {
            divisor = ratio >> 4;
            fract   = ratio & 0xf;
        } else {
            divisor = (ratio + 8) >> 4;
            fract   = 0;
        }
    }

    /* Designware has a busy state, check it before the LCR writes */
    timeout_start(&timeout, UART_POLL_TIMEOUT_US);
    for (;;) {
        /* Wait for UART Busy */
        rc = wait_busy(base);
        if (rc) {
            return rc;
        }
        /* Divisor latch access */
        mmio_write8(base + UART_LCR, UART_LCR_DLAB);
        /* Check that the write stuck */
        if (mmio_read8(base + UART_LCR) == UART_LCR_DLAB) {
            break;
        }
        if (timeout_expired(&timeout)) {
            return -ETIMEDOUT;
        }
    }

    /* Fractional divisor, then the common 16550 setup programs the rest */
    mmio_write8(base + UART_DW_DLF, fract);
    uart_8250_init(base, divisor);

    return 0;
}
