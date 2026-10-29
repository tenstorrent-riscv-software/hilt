#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-2.0-or-later
#
# SIMG ("Secure Image") packaging tool for Atlantis images
#
#     tools/simg.py --payload-type opensbi fw_jump.bin
#     tools/simg.py --patch-header build/hilt.bin -o hilt.simg
#
# By default a new header is prepended to the whole input. --patch-header
# instead replaces the 512-byte placeholder header a binary already carries
# (images linked with a reserved header area, like the CoreLab FSBL).
#

import argparse
import hashlib
import os
import struct
import sys
import zlib

SIMG_MAGIC = 0x2000006F
SIMG_FORMAT_V1 = 0x0001
SIMG_HEADER_V1_SIZE = 512
SIMG_HEADER_CRC_OFFSET = 0x58
# RSA-3072: the public key is the modulus (big-endian) and the signature is a
# RSASSA-PKCS#1 v1.5 block, both 384 bytes.
SIMG_SIGNATURE_SIZE = 384
SIMG_PUBLIC_KEY_LENGTH = 384

# flags bits (SIMG_FLAG_*). Only REVOCATION_PRESENT is a valid flag.
SIMG_FLAG_REVOCATION_PRESENT = 1 << 1
SIMG_FLAGS_VALID_MASK = SIMG_FLAG_REVOCATION_PRESENT

# Per-domain max auth slot / revocation mask (SIMG_*_MAX_SLOT, SIMG_REVOKE_MASK_*).
SIMG_ZSBL_MAX_SLOT = 1
SIMG_FSBL_MAX_SLOT = 1
SIMG_USER_MAX_SLOT = 0
SIMG_REVOKE_MASK_ZSBL = 0x03
SIMG_REVOKE_MASK_FSBL = 0x03
SIMG_REVOKE_MASK_USER = 0x01

# ZSBL domain
SIMG_PAYLOAD_FSBL = 1
# FSBL domain
SIMG_PAYLOAD_ASC_BL0 = 2
SIMG_PAYLOAD_OPENSBI = 3
SIMG_PAYLOAD_UBOOT = 4
# USER domain
SIMG_PAYLOAD_INITRD = 5
SIMG_PAYLOAD_FIT = 6

# Domain strings. The domain names the *verifier* stage: the FSBL image is
# verified by the ZSBL (domain "ZSBL"); ASC_BL0/OpenSBI/U-Boot are verified
# by the FSBL (domain "FSBL").
SIMG_DOMAIN_ZSBL = b"ZSBL"
SIMG_DOMAIN_FSBL = b"FSBL"
SIMG_DOMAIN_USER = b"USER"
SIMG_DOMAINS = (SIMG_DOMAIN_ZSBL, SIMG_DOMAIN_FSBL, SIMG_DOMAIN_USER)
SIMG_DOMAIN_SIZE = 4


SIMG_APP_NAME_SIZE = 16

# Payload type names accepted on the command line.
PAYLOAD_TYPES = {
    "fsbl": SIMG_PAYLOAD_FSBL,
    "asc_bl0": SIMG_PAYLOAD_ASC_BL0,
    "opensbi": SIMG_PAYLOAD_OPENSBI,
    "uboot": SIMG_PAYLOAD_UBOOT,
    "initrd": SIMG_PAYLOAD_INITRD,
    "fit": SIMG_PAYLOAD_FIT,
}

# The stage that verifies each payload type, i.e. the header's default domain.
DEFAULT_DOMAIN_FOR_PAYLOAD = {
    SIMG_PAYLOAD_FSBL: SIMG_DOMAIN_ZSBL,
    SIMG_PAYLOAD_ASC_BL0: SIMG_DOMAIN_FSBL,
    SIMG_PAYLOAD_OPENSBI: SIMG_DOMAIN_FSBL,
    SIMG_PAYLOAD_UBOOT: SIMG_DOMAIN_FSBL,
    SIMG_PAYLOAD_INITRD: SIMG_DOMAIN_USER,
    SIMG_PAYLOAD_FIT: SIMG_DOMAIN_USER,
}

