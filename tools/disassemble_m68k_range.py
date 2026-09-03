#!/usr/bin/env python3
"""Disassemble one hash-locked 68000 range from a nested original archive.

This preservation utility keeps the original disk image in memory only.  It
does not unpack, mount, write, or modify game media.  Its output is a linear
candidate listing: callers must still prove code/data classification, control
flow, relocation and operating-system ABI outcomes separately.
"""

from __future__ import annotations

import argparse
import hashlib
from io import BytesIO
from pathlib import Path
import os
from zipfile import ZipFile


def integer(value: str) -> int:
    return int(value, 0)


def sha256_file(path: Path) -> str:
    digest = hashlib.sha256()
    with path.open("rb") as source:
        for chunk in iter(lambda: source.read(1024 * 1024), b""):
            digest.update(chunk)
    return digest.hexdigest()


def read_exact_member(archive_path: Path, archive_sha256: str, nested_member: str | None,
                      nested_sha256: str | None, member: str, member_sha256: str) -> bytes:
    """Read one hash-bound direct or nested disk member in process memory."""
    try:
        if not archive_path.is_file() or archive_path.is_symlink():
            raise ValueError("archive is not a regular file")
        if sha256_file(archive_path) != archive_sha256:
            raise ValueError("outer archive SHA-256 mismatch")
        with ZipFile(archive_path) as outer:
            if nested_member is None:
                disk = outer.read(member)
            else:
                nested = outer.read(nested_member)
                if hashlib.sha256(nested).hexdigest() != nested_sha256:
                    raise ValueError("nested archive SHA-256 mismatch")
                with ZipFile(BytesIO(nested)) as inner:
                    disk = inner.read(member)
        if hashlib.sha256(disk).hexdigest() != member_sha256:
            raise ValueError("disk member SHA-256 mismatch")
        return disk
    except (KeyError, OSError, ValueError) as error:
        raise ValueError(f"Unable to read exact nested archive member: {error}") from error


def require_external_output(path: Path) -> Path:
    """Reports can contain decoded original instructions, never checkout/scratch files."""
    if not path.is_absolute():
        raise ValueError("output path must be absolute")
    lexical = os.path.normpath(str(path))
    if lexical == "/tmp" or lexical.startswith("/tmp/") or lexical == "/private/tmp" or lexical.startswith("/private/tmp/"):
        raise ValueError("output must be outside /tmp")
    if path.exists() or path.is_symlink():
        raise ValueError("output must not already exist or be a symlink")
    try:
        parent = path.parent.resolve(strict=True)
        checkout = Path(__file__).resolve().parents[1]
        path.relative_to(checkout)
    except ValueError:
        pass
    else:
        raise ValueError("output must be outside the repository")
    if not parent.is_dir():
        raise ValueError("output parent must be an existing directory")
    return path


def disassemble(data: bytes, address: int) -> list[str]:
    """Render a byte-complete M68000 candidate listing for an exact range."""
    try:
        from capstone import CS_ARCH_M68K, CS_MODE_BIG_ENDIAN, CS_MODE_M68K_000, Cs
    except ImportError as error:
        raise SystemExit("Install analysis dependencies: pip install -r requirements-analysis.txt") from error
    decoder = Cs(CS_ARCH_M68K, CS_MODE_BIG_ENDIAN | CS_MODE_M68K_000)
    lines: list[str] = []
    offset = 0
    while offset < len(data):
        item = next(decoder.disasm(data[offset:], address + offset, count=1), None)
        if (item is None or item.address != address + offset or item.size <= 0
                or item.size > len(data) - offset):
            lines.append(f"{address + offset:08x}  .byte      0x{data[offset]:02x} ; code/data-unclassified")
            offset += 1
            continue
        lines.append(f"{item.address:08x}  {item.mnemonic:<10} {item.op_str}".rstrip())
        offset += item.size
    return lines


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--archive", type=Path, required=True)
    parser.add_argument("--archive-sha256", required=True)
    parser.add_argument("--nested-member")
    parser.add_argument("--nested-sha256")
    parser.add_argument("--member", required=True)
    parser.add_argument("--member-sha256", required=True)
    parser.add_argument("--offset", type=integer, required=True, help="Disk byte offset, e.g. 0x4ec00")
    parser.add_argument("--length", type=integer, required=True, help="Exact byte length")
    parser.add_argument("--address", type=integer, required=True, help="Proven runtime load address")
    parser.add_argument("--sha256", required=True, help="Expected lower-case SHA-256 for this exact range")
    parser.add_argument("--output", type=Path)
    args = parser.parse_args()

    if bool(args.nested_member) != bool(args.nested_sha256):
        raise SystemExit("--nested-member and --nested-sha256 must be supplied together")
    if args.output:
        try:
            require_external_output(args.output)
        except ValueError as error:
            raise SystemExit(str(error)) from error

    disk = read_exact_member(args.archive, args.archive_sha256, args.nested_member,
                             args.nested_sha256, args.member, args.member_sha256)
    if args.offset < 0 or args.length <= 0 or args.offset > len(disk) - args.length:
        raise SystemExit("Requested range is outside the exact source disk")
    source = disk[args.offset:args.offset + args.length]
    digest = hashlib.sha256(source).hexdigest()
    if digest != args.sha256:
        raise SystemExit(f"Range SHA-256 mismatch: expected {args.sha256}, got {digest}")
    source_description = (f"{args.archive.name}!{args.member}" if args.nested_member is None
                          else f"{args.archive.name}!{args.nested_member}!{args.member}")
    lines = [
        "# Hash-locked M68000 linear candidate disassembly",
        "",
        f"- Source: `{source_description}`",
        f"- Disk SHA-256: `{hashlib.sha256(disk).hexdigest()}`",
        f"- Source range: disk `+0x{args.offset:x}` for `0x{args.length:x}` bytes",
        f"- Runtime address: `0x{args.address:x}`",
        f"- Range SHA-256: `{digest}`",
        "- Scope: full byte-complete linear decode; code/data and reachability are unclassified.",
        "- Coverage: every source byte is rendered as an instruction or explicit `.byte`.",
        "",
        "```asm",
        *disassemble(source, args.address),
        "```",
        "",
    ]
    output = "\n".join(lines)
    if args.output:
        require_external_output(args.output).write_text(output, encoding="utf-8")
    else:
        print(output, end="")


if __name__ == "__main__":
    main()
