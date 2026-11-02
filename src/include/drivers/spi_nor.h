/*
 * SPDX-License-Identifier: Apache-2.0
 * SPDX-FileCopyrightText: 2025-2026 Tenstorrent USA, Inc.
 */

#ifndef __DRIVERS_SPI_NOR_H__
#define __DRIVERS_SPI_NOR_H__

#include <types.h>

struct dwc_ssi;

struct spi_nor {
    const struct dwc_ssi *ssi;
    uint32_t jedec_id;    /* manufacturer << 16 | type << 8 | capacity */
    uint32_t size;        /* bytes */
};

/* Read the JEDEC ID and size the part. -ENODEV if nothing sane answers. */
int spi_nor_init(struct spi_nor *nor, const struct dwc_ssi *ssi);

int spi_nor_read(const struct spi_nor *nor, uint32_t off, void *buf, size_t len);

int spi_nor_read_status(const struct spi_nor *nor, uint8_t *sr);

#endif /* __DRIVERS_SPI_NOR_H__ */
