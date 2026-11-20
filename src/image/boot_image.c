/*
 * SPDX-License-Identifier: Apache-2.0
 * SPDX-FileCopyrightText: 2025-2026 Tenstorrent USA, Inc.
 */

#include <errno.h>
#include <stdio.h>
#include <types.h>
#include "image/boot_image.h"
#include "image/simg.h"

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

int boot_image_load_image(const struct boot_image *image, boot_image_read_fn read,
                          void *ctx, uint32_t *entry)
{
    if (!image || !ctx || !entry) {
        return -EINVAL;
    }

    if (image->is_fit) {
        printf("fit image loading not yet supported\n");
        return -EINVAL;
    }
    return load_simg(image, read, ctx, entry);
}
