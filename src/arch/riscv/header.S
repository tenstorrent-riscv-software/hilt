/*
 * SIMG header placeholder.
 *
 * The bootrom loads HILT as a SIMG image: [512-byte header][payload][signature].
 * The packaging/signing tool replaces these 512 bytes with the real header.
 */

    .section .binary_header, "a", %progbits

    .word   0x2000006f              /* magic / trampoline */

    .org    0x1f0                   /* app_name field */
    .asciz  "hilt"

    .org    0x200
