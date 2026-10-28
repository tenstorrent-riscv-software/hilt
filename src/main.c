#include <stdio.h>
#include <cpu.h>
#include <platform/atlantis.h>

extern const char g_build_banner[];

int main(void)
{
    platform_atlantis_init();

    printf("%s", g_build_banner);
    printf("HILT: hello, world\n");

    for (;;)
        cpu_idle();
}
