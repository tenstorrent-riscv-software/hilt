#!/usr/bin/env python3
# SPDX-License-Identifier: Apache-2.0
# SPDX-FileCopyrightText: 2025-2026 Tenstorrent USA, Inc.
#
# Build a raw NOR flash image. The first argument is a SIMG, placed at the
# offset NOR_LAYOUT gives for the payload type in its header. The optional
# second and third arguments are the platform-fw and ascalon-fw FITs, each
# placed in its slot as it is. The offsets are read from
# src/include/platform/layout.h, which the firmware is built from.
#
#     tools/mknor.py -o build/hilt-nor.img build/hilt.simg build/platform-fw.fit
#     tools/mknor.py -o nor.img hilt.simg platform-fw.fit ascalon-fw.fit
#     tools/mknor.py -s 8M -o nor.img build/hilt.simg
#

import argparse
import os
import re
import sys

from mksimg import (
    SIMG_MAGIC,
    SIMG_PAYLOAD_HILT_BOOT,
    SIMG_PAYLOAD_PLATFORM_FW,
    parse_simg,
)

LAYOUT_HEADER = os.path.join(os.path.dirname(os.path.abspath(__file__)),
                             "..", "src", "include", "platform", "layout.h")

_DEFINE_RE = re.compile(
    r"^\s*#\s*define\s+(\w+)\s+(0[xX][0-9a-fA-F]+|\d+)[uUlL]*\s*(?:/\*.*|//.*)?$")


def read_defines(path: str) -> dict:
    """name -> value for every '#define NAME <integer literal>' in a C header."""
    try:
        with open(path) as f:
            return {m[1]: int(m[2], 0)
                    for m in map(_DEFINE_RE.match, f) if m}
    except OSError as e:
        sys.exit(f"mknor: {e}")


# Slot name -> prefix of its <IMAGE>_NOR_OFFSET and <IMAGE>_MAX_SIZE defines.
SLOT_DEFINES = {
    "hilt-boot": "HILT_BOOT",
    "platform-fw": "PLATFORM_FW",
    "ascalon-fw": "ASCALON_FW",
}


def read_nor_layout(path: str) -> tuple:
    """(NOR offset -> slot name, slot name -> max image size), from the
    <IMAGE>_NOR_OFFSET and <IMAGE>_MAX_SIZE defines."""
    defines = read_defines(path)
    layout = {}
    max_sizes = {}
    for slot, image in SLOT_DEFINES.items():
        for name in (f"{image}_NOR_OFFSET", f"{image}_MAX_SIZE"):
            if name not in defines:
                sys.exit(f"mknor: {path}: no integer #define for {name}")
        offset = defines[f"{image}_NOR_OFFSET"]
        if offset in layout:
            sys.exit(f"mknor: {path}: {slot} and {layout[offset]} are both "
                     f"at {offset:#x}")
        layout[offset] = slot
        max_sizes[slot] = defines[f"{image}_MAX_SIZE"]

    # A slot's size limit must not reach into the slot after it.
    offsets = sorted(layout)
    for off, nxt in zip(offsets, offsets[1:]):
        slot = layout[off]
        if off + max_sizes[slot] > nxt:
            sys.exit(f"mknor: {path}: {slot} at {off:#x} with max size "
                     f"{max_sizes[slot]:#x} overlaps {layout[nxt]} at {nxt:#x}")
    return layout, max_sizes


# MAX_SIZES is the limit the loader puts on a whole image (for a SIMG: header +
# payload + signature); a larger image fits in flash but would be rejected at
# boot.
NOR_LAYOUT, MAX_SIZES = read_nor_layout(LAYOUT_HEADER)

# SIMG payload type -> the slot it goes in.
PAYLOAD_SLOTS = {
    SIMG_PAYLOAD_HILT_BOOT: "hilt-boot",
}

FDT_MAGIC = b"\xd0\x0d\xfe\xed"

DEFAULT_OUTPUT = "nor.img"
DEFAULT_SIZE = "32M"

_SUFFIXES = {"K": 1 << 10, "M": 1 << 20, "G": 1 << 30}


def parse_size(text: str) -> int:
    """Flash size in bytes: decimal or 0x/0o/0b prefixed, with an optional
    K/M/G suffix (8M, 0x10M)."""
    s = text.strip().upper()
    mult = _SUFFIXES.get(s[-1:], 1)
    if mult != 1:
        s = s[:-1]
    try:
        value = int(s, 0) * mult
    except ValueError:
        raise argparse.ArgumentTypeError(
            f"{text!r}: expected a decimal or 0x/0o/0b number with an "
            f"optional K/M/G suffix")
    if value <= 0:
        raise argparse.ArgumentTypeError(f"{text!r}: size must be positive")
    return value


