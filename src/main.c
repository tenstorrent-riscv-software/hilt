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
    platform_atlantis_init();

    printf("%s", g_build_banner);

    printf("HILT: complete\n");
    for (;;)
        cpu_idle();
}
