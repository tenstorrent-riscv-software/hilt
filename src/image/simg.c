/*
 * SPDX-License-Identifier: Apache-2.0
 * SPDX-FileCopyrightText: 2025-2026 Tenstorrent USA, Inc.
 */

#include "image/simg.h"

#include <crc32.h>
#include <string.h>

uint32_t simg_compute_header_crc(const struct simg_header *hdr)
{
    const uint8_t *bytes = (const uint8_t *)hdr;
    const size_t crc_off = offsetof(struct simg_header, header_crc);
    const uint8_t zero[4] = {};
    uint32_t crc = 0xffffffffu;

    crc = crc32_update(crc, bytes, crc_off);
    crc = crc32_update(crc, zero, sizeof(zero));
    crc = crc32_update(crc, bytes + crc_off + 4, SIMG_HEADER_SIZE - crc_off - 4);
    return crc ^ 0xffffffffu;
}

enum simg_domain simg_domain_id(const struct simg_header *hdr)
{
    if (memcmp(hdr->domain, "ZSBL", 4) == 0)
        return SIMG_DOMAIN_ZSBL;
    if (memcmp(hdr->domain, "FSBL", 4) == 0)
        return SIMG_DOMAIN_FSBL;
    if (memcmp(hdr->domain, "USER", 4) == 0)
        return SIMG_DOMAIN_USER;
    return SIMG_DOMAIN_INVALID;
}

static bool all_zero(const uint8_t *bytes, size_t len)
{
    for (size_t i = 0; i < len; i++) {
        if (bytes[i] != 0)
            return false;
    }
    return true;
}

/* Highest valid auth-key slot and valid revocation bits, per domain. */
static uint16_t domain_max_slot(enum simg_domain domain)
{
    return domain == SIMG_DOMAIN_USER ? 0 : 1;
}

static uint64_t domain_revocation_mask(enum simg_domain domain)
{
    return domain == SIMG_DOMAIN_USER ? 0x1 : 0x3;
}

static bool valid_payload_type(enum simg_domain domain, uint8_t type)
{
    if (type < SIMG_PAYLOAD_HILT_BOOT || type > SIMG_PAYLOAD_PLATFORM_FW)
        return false;
    /* The ZSBL only ever loads an FSBL; other domains take any defined type. */
    if (domain == SIMG_DOMAIN_ZSBL)
        return type == SIMG_PAYLOAD_HILT_BOOT;
    return true;
}

enum simg_status simg_validate_header(const struct simg_header *hdr)
{
    if (hdr->magic != SIMG_MAGIC)
        return SIMG_ERR_MAGIC;
    if (hdr->format_version != SIMG_FORMAT_V1)
        return SIMG_ERR_FORMAT_VERSION;
    if (hdr->header_length != SIMG_HEADER_SIZE)
        return SIMG_ERR_HEADER_LENGTH;

    enum simg_domain domain = simg_domain_id(hdr);
    if (domain == SIMG_DOMAIN_INVALID)
        return SIMG_ERR_DOMAIN;

    if ((hdr->flags & ~SIMG_FLAGS_VALID_MASK) != 0)
        return SIMG_ERR_FLAGS;
    if (hdr->header_crc != simg_compute_header_crc(hdr))
        return SIMG_ERR_CRC;
    if (hdr->auth_key_slot > domain_max_slot(domain))
        return SIMG_ERR_KEY_SLOT;
    if (hdr->public_key_length != SIMG_KEY_SIZE)
        return SIMG_ERR_KEY_LENGTH;

    if (hdr->reserved_a != 0 || hdr->reserved1 != 0 ||
        !all_zero(hdr->reserved0, sizeof(hdr->reserved0)) ||
        !all_zero(hdr->config, sizeof(hdr->config)) ||
        !all_zero(hdr->reserved2, sizeof(hdr->reserved2)))
        return SIMG_ERR_RESERVED;

    uint64_t mask = domain_revocation_mask(domain);
    if (hdr->flags & SIMG_FLAG_REVOCATION_PRESENT) {
        if (hdr->revocation_bitmap == 0 || (hdr->revocation_bitmap & ~mask) != 0)
            return SIMG_ERR_REVOCATION;
    } else if (hdr->revocation_bitmap != 0) {
        return SIMG_ERR_REVOCATION;
    }

    if (!valid_payload_type(domain, hdr->payload_type))
        return SIMG_ERR_PAYLOAD_TYPE;

    return SIMG_OK;
}

enum simg_status simg_check_domain(const struct simg_header *hdr,
                                   enum simg_domain expected)
{
    if (expected == SIMG_DOMAIN_INVALID || simg_domain_id(hdr) != expected)
        return SIMG_ERR_DOMAIN;
    return SIMG_OK;
}

enum simg_status simg_check_image_length(const struct simg_header *hdr,
                                         uint64_t image_len)
{
    uint64_t total = SIMG_HEADER_SIZE;

    if (hdr->payload_length > UINT64_MAX - total)
        return SIMG_ERR_IMAGE_LENGTH;
    total += hdr->payload_length;
    if (SIMG_SIGNATURE_SIZE > UINT64_MAX - total)
        return SIMG_ERR_IMAGE_LENGTH;
    total += SIMG_SIGNATURE_SIZE;

    return image_len == total ? SIMG_OK : SIMG_ERR_IMAGE_LENGTH;
}

enum simg_status simg_verify_payload_crc(const struct simg_header *hdr,
                                         const uint8_t *payload, size_t len)
{
    if ((uint64_t)len != hdr->payload_length)
        return SIMG_ERR_IMAGE_LENGTH;

    uint32_t crc = crc32_calc(payload, len);
    return crc == hdr->payload_crc ? SIMG_OK : SIMG_ERR_PAYLOAD_CRC;
}

const char *simg_status_str(enum simg_status status)
{
    switch (status) {
    case SIMG_OK:
        return "ok";
    case SIMG_ERR_MAGIC:
        return "bad magic";
    case SIMG_ERR_FORMAT_VERSION:
        return "unsupported format version";
    case SIMG_ERR_HEADER_LENGTH:
        return "bad header length";
    case SIMG_ERR_DOMAIN:
        return "unknown or unexpected domain";
    case SIMG_ERR_FLAGS:
        return "invalid flags";
    case SIMG_ERR_CRC:
        return "header CRC mismatch";
    case SIMG_ERR_KEY_SLOT:
        return "key slot out of range";
    case SIMG_ERR_KEY_LENGTH:
        return "bad public key length";
    case SIMG_ERR_RESERVED:
        return "reserved fields not zero";
    case SIMG_ERR_REVOCATION:
        return "inconsistent revocation bitmap";
    case SIMG_ERR_PAYLOAD_TYPE:
        return "payload type invalid for domain";
    case SIMG_ERR_IMAGE_LENGTH:
        return "image length mismatch";
    case SIMG_ERR_PAYLOAD_CRC:
        return "payload CRC mismatch";
    }
    return "unknown";
}
