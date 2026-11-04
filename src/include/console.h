/*
 * SPDX-License-Identifier: Apache-2.0
 * SPDX-FileCopyrightText: 2025-2026 Tenstorrent USA, Inc.
 */

#ifndef __CONSOLE_H__
#define __CONSOLE_H__

/* Route stdout and stderr to an initialized UART at uart_base */
void console_init(unsigned long uart_base);

#endif /* __CONSOLE_H__ */
