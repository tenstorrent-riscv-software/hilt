/*
 * SPDX-License-Identifier: Apache-2.0
 * SPDX-FileCopyrightText: 2025-2026 Tenstorrent USA, Inc.
 */

#include <errno.h>
#include <stdio.h>
#include <types.h>
#include "image/boot_image.h"
#include "image/simg.h"
#ifdef ENABLE_FDT
#include <libfdt.h>
#include "image/fit.h"
#endif

int load_simg(const struct boot_image *image, boot_image_read_fn read,
                          void *ctx, uint32_t *entry)
{
    uint32_t offset = image->nor_offset;
    uint8_t *dest = (uint8_t *)image->load_addr;
    struct simg_header *hdr = (struct simg_header *)dest;
    enum simg_status st;
    uint64_t rest;
    int rc;

    if (image->max_size < SIMG_HEADER_SIZE + SIMG_SIGNATURE_SIZE) {
        return -EINVAL;
    }

    rc = read(ctx, offset, hdr, SIMG_HEADER_SIZE);
    if (rc) {
        printf("%s: header read failed: %d\n", image->name, rc);
        return rc;
    }

    st = simg_validate_header(hdr);
    if (st == SIMG_OK) {
        st = simg_check_domain(hdr, image->simg.domain);
    }
    if (st == SIMG_OK && hdr->payload_type != image->simg.type) {
        st = SIMG_ERR_PAYLOAD_TYPE;
    }
    if (st != SIMG_OK) {
        printf("%s: bad SIMG header: %s\n", image->name, simg_status_str(st));
        return -EBADMSG;
    }

    rest = hdr->payload_length + SIMG_SIGNATURE_SIZE;
    if (hdr->payload_length > image->max_size - SIMG_HEADER_SIZE - SIMG_SIGNATURE_SIZE) {
        printf("%s: %u byte payload does not fit the 0x%x byte slot\n", image->name,
               (unsigned)hdr->payload_length, (unsigned)image->max_size);
        return -ENOSPC;
    }

    rc = read(ctx, offset + SIMG_HEADER_SIZE, dest + SIMG_HEADER_SIZE, (size_t)rest);
    if (rc) {
        printf("%s: payload read failed: %d\n", image->name, rc);
        return rc;
    }

    st = simg_verify_payload_crc(hdr, dest + SIMG_HEADER_SIZE,
                                 (size_t)hdr->payload_length);
    if (st != SIMG_OK) {
        printf("%s: %s\n", image->name, simg_status_str(st));
        return -EBADMSG;
    }

    /*
     * The entry for a simg is always the load adresses. The first byte of the
     * header contains a jump to +512 bytes. 0 is an invalid entry for HILT.
     */
    *entry = 0;
    if (image->simg.exec) {
        *entry = image->load_addr;
    }
    printf("%s: %u bytes loaded to 0x%08x, CRC ok\n", image->name,
           (unsigned)hdr->payload_length, (unsigned)image->load_addr);
    return 0;
}

#ifdef ENABLE_FDT
static int fit_errno(enum fit_status st)
{
    return st == FIT_ERR_SIZE ? -ENOSPC : -EBADMSG;
}

/* Read one FIT image from the boot medium to its load address and check it. */
static int load_fit_image(const struct boot_image *image, const char *prop,
                          int cfg, boot_image_read_fn read, int idx,
                          void *ctx, const struct fit *fit,
                          struct fit_image *img)
{
    enum fit_status st;
    void *dest;
    int rc;

    st = fit_open_image(fit, cfg, prop, idx, img);
    if (st == FIT_ERR_NOT_FOUND) {
        return -ENOENT;
    }
    if (st != FIT_OK) {
        printf("%s: bad FIT %s: %s\n", image->name, prop,
               fit_status_str(st));
        return fit_errno(st);
    }

    if (!img->has_load) {
        printf("%s: %s: no load address\n", image->name, img->name);
        return -EBADMSG;
    }