# domain -> (max auth_key_slot, valid revocation_bitmap bits), mirroring
# simg_get_max_slot_by_domain_id() / simg_is_valid_revocation_bitmap_by_domain_id().
DOMAIN_LIMITS = {
    SIMG_DOMAIN_ZSBL: (SIMG_ZSBL_MAX_SLOT, SIMG_REVOKE_MASK_ZSBL),
    SIMG_DOMAIN_FSBL: (SIMG_FSBL_MAX_SLOT, SIMG_REVOKE_MASK_FSBL),
    SIMG_DOMAIN_USER: (SIMG_USER_MAX_SLOT, SIMG_REVOKE_MASK_USER),
}


def encode_domain(text: str) -> bytes:
    """Encode a domain string into the 4-byte header field, NUL-padded. """
    try:
        raw = text.encode("ascii")
    except UnicodeEncodeError:
        raise ValueError(f"{text!r}: domain must be ASCII")
    if not 1 <= len(raw) <= SIMG_DOMAIN_SIZE:
        raise ValueError(f"{text!r}: domain is a {SIMG_DOMAIN_SIZE}-byte field, "
                         f"got {len(raw)} bytes")
    return raw.ljust(SIMG_DOMAIN_SIZE, b"\x00")


def encode_app_name(text: str) -> bytes:
    """Encode an app name into the 16-byte header field. At most 15 bytes so
    the device's char[16] stays NUL-terminated (simg_get_app_name returns it
    as a C string)."""
    try:
        raw = text.encode("ascii")
    except UnicodeEncodeError:
        raise ValueError(f"{text!r}: app_name must be ASCII")
    if len(raw) >= SIMG_APP_NAME_SIZE:
        raise ValueError(f"{text!r}: app_name must be at most "
                         f"{SIMG_APP_NAME_SIZE - 1} bytes, got {len(raw)}")
    return raw.ljust(SIMG_APP_NAME_SIZE, b"\x00")


def validate_fields(domain: bytes, auth_key_slot: int,
                    revocation_bitmap: int) -> None:
    """Apply the per-domain limits the device enforces on auth_key_slot and
    revocation_bitmap; raises ValueError with the offending value."""
    if domain not in DOMAIN_LIMITS:
        raise ValueError(f"{domain.rstrip(b'\\x00').decode(errors='replace')!r}: "
                         "unknown domain, expected one of "
                         + ", ".join(d.decode() for d in SIMG_DOMAINS))
    max_slot, revoke_mask = DOMAIN_LIMITS[domain]
    name = domain.rstrip(b"\x00").decode()
    if not 0 <= auth_key_slot <= max_slot:
        raise ValueError(f"auth_key_slot {auth_key_slot} out of range for "
                         f"domain {name} (max {max_slot})")
    if revocation_bitmap & ~revoke_mask:
        raise ValueError(f"revocation_bitmap {revocation_bitmap:#x} has bits "
                         f"outside the {name} mask {revoke_mask:#x}")

# Header field layout, little-endian, packed (matches simg_header_v1). The
# modulus and signature are big-endian byte strings (standard RSA encoding).
# Single 512-byte sector; payloads are not encrypted (no kdf_input/iv/tag).
_SIMG_FMT = (
    "<"
    "I"    # 0x00 magic
    "4s"   # 0x04 domain
    "H"    # 0x08 format_version
    "H"    # 0x0A header_length
    "I"    # 0x0C flags
    "H"    # 0x10 auth_key_slot
    "H"    # 0x12 public_key_length
    "H"    # 0x14 reserved_a (was tag_length)
    "B"    # 0x16 payload_type
    "9s"   # 0x17 reserved0
    "Q"    # 0x20 revocation_bitmap
    "I"    # 0x28 security_version
    "I"    # 0x2C reserved1
    "Q"    # 0x30 payload_length
    "32s"  # 0x38 payload_hash
    "I"    # 0x58 header_crc (CRC-32 of the header with this field = 0)
    "8s"   # 0x5C config
    "I"    # 0x64 payload_crc (CRC-32 of the payload)
    "8s"   # 0x68 reserved2
    "384s" # 0x70 public_key (RSA-3072 modulus, big-endian)
    "16s"  # 0x1F0 app_name
)

