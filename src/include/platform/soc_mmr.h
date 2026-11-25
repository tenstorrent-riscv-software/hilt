/*
 * SPDX-License-Identifier: Apache-2.0
 * SPDX-FileCopyrightText: 2025-2026 Tenstorrent USA, Inc.
 *
 * Register map of the Atlantis SoC control block (clocks, resets, boot mode),
 * and accessors for it. Platform code only: drivers do not include this.
 */

#ifndef __PLATFORM_SOC_MMR_H__
#define __PLATFORM_SOC_MMR_H__

#include <types.h>
#include <mmio.h>

#define MMR_SOC_PLL_RCPU_CFG             0x000
#define MMR_SOC_PLL_NOCC_CFG             0x004
#define MMR_SOC_NOCC_CLK_CFG             0x008
#define   SOC_NOCC_CLK_CFG_CLK_SEL    (1U << 0) /* 0: oscillator, 1: PLL */
#define   SOC_NOCC_CLK_CFG_DIV4_SHIFT 5
#define   SOC_NOCC_CLK_CFG_DIV4_MASK  (0xfU << SOC_NOCC_CLK_CFG_DIV4_SHIFT)
#define MMR_SOC_RCPU_DIV_CFG             0x00C
#define   SOC_RCPU_DIV_CFG_CLK_SEL (1U << 0) /* 0: oscillator, 1: PLL */
/* reserved 0x10 */
#define MMR_SOC_RCPU_BLK_CG              0x014
#define MMR_SOC_LSIO_BLK_CG              0x018
#define   SOC_LSIO_BLK_CG_QSPI           (1U << 0)
#define   SOC_LSIO_BLK_CG_I2C0           (1U << 1)
#define   SOC_LSIO_BLK_CG_I2C1           (1U << 2)
#define   SOC_LSIO_BLK_CG_I2C2           (1U << 3)
#define   SOC_LSIO_BLK_CG_I2C3           (1U << 4)
#define   SOC_LSIO_BLK_CG_I2C4           (1U << 5)
#define   SOC_LSIO_BLK_CG_UART0          (1U << 6)
#define   SOC_LSIO_BLK_CG_UART1          (1U << 7)
#define   SOC_LSIO_BLK_CG_UART2          (1U << 8)
#define   SOC_LSIO_BLK_CG_UART3          (1U << 9)
#define   SOC_LSIO_BLK_CG_UART4          (1U << 10)
#define   SOC_LSIO_BLK_CG_SPI0           (1U << 11)
#define   SOC_LSIO_BLK_CG_SPI1           (1U << 12)
#define   SOC_LSIO_BLK_CG_SPI2           (1U << 13)
#define   SOC_LSIO_BLK_CG_SPI3           (1U << 14)
#define   SOC_LSIO_BLK_CG_GPIO           (1U << 15)
#define   SOC_LSIO_BLK_CG_CAN0           (1U << 17)
#define   SOC_LSIO_BLK_CG_CAN1           (1U << 18)
#define   SOC_LSIO_BLK_CG_I2S0           (1U << 19)
#define   SOC_LSIO_BLK_CG_I2S1           (1U << 20)
#define MMR_SOC_RCPU_BLK_RST             0x01C
#define   SOC_RCPU_BLK_RST_DMA0          (1U << 0)
#define   SOC_RCPU_BLK_RST_DMA1          (1U << 1)
#define   SOC_RCPU_BLK_RST_WDT0          (1U << 2)
#define   SOC_RCPU_BLK_RST_WDT1          (1U << 3)
#define   SOC_RCPU_BLK_RST_TMR           (1U << 4)
#define   SOC_RCPU_BLK_RST_PVTC          (1U << 12)
#define   SOC_RCPU_BLK_RST_PMU           (1U << 13)
#define   SOC_RCPU_BLK_RST_IPC           (1U << 14)
#define   SOC_RCPU_BLK_RST_SPACC         (1U << 26)
#define   SOC_RCPU_BLK_RST_OTP           (1U << 28)
#define   SOC_RCPU_BLK_RST_TRNG          (1U << 29)
#define   SOC_RCPU_BLK_RST_CRC           (1U << 30)
#define   SOC_RCPU_BLK_RST_ALL           0x7400701fU
#define MMR_SOC_LSIO_BLK_RST             0x020
#define   SOC_LSIO_BLK_RST_QSPI          (1U << 0)
#define   SOC_LSIO_BLK_RST_I2C0          (1U << 1)
#define   SOC_LSIO_BLK_RST_I2C1          (1U << 2)
#define   SOC_LSIO_BLK_RST_I2C2          (1U << 3)
#define   SOC_LSIO_BLK_RST_I2C3          (1U << 4)
#define   SOC_LSIO_BLK_RST_I2C4          (1U << 5)
#define   SOC_LSIO_BLK_RST_UART0         (1U << 6)
#define   SOC_LSIO_BLK_RST_UART1         (1U << 7)
#define   SOC_LSIO_BLK_RST_UART2         (1U << 8)
#define   SOC_LSIO_BLK_RST_UART3         (1U << 9)
#define   SOC_LSIO_BLK_RST_UART4         (1U << 10)
#define   SOC_LSIO_BLK_RST_SPI0          (1U << 11)
#define   SOC_LSIO_BLK_RST_SPI1          (1U << 12)
#define   SOC_LSIO_BLK_RST_SPI2          (1U << 13)
#define   SOC_LSIO_BLK_RST_SPI3          (1U << 14)
#define   SOC_LSIO_BLK_RST_GPIO          (1U << 15)
#define   SOC_LSIO_BLK_RST_CAN0          (1U << 17)
#define   SOC_LSIO_BLK_RST_CAN1          (1U << 18)
#define   SOC_LSIO_BLK_RST_I2S0          (1U << 19)
#define   SOC_LSIO_BLK_RST_I2S1          (1U << 20)
#define   SOC_LSIO_BLK_RST_ALL           0x001effffU
#define MMR_SOC_GLOBAL_SOFT_RST          0x024
#define MMR_SOC_RCPU_BOOT_ADDR           0x028
#define MMR_SOC_RCPU_MTVEC_ADDR          0x02C
#define MMR_SOC_ATE_TEST_MODE            0x030
#define MMR_SOC_MEM_INTLV_MODE           0x034
#define MMR_SOC_RCPU_RADDR_EXT           0x038
#define MMR_SOC_RCPU_WADDR_EXT           0x03C
/* reserved 0x40 */
#define MMR_SOC_DMA0_HS_SEL0             0x044
#define MMR_SOC_DMA0_HS_SEL1             0x048
#define MMR_SOC_DMA0_HS_SEL2             0x04C
#define MMR_SOC_DMA0_HS_SEL3             0x050
#define MMR_SOC_DMA1_HS_SEL0             0x054
#define MMR_SOC_DMA1_HS_SEL1             0x058
#define MMR_SOC_DMA1_HS_SEL2             0x05C
#define MMR_SOC_DMA1_HS_SEL3             0x060
#define MMR_SOC_RCPU_BLK_RSTN_CTRL       0x064
#define MMR_SOC_LSIO_BLK_RSTN_CTRL       0x068
#define MMR_SOC_RCPU_BLK_RSTN_STS        0x06C
#define MMR_SOC_LSIO_BLK_RSTN_STS        0x070
#define MMR_SOC_QSPI_CTRL                0x074
/* reserved 0x78 */
#define MMR_SOC_RCPU_TIMEOUT             0x07C
#define MMR_SOC_TRNG_EXT_CTRL            0x080
#define MMR_SOC_SPACC_CTRL               0x084
#define MMR_SOC_MISC_OTP_STATUS          0x088
#define MMR_SOC_CAN_FD_ENABLE            0x08C
#define MMR_SOC_CAN_ERROR_COUNT          0x090
#define MMR_SOC_SPI_STATUS               0x094
/* reserved 0x98-0x100 */
#define MMR_SOC_PLL_INT_STA              0x104
/* reserved 0x108 */
#define MMR_SOC_RCPU_AUX_DBG0            0x10C
#define MMR_SOC_RCPU_AUX_DBG1            0x110
#define MMR_SOC_RCPU_AUX_DBG2            0x114
#define MMR_SOC_RCPU_AUX_DBG3            0x118
#define MMR_SOC_PLL_RCPU_EN              0x11C
#define MMR_SOC_PLL_NOCC_EN              0x120
#define MMR_SOC_PLL_MM_EN                0x124
#define MMR_SOC_PLL1_MM_EN               0x128
#define MMR_SOC_PLL_HSIO_EN              0x12C
#define MMR_SOC_PLL_LPDDR0_EN            0x130
#define MMR_SOC_PLL_LPDDR1_EN            0x134
#define MMR_SOC_PLL_PCIE_EN              0x138
#define MMR_SOC_PLL_I2S_EN               0x13C
/* reserved 0x140-0x15C */
#define MMR_SOC_VERSION_ID               0x160
#define MMR_SOC_CHIP_ID                  0x164
/* reserved 0x168-0x198 */
#define MMR_SOC_PLL_MM_CFG               0x19C
#define MMR_SOC_PLL_HSIO_CFG             0x1A0
#define MMR_SOC_PLL_LPDDR0_CFG           0x1A4
#define MMR_SOC_PLL_LPDDR1_CFG           0x1A8
#define MMR_SOC_PLL_PCIE_CFG             0x1AC
#define MMR_SOC_RCPU_BOOT_MODE           0x1B0
#define   SOC_RCPU_BOOT_MODE_RESET_STATUS_MASK          (0xFU << 0)
#define   SOC_RCPU_BOOT_MODE_RESET_STATUS_SHIFT         0
#define   SOC_RCPU_BOOT_MODE_STATUS_POR                 0
#define   SOC_RCPU_BOOT_MODE_STATUS_RESETN              1
#define   SOC_RCPU_BOOT_MODE_STATUS_WDT0                2
#define   SOC_RCPU_BOOT_MODE_STATUS_WDT1                3
#define   SOC_RCPU_BOOT_MODE_STATUS_GLOBAL_SOFT_RESET   4
#define   SOC_RCPU_BOOT_MODE_STATUS_RCPU_SOFT_RESET     5
#define   SOC_RCPU_BOOT_MODE_STATUS_NDM_RESET           6
#define   SOC_RCPU_BOOT_MODE_BOOT_SEL_MASK              (0x3U << 4)
#define   SOC_RCPU_BOOT_MODE_BOOT_SEL_SHIFT             4
#define   SOC_RCPU_BOOT_MODE_SEL_UART                   0
#define   SOC_RCPU_BOOT_MODE_SEL_NOR                    1
#define   SOC_RCPU_BOOT_MODE_SEL_SD                     2
#define   SOC_RCPU_BOOT_MODE_SEL_INVALID                3
#define MMR_SOC_RCPU_PLL_FRAC            0x1B4
#define MMR_SOC_NOCC_PLL_FRAC            0x1B8
#define MMR_SOC_HSIO_PLL_FRAC            0x1BC
#define MMR_SOC_LPDDR0_PLL_FRAC          0x1C0
#define MMR_SOC_LPDDR1_PLL_FRAC          0x1C4
#define MMR_SOC_PCIE_PLL_FRAC            0x1C8
#define MMR_SOC_MM_PLL_FRAC              0x1CC
#define MMR_SOC_MM_PLL1_FRAC             0x1D0
/* reserved 0x1D4-0x1F0 */
#define MMR_SOC_PLL_I2S_CFG              0x1F4
#define MMR_SOC_I2S_DIV_CFG              0x1F8
#define MMR_SOC_BUS_CG                   0x1FC
#define   SOC_BUS_CG_NOCC                (1U << 0)
#define   SOC_BUS_CG_PCIE                (1U << 1)
#define   SOC_BUS_CG_MM                  (1U << 2)
#define   SOC_BUS_CG_HSIO                (1U << 3)
#define   SOC_BUS_CG_DDRC0               (1U << 4)
#define   SOC_BUS_CG_DDRC1               (1U << 5)
#define   SOC_BUS_CG_MM_BUS1             (1U << 6)
#define   SOC_BUS_CG_I2S                 (1U << 8)
#define MMR_SOC_BUS_RSTN                 0x200
#define   SOC_BUS_RSTN_NOCC_CLK          (1U << 0)
#define   SOC_BUS_RSTN_PCIE_CLK          (1U << 1)
#define   SOC_BUS_RSTN_MM_CLK            (1U << 2)
#define   SOC_BUS_RSTN_HSIO_CLK          (1U << 3)
#define   SOC_BUS_RSTN_DDRC0_CLK         (1U << 4)
#define   SOC_BUS_RSTN_DDRC1_CLK         (1U << 5)
#define   SOC_BUS_RSTN_NOCC_CLK_STATUS   (1U << 16)
#define   SOC_BUS_RSTN_PCIE_CLK_STATUS   (1U << 17)
#define   SOC_BUS_RSTN_MM_CLK_STATUS     (1U << 18)
#define   SOC_BUS_RSTN_HSIO_CLK_STATUS   (1U << 19)
#define   SOC_BUS_RSTN_DDRC0_CLK_STATUS  (1U << 20)
#define   SOC_BUS_RSTN_DDRC1_CLK_STATUS  (1U << 21)
#define MMR_SOC_I2S_PLL_TEST_CFG         0x204
#define MMR_SOC_I2S_PLL_SKEW_CFG         0x208
#define MMR_SOC_RCPU_PLL_TEST_CFG        0x20C
#define MMR_SOC_RCPU_PLL_SKEW_CFG        0x210
#define MMR_SOC_NOCC_PLL_TEST_CFG        0x214
#define MMR_SOC_NOCC_PLL_SKEW_CFG        0x218
#define MMR_SOC_HSIO_PLL_TEST_CFG        0x21C
#define MMR_SOC_HSIO_PLL_SKEW_CFG        0x220
#define MMR_SOC_PCIE_PLL_TEST_CFG        0x224
#define MMR_SOC_PCIE_PLL_SKEW_CFG        0x228
#define MMR_SOC_MM_PLL_TEST_CFG          0x22C
#define MMR_SOC_MM_PLL_SKEW_CFG          0x230
#define MMR_SOC_LPDDR0_PLL_TEST_CFG      0x234
#define MMR_SOC_LPDDR0_PLL_SKEW_CFG      0x238
#define MMR_SOC_LPDDR1_PLL_TEST_CFG      0x23C
#define MMR_SOC_LPDDR1_PLL_SKEW_CFG      0x240
#define MMR_SOC_RCPU_SOFT_RST            0x244
#define MMR_SOC_RCPU_SOURCE_ID           0x248
/* reserved 0x24C-0x260 */
#define MMR_SOC_NOCC_PMU_STATUS_0        0x264
#define MMR_SOC_NOCC_PMU_STATUS_1        0x268
#define MMR_SOC_DMA0_M0_USER             0x26C
#define MMR_SOC_DMA0_M1_USER             0x270
#define MMR_SOC_DMA1_M0_USER             0x274
#define MMR_SOC_DMA1_M1_USER             0x278
#define MMR_SOC_RCPU_USER                0x27C
#define MMR_SOC_SPACC_M_REQUSER          0x280
/* reserved 0x284-0x2A0 */
#define MMR_SOC_PLL1_MM_CFG              0x2A4
#define MMR_SOC_MM_PLL1_TEST_CFG         0x2A8
#define MMR_SOC_MM_PLL1_SKEW_CFG         0x2AC
#define MMR_SOC_LSIO_PMU_STATUS_0        0x2B0
#define MMR_SOC_LSIO_PMU_STATUS_1        0x2B4
/* reserved 0x2B8-0x2FC */
#define MMR_SOC_APLIC_BLK_RST            0x300
#define   SOC_APLIC_BLK_RST_APLIC        (1U << 0)
/* reserved 0x304-0x3FC */
#define MMR_SOC_DFD_TOP_CTRL             0x400
#define MMR_SOC_DFD_TOP_ASC_OCC          0x404
#define MMR_SOC_DFD_TOP_APLIC_OCC        0x408
#define MMR_SOC_DFD_TOP_PCIE_IOMMU_OCC   0x40C
#define MMR_SOC_DFD_TOP_MM_IOMMU_OCC     0x410
#define MMR_SOC_DFD_TOP_HSIO_IOMMU_OCC   0x414

