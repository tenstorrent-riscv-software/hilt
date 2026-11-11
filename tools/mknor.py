#!/usr/bin/env python3
# SPDX-License-Identifier: Apache-2.0
# SPDX-FileCopyrightText: 2025-2026 Tenstorrent USA, Inc.
#
# Build a raw NOR flash image from SIMGs. Each SIMG is placed at the offset
# NOR_LAYOUT gives for the payload type in its header.
#
#     tools/mknor.py -o build/hilt-nor.img build/hilt.simg build/platform-fw.simg
#     tools/mknor.py -s 8M -o nor.img build/hilt.simg
#

import argparse
import os
import sys

from mksimg import (
    SIMG_MAGIC,
    SIMG_PAYLOAD_HILT_BOOT,
    SIMG_PAYLOAD_PLATFORM_FW,
    parse_header,
)

NOR_LAYOUT = {
    0x10000: SIMG_PAYLOAD_HILT_BOOT,
    0x50000: SIMG_PAYLOAD_PLATFORM_FW,
}

PAYLOAD_NAMES = {
    SIMG_PAYLOAD_HILT_BOOT: "hilt-boot",
    SIMG_PAYLOAD_PLATFORM_FW: "platform-fw",
}

DEFAULT_OUTPUT = "nor.img"
DEFAULT_SIZE = "32M"

_SUFFIXES = {"K": 1 << 10, "M": 1 << 20, "G": 1 << 30}


def parse_size(text: str) -> int:
    """Flash size as bytes, 0x-prefixed hex, or with a K/M/G suffix."""
    s = text.strip().upper()
    mult = _SUFFIXES.get(s[-1:], 1)
    if mult != 1:
        s = s[:-1]
    try:
        value = int(s, 0) * mult
    except ValueError:
        raise argparse.ArgumentTypeError(
            f"{text!r}: expected bytes, 0x..., or a K/M/G suffix")
    if value <= 0:
        raise argparse.ArgumentTypeError(f"{text!r}: size must be positive")
    return value


def slot_sizes(flash_size: int) -> dict:
    """offset -> slot size, each slot ending where the next begins."""
    offsets = sorted(NOR_LAYOUT)
    ends = offsets[1:] + [flash_size]
    return {off: end - off for off, end in zip(offsets, ends)}


def payload_name(ptype: int) -> str:
    return PAYLOAD_NAMES.get(ptype, str(ptype))


def main(argv=None) -> int:
    parser = argparse.ArgumentParser(
        prog="mknor.py",
        description="Build a raw NOR flash image from SIMGs, each at the offset "
                    "NOR_LAYOUT assigns to its payload type.")
    parser.add_argument("simgs", nargs="+", metavar="SIMG", help="SIMG files to place")
    parser.add_argument("-s", "--size", type=parse_size, default=DEFAULT_SIZE,
                        metavar="SIZE",
                        help=f"flash size: bytes, 0x..., or K/M/G suffix "
                             f"(default: {DEFAULT_SIZE})")
    parser.add_argument("-o", "--output", default=DEFAULT_OUTPUT, metavar="FILE",
                        help="output image (default: %(default)s)")
    args = parser.parse_args(argv)

    slots = slot_sizes(args.size)
    offset_of = {ptype: off for off, ptype in NOR_LAYOUT.items()}
    for off in NOR_LAYOUT:
        if off >= args.size:
            sys.exit(f"mknor: slot at {off:#x} is past the end of the "
                     f"{args.size:#x}-byte flash")

    placements = []
    seen = {}
    for path in args.simgs:
        try:
            with open(path, "rb") as f:
                blob = f.read()
        except OSError as e:
            sys.exit(f"mknor: {e}")
        try:
            header = parse_header(blob)
        except ValueError as e:
            sys.exit(f"mknor: {path}: {e}")
        if header["magic"] != SIMG_MAGIC:
            sys.exit(f"mknor: {path}: not a SIMG (bad magic)")
        if not header["crc_valid"]:
            sys.exit(f"mknor: {path}: SIMG header CRC mismatch")

        ptype = header["payload_type"]
        name = payload_name(ptype)
        if ptype not in offset_of:
            sys.exit(f"mknor: {path}: no NOR slot for payload type {name}; "
                     f"known: {', '.join(payload_name(t) for t in offset_of)}")
        if ptype in seen:
            sys.exit(f"mknor: {path} and {seen[ptype]} are both {name}")
        seen[ptype] = path

        offset = offset_of[ptype]
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
