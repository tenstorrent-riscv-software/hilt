/*
 * SPDX-License-Identifier: Apache-2.0
 * SPDX-FileCopyrightText: 2025-2026 Tenstorrent USA, Inc.
 *
 * Host-side unit tests for the FIT parser.
 *
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <libfdt.h>

#include "image/fit.h"
#include <crc32.h>

static int failures;

#define CHECK(cond)                                                       \
    do {                                                                  \
        if (!(cond)) {                                                    \
            failures++;                                                   \
            fprintf(stderr, "FAIL %s:%d: %s\n", __FILE__, __LINE__, #cond); \
        }                                                                 \
    } while (0)

#define FW_SIZE       300u
#define EXTRA_SIZE    100u
#define EXTRA_OFFSET  0x200u /* data-offset of the second image */
#define MEDIUM_SIZE   0x2000u

/* What to build; the zero value is a valid one-image FIT with external data. */
struct fit_opts {
    bool embedded;           /* image data in a `data` property */
    bool data_position;      /* add an (unsupported) data-position */
    uint32_t address_cells;  /* 0 leaves #address-cells out */
    const char *compression; /* default "none" */
    const char *arch;        /* default "riscv" */
    const char *algo;        /* default "crc32" */
    bool no_hash;            /* images without a hash subnode */
    const char *default_cfg; /* /configurations/default; default "conf" */
    bool no_default;         /* /configurations without a default property */
    const void *default_raw; /* raw bytes for default, instead of a string */
    int default_raw_len;
    const char *firmware;    /* image the configuration names; default "fw" */
    bool no_firmware;        /* configuration without a firmware property */
    bool no_load;            /* images without a load address */
    bool loadable;           /* add a second image, listed under loadables */
    uint32_t load;           /* load address of "fw" */
    uint32_t extra_load;     /* load address of "extra" */
};

/* The boot medium: a FIT at offset 0. */
static _Alignas(8) uint8_t medium[MEDIUM_SIZE];
static uint8_t fw_data[FW_SIZE];
static uint8_t extra_data[EXTRA_SIZE];

static void add_image(void *fdt, const struct fit_opts *o, const char *name,
                      const uint8_t *data, uint32_t size, uint32_t offset,
                      uint32_t load)
{
    CHECK(fdt_begin_node(fdt, name) == 0);
    CHECK(fdt_property_string(fdt, "type", "firmware") == 0);
    CHECK(fdt_property_string(fdt, "arch", o->arch ? o->arch : "riscv") == 0);
    CHECK(fdt_property_string(fdt, "compression",
                              o->compression ? o->compression : "none") == 0);
    if (!o->no_load)
        CHECK(fdt_property_u32(fdt, "load", load) == 0);
    CHECK(fdt_property_u32(fdt, "entry", load) == 0);
    if (o->embedded) {
        CHECK(fdt_property(fdt, "data", data, (int)size) == 0);
    } else {
        CHECK(fdt_property_u32(fdt, "data-offset", offset) == 0);
        CHECK(fdt_property_u32(fdt, "data-size", size) == 0);
    }
    if (o->data_position)
        CHECK(fdt_property_u32(fdt, "data-position", 0x1000) == 0);

    if (!o->no_hash) {
        CHECK(fdt_begin_node(fdt, "hash-1") == 0);
        CHECK(fdt_property_string(fdt, "algo", o->algo ? o->algo : "crc32") == 0);
        CHECK(fdt_property_u32(fdt, "value", crc32_calc(data, size)) == 0);
        CHECK(fdt_end_node(fdt) == 0);
    }
    CHECK(fdt_end_node(fdt) == 0);
}

