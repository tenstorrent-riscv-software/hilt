/*
 * SPDX-License-Identifier: Apache-2.0
 * SPDX-FileCopyrightText: 2025-2026 Tenstorrent USA, Inc.
 */

#ifndef __PLATFORM_ATLANTIS_H__
#define __PLATFORM_ATLANTIS_H__

#include <types.h>

struct boot_image;

int platform_atlantis_init(void);

/* True when running from the mask ROM as the bootrom, false when loaded into SRAM */
bool platform_atlantis_is_bootrom(void);

/* The firmware pieces this stage boots, in load order. */
const struct boot_image *platform_atlantis_boot_images(size_t *count);

int platform_atlantis_load_boot_image(const struct boot_image *image);

const char *platform_atlantis_boot_mode_name(void);
const char *platform_atlantis_boot_cause_name(void);

#endif /* __PLATFORM_ATLANTIS_H__ */
