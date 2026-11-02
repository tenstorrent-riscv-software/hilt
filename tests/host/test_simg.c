/*
 * SPDX-License-Identifier: Apache-2.0
 * SPDX-FileCopyrightText: 2025-2026 Tenstorrent USA, Inc.
 *
 * Host-side unit tests for the SIMG header parser.
 *
 * Runs two ways:
 *   - always: constructed-header tests, one per validation rule
 *   - with an argument: also validates a golden SIMG file produced by
 *     tools/mksimg.py, cross-checking the tool against the parser
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "image/simg.h"
#include <crc32.h>

static int failures;

#define CHECK(cond)                                                       \
    do {                                                                  \
        if (!(cond)) {                                                    \
            failures++;                                                   \
            fprintf(stderr, "FAIL %s:%d: %s\n", __FILE__, __LINE__, #cond); \
        }                                                                 \
    } while (0)

static struct simg_header valid_header(void)
{
    struct simg_header hdr = {
        .magic = SIMG_MAGIC,
        .domain = { 'Z', 'S', 'B', 'L' },
        .format_version = SIMG_FORMAT_V1,
        .header_length = SIMG_HEADER_SIZE,
        .public_key_length = SIMG_KEY_SIZE,
        .payload_type = SIMG_PAYLOAD_HILT_BOOT,
        .payload_length = 0x1234,
        /* Non-zero on purpose: 0x64 is a live field, not reserved. */
        .payload_crc = 0xdeadbeef,
    };
    hdr.header_crc = simg_compute_header_crc(&hdr);
    return hdr;
}

static void test_crc32_matches_zlib(void)
{
    /* zlib.crc32(b"123456789") == 0xcbf43926, the standard check value. */
    const uint8_t check[] = "123456789";
    uint32_t crc = crc32_calc(check, 9);
    CHECK(crc == 0xcbf43926u);
}

static void test_valid_header_passes(void)
{
    struct simg_header hdr = valid_header();
    CHECK(simg_validate_header(&hdr) == SIMG_OK);
    CHECK(simg_domain_id(&hdr) == SIMG_DOMAIN_ZSBL);
}