# Field names in _SIMG_FMT order, used to build the parse dict.
_FIELD_NAMES = (
    "magic", "domain", "format_version", "header_length", "flags",
    "auth_key_slot", "public_key_length", "reserved_a", "payload_type",
    "reserved0", "revocation_bitmap", "security_version", "reserved1",
    "payload_length", "payload_hash", "header_crc", "config", "payload_crc",
    "reserved2", "public_key", "app_name",
)

# name -> (byte offset, byte size) within the 512-byte header, derived from
# _SIMG_FMT. Handy for callers that poke a single field (mutate_simg.py).
FIELD_OFFSETS = {}
_off = 0
for _name, _code in zip(
    _FIELD_NAMES,
    ("I", "4s", "H", "H", "I", "H", "H", "H", "B", "9s", "Q", "I", "I", "Q",
     "32s", "I", "8s", "I", "8s", "384s", "16s"),
):
    FIELD_OFFSETS[_name] = (_off, struct.calcsize(_code))
    _off += struct.calcsize(_code)
assert _off == SIMG_HEADER_V1_SIZE, _off
del _off, _name, _code


# --- RSA-3072 signing (pyca cryptography) ---

def load_rsa3072_key(key_path: str):
    """Load an RSA-3072 private key from a PEM file (pyca cryptography)."""
    try:
        from cryptography.hazmat.primitives import serialization
        from cryptography.hazmat.primitives.asymmetric import rsa
        from cryptography.hazmat.backends import default_backend
    except ImportError:
        sys.exit("this requires the 'cryptography' package "
                 "(pip install cryptography)")
    with open(key_path, "rb") as f:
        key = serialization.load_pem_private_key(f.read(), password=None,
                                                 backend=default_backend())
    if not isinstance(key, rsa.RSAPrivateKey) or key.key_size != 3072:
        sys.exit(f"{key_path}: expected a 3072-bit RSA private key")
    return key


def rsa3072_modulus(key) -> bytes:
    """The RSA-3072 modulus as a 384-byte big-endian string."""
    return key.public_key().public_numbers().n.to_bytes(SIMG_PUBLIC_KEY_LENGTH,
                                                         "big")


def rsa3072_sign(key, signed_region: bytes) -> bytes:
    """RSASSA-PKCS#1 v1.5 / SHA-256 signature over signed_region (header ||
    payload). Mirrors the device-side secure_boot_rsa3072_verify()."""
    from cryptography.hazmat.primitives import hashes
    from cryptography.hazmat.primitives.asymmetric import padding
    sig = key.sign(signed_region, padding.PKCS1v15(), hashes.SHA256())
    assert len(sig) == SIMG_SIGNATURE_SIZE
    return sig


def rsa3072_verify(modulus: bytes, signed_region: bytes, signature: bytes) -> bool:
    """True if `signature` is a valid RSASSA-PKCS#1 v1.5 / SHA-256 signature over
    signed_region for the public key given by `modulus` (big-endian, exponent
    65537). Used by the self-test to confirm re-signed images verify."""
    from cryptography.hazmat.primitives import hashes
    from cryptography.hazmat.primitives.asymmetric import padding, rsa
    from cryptography.exceptions import InvalidSignature
    n = int.from_bytes(modulus, "big")
    pub = rsa.RSAPublicNumbers(65537, n).public_key()
    try:
        pub.verify(signature, signed_region, padding.PKCS1v15(), hashes.SHA256())
        return True
    except InvalidSignature:
        return False


# --- CRC ---

def compute_header_crc(header: bytes) -> int:
    """CRC-32 (IEEE/zlib) over the 512-byte header with the header_crc field
    treated as zero; matches simg_compute_header_crc() on the device."""
    if len(header) < SIMG_HEADER_V1_SIZE:
        raise ValueError("header shorter than a SIMG header")
    tmp = bytearray(header[:SIMG_HEADER_V1_SIZE])
    struct.pack_into("<I", tmp, SIMG_HEADER_CRC_OFFSET, 0)
    return zlib.crc32(bytes(tmp)) & 0xFFFFFFFF


