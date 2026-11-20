/*
 * SPDX-License-Identifier: Apache-2.0
 * SPDX-FileCopyrightText: 2025-2026 Tenstorrent USA, Inc.
 */

#include "image/fit.h"

#include <crc32.h>
#include <libfdt.h>
#include <string.h>

/* A property that must be one NUL-terminated string; NULL if absent or malformed. */
static const char *prop_string(const void *fdt, int node, const char *name)
{
    int len;
    const char *s = fdt_getprop(fdt, node, name, &len);

    if (!s || len < 1 || s[len - 1] != '\0')
        return NULL;
    return s;
}

/* An optional 32-bit cell. FIT_ERR_FORMAT if present with the wrong size. */
static enum fit_status prop_u32(const void *fdt, int node, const char *name,
                                uint32_t *val, bool *present)
{
    int len;
    const fdt32_t *cell = fdt_getprop(fdt, node, name, &len);

    *present = false;
    if (!cell)
        return FIT_OK;
    if (len != (int)sizeof(*cell))
        return FIT_ERR_FORMAT;
    *val = fdt32_to_cpu(*cell);
    *present = true;
    return FIT_OK;
}

enum fit_status fit_open(struct fit *fit, const void *buf, size_t size)
{
    memset(fit, 0, sizeof(*fit));

    if (size < sizeof(struct fdt_header))
        return FIT_ERR_SIZE;
    if (fdt_check_header(buf) != 0)
        return FIT_ERR_FORMAT;

    size_t total = fdt_totalsize(buf);
    if (total > size)
        return FIT_ERR_SIZE;
    if (fdt_check_full(buf, total) != 0)
        return FIT_ERR_FORMAT;

    /* The RCPU is 32-bit: load and entry are one cell. */
    uint32_t cells;
    bool present;
    if (prop_u32(buf, 0, "#address-cells", &cells, &present) != FIT_OK)
        return FIT_ERR_FORMAT;
    if (present && cells != 1)
        return FIT_ERR_UNSUPPORTED;

    int images = fdt_path_offset(buf, "/images");
    int configurations = fdt_path_offset(buf, "/configurations");
    if (images < 0 || configurations < 0)
        return FIT_ERR_FORMAT;

    fit->fdt = buf;
    fit->size = size;
    fit->fdt_size = (total + 3) & ~(size_t)3;
    fit->images = images;
    fit->configurations = configurations;
    return FIT_OK;
}

enum fit_status fit_lookup_config(const struct fit *fit, int *cfg)
{
    const char *name = prop_string(fit->fdt, fit->configurations, "default");
    int node;

    if (name)
        node = fdt_subnode_offset(fit->fdt, fit->configurations, name);
    else
        node = fdt_first_subnode(fit->fdt, fit->configurations);

    if (node < 0)
        return FIT_ERR_NOT_FOUND;
    *cfg = node;
    return FIT_OK;
}

static enum fit_status decode_image(const struct fit *fit, int node, struct fit_image *img)
{
    const void *fdt = fit->fdt;
    enum fit_status status;
    const char *s;
    int len;

    memset(img, 0, sizeof(*img));
    img->node = node;

    img->name = fdt_get_name(fdt, node, NULL);
    img->type = prop_string(fdt, node, "type");
    if (!img->name || !img->type)
        return FIT_ERR_FORMAT;

    s = prop_string(fdt, node, "compression");
    if (!s || strcmp(s, "none") != 0)
        return FIT_ERR_UNSUPPORTED;

    if (fdt_getprop(fdt, node, "arch", &len)) {
        s = prop_string(fdt, node, "arch");
        if (!s || strcmp(s, "riscv") != 0)
            return FIT_ERR_UNSUPPORTED;
    }

    status = prop_u32(fdt, node, "load", &img->load, &img->has_load);
    if (status != FIT_OK)
        return status;
    status = prop_u32(fdt, node, "entry", &img->entry, &img->has_entry);
    if (status != FIT_OK)
        return status;

    /* Data lives in the FDT, or after it at data-offset; never at a fixed address. */
    if (fdt_getprop(fdt, node, "data-position", &len))
        return FIT_ERR_UNSUPPORTED;

    const uint8_t *data = fdt_getprop(fdt, node, "data", &len);
    if (data) {
        img->data_offset = (uint64_t)(data - (const uint8_t *)fdt);
        img->data_size = (uint32_t)len;
        if (img->data_offset + img->data_size > fdt_totalsize(fdt))
            return FIT_ERR_FORMAT;
    } else {
        uint32_t offset, size;
        bool have_offset, have_size;

        status = prop_u32(fdt, node, "data-offset", &offset, &have_offset);
        if (status != FIT_OK)
            return status;
        status = prop_u32(fdt, node, "data-size", &size, &have_size);
        if (status != FIT_OK)
            return status;
        if (!have_offset || !have_size)
            return FIT_ERR_FORMAT;

        img->data_offset = (uint64_t)fit->fdt_size + offset;
        img->data_size = size;
    }

    if (img->data_offset + img->data_size > fit->size)
        return FIT_ERR_SIZE;

    return FIT_OK;
}

enum fit_status fit_open_image(const struct fit *fit, int cfg, const char *prop,
                                 unsigned int index, struct fit_image *img)
{
    int len;
    const char *name = fdt_stringlist_get(fit->fdt, cfg, prop, (int)index, &len);

    if (!name)
        return len == -FDT_ERR_NOTFOUND ? FIT_ERR_NOT_FOUND : FIT_ERR_FORMAT;

    int node = fdt_subnode_offset(fit->fdt, fit->images, name);

    if (node < 0)
        return FIT_ERR_NOT_FOUND;

    return decode_image(fit, node, img);
}

enum fit_status fit_image_verify_crc(const struct fit *fit, const struct fit_image *img,
                                      const void *data)
{
    const void *fdt = fit->fdt;
    int node;

    fdt_for_each_subnode(node, fdt, img->node) {
        const char *name = fdt_get_name(fdt, node, NULL);
        if (!name || strncmp(name, "hash", 4) != 0)
            continue;

        const char *algo = prop_string(fdt, node, "algo");
        if (!algo)
            return FIT_ERR_FORMAT;
        if (strcmp(algo, "crc32") != 0)
            return FIT_ERR_UNSUPPORTED_HASH;

        uint32_t value;
        bool present;
        if (prop_u32(fdt, node, "value", &value, &present) != FIT_OK || !present)
            return FIT_ERR_FORMAT;
        if (crc32_calc(data, img->data_size) != value)
            return FIT_ERR_HASH;
    }
    if (node < 0 && node != -FDT_ERR_NOTFOUND)
        return FIT_ERR_FORMAT;

    return FIT_OK;
}

const char *fit_status_str(enum fit_status status)
{
    switch (status) {
    case FIT_OK:
        return "ok";
    case FIT_ERR_FORMAT:
        return "malformed FIT";
    case FIT_ERR_SIZE:
        return "FIT larger than its slot";
    case FIT_ERR_NOT_FOUND:
        return "image or configuration not found";
    case FIT_ERR_UNSUPPORTED:
        return "unsupported image property";
    case FIT_ERR_UNSUPPORTED_HASH:
        return "unsupported hash algorithm";
    case FIT_ERR_HASH:
        return "hash mismatch";
    }
    return "unknown";
}
