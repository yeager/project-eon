#!/usr/bin/env python3
"""Generate DOS reports from read-only files or exact ZIP members."""

from __future__ import annotations

import argparse
import hashlib
from io import BytesIO
import json
import os
from pathlib import Path
import stat
import sys
from zipfile import ZipFile

# Allow direct execution from a source checkout without installing the package.
sys.path.insert(0, str(Path(__file__).resolve().parents[1]))
from eon.dos import describe_bytes


def sha256_file(path: Path) -> str:
    digest = hashlib.sha256()
    with path.open("rb") as source:
        for chunk in iter(lambda: source.read(1024 * 1024), b""):
            digest.update(chunk)
    return digest.hexdigest()


def read_verified_direct_file(path: Path, expected_sha256: str,
                              expected_size: int | None = None) -> bytes:
    """Read one direct-media child through a no-follow descriptor once.

    A directory scan is not authority for a later read: the selected member is
    reopened and rehashed immediately before it becomes a report source.
    ``O_NOFOLLOW`` and ``fstat`` close the common replacement/symlink race
    without creating a materialized copy beside the user's media.
    """
    try:
        before = path.lstat()
        if stat.S_ISLNK(before.st_mode) or not stat.S_ISREG(before.st_mode):
            raise ValueError("direct member is not its declared regular file")
        flags = os.O_RDONLY | getattr(os, "O_NOFOLLOW", 0)
        descriptor = os.open(path, flags)
        with os.fdopen(descriptor, "rb") as source:
            status = os.fstat(source.fileno())
            if not stat.S_ISREG(status.st_mode) or (expected_size is not None
                    and status.st_size != expected_size):
                raise ValueError("direct member is not its declared regular file")
            data = source.read()
        after = path.lstat()
    except OSError as error:
        raise ValueError("unable to read declared direct media member") from error
    if (stat.S_ISLNK(after.st_mode) or not stat.S_ISREG(after.st_mode)
            or before.st_dev != after.st_dev or before.st_ino != after.st_ino
            or before.st_size != after.st_size or before.st_mtime_ns != after.st_mtime_ns
            or (status.st_dev, status.st_ino) != (before.st_dev, before.st_ino)):
        raise ValueError("direct member changed while being read")
    if (expected_size is not None and len(data) != expected_size
            or hashlib.sha256(data).hexdigest() != expected_sha256):
        raise ValueError("direct member hash mismatch")
    return data


def parse_member_hashes(values: list[str], members: list[str]) -> dict[str, str]:
    hashes: dict[str, str] = {}
    for value in values:
        name, separator, digest = value.partition("=")
        if not separator or not name or len(digest) != 64 or any(c not in "0123456789abcdef" for c in digest):
            raise ValueError("--member-sha256 must be exact-member-path=lowercase-sha256")
        if name in hashes:
            raise ValueError("duplicate --member-sha256 path")
        hashes[name] = digest
    if set(hashes) != set(members):
        raise ValueError("every requested --member needs exactly one --member-sha256")
    return hashes


def require_external_output(path: Path) -> Path:
    if not path.is_absolute():
        raise ValueError("output path must be absolute")
    lexical = os.path.normpath(str(path))
    if lexical == "/tmp" or lexical.startswith("/tmp/") or lexical == "/private/tmp" or lexical.startswith("/private/tmp/"):
        raise ValueError("output must be outside /tmp")
    if path.exists() or path.is_symlink():
        raise ValueError("output must not already exist or be a symlink")
    parent = path.parent.resolve(strict=True)
    checkout = Path(__file__).resolve().parents[1]
    candidate = parent / path.name
    if candidate == checkout or checkout in candidate.parents:
        raise ValueError("output must be outside the repository")
    return path


