/*
 * SPDX-License-Identifier: Apache-2.0
 * SPDX-FileCopyrightText: 2025-2026 Tenstorrent USA, Inc.
 *
 * HILT runtime Zephyr application. Loaded by HILT boot stage.
 */

#include <stdio.h>
#include <zephyr/kernel.h>

int main(void)
{
	printf("HILT runtime: %s\n", CONFIG_BOARD_TARGET);

	return 0;
}