def fix_header_crc(header: bytearray) -> None:
    """Recompute the header CRC over `header` and write it into the crc field
    in place. `header` must be a mutable buffer of at least 512 bytes."""
    crc = compute_header_crc(header)
    struct.pack_into("<I", header, SIMG_HEADER_CRC_OFFSET, crc)


# --- Header build ---

def build_header(domain: bytes, payload_type: int, payload: bytes,
                 modulus: bytes, auth_key_slot: int = 0,
                 security_version: int = 0,
                 revocation_bitmap: int = 0,
                 app_name: bytes = bytes(SIMG_APP_NAME_SIZE)) -> bytes:
    """Build a valid 512-byte SIMG header with the CRC filled in."""

    flags = SIMG_FLAG_REVOCATION_PRESENT if revocation_bitmap else 0

    header = bytearray(struct.pack(
        _SIMG_FMT,
        SIMG_MAGIC,
        domain,
        SIMG_FORMAT_V1,
        SIMG_HEADER_V1_SIZE,
        flags,                   # not encrypted; revocation per the bitmap
        auth_key_slot,
        SIMG_PUBLIC_KEY_LENGTH,
        0,                       # reserved_a
        payload_type,
        bytes(9),                # reserved0
        revocation_bitmap,
        security_version,
        0,                       # reserved1
        len(payload),            # payload_length
        hashlib.sha256(payload).digest(),
        0,                       # header_crc (filled in below)
        bytes(8),                # config
        zlib.crc32(payload) & 0xFFFFFFFF,  # payload_crc
        bytes(8),                # reserved2
        modulus,                 # public_key (RSA modulus, or zero)
        app_name,                # app_name (16 bytes, NUL-padded)
    ))
    assert len(header) == SIMG_HEADER_V1_SIZE
    fix_header_crc(header)
    return bytes(header)


def build_simg(domain: bytes, payload_type: int, payload: bytes, key,
               auth_key_slot: int = 0, security_version: int = 0,
               revocation_bitmap: int = 0,
               app_name: bytes = bytes(SIMG_APP_NAME_SIZE)) -> bytes:
    """Assemble a full SIMG (header || payload || signature). When `key` is
    given the RSA-3072 modulus is embedded and the signature covers
    header || payload; otherwise both are left zero (secure boot disabled only)."""
    if key is not None:
        modulus = rsa3072_modulus(key)
    else:
        modulus = bytes(SIMG_PUBLIC_KEY_LENGTH)
    header = build_header(domain, payload_type, payload, modulus, auth_key_slot,
                          security_version, revocation_bitmap, app_name)
    signed_region = header + payload
    signature = rsa3072_sign(key, signed_region) if key is not None \
        else bytes(SIMG_SIGNATURE_SIZE)
    return signed_region + signature


def make_simg_fsbl(fsbl: bytes, key=None, auth_key_slot: int = 0,
                   security_version: int = 0,
                   revocation_bitmap: int = 0,
                   domain: bytes = SIMG_DOMAIN_ZSBL,
                   payload_type: int = SIMG_PAYLOAD_FSBL,
                   app_name: bytes = None) -> bytes:
    """FSBL 'patch' form: the FSBL reserves a 512-byte header area at its start,
    so the SIMG header replaces it and the payload is fsbl[512:]. The 4-byte
    magic at offset 0 is a 'j .+0x200' trampoline jumping over the header. The
    FSBL image is verified by the ZSBL, so it carries the "ZSBL" domain by
    default. When app_name is None the placeholder header's app_name is kept."""
    if len(fsbl) <= SIMG_HEADER_V1_SIZE:
        raise ValueError(f"binary too small ({len(fsbl)} bytes); expected a "
                         f"linked image with a {SIMG_HEADER_V1_SIZE}-byte "
                         f"header area")
    if app_name is None:
        off, size = FIELD_OFFSETS["app_name"]
        app_name = fsbl[off:off + size]
    payload = fsbl[SIMG_HEADER_V1_SIZE:]
    return build_simg(domain, payload_type, payload, key,
                      auth_key_slot, security_version, revocation_bitmap,
                      app_name)


