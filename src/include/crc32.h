/*
 * SPDX-License-Identifier: Apache-2.0
 * SPDX-FileCopyrightText: 2025-2026 Tenstorrent USA, Inc.
 */

#ifndef __CRC32_H__
#define __CRC32_H__

#include <types.h>

uint32_t crc32_update(uint32_t crc, const uint8_t *data, size_t len);

static inline uint32_t crc32_calc(const uint8_t *data, size_t len)
{
    return crc32_update(0xffffffffu, data, len) ^ 0xffffffffu;
}

#endif /* __CRC32_H__ */
