#!/usr/bin/env python3
"""Render a hash-locked Atari ST PRG as an image-relative 68000 listing.

The utility reads one exact nested original archive member into memory.  It
does not extract, mount, modify, or assign a GEMDOS load base to game media.
The listing's addresses are PRG image-relative offsets, never runtime
addresses: GEMDOS relocation and load placement remain an explicit boundary.
"""

from __future__ import annotations

import argparse
import hashlib
from io import BytesIO
from pathlib import Path
from zipfile import ZipFile


PRG_HEADER_BYTES = 28


def read_le16(data: bytes, offset: int) -> int:
    if offset < 0 or offset + 2 > len(data):
        raise ValueError("truncated FAT12 word")
    return int.from_bytes(data[offset:offset + 2], "little")


def read_le32(data: bytes, offset: int) -> int:
    if offset < 0 or offset + 4 > len(data):
        raise ValueError("truncated FAT12 longword")
    return int.from_bytes(data[offset:offset + 4], "little")


def read_be16(data: bytes, offset: int) -> int:
    if offset < 0 or offset + 2 > len(data):
        raise ValueError("truncated Atari ST PRG word")
    return int.from_bytes(data[offset:offset + 2], "big")


def read_be32(data: bytes, offset: int) -> int:
    if offset < 0 or offset + 4 > len(data):
        raise ValueError("truncated Atari ST PRG longword")
    return int.from_bytes(data[offset:offset + 4], "big")


def fat12_member(image: bytes, wanted: str) -> bytes:
    """Read one exact 8.3 root file from an in-memory standard FAT12 disk."""
    if len(image) < 512:
        raise ValueError("FAT12 image is too short")
    bytes_per_sector = read_le16(image, 11)
    sectors_per_cluster = image[13]
    reserved_sectors = read_le16(image, 14)
    fat_count = image[16]
    root_entries = read_le16(image, 17)
    total_sectors = read_le16(image, 19) or read_le32(image, 32)
    sectors_per_fat = read_le16(image, 22)
    if (bytes_per_sector not in {512, 1024, 2048} or not sectors_per_cluster
            or not reserved_sectors or not fat_count or not root_entries
            or not total_sectors or not sectors_per_fat
            or total_sectors * bytes_per_sector > len(image)):
        raise ValueError("invalid FAT12 BIOS parameter block")
    fat_offset = reserved_sectors * bytes_per_sector
    root_offset = (reserved_sectors + fat_count * sectors_per_fat) * bytes_per_sector
    root_bytes = root_entries * 32
    root_sectors = (root_bytes + bytes_per_sector - 1) // bytes_per_sector
    data_offset = root_offset + root_sectors * bytes_per_sector
    if root_offset + root_bytes > len(image) or data_offset > len(image):
        raise ValueError("FAT12 regions are outside the disk")
    target = wanted.upper()
    first_cluster = size = None
    for index in range(root_entries):
        entry = root_offset + index * 32
        first = image[entry]
        attributes = image[entry + 11]
        if first == 0:
            break
        if first in {0xe5} or attributes == 0x0f or attributes & 0x18:
            continue
        name = image[entry:entry + 8].rstrip(b" ").decode("ascii", "strict")
        extension = image[entry + 8:entry + 11].rstrip(b" ").decode("ascii", "strict")
        candidate = name + ("." + extension if extension else "")
        if candidate.upper() == target:
            first_cluster, size = read_le16(image, entry + 26), read_le32(image, entry + 28)
            break
    if first_cluster is None or size is None or first_cluster < 2:
        raise ValueError("exact FAT12 program member is unavailable")
    cluster_size = sectors_per_cluster * bytes_per_sector
    result = bytearray()
    visited: set[int] = set()
    cluster = first_cluster
    while len(result) < size:
        if cluster < 2 or cluster >= 0xff8 or cluster in visited:
            raise ValueError("invalid or cyclic FAT12 cluster chain")
        visited.add(cluster)
        offset = data_offset + (cluster - 2) * cluster_size
        if offset + cluster_size > len(image):
            raise ValueError("FAT12 data cluster is outside the disk")
        result.extend(image[offset:offset + min(cluster_size, size - len(result))])
        if len(result) < size:
            entry_offset = fat_offset + cluster + cluster // 2
            value = read_le16(image, entry_offset)
            cluster = value & 0x0fff if cluster % 2 == 0 else value >> 4
    return bytes(result)


