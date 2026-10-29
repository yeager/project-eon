#!/usr/bin/env python3
"""Read-only locator for user-supplied game media.

This diagnostic streams ZIP members in memory. It never extracts or writes
original media. ADF members receive bounded content-marker scans; Atari ST
members receive a bounded FAT12 root and per-file mirror-chain scan. Atari
STX members receive bounded physical-sector and FAT12-root scans; MSA members
are reported by type and hash only. Results are search leads, not execution
evidence.
"""

from __future__ import annotations

import argparse
import hashlib
import json
import pathlib
import sys
import zipfile


TEXT_MARKERS = (
    b"LOAD", b"SAVE", b"DISK", b"SYSTEM", b"PLANET", b"COLONY",
    b"SHIP", b"PRESS", b"INSERT", b"CONTINUE", b"GAME OVER",
    b"NEW GAME", b"START",
)
GERMAN_TEXT_MARKERS = (
    b"Forschungsleiter.", b"Wir haben einige Vorentw",
    b"Operations-Schiff (IOS)", b"WIR MACHEN DEN REST ...",
)
KNOWN_DEUTEROS_DATA_SHA256 = (
    "99909db1e190be02e049084743af44f00e331be6bf2d97b4831ada5fe4c30b4a"
)
MAX_ADF_MEMBER_SIZE = 4 * 1024 * 1024
MAX_ATARI_ROOT_FILES = 512
DISK_MEMBER_SUFFIXES = {".adf", ".stx", ".st", ".msa"}


def hash_member(archive: zipfile.ZipFile, info: zipfile.ZipInfo) -> str:
    if info.file_size <= 0 or info.file_size > MAX_ADF_MEMBER_SIZE:
        raise ValueError(f"disk member size is outside locator bounds: {info.file_size}")
    digest = hashlib.sha256()
    size = 0
    with archive.open(info, "r") as member:
        while chunk := member.read(1 << 20):
            digest.update(chunk)
            size += len(chunk)
    if size != info.file_size:
        raise ValueError("disk member size changed while reading archive")
    return digest.hexdigest()


