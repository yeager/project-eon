#!/usr/bin/env python3
"""Read and disassemble one FAT12 PRG from an exact Atari STX disk in memory.

The tool preserves the physical image and never writes a flat disk or PRG.
When FAT mirrors differ, it admits a file only if both copies resolve the
same complete cluster chain and byte stream. The resulting listing is a
linear candidate; runtime load address and reachability remain unknown.
"""

from __future__ import annotations

import argparse
import hashlib
from pathlib import Path
import sys
from zipfile import BadZipFile, ZipFile

ROOT = Path(__file__).resolve().parents[1]
if str(ROOT) not in sys.path:
    sys.path.insert(0, str(ROOT))

from tools import analyze_atari_st_prg as prg


class StxAnalysisError(ValueError):
    """The supplied STX transport or exact physical file chain was rejected."""


def le16(data: bytes, offset: int) -> int:
    return prg.read_le16(data, offset)


def le32(data: bytes, offset: int) -> int:
    return prg.read_le32(data, offset)


def index_stx(image: bytes) -> dict[tuple[int, int, int], bytes]:
    if len(image) < 16 or image[:4] != b"RSY\0" or le16(image, 4) != 3:
        raise StxAnalysisError("unsupported STX version or header")
    expected_tracks = image[10]
    sectors: dict[tuple[int, int, int], bytes] = {}
    offset = 16
    tracks = 0
    while offset < len(image):
        if len(image) - offset < 16:
            raise StxAnalysisError("truncated STX track header")
        block_size, fuzzy_size = le32(image, offset), le32(image, offset + 4)
        count = le16(image, offset + 8)
        if block_size < 16 or block_size > len(image) - offset:
            raise StxAnalysisError("invalid STX track extent")
        descriptor_end = offset + 16 + count * 16
        if descriptor_end > offset + block_size or fuzzy_size > offset + block_size - descriptor_end:
            raise StxAnalysisError("STX sector descriptors exceed track extent")
        payload_base = descriptor_end + fuzzy_size
        track_number = image[offset + 14]
        for index in range(count):
            descriptor = offset + 16 + index * 16
            relative = le32(image, descriptor)
            track, side, sector_id, size_code = image[descriptor + 8:descriptor + 12]
            if size_code > 7 or track != (track_number & 0x7f) or side != (track_number >> 7):
                raise StxAnalysisError("unsupported STX sector identity")
            length = 128 << size_code
            payload = payload_base + relative
            if payload > offset + block_size or length > offset + block_size - payload:
                raise StxAnalysisError("STX sector payload exceeds track extent")
            key = (track, side, sector_id)
            if key in sectors:
                raise StxAnalysisError("duplicate STX sector identity")
            sectors[key] = image[payload:payload + length]
        offset += block_size
        tracks += 1
    if offset != len(image) or tracks != expected_tracks:
        raise StxAnalysisError("STX track count differs from header")
    return sectors


