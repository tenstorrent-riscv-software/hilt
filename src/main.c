/*
 * SPDX-License-Identifier: Apache-2.0
 * SPDX-FileCopyrightText: 2025-2026 Tenstorrent USA, Inc.
 */

#include <stdio.h>
#include <cpu.h>

int main(void)
{
    for (;;)
        cpu_idle();
}
