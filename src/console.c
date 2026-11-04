/*
 * SPDX-License-Identifier: Apache-2.0
 * SPDX-FileCopyrightText: 2025-2026 Tenstorrent USA, Inc.
 *
 * Console glue: picolibc stdout and stderr on top of the UART driver.
 */

#include <types.h>
#include <stdio.h>
#include <console.h>
#include <drivers/uart_8250.h>

static unsigned long console_uart_base;

static int console_putc(char c, FILE *file)
{
    (void)file;
    if (c == '\n')
        uart_tx(console_uart_base, '\r');
    uart_tx(console_uart_base, (uint8_t)c);

    return (unsigned char)c;
}

static int console_flush(FILE *file)
{
    (void)file;
    if (uart_transmit_flush(console_uart_base))
        return EOF;

    return 0;
}

static FILE console = FDEV_SETUP_STREAM(console_putc, NULL, console_flush, _FDEV_SETUP_WRITE);

FILE *const stdout = &console;
FILE *const stderr = &console;

void console_init(unsigned long uart_base)
{
    console_uart_base = uart_base;
}
