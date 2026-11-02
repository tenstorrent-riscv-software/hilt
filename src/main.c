/*
 * SPDX-License-Identifier: Apache-2.0
 * SPDX-FileCopyrightText: 2025-2026 Tenstorrent USA, Inc.
 */

#include <stdio.h>
#include <cpu.h>
#include <platform/atlantis.h>

extern const char g_build_banner[];

int main(void)
{
    bool is_bootrom;
    const char *target = "SRAM";

    platform_atlantis_init();

    is_bootrom = platform_atlantis_is_bootrom();
    if (is_bootrom) {
        target = "bootrom";
    }
    printf("%s", g_build_banner);
    printf("HILT %s\n", target);

    printf("HILT: complete\n");
    for (;;)
        cpu_idle();
}