def stx_fat12_member(image: bytes, wanted: str) -> bytes:
    sectors = index_stx(image)
    try:
        boot = sectors[(0, 0, 1)]
    except KeyError as error:
        raise StxAnalysisError("STX boot-sector identity is absent") from error
    bps, spc = le16(boot, 11), boot[13]
    reserved, fat_count, root_entries = le16(boot, 14), boot[16], le16(boot, 17)
    total = le16(boot, 19) or le32(boot, 32)
    sectors_per_fat, spt, heads = le16(boot, 22), le16(boot, 24), le16(boot, 26)
    if (bps != 512 or not spc or not reserved or fat_count != 2 or not root_entries
            or not total or not sectors_per_fat or not spt or not heads):
        raise StxAnalysisError("invalid STX FAT12 geometry")

    def logical_sector(lba: int) -> bytes:
        if lba < 0 or lba >= total:
            raise StxAnalysisError("FAT12 logical sector is outside declared media")
        cylinder, within = divmod(lba, spt * heads)
        side, sector_index = divmod(within, spt)
        try:
            result = sectors[(cylinder, side, sector_index + 1)]
        except KeyError as error:
            raise StxAnalysisError("FAT12 logical sector is absent from STX") from error
        if len(result) != bps:
            raise StxAnalysisError("STX sector size differs from BPB")
        return result

    root_start = reserved + fat_count * sectors_per_fat
    root_count = (root_entries * 32 + bps - 1) // bps
    data_start = root_start + root_count
    if data_start > total:
        raise StxAnalysisError("STX FAT12 regions exceed declared medium")
    root = b"".join(logical_sector(root_start + index) for index in range(root_count))
    target = wanted.upper()
    first_cluster = file_size = None
    for index in range(root_entries):
        entry = root[index * 32:index * 32 + 32]
        if len(entry) < 32 or entry[0] == 0:
            break
        if entry[0] == 0xe5 or entry[11] == 0x0f or entry[11] & 0x18:
            continue
        name = entry[:8].rstrip(b" ").decode("ascii", "strict")
        extension = entry[8:11].rstrip(b" ").decode("ascii", "strict")
        candidate = (name + ("." + extension if extension else "")).upper()
        if candidate == target:
            first_cluster, file_size = le16(entry, 26), le32(entry, 28)
            break
    if first_cluster is None or file_size is None or first_cluster < 2:
        raise StxAnalysisError("exact STX FAT12 PRG is unavailable")

    fats = [b"".join(logical_sector(reserved + fat_index * sectors_per_fat + index)
                     for index in range(sectors_per_fat)) for fat_index in range(2)]

    def follow(fat: bytes) -> tuple[tuple[int, ...], bytes]:
        cluster = first_cluster
        chain: list[int] = []
        payload = bytearray()
        visited: set[int] = set()
        while len(payload) < file_size:
            if cluster < 2 or cluster in visited:
                raise StxAnalysisError("invalid or cyclic STX FAT12 cluster chain")
            visited.add(cluster)
            first_lba = data_start + (cluster - 2) * spc
            if first_lba + spc > total:
                raise StxAnalysisError("STX FAT12 cluster lies outside medium")
            chain.append(cluster)
            for index in range(spc):
                payload.extend(logical_sector(first_lba + index))
            if len(payload) < file_size:
                fat_offset = cluster + cluster // 2
                if fat_offset + 2 > len(fat):
                    raise StxAnalysisError("STX FAT12 link lies outside FAT")
                pair = le16(fat, fat_offset)
                cluster = pair & 0x0fff if cluster % 2 == 0 else pair >> 4
        return tuple(chain), bytes(payload[:file_size])

    first, second = follow(fats[0]), follow(fats[1])
    if first != second:
        raise StxAnalysisError("FAT mirrors disagree on this PRG chain or payload")
    return first[1]


def read_exact_program(archive: Path, archive_sha256: str, member: str,
                       member_sha256: str, program_name: str) -> tuple[bytes, bytes]:
    if not archive.is_file() or archive.is_symlink():
        raise StxAnalysisError("archive must be a regular non-symlink file")
    digest = hashlib.sha256(archive.read_bytes()).hexdigest()
    if digest != prg.require_sha256(archive_sha256, "archive SHA-256"):
        raise StxAnalysisError("outer archive SHA-256 mismatch")
    try:
        with ZipFile(archive) as stream:
            image = stream.read(member)
        if hashlib.sha256(image).hexdigest() != prg.require_sha256(member_sha256, "STX SHA-256"):
            raise StxAnalysisError("STX member SHA-256 mismatch")
        executable = stx_fat12_member(image, program_name)
    except (BadZipFile, KeyError, OSError, UnicodeError, ValueError) as error:
        if isinstance(error, StxAnalysisError):
            raise
        raise StxAnalysisError(f"unable to read exact STX PRG: {error}") from error
    return image, executable


