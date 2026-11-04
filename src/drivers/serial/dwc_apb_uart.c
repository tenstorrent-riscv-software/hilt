/*
 * SPDX-License-Identifier: Apache-2.0
 * SPDX-FileCopyrightText: 2025-2026 Tenstorrent USA, Inc.
 *
 * Synopsys DW_apb_uart: a 16550 with 32-bit register stride and DesignWare
 * extensions.
 */

#include <types.h>
#include <errno.h>
#include <mmio.h>
#include <time.h>
#include <drivers/dwc_apb_uart.h>

#define UART_THR    0x00u /* transmitter holding register */
#define UART_IER    0x04u /* interrupt enable register */
#define UART_FCR    0x08u /* FIFO control register */
#define UART_LCR    0x0cu /* line control register */
#define UART_MCR    0x10u /* modem control register */
#define UART_LSR    0x14u /* line status register */
#define UART_BRDL   0x00u /* baud rate divisor (low) */
#define UART_BRDH   0x04u /* baud rate divisor (high) */

#define UART_DW_USR 0x7cu /* DW UART status register   */
#define UART_DW_DLF 0xc0u /* DW divisor latch fraction    */

#define UART_FCR_FE     (1U << 0) /* FIFO Enable */
#define UART_FCR_RFR    (1U << 1) /* RCVR FIFO Reset */
#define UART_FCR_XFR    (1U << 2) /* XMIT FIFO Reset */
#define UART_FCR_DMS    (1U << 3) /* DMA Mode Select */
#define UART_FCR_FTL_1  (0U << 6) /* 1 byte fifo trigger levels   */
#define UART_FCR_FTL_4  (1U << 6) /* 4 bytes fifo trigger levels  */
#define UART_FCR_FTL_8  (2U << 6) /* 8 bytes fifo trigger levels  */
#define UART_FCR_FTL_14 (3U << 6) /* 14 bytes fifo trigger levels */

#define UART_LCR_CS5   (0U << 0) /* 5 bits data size */
#define UART_LCR_CS6   (1U << 0) /* 6 bits data size */
#define UART_LCR_CS7   (2U << 0) /* 7 bits data size */
#define UART_LCR_CS8   (3U << 0) /* 8 bits data size */
#define UART_LCR_1_STB (0U << 2) /* 1 stop bit  */
#define UART_LCR_2_STB (1U << 2) /* 2 stop bits */
#define UART_LCR_PDIS  (0U << 3) /* Parity disable */
#define UART_LCR_PEN   (1U << 3) /* Parity enable  */
#define UART_LCR_EPS   (1U << 4) /* Even parity select  */
#define UART_LCR_SP    (1U << 5) /* Stick parity select */
#define UART_LCR_SBRK  (1U << 6) /* Break control bit   */
#define UART_LCR_DLAB  (1U << 7) /* Divisor latch access enable */

#define UART_MCR_DTR  (1U << 0) /* DTR output */
#define UART_MCR_RTS  (1U << 1) /* RTS output */
#define UART_MCR_OUT1 (1U << 2) /* Output #1  */
#define UART_MCR_OUT2 (1U << 3) /* Output #2  */
#define UART_MCR_LOOP (1U << 4) /* Loop back  */
#define UART_MCR_AFCE (1U << 5) /* Auto flow control enable */

#define UART_LSR_THRE (1u << 5)
#define UART_LSR_TEMT (1u << 6)

#define UART_DW_USR_BUSY (1U << 0) /* DW UART busy */

#define UART_POLL_TIMEOUT_US 100000

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

static int dw_uart_wait_busy(unsigned long base)
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

int uart_init(unsigned long base, unsigned long clock_freq, unsigned int baud)
{
    uint8_t dlf_bits;
    unsigned long ratio;
    uint16_t divisor;
    uint8_t fract;
    uint64_t timeout;
    int rc;

    /*
     * Designware controller requires UART busy to clear otherwise
     * UART_LCR and divisor writes can be ignored.
     */
    uart_transmit_flush(base);

    /* Disable interrupts */
    mmio_write8(base + UART_IER, 0x00);

    /* Detect and set DesignWare DLF */
    rc = dw_uart_wait_busy(base);
    if (rc) {
        return rc;
    }
    mmio_write8(base + UART_DW_DLF, 0xff);
    dlf_bits = mmio_read8(base + UART_DW_DLF);

    /* Use ~round-nearest division instead of truncating */
    if (dlf_bits == 0x3f) {
        /* 6-bit fractional divisor case */
        ratio   = (((uint64_t)clock_freq << 2) + (baud / 2)) / baud;
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

    timeout_start(&timeout, UART_POLL_TIMEOUT_US);
    for (;;) {
        /* Wait for UART Busy */
        rc = dw_uart_wait_busy(base);
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

    /* Divisor changes */
    mmio_write8(base + UART_DW_DLF, fract);
    mmio_write8(base + UART_BRDL, (uint8_t)divisor);
    mmio_write8(base + UART_BRDH, (divisor >> 8) & 0xFF);

    mmio_write8(base + UART_LCR, UART_LCR_CS8 | UART_LCR_1_STB | UART_LCR_PDIS);
    mmio_write8(base + UART_FCR, UART_FCR_FE | UART_FCR_RFR | UART_FCR_XFR | UART_FCR_FTL_14);
    mmio_write8(base + UART_MCR, UART_MCR_AFCE);

    return 0;
}