def make_simg(payload: bytes, payload_type: int, key=None,
              auth_key_slot: int = 0, security_version: int = 0,
              revocation_bitmap: int = 0,
              domain: bytes = SIMG_DOMAIN_FSBL,
              app_name: bytes = bytes(SIMG_APP_NAME_SIZE)) -> bytes:
    """Non-FSBL SIMG: header is prepended to the whole binary (entry point is
    load_addr, i.e. just past the header). Verified by the FSBL by default, so
    it carries the "FSBL" domain unless told otherwise."""
    return build_simg(domain, payload_type, payload, key,
                      auth_key_slot, security_version, revocation_bitmap,
                      app_name)


def placeholder_simg(payload_type: int, key=None) -> bytes:
    """A minimal valid SIMG for an image we don't have yet. With secure boot off
    the firmware only validates the header, so a 16-byte dummy payload loads
    fine (it just isn't functional firmware)."""
    return make_simg(b"\x00" * 16, payload_type, key)


# --- Parse ---

def parse_header(data: bytes) -> dict:
    """Decode a 512-byte SIMG header into a field dict (struct-based, no regex).

    Integer fields come back as ints; byte fields (domain, reserved*, config,
    public_key, app_name, payload_hash) as bytes. The extra key "crc_valid" is
    True iff header_crc matches the CRC recomputed over the header; "computed_crc"
    is that recomputed value.
    """
    if len(data) < SIMG_HEADER_V1_SIZE:
        raise ValueError(f"need at least {SIMG_HEADER_V1_SIZE} bytes, "
                         f"got {len(data)}")
    values = struct.unpack(_SIMG_FMT, data[:SIMG_HEADER_V1_SIZE])
    fields = dict(zip(_FIELD_NAMES, values))
    computed = compute_header_crc(data)
    fields["computed_crc"] = computed
    fields["crc_valid"] = (fields["header_crc"] == computed)
    return fields


def parse_simg(data: bytes) -> dict:
    """Parse a whole SIMG image (header || payload || signature).

    Returns the header field dict (see parse_header) plus:
      payload_offset, payload_length (from the header), signature_offset,
      total_size (header+payload+sig), image_length (actual len(data)),
      length_valid (image_length == total_size), and, when the buffer is long
      enough, the raw `payload` and `signature` byte slices.
    """
    fields = parse_header(data)
    payload_len = fields["payload_length"]
    payload_off = SIMG_HEADER_V1_SIZE
    sig_off = payload_off + payload_len
    total = sig_off + SIMG_SIGNATURE_SIZE
    fields["payload_offset"] = payload_off
    fields["signature_offset"] = sig_off
    fields["total_size"] = total
    fields["image_length"] = len(data)
    fields["length_valid"] = (len(data) == total)
    if len(data) >= sig_off:
        fields["payload"] = data[payload_off:sig_off]
    if len(data) >= total:
        fields["signature"] = data[sig_off:total]
    return fields


# --- Command line ---

def _auto_int(text: str) -> int:
    try:
        return int(text, 0)
    except ValueError:
        raise argparse.ArgumentTypeError(f"{text!r} is not an integer")


def _payload_type(text: str) -> int:
    if text.lower() in PAYLOAD_TYPES:
        return PAYLOAD_TYPES[text.lower()]
    try:
        value = int(text, 0)
    except ValueError:
        raise argparse.ArgumentTypeError(
            f"{text!r}: expected one of {', '.join(PAYLOAD_TYPES)} or an integer")
    if not 0 <= value <= 0xFF:
        raise argparse.ArgumentTypeError(f"payload type {value} must fit in a byte")
    return value


def _payload_type_name(value: int) -> str:
    for name, code in PAYLOAD_TYPES.items():
        if code == value:
            return name
    return str(value)