def read_exact_program(archive: Path, nested_member: str, disk_member: str,
                       program_member: str) -> tuple[bytes, bytes]:
    try:
        with ZipFile(archive) as outer:
            nested = outer.read(nested_member)
        with ZipFile(BytesIO(nested)) as inner:
            disk = inner.read(disk_member)
    except (KeyError, OSError, ValueError) as error:
        raise ValueError(f"unable to read exact nested Atari ST disk: {error}") from error
    return disk, fat12_member(disk, program_member)


def linear_listing(data: bytes) -> list[str]:
    try:
        from capstone import CS_ARCH_M68K, CS_MODE_BIG_ENDIAN, CS_MODE_M68K_000, Cs
    except ImportError as error:
        raise SystemExit("Install analysis dependencies: pip install -r requirements-analysis.txt") from error
    decoder = Cs(CS_ARCH_M68K, CS_MODE_BIG_ENDIAN | CS_MODE_M68K_000)
    lines: list[str] = []
    offset = 0
    while offset < len(data):
        item = next(decoder.disasm(data[offset:], offset, count=1), None)
        if item is None or item.address != offset or item.size <= 0 or item.size > len(data) - offset:
            lines.append(f"+0x{offset:05x}  .byte      0x{data[offset]:02x} ; code/data-unclassified")
            offset += 1
            continue
        lines.append(f"+0x{item.address:05x}  {item.mnemonic:<10} {item.op_str}".rstrip())
        offset += item.size
    return lines


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--archive", type=Path, required=True)
    parser.add_argument("--nested-member", required=True)
    parser.add_argument("--disk-member", required=True)
    parser.add_argument("--program", default="MILENIUM.TOS")
    parser.add_argument("--disk-sha256", required=True)
    parser.add_argument("--program-sha256", required=True)
    parser.add_argument("--output", type=Path)
    args = parser.parse_args()
    disk, program = read_exact_program(args.archive, args.nested_member, args.disk_member, args.program)
    disk_hash = hashlib.sha256(disk).hexdigest()
    program_hash = hashlib.sha256(program).hexdigest()
    if disk_hash != args.disk_sha256 or program_hash != args.program_sha256:
        raise SystemExit("exact Atari ST disk or PRG SHA-256 mismatch")
    if len(program) < PRG_HEADER_BYTES or read_be16(program, 0) != 0x601a:
        raise SystemExit("unsupported Atari ST PRG header")
    text_bytes, data_bytes = read_be32(program, 2), read_be32(program, 6)
    bss_bytes, symbol_bytes = read_be32(program, 10), read_be32(program, 14)
    loadable_bytes = text_bytes + data_bytes
    if PRG_HEADER_BYTES + loadable_bytes + symbol_bytes > len(program):
        raise SystemExit("Atari ST PRG segments lie outside its exact file")
    report = [
        "# Hash-locked Millennium Atari ST PRG linear candidate disassembly", "",
        f"- Source: `{args.archive.name}!{args.nested_member}!{args.disk_member}:{args.program}`",
        f"- Disk SHA-256: `{disk_hash}`",
        f"- PRG SHA-256: `{program_hash}`",
        f"- PRG header: TEXT `0x{text_bytes:x}`, DATA `0x{data_bytes:x}`, BSS `0x{bss_bytes:x}`, symbols `0x{symbol_bytes:x}`",
        "- Address space: PRG image-relative TEXT+DATA offsets; this is not a GEMDOS runtime load address.",
        "- Scope: full byte-complete linear candidate decode; code/data, relocation results, reachability, and ABI results are unclassified.",
        "- Coverage: every loadable source byte is rendered as an instruction or explicit `.byte`.", "", "```asm",
        *linear_listing(program[PRG_HEADER_BYTES:PRG_HEADER_BYTES + loadable_bytes]), "```", "",
    ]
    rendered = "\n".join(report)
    if args.output:
        args.output.write_text(rendered, encoding="utf-8")
    else:
        print(rendered)


if __name__ == "__main__":
    main()
