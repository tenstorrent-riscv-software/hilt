/*
 * SPDX-License-Identifier: Apache-2.0
 * SPDX-FileCopyrightText: 2025-2026 Tenstorrent USA, Inc.
 */

#include <types.h>
#include <errno.h>
#include <cpu.h>
#include <mmio.h>
#include <timer.h>
#include <drivers/dwc_ssi.h>

#define SSI_CTRLR0     0x00u /* control register 0 */
#define SSI_CTRLR1     0x04u /* control register 1: frames to receive */
#define SSI_SSIENR     0x08u /* enable */
#define SSI_SER        0x10u /* slave enable */
#define SSI_BAUDR      0x14u /* serial clock divider */
#define SSI_TXFTLR     0x18u /* TX FIFO thresholds */
#define SSI_RXFTLR     0x1cu /* RX FIFO threshold */
#define SSI_TXFLR      0x20u /* TX FIFO level */
#define SSI_RXFLR      0x24u /* RX FIFO level */
#define SSI_SR         0x28u /* status */
#define SSI_IMR        0x2cu /* interrupt mask */
#define SSI_ICR        0x48u /* interrupt clear */
#define SSI_IDR        0x58u /* identification */
#define SSI_VERSION_ID 0x5cu /* component version */
#define SSI_DR         0x60u /* data register, both FIFOs */
#define SSI_SPI_CTRLR0 0xf4u /* SPI control register 0 */

#define SSI_CTRLR0_DFS(bits)    ((uint32_t)((bits) - 1) << 0) /* frame size */
#define SSI_CTRLR0_FRF_SPI      (0U << 6)  /* Motorola SPI */
#define SSI_CTRLR0_SCPH         (1U << 8)  /* clock phase */
#define SSI_CTRLR0_SCPOL        (1U << 9)  /* clock polarity */
#define SSI_CTRLR0_TMOD_TX_RX   (0U << 10)
#define SSI_CTRLR0_TMOD_TX_ONLY (1U << 10)
#define SSI_CTRLR0_TMOD_RX_ONLY (2U << 10)
#define SSI_CTRLR0_TMOD_EEPROM  (3U << 10)
#define SSI_CTRLR0_SSTE         (1U << 14) /* toggle slave select per frame */
#define SSI_CTRLR0_SPI_FRF_STD  (0U << 22) /* single lane */
#define SSI_CTRLR0_SSI_IS_MST   (1U << 31)

#define SSI_CTRLR1_NDF(frames)  ((uint32_t)((frames) - 1))

#define SSI_SSIENR_EN           (1U << 0)

#define SSI_SER_SS0             (1U << 0)

#define SSI_BAUDR_MIN           2U
#define SSI_BAUDR_MAX           65534U

#define SSI_TXFTLR_TFT(n)       ((uint32_t)(n))       /* empty threshold */
#define SSI_TXFTLR_TXFTHR(n)    ((uint32_t)(n) << 16) /* transfer start level */
#define SSI_RXFTLR_RFT(n)       ((uint32_t)(n))

#define SSI_SR_BUSY (1U << 0)
#define SSI_SR_TFNF (1U << 1) /* TX FIFO not full */
#define SSI_SR_TFE  (1U << 2) /* TX FIFO empty */
#define SSI_SR_RFNE (1U << 3) /* RX FIFO not empty */
#define SSI_SR_RFF  (1U << 4) /* RX FIFO full */

#define SSI_SPI_CTRLR0_TRANS_TYPE_STD (0U << 0) /* instruction and address on one lane */
#define SSI_SPI_CTRLR0_ADDR_L(bits)   ((uint32_t)((bits) / 4) << 2)
#define SSI_SPI_CTRLR0_INST_L_NONE    (0U << 8)
#define SSI_SPI_CTRLR0_INST_L_8       (2U << 8)
#define SSI_SPI_CTRLR0_WAIT_CYCLES(n) ((uint32_t)(n) << 11)

