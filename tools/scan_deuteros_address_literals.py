#!/usr/bin/env python3
"""Scan Deuteros ADF members in ZIP archives for literal 68000 addresses.

Archives are opened read-only and bounded ADF members are read in memory.
The report contains hashes and offsets only; it never extracts or writes media.
"""

from __future__ import annotations

import argparse
import hashlib
import zipfile
from dataclasses import dataclass
from pathlib import Path


MAX_ADF_BYTES = 2 * 1024 * 1024


@dataclass(frozen=True)
class AddressLiteralHit:
    encoding: str
    offsets: tuple[int, ...]


@dataclass(frozen=True)
class ADFScan:
    archive: Path
    member: str
    size: int
    sha256: str
    hits: tuple[AddressLiteralHit, ...]


def _all_offsets(data: bytes, pattern: bytes) -> tuple[int, ...]:
    offsets: list[int] = []
    offset = 0
    while True:
        offset = data.find(pattern, offset)
        if offset < 0:
            return tuple(offsets)
        offsets.append(offset)
        offset += 1


def find_address_literals(data: bytes, address: int) -> tuple[AddressLiteralHit, ...]:
    """Find unaligned big-endian absolute-long and contiguous 24-bit forms."""
    if address < 0 or address > 0xFFFFFF:
        raise ValueError("68000 address must fit in 24 bits")
    patterns = (
        ("absolute-long", address.to_bytes(4, "big")),
        ("24-bit", address.to_bytes(3, "big")),
    )
    return tuple(
        AddressLiteralHit(kind, offsets)
        for kind, pattern in patterns
        if (offsets := _all_offsets(data, pattern))
    )


def scan_archive(path: Path, address: int) -> tuple[ADFScan, ...]:
    scans: list[ADFScan] = []
    with zipfile.ZipFile(path) as archive:
        for info in archive.infolist():
            if info.is_dir() or not info.filename.lower().endswith(".adf"):
                continue
            if info.file_size <= 0 or info.file_size > MAX_ADF_BYTES:
                raise ValueError(f"ADF member exceeds scan bound: {info.filename}")
            payload = archive.read(info)
            if len(payload) != info.file_size:
                raise ValueError(f"ADF member size changed while scanning: {info.filename}")
            scans.append(ADFScan(path, info.filename, len(payload),
                hashlib.sha256(payload).hexdigest(), find_address_literals(payload, address)))
    return tuple(scans)


def scan_collection(root: Path, address: int) -> tuple[ADFScan, ...]:
    scans: list[ADFScan] = []
    for path in sorted(root.glob("Deuteros*.zip")):
        scans.extend(scan_archive(path, address))
    return tuple(scans)


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("root", type=Path, help="directory containing user-supplied Deuteros ZIPs")
    parser.add_argument("--address", type=lambda value: int(value, 0), default=0x4077E)
    args = parser.parse_args()
    try:
        scans = scan_collection(args.root, args.address)
    except (OSError, ValueError, zipfile.BadZipFile) as error:
        parser.error(str(error))
    archives = len({scan.archive for scan in scans})
    print(f"archives_scanned={archives}")
    print(f"adf_members_scanned={len(scans)}")
    print(f"address=0x{args.address:06x}")
    for scan in scans:
        for hit in scan.hits:
            print(f"{scan.archive.name}\t{scan.member}\t{scan.sha256}\t"
                f"{hit.encoding}\t{len(hit.offsets)}\t"
                + ",".join(f"0x{offset:x}" for offset in hit.offsets[:64]))
    print("literal_hits=" + str(sum(len(scan.hits) for scan in scans)))
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