static void test_each_rule_rejects(void)
{
    struct simg_header hdr;

    hdr = valid_header();
    hdr.magic = 0x12345678;
    CHECK(simg_validate_header(&hdr) == SIMG_ERR_MAGIC);

    hdr = valid_header();
    hdr.format_version = 2;
    hdr.header_crc = simg_compute_header_crc(&hdr);
    CHECK(simg_validate_header(&hdr) == SIMG_ERR_FORMAT_VERSION);

    hdr = valid_header();
    hdr.header_length = 1024;
    hdr.header_crc = simg_compute_header_crc(&hdr);
    CHECK(simg_validate_header(&hdr) == SIMG_ERR_HEADER_LENGTH);

    hdr = valid_header();
    memcpy(hdr.domain, "HILT", 4);
    hdr.header_crc = simg_compute_header_crc(&hdr);
    CHECK(simg_validate_header(&hdr) == SIMG_ERR_DOMAIN);

    hdr = valid_header();
    hdr.flags = 1u << 0;
    hdr.header_crc = simg_compute_header_crc(&hdr);
    CHECK(simg_validate_header(&hdr) == SIMG_ERR_FLAGS);

    /* Any field flip without recomputing the CRC must be caught. */
    hdr = valid_header();
    hdr.payload_length++;
    CHECK(simg_validate_header(&hdr) == SIMG_ERR_CRC);

    hdr = valid_header();
    hdr.auth_key_slot = 2; /* ZSBL domain allows slots 0-1 */
    hdr.header_crc = simg_compute_header_crc(&hdr);
    CHECK(simg_validate_header(&hdr) == SIMG_ERR_KEY_SLOT);

    hdr = valid_header();
    hdr.public_key_length = 256;
    hdr.header_crc = simg_compute_header_crc(&hdr);
    CHECK(simg_validate_header(&hdr) == SIMG_ERR_KEY_LENGTH);

    hdr = valid_header();
    hdr.config[3] = 1;
    hdr.header_crc = simg_compute_header_crc(&hdr);
    CHECK(simg_validate_header(&hdr) == SIMG_ERR_RESERVED);

    hdr = valid_header();
    hdr.reserved2[7] = 1;
    hdr.header_crc = simg_compute_header_crc(&hdr);
    CHECK(simg_validate_header(&hdr) == SIMG_ERR_RESERVED);

    /* Revocation bitmap without the flag... */
    hdr = valid_header();
    hdr.revocation_bitmap = 0x1;
    hdr.header_crc = simg_compute_header_crc(&hdr);
    CHECK(simg_validate_header(&hdr) == SIMG_ERR_REVOCATION);

    /* ...the flag with an empty bitmap... */
    hdr = valid_header();
    hdr.flags = SIMG_FLAG_REVOCATION_PRESENT;
    hdr.header_crc = simg_compute_header_crc(&hdr);
    CHECK(simg_validate_header(&hdr) == SIMG_ERR_REVOCATION);

    /* ...bits outside the domain mask (ZSBL allows 0x3)... */
    hdr = valid_header();
    hdr.flags = SIMG_FLAG_REVOCATION_PRESENT;
    hdr.revocation_bitmap = 0x4;
    hdr.header_crc = simg_compute_header_crc(&hdr);
    CHECK(simg_validate_header(&hdr) == SIMG_ERR_REVOCATION);

    /* ...and a consistent revocation request passes. */
    hdr = valid_header();
    hdr.flags = SIMG_FLAG_REVOCATION_PRESENT;
    hdr.revocation_bitmap = 0x1;
    hdr.header_crc = simg_compute_header_crc(&hdr);
    CHECK(simg_validate_header(&hdr) == SIMG_OK);

    /* The ZSBL domain only carries FSBL payloads. */
    hdr = valid_header();
    hdr.payload_type = SIMG_PAYLOAD_PLATFORM_FW;
    hdr.header_crc = simg_compute_header_crc(&hdr);
    CHECK(simg_validate_header(&hdr) == SIMG_ERR_PAYLOAD_TYPE);

    /* The FSBL domain carries the FIT of the next stages */
    hdr = valid_header();
    memcpy(hdr.domain, "FSBL", 4);
    hdr.payload_type = SIMG_PAYLOAD_PLATFORM_FW;
    hdr.header_crc = simg_compute_header_crc(&hdr);
    CHECK(simg_validate_header(&hdr) == SIMG_OK);
    CHECK(SIMG_PAYLOAD_PLATFORM_FW == 2);

    /* Types past the last defined one are rejected in every domain. */
    hdr = valid_header();
    memcpy(hdr.domain, "FSBL", 4);
    hdr.payload_type = SIMG_PAYLOAD_PLATFORM_FW + 1;
    hdr.header_crc = simg_compute_header_crc(&hdr);
    CHECK(simg_validate_header(&hdr) == SIMG_ERR_PAYLOAD_TYPE);

    hdr = valid_header();
    memcpy(hdr.domain, "USER", 4);
    hdr.payload_type = 7;
    hdr.header_crc = simg_compute_header_crc(&hdr);
    CHECK(simg_validate_header(&hdr) == SIMG_ERR_PAYLOAD_TYPE);

    hdr = valid_header();
    hdr.payload_type = 0;
    hdr.header_crc = simg_compute_header_crc(&hdr);
    CHECK(simg_validate_header(&hdr) == SIMG_ERR_PAYLOAD_TYPE);
}

static void test_domain_pin(void)
{
    struct simg_header hdr = valid_header();

    CHECK(simg_check_domain(&hdr, SIMG_DOMAIN_ZSBL) == SIMG_OK);
    /* An FSBL-stage verifier must refuse a ZSBL-domain image and vice
       versa, even though the header itself is well-formed. */
    CHECK(simg_check_domain(&hdr, SIMG_DOMAIN_FSBL) == SIMG_ERR_DOMAIN);
    CHECK(simg_check_domain(&hdr, SIMG_DOMAIN_USER) == SIMG_ERR_DOMAIN);
    CHECK(simg_check_domain(&hdr, SIMG_DOMAIN_INVALID) == SIMG_ERR_DOMAIN);

    memcpy(hdr.domain, "FSBL", 4);
    CHECK(simg_check_domain(&hdr, SIMG_DOMAIN_FSBL) == SIMG_OK);
    CHECK(simg_check_domain(&hdr, SIMG_DOMAIN_ZSBL) == SIMG_ERR_DOMAIN);
}

