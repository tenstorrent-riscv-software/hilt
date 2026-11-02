/*
 * SPDX-License-Identifier: Apache-2.0
 * SPDX-FileCopyrightText: 2025-2026 Tenstorrent USA, Inc.
 */

#ifndef __PLATFORM_ATLANTIS_H__
#define __PLATFORM_ATLANTIS_H__

int platform_atlantis_init(void);

/* True when running from the mask ROM as the bootrom, false when loaded into SRAM */
bool platform_atlantis_is_bootrom(void);

#endif /* __PLATFORM_ATLANTIS_H__ */
