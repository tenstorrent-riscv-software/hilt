/*
 * SPDX-License-Identifier: Apache-2.0
 * SPDX-FileCopyrightText: 2025-2026 Tenstorrent USA, Inc.
 */

#include <types.h>
#include <errno.h>
#include <timer.h>
#include <drivers/dwc_ssi.h>
#include <drivers/spi_nor.h>

#define SPI_NOR_OP_RDID    0x9fu /* read JEDEC id */
#define SPI_NOR_OP_RDSR    0x05u /* read status register */
#define SPI_NOR_OP_READ    0x03u /* read, 3-byte address, no dummy cycles */
#define SPI_NOR_OP_READ_4B 0x13u /* read, 4-byte address */

/* Largest part reachable with 3-byte addresses. */
#define SPI_NOR_3B_MAX_SIZE (16u << 20)

#define SPI_NOR_CAPACITY_MIN 16 /* 64 KiB */
#define SPI_NOR_CAPACITY_MAX 27 /* 128 MiB */

static int nor_cmd(const struct spi_nor *nor, uint8_t opcode, uint32_t addr,
                   uint8_t addr_bytes, uint8_t *rx, size_t rx_len)
{
    struct dwc_ssi_cmd cmd = {
        .opcode = opcode,
        .addr_bytes = addr_bytes,
        .addr = addr,
        .rx = rx,
        .rx_len = rx_len,
    };

    return dwc_ssi_cmd(nor->ssi, &cmd);
}

static uint8_t nor_addr_bytes(const struct spi_nor *nor)
{
    return nor->size > SPI_NOR_3B_MAX_SIZE ? 4 : 3;
}

static uint8_t nor_opcode(const struct spi_nor *nor, uint8_t op_3b, uint8_t op_4b)
{
    return nor_addr_bytes(nor) == 4 ? op_4b : op_3b;
}

static bool nor_in_range(const struct spi_nor *nor, uint32_t off, size_t len)
{
    return len <= nor->size && off <= nor->size - len;
}

int spi_nor_read_status(const struct spi_nor *nor, uint8_t *sr)
{
    return nor_cmd(nor, SPI_NOR_OP_RDSR, 0, 0, sr, 1);
}

int spi_nor_init(struct spi_nor *nor, const struct dwc_ssi *ssi)
{
    uint8_t id[3];
    int rc;

    nor->ssi = ssi;
    nor->jedec_id = 0;
    nor->size = 0;

    rc = nor_cmd(nor, SPI_NOR_OP_RDID, 0, 0, id, sizeof(id));
    if (rc) {
        return rc;
    }

    nor->jedec_id = ((uint32_t)id[0] << 16) | ((uint32_t)id[1] << 8) | id[2];
    /* All zeros or all ones is a floating bus, not a part. */
    if (nor->jedec_id == 0 || nor->jedec_id == 0xffffff) {
        return -ENODEV;
    }
    if (id[2] < SPI_NOR_CAPACITY_MIN || id[2] > SPI_NOR_CAPACITY_MAX) {
        return -ENODEV;
    }
    nor->size = 1u << id[2];

    return 0;
}

int spi_nor_read(const struct spi_nor *nor, uint32_t off, void *buf, size_t len)
{
    if (!nor_in_range(nor, off, len)) {
        return -EINVAL;
    }
    if (len == 0) {
        return 0;
    }

    return nor_cmd(nor, nor_opcode(nor, SPI_NOR_OP_READ, SPI_NOR_OP_READ_4B),
                   off, nor_addr_bytes(nor), buf, len);
}
