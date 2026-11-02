/*
 * SPDX-License-Identifier: Apache-2.0
 * SPDX-FileCopyrightText: 2025-2026 Tenstorrent USA, Inc.
 *
 * Placeholder for the RCPU runtime stage.
 *
 * Stands in for the runtime when it is not provided. It is position
 * independent, so it is safe to branch to wherever the image is loaded.
 */

    .section .text._start, "ax", %progbits
    .globl  _start
    .type   _start, @function
_start:
    wfi
    j       _start
    .size   _start, . - _start
