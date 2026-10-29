#!/usr/bin/env python3
"""Disassemble hash-locked Kickstart 1.3 graphics.library MoveSprite spans.

The ROM is read directly from its supplied ZIP archive and stays in memory.
The raw listing is written only to a new file under the external Project Eon
analysis cache; it must never be placed in the repository or media directory.
"""

from __future__ import annotations

import argparse
from io import BytesIO
import hashlib
import os
from pathlib import Path
import stat
import sys
from zipfile import ZipFile


ARCHIVE_SHA256 = "c9521c114900633c09317ca6ff979db7b9df34d3cb537de062f5d51811c42c04"
ROM_MEMBER = (
    "Kickstart v1.3 r34.005 (1987-12)(Commodore)"
    "(A500-A1000-A2000-CDTV)[!].rom"
)
ROM_SIZE = 262_144
ROM_SHA256 = "ee05862d8102a08436ac4056da7d549db31625c7d47b24dfb7b3c9a5c113ca53"
ROM_BASE = 0xFC0000
MAX_ARCHIVE_SIZE = 16 * 1024 * 1024

# End addresses are exclusive. Each span is bounded at a hash-identified RTS.
SPANS = (
    ("graphics_library_movesprite_vector_stub", 0xFC9B24, 0xFC9B38),
    ("graphics_library_movesprite_body", 0xFCED90, 0xFCEEE0),
    ("movesprite_viewport_sprite_helper", 0xFCECAA, 0xFCED5E),
    ("movesprite_register_slot_helper", 0xFCEBFA, 0xFCEC3A),
    ("movesprite_sprite_flag_helper", 0xFCEB3C, 0xFCEB80),
    ("movesprite_sprite_position_helper", 0xFCEC3A, 0xFCEC72),
)


class AnalysisError(ValueError):
    """Raised when input identity or output bounds do not match the contract."""


def sha256(data: bytes) -> str:
    return hashlib.sha256(data).hexdigest()


def read_verified_rom(archive_path: Path) -> bytes:
    try:
        before = archive_path.lstat()
        if stat.S_ISLNK(before.st_mode) or not stat.S_ISREG(before.st_mode):
            raise AnalysisError("ROM archive must be a regular non-symlink file")
        if before.st_size <= 0 or before.st_size > MAX_ARCHIVE_SIZE:
            raise AnalysisError("ROM archive size is outside the bounded read limit")
        descriptor = os.open(archive_path, os.O_RDONLY | getattr(os, "O_NOFOLLOW", 0))
        with os.fdopen(descriptor, "rb") as source:
            opened = os.fstat(source.fileno())
            if (not stat.S_ISREG(opened.st_mode)
                    or (opened.st_dev, opened.st_ino) != (before.st_dev, before.st_ino)):
                raise AnalysisError("ROM archive changed before its bounded read")
            archive_bytes = source.read(MAX_ARCHIVE_SIZE + 1)
        after = archive_path.lstat()
    except OSError as error:
        raise AnalysisError("ROM archive could not be read safely") from error

    if (stat.S_ISLNK(after.st_mode) or not stat.S_ISREG(after.st_mode)
            or (before.st_dev, before.st_ino, before.st_size, before.st_mtime_ns)
            != (after.st_dev, after.st_ino, after.st_size, after.st_mtime_ns)):
        raise AnalysisError("ROM archive changed during its bounded read")
    if len(archive_bytes) != before.st_size or sha256(archive_bytes) != ARCHIVE_SHA256:
        raise AnalysisError("ROM archive size or SHA-256 mismatch")

    try:
        with ZipFile(BytesIO(archive_bytes)) as archive:
            matching = [item for item in archive.infolist() if item.filename == ROM_MEMBER]
            if len(matching) != 1 or matching[0].file_size != ROM_SIZE:
                raise AnalysisError("exact Kickstart ROM member is missing or duplicated")
            rom = archive.read(matching[0])
    except OSError as error:
        raise AnalysisError("ROM archive member could not be read") from error
    if len(rom) != ROM_SIZE or sha256(rom) != ROM_SHA256:
        raise AnalysisError("Kickstart ROM size or SHA-256 mismatch")
    return rom


def resolve_output(path: Path) -> Path:
    if not path.is_absolute() or path.exists() or path.is_symlink():
        raise AnalysisError("output must be a new absolute path")
    normalized = Path(os.path.normpath(str(path)))
    if (normalized == Path("/tmp") or Path("/tmp") in normalized.parents
            or normalized == Path("/private/tmp")
            or Path("/private/tmp") in normalized.parents):
        raise AnalysisError("output must not use /tmp")
    cache_root = (Path.home() / ".cache" / "project-eon-analysis").resolve(strict=True)
    parent = path.parent.resolve(strict=True)
    if parent != cache_root and cache_root not in parent.parents:
        raise AnalysisError("output must stay under the external Project Eon analysis cache")
    return parent / path.name


def disassemble_report(rom: bytes) -> bytes:
    try:
        from capstone import CS_ARCH_M68K, CS_MODE_BIG_ENDIAN, Cs
        from capstone.m68k import M68K_INS_RTS
    except ImportError as error:
        raise AnalysisError("Capstone with M68K support is required") from error

    decoder = Cs(CS_ARCH_M68K, CS_MODE_BIG_ENDIAN)
    lines = [
        f"ROM_SHA256 {ROM_SHA256}",
        f"ROM_BASE 0x{ROM_BASE:06x}",
        f"ROM_MEMBER {ROM_MEMBER}",
        "ADDRESS_RANGES end-exclusive",
        "",
    ]
    for name, start, end in SPANS:
        file_offset = start - ROM_BASE
        span = rom[file_offset:end - ROM_BASE]
        if len(span) != end - start:
            raise AnalysisError(f"ROM span is out of bounds: {name}")
        instructions = list(decoder.disasm(span, start))
        decoded_size = sum(instruction.size for instruction in instructions)
        if decoded_size != len(span) or not instructions or instructions[-1].id != M68K_INS_RTS:
            raise AnalysisError(f"ROM span does not decode through its final RTS: {name}")
        lines.extend((
            f"FUNCTION {name}",
            f"RUNTIME 0x{start:06x}..0x{end:06x}",
            f"FILE_OFFSET 0x{file_offset:06x}..0x{file_offset + len(span):06x}",
            f"SIZE {len(span)}",
            f"SHA256 {sha256(span)}",
        ))
        for instruction in instructions:
            lines.append(
                f"{instruction.address:06x}: {instruction.bytes.hex():<12} "
                f"{instruction.mnemonic} {instruction.op_str}".rstrip()
            )
        lines.append("")
    return ("\n".join(lines) + "\n").encode("utf-8")


def main(argv: list[str] | None = None) -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--rom-archive", type=Path, required=True,
                        help="existing supplied ZIP archive, read in place")
    parser.add_argument("--output", type=Path, required=True,
                        help="new absolute report path under ~/.cache/project-eon-analysis")
    arguments = parser.parse_args(argv)
    try:
        output_path = resolve_output(arguments.output)
        report = disassemble_report(read_verified_rom(arguments.rom_archive))
        descriptor = os.open(output_path, os.O_WRONLY | os.O_CREAT | os.O_EXCL, 0o600)
        with os.fdopen(descriptor, "wb") as destination:
            destination.write(report)
        print(f"rom_sha256={ROM_SHA256}")
        print(f"report={output_path} bytes={len(report)} sha256={sha256(report)} mode=0600")
    except (AnalysisError, OSError) as error:
        print(f"analyze_deuteros_movesprite_rom: {error}", file=sys.stderr)
        return 2
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
