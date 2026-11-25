/*
 * SPDX-License-Identifier: Apache-2.0
 * SPDX-FileCopyrightText: 2025-2026 Tenstorrent USA, Inc.
 */

#include <types.h>
#include <errno.h>
#include <mmio.h>
#include <stdio.h>
#include <timer.h>
#include <platform/clock.h>
#include <platform/reset.h>
#include <platform/soc_mmr.h>

/* A set bit opens the gate, i.e. the clock runs. */
static void clock_set_cg_enable(uintptr_t addr, uint32_t mask, bool enable)
{
    uint32_t reg = mmio_read32(addr);

    if (enable) {
        reg |= mask;
    } else {
        reg &= ~mask;
    }
    mmio_write32(addr, reg);
    udelay(CLOCK_GATE_DELAY_US);
}

void clock_lsio_set_cg_enable(uint32_t mask, bool enable)
{
    clock_set_cg_enable(SOC_MMR_BASE + MMR_SOC_LSIO_BLK_CG, mask, enable);
}

void clock_bus_set_cg_enable(uint32_t mask, bool enable)
{
    clock_set_cg_enable(SOC_MMR_BASE + MMR_SOC_BUS_CG, mask, enable);
}

void clock_rcpu_set_cg_enable(uint32_t mask, bool enable)
{
    clock_set_cg_enable(SOC_MMR_BASE + MMR_SOC_RCPU_BLK_CG, mask, enable);
}

void clock_mm_set_cg_enable(uint32_t mask, bool enable)
{
    clock_set_cg_enable(MM_CTRL_BASE + MMR_MM_CLK_GATE_CFG, mask, enable);
}

void clock_hsio_set_cg_enable(uint32_t mask, bool enable)
{
    clock_set_cg_enable(HSIO_CTRL_BASE + MMR_HSIO_BLK_CG, mask, enable);
}

void clock_gmac_set_cg_enable(uint32_t mask, bool enable)
{
    clock_set_cg_enable(HSIO_CTRL_BASE + MMR_HSIO_GMAC_CRG_CFG, mask, enable);
}

void clock_pcie_set_cg_enable(uint32_t mask, bool enable)
{
    clock_set_cg_enable(PCIE_MMR_BASE + MMR_PCIE_SUBS_CG_CTL, mask, enable);
}

/* All from the 24 MHz oscillator: osc * fb_div / (ref_div * post_div1 * post_div2) */
struct pll_dividers {
    uint32_t ref_div;
    uint32_t post_div1;
    uint32_t post_div2;
    uint32_t fb_div;
};

/* 600 MHz = 24 MHz * 100 / (1 * 4 * 1) */
static const struct pll_dividers rcpu_pll_dividers = {
    .ref_div = 1, .post_div1 = 4, .post_div2 = 1, .fb_div = 100,
};

/* 800 MHz = 24 MHz * 100 / (1 * 3 * 1) */
static const struct pll_dividers nocc_pll_dividers = {
    .ref_div = 1, .post_div1 = 3, .post_div2 = 1, .fb_div = 100,
};


/* 800 MHz = 24 MHz * 100 / (1 * 3 * 1) */
static const struct pll_dividers lpddr_pll_dividers = {
    .ref_div = 1, .post_div1 = 3, .post_div2 = 1, .fb_div = 100,
};

/* 2304 MHz = 24 MHz * 192 / (2 * 1 * 1) */
static const struct pll_dividers mm_pll_dividers = {
    .ref_div = 2, .post_div1 = 1, .post_div2 = 1, .fb_div = 192,
};

/* 800 MHz = 24 MHz * 100 / (1 * 3 * 1) */
static const struct pll_dividers hsio_pll_dividers = {
    .ref_div = 1, .post_div1 = 3, .post_div2 = 1, .fb_div = 100,
};

/* 1500 MHz = 24 MHz * 125 / (2 * 1 * 1) */
static const struct pll_dividers pcie_pll_dividers = {
    .ref_div = 2, .post_div1 = 1, .post_div2 = 1, .fb_div = 125,
};

