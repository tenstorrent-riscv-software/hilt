# HILT

Firmware for the Tenstorrent Atlantis SoC System Management Controller
(SMC). Runs on the Rondel based RCPU, the Atlantis SMC.

## Building

Requires `riscv64-unknown-elf-gcc`, meson, and ninja.

```sh
meson setup build --cross-file cross/riscv32-rondel.ini
ninja -C build
```

Build outputs are produced in the build/ directory:

- `hilt.bin` / `hilt.elf`- the firmware targeting RCPU SRAM.
- `hilt.simg` - a simg of hilt.bin
- `hilt-nor.img` - 32 MiB NOR flash image
- `hilt-rom.bin` / `hilt-rom.elf` - bootrom image
