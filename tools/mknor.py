#!/usr/bin/env python3
#
# Build a raw NOR flash image from SIMGs placed at fixed offsets.
#
#     tools/mknor.py                      # build/hilt-nor.img from build/*.simg
#     tools/mknor.py -s 8M -o nor.img     # 8 MiB image, custom output
#
# The layout comes from tools/nor.toml

import argparse
import os
import sys
import tomllib

SCRIPT_DIR = os.path.dirname(os.path.abspath(__file__))
DEFAULT_CONFIG = os.path.join(SCRIPT_DIR, "nor.toml")
DEFAULT_DIR = os.path.normpath(os.path.join(SCRIPT_DIR, os.pardir, "build"))
DEFAULT_OUTPUT_NAME = "hilt-nor.img"
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


def main(argv=None) -> int:
    parser = argparse.ArgumentParser(
        prog="mknor.py",
        description="Build a raw NOR flash image from SIMGs at fixed offsets.")
    parser.add_argument("-c", "--config", default=DEFAULT_CONFIG,
                        metavar="TOML", help="layout file (default: %(default)s)")
    parser.add_argument("-d", "--dir", default=DEFAULT_DIR, metavar="DIR",
                        help="directory holding <name>.simg (default: %(default)s)")
    parser.add_argument("-s", "--size", type=parse_size, default=DEFAULT_SIZE,
                        metavar="SIZE",
                        help=f"flash size: bytes, 0x..., or K/M/G suffix "
                             f"(default: {DEFAULT_SIZE})")
    parser.add_argument("-o", "--output", metavar="FILE",
                        help=f"output image (default: {DEFAULT_OUTPUT_NAME} in --dir)")
    args = parser.parse_args(argv)
    output = args.output or os.path.join(args.dir, DEFAULT_OUTPUT_NAME)

    try:
        with open(args.config, "rb") as f:
            images = tomllib.load(f).get("image", [])
    except (OSError, tomllib.TOMLDecodeError) as e:
        sys.exit(f"mknor: {args.config}: {e}")
    if not images:
        sys.exit(f"mknor: {args.config}: no [[image]] entries")

    placements = []
    for entry in images:
        name, offset = entry.get("name"), entry.get("offset")
        if not isinstance(name, str) or not isinstance(offset, int) or offset < 0:
            sys.exit(f"mknor: {args.config}: each [[image]] needs a string "
                     f"'name' and a non-negative integer 'offset': {entry}")
        path = os.path.join(args.dir, name + ".simg")
        try:
            with open(path, "rb") as f:
                placements.append((name, offset, f.read()))
        except OSError as e:
            sys.exit(f"mknor: {e}")
    placements.sort(key=lambda p: p[1])

    image = bytearray(b"\xff" * args.size)
    for i, (name, offset, blob) in enumerate(placements):
        end = offset + len(blob)
        if end > args.size:
            sys.exit(f"mknor: {name} ({len(blob)} bytes at {offset:#x}) runs to "
                     f"{end:#x}, past the end of the {args.size:#x}-byte flash")
        if i + 1 < len(placements) and end > placements[i + 1][1]:
            sys.exit(f"mknor: {name} ({len(blob)} bytes at {offset:#x}) runs to "
                     f"{end:#x}, overlapping {placements[i + 1][0]} at "
                     f"{placements[i + 1][1]:#x}")
        image[offset:end] = blob
        print(f"  {name} @ {offset:#x}: {len(blob)} bytes")

    try:
        with open(output, "wb") as f:
            f.write(image)
    except OSError as e:
        sys.exit(f"mknor: {e}")
    print(f"{output}: {args.size >> 20} MiB NOR image, {len(placements)} image(s)")
    return 0


if __name__ == "__main__":
    sys.exit(main())
