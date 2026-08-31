#!/usr/bin/env python3
"""Generate DOS reports from read-only files or exact ZIP members."""

from __future__ import annotations

import argparse
import hashlib
from io import BytesIO
from pathlib import Path
import sys
from zipfile import ZipFile

# Allow direct execution from a source checkout without installing the package.
sys.path.insert(0, str(Path(__file__).resolve().parents[1]))
from eon.dos import describe_bytes


def _render_instruction(instruction) -> str:
    return f"{instruction.address:05x}  {instruction.mnemonic:<8} {instruction.op_str}".rstrip()


def disassemble_linear(data: bytes, address: int) -> list[str]:
    """Render a byte-complete, deliberately unclassified x86 listing.

    Capstone stops its iterator at malformed or incomplete input. That is
    useful for normal disassembly, but a preservation report that calls itself
    complete must retain those bytes as explicit unknown data rather than
    silently omitting its tail.
    """
    try:
        from capstone import CS_ARCH_X86, CS_MODE_16, Cs
    except ImportError as error:
        raise SystemExit("Install analysis dependencies: pip install -r requirements-analysis.txt") from error
    decoder = Cs(CS_ARCH_X86, CS_MODE_16)
    lines: list[str] = []
    offset = 0
    while offset < len(data):
        instruction = next(decoder.disasm(data[offset:], address + offset, count=1), None)
        if (instruction is None or instruction.address != address + offset
                or instruction.size <= 0 or instruction.size > len(data) - offset):
            lines.append(f"{address + offset:05x}  .byte    0x{data[offset]:02x} ; code/data-unclassified")
            offset += 1
            continue
        lines.append(_render_instruction(instruction))
        offset += instruction.size
    return lines


def disassemble(data: bytes, address: int, offset: int, count: int = 96,
                *, complete_linear: bool = False) -> list[str]:
    if complete_linear:
        return disassemble_linear(data[offset : offset + count], address)
    try:
        from capstone import CS_ARCH_X86, CS_MODE_16, Cs
    except ImportError as error:
        raise SystemExit("Install analysis dependencies: pip install -r requirements-analysis.txt") from error
    decoder = Cs(CS_ARCH_X86, CS_MODE_16)
    return [_render_instruction(instruction)
            for instruction in decoder.disasm(data[offset : offset + count], address)]


def _little16(data: bytes, offset: int) -> int:
    if offset < 0 or offset + 2 > len(data):
        raise ValueError("truncated FAT12 word")
    return int.from_bytes(data[offset:offset + 2], "little")


def _little32(data: bytes, offset: int) -> int:
    if offset < 0 or offset + 4 > len(data):
        raise ValueError("truncated FAT12 longword")
    return int.from_bytes(data[offset:offset + 4], "little")


