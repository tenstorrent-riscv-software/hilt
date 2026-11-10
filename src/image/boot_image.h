/*
 * SPDX-License-Identifier: Apache-2.0
 * SPDX-FileCopyrightText: 2025-2026 Tenstorrent USA, Inc.
 */

#ifndef __BOOT_IMAGE_H__
#define __BOOT_IMAGE_H__

#include <types.h>
#include "image/simg.h"

struct boot_image {
    const char *name;
    enum simg_payload_type type; /* expected SIMG payload type */
    uintptr_t load_addr;         /* address the SIMG is loaded to */
    size_t max_size;             /* header + payload + signature limit */
    uint32_t nor_offset;         /* byte offset of the SIMG in NOR flash */
    bool exec;                   /* jump to this image once all are loaded */
};

/* Get the entry point for a boot image */
static inline uintptr_t boot_image_entry(const struct boot_image *image)
{
    return image->load_addr;
}

/*
 * Fn ptr to read an image from the boot medium
 * @param ctx pointer to pass to the read function
 * @param offset byte offset into the boot medium
 * @param buf to read image into
 * @param length of read
 * */
typedef int (*boot_image_read_fn)(void *ctx, uint32_t offset, void *buf, size_t len);

/* Read, validate and place the SIMG at image->nor_offset. */
int boot_image_load_image(const struct boot_image *image, boot_image_read_fn read,
                          void *ctx, bool is_bootrom);

#endif /* __BOOT_IMAGE_H__ */
