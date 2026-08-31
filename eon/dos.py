"""Analysis helpers for Millennium's unusual flat DOS executables."""

from __future__ import annotations

from collections import Counter
from dataclasses import dataclass
from pathlib import Path


@dataclass(frozen=True)
class NearJump:
    file_offset: int
    load_address: int


def initial_near_jump(data: bytes, load_address: int = 0x100) -> NearJump | None:
    """Decode the first E9 near jump, allowing common segment setup prefixes."""
    offset = 0
    # push cs/pop ds and push cs/pop es are used before the entry jump.
    while data[offset : offset + 2] in {b"\x0e\x1f", b"\x0e\x07"}:
        offset += 2
    if len(data) < offset + 3 or data[offset] != 0xE9:
        return None
    # 8086 near targets wrap within the 16-bit instruction pointer. Since a
    # COM-style image is loaded at 0x100, file offset and IP differ by 0x100;
    # adding the encoded unsigned word gives the same wrapped file offset.
    displacement = int.from_bytes(data[offset + 1 : offset + 3], "little")
    target_offset = (offset + 3 + displacement) & 0xFFFF
    return NearJump(target_offset, load_address + target_offset)


def interrupt_counts(data: bytes) -> Counter[int]:
    """Return syntactic `int imm8` occurrences for reverse-engineering leads."""
    return Counter(data[index + 1] for index in range(len(data) - 1) if data[index] == 0xCD)


def ascii_strings(data: bytes, minimum: int = 4) -> list[tuple[int, str]]:
    strings: list[tuple[int, str]] = []
    start = None
    for index, value in enumerate(data + b"\0"):
        printable = 0x20 <= value <= 0x7E
        if printable and start is None:
            start = index
        elif not printable and start is not None:
            if index - start >= minimum:
                strings.append((start, data[start:index].decode("ascii")))
            start = None
    return strings


def describe_bytes(name: str, data: bytes) -> dict:
    """Describe one in-memory DOS image without changing its source media."""
    jump = initial_near_jump(data)
    entry_file_offset = jump.file_offset if jump else 0
    # A flat DOS image can deliberately transfer into a separately prepared
    # runtime region.  That target is not source code merely because an 8086
    # decoder can assign mnemonics to zero-filled bytes past the member end.
    # Keep the candidate address for preservation accounting, but expose the
    # member boundary so consumers cannot silently turn an external runtime
    # dependency into a false static code map.
    entry_within_image = entry_file_offset < len(data)
    return {
        "name": name,
        "size": len(data),
        "entry_file_offset": entry_file_offset,
        "entry_load_address": jump.load_address if jump else 0x100,
        "entry_within_image": entry_within_image,
        "entry_beyond_image_bytes": max(0, entry_file_offset - len(data)),
        "interrupts": {f"0x{key:02x}": value for key, value in interrupt_counts(data).most_common()},
        "strings": [{"offset": offset, "text": text} for offset, text in ascii_strings(data)],
    }


def describe(path: Path) -> dict:
    return describe_bytes(path.name, path.read_bytes())
