#!/usr/bin/env python3
"""Export one hash-pinned MakeVPort ROM span to the external analysis cache.

The supplied ROM archive is verified and read in place.  The requested span is
written once beneath ``~/.cache/project-eon-analysis`` so an external
decompiler can import that bounded region.  It never writes into the checkout
or the supplied archive directory.
"""

from __future__ import annotations

import argparse
import hashlib
import os
from pathlib import Path
import sys

try:
    from .analyze_deuteros_makevport_rom import (
        AnalysisError,
        ROM_BASE,
        SPANS,
        read_verified_rom,
        resolve_output,
    )
except ImportError:  # Direct execution keeps tools/ on sys.path instead.
    from analyze_deuteros_makevport_rom import (
        AnalysisError,
        ROM_BASE,
        SPANS,
        read_verified_rom,
        resolve_output,
    )


def extract_span(rom: bytes, name: str) -> tuple[int, int, str, bytes]:
    """Return the named ROM span only after its declared identity is verified."""
    for span_name, start, end, expected_sha256 in SPANS:
        if span_name != name:
            continue
        begin = start - ROM_BASE
        data = rom[begin:end - ROM_BASE]
        if len(data) != end - start:
            raise AnalysisError("selected ROM span is outside the verified image")
        actual_sha256 = hashlib.sha256(data).hexdigest()
        if actual_sha256 != expected_sha256:
            raise AnalysisError("selected ROM span SHA-256 mismatch")
        return start, end, expected_sha256, data
    raise AnalysisError("selected ROM span is not declared")


def write_new_output(path: Path, data: bytes) -> None:
    descriptor = os.open(path, os.O_WRONLY | os.O_CREAT | os.O_EXCL, 0o600)
    with os.fdopen(descriptor, "wb") as destination:
        destination.write(data)


def main(argv: list[str] | None = None) -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--rom-archive", type=Path, required=True,
                        help="existing supplied Kickstart ZIP, read only")
    parser.add_argument("--span", choices=[item[0] for item in SPANS], required=True,
                        help="exact declared ROM span to export")
    parser.add_argument("--output", type=Path, required=True,
                        help="new absolute output path beneath the analysis cache")
    arguments = parser.parse_args(argv)
    try:
        output = resolve_output(arguments.output)
        start, end, source_sha256, data = extract_span(
            read_verified_rom(arguments.rom_archive), arguments.span)
        write_new_output(output, data)
        print(f"span={arguments.span}")
        print(f"runtime=0x{start:06x}..0x{end:06x}")
        print(f"source_sha256={source_sha256}")
        print(f"output={output} bytes={len(data)} mode=0600")
    except (AnalysisError, OSError) as error:
        print(f"export_deuteros_makevport_rom_span: {error}", file=sys.stderr)
        return 2
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