def slot_sizes(flash_size: int) -> dict:
    """offset -> slot size: up to where the next slot begins (or the flash
    ends), capped at the loader's size limit for the image."""
    offsets = sorted(NOR_LAYOUT)
    ends = offsets[1:] + [flash_size]
    return {off: max(0, min(min(end, flash_size) - off, MAX_SIZES[NOR_LAYOUT[off]]))
            for off, end in zip(offsets, ends)}


def read_image(path: str) -> bytes:
    try:
        with open(path, "rb") as f:
            return f.read()
    except OSError as e:
        sys.exit(f"mknor: {e}")


def simg_payload_type(path: str, blob: bytes) -> int:
    """Payload type of a SIMG, after checking it is a well-formed one."""
    try:
        header = parse_simg(blob)
    except ValueError as e:
        sys.exit(f"mknor: {path}: {e}")
    if header["magic"] != SIMG_MAGIC:
        sys.exit(f"mknor: {path}: not a SIMG (bad magic)")
    if not header["crc_valid"]:
        sys.exit(f"mknor: {path}: SIMG header CRC mismatch")
    if not header["length_valid"]:
        sys.exit(f"mknor: {path}: is {header['image_length']} bytes, but its "
                 f"header describes a {header['total_size']}-byte SIMG")
    return header["payload_type"]


def main(argv=None) -> int:
    parser = argparse.ArgumentParser(
        prog="mknor.py",
        description="Build a raw NOR flash image from a SIMG, placed at the "
                    "offset NOR_LAYOUT assigns to its payload type, and "
                    "optionally the platform-fw and ascalon-fw FITs.")
    parser.add_argument("simg", metavar="SIMG", help="SIMG file to place")
    parser.add_argument("platform_fw", nargs="?", metavar="PLATFORM_FW_FIT",
                        help="platform-fw FIT to place in the platform-fw slot")
    parser.add_argument("ascalon_fw", nargs="?", metavar="ASCALON_FW_FIT",
                        help="ascalon-fw FIT to place in the ascalon-fw slot")
    parser.add_argument("-s", "--size", type=parse_size, default=DEFAULT_SIZE,
                        metavar="SIZE",
                        help="flash size in bytes, decimal or 0x/0o/0b prefixed, "
                             f"optional K/M/G suffix (default: {DEFAULT_SIZE})")
    parser.add_argument("-o", "--output", default=DEFAULT_OUTPUT, metavar="FILE",
                        help="output image (default: %(default)s)")
    args = parser.parse_args(argv)

    slots = slot_sizes(args.size)
    offset_of = {slot: off for off, slot in NOR_LAYOUT.items()}

    # (path, contents, name of the slot it goes in)
    images = []

    blob = read_image(args.simg)
    ptype = simg_payload_type(args.simg, blob)
    if ptype not in PAYLOAD_SLOTS:
        sys.exit(f"mknor: {args.simg}: no NOR slot for payload type {ptype}; "
                 f"known: {', '.join(PAYLOAD_SLOTS.values())}")
    simg_slot = PAYLOAD_SLOTS[ptype]
    images.append((args.simg, blob, simg_slot))

    for path, slot in ((args.platform_fw, "platform-fw"),
                       (args.ascalon_fw, "ascalon-fw")):
        if path is None:
            continue
        if slot == simg_slot:
            sys.exit(f"mknor: {args.simg} and {path} are both {slot}")
        blob = read_image(path)
        if not blob.startswith(FDT_MAGIC):
            sys.exit(f"mknor: {path}: not a FIT (bad magic)")
        images.append((path, blob, slot))

    placements = []
    for path, blob, name in images:
        offset = offset_of[name]
        if offset >= args.size:
            sys.exit(f"mknor: {name} slot at {offset:#x} is past the end of "
                     f"the {args.size:#x}-byte flash")
        if len(blob) > slots[offset]:
            sys.exit(f"mknor: {path} is {len(blob)} bytes, larger than the "
                     f"{slots[offset]:#x}-byte {name} slot at {offset:#x}")
        placements.append((name, offset, slots[offset], blob, os.path.basename(path)))
    placements.sort(key=lambda p: p[1])

    image = bytearray(b"\xff" * args.size)
    for name, offset, slot, blob, base in placements:
        image[offset:offset + len(blob)] = blob
        print(f"  {name} @ {offset:#x}: {base}, {len(blob)} bytes of {slot:#x}")

    try:
        with open(args.output, "wb") as f:
            f.write(image)
    except OSError as e:
        sys.exit(f"mknor: {e}")
    print(f"{args.output}: {args.size:#x}-byte NOR image, {len(placements)} image(s)")
    return 0


if __name__ == "__main__":
    sys.exit(main())
