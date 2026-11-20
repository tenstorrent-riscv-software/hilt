/*
 * SPDX-License-Identifier: Apache-2.0
 * SPDX-FileCopyrightText: 2025-2026 Tenstorrent USA, Inc.
 *
 * Flattened Image Tree (FIT) parser.
 *
 * Only the FDT is stored in memory.
 */

#pragma once

#include <types.h>

enum fit_status {
    FIT_OK = 0,
    FIT_ERR_FORMAT,           /* not a well-formed FIT */
    FIT_ERR_SIZE,             /* the FDT or an image runs past the FIT's size */
    FIT_ERR_NOT_FOUND,        /* no such image, configuration or entry */
    FIT_ERR_UNSUPPORTED,      /* compression, arch, address size, data-position */
    FIT_ERR_UNSUPPORTED_HASH, /* hash algorithm other than crc32 */
    FIT_ERR_HASH,             /* image data does not match its hash */
};

struct fit {
    const void *fdt;  /* FIT base; only the FDT has to be readable here */
    size_t size;      /* size limit of the whole FIT: FDT plus external data */
    size_t fdt_size;  /* FDT totalsize rounded up to 4; external data starts here */
    int images;       /* node offsets of /images and /configurations */
    int configurations;
};

struct fit_image {
    const char *name; /* unit name */
    const char *type; /* "firmware", ... */
    uint32_t load;
    uint32_t entry;
    bool has_load;
    bool has_entry;
    uint64_t data_offset; /* from the FIT base, for embedded and external data alike */
    uint32_t data_size;
    int node;
};

/*
 * Validate and parse an fdt into the fit structure
 */
enum fit_status fit_open(struct fit *fit, const void *buf, size_t size);

/*
 * Get the node offset of a given configuration
 */
enum fit_status fit_lookup_config(const struct fit *fit, int *cfg);

/*
 * Decode an image into the fit_image struct of a given prop ("firmware" or
 * "loadables")
 */
enum fit_status fit_open_image(const struct fit *fit, int cfg, const char *prop,
                                 unsigned int index, struct fit_image *img);

/*
 * Check `data` (the image's data_size bytes, wherever they were copied to)
 * against every hash subnode of the image. No hash subnodes is a pass.
 */
enum fit_status fit_image_verify_crc(const struct fit *fit, const struct fit_image *img,
                                      const void *data);

const char *fit_status_str(enum fit_status status);
