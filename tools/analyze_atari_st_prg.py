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
from zipfile import BadZipFile, ZipFile


PRG_HEADER_BYTES = 28
ROOT = Path(__file__).resolve().parents[1]


class AnalysisError(ValueError):
    """An input/provenance boundary rejected by the preservation tool."""


def sha256_file(path: Path) -> str:
    digest = hashlib.sha256()
    with path.open("rb") as stream:
        while chunk := stream.read(1024 * 1024):
            digest.update(chunk)
    return digest.hexdigest()


def require_sha256(value: str, label: str) -> str:
    if len(value) != 64 or any(character not in "0123456789abcdef" for character in value):
        raise AnalysisError(f"{label} must be a lowercase SHA-256")
    return value


def require_regular_input(path: Path, label: str) -> Path:
    try:
        status = path.lstat()
    except OSError as error:
        raise AnalysisError(f"unable to stat {label}: {error}") from error
    if path.is_symlink() or not path.is_file():
        raise AnalysisError(f"{label} must be an existing regular non-symlink file")
    return path


def require_external_output(path: Path) -> Path:
    """Reject paths that could turn a preservation report into repository data."""
    if not path.is_absolute():
        raise AnalysisError("output path must be absolute")
    if path.exists() or path.is_symlink():
        raise AnalysisError("output path must not already exist or be a symlink")
    try:
        parent = path.parent.resolve(strict=True)
    except OSError as error:
        raise AnalysisError(f"output parent cannot be resolved: {error}") from error
    if not parent.is_dir():
        raise AnalysisError("output parent must be an existing directory")
    resolved = parent / path.name
    if resolved == ROOT or ROOT in resolved.parents:
        raise AnalysisError("output path must be outside the repository")
    tmp = Path("/tmp")
    if resolved == tmp or tmp in resolved.parents:
        raise AnalysisError("output path must be outside /tmp")
    return resolved


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


def read_exact_program(archive: Path, archive_sha256: str, nested_member: str | None,
                       nested_sha256: str | None, disk_member: str,
                       program_member: str) -> tuple[bytes, bytes, str | None]:
    """Read a hash-locked direct or carrier-contained disk without extracting it.

    A direct release is one ZIP whose named member is the original ST image.
    A carrier release is a ZIP containing one exact nested ZIP; both ZIP byte
    streams are independently authenticated before the named image is read.
    """
    require_regular_input(archive, "archive")
    if sha256_file(archive) != require_sha256(archive_sha256, "archive SHA-256"):
        raise AnalysisError("outer archive SHA-256 mismatch")
    if (nested_member is None) != (nested_sha256 is None):
        raise AnalysisError("nested member and nested SHA-256 must be supplied together")
    try:
        with ZipFile(archive) as outer:
            if nested_member is None:
                disk = outer.read(disk_member)
                return disk, fat12_member(disk, program_member), None
            nested = outer.read(nested_member)
        if hashlib.sha256(nested).hexdigest() != require_sha256(nested_sha256, "nested archive SHA-256"):
            raise AnalysisError("nested archive SHA-256 mismatch")
        with ZipFile(BytesIO(nested)) as inner:
            disk = inner.read(disk_member)
    except (BadZipFile, KeyError, OSError, ValueError) as error:
        raise AnalysisError(f"unable to read exact Atari ST disk container: {error}") from error
    return disk, fat12_member(disk, program_member), nested_sha256


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


def main(argv: list[str] | None = None) -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--archive", type=Path, required=True)
    parser.add_argument("--archive-sha256", required=True,
                        help="SHA-256 of the exact selected outer ZIP")
    parser.add_argument("--nested-member",
                        help="Exact nested ZIP member; omit for a direct ZIP container")
    parser.add_argument("--nested-sha256",
                        help="SHA-256 of --nested-member; required with that option")
    parser.add_argument("--disk-member", required=True)
    parser.add_argument("--program", default="MILENIUM.TOS")
    parser.add_argument("--disk-sha256", required=True)
    parser.add_argument("--program-sha256", required=True)
    parser.add_argument("--output", type=Path)
    args = parser.parse_args(argv)
    try:
        archive_sha256 = require_sha256(args.archive_sha256, "archive SHA-256")
        if (args.nested_member is None) != (args.nested_sha256 is None):
            raise AnalysisError("--nested-member and --nested-sha256 must be supplied together")
        disk, program, nested_sha256 = read_exact_program(
            args.archive, archive_sha256, args.nested_member, args.nested_sha256,
            args.disk_member, args.program)
    except AnalysisError as error:
        parser.error(str(error))
    disk_hash = hashlib.sha256(disk).hexdigest()
    program_hash = hashlib.sha256(program).hexdigest()
    if disk_hash != require_sha256(args.disk_sha256, "disk SHA-256") or program_hash != require_sha256(
            args.program_sha256, "program SHA-256"):
        parser.error("exact Atari ST disk or PRG SHA-256 mismatch")
    if len(program) < PRG_HEADER_BYTES or read_be16(program, 0) != 0x601a:
        raise SystemExit("unsupported Atari ST PRG header")
    text_bytes, data_bytes = read_be32(program, 2), read_be32(program, 6)
    bss_bytes, symbol_bytes = read_be32(program, 10), read_be32(program, 14)
    loadable_bytes = text_bytes + data_bytes
    if PRG_HEADER_BYTES + loadable_bytes + symbol_bytes > len(program):
        raise SystemExit("Atari ST PRG segments lie outside its exact file")
    report = [
        "# Hash-locked Millennium Atari ST PRG linear candidate disassembly", "",
        f"- Source: `{args.archive.name}!"
        + (f"{args.nested_member}!" if args.nested_member else "")
        + f"{args.disk_member}:{args.program}`",
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
        try:
            require_external_output(args.output).write_text(rendered, encoding="utf-8")
        except (AnalysisError, OSError) as error:
            parser.error(str(error))
    else:
        print(rendered)
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
