/*
 * SPDX-License-Identifier: Apache-2.0
 * SPDX-FileCopyrightText: 2025-2026 Tenstorrent USA, Inc.
 */

#ifndef __CSR_H__
#define __CSR_H__

#define CSR_CYCLE               0xc00
#define CSR_TIME                0xc01
#define CSR_CYCLEH              0xc80
#define CSR_TIMEH               0xc81

static inline unsigned long csr_read(unsigned int csr)
{
    unsigned long val;

    asm volatile("csrr %0, %1" : "=r"(val) : "i"(csr) : "memory");

    return val;
}

static inline void csr_write(unsigned int csr, unsigned long val)
{
    asm volatile("csrw %1, %0" : : "r"(val), "i"(csr) : "memory");
}

#endif /* __CSR_H__ */
