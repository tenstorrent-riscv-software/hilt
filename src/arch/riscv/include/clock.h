#ifndef __CLOCK_H__
#define __CLOCK_H__

#include <types.h>
#include <csr.h>

static inline uint64_t get_clock(void)
{
#if __riscv_xlen == 32
    uint32_t t;
    uint32_t time;
    uint32_t timeh;

    t = csr_read(CSR_CYCLEH);
    for (;;) {
        time = csr_read(CSR_CYCLE);
        timeh = csr_read(CSR_CYCLEH);
        if (timeh == t)
            break;
        t = timeh;
    }

    return ((uint64_t)timeh << 32) | time;
#elif __riscv_xlen == 64
    return csr_read(CSR_CYCLE);
#else
#error "Unsupported XLEN"
#endif
}

#endif /* __CLOCK_H__ */
