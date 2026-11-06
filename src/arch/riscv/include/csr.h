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

#define csr_read(csr)                                                   \
    ({                                                                  \
        unsigned long __v;                                              \
        asm volatile("csrr %0, %1" : "=r"(__v) : "i"(csr) : "memory"); \
        __v;                                                            \
    })

#define csr_write(csr, val)                                             \
    ({                                                                  \
        unsigned long __v = (unsigned long)(val);                       \
        asm volatile("csrw %1, %0" : : "r"(__v), "i"(csr) : "memory"); \
    })

#endif /* __CSR_H__ */
