![HILT logo](img/logo.png)

# HILT

Firmware for the Tenstorrent Atlantis SoC System Management Controller
(SMC). Runs on the Rondel based RCPU, the Atlantis SMC.

## Building

Requires `riscv64-unknown-elf-gcc`, meson, ninja, and Python 3.

```sh
meson setup build --cross-file cross/riscv32-rondel.ini
ninja -C build
```

Build outputs are produced in the build/ directory:

- `hilt.bin` / `hilt.elf` - the firmware targeting RCPU SRAM.
- `hilt.simg` - a simg of hilt.bin
- `hilt-rom.bin` / `hilt-rom.elf` - bootrom image
- `hilt-rom_h.hex` / `hilt-rom_l.hex` - bootrom memory files for HAPs

## Host unit tests

Unit tests can be run natively on the host running:

```sh
meson setup build-host
meson test -C build-host
```

## Contributing

See [CONTRIBUTING.md](CONTRIBUTING.md) for information on how to contribute to this project.

## License

This project is licensed under the terms described in:

* [LICENSE](LICENSE) – code license
* [LICENSE_understanding.txt](LICENSE_understanding.txt) – license summary and clarification
* [LICENSE-DOCS](LICENSE-DOCS) – Creative Commons license for all documentation and logos
