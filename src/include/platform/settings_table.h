/*
 * SPDX-License-Identifier: Apache-2.0
 * SPDX-FileCopyrightText: 2025-2026 Tenstorrent USA, Inc.
 *
 * Settings the HILT boot stage leaves in RCPU SRAM for the runtime stage.
 * The address of the map is passed in a1 on handoff.
 */

#ifndef __SETTINGS_TABLE_H__
#define __SETTINGS_TABLE_H__

#include <stdint.h>

#define PLATFORM_UNKNOWN   0u
#define PLATFORM_SILICON   1u /* silicon or QEMU */
#define PLATFORM_HAPS_BOOT 2u /* HAPS boot model */
#define PLATFORM_HAPS_PCIE 3u /* HAPS PCIe model */

struct settings_table_init {
    uint8_t platform;   /* Offset 0x0 */
    uint32_t baud;      /* Offset 0x4 */
    uint32_t frequency; /* Offset 0x8 */
};

#endif /* __SETTINGS_TABLE_H__ */