def verify_direct_directory(directory: Path, set_sha256: str) -> Path:
    manifest_path = Path(__file__).resolve().parents[1] / "docs" / "release-manifest.json"
    try:
        sets = json.loads(manifest_path.read_text(encoding="utf-8"))["direct_media_sets"]
        declared = next(entry for entry in sets if entry["set_sha256"] == set_sha256)
    except (OSError, json.JSONDecodeError, KeyError, StopIteration) as error:
        raise ValueError("directory media-set identity is not declared") from error
    try:
        directory_status = directory.lstat()
    except OSError as error:
        raise ValueError("direct media root is unavailable") from error
    if directory.is_symlink() or not stat.S_ISDIR(directory_status.st_mode):
        raise ValueError("direct media root must be a regular non-symlink directory")
    resolved_directory = directory.resolve(strict=True)
    canonical = ""
    for member in declared["members"]:
        name = member["name"]
        # Direct-set manifest members are flat DOS filenames. Reject a ledger
        # change that tries to traverse away from the admitted root before it
        # can influence an OS path lookup.
        if (not name or Path(name).name != name or "/" in name or "\\" in name):
            raise ValueError("declared direct media-set member name is unsafe")
        path = resolved_directory / name
        try:
            read_verified_direct_file(path, member["sha256"], member["size"])
        except ValueError:
            raise ValueError("directory does not match declared direct media-set")
        canonical += f'{name}\t{member["size"]}\t{member["sha256"]}\n'
    if hashlib.sha256(canonical.encode("ascii")).hexdigest() != set_sha256:
        raise ValueError("declared direct media-set canonical hash mismatch")
    return resolved_directory


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
                        help="Exact installed DOS directory")
    parser.add_argument("--directory-set-sha256", help="Declared complete direct-media set SHA-256")
    parser.add_argument("--archive", type=Path,
                        help="Original outer ZIP; requires one or more exact --member paths")
    parser.add_argument("--archive-sha256", help="SHA-256 of exact --archive")
    parser.add_argument("--fat12-archive", type=Path,
                        help="Original ZIP with one exact FAT12 --fat12-member image")
    parser.add_argument("--fat12-archive-sha256", help="SHA-256 of exact --fat12-archive")
    parser.add_argument("--fat12-member",
                        help="Exact FAT12 image member inside --fat12-archive")
    parser.add_argument("--fat12-member-sha256", help="SHA-256 of exact FAT12 image member")
    parser.add_argument("--member", action="append", default=[],
                        help="Exact EXE/COM path inside --archive; repeat for every program")
    parser.add_argument("--member-sha256", action="append", default=[],
                        help="Exact member path=lowercase-sha256; repeat for every --member")
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
    if directory_mode and not args.member:
        raise SystemExit("direct directory input requires one or more exact --member names")
    if fat12_archive_mode != (args.fat12_member is not None) or fat12_archive_mode != (args.fat12_member_sha256 is not None):
        raise SystemExit("--fat12-archive requires --fat12-member and --fat12-member-sha256")
    if directory_mode != (args.directory_set_sha256 is not None):
        raise SystemExit("direct directory input requires --directory-set-sha256")
    if archive_mode and not args.archive_sha256:
        raise SystemExit("--archive requires --archive-sha256")
    if fat12_archive_mode and not args.fat12_archive_sha256:
        raise SystemExit("--fat12-archive requires --fat12-archive-sha256")
    try:
        member_hashes = parse_member_hashes(args.member_sha256, args.member)
        if args.output:
            require_external_output(args.output)
    except ValueError as error:
        raise SystemExit(str(error)) from error
    if directory_mode:
        try:
            directory = verify_direct_directory(args.directory, args.directory_set_sha256)
            sources = []
            for member in args.member:
                if not member or Path(member).name != member or "/" in member or "\\" in member:
                    raise ValueError("requested direct member name is unsafe")
                sources.append((member, read_verified_direct_file(
                    directory / member, member_hashes[member])))
        except (OSError, ValueError) as error:
            raise SystemExit(f"Unable to read exact DOS directory member: {error}") from error
    elif archive_mode:
        try:
            if not args.archive.is_file() or args.archive.is_symlink() or sha256_file(args.archive) != args.archive_sha256:
                raise ValueError("outer archive SHA-256 mismatch")
            with ZipFile(args.archive) as archive:
                sources = [(member, archive.read(member)) for member in args.member]
            if any(hashlib.sha256(data).hexdigest() != member_hashes[name] for name, data in sources):
                raise ValueError("archive member SHA-256 mismatch")
        except (KeyError, OSError, ValueError) as error:
            raise SystemExit(f"Unable to read exact DOS archive member: {error}") from error
    else:
        try:
            if (not args.fat12_archive.is_file() or args.fat12_archive.is_symlink()
                    or sha256_file(args.fat12_archive) != args.fat12_archive_sha256):
                raise ValueError("outer FAT12 archive SHA-256 mismatch")
            with ZipFile(args.fat12_archive) as archive:
                image = archive.read(args.fat12_member)
            if hashlib.sha256(image).hexdigest() != args.fat12_member_sha256:
                raise ValueError("FAT12 member SHA-256 mismatch")
            sources = read_fat12_members(image, args.member)
            if any(hashlib.sha256(data).hexdigest() != member_hashes[name] for name, data in sources):
                raise ValueError("FAT12 program SHA-256 mismatch")
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
        require_external_output(args.output).write_text(output, encoding="utf-8")
    else:
        print(output)


if __name__ == "__main__":
    main()