def inspect_member(archive: zipfile.ZipFile, info: zipfile.ZipInfo) -> dict[str, object]:
    if info.file_size <= 0 or info.file_size > MAX_ADF_MEMBER_SIZE:
        raise ValueError(f"ADF member size is outside locator bounds: {info.file_size}")
    digest = hashlib.sha256()
    size = 0
    zero_bytes = [0] * ((info.file_size + 511) // 512)
    marker_offsets: list[int] = []
    german_offsets: dict[bytes, set[int]] = {marker: set() for marker in GERMAN_TEXT_MARKERS}
    tail = b""
    offset = 0
    with archive.open(info, "r") as member:
        while chunk := member.read(1 << 20):
            digest.update(chunk)
            size += len(chunk)
            data = tail + chunk
            search_from = 0
            while True:
                found = data.find(b"DEUTEROSDATA", search_from)
                if found < 0:
                    break
                marker_offsets.append(offset - len(tail) + found)
                search_from = found + 1
            for marker in GERMAN_TEXT_MARKERS:
                search_from = 0
                while True:
                    found = data.find(marker, search_from)
                    if found < 0:
                        break
                    absolute = offset - len(tail) + found
                    if absolute + len(marker) > offset:
                        german_offsets[marker].add(absolute)
                    search_from = found + 1
            tail = data[-11:]
            for index, value in enumerate(chunk):
                zero_bytes[(offset + index) // 512] += value == 0
            offset += len(chunk)

    sha = digest.hexdigest()
    nonempty_sectors = [index for index, zeros in enumerate(zero_bytes) if zeros < 512]
    return {
        "name": info.filename,
        "size": size,
        "sha256": sha,
        "deuteros_data_disk": sha == KNOWN_DEUTEROS_DATA_SHA256,
        "nonempty_sector_count": len(nonempty_sectors),
        "first_nonempty_sectors": nonempty_sectors[:32],
        "deuterosdata_marker_offsets": marker_offsets,
        "german_briefing_marker_offsets": {
            marker.decode("ascii"): sorted(offsets)
            for marker, offsets in german_offsets.items() if offsets
        },
    }


def _u16(data: bytes, offset: int) -> int:
    return int.from_bytes(data[offset : offset + 2], "little")


def _fat12_entry(fat: bytes, cluster: int) -> int:
    offset = cluster + cluster // 2
    if offset + 1 >= len(fat):
        raise ValueError("Atari ST FAT12 entry is outside FAT")
    pair = fat[offset] | (fat[offset + 1] << 8)
    return (pair >> 4) & 0xFFF if cluster & 1 else pair & 0xFFF


def _inspect_atari_st_image(image: bytes) -> dict[str, object]:
    """Return only hash-addressed root-file metadata from a bounded FAT12 .st."""
    # Atari ST boot sectors use a different checksum convention from the
    # PC 0x55AA signature. Geometry and bounded FAT/root checks below are the
    # admission criteria; this locator does not infer bootability.
    if len(image) < 512:
        return {"fat12_root_inspected": False}
    bps = _u16(image, 11)
    spc = image[13]
    reserved = _u16(image, 14)
    fat_count = image[16]
    root_entries = _u16(image, 17)
    total_sectors = _u16(image, 19) or int.from_bytes(image[32:36], "little")
    sectors_per_fat = _u16(image, 22)
    sectors_per_track = _u16(image, 24)
    heads = _u16(image, 26)
    if (bps != 512 or spc == 0 or spc & (spc - 1) or reserved == 0
            or fat_count != 2 or root_entries == 0 or sectors_per_fat == 0
            or sectors_per_track == 0 or heads == 0 or total_sectors == 0
            or total_sectors * bps > len(image)):
        return {"fat12_root_inspected": False}
    root_start = reserved + fat_count * sectors_per_fat
    root_sectors = (root_entries * 32 + bps - 1) // bps
    data_start = root_start + root_sectors
    if data_start >= total_sectors:
        return {"fat12_root_inspected": False}
    fat_start = reserved * bps
    fat_size = sectors_per_fat * bps
    fats = (image[fat_start : fat_start + fat_size],
            image[fat_start + fat_size : fat_start + 2 * fat_size])
    root = image[root_start * bps : data_start * bps]
    if any(len(fat) != fat_size for fat in fats) or len(root) != root_sectors * bps:
        return {"fat12_root_inspected": False}

    files: list[dict[str, object]] = []
    for index in range(min(root_entries, MAX_ATARI_ROOT_FILES)):
        record = root[index * 32 : (index + 1) * 32]
        first = record[0]
        if first == 0:
            break
        if first == 0xE5 or record[11] & 0x18 or record[11] == 0x0F:
            continue
        try:
            stem = record[:8].decode("ascii").rstrip()
            suffix = record[8:11].decode("ascii").rstrip()
        except UnicodeDecodeError:
            continue
        if not stem or any(ord(char) < 0x20 for char in stem + suffix):
            continue
        name = stem + ("." + suffix if suffix else "")
        size = int.from_bytes(record[28:32], "little")
        cluster = _u16(record, 26)
        if size == 0:
            files.append({"name": name, "size": 0, "sha256": hashlib.sha256(b"").hexdigest()})
            continue
        cluster_bytes = bps * spc
        needed = (size + cluster_bytes - 1) // cluster_bytes
        cluster_limit = (total_sectors - data_start) // spc + 2
        chain = [cluster]
        valid = 2 <= cluster < cluster_limit and needed <= cluster_limit - 2
        for _ in range(needed - 1):
            if not valid:
                break
            next_values = [_fat12_entry(fat, chain[-1]) for fat in fats]
            if next_values[0] != next_values[1] or not (2 <= next_values[0] < cluster_limit):
                valid = False
                break
            chain.append(next_values[0])
        if valid:
            terminal_values = [_fat12_entry(fat, chain[-1]) for fat in fats]
            valid = terminal_values[0] == terminal_values[1] and terminal_values[0] >= 0xFF8
        if not valid or len(chain) != needed:
            files.append({"name": name, "size": size, "chain_consensus": False})
            continue
        payload = bytearray()
        for current in chain:
            lba = data_start + (current - 2) * spc
            end = lba + spc
            if end > total_sectors:
                valid = False
                break
            payload.extend(image[lba * bps : end * bps])
        if not valid or len(payload) < size:
            files.append({"name": name, "size": size, "chain_consensus": False})
        else:
            files.append({
                "name": name,
                "size": size,
                "sha256": hashlib.sha256(payload[:size]).hexdigest(),
                "chain_consensus": True,
            })
    return {
        "fat12_root_inspected": True,
        "bpb_geometry": {"bytes_per_sector": bps, "sectors_per_cluster": spc,
                         "total_sectors": total_sectors, "sectors_per_track": sectors_per_track,
                         "heads": heads},
        "fat_mirrors_identical": fats[0] == fats[1],
        "root_files": files,
    }


def inspect_atari_member(archive: zipfile.ZipFile, info: zipfile.ZipInfo) -> dict[str, object]:
    if info.file_size <= 0 or info.file_size > MAX_ADF_MEMBER_SIZE:
        raise ValueError(f"Atari member size is outside locator bounds: {info.file_size}")
    digest = hashlib.sha256()
    image = bytearray()
    with archive.open(info, "r") as member:
        while chunk := member.read(1 << 20):
            digest.update(chunk)
            image.extend(chunk)
    if len(image) != info.file_size:
        raise ValueError("Atari member size changed while reading archive")
    result: dict[str, object] = {
        "name": info.filename,
        "size": len(image),
        "sha256": digest.hexdigest(),
        "media_kind": "atari_st",
        "content_inspected": True,
        "evidence_scope": "root-file hashes only; no execution inference",
    }
    result.update(_inspect_atari_st_image(bytes(image)))
    return result


def _stx_logical_image(image: bytes) -> tuple[bytes, int]:
    """Reconstruct a bounded FAT12 logical view from an STX track container."""
    if len(image) < 16 or image[:4] != b"RSY\0" or int.from_bytes(image[4:6], "little") != 3:
        raise ValueError("unsupported STX version or header")
    expected_tracks = image[10]
    sectors: dict[tuple[int, int, int], bytes] = {}
    offset = 16
    tracks = 0
    while offset < len(image):
        if len(image) - offset < 16:
            raise ValueError("truncated STX track header")
        block_size = int.from_bytes(image[offset:offset + 4], "little")
        fuzzy_size = int.from_bytes(image[offset + 4:offset + 8], "little")
        count = int.from_bytes(image[offset + 8:offset + 10], "little")
        if block_size < 16 or block_size > len(image) - offset:
            raise ValueError("invalid STX track extent")
        descriptor_end = offset + 16 + count * 16
        if descriptor_end > offset + block_size or fuzzy_size > offset + block_size - descriptor_end:
            raise ValueError("STX sector descriptors exceed track extent")
        payload_base = descriptor_end + fuzzy_size
        track_number = image[offset + 14]
        for index in range(count):
            descriptor = offset + 16 + index * 16
            relative = int.from_bytes(image[descriptor:descriptor + 4], "little")
            track, side, sector_id, size_code = image[descriptor + 8:descriptor + 12]
            if size_code > 7 or track != (track_number & 0x7F) or side != (track_number >> 7):
                raise ValueError("unsupported STX sector identity")
            length = 128 << size_code
            payload = payload_base + relative
            if payload > offset + block_size or length > offset + block_size - payload:
                raise ValueError("STX sector payload exceeds track extent")
            key = (track, side, sector_id)
            if key in sectors:
                raise ValueError("duplicate STX sector identity")
            sectors[key] = image[payload:payload + length]
        offset += block_size
        tracks += 1
    if offset != len(image) or tracks != expected_tracks:
        raise ValueError("STX track count differs from header")

    boot = sectors.get((0, 0, 1))
    if boot is None or len(boot) < 512:
        raise ValueError("STX boot-sector identity is absent")
    bps = _u16(boot, 11)
    heads = _u16(boot, 26)
    sectors_per_track = _u16(boot, 24)
    total = _u16(boot, 19) or int.from_bytes(boot[32:36], "little")
    if (bps != 512 or not heads or not sectors_per_track or not total
            or total * bps > MAX_ADF_MEMBER_SIZE):
        raise ValueError("STX FAT12 logical volume is outside locator bounds")
    logical = bytearray(total * bps)
    for lba in range(total):
        cylinder, within = divmod(lba, sectors_per_track * heads)
        side, sector_index = divmod(within, sectors_per_track)
        sector = sectors.get((cylinder, side, sector_index + 1))
        if sector is None or len(sector) != bps:
            raise ValueError("STX logical volume has a missing or wrong-size sector")
        start = lba * bps
        logical[start:start + bps] = sector
    return bytes(logical), tracks


def inspect_atari_stx_member(archive: zipfile.ZipFile, info: zipfile.ZipInfo) -> dict[str, object]:
    if info.file_size <= 0 or info.file_size > MAX_ADF_MEMBER_SIZE:
        raise ValueError(f"STX member size is outside locator bounds: {info.file_size}")
    digest = hashlib.sha256()
    image = bytearray()
    with archive.open(info, "r") as member:
        while chunk := member.read(1 << 20):
            digest.update(chunk)
            image.extend(chunk)
    if len(image) != info.file_size:
        raise ValueError("STX member size changed while reading archive")
    result: dict[str, object] = {
        "name": info.filename,
        "size": len(image),
        "sha256": digest.hexdigest(),
        "media_kind": "atari_st_stx",
        "content_inspected": True,
        "evidence_scope": "FAT12 root-file hashes only; no execution inference",
    }
    logical, track_count = _stx_logical_image(bytes(image))
    result["stx_track_count"] = track_count
    result.update(_inspect_atari_st_image(logical))
    return result


def inspect_archive(path: pathlib.Path) -> dict[str, object]:
    archive_hash = hashlib.sha256()
    with path.open("rb") as source:
        while chunk := source.read(1 << 20):
            archive_hash.update(chunk)
    members: list[dict[str, object]] = []
    with zipfile.ZipFile(path) as archive:
        for info in archive.infolist():
            suffix = pathlib.PurePosixPath(info.filename).suffix.lower()
            if info.is_dir() or suffix not in DISK_MEMBER_SUFFIXES:
                continue
            if suffix == ".st":
                members.append(inspect_atari_member(archive, info))
                continue
            if suffix == ".stx":
                try:
                    members.append(inspect_atari_stx_member(archive, info))
                except ValueError as error:
                    members.append({
                        "name": info.filename,
                        "size": info.file_size,
                        "sha256": hash_member(archive, info),
                        "media_kind": "atari_st_stx",
                        "content_inspected": False,
                        "root_inspection_error": str(error),
                    })
                continue
            if suffix != ".adf":
                members.append({
                    "name": info.filename,
                    "size": info.file_size,
                    "sha256": hash_member(archive, info),
                    "media_kind": {
                        ".st": "atari_st",
                        ".msa": "atari_st_msa",
                    }[suffix],
                    "content_inspected": False,
                })
                continue
            member = inspect_member(archive, info)
            member["media_kind"] = "amiga_adf_candidate"
            member["content_inspected"] = True
            # Search terms are recorded as a coarse lead only. A separate
            # bounded text scan below avoids retaining any disk image.
            with archive.open(info, "r") as source:
                carry = b""
                found: set[bytes] = set()
                while chunk := source.read(1 << 20):
                    upper = carry + chunk.upper()
                    found.update(marker for marker in TEXT_MARKERS if marker in upper)
                    carry = upper[-max(map(len, TEXT_MARKERS)) :]
            member["game_text_markers"] = sorted(marker.decode("ascii") for marker in found)
            members.append(member)
    return {
        "archive": str(path),
        "archive_sha256": archive_hash.hexdigest(),
        "members": members,
        "evidence_scope": "locator only; no runtime or gameplay inference",
    }


def archive_paths(paths: list[pathlib.Path]):
    """Yield candidate archives from files or recursively scanned directories."""
    seen: set[pathlib.Path] = set()
    for path in paths:
        candidates = path.rglob("*") if path.is_dir() else (path,)
        for candidate in candidates:
            if not candidate.is_file() or candidate.suffix.lower() != ".zip":
                continue
            resolved = candidate.resolve()
            if resolved not in seen:
                seen.add(resolved)
                yield candidate


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("paths", nargs="+", type=pathlib.Path)
    args = parser.parse_args()
    try:
        for path in archive_paths(args.paths):
            print(json.dumps(inspect_archive(path), ensure_ascii=False, sort_keys=True))
    except (OSError, ValueError, zipfile.BadZipFile, RuntimeError) as error:
        print(f"locate_real_game_media: {error}", file=sys.stderr)
        return 2
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
