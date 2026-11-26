/*
 * SPDX-License-Identifier: Apache-2.0
 * SPDX-FileCopyrightText: 2025-2026 Tenstorrent USA, Inc.
 */

#ifndef __BOOT_IMAGE_H__
#define __BOOT_IMAGE_H__

#include <types.h>
#include "image/simg.h"

struct boot_image_simg {
    enum simg_payload_type type; /* expected SIMG payload type */
    enum simg_domain domain;      /* The domain this image should be loaded from */
    bool exec;                   /* jump to this image once all are loaded */
};

/*
 * A boot_image can be a FIT consisting of multiple images or a simg containing
 * a single image.
 */
struct boot_image {
    const char *name;
    bool is_fit;
    struct boot_image_simg simg; /* simg specific data, only used if is_fit is false */
    uintptr_t load_addr;         /* address the SIMG or the FIT's FDT is loaded to */
    size_t max_size;             /* limit on the whole SIMG or FIT */
    size_t fdt_max_size;         /* FIT only: room at load_addr for the FIT's FDT */
    uint32_t nor_offset;         /* byte offset of the SIMG in NOR flash */
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

/*
 * Load an image into memory
 * @param image struct defining parameters for image
 * @param read fn ptr to read from the boot media
 * @param ctx struct to pass to read fn ptr
 * @param entry ptr to be set with the address of the next stage
 */
int boot_image_load_image(const struct boot_image *image, boot_image_read_fn read,
                          void *ctx, uint32_t *entry);

#endif /* __BOOT_IMAGE_H__ */
