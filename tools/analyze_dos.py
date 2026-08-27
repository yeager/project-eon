#!/usr/bin/env python3
"""Generate a Markdown report and entry-point disassembly for DOS game data."""

from __future__ import annotations

import argparse
from pathlib import Path
import sys

# Allow direct execution from a source checkout without installing the package.
sys.path.insert(0, str(Path(__file__).resolve().parents[1]))
from eon.dos import describe


def disassemble(data: bytes, address: int, offset: int, count: int = 96) -> list[str]:
    try:
        from capstone import CS_ARCH_X86, CS_MODE_16, Cs
    except ImportError as error:
        raise SystemExit("Install analysis dependencies: pip install -r requirements-analysis.txt") from error
    decoder = Cs(CS_ARCH_X86, CS_MODE_16)
    return [
        f"{instruction.address:05x}  {instruction.mnemonic:<8} {instruction.op_str}".rstrip()
        for instruction in decoder.disasm(data[offset : offset + count], address)
    ]


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("directory", type=Path, help="Extracted Millennium DOS directory")
    parser.add_argument("--output", type=Path)
    args = parser.parse_args()
    executables = sorted([*args.directory.glob("*.EXE"), *args.directory.glob("*.COM")])
    lines = ["# Generated Millennium DOS binary report", ""]
    for executable in executables:
        report = describe(executable)
        lines.extend([
            f"## `{report['name']}`",
            "",
            f"- Size: {report['size']} bytes",
            f"- Entry file offset: `0x{report['entry_file_offset']:x}`",
            f"- Entry load address: `0x{report['entry_load_address']:x}`",
            f"- Syntactic interrupt occurrences: {report['interrupts']}",
            "",
            "```asm",
            *disassemble(
                executable.read_bytes(),
                report["entry_load_address"],
                report["entry_file_offset"],
            ),
            "```",
            "",
            "Selected strings:",
            "",
        ])
        selected = [item for item in report["strings"] if any(c.isalpha() for c in item["text"])]
        lines.extend(
            f"- `0x{item['offset']:x}`: `{item['text'].replace('`', chr(92) + '`')}`"
            for item in selected[:40]
        )
        lines.append("")
    output = "\n".join(lines)
    if args.output:
        args.output.write_text(output, encoding="utf-8")
    else:
        print(output)


if __name__ == "__main__":
    main()
