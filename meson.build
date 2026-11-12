# SPDX-License-Identifier: Apache-2.0
# SPDX-FileCopyrightText: 2025-2026 Tenstorrent USA, Inc.

project(
  'hilt',
  'c',
  version: '0.1',
  license: 'Apache-2.0',
  meson_version: '>=1.4.0',
  default_options: [
    'c_std=gnu23,gnu2x',
    'warning_level=2',
    'werror=true',
    'buildtype=minsize',
  ],
)

# src/arch/riscv/include/csr.h does not support -O0, plain is -O0.
if get_option('optimization') in ['0', 'plain']
  error('-O0 is not supported, minimum of -O1 is required.')
endif

hilt_inc = include_directories(
    'src',
    'src/include',
    'src/arch/riscv/include',
)

# simg parsing source code. Must be able to be compiled on the host for testing
image_sources = files('src/crc32.c', 'src/image/simg.c', 'src/image/boot_image.c')

python = find_program('python3')
bin2hex = files('tools/bin2hex.py')
mksimg= files('tools/mksimg.py')
mknor = files('tools/mknor.py')

if meson.is_cross_build()
  # ---- Firmware ----
  cc = meson.get_compiler('c')

  # Header hygiene: the distro toolchain puts newlib headers on the default
  # include path; use -nostdinc and re-add only GCC's internal headers.
  # Everything else comes from the picolibc subproject.
  gcc_include_dir = run_command(
    cc.cmd_array() + ['-print-file-name=include'],
    check: true,
  ).stdout().strip()
  add_project_arguments('-nostdinc', '-idirafter', gcc_include_dir, language: 'c')

  picolibc = subproject(
    'picolibc',
    default_options: [
      'multilib=false',        # required for picolibc_dep to exist
      'picocrt=false',         # own _start in src/arch/riscv/start.S
      'picocrt-lib=false',
      'semihost=false',
      'tests=false',
      'single-thread=true',    # no threads; drops stdio locking
      'thread-local-storage=false',
      'format-default=double', # FP printf (upstream default; explicit)
      'werror=false',          # HILT's werror=true would inherit otherwise
    ],
  )
  # Upstream exports 'picolibc_dep' (its own comment saying 'libc_dep' is stale).
  libc_dep = picolibc.get_variable('picolibc_dep')

  # libgcc must follow libc.a; gcc's driver picks the rv32imafdc/ilp32d multilib.
  libgcc_dep = declare_dependency(link_args: ['-lgcc'])

  # Link to maskrom for bootrom and sram for first stage bootloader
  linker_variants = {
    'sram': {'TEXT_REGION': 'sram', 'DATA_REGION': 'sram',  'DATA_LMA_REGION': 'sram'},
    'rom': {'TEXT_REGION': 'rom', 'DATA_REGION': 'bootrom_sram', 'DATA_LMA_REGION': 'rom'},
  }

  build_info_c = vcs_tag(
    input: 'src/build_info.c.in',
    output: 'build_info.c',
    command: ['git', 'describe', '--always', '--dirty'],
  )

  sources = [
    'src/arch/riscv/header.S',
    'src/arch/riscv/start.S',
    'src/console.c',
    'src/drivers/serial/uart_8250.c',
    'src/drivers/serial/uart_dw_16550.c',
    'src/drivers/spi/dwc_ssi.c',
    'src/drivers/flash/spi_nor.c',
    'src/platform/atlantis.c',
    'src/main.c',
    'src/timer.c',
    image_sources,
    build_info_c,
  ]

  objcopy = find_program('objcopy')

  # The first stage bootloader image in sram image keeps the plain 'hilt' name.
  # The bootrom image is suffixed with _rom
  hilt_elf = {}
  hilt_bin = {}
  foreach variant, regions : linker_variants
    name = variant == 'sram' ? 'hilt' : 'hilt-' + variant

    linker_script = configure_file(
      input: 'linker/hilt.ld.in',
      output: 'hilt-' + variant + '.ld',
      configuration: regions,
    )

    hilt_elf += {variant: executable(
      name + '.elf',
      sources,
      include_directories: hilt_inc,
      dependencies: [libc_dep, libgcc_dep],
      link_args: [
        '-T' + linker_script.full_path(),
        '-Wl,-Map=' + name + '.map',
      ],
      link_depends: linker_script,
    )}

    # Raw binary: first 512 bytes are the .binary_header placeholder for simg
    hilt_bin += {variant: custom_target(
      name + '.bin',
      input: hilt_elf[variant],
      output: name + '.bin',
      command: [objcopy, '-O', 'binary', '@INPUT@', '@OUTPUT@'],
      build_by_default: true,
    )}
  endforeach

  custom_target(
    'hilt-rom.mem',
    input: [bin2hex, hilt_bin['rom']],
    output: ['hilt-rom_h.hex', 'hilt-rom_l.hex'],
    command: [python, '@INPUT0@', '@INPUT1@', '@OUTPUT0@', '@OUTPUT1@'],
    build_by_default: true,
  )

  # Placeholder for the runtime stage: loops on a wfi
  rcpu_runtime_elf = executable(
    'rcpu_runtime-placeholder.elf',
    'src/arch/riscv/runtime_placeholder.S',
  )

  rcpu_runtime = custom_target(
    'rcpu_runtime-placeholder.bin',
    input: rcpu_runtime_elf,
    output: 'rcpu_runtime-placeholder.bin',
    command: [objcopy, '-O', 'binary', '@INPUT@', '@OUTPUT@'],
  )

  # simg wrapped binary
  hilt_simg = custom_target(
    'hilt.simg',
    input: [mksimg, hilt_bin['sram']],
    output: 'hilt.simg',
    command: [python, '@INPUT0@', '@INPUT1@', '--patch-header', '-o', '@OUTPUT@'],
    build_by_default: true,
  )


  # SIMGs placed in the NOR image
  nor_simgs = [hilt_simg]

  # Use the meson option for platform fw if it is provided. Otherwise use the placeholder.
  platform_fw_option = get_option('platform-fw')
  if platform_fw_option != ''
    platform_fw = files(platform_fw_option)
  else
    platform_fw = rcpu_runtime
  endif

  platform_fw_simg = custom_target(
    'platform-fw.simg',
    input: [mksimg, platform_fw],
    output: 'platform-fw.simg',
    command: [python, '@INPUT0@', '@INPUT1@', '--payload-type', 'platform-fw',
              '--app-name', 'platform-fw', '-o', '@OUTPUT@'],
    build_by_default: true,
  )
  nor_simgs += platform_fw_simg

  # Generate the entire NOR image. Use the layout information defined in
  # src/include/platform/layout.h, the same definitions used by the target
  hilt_nor = custom_target(
    'nor.img',
    input: [mknor, nor_simgs],
    output: 'nor.img',
    command: [python, '@INPUT0@', '-o', '@OUTPUT@', nor_simgs],
    depend_files: files('src/include/platform/layout.h'),
    build_by_default: true,
  )
else
  # ---- host unit tests ----
  simg_lib = static_library(
    'simg',
    image_sources,
    include_directories: hilt_inc,
  )

  test_simg = executable(
    'test_simg',
    'tests/host/test_simg.c',
    include_directories: hilt_inc,
    link_with: simg_lib,
  )

  golden_payload = custom_target(
    'golden-payload.bin',
    output: 'golden-payload.bin',
    command: [
      python, '-c',
      'import sys; open(sys.argv[1], "wb").write(bytes(range(256)) * 37)',
      '@OUTPUT@',
    ],
  )

  # Generate a simg from the packing tool and test against parsing library
  golden_simg = custom_target(
    'golden.simg',
    input: [mksimg, golden_payload],
    output: 'golden.simg',
    command: [python, '@INPUT0@', '@INPUT1@', '--patch-header', '-o', '@OUTPUT@'],
  )

  test('simg', test_simg, args: [golden_simg])
endif
