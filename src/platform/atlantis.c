/*
 * SPDX-License-Identifier: Apache-2.0
 * SPDX-FileCopyrightText: 2025-2026 Tenstorrent USA, Inc.
 */

#include <platform/atlantis.h>
#include <platform/clock.h>
#include <errno.h>
#include <mmio.h>
#include <stdio.h>
#include <timer.h>
#include <drivers/uart_dw_16550.h>
#include <drivers/dwc_ssi.h>
#include <drivers/spi_nor.h>
#include <console.h>
#include <image/boot_image.h>
#include <image/simg.h>
#include <platform/layout.h>
#include <platform/reset.h>
#include <platform/settings_table.h>
#include <platform/soc_mmr.h>

/*
 * __is_bootrom is set to 1 by linker script if this image is compiled for
 * maskrom. Use this to determine if the bootrom or SRAM image is running
 */
extern char __is_bootrom[] __attribute__((weak));

bool platform_atlantis_is_bootrom(void)
{
    return (uintptr_t)__is_bootrom != 0;
}

/* Images to be loaded by the HILT bootrom. */
static const struct boot_image bootrom_images[] = {
    {
        .name = "hilt",
        .is_fit = false,
        .simg.type = SIMG_PAYLOAD_HILT_BOOT,
        .simg.exec = true,
        .simg.domain = SIMG_DOMAIN_ZSBL,
        .load_addr = HILT_BOOT_LOAD_ADDR,
        .max_size = HILT_BOOT_MAX_SIZE,
        .nor_offset = HILT_BOOT_NOR_OFFSET,
    },
};

/*
 * FDT cannot be written before the image since the entry will be incorrect.
 * Setup buffer in hilt to store the FDT while parsing the image
 * */
#define PLATFORM_FW_FDT_MAX_SIZE 0x1000
static uint8_t fdt_buffer[PLATFORM_FW_FDT_MAX_SIZE] __attribute__((aligned(8)));

/* Images to be loaded by the HILT boot stage */
static const struct boot_image fsbl_boot_images[] = {
    {
        .name = "platform-fw",
        .is_fit = true,
        .load_addr = (uintptr_t)fdt_buffer,
        .fdt_max_size = sizeof(fdt_buffer),
        .max_size = PLATFORM_FW_MAX_SIZE,
        .nor_offset = PLATFORM_FW_NOR_OFFSET,
    },
    {
        .name = "ascalon-fw",
        .is_fit = true,
        .load_addr = (uintptr_t)fdt_buffer,
        .fdt_max_size = sizeof(fdt_buffer),
        .max_size = ASCALON_FW_MAX_SIZE,
        .nor_offset = ASCALON_FW_NOR_OFFSET,
    },
};

const struct boot_image *platform_atlantis_boot_images(size_t *count)
{
    if (platform_atlantis_is_bootrom()) {
        *count = sizeof(bootrom_images) / sizeof(bootrom_images[0]);
        return bootrom_images;
    }
    *count = sizeof(fsbl_boot_images) / sizeof(fsbl_boot_images[0]);
    return fsbl_boot_images;
}

#if defined(BUILD_PLATFORM_HAPS_BOOT)
#define PLATFORM_TYPE PLATFORM_HAPS_BOOT
#elif defined(BUILD_PLATFORM_HAPS_PCIE)
#define PLATFORM_TYPE PLATFORM_HAPS_PCIE
#else
#define PLATFORM_TYPE PLATFORM_SILICON
#endif

#ifndef OSC_CLOCK_FREQUENCY
#define OSC_CLOCK_FREQUENCY          (24 * 1000000) /* Default to 24MHz */
#endif

/* Default to 9600 for now, will override based on efuse value */
#ifndef CONSOLE_BAUD
#define CONSOLE_BAUD 9600U
#endif

/*
 * Linked at RCPU_BOOT_SETTINGS_OFFSET in the image, see linker/hilt.ld.in.
 * hilt should read this table to determine settings to use and not depend on
 * compile time options. This table may be changed after building.
 */
static const volatile struct settings_table_init boot_settings
    __attribute__((section(".boot_settings"), used)) = {
    .platform  = PLATFORM_TYPE,
    .baud      = CONSOLE_BAUD,
    .frequency = OSC_CLOCK_FREQUENCY,
};

uintptr_t platform_atlantis_boot_settings(void)
{
    /* Only the boot stage passes settings on, to the runtime stage */
    if (platform_atlantis_is_bootrom()) {
        return 0;
    }
    return (uintptr_t)&boot_settings;
}

const char *platform_atlantis_platform_name(void)
{
    switch (boot_settings.platform) {
    case PLATFORM_HAPS_BOOT:
        return "HAPS boot";
    case PLATFORM_HAPS_PCIE:
        return "HAPS PCIe";
    case PLATFORM_SILICON:
        return "silicon";
    default:
        return "unknown";
    }
}

#define UART0_BASE 0x48100000U
#define QSPI0_BASE 0x4A000000U

/*
 * clock ceiling for the NOR flash. The bootrom uses 25MHz which is reduced to
 * 12MHz when in 24MHz NOCC mode. The FSBL can probably use 33MHz safely. Some
 * parts do not support 50MHz.
 */
#define QSPI_MAX_SCLK_HZ 25000000U
#define QSPI_FIFO_DEPTH  256

static bool is_haps(void)
{
    return boot_settings.platform == PLATFORM_HAPS_BOOT ||
           boot_settings.platform == PLATFORM_HAPS_PCIE;
}

