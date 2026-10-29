#!/usr/bin/env python3
"""Find aligned big-endian longwords in a range in exact Atari STX files.

The scanner reads one hash-pinned physical STX ZIP member in memory and
resolves requested files only through complete, identical FAT12 chains. It
reports file hashes and offsets, never extracts or writes original media.
Results are direct-literal search leads, not reachability evidence.
"""

from __future__ import annotations

import argparse
import hashlib
from pathlib import Path
import sys

ROOT = Path(__file__).resolve().parents[1]
if str(ROOT) not in sys.path:
    sys.path.insert(0, str(ROOT))

from tools.analyze_atari_st_stx_prg import StxAnalysisError, read_exact_program


def aligned_longword_offsets(data: bytes, start: int, end: int) -> tuple[tuple[int, int], ...]:
    """Return even-offset big-endian longwords whose value is in [start,end)."""
    if start < 0 or end > 0x1_0000_0000 or start >= end:
        raise ValueError("address range must be a nonempty 32-bit half-open interval")
    hits = []
    for offset in range(0, max(0, len(data) - 3), 2):
        value = int.from_bytes(data[offset:offset + 4], "big")
        if start <= value < end:
            hits.append((offset, value))
    return tuple(hits)


def file_spec(value: str) -> tuple[str, str]:
    name, separator, digest = value.partition("=")
    if not separator or not name or len(digest) != 64:
        raise argparse.ArgumentTypeError("file must be NAME=SHA256")
    try:
        int(digest, 16)
    except ValueError as error:
        raise argparse.ArgumentTypeError("file SHA256 must be hexadecimal") from error
    return name, digest.lower()


def main(argv: list[str] | None = None) -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("archive", type=Path)
    parser.add_argument("--archive-sha256", required=True)
    parser.add_argument("--disk-member", required=True)
    parser.add_argument("--disk-sha256", required=True)
    parser.add_argument("--range-start", required=True, type=lambda value: int(value, 0))
    parser.add_argument("--range-end", required=True, type=lambda value: int(value, 0))
    parser.add_argument("--file", action="append", required=True, type=file_spec,
        metavar="NAME=SHA256", help="exact FAT12 file to scan; may be repeated")
    args = parser.parse_args(argv)
    try:
        for name, expected_sha in args.file:
            image, payload = read_exact_program(args.archive, args.archive_sha256,
                args.disk_member, args.disk_sha256, name)
            actual_sha = hashlib.sha256(payload).hexdigest()
            if actual_sha != expected_sha:
                raise StxAnalysisError(f"{name} file SHA-256 mismatch")
            hits = aligned_longword_offsets(payload, args.range_start, args.range_end)
            print(f"{name}\t{len(payload)}\t{actual_sha}\t{len(hits)}\t"
                + ",".join(f"0x{offset:x}=0x{value:x}" for offset, value in hits))
    except (OSError, StxAnalysisError, ValueError) as error:
        parser.error(str(error))
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
