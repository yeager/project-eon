#!/usr/bin/env python3
"""Disassemble genuine Amiga 68000 boot code from an ADF image."""

from __future__ import annotations

import argparse
from pathlib import Path
import sys


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("adf", type=Path)
    parser.add_argument("--output", type=Path)
    parser.add_argument("--bytes", type=int, default=144)
    args = parser.parse_args()
    try:
        from capstone import CS_ARCH_M68K, CS_MODE_BIG_ENDIAN, CS_MODE_M68K_000, Cs
    except ImportError as error:
        raise SystemExit("Install analysis dependencies: pip install -r requirements-analysis.txt") from error
    data = args.adf.read_bytes()
    if len(data) != 901_120:
        raise SystemExit("Expected a standard 901120-byte ADF")
    identifier = data[:4]
    root = int.from_bytes(data[8:12], "big")
    decoder = Cs(CS_ARCH_M68K, CS_MODE_BIG_ENDIAN | CS_MODE_M68K_000)
    lines = [
        "# Generated Deuteros Amiga boot disassembly",
        "",
        f"- Source: `{args.adf.name}`",
        f"- Disk identifier: `{identifier!r}`",
        f"- Root/custom block: `{root}` (`0x{root * 512:x}`)",
        "",
        "```asm",
    ]
    for instruction in decoder.disasm(data[12 : 12 + args.bytes], 12):
        lines.append(f"{instruction.address:08x}  {instruction.mnemonic:<10} {instruction.op_str}".rstrip())
    lines.extend(["```", ""])
    report = "\n".join(lines)
    if args.output:
        args.output.write_text(report, encoding="utf-8")
    else:
        print(report)


if __name__ == "__main__":
    main()

