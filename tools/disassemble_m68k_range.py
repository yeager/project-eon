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
from zipfile import ZipFile


def integer(value: str) -> int:
    return int(value, 0)


def read_exact_member(archive_path: Path, nested_member: str, member: str) -> bytes:
    """Read one explicitly named disk member; no archive search/fallback occurs."""
    try:
        with ZipFile(archive_path) as outer:
            nested = outer.read(nested_member)
        with ZipFile(BytesIO(nested)) as inner:
            return inner.read(member)
    except (KeyError, OSError, ValueError) as error:
        raise ValueError(f"Unable to read exact nested archive member: {error}") from error


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
    parser.add_argument("--nested-member", required=True)
    parser.add_argument("--member", required=True)
    parser.add_argument("--offset", type=integer, required=True, help="Disk byte offset, e.g. 0x4ec00")
    parser.add_argument("--length", type=integer, required=True, help="Exact byte length")
    parser.add_argument("--address", type=integer, required=True, help="Proven runtime load address")
    parser.add_argument("--sha256", required=True, help="Expected lower-case SHA-256 for this exact range")
    parser.add_argument("--output", type=Path)
    args = parser.parse_args()

    disk = read_exact_member(args.archive, args.nested_member, args.member)
    if args.offset < 0 or args.length <= 0 or args.offset > len(disk) - args.length:
        raise SystemExit("Requested range is outside the exact source disk")
    source = disk[args.offset:args.offset + args.length]
    digest = hashlib.sha256(source).hexdigest()
    if digest != args.sha256:
        raise SystemExit(f"Range SHA-256 mismatch: expected {args.sha256}, got {digest}")
    lines = [
        "# Hash-locked M68000 linear candidate disassembly",
        "",
        f"- Source: `{args.archive.name}!{args.nested_member}!{args.member}`",
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
        args.output.write_text(output, encoding="utf-8")
    else:
        print(output, end="")


if __name__ == "__main__":
    main()
