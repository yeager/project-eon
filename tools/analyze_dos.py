#!/usr/bin/env python3
"""Generate DOS reports from read-only files or exact ZIP members."""

from __future__ import annotations

import argparse
import hashlib
from io import BytesIO
from pathlib import Path
import sys
from zipfile import ZipFile

# Allow direct execution from a source checkout without installing the package.
sys.path.insert(0, str(Path(__file__).resolve().parents[1]))
from eon.dos import describe_bytes


def _render_instruction(instruction) -> str:
    return f"{instruction.address:05x}  {instruction.mnemonic:<8} {instruction.op_str}".rstrip()


def disassemble_linear(data: bytes, address: int) -> list[str]:
    """Render a byte-complete, deliberately unclassified x86 listing.

    Capstone stops its iterator at malformed or incomplete input. That is
    useful for normal disassembly, but a preservation report that calls itself
    complete must retain those bytes as explicit unknown data rather than
    silently omitting its tail.
    """
    try:
        from capstone import CS_ARCH_X86, CS_MODE_16, Cs
    except ImportError as error:
        raise SystemExit("Install analysis dependencies: pip install -r requirements-analysis.txt") from error
    decoder = Cs(CS_ARCH_X86, CS_MODE_16)
    lines: list[str] = []
    offset = 0
    while offset < len(data):
        instruction = next(decoder.disasm(data[offset:], address + offset, count=1), None)
        if (instruction is None or instruction.address != address + offset
                or instruction.size <= 0 or instruction.size > len(data) - offset):
            lines.append(f"{address + offset:05x}  .byte    0x{data[offset]:02x} ; code/data-unclassified")
            offset += 1
            continue
        lines.append(_render_instruction(instruction))
        offset += instruction.size
    return lines


def disassemble(data: bytes, address: int, offset: int, count: int = 96,
                *, complete_linear: bool = False) -> list[str]:
    if complete_linear:
        return disassemble_linear(data[offset : offset + count], address)
    try:
        from capstone import CS_ARCH_X86, CS_MODE_16, Cs
    except ImportError as error:
        raise SystemExit("Install analysis dependencies: pip install -r requirements-analysis.txt") from error
    decoder = Cs(CS_ARCH_X86, CS_MODE_16)
    return [_render_instruction(instruction)
            for instruction in decoder.disasm(data[offset : offset + count], address)]


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("directory", type=Path, nargs="?",
                        help="Read-only DOS directory (legacy investigator input)")
    parser.add_argument("--archive", type=Path,
                        help="Original outer ZIP; requires one or more exact --member paths")
    parser.add_argument("--member", action="append", default=[],
                        help="Exact EXE/COM path inside --archive; repeat for every program")
    parser.add_argument("--complete-linear", action="store_true",
                        help="Decode every byte as an explicitly unproven linear x86 candidate")
    parser.add_argument("--output", type=Path)
    args = parser.parse_args()
    archive_mode = args.archive is not None
    directory_mode = args.directory is not None
    if archive_mode == directory_mode:
        raise SystemExit("Specify exactly one directory or --archive")
    if archive_mode and not args.member:
        raise SystemExit("--archive requires one or more exact --member paths")
    if directory_mode:
        sources = [(path.name, path.read_bytes()) for path in sorted(
            [*args.directory.glob("*.EXE"), *args.directory.glob("*.COM")])]
    else:
        try:
            with ZipFile(args.archive) as archive:
                sources = [(member, archive.read(member)) for member in args.member]
        except (KeyError, OSError, ValueError) as error:
            raise SystemExit(f"Unable to read exact DOS archive member: {error}") from error
    lines = ["# Generated Millennium DOS binary report", ""]
    for name, data in sources:
        report = describe_bytes(Path(name).name, data)
        listing_offset = 0 if args.complete_linear else report["entry_file_offset"]
        listing_address = 0x100 if args.complete_linear else report["entry_load_address"]
        listing_count = len(data) if args.complete_linear else 96
        lines.extend([
            f"## `{report['name']}`",
            "",
            f"- Size: {report['size']} bytes",
            f"- SHA-256: `{hashlib.sha256(data).hexdigest()}`",
            f"- Entry file offset: `0x{report['entry_file_offset']:x}`",
            f"- Entry load address: `0x{report['entry_load_address']:x}`",
            f"- Syntactic interrupt occurrences: {report['interrupts']}",
            "- Listing scope: " + ("entire image, linear candidate only (code/data unclassified)"
                if args.complete_linear else "96 bytes from inferred entry candidate"),
            *( ["- Linear coverage: every source byte is rendered as an instruction or explicit `.byte`."]
               if args.complete_linear else [] ),
            "",
            "```asm",
            *disassemble(
                data,
                listing_address,
                listing_offset,
                listing_count,
                complete_linear=args.complete_linear,
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
