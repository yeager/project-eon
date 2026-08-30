#!/usr/bin/env python3
"""Disassemble genuine Amiga 68000 boot code without extracting its ADF."""

from __future__ import annotations

import argparse
from io import BytesIO
from pathlib import Path
import sys
from zipfile import ZipFile


def render_instructions(decoder, data: bytes, address: int, *, complete_linear: bool) -> list[str]:
    """Render a M68000 listing, retaining every byte in complete-linear mode."""
    def render(instruction) -> str:
        return f"{instruction.address:08x}  {instruction.mnemonic:<10} {instruction.op_str}".rstrip()

    if not complete_linear:
        return [render(instruction) for instruction in decoder.disasm(data, address)]
    lines: list[str] = []
    offset = 0
    while offset < len(data):
        instruction = next(decoder.disasm(data[offset:], address + offset, count=1), None)
        if (instruction is None or instruction.address != address + offset
                or instruction.size <= 0 or instruction.size > len(data) - offset):
            lines.append(f"{address + offset:08x}  .byte      0x{data[offset]:02x} ; code/data-unclassified")
            offset += 1
            continue
        lines.append(render(instruction))
        offset += instruction.size
    return lines


def read_source(args: argparse.Namespace) -> tuple[bytes, str]:
    """Read one source image in memory, with no media-file output.

    The archive form deliberately names every nesting layer.  Selecting the
    first matching member would turn archive ordering into an unsupported
    release fallback, particularly for collections containing crack variants.
    """
    archive_mode = args.archive is not None
    direct_mode = args.adf is not None
    if archive_mode == direct_mode:
        raise SystemExit("Specify exactly one ADF path or --archive source")
    if direct_mode:
        return args.adf.read_bytes(), args.adf.name
    if not args.nested_member or not args.member:
        raise SystemExit("--archive requires --nested-member and --member")
    try:
        with ZipFile(args.archive) as outer:
            nested = outer.read(args.nested_member)
        with ZipFile(BytesIO(nested)) as inner:
            return inner.read(args.member), f"{args.archive.name}!{args.nested_member}!{args.member}"
    except (KeyError, OSError, ValueError) as error:
        raise SystemExit(f"Unable to read exact ADF archive member: {error}") from error


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("adf", type=Path, nargs="?", help="Direct ADF path (read only)")
    parser.add_argument("--archive", type=Path,
                        help="Outer ZIP containing one exact nested ZIP member")
    parser.add_argument("--nested-member", help="Exact ZIP member holding the inner ZIP")
    parser.add_argument("--member", help="Exact ADF member inside --nested-member")
    parser.add_argument("--output", type=Path)
    parser.add_argument("--bytes", type=int, default=144)
    parser.add_argument("--complete-linear", action="store_true",
                        help="Decode each proven loaded range as code/data-unclassified M68000 bytes")
    args = parser.parse_args()
    try:
        from capstone import CS_ARCH_M68K, CS_MODE_BIG_ENDIAN, CS_MODE_M68K_000, Cs
    except ImportError as error:
        raise SystemExit("Install analysis dependencies: pip install -r requirements-analysis.txt") from error
    data, source_label = read_source(args)
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
        f"- Source: `{source_label}`",
        f"- Disk identifier: `{identifier!r}`",
        f"- Root/custom block: `{root}` (`0x{root * 512:x}`)",
        f"- Bootstrap track load: disk `0x{loader_disk_offset:x}` → memory `0x{loader_destination:x}`, length `0x{loader_length:x}`",
        f"- Bootstrap entry: `0x{loader_entry:x}`",
        f"- Main stage load: disk `0x{stage_offset:x}` → memory `0x{destination:x}`, length `0x{length:x}`",
        f"- Main entry: `0x{stage_entry:x}` (disk `0x{stage_entry_offset:x}`)",
        f"- Title handoff stage: disk `0x{title_offset:x}` → memory `0x{title_destination:x}`, length `0x{title_length:x}`",
        f"- Title entry: `0x{title_entry:x}` (disk `0x{title_entry_offset:x}`)",
        "- Listing scope: " + ("complete loaded ranges, linear candidate only (code/data unclassified)"
            if args.complete_linear else "bounded entrypoint windows"),
        *( ["- Linear coverage: every source byte is rendered as an instruction or explicit `.byte`."]
           if args.complete_linear else [] ),
        "",
        "## Boot block",
        "",
        "```asm",
    ]
    boot_bytes = 1024 if args.complete_linear else args.bytes
    lines.extend(render_instructions(decoder, data[12 : 12 + boot_bytes], 12,
                                     complete_linear=args.complete_linear))
    lines.extend(["```", "", "## Bootstrap entry", "", "```asm"])
    loader_entry_offset = loader_disk_offset + loader_entry - loader_destination
    bootstrap_bytes = loader_length if args.complete_linear else 192
    bootstrap_source_offset = loader_disk_offset if args.complete_linear else loader_entry_offset
    bootstrap_address = loader_destination if args.complete_linear else loader_entry
    lines.extend(render_instructions(
        decoder, data[bootstrap_source_offset : bootstrap_source_offset + bootstrap_bytes], bootstrap_address,
        complete_linear=args.complete_linear))
    lines.extend(["```", "", "## Main entry", "", "```asm"])
    # End on the instruction boundary immediately before the next routine's
    # absolute LEA; Capstone otherwise substitutes its invalid-input sentinel
    # for a deliberately truncated operand.
    main_bytes = length if args.complete_linear else 0x1FE
    main_source_offset = stage_offset if args.complete_linear else stage_entry_offset
    main_address = destination if args.complete_linear else stage_entry
    lines.extend(render_instructions(
        decoder, data[main_source_offset : main_source_offset + main_bytes], main_address,
        complete_linear=args.complete_linear))
    lines.extend(["```", "", "## Title handoff entry", "", "```asm"])
    # Use a fresh decoder after the intentionally truncated main-entry view.
    # Capstone's M68K binding otherwise can retain an invalid-input sentinel
    # while decoding the next independent byte range.
    title_decoder = Cs(CS_ARCH_M68K, CS_MODE_BIG_ENDIAN | CS_MODE_M68K_000)
    # 196 bytes ends immediately after the complete six-byte move at $404e4.
    # Do not terminate in the following JSR operand: Capstone then renders a
    # misleading sentinel address for that truncated final instruction.
    title_bytes = title_length if args.complete_linear else 196
    title_source_offset = title_offset if args.complete_linear else title_entry_offset
    title_address = title_destination if args.complete_linear else title_entry
    lines.extend(render_instructions(
        title_decoder, data[title_source_offset : title_source_offset + title_bytes], title_address,
        complete_linear=args.complete_linear))
    lines.extend(["```", ""])
    report = "\n".join(lines)
    if args.output:
        args.output.write_text(report, encoding="utf-8")
    else:
        print(report)


if __name__ == "__main__":
    main()
