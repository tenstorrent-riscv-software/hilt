/*
 * SPDX-License-Identifier: Apache-2.0
 * SPDX-FileCopyrightText: 2025-2026 Tenstorrent USA, Inc.
 */

#ifndef __CSR_H__
#define __CSR_H__

#ifndef __ASSEMBLER__

#define CSR_MHARTID             0xf14
#define CSR_CYCLE               0xc00
#define CSR_TIME                0xc01
#define CSR_CYCLEH              0xc80
#define CSR_TIMEH               0xc81

static inline __attribute__((always_inline)) unsigned long csr_read(int csr)
{
        register unsigned long value;

        /*
         * GCC orders asm volatile with respect to one another, so a
         * CSR read won't be moved ahead of a write, etc.
         */
        asm volatile ("csrr %[value], %[csr]"
                        : [value] "=r" (value)
                        : [csr] "i" (csr)
                        : "memory");

        return value;
}

static inline __attribute__((always_inline)) void csr_write(int csr, unsigned long value)
{
        asm volatile ("csrw %[csr], %[value]"
                        :
                        : [csr] "i" (csr),
                          [value] "rK"(value)
                        : "memory");
}

#endif /* !__ASSEMBLER__ */
#endif /* __CSR_H__ */