/* PLL config register */
#define PLL_CFG_BYPASS          (1U << 1)
#define PLL_CFG_REF_DIV_SHIFT   2
#define PLL_CFG_REF_DIV_MASK    (0x3fU << PLL_CFG_REF_DIV_SHIFT)
#define PLL_CFG_POST_DIV1_SHIFT 8
#define PLL_CFG_POST_DIV1_MASK  (0x7U << PLL_CFG_POST_DIV1_SHIFT)
#define PLL_CFG_POST_DIV2_SHIFT 11
#define PLL_CFG_POST_DIV2_MASK  (0x7U << PLL_CFG_POST_DIV2_SHIFT)
#define PLL_CFG_FB_DIV_SHIFT    14
#define PLL_CFG_FB_DIV_MASK     (0xfffU << PLL_CFG_FB_DIV_SHIFT)
#define PLL_CFG_LOCK            (1U << 31)

/* PLL enable register */
#define PLL_EN_PLL_EN (1U << 0) /* 1: PLL is powered on */

#define PLL_LOCK_TIMEOUT_US 100000

static uint64_t pll_frequency(uint32_t osc_frequency, const struct pll_dividers *div)
{
    return (uint64_t)osc_frequency * div->fb_div /
           (div->ref_div * div->post_div1 * div->post_div2);
}

static void pll_set_enable(unsigned long en_reg, bool enable)
 {
    uint32_t reg = mmio_read32(en_reg);

     if (enable) {
        reg |= PLL_EN_PLL_EN;
     } else {
        reg &= ~PLL_EN_PLL_EN;
    }
    mmio_write32(en_reg, reg);
}

static int pll_init(unsigned long cfg_reg, unsigned long en_reg,
             const struct pll_dividers *div)
{
    uint64_t timeout;
    uint32_t reg;

    if ((div->ref_div << PLL_CFG_REF_DIV_SHIFT) & ~PLL_CFG_REF_DIV_MASK ||
        (div->post_div1 << PLL_CFG_POST_DIV1_SHIFT) & ~PLL_CFG_POST_DIV1_MASK ||
        (div->post_div2 << PLL_CFG_POST_DIV2_SHIFT) & ~PLL_CFG_POST_DIV2_MASK ||
        (div->fb_div << PLL_CFG_FB_DIV_SHIFT) & ~PLL_CFG_FB_DIV_MASK) {
        return -EINVAL;
    }

    /* The dividers may only change while the PLL is powered down. */
    pll_set_enable(en_reg, false);

    reg = mmio_read32(cfg_reg);
    reg &= ~(PLL_CFG_BYPASS | PLL_CFG_REF_DIV_MASK | PLL_CFG_POST_DIV1_MASK |
             PLL_CFG_POST_DIV2_MASK | PLL_CFG_FB_DIV_MASK);
    reg |= div->ref_div << PLL_CFG_REF_DIV_SHIFT;
    reg |= div->post_div1 << PLL_CFG_POST_DIV1_SHIFT;
    reg |= div->post_div2 << PLL_CFG_POST_DIV2_SHIFT;
    reg |= div->fb_div << PLL_CFG_FB_DIV_SHIFT;
    mmio_write32(cfg_reg, reg);

    pll_set_enable(en_reg, true);

    timeout_start(&timeout, PLL_LOCK_TIMEOUT_US);
    while (!(mmio_read32(cfg_reg) & PLL_CFG_LOCK)) {
        if (timeout_expired(&timeout)) {
            /* Leave a PLL that did not lock powered down. */
            pll_set_enable(en_reg, false);
            return -ETIMEDOUT;
        }
    }

    return 0;
}

/*
 * Bring up the PLL of a subsystem that sits behind the bus: release its bus
 * reset, program the PLL with the bus clock gated, and open the gate once the
 * PLL has locked. The gate stays closed if the PLL does not lock.
 */
static int bus_pll_init(uint32_t cfg, uint32_t en, uint32_t bus_rstn,
                        uint32_t bus_cg, const struct pll_dividers *div)
{
    int rc;

    reset_bus_set(bus_rstn, false);
    clock_bus_set_cg_enable(bus_cg, false);

    rc = pll_init(SOC_MMR_BASE + cfg, SOC_MMR_BASE + en, div);
    if (rc) {
        return rc;
    }

    clock_bus_set_cg_enable(bus_cg, true);
    return 0;
}

