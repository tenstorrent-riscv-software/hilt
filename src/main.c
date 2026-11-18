/*
 * SPDX-License-Identifier: Apache-2.0
 * SPDX-FileCopyrightText: 2025-2026 Tenstorrent USA, Inc.
 */

#include <stdio.h>
#include <cpu.h>
#include <csr.h>
#include <image/boot_image.h>
#include <platform/atlantis.h>

extern const char g_build_banner[];

int main(void)
{
    const struct boot_image *images;
    uint32_t entry = 0;
    size_t count;
    bool is_bootrom;
    const char *target = "SRAM";
    int rc;

    platform_atlantis_init();

    is_bootrom = platform_atlantis_is_bootrom();
    if (is_bootrom) {
        target = "bootrom";
    }
    printf("%s", g_build_banner);
    printf("HILT %s: boot mode %s, boot cause %s\n",
            target, platform_atlantis_boot_mode_name(),
            platform_atlantis_boot_cause_name());

    images = platform_atlantis_boot_images(&count);
    for (size_t i = 0; i < count; i++) {
        uint32_t e = 0;
        printf("HILT: image %s: load 0x%08x, NOR 0x%x\n",
               images[i].name, (unsigned)images[i].load_addr,
               (unsigned)images[i].nor_offset);
        rc = platform_atlantis_load_boot_image(&images[i], &e);
        if (rc) {
            printf("failed to load %s rc=%d\n", images[i].name, rc);
        }
        if (!rc && e) {
            /* A valid entry point was found */
            entry = e;
        }
    }

    /* 0x0 is an invalid entry for HILT */
    if (!entry) {
        printf("HILT: nothing to run, idling\n");
        for (;;)
            cpu_idle();
    }

    printf("HILT: handoff at 0x%08x, boot settings at 0x%08x\n", (unsigned)entry,
           (unsigned)platform_atlantis_boot_settings());
    printf("HILT: complete\n");
    fflush(stdout);
    cpu_handoff(entry, csr_read(CSR_MHARTID), platform_atlantis_boot_settings());
}
