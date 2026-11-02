/*
 * SPDX-License-Identifier: Apache-2.0
 * SPDX-FileCopyrightText: 2025-2026 Tenstorrent USA, Inc.
 */

#include <stdio.h>
#include <cpu.h>
#include <image/boot_image.h>
#include <platform/atlantis.h>

extern const char g_build_banner[];

int main(void)
{
    const struct boot_image *images;
    size_t count;
    bool is_bootrom;
    const char *target = "SRAM";

    platform_atlantis_init();

    is_bootrom = platform_atlantis_is_bootrom();
    if (is_bootrom) {
        target = "bootrom";
    }
    printf("%s", g_build_banner);
    printf("HILT %s\n", target);

    images = platform_atlantis_boot_images(&count);
    for (size_t i = 0; i < count; i++) {
        printf("HILT: image %s: load 0x%08x, NOR 0x%x\n",
               images[i].name, (unsigned)images[i].load_addr,
               (unsigned)images[i].nor_offset);
    }

    printf("HILT: complete\n");
    for (;;)
        cpu_idle();
}