/* There is a PLL for each of the two DDR controllers. */
int clock_lpddr_init(void)
{
    int rc;

    rc = bus_pll_init(MMR_SOC_PLL_LPDDR0_CFG, MMR_SOC_PLL_LPDDR0_EN,
                      SOC_BUS_RSTN_DDRC0_CLK, SOC_BUS_CG_DDRC0, &lpddr_pll_dividers);
    if (rc) {
        return rc;
    }
    return bus_pll_init(MMR_SOC_PLL_LPDDR1_CFG, MMR_SOC_PLL_LPDDR1_EN,
                        SOC_BUS_RSTN_DDRC1_CLK, SOC_BUS_CG_DDRC1, &lpddr_pll_dividers);
}

int clock_mm_init(void)
{
    return bus_pll_init(MMR_SOC_PLL_MM_CFG, MMR_SOC_PLL_MM_EN, SOC_BUS_RSTN_MM_CLK,
                        SOC_BUS_CG_MM, &mm_pll_dividers);
}

int clock_hsio_init(void)
{
    return bus_pll_init(MMR_SOC_PLL_HSIO_CFG, MMR_SOC_PLL_HSIO_EN, SOC_BUS_RSTN_HSIO_CLK,
                        SOC_BUS_CG_HSIO, &hsio_pll_dividers);
}

int clock_pcie_init(void)
{
    return bus_pll_init(MMR_SOC_PLL_PCIE_CFG, MMR_SOC_PLL_PCIE_EN, SOC_BUS_RSTN_PCIE_CLK,
                        SOC_BUS_CG_PCIE, &pcie_pll_dividers);
}

/*
 * The RCPU and NOCC have a mux to select PLL or 24MHz oscillator clock. Select
 * the mux for a given config register.
 */
static void select_pll(uint32_t config, bool pll)
{
    uint32_t reg = soc_mmr_read(config);

    if (pll) {
        reg |= SOC_CFG_CLK_SEL;
    } else {
        reg &= ~SOC_CFG_CLK_SEL;
    }
    soc_mmr_write(config, reg);
}

int clock_rcpu_init(uint32_t osc_frequency, bool haps)
{
    int rc;

    select_pll(MMR_SOC_RCPU_DIV_CFG, false);

    /* The cycle counter that delays and timeouts use follows the RCPU clock. */
    time_init(rcpu_clock_frequency(osc_frequency, haps));

    /* The rcpu has no bus resets to release or clock gating. */
    rc = pll_init(SOC_MMR_BASE + MMR_SOC_PLL_RCPU_CFG,
                  SOC_MMR_BASE + MMR_SOC_PLL_RCPU_EN, &rcpu_pll_dividers);
    if (!rc) {
        /* If PLL locked, switch the RCPU to run off it. */
        select_pll(MMR_SOC_RCPU_DIV_CFG, true);
        time_init(rcpu_clock_frequency(osc_frequency, haps));
    }

    return 0;
}

int clock_nocc_init(void)
{
    int rc;

    select_pll(MMR_SOC_NOCC_CLK_CFG, false);

    /*
     * The platform has most likely already release the bus reset for nocc in
     * order to enable uart prints. Don't rely on this and re-release the reset
     */
    rc = bus_pll_init(MMR_SOC_PLL_NOCC_CFG, MMR_SOC_PLL_NOCC_EN,
                      SOC_BUS_RSTN_NOCC_CLK, SOC_BUS_CG_NOCC, &nocc_pll_dividers);
    if (rc) {
        /* If the PLL failed to lock, leave the nocc running in bypass */
        return rc;
    }

    select_pll(MMR_SOC_NOCC_CLK_CFG, true);
    return 0;
}

/* The Ascalon PLLs run from a 100 MHz reference clock, not the oscillator. */

/* 800 MHz = 100 MHz * 64 / (2 * 2 * 2) */
static const struct pll_dividers asc_fb_pll_dividers = {
    .ref_div = 2, .post_div1 = 2, .post_div2 = 2, .fb_div = 64,
};

/* 400 MHz = 100 MHz * 64 / (4 * 2 * 2) */
static const struct pll_dividers asc_sc_pll_dividers = {
    .ref_div = 4, .post_div1 = 2, .post_div2 = 2, .fb_div = 64,
};

/* Ascalon PLL config is slightly different from the SoC PLL */
#define ASC_PLL_CFG_PLL_EN (1U << 0)
#define ASC_PLL_CFG_DSMEN  (1U << 27)
#define ASC_PLL_CFG_INT_EN (1U << 30)
#define ASC_PLL_STAT_LOCK  (1U << 0)


