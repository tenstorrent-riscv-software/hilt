#!/usr/bin/env python3
# SPDX-License-Identifier: Apache-2.0
# SPDX-FileCopyrightText: 2025-2026 Tenstorrent USA, Inc.
"""Write a binary as high/low $readmemh files for a 2x64-bit memory model.

Each 16-byte group becomes one line in each file: bytes 15..8 in the high
file, bytes 7..0 in the low file, each as a little-endian word written MSB
first. A trailing partial group is zero-padded.
"""
import argparse
import sys


def main():
    p = argparse.ArgumentParser(description=__doc__)
    p.add_argument('input', help='binary image')
    p.add_argument('high', help='output file for the upper 8 bytes of each group')
    p.add_argument('low', help='output file for the lower 8 bytes of each group')
    args = p.parse_args()

    with open(args.input, 'rb') as f:
        data = f.read()
    if not data:
        sys.exit(f'{p.prog}: error: {args.input} is empty')
    data += bytes(-len(data) % 16)

    with open(args.high, 'w') as hi, open(args.low, 'w') as lo:
        for i in range(0, len(data), 16):
            lo.write(data[i:i + 8][::-1].hex().upper() + '\n')
            hi.write(data[i + 8:i + 16][::-1].hex().upper() + '\n')


if __name__ == '__main__':
    main()