/* Build a FIT into the medium. Returns the FDT's total size. */
static size_t build_fit(const struct fit_opts *o)
{
    void *fdt = medium;

    for (size_t i = 0; i < FW_SIZE; i++)
        fw_data[i] = (uint8_t)(i * 7 + 1);
    for (size_t i = 0; i < EXTRA_SIZE; i++)
        extra_data[i] = (uint8_t)(i * 13 + 5);

    memset(medium, 0xff, sizeof(medium));
    CHECK(fdt_create(fdt, sizeof(medium)) == 0);
    CHECK(fdt_finish_reservemap(fdt) == 0);
    CHECK(fdt_begin_node(fdt, "") == 0);
    CHECK(fdt_property_string(fdt, "description", "test FIT") == 0);
    if (o->address_cells)
        CHECK(fdt_property_u32(fdt, "#address-cells", o->address_cells) == 0);

    CHECK(fdt_begin_node(fdt, "images") == 0);
    add_image(fdt, o, "fw", fw_data, FW_SIZE, 0, o->load);
    if (o->loadable)
        add_image(fdt, o, "extra", extra_data, EXTRA_SIZE, EXTRA_OFFSET,
                  o->extra_load);
    CHECK(fdt_end_node(fdt) == 0);

    CHECK(fdt_begin_node(fdt, "configurations") == 0);
    if (o->default_raw)
        CHECK(fdt_property(fdt, "default", o->default_raw,
                           o->default_raw_len) == 0);
    else if (!o->no_default)
        CHECK(fdt_property_string(fdt, "default",
                                  o->default_cfg ? o->default_cfg : "conf") == 0);
    CHECK(fdt_begin_node(fdt, "conf") == 0);
    if (!o->no_firmware)
        CHECK(fdt_property_string(fdt, "firmware",
                                  o->firmware ? o->firmware : "fw") == 0);
    if (o->loadable)
        CHECK(fdt_property_string(fdt, "loadables", "extra") == 0);
    CHECK(fdt_end_node(fdt) == 0);
    CHECK(fdt_end_node(fdt) == 0);

    CHECK(fdt_end_node(fdt) == 0);
    CHECK(fdt_finish(fdt) == 0);

    size_t total = fdt_totalsize(fdt);
    size_t base = (total + 3) & ~(size_t)3;

    CHECK(base + EXTRA_OFFSET + EXTRA_SIZE <= sizeof(medium));
    if (!o->embedded) {
        memcpy(medium + base, fw_data, FW_SIZE);
        if (o->loadable)
            memcpy(medium + base + EXTRA_OFFSET, extra_data, EXTRA_SIZE);
    }
    return total;
}

static void test_fit_external(void)
{
    struct fit_opts o = { .loadable = true, .load = 0x1000, .extra_load = 0x2000 };
    size_t total = build_fit(&o);
    struct fit_image img;
    struct fit fit;
    int cfg = -1;

    CHECK(fit_open(&fit, medium, sizeof(medium)) == FIT_OK);
    CHECK(fit.fdt_size == ((total + 3) & ~(size_t)3));
    CHECK(fit_lookup_config(&fit, &cfg) == FIT_OK);

    CHECK(fit_open_image(&fit, cfg, "firmware", 0, &img) == FIT_OK);
    CHECK(strcmp(img.name, "fw") == 0);
    CHECK(strcmp(img.type, "firmware") == 0);
    CHECK(img.has_load && img.load == 0x1000);
    CHECK(img.has_entry && img.entry == 0x1000);
    CHECK(img.data_size == FW_SIZE);
    CHECK(img.data_offset == fit.fdt_size);
    CHECK(fit_image_verify_crc(&fit, &img, medium + img.data_offset) == FIT_OK);
    CHECK(fit_open_image(&fit, cfg, "firmware", 1, &img) == FIT_ERR_NOT_FOUND);

    CHECK(fit_open_image(&fit, cfg, "loadables", 0, &img) == FIT_OK);
    CHECK(strcmp(img.name, "extra") == 0);
    CHECK(img.load == 0x2000);
    CHECK(img.data_size == EXTRA_SIZE);
    CHECK(img.data_offset == fit.fdt_size + EXTRA_OFFSET);
    CHECK(fit_image_verify_crc(&fit, &img, medium + img.data_offset) == FIT_OK);
    CHECK(fit_open_image(&fit, cfg, "loadables", 1, &img) == FIT_ERR_NOT_FOUND);

    /* A property the configuration does not have at all. */
    CHECK(fit_open_image(&fit, cfg, "fdt", 0, &img) == FIT_ERR_NOT_FOUND);
}