/* Program an Ascalon PLL, enable it and wait for it to lock. */
static int asc_pll_init(uint32_t cfg, uint32_t stat, const struct pll_dividers *div,
                        uint32_t flags)
{
    uint64_t timeout;
    uint32_t reg;

    reg = flags;
    reg |= div->ref_div << PLL_CFG_REF_DIV_SHIFT;
    reg |= div->post_div1 << PLL_CFG_POST_DIV1_SHIFT;
    reg |= div->post_div2 << PLL_CFG_POST_DIV2_SHIFT;
    reg |= div->fb_div << PLL_CFG_FB_DIV_SHIFT;

    /* The dividers are written with the PLL disabled, then it is enabled. */
    asc_mmr_write(cfg, reg);
    asc_mmr_write(cfg, reg | ASC_PLL_CFG_PLL_EN);

    timeout_start(&timeout, PLL_LOCK_TIMEOUT_US);
    while (!(asc_mmr_read(stat) & ASC_PLL_STAT_LOCK)) {
        if (timeout_expired(&timeout)) {
            return -ETIMEDOUT;
        }
    }
    return 0;
}

static void asc_force_ref_clock(bool force)
{
    uint32_t reg = asc_mmr_read(MMR_ASC_CL_CFG);

    if (force) {
        reg &= ~ASC_CL_CFG_FORCE_SS_TO_REF_CLOCK_N;
    } else {
        reg |= ASC_CL_CFG_FORCE_SS_TO_REF_CLOCK_N;
    }
    asc_mmr_write(MMR_ASC_CL_CFG, reg);
}

int clock_asc_init(void)
{
    uint64_t timeout;
    int fb_rc;
    int sc_rc;

    /* Run all Ascalon clocks from the reference clock while they are set up. */
    asc_force_ref_clock(true);

    /* Run the FB clock from the reference clock while its PLL is set up. */
    asc_mmr_write(MMR_ASC_SS_FB_CLK_MUX,
                  asc_mmr_read(MMR_ASC_SS_FB_CLK_MUX) & ~ASC_SS_FB_CLK_MUX_FB_PLL);

    fb_rc = asc_pll_init(MMR_ASC_FB_PLL_CFG, MMR_ASC_FB_PLL_STAT, &asc_fb_pll_dividers,
                         ASC_PLL_CFG_DSMEN | ASC_PLL_CFG_INT_EN);
    if (!fb_rc) {
        asc_mmr_write(MMR_ASC_SS_FB_CLK_MUX,
                      asc_mmr_read(MMR_ASC_SS_FB_CLK_MUX) | ASC_SS_FB_CLK_MUX_FB_PLL);
    }

    /* The SC PLL does not depend on the FB one, so it is set up either way. */
    sc_rc = asc_pll_init(MMR_ASC_SC_PLL_CFG, MMR_ASC_SC_PLL_STAT, &asc_sc_pll_dividers,
                         0);

    asc_mmr_write(MMR_ASC_MMCAB_CFG0, ASC_MMCAB_CFG0_DEFAULT);

    /* Open all Ascalon subsystem clock gates */
    asc_mmr_write(MMR_ASC_SS_CLK_CG, ASC_SS_CLK_CG_ALL);

    /* Wait for the subsystem to come out of its soft reset. */
    if (asc_mmr_read(MMR_ASC_VERSION_ID) >= ASC_VERSION_ID_SFT_RST_DONE) {
        timeout_start(&timeout, PLL_LOCK_TIMEOUT_US);
        while (!(asc_mmr_read(MMR_ASC_CL_STAT) & ASC_CL_STAT_SFT_RST_DONE)) {
            if (timeout_expired(&timeout)) {
                /* The clocks are left on the reference clock. */
                return -ETIMEDOUT;
            }
        }
    }

    /* Switch the Ascalon clocks to their own source */
    asc_force_ref_clock(false);

    reset_cpl_release();
    return fb_rc ? fb_rc : sc_rc;
}

/* Check whether a subsystem bus has been enabled */
static bool bus_enabled(uint32_t bus_rstn, uint32_t bus_cg)
{
    return (soc_mmr_read(MMR_SOC_BUS_RSTN) & bus_rstn) &&
           (soc_mmr_read(MMR_SOC_BUS_CG) & bus_cg);
}

