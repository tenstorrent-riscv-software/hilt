/*
 * Copyright (c) 2026 Tenstorrent
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <zephyr/kernel.h>
#include <zephyr/sys/sys_io.h>
#include <zephyr/sys/poweroff.h>

#include "soc.h"

void sys_arch_reboot(int type)
{
	ARG_UNUSED(type);

	sys_write32(ATLANTIS_GLOBAL_SOFT_RST_KEY,
		    ATLANTIS_PRCM_BASE + ATLANTIS_PRCM_GLOBAL_SOFT_RST);
	for (;;) {
		__asm__ volatile("wfi");
	}
}

void z_sys_poweroff(void)
{
	/* The RCPU cannot cut its own power; board power is the BMC's. Park. */
	for (;;) {
		__asm__ volatile("wfi");
	}
}
