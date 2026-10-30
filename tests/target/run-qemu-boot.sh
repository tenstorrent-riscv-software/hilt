#!/usr/bin/env bash
# Boot the built NOR flash image under the ZSBL on the tt-atlantis QEMU machine
# and check that HILT reaches hello-world on the RCPU console.
#
# Environment:
#   BUILD       meson build dir with hilt-nor.img      (default: build)
#   QEMU_STAGE  dir with qemu-system-riscv64, otp/ and zsbl-silicon.bin
#               (default: ../out/qemu relative to the repo, the cihub layout)
#   BOOTROM     path to a binary to boot instead of the staged zsbl
#   OTP         staged otp-gen config name              (default: unprogrammed)
#   TIMEOUT     seconds to wait for the banner          (default: 60)

set -euo pipefail

# Resolve a user-supplied BOOTROM path before changing directory.
if [ -n "${BOOTROM:-}" ]; then
    [ -e "$BOOTROM" ] || { echo "missing: $BOOTROM" >&2; exit 1; }
    BOOTROM=$(readlink -f "$BOOTROM")
fi

cd "$(dirname "$0")/../.."

BUILD=${BUILD:-build}
QEMU_STAGE=${QEMU_STAGE:-$PWD/../out/qemu}
OTP=${OTP:-unprogrammed}
TIMEOUT=${TIMEOUT:-60}

NOR=$BUILD/hilt-nor.img
CONSOLE=$BUILD/rcpu-console.log
ASC_LOG=$BUILD/asc-console.log

for f in "$NOR" "$QEMU_STAGE/qemu-system-riscv64" "$QEMU_STAGE/otp/$OTP"; do
    [ -e "$f" ] || { echo "missing: $f" >&2; exit 1; }
done

# The machine's zsbl= property names the ROM image; without it QEMU looks up
# tt-zsbl.bin on the -L search path, so make sure one is there to find.
MACHINE=tt-atlantis,rcpu=true,boot-mode=nor
if [ -n "${BOOTROM:-}" ]; then
    MACHINE=$MACHINE,zsbl=$BOOTROM
    echo "using BOOTROM $BOOTROM"
elif [ ! -e "$QEMU_STAGE/tt-zsbl.bin" ] && [ ! -e "$QEMU_STAGE/share/qemu/tt-zsbl.bin" ]; then
    echo "missing: $QEMU_STAGE/tt-zsbl.bin (or share/qemu/tt-zsbl.bin); set BOOTROM=" >&2
    exit 1
fi

rm -f "$CONSOLE"
"$QEMU_STAGE/qemu-system-riscv64" \
    -M "$MACHINE" \
    -display none \
    -serial "file:$ASC_LOG" \
    -serial "file:$CONSOLE" \
    -L "$QEMU_STAGE" -L "$QEMU_STAGE/share/qemu" -L "$QEMU_STAGE/otp/$OTP" \
    -drive "if=mtd,format=raw,file=$NOR" \
    -monitor none &
QEMU_PID=$!
trap 'kill $QEMU_PID 2>/dev/null || true' EXIT

for _ in $(seq "$TIMEOUT"); do
    if grep -q "HILT: hello, world" "$CONSOLE" 2>/dev/null; then
        echo "--- RCPU console ---"
        cat "$CONSOLE"
        echo "PASS: HILT booted via ZSBL"
        exit 0
    fi
    if ! kill -0 $QEMU_PID 2>/dev/null; then
        echo "FAIL: QEMU exited early" >&2
        break
    fi
    sleep 1
done

echo "--- RCPU console ---" >&2
cat "$CONSOLE" >&2 || true
echo "FAIL: no hello-world within ${TIMEOUT}s" >&2
exit 1