static void test_fit_embedded(void)
{
    struct fit_opts o = { .embedded = true, .load = 0x1000 };
    size_t total = build_fit(&o);
    struct fit_image img;
    struct fit fit;
    int cfg = -1;

    /* With embedded data the FDT is the whole FIT. */
    CHECK(fit_open(&fit, medium, total) == FIT_OK);
    CHECK(fit_lookup_config(&fit, &cfg) == FIT_OK);
    CHECK(fit_open_image(&fit, cfg, "firmware", 0, &img) == FIT_OK);
    CHECK(img.data_size == FW_SIZE);
    CHECK(img.data_offset + img.data_size <= total);
    CHECK(memcmp(medium + img.data_offset, fw_data, FW_SIZE) == 0);
    CHECK(fit_image_verify_crc(&fit, &img, medium + img.data_offset) == FIT_OK);
}

static void test_fit_crc(void)
{
    struct fit_opts o = { .load = 0x1000 };
    struct fit_image img;
    struct fit fit;
    int cfg = -1;

    build_fit(&o);
    CHECK(fit_open(&fit, medium, sizeof(medium)) == FIT_OK);
    CHECK(fit_lookup_config(&fit, &cfg) == FIT_OK);
    CHECK(fit_open_image(&fit, cfg, "firmware", 0, &img) == FIT_OK);
    medium[img.data_offset + 17] ^= 0x01;
    CHECK(fit_image_verify_crc(&fit, &img, medium + img.data_offset) == FIT_ERR_HASH);

    /* An image without hash subnodes passes, whatever the data is. */
    o.no_hash = true;
    build_fit(&o);
    CHECK(fit_open(&fit, medium, sizeof(medium)) == FIT_OK);
    CHECK(fit_lookup_config(&fit, &cfg) == FIT_OK);
    CHECK(fit_open_image(&fit, cfg, "firmware", 0, &img) == FIT_OK);
    medium[img.data_offset + 17] ^= 0x01;
    CHECK(fit_image_verify_crc(&fit, &img, medium + img.data_offset) == FIT_OK);
    o.no_hash = false;

    o.algo = "sha256";
    build_fit(&o);
    CHECK(fit_open(&fit, medium, sizeof(medium)) == FIT_OK);
    CHECK(fit_lookup_config(&fit, &cfg) == FIT_OK);
    CHECK(fit_open_image(&fit, cfg, "firmware", 0, &img) == FIT_OK);
    CHECK(fit_image_verify_crc(&fit, &img, medium + img.data_offset) ==
          FIT_ERR_UNSUPPORTED_HASH);
}

static void test_fit_config(void)
{
    struct fit_image img;
    struct fit fit;
    int cfg = -1;

    /* Without a default property the first configuration is used. */
    build_fit(&(struct fit_opts){ .load = 0x1000, .no_default = true });
    CHECK(fit_open(&fit, medium, sizeof(medium)) == FIT_OK);
    CHECK(fit_lookup_config(&fit, &cfg) == FIT_OK);
    CHECK(strcmp(fdt_get_name(medium, cfg, NULL), "conf") == 0);
    CHECK(fit_open_image(&fit, cfg, "firmware", 0, &img) == FIT_OK);

    /* The default names a configuration that is not there. */
    build_fit(&(struct fit_opts){ .load = 0x1000, .default_cfg = "nope" });
    CHECK(fit_open(&fit, medium, sizeof(medium)) == FIT_OK);
    CHECK(fit_lookup_config(&fit, &cfg) == FIT_ERR_NOT_FOUND);

    /*
     * A malformed default is rejected, not treated as absent: no terminating
     * NUL, empty, an empty string, or more than one string.
     */
    static const struct {
        const char *raw;
        int len;
    } bad_defaults[] = {
        { "conf", 4 },
        { "", 0 },
        { "", 1 },
        { "conf\0conf", 10 },
        { "nope\0conf", 10 },
    };
    for (size_t i = 0; i < sizeof(bad_defaults) / sizeof(bad_defaults[0]); i++) {
        build_fit(&(struct fit_opts){ .load = 0x1000,
                                      .default_raw = bad_defaults[i].raw,
                                      .default_raw_len = bad_defaults[i].len });
        CHECK(fit_open(&fit, medium, sizeof(medium)) == FIT_OK);
        CHECK(fit_lookup_config(&fit, &cfg) == FIT_ERR_FORMAT);
    }

    /* An image without a load address still decodes; the caller decides. */
    build_fit(&(struct fit_opts){ .no_load = true });
    CHECK(fit_open(&fit, medium, sizeof(medium)) == FIT_OK);
    CHECK(fit_lookup_config(&fit, &cfg) == FIT_OK);
    CHECK(fit_open_image(&fit, cfg, "firmware", 0, &img) == FIT_OK);
    CHECK(!img.has_load);
    CHECK(img.has_entry);
}