def parse_relocations(program: bytes, text: int, data: int, symbols: int) -> list[tuple[int, int]]:
    """Read the PRG's bounded relocation stream without applying a load base."""
    if prg.read_be16(program, 26) != 0:
        if 28 + text + data + symbols != len(program):
            raise StxAnalysisError("absolute PRG has unexpected trailing bytes")
        return []
    cursor = 28 + text + data + symbols
    if cursor > len(program) or len(program) - cursor < 4:
        raise StxAnalysisError("PRG relocation table is truncated")
    relocation = prg.read_be32(program, cursor)
    cursor += 4
    if relocation == 0:
        if cursor != len(program):
            raise StxAnalysisError("PRG empty relocation table has trailing bytes")
        return []
    loadable = text + data
    if loadable < 4 or relocation > loadable - 4:
        raise StxAnalysisError("first PRG relocation lies outside text and data")
    result = [(relocation, prg.read_be32(program, 28 + relocation))]
    while cursor < len(program):
        delta = program[cursor]
        cursor += 1
        if delta == 0:
            if cursor != len(program):
                raise StxAnalysisError("PRG relocation stream has trailing bytes")
            return result
        relocation += 254 if delta == 1 else delta
        if relocation > loadable - 4:
            raise StxAnalysisError("PRG relocation lies outside text and data")
        result.append((relocation, prg.read_be32(program, 28 + relocation)))
    raise StxAnalysisError("PRG relocation stream is unterminated")


def main(argv: list[str] | None = None) -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--archive", type=Path, required=True)
    parser.add_argument("--archive-sha256", required=True)
    parser.add_argument("--disk-member", required=True)
    parser.add_argument("--disk-sha256", required=True)
    parser.add_argument("--program", default="EXEC.TOS")
    parser.add_argument("--program-sha256", required=True)
    parser.add_argument("--output", type=Path)
    args = parser.parse_args(argv)
    try:
        image, program = read_exact_program(args.archive, args.archive_sha256,
            args.disk_member, args.disk_sha256, args.program)
    except StxAnalysisError as error:
        parser.error(str(error))
    image_hash, program_hash = hashlib.sha256(image).hexdigest(), hashlib.sha256(program).hexdigest()
    if program_hash != prg.require_sha256(args.program_sha256, "PRG SHA-256"):
        parser.error("exact STX PRG SHA-256 mismatch")
    if len(program) < prg.PRG_HEADER_BYTES or prg.read_be16(program, 0) != 0x601a:
        parser.error("unsupported Atari ST PRG header")
    text, data, bss, symbols = (prg.read_be32(program, offset) for offset in (2, 6, 10, 14))
    loadable = text + data
    if prg.PRG_HEADER_BYTES + loadable + symbols > len(program):
        parser.error("Atari ST PRG segments lie outside its exact file")
    try:
        relocations = parse_relocations(program, text, data, symbols)
    except (StxAnalysisError, ValueError) as error:
        parser.error(str(error))
    report = [
        "# Hash-locked Millennium Atari STX PRG linear candidate disassembly", "",
        f"- Source: `{args.archive.name}!{args.disk_member}:{args.program}`",
        f"- STX SHA-256: `{image_hash}`", f"- PRG SHA-256: `{program_hash}`",
        "- FAT policy: both original FAT copies resolve the same complete cluster chain and byte stream.",
        f"- PRG header: TEXT `0x{text:x}`, DATA `0x{data:x}`, BSS `0x{bss:x}`, symbols `0x{symbols:x}`",
        "- Relocations: " + (", ".join(
            f"image+`0x{offset:x}` contains `0x{value:x}`" for offset, value in relocations)
            if relocations else "none"),
        "- Address space: PRG image-relative TEXT+DATA offsets; relocation entries are retained but not applied, and the GEMDOS load base and reachability are unknown.",
        "- Scope: full byte-complete linear candidate decode; code/data classification is unproven.", "", "```asm",
        *prg.linear_listing(program[prg.PRG_HEADER_BYTES:prg.PRG_HEADER_BYTES + loadable]), "```", "",
    ]
    rendered = "\n".join(report)
    if args.output:
        try:
            prg.require_external_output(args.output).write_text(rendered, encoding="utf-8")
        except (prg.AnalysisError, OSError) as error:
            parser.error(str(error))
    else:
        print(rendered)
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