def read_fat12_members(image: bytes, names: list[str]) -> list[tuple[str, bytes]]:
    """Read only explicitly named 8.3 root files from a bounded FAT12 image."""
    if len(image) < 512:
        raise ValueError("FAT12 image is too short")
    sector_bytes = _little16(image, 11)
    sectors_per_cluster = image[13]
    reserved = _little16(image, 14)
    fat_count = image[16]
    root_entries = _little16(image, 17)
    total_sectors = _little16(image, 19) or _little32(image, 32)
    sectors_per_fat = _little16(image, 22)
    if (sector_bytes not in {512, 1024, 2048} or not sectors_per_cluster
            or not reserved or not fat_count or not root_entries or not total_sectors
            or not sectors_per_fat or total_sectors * sector_bytes > len(image)):
        raise ValueError("invalid FAT12 BIOS parameter block")
    fat_offset = reserved * sector_bytes
    root_offset = (reserved + fat_count * sectors_per_fat) * sector_bytes
    root_bytes = root_entries * 32
    root_sectors = (root_bytes + sector_bytes - 1) // sector_bytes
    data_offset = root_offset + root_sectors * sector_bytes
    if root_offset + root_bytes > len(image) or data_offset > len(image):
        raise ValueError("FAT12 regions lie outside image")
    entries: dict[str, tuple[int, int]] = {}
    for index in range(root_entries):
        offset = root_offset + index * 32
        first, attributes = image[offset], image[offset + 11]
        if first == 0:
            break
        if first == 0xe5 or attributes == 0x0f or attributes & 0x18:
            continue
        stem = image[offset:offset + 8].rstrip(b" ").decode("ascii", "strict")
        extension = image[offset + 8:offset + 11].rstrip(b" ").decode("ascii", "strict")
        name = stem + ("." + extension if extension else "")
        entries[name.upper()] = (_little16(image, offset + 26), _little32(image, offset + 28))
    result: list[tuple[str, bytes]] = []
    cluster_bytes = sectors_per_cluster * sector_bytes
    for requested in names:
        entry = entries.get(requested.upper())
        if not entry:
            raise ValueError(f"exact FAT12 member is unavailable: {requested}")
        cluster, size = entry
        if cluster < 2:
            raise ValueError(f"invalid FAT12 first cluster: {requested}")
        data = bytearray()
        visited: set[int] = set()
        while len(data) < size:
            if cluster < 2 or cluster >= 0xff8 or cluster in visited:
                raise ValueError(f"invalid or cyclic FAT12 chain: {requested}")
            visited.add(cluster)
            offset = data_offset + (cluster - 2) * cluster_bytes
            if offset + cluster_bytes > len(image):
                raise ValueError(f"FAT12 cluster outside image: {requested}")
            data.extend(image[offset:offset + min(cluster_bytes, size - len(data))])
            if len(data) < size:
                fat_entry = fat_offset + cluster + cluster // 2
                value = _little16(image, fat_entry)
                cluster = value & 0x0fff if cluster % 2 == 0 else value >> 4
        result.append((requested, bytes(data)))
    return result


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("directory", type=Path, nargs="?",
                        help="Read-only DOS directory (legacy investigator input)")
    parser.add_argument("--archive", type=Path,
                        help="Original outer ZIP; requires one or more exact --member paths")
    parser.add_argument("--fat12-archive", type=Path,
                        help="Original ZIP with one exact FAT12 --fat12-member image")
    parser.add_argument("--fat12-member",
                        help="Exact FAT12 image member inside --fat12-archive")
    parser.add_argument("--member", action="append", default=[],
                        help="Exact EXE/COM path inside --archive; repeat for every program")
    parser.add_argument("--complete-linear", action="store_true",
                        help="Decode every byte as an explicitly unproven linear x86 candidate")
    parser.add_argument("--output", type=Path)
    args = parser.parse_args()
    archive_mode = args.archive is not None
    fat12_archive_mode = args.fat12_archive is not None
    directory_mode = args.directory is not None
    if sum((archive_mode, fat12_archive_mode, directory_mode)) != 1:
        raise SystemExit("Specify exactly one directory, --archive, or --fat12-archive")
    if (archive_mode or fat12_archive_mode) and not args.member:
        raise SystemExit("archive modes require one or more exact --member names")
    if fat12_archive_mode != (args.fat12_member is not None):
        raise SystemExit("--fat12-archive requires --fat12-member")
    if directory_mode:
        sources = [(path.name, path.read_bytes()) for path in sorted(
            [*args.directory.glob("*.EXE"), *args.directory.glob("*.COM")])]
    elif archive_mode:
        try:
            with ZipFile(args.archive) as archive:
                sources = [(member, archive.read(member)) for member in args.member]
        except (KeyError, OSError, ValueError) as error:
            raise SystemExit(f"Unable to read exact DOS archive member: {error}") from error
    else:
        try:
            with ZipFile(args.fat12_archive) as archive:
                image = archive.read(args.fat12_member)
            sources = read_fat12_members(image, args.member)
        except (KeyError, OSError, ValueError) as error:
            raise SystemExit(f"Unable to read exact FAT12 DOS members: {error}") from error
    lines = ["# Generated Millennium DOS binary report", ""]
    for name, data in sources:
        report = describe_bytes(Path(name).name, data)
        complete_linear = args.complete_linear
        has_entry_listing = complete_linear or report["entry_within_image"]
        listing_offset = 0 if complete_linear else report["entry_file_offset"]
        listing_address = 0x100 if complete_linear else report["entry_load_address"]
        listing_count = len(data) if complete_linear else 96
        entry_boundary = (
            "- Entry candidate is within the hash-bound member."
            if report["entry_within_image"] else
            "- Entry candidate lies outside the hash-bound member by "
            f"{report['entry_beyond_image_bytes']} bytes; it is an external runtime boundary, not a static code listing."
        )
        lines.extend([
            f"## `{report['name']}`",
            "",
            f"- Size: {report['size']} bytes",
            f"- SHA-256: `{hashlib.sha256(data).hexdigest()}`",
            f"- Entry file offset: `0x{report['entry_file_offset']:x}`",
            f"- Entry load address: `0x{report['entry_load_address']:x}`",
            entry_boundary,
            f"- Syntactic interrupt occurrences: {report['interrupts']}",
            "- Listing scope: " + ("entire image, linear candidate only (code/data unclassified)"
                if complete_linear else ("96 bytes from inferred entry candidate" if has_entry_listing
                else "no entry listing: inferred target is outside the member")),
            *( ["- Linear coverage: every source byte is rendered as an instruction or explicit `.byte`."]
               if args.complete_linear else [] ),
            "",
            "```asm",
            *(disassemble(
                data,
                listing_address,
                listing_offset,
                listing_count,
                complete_linear=complete_linear,
            ) if has_entry_listing else ["; no bytes: inferred entry lies beyond the hash-bound member"]),
            "```",
            "",
            "Selected strings:",
            "",
        ])
        selected = [item for item in report["strings"] if any(c.isalpha() for c in item["text"])]
        lines.extend(
            f"- `0x{item['offset']:x}`: `{item['text'].replace('`', chr(92) + '`')}`"
            for item in selected[:40]
        )
        lines.append("")
    output = "\n".join(lines)
    if args.output:
        args.output.write_text(output, encoding="utf-8")
    else:
        print(output)


if __name__ == "__main__":
    main()
