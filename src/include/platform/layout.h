/*
 * SPDX-License-Identifier: Apache-2.0
 * SPDX-FileCopyrightText: 2025-2026 Tenstorrent USA, Inc.
 *
 * Definitions of the layout of boot media and destination memory.
 */

#ifndef __LAYOUT_H__
#define __LAYOUT_H__

#define HILT_BOOT_NOR_OFFSET   0x10000
#define HILT_BOOT_LOAD_ADDR    0x20100000
#define HILT_BOOT_MAX_SIZE     0x40000

/* Offset of the settings map passed to the runtime, in the HILT boot image */
#define RCPU_BOOT_SETTINGS_OFFSET 0x400

#define PLATFORM_FW_NOR_OFFSET 0x50000
#define PLATFORM_FW_MAX_SIZE   0x80000

#endif /* __LAYOUT_H__ */
