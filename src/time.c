#include <types.h>
#include <time.h>

uint64_t timebase_frequency;

void time_init(uint64_t clock_frequency)
{
    timebase_frequency = clock_frequency;
}
