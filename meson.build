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

hilt_inc = include_directories(
    'src',
    'src/include',
    'src/arch/riscv/include',
)

mksimg = files('tools/mksimg.py')
mknor = files('tools/mknor.py')
nor_layout = files('tools/nor.toml')
python = find_program('python3')

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
    'src/drivers/console/uart16550.c',
    'src/platform/atlantis.c',
    'src/main.c',
    'src/time.c',
    build_info_c,
  ]

  objcopy = find_program('objcopy')

  # The first stage bootloader image in sram image keeps the plain 'hilt' name.
  # The bootrom image is suffixed with _bootrom
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

  # Write NOR image
  hilt_nor = custom_target(
    'hilt-nor.img',
    input: [mknor, hilt_simg],
    output: 'hilt-nor.img',
    command: [python, '@INPUT0@', '-c', nor_layout, '-d', '@OUTDIR@', '-o', '@OUTPUT@'],
    depend_files: nor_layout,
    build_by_default: true,
  )
endif