/* Per status wait. A full FIFO of 32-bit frames takes 6.6 ms at 1.25 MHz. */
#define SSI_POLL_TIMEOUT_US 100000

static uint32_t ssi_read(const struct dwc_ssi *ssi, unsigned int reg)
{
    return mmio_read32(ssi->base + reg);
}

static void ssi_write(const struct dwc_ssi *ssi, unsigned int reg, uint32_t val)
{
    mmio_write32(ssi->base + reg, val);
}

static int ssi_wait_sr(const struct dwc_ssi *ssi, uint32_t mask, uint32_t want)
{
    uint64_t timeout;

    timeout_start(&timeout, SSI_POLL_TIMEOUT_US);
    for (;;) {
        if ((ssi_read(ssi, SSI_SR) & mask) == want) {
            return 0;
        }
        if (timeout_expired(&timeout)) {
            return -ETIMEDOUT;
        }
        cpu_relax();
    }
}

/* Wait for idle then disable. */
static int ssi_disable(const struct dwc_ssi *ssi)
{
    int rc = ssi_wait_sr(ssi, SSI_SR_BUSY, 0);

    /* Try to disable even if there is a timeout */
    ssi_write(ssi, SSI_SSIENR, 0);
    return rc;
}

/*
 * Program a transfer and enable the controller. The controller must be
 * disabled prior to calling.
 */
static void ssi_program(const struct dwc_ssi *ssi, uint32_t ctrlr0,
                        unsigned int frames, uint32_t spi_ctrlr0,
                        uint32_t txftlr)
{
    /* Mode 0, slave select held low across frames (SSTE clear). */
    ssi_write(ssi, SSI_CTRLR0, ctrlr0 | SSI_CTRLR0_SSI_IS_MST |
                                   SSI_CTRLR0_FRF_SPI | SSI_CTRLR0_SPI_FRF_STD);
    ssi_write(ssi, SSI_CTRLR1, SSI_CTRLR1_NDF(frames));
    ssi_write(ssi, SSI_SPI_CTRLR0, spi_ctrlr0);
    ssi_write(ssi, SSI_TXFTLR, txftlr);
    ssi_write(ssi, SSI_RXFTLR, SSI_RXFTLR_RFT(0));
    ssi_write(ssi, SSI_BAUDR, ssi->baudr);
    ssi_write(ssi, SSI_SER, SSI_SER_SS0);
    ssi_write(ssi, SSI_SSIENR, SSI_SSIENR_EN);
}

/*
 * Receive frames * frame_bytes bytes from addr. The chunk must fit in the
 * RX FIFO: it is received in full before being drained, so a slow CPU
 * cannot overrun the FIFO.
 */
static int ssi_rx_chunk(const struct dwc_ssi *ssi, const struct dwc_ssi_cmd *cmd,
                        uint32_t addr, uint8_t *buf, unsigned int frames,
                        unsigned int frame_bytes)
{
    int rc;

    rc = ssi_disable(ssi);
    if (rc) {
        return rc;
    }

    ssi_program(ssi, SSI_CTRLR0_TMOD_RX_ONLY | SSI_CTRLR0_DFS(8 * frame_bytes),
                frames,
                SSI_SPI_CTRLR0_TRANS_TYPE_STD | SSI_SPI_CTRLR0_INST_L_8 |
                    SSI_SPI_CTRLR0_ADDR_L(8 * cmd->addr_bytes) |
                    SSI_SPI_CTRLR0_WAIT_CYCLES(0),
                SSI_TXFTLR_TFT(0) | SSI_TXFTLR_TXFTHR(0));

    /* The transfer starts once the instruction and address words are queued. */
    ssi_write(ssi, SSI_DR, cmd->opcode);
    if (cmd->addr_bytes) {
        ssi_write(ssi, SSI_DR, addr);
    }

    rc = ssi_wait_sr(ssi, SSI_SR_BUSY | SSI_SR_RFNE, SSI_SR_RFNE);
    if (rc) {
        ssi_disable(ssi);
        return rc;
    }

    for (unsigned int f = 0; f < frames; f++) {
        uint32_t word;

        if (!(ssi_read(ssi, SSI_SR) & SSI_SR_RFNE)) {
            rc = -EIO;
            break;
        }
        word = ssi_read(ssi, SSI_DR);
        /* Frames shift MSB first: the first byte off the wire is the top byte. */
        for (unsigned int i = frame_bytes; i-- > 0;) {
            *buf++ = (word >> (8 * i)) & 0xff;
        }
    }

    if (rc) {
        ssi_disable(ssi);
        return rc;
    }

    return ssi_disable(ssi);
}