static uint32_t get_boot_mode(void)
{
    uint32_t reg = soc_mmr_read(MMR_SOC_RCPU_BOOT_MODE);
    return (reg & SOC_RCPU_BOOT_MODE_BOOT_SEL_MASK) >>
           SOC_RCPU_BOOT_MODE_BOOT_SEL_SHIFT;
}

static uint32_t get_boot_cause(void)
{
    uint32_t reg = soc_mmr_read(MMR_SOC_RCPU_BOOT_MODE);
    return (reg & SOC_RCPU_BOOT_MODE_RESET_STATUS_MASK) >>
           SOC_RCPU_BOOT_MODE_RESET_STATUS_SHIFT;
}

const char *platform_atlantis_boot_mode_name(void)
{
    switch (get_boot_mode()) {
    case SOC_RCPU_BOOT_MODE_SEL_UART:
        return "UART";
    case SOC_RCPU_BOOT_MODE_SEL_NOR:
        return "NOR";
    case SOC_RCPU_BOOT_MODE_SEL_SD:
        return "SD";
    default:
        return "invalid";
    }
}

const char *platform_atlantis_boot_cause_name(void)
{
    switch (get_boot_cause()) {
    case SOC_RCPU_BOOT_MODE_STATUS_POR:
        return "power on";
    case SOC_RCPU_BOOT_MODE_STATUS_RESETN:
        return "external reset";
    case SOC_RCPU_BOOT_MODE_STATUS_WDT0:
        return "watchdog 0";
    case SOC_RCPU_BOOT_MODE_STATUS_WDT1:
        return "watchdog 1";
    case SOC_RCPU_BOOT_MODE_STATUS_GLOBAL_SOFT_RESET:
        return "global soft reset";
    case SOC_RCPU_BOOT_MODE_STATUS_RCPU_SOFT_RESET:
        return "RCPU soft reset";
    case SOC_RCPU_BOOT_MODE_STATUS_NDM_RESET:
        return "RCPU debug reset";
    default:
        return "unknown";
    }
}

void clock_init(void)
{
    if (platform_atlantis_is_bootrom()) {
        /* bootrom only enables the bareminimum */
        uint8_t mode = get_boot_mode();
        if (mode == SOC_RCPU_BOOT_MODE_SEL_NOR) {
            clock_lsio_set_cg_enable(SOC_LSIO_BLK_CG_QSPI, true);
            reset_lsio_set(SOC_LSIO_BLK_RST_QSPI, false);
        }
        return;
    }
}

int platform_atlantis_init(void)
{
    int rcpu_rc;
    time_init(rcpu_clock_frequency(boot_settings.frequency, is_haps()));

    reset_bus_set(SOC_BUS_RSTN_NOCC_CLK, false);

    clock_lsio_set_cg_enable(SOC_LSIO_BLK_CG_UART0, true);
    reset_lsio_set(SOC_LSIO_BLK_RST_UART0, false);

    rcpu_rc = clock_rcpu_init(boot_settings.frequency, is_haps());

    /* Ignore failing uart_init for now until postcodes are added */
    uart_dw_16550_init(UART0_BASE, uart_clock_frequency(boot_settings.frequency),
                       boot_settings.baud);

    /*
     * Initialize the console even if uart_init failed to prevent printf()
     * from writing to address 0x0
     */
    console_init(UART0_BASE);

    /* Print the rcpu error now that uart is up */
    if (rcpu_rc) {
        printf("Failure initializing the RCPU clocks rc=%d\n", rcpu_rc);
    }

    clock_init();

    return 0;
}

static int nor_init(struct dwc_ssi *ssi, struct spi_nor *nor)
{
    int rc;

    rc = dwc_ssi_init(ssi, QSPI0_BASE, qspi_clock_frequency(boot_settings.frequency),
                          QSPI_MAX_SCLK_HZ, QSPI_FIFO_DEPTH);
    if (rc) {
        return rc;
    }

    rc = spi_nor_init(nor, ssi);
    if (rc) {
        /* The controller is up even though no flash answered. */
        dwc_ssi_shutdown(ssi);
        return rc;
    }
    printf("Initialized %u MB NOR flash, JEDEC ID 0x%06x\n",
           (unsigned int)(nor->size >> 20), (unsigned int)nor->jedec_id);
    return 0;
}

static struct dwc_ssi ssi;
static struct spi_nor nor;

static int load_nor_image(void *ctx, uint32_t off, void *buf, size_t len)
{
    return spi_nor_read((const struct spi_nor *)ctx, off, buf, len);
}

static int load_image_nor(const struct boot_image *image, uint32_t *entry)
{
    int rc, shutdown_rc;
    static bool nor_initialized = false;

    if (!nor_initialized) {
        rc = nor_init(&ssi, &nor);
        if (rc) {
            printf("NOR init failed rc=%d\n", rc);
            return rc;
        }
        nor_initialized = true;
    }

    rc = boot_image_load_image(image, &load_nor_image, &nor, entry);
    if (rc) {
        printf("failed to load boot_images from nor rc=%d\n", rc);
    }

    /* shutdown the QSPI controller even if the load failed */
    shutdown_rc = dwc_ssi_shutdown(&ssi);
    return rc ? rc : shutdown_rc;
}

int platform_atlantis_load_boot_image(const struct boot_image *image,
                                      uint32_t *entry)
{
    int rc;
    if (!image || !entry) {
        return -EINVAL;
    }

    if (get_boot_mode() == SOC_RCPU_BOOT_MODE_SEL_NOR) {
        rc = load_image_nor(image, entry);
    } else {
        /* Only NOR boot is currently supported in HILT */
        rc = -EINVAL;
    }

    return rc;
}