/* MM subsystem control block */
#define MMR_MM_RSTN                      0x014
#define   MM_RSTN_DSIO_VO                (1U << 0)
#define   MM_RSTN_VPU_BUS                (1U << 1)
#define   MM_RSTN_DSI                    (1U << 2)
#define   MM_RSTN_HDMI                   (1U << 3)
#define   MM_RSTN_GPU_CORE               (1U << 4)
#define   MM_RSTN_HDMI_VO                (1U << 5)
#define   MM_RSTN_VPU_CORE               (1U << 6)
#define   MM_RSTN_ALL                    0x0000007fU

/* HSIO subsystem control block */
#define MMR_HSIO_BLK_RST                 0x00C
#define   HSIO_BLK_RST_EMMC0             (1U << 0)
#define   HSIO_BLK_RST_EMMC1             (1U << 1)
#define   HSIO_BLK_RST_SDIO0             (1U << 2)
#define   HSIO_BLK_RST_SDIO1             (1U << 3)
#define   HSIO_BLK_RST_DMA0              (1U << 6)
#define   HSIO_BLK_RST_DMA1              (1U << 7)
#define   HSIO_BLK_RST_U2C0              (1U << 8)
#define   HSIO_BLK_RST_U2C1              (1U << 9)
#define   HSIO_BLK_RST_U3C0              (1U << 10)
#define   HSIO_BLK_RST_U3C1              (1U << 11)
#define   HSIO_BLK_RST_ALL               0x00000fcfU
#define MMR_HSIO_GMAC_CRG_CFG            0x010
#define   HSIO_GMAC_CRG_CFG_GMAC0_XRSTN  (1U << 0)
#define   HSIO_GMAC_CRG_CFG_GMAC1_XRSTN  (1U << 1)
#define   HSIO_GMAC_CRG_CFG_XRSTN_ALL    0x00000003U

#define SOC_MMR_BASE    0x40000000
#define MM_MMR_BASE     0xB0000000
#define HSIO_MMR_BASE   0xB40C0000

#define CLOCK_GATE_DELAY_US 1000

static inline uint32_t soc_mmr_read(unsigned int reg)
{
    return mmio_read32(SOC_MMR_BASE + reg);
}

static inline void soc_mmr_write(unsigned int reg, uint32_t val)
{
    mmio_write32(SOC_MMR_BASE + reg, val);
}

/* The MM bus has to be out of reset and clocked for these to complete. */
static inline uint32_t mm_mmr_read(unsigned int reg)
{
    return mmio_read32(MM_MMR_BASE + reg);
}

static inline void mm_mmr_write(unsigned int reg, uint32_t val)
{
    mmio_write32(MM_MMR_BASE + reg, val);
}

/* The HSIO bus has to be out of reset and clocked for these to complete. */
static inline uint32_t hsio_mmr_read(unsigned int reg)
{
    return mmio_read32(HSIO_MMR_BASE + reg);
}

static inline void hsio_mmr_write(unsigned int reg, uint32_t val)
{
    mmio_write32(HSIO_MMR_BASE + reg, val);
}

#endif /* __PLATFORM_SOC_MMR_H__ */