    /* fit_open() bounded data_offset + data_size by max_size, so no overflow. */
    dest = (void *)(uintptr_t)img->load;
    rc = read(ctx, image->nor_offset + (uint32_t)img->data_offset, dest,
              img->data_size);
    if (rc) {
        printf("%s: %s: read failed: %d\n", image->name, img->name, rc);
        return rc;
    }

    st = fit_image_verify_crc(fit, img, dest);
    if (st != FIT_OK) {
        printf("%s: %s: %s\n", image->name, img->name, fit_status_str(st));
        return fit_errno(st);
    }

    printf("%s: %s: %u bytes loaded to 0x%08x, hash ok\n", image->name, img->name,
           (unsigned)img->data_size, (unsigned)img->load);
    return 0;
}

/* Load the entire fit images from boot media into memory */
static int load_fit(const struct boot_image *image, boot_image_read_fn read,
                          void *ctx, uint32_t *entry)
{
    const size_t hdr_size = sizeof(struct fdt_header);
    uint8_t *fdt = (uint8_t *)image->load_addr;
    size_t fdt_size;
    struct fit_image img;
    enum fit_status st;
    struct fit fit;
    int cfg;
    int rc;

    /* The NOR slot and the fdt buffer must be able to hold the fdt */
    if (image->max_size < hdr_size || image->fdt_max_size < hdr_size) {
        return -EINVAL;
    }

    /* The FDT header says how large the FDT is. */
    rc = read(ctx, image->nor_offset, fdt, hdr_size);
    if (rc) {
        printf("%s: header read failed: %d\n", image->name, rc);
        return rc;
    }
    if (fdt_magic(fdt) != FDT_MAGIC) {
        printf("%s: not a FIT\n", image->name);
        return -EBADMSG;
    }

    fdt_size = fdt_totalsize(fdt);
    if (fdt_size < hdr_size) {
        printf("%s: bad FIT: 0x%x byte FDT\n", image->name, (unsigned)fdt_size);
        return -EBADMSG;
    }
    if (fdt_size > image->max_size) {
        printf("%s: 0x%x byte FDT does not fit the 0x%x byte slot\n", image->name,
               (unsigned)fdt_size, (unsigned)image->max_size);
        return -ENOSPC;
    }
    if (fdt_size > image->fdt_max_size) {
        printf("%s: 0x%x byte FDT does not fit the 0x%x byte FDT buffer\n",
               image->name, (unsigned)fdt_size, (unsigned)image->fdt_max_size);
        return -ENOSPC;
    }

    rc = read(ctx, image->nor_offset + hdr_size, fdt + hdr_size, fdt_size - hdr_size);
    if (rc) {
        printf("%s: FDT read failed: %d\n", image->name, rc);
        return rc;
    }

    st = fit_open(&fit, fdt, image->max_size);
    if (st == FIT_OK) {
        st = fit_lookup_config(&fit, &cfg);
    }
    if (st != FIT_OK) {
        printf("%s: bad FIT: %s\n", image->name, fit_status_str(st));
        return fit_errno(st);
    }

    /* 0 is an invalid entry for HILT: nothing to hand off to. */
    *entry = 0;

    /* First load firmware, there should only be at most 1 */
    rc = load_fit_image(image, "firmware", cfg, read, 0, ctx, &fit, &img);
    if (rc && rc != -ENOENT) {
        return rc;
    }
    if (!rc && img.has_entry) {
        *entry = img.entry;
    }

    /* Now load all loadables */
    for(int i = 0;; i++) {
        rc = load_fit_image(image, "loadables", cfg, read, i, ctx, &fit, &img);
        if (rc == -ENOENT) { /* End of the list of images */
            break;
        }
        if (rc) {
            return rc;
        }
    }
    return 0;
}
#endif /* ENABLE_FDT */

int boot_image_load_image(const struct boot_image *image, boot_image_read_fn read,
                          void *ctx, uint32_t *entry)
{
    if (!image || !ctx || !entry) {
        return -EINVAL;
    }

    if (image->is_fit) {
#ifdef ENABLE_FDT
        return load_fit(image, read, ctx, entry);
#else
        printf("%s: FIT images are not supported by this build\n", image->name);
        return -ENOTSUP;
#endif
    }
    return load_simg(image, read, ctx, entry);
}
