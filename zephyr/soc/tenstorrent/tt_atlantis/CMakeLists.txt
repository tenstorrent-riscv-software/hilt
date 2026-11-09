# SPDX-License-Identifier: Apache-2.0

zephyr_include_directories(.)

zephyr_sources_ifdef(CONFIG_SOC_TT_ATLANTIS_RCPU soc.c)

set(SOC_LINKER_SCRIPT ${ZEPHYR_BASE}/include/zephyr/arch/riscv/common/linker.ld CACHE INTERNAL "")

# Everything lives in one RAM region, so a single RWX segment is expected.
zephyr_ld_options(-Wl,--no-warn-rwx-segments)
