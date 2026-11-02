/*
 * SPDX-License-Identifier: Apache-2.0
 * SPDX-FileCopyrightText: 2025-2026 Tenstorrent USA, Inc.
 */

#include <errno.h>
#include <stdio.h>
#include <types.h>
#include "image/boot_image.h"
#include "image/simg.h"

int boot_image_load_image(const struct boot_image *image, boot_image_read_fn read,
                          void *ctx, bool is_bootrom)
{
    uint32_t offset = image->nor_offset;
    uint8_t *dest = (uint8_t *)image->load_addr;
    struct simg_header *hdr = (struct simg_header *)dest;
    enum simg_domain domain;
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

    /* If running as bootrom use the ZSBL domain, otherwise FSBL domain */
    domain = is_bootrom ? SIMG_DOMAIN_ZSBL : SIMG_DOMAIN_FSBL;
    st = simg_validate_header(hdr);
    if (st == SIMG_OK) {
        st = simg_check_domain(hdr, domain);
    }
    if (st == SIMG_OK && hdr->payload_type != image->type) {
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

    printf("%s: %u bytes loaded to 0x%08x, CRC ok\n", image->name,
           (unsigned)hdr->payload_length, (unsigned)image->load_addr);
    return 0;
}