def main(argv=None) -> int:
    parser = argparse.ArgumentParser(
        prog="simg.py",
        description="Wrap a binary into a SIMG (header || payload || signature).")
    parser.add_argument("input", help="binary to wrap")
    parser.add_argument("-o", "--output",
                        help="output file (default: <input>.simg)")
    parser.add_argument("--patch-header", action="store_true",
                        help="replace the 512-byte placeholder header the "
                             "input already carries instead of prepending a "
                             "new one (default: prepend)")
    parser.add_argument("--payload-type", type=_payload_type, default="fsbl",
                        metavar="TYPE",
                        help="one of %s, or an integer (default: fsbl)"
                             % ", ".join(PAYLOAD_TYPES))
    parser.add_argument("--domain", metavar="DOMAIN",
                        help="verifying stage: ZSBL, FSBL or USER "
                             "(default: derived from the payload type)")
    parser.add_argument("--auth-key-slot", type=_auto_int, default=0,
                        metavar="N", help="OTP key slot (default: 0)")
    parser.add_argument("--revocation-bitmap", type=_auto_int, default=0,
                        metavar="BITS",
                        help="key slots to revoke; nonzero sets the "
                             "REVOCATION_PRESENT flag (default: 0)")
    parser.add_argument("--security-version", type=_auto_int, default=0,
                        metavar="N", help="anti-rollback counter (default: 0)")
    parser.add_argument("--app-name", metavar="NAME",
                        help="free-text name, up to 15 ASCII bytes (default: "
                             "kept from the placeholder with --patch-header, "
                             "else empty)")
    parser.add_argument("--key", metavar="KEY.pem",
                        help="RSA-3072 private key; embeds the modulus and "
                             "signs header || payload (default: unsigned)")
    args = parser.parse_args(argv)

    if args.domain is None:
        domain = DEFAULT_DOMAIN_FOR_PAYLOAD.get(args.payload_type)
        if domain is None:
            parser.error(f"--domain is required for payload type "
                         f"{args.payload_type}")
    else:
        try:
            domain = encode_domain(args.domain.upper())
        except ValueError as e:
            parser.error(str(e))
    if not 0 <= args.security_version <= 0xFFFFFFFF:
        parser.error("--security-version must fit in 32 bits")
    if not 0 <= args.revocation_bitmap <= 0xFFFFFFFFFFFFFFFF:
        parser.error("--revocation-bitmap must fit in 64 bits")
    try:
        validate_fields(domain, args.auth_key_slot, args.revocation_bitmap)
        app_name = encode_app_name(args.app_name) if args.app_name is not None \
            else None
    except ValueError as e:
        parser.error(str(e))

    try:
        with open(args.input, "rb") as f:
            data = f.read()
    except OSError as e:
        sys.exit(f"simg: {e}")

    has_magic = len(data) >= 4 and \
        struct.unpack_from("<I", data, 0)[0] == SIMG_MAGIC
    if args.patch_header and not has_magic:
        print(f"simg: warning: {args.input} does not start with the SIMG "
              f"magic; is the placeholder header present?", file=sys.stderr)
    if not args.patch_header and has_magic:
        print(f"simg: warning: {args.input} already starts with the SIMG "
              f"magic; prepending will leave a second header in the payload",
              file=sys.stderr)

    key = load_rsa3072_key(args.key) if args.key else None

    try:
        if args.patch_header:
            image = make_simg_fsbl(data, key, args.auth_key_slot,
                                   args.security_version,
                                   args.revocation_bitmap, domain,
                                   args.payload_type, app_name)
        else:
            image = make_simg(data, args.payload_type, key,
                              args.auth_key_slot, args.security_version,
                              args.revocation_bitmap, domain,
                              app_name if app_name is not None
                              else bytes(SIMG_APP_NAME_SIZE))
    except ValueError as e:
        sys.exit(f"simg: {e}")

    output = args.output
    if output is None:
        output = os.path.splitext(args.input)[0] + ".simg"
    try:
        with open(output, "wb") as f:
            f.write(image)
    except OSError as e:
        sys.exit(f"simg: {e}")

    fields = parse_header(image)
    payload_len = fields["payload_length"]
    domain_name = domain.rstrip(b"\x00").decode()
    app_name_text = fields["app_name"].rstrip(b"\x00").decode(errors="replace")
    print(f"{output}: {'patched' if args.patch_header else 'prepended'} header, "
          f"domain {domain_name}, "
          f"payload type {_payload_type_name(args.payload_type)}, "
          f"slot {args.auth_key_slot}, "
          f"app_name {app_name_text!r}, "
          f"payload {payload_len} bytes, total {len(image)} bytes, "
          f"{'signed' if key else 'unsigned'}")
    return 0


if __name__ == "__main__":
    sys.exit(main())