int clock_device_init(void)
{
    /*
     * The RCPU and LSIO blocks are not behind a bus gate: they also work with
     * the RCPU and NOCC left running in bypass.
     */
    clock_rcpu_set_cg_enable(SOC_RCPU_BLK_CG_ALL, true);
    clock_lsio_set_cg_enable(SOC_LSIO_BLK_CG_ALL, true);

    /*
     * Reset every block and release it again, so one that an earlier stage
     * used does not keep its state. UART0 is left alone: it is the console.
     */
    reset_lsio_set(SOC_LSIO_BLK_RST_ALL & ~SOC_LSIO_BLK_RST_UART0, true);
    reset_rcpu_set(SOC_RCPU_BLK_RST_ALL, true);
    reset_aplic_set(true);

    reset_rcpu_set(SOC_RCPU_BLK_RST_ALL, false);
    reset_lsio_set(SOC_LSIO_BLK_RST_ALL, false);
    reset_aplic_set(false);

    /*
     * For subsystems that sit on buses that need to be enabled, check they are
     * enabled before trying to access the registers in the subsystem. This will
     * result in a bus hang.
     */
    if (bus_enabled(SOC_BUS_RSTN_MM_CLK, SOC_BUS_CG_MM)) {
        clock_mm_set_cg_enable(MM_CLK_GATE_CFG_ALL, true);
        reset_mm_set(MM_RSTN_ALL, true);
        reset_mm_set(MM_RSTN_ALL, false);
    } else {
        printf("%s mm bus is not enabled, skipping its devices\n", __FILE__);
    }

    if (bus_enabled(SOC_BUS_RSTN_HSIO_CLK, SOC_BUS_CG_HSIO)) {
        clock_hsio_set_cg_enable(HSIO_BLK_CG_ALL, true);
        reset_hsio_set(HSIO_BLK_RST_ALL, true);
        reset_hsio_set(HSIO_BLK_RST_ALL, false);

        /* The GMACs have their clock gates and resets in a register of their own */
        clock_gmac_set_cg_enable(HSIO_GMAC_CRG_CFG_XCLK_ALL, true);
        reset_gmac_set(HSIO_GMAC_CRG_CFG_XRSTN_ALL, false);
    } else {
        printf("%s hsio bus is not enabled, skipping its devices\n", __FILE__);
    }

    if (bus_enabled(SOC_BUS_RSTN_PCIE_CLK, SOC_BUS_CG_PCIE)) {
        clock_pcie_set_cg_enable(PCIE_SUBS_CG_CTL_ALL, true);
        reset_pcie_set(PCIE_SUBS_RST_CTL_ALL, true);
        reset_pcie_set(PCIE_SUBS_RST_CTL_ALL, false);
    } else {
        printf("%s pcie bus is not enabled, skipping its devices\n", __FILE__);
    }

    return 0;
}

static bool nocc_on_pll(void)
{
    return soc_mmr_read(MMR_SOC_NOCC_CLK_CFG) & SOC_CFG_CLK_SEL;
}

static uint64_t nocc_clock_frequency(uint32_t osc_frequency, bool haps)
{
    /*
     * The HAPS models have no PLL: the NOCC clock stays at the oscillator rate
     * whichever source is selected.
     */
    if (haps || !nocc_on_pll()) {
        return osc_frequency;
    }
    return pll_frequency(osc_frequency, &nocc_pll_dividers);
}

uint64_t uart_clock_frequency(uint32_t osc_frequency, bool haps)
{
    return nocc_clock_frequency(osc_frequency, haps);
}

uint64_t qspi_clock_frequency(uint32_t osc_frequency, bool haps)
{
    return nocc_clock_frequency(osc_frequency, haps);
}

static bool rcpu_on_pll(void)
{
    return soc_mmr_read(MMR_SOC_RCPU_DIV_CFG) & SOC_CFG_CLK_SEL;
}

uint64_t rcpu_clock_frequency(uint32_t osc_frequency, bool haps)
{
    /*
     * HAPs models have no PLLs. The clock source remains osc_frequency even
     * when PLL is selected.
     */
    if (haps || !rcpu_on_pll()) {
        return osc_frequency;
    }
    return pll_frequency(osc_frequency, &rcpu_pll_dividers);
}
