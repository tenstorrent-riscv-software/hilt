#ifndef __CPU_H__
#define __CPU_H__

static inline void cpu_idle(void)
{
    asm volatile("wfi" ::: "memory");
}

static inline void cpu_relax(void)
{
    asm volatile("" ::: "memory");
}

#endif /* __CPU_H__ */