static int ssi_rx(const struct dwc_ssi *ssi, const struct dwc_ssi_cmd *cmd)
{
    uint8_t *buf = cmd->rx;
    size_t remaining = cmd->rx_len;
    uint32_t addr = cmd->addr;

    /* Without an address a second chunk would restart the command. */
    if (cmd->addr_bytes == 0 && remaining > ssi->fifo_depth) {
        return -EINVAL;
    }

    while (remaining) {
        unsigned int frame_bytes = (cmd->addr_bytes && remaining >= 4) ? 4 : 1;
        size_t frames = remaining / frame_bytes;
        size_t n;
        int rc;

        if (frames > ssi->fifo_depth) {
            frames = ssi->fifo_depth;
        }
        n = frames * frame_bytes;

        rc = ssi_rx_chunk(ssi, cmd, addr, buf, frames, frame_bytes);
        if (rc) {
            return rc;
        }

        buf += n;
        addr += n;
        remaining -= n;
    }

    return 0;
}

int dwc_ssi_cmd(const struct dwc_ssi *ssi, const struct dwc_ssi_cmd *cmd)
{
    if (cmd->addr_bytes != 0 && cmd->addr_bytes != 3 && cmd->addr_bytes != 4) {
        return -EINVAL;
    }
    if (!cmd->rx_len || !cmd->rx) {
        return -EINVAL;
    }

    return ssi_rx(ssi, cmd);
}

int dwc_ssi_init(struct dwc_ssi *ssi, uintptr_t base, uint32_t ref_hz,
                 uint32_t max_sclk_hz, unsigned int fifo_depth)
{
    uint32_t div;
    int rc;

    if (!ref_hz || !max_sclk_hz) {
        return -EINVAL;
    }

    ssi->base = base;

    if (fifo_depth < 2 || fifo_depth > 256) {
        return -EINVAL;
    }
    ssi->fifo_depth = fifo_depth;

    /* sclk = ref / BAUDR, BAUDR even. Round up so sclk never exceeds the max. */
    div = (ref_hz + max_sclk_hz - 1) / max_sclk_hz;
    if (div < SSI_BAUDR_MIN) {
        div = SSI_BAUDR_MIN;
    }
    if (div & 1) {
        div++;
    }
    if (div > SSI_BAUDR_MAX) {
        div = SSI_BAUDR_MAX;
    }
    ssi->baudr = div;
    ssi->sclk_hz = ref_hz / div;

    /*
     * The IP is configured with SSIC_BOOT_MODE_EN, meaning that SSIC_EN is set
     * to 1 on reset. The bootrom will already have cleared it.
     */
    rc = ssi_disable(ssi);
    if (rc) {
        return rc;
    }

    /* Polled only. No interrupts enabled */
    ssi_write(ssi, SSI_IMR, 0);
    return 0;
}

int dwc_ssi_shutdown(const struct dwc_ssi *ssi)
{
    int rc = ssi_disable(ssi);

    /* Reset registers to their reset state. This makes u-boot happy. */
    ssi_write(ssi, SSI_SPI_CTRLR0, 0);
    ssi_write(ssi, SSI_CTRLR1, 0);
    ssi_write(ssi, SSI_TXFTLR, 0);
    ssi_write(ssi, SSI_RXFTLR, 0);

    return rc;
}

uint32_t dwc_ssi_version(const struct dwc_ssi *ssi)
{
    return ssi_read(ssi, SSI_VERSION_ID);
}
