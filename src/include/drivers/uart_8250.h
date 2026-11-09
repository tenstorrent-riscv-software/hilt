/*
 * SPDX-License-Identifier: Apache-2.0
 * SPDX-FileCopyrightText: 2025-2026 Tenstorrent USA, Inc.
 */

#ifndef __DRIVERS_UART_8250_H__
#define __DRIVERS_UART_8250_H__

#include <types.h>

/* Register stride in bytes: the 8250 registers are byte wide, but the
 * instances we drive place them on 32-bit boundaries. */
#define UART_REG_STRIDE 4u
#define UART_REG(n)     ((n) * UART_REG_STRIDE)

#define UART_THR    UART_REG(0) /* transmitter holding register */
#define UART_IER    UART_REG(1) /* interrupt enable register */
#define UART_FCR    UART_REG(2) /* FIFO control register */
#define UART_LCR    UART_REG(3) /* line control register */
#define UART_MCR    UART_REG(4) /* modem control register */
#define UART_LSR    UART_REG(5) /* line status register */
#define UART_BRDL   UART_REG(0) /* baud rate divisor (low), when LCR.DLAB=1 */
#define UART_BRDH   UART_REG(1) /* baud rate divisor (high), when LCR.DLAB=1 */

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

#define UART_LSR_THRE (1U << 5) /* Transmitter holding register empty */
#define UART_LSR_TEMT (1U << 6) /* Transmitter empty */

/* Bound on every register poll in the 8250-family drivers */
#define UART_POLL_TIMEOUT_US 100000

/*
 * Basic 16550 setup with a precomputed baud divisor: interrupts off,
 * divisor latched, 8N1, FIFOs enabled, auto flow control on.
 * Variants with extra registers (e.g. DesignWare) do their own work
 * around this.
 */
void uart_8250_init(unsigned long base, uint16_t divisor);

/* Blocking transmit of one byte; dropped if the FIFO does not drain in time */
void uart_tx(unsigned long base, uint8_t byte);

/* Wait for the transmitter to go idle */
int uart_transmit_flush(unsigned long base);

#endif /* __DRIVERS_UART_8250_H__ */