static void test_fit_rejects(void)
{
    struct fit_image img;
    struct fit fit;
    int cfg = -1;
    size_t total;

    /* Not an FDT at all. */
    memset(medium, 0, sizeof(medium));
    CHECK(fit_open(&fit, medium, sizeof(medium)) == FIT_ERR_FORMAT);
    CHECK(fit_open(&fit, medium, 8) == FIT_ERR_SIZE);

    /* The FDT itself is larger than the FIT is allowed to be. */
    total = build_fit(&(struct fit_opts){ .load = 0x1000 });
    CHECK(fit_open(&fit, medium, total - 1) == FIT_ERR_SIZE);

    /* External data running past the size limit. */
    CHECK(fit_open(&fit, medium, total + FW_SIZE - 1) == FIT_OK);
    CHECK(fit_lookup_config(&fit, &cfg) == FIT_OK);
    CHECK(fit_open_image(&fit, cfg, "firmware", 0, &img) == FIT_ERR_SIZE);

    /* A corrupted structure block. */
    build_fit(&(struct fit_opts){ .load = 0x1000 });
    memset(medium + fdt_off_dt_struct(medium), 0x5a, 16);
    CHECK(fit_open(&fit, medium, sizeof(medium)) == FIT_ERR_FORMAT);

    build_fit(&(struct fit_opts){ .address_cells = 2 });
    CHECK(fit_open(&fit, medium, sizeof(medium)) == FIT_ERR_UNSUPPORTED);

    build_fit(&(struct fit_opts){ .address_cells = 1 });
    CHECK(fit_open(&fit, medium, sizeof(medium)) == FIT_OK);

    build_fit(&(struct fit_opts){ .compression = "gzip" });
    CHECK(fit_open(&fit, medium, sizeof(medium)) == FIT_OK);
    CHECK(fit_lookup_config(&fit, &cfg) == FIT_OK);
    CHECK(fit_open_image(&fit, cfg, "firmware", 0, &img) == FIT_ERR_UNSUPPORTED);

    build_fit(&(struct fit_opts){ .arch = "arm64" });
    CHECK(fit_open(&fit, medium, sizeof(medium)) == FIT_OK);
    CHECK(fit_lookup_config(&fit, &cfg) == FIT_OK);
    CHECK(fit_open_image(&fit, cfg, "firmware", 0, &img) == FIT_ERR_UNSUPPORTED);

    build_fit(&(struct fit_opts){ .data_position = true });
    CHECK(fit_open(&fit, medium, sizeof(medium)) == FIT_OK);
    CHECK(fit_lookup_config(&fit, &cfg) == FIT_OK);
    CHECK(fit_open_image(&fit, cfg, "firmware", 0, &img) == FIT_ERR_UNSUPPORTED);

    /* The configuration names an image that is not there. */
    build_fit(&(struct fit_opts){ .firmware = "nope" });
    CHECK(fit_open(&fit, medium, sizeof(medium)) == FIT_OK);
    CHECK(fit_lookup_config(&fit, &cfg) == FIT_OK);
    CHECK(fit_open_image(&fit, cfg, "firmware", 0, &img) == FIT_ERR_BAD_REF);

    /* A FIT has to have /images and /configurations. */
    build_fit(&(struct fit_opts){ .load = 0x1000 });
    CHECK(fdt_nop_node(medium, fdt_path_offset(medium, "/configurations")) == 0);
    CHECK(fit_open(&fit, medium, sizeof(medium)) == FIT_ERR_FORMAT);
}

int main(void)
{
    test_fit_external();
    test_fit_embedded();
    test_fit_crc();
    test_fit_config();
    test_fit_rejects();

    if (failures) {
        fprintf(stderr, "%d check(s) failed\n", failures);
        return EXIT_FAILURE;
    }
    printf("all checks passed\n");
    return EXIT_SUCCESS;
}
