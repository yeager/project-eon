#!/usr/bin/env python3
"""Render hash-locked MILL22A.INF bytes as an image-relative M68000 listing.

This preservation tool reads one exact nested Atari ST archive member in
memory.  It neither extracts nor executes the file.  Its offsets are file
relative: the competing observed Fread/load candidates are deliberately not
chosen as a runtime address.
"""

from __future__ import annotations

import argparse
import hashlib
from io import BytesIO
from pathlib import Path
from zipfile import ZipFile

from analyze_atari_st_prg import fat12_member, require_sha256, sha256_file


def require_external_output(path: Path) -> Path:
    """Reject report destinations that could commit or use system scratch."""
    # Preserve the contractual ``/tmp`` spelling before macOS resolves it as
    # ``/private/tmp``.  On Windows, a POSIX-looking ``/tmp`` route is also
    # drive-rooted only after parsing, so retain the lexical check there too.
    normalized = path.as_posix().replace("\\", "/")
    parts = path.parts
    if (normalized == "/tmp" or normalized.startswith("/tmp/")
            or normalized == "/private/tmp" or normalized.startswith("/private/tmp/")
            or bool(path.anchor and len(parts) > 1
                    and parts[1].casefold() == "tmp")):
        raise SystemExit("output must be outside /tmp")
    if not path.is_absolute():
        raise SystemExit("output must be absolute")
    if path.exists() or path.is_symlink():
        raise SystemExit("output must not already exist or be a symlink")
    try:
        parent = path.parent.resolve(strict=True)
    except OSError as error:
        raise SystemExit(f"output parent cannot be resolved: {error}") from error
    resolved = parent / path.name
    normalized_resolved = resolved.as_posix().replace("\\", "/")
    if (normalized_resolved == "/tmp" or normalized_resolved.startswith("/tmp/")
            or normalized_resolved == "/private/tmp"
            or normalized_resolved.startswith("/private/tmp/")):
        raise SystemExit("output must be outside /tmp")
    repository = Path(__file__).resolve().parents[1]
    if resolved == repository or repository in resolved.parents:
        raise SystemExit("output must be outside the repository")
    if not parent.is_dir():
        raise SystemExit("output parent directory must already exist")
    return resolved


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
        else:
            lines.append(f"+0x{item.address:05x}  {item.mnemonic:<10} {item.op_str}".rstrip())
            offset += item.size
    return lines


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--archive", type=Path, required=True)
    parser.add_argument("--archive-sha256", required=True)
    parser.add_argument("--nested-member")
    parser.add_argument("--nested-sha256")
    parser.add_argument("--disk-member", required=True)
    parser.add_argument("--disk-sha256", required=True)
    parser.add_argument("--file-sha256", required=True)
    parser.add_argument("--output", type=Path, required=True)
    args = parser.parse_args()
    output = require_external_output(args.output)
    try:
        require_sha256(args.archive_sha256, "outer archive SHA-256")
        if (args.nested_member is None) != (args.nested_sha256 is None):
            raise ValueError("--nested-member and --nested-sha256 must be supplied together")
        if args.nested_sha256: require_sha256(args.nested_sha256, "nested archive SHA-256")
        require_sha256(args.disk_sha256, "disk SHA-256")
        require_sha256(args.file_sha256, "MILL22A.INF SHA-256")
        if not args.archive.is_file() or args.archive.is_symlink():
            raise ValueError("outer archive is not an existing regular non-symlink file")
        if sha256_file(args.archive) != args.archive_sha256:
            raise ValueError("outer archive SHA-256 mismatch")
        with ZipFile(args.archive) as outer:
            if args.nested_member is None: disk = outer.read(args.disk_member)
            else:
                nested = outer.read(args.nested_member)
                if hashlib.sha256(nested).hexdigest() != args.nested_sha256: raise ValueError("nested archive SHA-256 mismatch")
                with ZipFile(BytesIO(nested)) as inner: disk = inner.read(args.disk_member)
    except (KeyError, OSError, ValueError) as error:
        raise SystemExit(f"unable to read exact nested Atari ST disk: {error}") from error
    config = fat12_member(disk, "MILL22A.INF")
    disk_hash = hashlib.sha256(disk).hexdigest()
    config_hash = hashlib.sha256(config).hexdigest()
    if disk_hash != args.disk_sha256 or config_hash != args.file_sha256:
        raise SystemExit("exact Atari ST disk or MILL22A.INF SHA-256 mismatch")
    report = [
        "# Hash-locked Millennium Atari ST MILL22A.INF linear candidate disassembly", "",
        f"- Source: `{args.archive.name}!" + (f"{args.nested_member}!" if args.nested_member else "") + f"{args.disk_member}:MILL22A.INF`",
        f"- Outer archive SHA-256: `{args.archive_sha256}`",
        *([f"- Nested archive SHA-256: `{args.nested_sha256}`"] if args.nested_sha256 else []),
        f"- Disk SHA-256: `{disk_hash}`",
        f"- File SHA-256: `{config_hash}`",
        f"- File bytes: `{len(config)}`",
        "- Address space: file-image-relative, unrelocated; this is not a GEMDOS/Fread runtime load address.",
        "- Scope: full byte-complete linear candidate decode; code/data, entry, reachability, load address and ABI results are unclassified.",
        "- Coverage: every source byte is rendered as an instruction or explicit `.byte`.", "", "```asm",
        *linear_listing(config), "```", "",
    ]
    output.write_text("\n".join(report), encoding="utf-8")


if __name__ == "__main__":
    main()
