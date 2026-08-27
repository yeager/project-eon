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
    loader_length = int.from_bytes(data[0x34:0x38], "big")
    loader_destination = int.from_bytes(data[0x3c:0x40], "big")
    loader_track = int.from_bytes(data[0x44:0x48], "big")
    track_size = int.from_bytes(data[0x4a:0x4c], "big")
    loader_entry = int.from_bytes(data[0x84:0x88], "big")
    loader_disk_offset = loader_track * track_size
    table_offset = loader_disk_offset + 0x12A36 - loader_destination
    profile_address = int.from_bytes(data[table_offset : table_offset + 4], "big")
    profile_offset = loader_disk_offset + profile_address - loader_destination
    destination = int.from_bytes(data[profile_offset + 2 : profile_offset + 6], "big")
    length = int.from_bytes(data[profile_offset + 8 : profile_offset + 12], "big")
    track = int.from_bytes(data[profile_offset + 14 : profile_offset + 18], "big")
    stage_offset = track * track_size
    stage_entry = int.from_bytes(data[stage_offset + 2 : stage_offset + 6], "big")
    stage_entry_offset = stage_offset + stage_entry - destination
    title_profile_address = int.from_bytes(data[table_offset + 4 : table_offset + 8], "big")
    title_profile_offset = loader_disk_offset + title_profile_address - loader_destination
    title_destination = int.from_bytes(data[title_profile_offset + 2 : title_profile_offset + 6], "big")
    title_length = int.from_bytes(data[title_profile_offset + 8 : title_profile_offset + 12], "big")
    title_track = int.from_bytes(data[title_profile_offset + 14 : title_profile_offset + 18], "big")
    title_offset = title_track * track_size
    title_entry = int.from_bytes(data[title_offset + 2 : title_offset + 6], "big")
    title_entry_offset = title_offset + title_entry - title_destination
    lines = [
        "# Generated Deuteros Amiga boot disassembly",
        "",
        f"- Source: `{args.adf.name}`",
        f"- Disk identifier: `{identifier!r}`",
        f"- Root/custom block: `{root}` (`0x{root * 512:x}`)",
        f"- Bootstrap track load: disk `0x{loader_disk_offset:x}` → memory `0x{loader_destination:x}`, length `0x{loader_length:x}`",
        f"- Bootstrap entry: `0x{loader_entry:x}`",
        f"- Main stage load: disk `0x{stage_offset:x}` → memory `0x{destination:x}`, length `0x{length:x}`",
        f"- Main entry: `0x{stage_entry:x}` (disk `0x{stage_entry_offset:x}`)",
        f"- Title handoff stage: disk `0x{title_offset:x}` → memory `0x{title_destination:x}`, length `0x{title_length:x}`",
        f"- Title entry: `0x{title_entry:x}` (disk `0x{title_entry_offset:x}`)",
        "",
        "## Boot block",
        "",
        "```asm",
    ]
    for instruction in decoder.disasm(data[12 : 12 + args.bytes], 12):
        lines.append(f"{instruction.address:08x}  {instruction.mnemonic:<10} {instruction.op_str}".rstrip())
    lines.extend(["```", "", "## Bootstrap entry", "", "```asm"])
    loader_entry_offset = loader_disk_offset + loader_entry - loader_destination
    for instruction in decoder.disasm(data[loader_entry_offset : loader_entry_offset + 192], loader_entry):
        lines.append(f"{instruction.address:08x}  {instruction.mnemonic:<10} {instruction.op_str}".rstrip())
    lines.extend(["```", "", "## Main entry", "", "```asm"])
    # End on the instruction boundary immediately before the next routine's
    # absolute LEA; Capstone otherwise substitutes its invalid-input sentinel
    # for a deliberately truncated operand.
    for instruction in decoder.disasm(data[stage_entry_offset : stage_entry_offset + 0x1FE], stage_entry):
        lines.append(f"{instruction.address:08x}  {instruction.mnemonic:<10} {instruction.op_str}".rstrip())
    lines.extend(["```", "", "## Title handoff entry", "", "```asm"])
    # Use a fresh decoder after the intentionally truncated main-entry view.
    # Capstone's M68K binding otherwise can retain an invalid-input sentinel
    # while decoding the next independent byte range.
    title_decoder = Cs(CS_ARCH_M68K, CS_MODE_BIG_ENDIAN | CS_MODE_M68K_000)
    # 196 bytes ends immediately after the complete six-byte move at $404e4.
    # Do not terminate in the following JSR operand: Capstone then renders a
    # misleading sentinel address for that truncated final instruction.
    for instruction in title_decoder.disasm(data[title_entry_offset : title_entry_offset + 196], title_entry):
        lines.append(f"{instruction.address:08x}  {instruction.mnemonic:<10} {instruction.op_str}".rstrip())
    lines.extend(["```", ""])
    report = "\n".join(lines)
    if args.output:
        args.output.write_text(report, encoding="utf-8")
    else:
        print(report)


if __name__ == "__main__":
    main()
