/*
 * SPDX-License-Identifier: Apache-2.0
 * SPDX-FileCopyrightText: 2025-2026 Tenstorrent USA, Inc.
 *
 * Atlantis SIMG v1 container: [512-byte header][payload][384-byte signature].
 *
 * The wire format is fixed by the ZSBL mask ROM and must byte-match the
 * signing tooling (tools/mksimg.py). This module is pure logic, no hardware
 * access, so it compiles into both the firmware and the host unit tests.
 */

#pragma once

#include <types.h>

#define SIMG_MAGIC       0x2000006fu /* RISC-V "j .+0x200" boot trampoline */
#define SIMG_FORMAT_V1   0x0001u
#define SIMG_HEADER_SIZE 512u

#define SIMG_KEY_SIZE       384u /* RSA-3072 modulus and signature */
#define SIMG_SIGNATURE_SIZE 384u
#define SIMG_HASH_SIZE      32u /* SHA-256 */

#define SIMG_FLAG_REVOCATION_PRESENT (1u << 1)
#define SIMG_FLAGS_VALID_MASK        SIMG_FLAG_REVOCATION_PRESENT

enum simg_domain {
    SIMG_DOMAIN_INVALID = 0,
    SIMG_DOMAIN_ZSBL, /* verifies the FSBL */
    SIMG_DOMAIN_FSBL, /* verifies the platform firmware */
    SIMG_DOMAIN_USER,
};

enum simg_payload_type {
    SIMG_PAYLOAD_HILT_BOOT = 1,    /* ZSBL domain: the HILT boot stage (FSBL) */
    SIMG_PAYLOAD_PLATFORM_FW = 2,  /* FSBL domain: platform firmware FIT (runtime stage, ...) */
};

enum simg_status {
    SIMG_OK = 0,
    SIMG_ERR_MAGIC,
    SIMG_ERR_FORMAT_VERSION,
    SIMG_ERR_HEADER_LENGTH,
    SIMG_ERR_DOMAIN,
    SIMG_ERR_FLAGS,
    SIMG_ERR_CRC,
    SIMG_ERR_KEY_SLOT,
    SIMG_ERR_KEY_LENGTH,
    SIMG_ERR_RESERVED,
    SIMG_ERR_REVOCATION,
    SIMG_ERR_PAYLOAD_TYPE,
    SIMG_ERR_IMAGE_LENGTH,
    SIMG_ERR_PAYLOAD_CRC,
};

struct simg_header {
    uint32_t magic;             /* 0x00 */
    uint8_t domain[4];          /* 0x04 "ZSBL"/"FSBL"/"USER" */
    uint16_t format_version;    /* 0x08 */
    uint16_t header_length;     /* 0x0A */
    uint32_t flags;             /* 0x0C */
    uint16_t auth_key_slot;     /* 0x10 */
    uint16_t public_key_length; /* 0x12 */
    uint16_t reserved_a;        /* 0x14 */
    uint8_t payload_type;       /* 0x16 */
    uint8_t reserved0[9];       /* 0x17 */
    uint64_t revocation_bitmap; /* 0x20 */
    uint32_t security_version;  /* 0x28 */
    uint32_t reserved1;         /* 0x2C */
    uint64_t payload_length;    /* 0x30 */
    uint8_t payload_hash[SIMG_HASH_SIZE]; /* 0x38 */
    uint32_t header_crc;        /* 0x58 CRC-32 with this field zeroed */
    uint8_t config[8];          /* 0x5C must be zero */
    uint32_t payload_crc;       /* 0x64 CRC-32 of the payload */
    uint8_t reserved2[8];       /* 0x68 must be zero */
    uint8_t public_key[SIMG_KEY_SIZE]; /* 0x70 RSA-3072 modulus, big-endian */
    char app_name[16];          /* 0x1F0 */
} __attribute__((packed, aligned(4)));

static_assert(sizeof(struct simg_header) == SIMG_HEADER_SIZE);
static_assert(offsetof(struct simg_header, payload_type) == 0x16);
static_assert(offsetof(struct simg_header, revocation_bitmap) == 0x20);
static_assert(offsetof(struct simg_header, payload_length) == 0x30);
static_assert(offsetof(struct simg_header, payload_hash) == 0x38);
static_assert(offsetof(struct simg_header, header_crc) == 0x58);
static_assert(offsetof(struct simg_header, config) == 0x5c);
static_assert(offsetof(struct simg_header, payload_crc) == 0x64);
static_assert(offsetof(struct simg_header, reserved2) == 0x68);
static_assert(offsetof(struct simg_header, public_key) == 0x70);
static_assert(offsetof(struct simg_header, app_name) == 0x1f0);

/* CRC over the full header with the header_crc field treated as zero. */
uint32_t simg_compute_header_crc(const struct simg_header *hdr);

enum simg_domain simg_domain_id(const struct simg_header *hdr);

/*
 * Validate every header-only constraint: magic, version, length, domain,
 * flags, CRC, key slot/length, reserved fields, revocation-bitmap
 * consistency, payload type vs domain. Returns SIMG_OK or the first failure.
 */
enum simg_status simg_validate_header(const struct simg_header *hdr);

/*
 * Pin an image to the verifying stage's domain. The boot ROM rejects a header
 * whose domain is not its own before touching OTP, because the domain selects
 * the key hash, security version and revocation bitmap an image is checked
 * against (and could revoke). Call with the stage's domain, not the header's.
 */
enum simg_status simg_check_domain(const struct simg_header *hdr,
                                   enum simg_domain expected);

/*
 * Check a complete image's length against the header (header + payload +
 * signature, with overflow checking). Call after simg_validate_header().
 */
enum simg_status simg_check_image_length(const struct simg_header *hdr,
                                         uint64_t image_len);

/*
 * Non-secure integrity gate: payload CRC-32 vs the header's payload_crc.
 * The ZSBL checks this before the signature whenever secure boot is off or
 * not in enhanced mode, and a mismatch is fatal; enhanced mode relies on the
 * signature alone and skips it. `len` must equal hdr->payload_length.
 */
enum simg_status simg_verify_payload_crc(const struct simg_header *hdr,
                                         const uint8_t *payload, size_t len);

static inline uint64_t simg_payload_offset(const struct simg_header *hdr)
{
    (void)hdr;
    return SIMG_HEADER_SIZE;
}

static inline uint64_t simg_signature_offset(const struct simg_header *hdr)
{
    return SIMG_HEADER_SIZE + hdr->payload_length;
}

const char *simg_status_str(enum simg_status status);
