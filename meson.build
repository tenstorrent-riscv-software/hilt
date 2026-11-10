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
    'src/arch/riscv/handoff.S',
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
  sh = find_program('sh')

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

  # simg wrapped binary
  hilt_simg = custom_target(
    'hilt.simg',
    input: [mksimg, hilt_bin['sram']],
    output: 'hilt.simg',
    command: [python, '@INPUT0@', '@INPUT1@', '--patch-header', '-o', '@OUTPUT@'],
    build_by_default: true,
  )

  bin2hex = find_program('scripts/bin2hex')
  rom_load_addr = '0x00040000'
  custom_target(
    'hilt-rom.mem',
    input: hilt_bin['rom'],
    output: ['hilt-rom_h.hex', 'hilt-rom_l.hex'],
    command: [
      sh, '-c',
      '"$0" -s -o "$1" "$2" "$3" >/dev/null && cp "$1/hex-$3_h.hex" "$4" && cp "$1/hex-$3_l.hex" "$5"',
      bin2hex, '@PRIVATE_DIR@', '@INPUT@', rom_load_addr, '@OUTPUT0@', '@OUTPUT1@',
    ],
    depend_files: files('scripts/bin2hex'),
    build_by_default: true,
  )

  # Built hilt runtime, a Zephyr applicaiton built using west. First setup
  # the west workspace, one level up (pwd/..).
  fs = import('fs')

  # First check that west is installed.
  west = find_program('west', required: false)
  if not west.found()
    error('west is required for the Zephyr runtime: pipx install west')
  endif

  # Check if a workspace already exists and confirm it is for this project. Do
  # not overwrite a workspace for a different project, just error out.
  west_base = run_command(west, 'list', '-f', '{abspath}', 'zephyr',
                          check: false, capture: true)
  if west_base.returncode() != 0
    repo_dir = fs.name(meson.current_source_dir())
    if fs.is_dir('../.west')
      error('the west workspace in the parent directory does not use this ' +
            'checkout as its manifest.')
    endif

    # Create a workspace if one doesn't exist
    message('No west workspace found: initialising one')
    west_init = run_command(west, 'init', '-l', '.', check: false,
                            capture: false)
    if west_init.returncode() != 0
      error('west init failed')
    endif
    west_update = run_command(west, 'update', '--narrow', check: false,
                              capture: false)
    if west_update.returncode() != 0
      error('west update failed; fix the cause and rerun meson setup (or ' +
            '"west update" from here) to complete the workspace')
    endif
    west_base = run_command(west, 'list', '-f', '{abspath}', 'zephyr',
                            check: true, capture: true)
  endif
  zephyr_base = west_base.stdout().strip()
  message('Zephyr runtime from ' + zephyr_base)

  # Build the HILT runtime application using the Zephyr SDK and Zephyr's own CMake.
  zephyr_board = 'tt_atlantis_evb/tt_atlantis/rcpu'
  zephyr_app = meson.current_source_dir()
  zephyr_build = meson.current_build_dir() / 'zephyr'
  hilt_runtime = custom_target(
    'hilt_runtime.bin',
    output: 'hilt_runtime.bin',
    command: [
      sh, '-c',
      '"$0" build -b "$1" -d "$2" "$3" -- "$4" >/dev/null && cp "$2/zephyr/zephyr.bin" "$5"',
      west, zephyr_board, zephyr_build, zephyr_app,
      '-DZEPHYR_TOOLCHAIN_VARIANT=zephyr',
      '@OUTPUT@',
    ],
    # west/ninja track the Zephyr sources themselves; always let them run
    build_always_stale: true,
    console: true,
  )

  # For now platform-fw is just hilt runtime
  platform_fw_simg = custom_target(
    'platform-fw.simg',
    input: [mksimg, hilt_runtime],
    output: 'platform-fw.simg',
    command: [python, '@INPUT0@', '@INPUT1@', '--payload-type', 'platform-fw',
              '--app-name', 'platform-fw', '-o', '@OUTPUT@'],
    build_by_default: true,
  )

  # Write NOR image: each SIMG at the offset tools/mknor.py assigns to its
  # payload type
  hilt_nor = custom_target(
    'nor.img',
    input: [mknor, hilt_simg, platform_fw_simg],
    output: 'nor.img',
    command: [python, '@INPUT0@', '-o', '@OUTPUT@', '@INPUT1@', '@INPUT2@'],
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
