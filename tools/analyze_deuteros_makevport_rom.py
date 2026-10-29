#!/usr/bin/env python3
"""Disassemble hash-pinned Kickstart 1.3 MakeVPort spans.

The supplied ROM archive is read in place and its member stays in memory.
Generated instruction listings must be written beneath the external Project
Eon analysis cache, never into the repository or media directory.
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
GRAPHICS_VECTOR_TABLE_OFFSET = 0xB05A
MAKEVPORT_LVO = -0xD8
MAKEVPORT_VECTOR_SLOT = 0xB0E6
MAKEVPORT_VECTOR_STUB = 0xFC63BC
MAKEVPORT_BODY = 0xFCC678

# Runtime end addresses are exclusive. Each span ends at an exact RTS.
SPANS = (
    ("graphics_library_makevport_vector_stub", 0xFC63BC, 0xFC63CC,
     "279ee4741d512e2a9f776aa427c19d86a8ac40d9fde3c0e24a64649b3fb26713"),
    ("graphics_library_makevport_body", 0xFCC678, 0xFCCFF4,
     "a4411cc0b426c8f77509751e07f12e77cde2b98844b344ae769a289d749c01ef"),
    ("makevport_allocate_viewport_buffer", 0xFCC452, 0xFCC4D0,
     "e82f11e8dd66eece16e902874d9c7f5af3253302f1ce313c7cb728a8f10a167a"),
    ("makevport_allocate_copper_node", 0xFCA358, 0xFCA3EE,
     "31c75b5db29ae12d9a29b3b816f790fec90b2df2170e1ad38a8ebf6ea01d6a44"),
    ("makevport_build_viewport_copper", 0xFCCFF4, 0xFCD2A4,
     "79e653e0cdf54ba646ab23eea1703040d8045f472022d08a247030cbfab187c0"),
    ("makevport_geometry_helper", 0xFCC62E, 0xFCC678,
     "de1fe3717f78dd713e1f02edba8af1a9d0cab1835c766f44e345e30146aebc85"),
    ("makevport_copper_instruction_helper", 0xFCC5C0, 0xFCC62E,
     "10fcda1257b0c6ffc30d1bebf9444f59f1ba9a53053c819a1001633e0ffe20e5"),
    ("graphics_allocate_mem_wrapper", 0xFD1470, 0xFD149E,
     "fecb0d19c6f60f10c4c4dfe7bd8a43160bd86ae17032b7932e71a69f7ba9f97b"),
    ("exec_alert_wrapper", 0xFD3BA8, 0xFD3BC4,
     "468e58981834156a271fa6363af1685770189d6874cfe13bee5ce0ce87e30555"),
    ("exec_alloc_mem_wrapper", 0xFD3C34, 0xFD3C4A,
     "f480a438d536b92fcfe8d2d8ace5779bb21db9b341164013178faa65bd9edfd5"),
    ("exec_free_mem_wrapper", 0xFD3C4C, 0xFD3C64,
     "f8bdf96f20b3e97c5b94e25edbce65569c1f486a431332b34e108c9d0ec226b5"),
)


class AnalysisError(ValueError):
    """Raised when media identity, span identity, or output bounds fail."""


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
    vector_entry_index = ((-MAKEVPORT_LVO) // 6) - 1
    vector_target = int.from_bytes(
        rom[MAKEVPORT_VECTOR_SLOT:MAKEVPORT_VECTOR_SLOT + 4], "big")
    wrapper_offset = MAKEVPORT_VECTOR_STUB - ROM_BASE
    wrapper_body_target = int.from_bytes(rom[wrapper_offset + 6:wrapper_offset + 10], "big")
    if (GRAPHICS_VECTOR_TABLE_OFFSET + vector_entry_index * 4 != MAKEVPORT_VECTOR_SLOT
            or vector_target != MAKEVPORT_VECTOR_STUB
            or wrapper_body_target != MAKEVPORT_BODY):
        raise AnalysisError("Kickstart graphics.library MakeVPort vector chain mismatch")
    lines = [
        f"ROM_SHA256 {ROM_SHA256}",
        f"ROM_BASE 0x{ROM_BASE:06x}",
        f"ROM_MEMBER {ROM_MEMBER}",
        f"GRAPHICS_VECTOR_TABLE_FILE_OFFSET 0x{GRAPHICS_VECTOR_TABLE_OFFSET:06x}",
        f"MAKEVPORT_LVO -0x{(-MAKEVPORT_LVO):04x}",
        f"MAKEVPORT_VECTOR_SLOT 0x{MAKEVPORT_VECTOR_SLOT:06x}",
        f"MAKEVPORT_VECTOR_STUB 0x{MAKEVPORT_VECTOR_STUB:06x}",
        f"MAKEVPORT_BODY 0x{MAKEVPORT_BODY:06x}",
        "RANGES end-exclusive",
        "",
    ]
    for name, start, end, expected_sha256 in SPANS:
        file_offset = start - ROM_BASE
        span = rom[file_offset:end - ROM_BASE]
        if len(span) != end - start or sha256(span) != expected_sha256:
            raise AnalysisError(f"ROM span identity mismatch: {name}")
        instructions = list(decoder.disasm(span, start))
        decoded_size = sum(instruction.size for instruction in instructions)
        if (decoded_size != len(span) or not instructions
                or instructions[-1].id != M68K_INS_RTS):
            raise AnalysisError(f"ROM span does not decode through its final RTS: {name}")
        lines.extend((
            f"FUNCTION {name}",
            f"RUNTIME 0x{start:06x}..0x{end:06x}",
            f"FILE_OFFSET 0x{file_offset:06x}..0x{file_offset + len(span):06x}",
            f"SIZE {len(span)}",
            f"SHA256 {expected_sha256}",
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
        print(f"analyze_deuteros_makevport_rom: {error}", file=sys.stderr)
        return 2
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