static void test_image_length(void)
{
    struct simg_header hdr = valid_header();
    uint64_t total = SIMG_HEADER_SIZE + hdr.payload_length + SIMG_SIGNATURE_SIZE;

    CHECK(simg_check_image_length(&hdr, total) == SIMG_OK);
    CHECK(simg_check_image_length(&hdr, total - 1) == SIMG_ERR_IMAGE_LENGTH);
    CHECK(simg_check_image_length(&hdr, total + 1) == SIMG_ERR_IMAGE_LENGTH);

    hdr.payload_length = UINT64_MAX - 100; /* must not overflow */
    CHECK(simg_check_image_length(&hdr, 0) == SIMG_ERR_IMAGE_LENGTH);

    hdr = valid_header();
    CHECK(simg_signature_offset(&hdr) == SIMG_HEADER_SIZE + 0x1234);
}

static void test_payload_crc(void)
{
    uint8_t payload[] = "123456789";
    struct simg_header hdr = valid_header();

    hdr.payload_length = 9;
    hdr.payload_crc = 0xcbf43926u; /* zlib.crc32(b"123456789") */
    hdr.header_crc = simg_compute_header_crc(&hdr);
    CHECK(simg_validate_header(&hdr) == SIMG_OK);
    CHECK(simg_verify_payload_crc(&hdr, payload, 9) == SIMG_OK);

    /* Length must match the header before the CRC is even considered. */
    CHECK(simg_verify_payload_crc(&hdr, payload, 8) == SIMG_ERR_IMAGE_LENGTH);

    payload[4] ^= 0x01;
    CHECK(simg_verify_payload_crc(&hdr, payload, 9) == SIMG_ERR_PAYLOAD_CRC);

    /* A zeroed field */
    payload[4] ^= 0x01;
    hdr.payload_crc = 0;
    CHECK(simg_verify_payload_crc(&hdr, payload, 9) == SIMG_ERR_PAYLOAD_CRC);
}

static void test_golden_file(const char *path)
{
    FILE *f = fopen(path, "rb");
    if (f == NULL) {
        failures++;
        fprintf(stderr, "FAIL: cannot open golden image %s\n", path);
        return;
    }

    struct simg_header hdr;
    CHECK(fread(&hdr, 1, sizeof(hdr), f) == sizeof(hdr));
    fseek(f, 0, SEEK_END);
    long file_len = ftell(f);

    enum simg_status status = simg_validate_header(&hdr);
    if (status != SIMG_OK) {
        failures++;
        fprintf(stderr, "FAIL: golden image invalid: %s\n",
                simg_status_str(status));
        fclose(f);
        return;
    }
    CHECK(simg_check_domain(&hdr, SIMG_DOMAIN_ZSBL) == SIMG_OK);
    CHECK(hdr.payload_type == SIMG_PAYLOAD_HILT_BOOT);
    CHECK(simg_check_image_length(&hdr, (uint64_t)file_len) == SIMG_OK);

    /* The tool's payload CRC must be the one the parser computes: this is
       the check that catches format drift between mksimg.py and simg.c. */
    uint8_t *payload = malloc((size_t)hdr.payload_length);
    CHECK(payload != NULL);
    if (payload != NULL) {
        fseek(f, (long)simg_payload_offset(&hdr), SEEK_SET);
        CHECK(fread(payload, 1, (size_t)hdr.payload_length, f) ==
              (size_t)hdr.payload_length);
        status = simg_verify_payload_crc(&hdr, payload, (size_t)hdr.payload_length);
        if (status != SIMG_OK) {
            failures++;
            fprintf(stderr, "FAIL: golden payload CRC: %s\n",
                    simg_status_str(status));
        }
        free(payload);
    }
    fclose(f);
}

int main(int argc, char **argv)
{
    test_crc32_matches_zlib();
    test_valid_header_passes();
    test_each_rule_rejects();
    test_domain_pin();
    test_image_length();
    test_payload_crc();
    if (argc > 1)
        test_golden_file(argv[1]);

    if (failures) {
        fprintf(stderr, "%d check(s) failed\n", failures);
        return EXIT_FAILURE;
    }
    printf("all checks passed\n");
    return EXIT_SUCCESS;
}
