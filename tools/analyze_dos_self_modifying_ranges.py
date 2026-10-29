#!/usr/bin/env python3
"""Report bounded direct x86 references to an ambiguous DOS code/data span.

The report is a byte-start scan of a hash-locked flat DOS image. It records
candidate instruction decodes only; it does not classify instruction starts,
prove reachability, track runtime register values, or execute patched bytes.
Keep generated reports outside the repository and original media.
"""

from __future__ import annotations

import argparse
import hashlib
from io import BytesIO
import os
from pathlib import Path
import stat
from zipfile import ZipFile

from capstone import CS_ARCH_X86, CS_MODE_16, CS_AC_READ, CS_AC_WRITE, Cs
from capstone.x86_const import X86_OP_MEM


ARCHIVE_SHA256 = "e6e7044b25877fdf8b10d16d2f395886d9957953144ae15ca630cda9cab2a123"
MEMBER = "millennium-return-to-earth-2-2/2200AD.EXE"
MEMBER_SHA256 = "427574e5f780b2a7b5c4207d167116dc44aea3fb67096fbf12a46c4f544a0a57"
MEMBER_SIZE = 54_391
IMAGE_BASE = 0x0100
TARGET_START = 0x740F
TARGET_END = 0x7415
MAX_REFERENCES = 256
CONTEXT_WINDOWS = (
    (0x7415, 0x7480),
    (0x9290, 0x92F2),
    (0xB835, 0xB86E),
    (0xB894, 0xB8A4),
)


def sha256(data: bytes) -> str:
    return hashlib.sha256(data).hexdigest()


def read_image(path: Path) -> bytes:
    try:
        before = path.lstat()
        if stat.S_ISLNK(before.st_mode) or not stat.S_ISREG(before.st_mode):
            raise ValueError("original archive must be a regular non-symlink file")
        descriptor = os.open(path, os.O_RDONLY | getattr(os, "O_NOFOLLOW", 0))
        with os.fdopen(descriptor, "rb") as source:
            opened = os.fstat(source.fileno())
            if (not stat.S_ISREG(opened.st_mode)
                    or (opened.st_dev, opened.st_ino) != (before.st_dev, before.st_ino)):
                raise ValueError("original archive changed before its bounded read")
            archive_bytes = source.read()
        after = path.lstat()
    except OSError as error:
        raise ValueError("original archive could not be read safely") from error
    if (stat.S_ISLNK(after.st_mode) or not stat.S_ISREG(after.st_mode)
            or (before.st_dev, before.st_ino, before.st_size, before.st_mtime_ns)
            != (after.st_dev, after.st_ino, after.st_size, after.st_mtime_ns)
            or (opened.st_size != len(archive_bytes))):
        raise ValueError("original archive changed while it was read")
    if sha256(archive_bytes) != ARCHIVE_SHA256:
        raise ValueError("original archive SHA-256 mismatch")
    with ZipFile(BytesIO(archive_bytes)) as archive:
        names = [item.filename for item in archive.infolist() if item.filename == MEMBER]
        if len(names) != 1:
            raise ValueError("exact original executable member is missing or duplicated")
        image = archive.read(MEMBER)
    if len(image) != MEMBER_SIZE or sha256(image) != MEMBER_SHA256:
        raise ValueError("original executable size or SHA-256 mismatch")
    return image


def output_path(path: Path) -> Path:
    if not path.is_absolute():
        raise ValueError("report path must be absolute")
    if path.exists() or path.is_symlink():
        raise ValueError("report path must not already exist or be a symlink")
    lexical = os.path.normpath(str(path))
    if lexical == "/tmp" or lexical.startswith(("/tmp/", "/private/tmp/")):
        raise ValueError("report path must not use /tmp")
    repo = Path(__file__).resolve().parents[1]
    resolved_parent = path.parent.resolve(strict=True)
    if resolved_parent == repo or repo in resolved_parent.parents:
        raise ValueError("generated report must remain outside the repository")
    cache_root = (Path.home() / ".cache" / "project-eon-analysis").resolve(strict=True)
    destination = resolved_parent / path.name
    if cache_root not in destination.parents:
        raise ValueError("generated report must remain under the external Project Eon analysis cache")
    return destination


def candidate_references(image: bytes) -> list[tuple[int, str, str, int, str, str]]:
    try:
        decoder = Cs(CS_ARCH_X86, CS_MODE_16)
    except ImportError as error:
        raise ValueError("Capstone is required for this bounded disassembly") from error
    decoder.detail = True
    rows: list[tuple[int, str, str, int, str, str]] = []
    linear_starts: set[int] = set()
    offset = 0
    while offset < len(image):
        address = IMAGE_BASE + offset
        instruction = next(decoder.disasm(image[offset:offset + 15], address, count=1), None)
        if instruction is None or instruction.size <= 0:
            offset += 1
            continue
        linear_starts.add(address)
        offset += instruction.size
    for offset in range(len(image)):
        address = IMAGE_BASE + offset
        instruction = next(decoder.disasm(image[offset:offset + 15], address, count=1), None)
        if instruction is None:
            continue
        for operand_index, operand in enumerate(instruction.operands):
            if operand.type != X86_OP_MEM or operand.mem.base or operand.mem.index:
                continue
            effective = operand.mem.disp & 0xFFFF
            width = max(1, operand.size)
            span = {(effective + delta) & 0xFFFF for delta in range(width)}
            overlaps_code = any(TARGET_START <= byte < TARGET_END for byte in span)
            operand_end = effective + width
            overlaps_state = any(
                effective < end and start < operand_end
                for start, end in ((0x75A6, 0x75A8), (0x75A8, 0x75A9),
                                   (0x75AC, 0x75AE), (0x75AE, 0x75AF))
            )
            if not overlaps_code and not overlaps_state:
                continue
            access = operand.access
            operation = ("read/write" if access & CS_AC_READ and access & CS_AC_WRITE else
                         "write" if access & CS_AC_WRITE else
                         "read" if access & CS_AC_READ else "unknown")
            category = "target-span" if overlaps_code else "patch-state"
            alignment = "linear-boundary" if address in linear_starts else "overlap-start"
            rows.append((address, category, alignment, operand_index, operation,
                         f"{instruction.bytes.hex()} {instruction.mnemonic} {instruction.op_str}"))
            if len(rows) > MAX_REFERENCES:
                raise ValueError("candidate reference count exceeds the bounded cap")
    return rows


def candidate_context(image: bytes) -> list[tuple[int, str, str]]:
    """Decode bounded windows around xref candidates without classifying them."""
    decoder = Cs(CS_ARCH_X86, CS_MODE_16)
    rows: list[tuple[int, str, str]] = []
    for start, end in CONTEXT_WINDOWS:
        offset = start - IMAGE_BASE
        limit = end - IMAGE_BASE
        if offset < 0 or limit > len(image) or limit <= offset:
            raise ValueError("candidate context window is outside the verified image")
        cursor = offset
        while cursor < limit:
            address = IMAGE_BASE + cursor
            instruction = next(decoder.disasm(image[cursor:min(cursor + 15, limit)], address,
                                               count=1), None)
            if instruction is None or instruction.size <= 0:
                rows.append((address, image[cursor:cursor + 1].hex(), "<decode-stop>"))
                cursor += 1
                continue
            rows.append((address, instruction.bytes.hex(),
                         f"{instruction.mnemonic} {instruction.op_str}".rstrip()))
            cursor += instruction.size
    return rows


def render(image: bytes, rows: list[tuple[int, str, str, int, str, str]],
           context: list[tuple[int, str, str]]) -> str:
    target_offset = TARGET_START - IMAGE_BASE
    target_bytes = image[target_offset:target_offset + (TARGET_END - TARGET_START)]
    fields = ((0x75A6, 2), (0x75A8, 1), (0x75AC, 2), (0x75AE, 1))
    lines = [
        "# Millennium DOS ambiguous code/data span reference candidates",
        "",
        f"- Outer archive SHA-256: `{ARCHIVE_SHA256}`",
        f"- Exact member: `{MEMBER}`",
        f"- Member size / SHA-256: `{len(image)}` / `{sha256(image)}`",
        f"- Image address basis: flat DOS image at `0x{IMAGE_BASE:04x}`",
        f"- Candidate patch span: `0x{TARGET_START:04x}..0x{TARGET_END - 1:04x}`",
        f"- Initial source bytes: `{target_bytes.hex()}`; SHA-256 `{sha256(target_bytes)}`",
        "- Scope: direct absolute memory operands found by decoding at every byte start.",
        "- Uncertainty: candidate decodes may begin inside data or another instruction; no runtime values or reachability are inferred.",
        "",
        "## Initial bytes at the candidate patch span",
        "",
        "| Address | File offset | Bytes |",
        "| --- | --- | --- |",
    ]
    for address in range(TARGET_START, TARGET_END, 2):
        count = min(2, TARGET_END - address)
        offset = address - IMAGE_BASE
        data = image[offset:offset + count]
        lines.append(f"| `0x{address:04x}` | `0x{offset:04x}` | `{data.hex()}` |")
    lines.extend(["", "## Original bytes at observed patch-state addresses", "",
                  "| Address | Width | File offset | Initial bytes |",
                  "| --- | ---: | --- | --- |"])
    for address, width in fields:
        offset = address - IMAGE_BASE
        lines.append(f"| `0x{address:04x}` | {width} | `0x{offset:04x}` | `{image[offset:offset + width].hex()}` |")
    lines.extend(["", "## Direct-reference candidates", "",
                  "| IP candidate | Kind | Linear alignment | Operand | Access | Bytes and decode |",
                  "| --- | --- | --- | ---: | --- | --- |"])
    for address, category, alignment, operand, access, disassembly in rows:
        lines.append(f"| `0x{address:04x}` | {category} | {alignment} | {operand} | {access} | `{disassembly}` |")
    lines.extend(["", "## Bounded candidate context disassembly", "",
                  "The following windows begin at fixed addresses selected from the reference scan. Each decode is a candidate only; instruction alignment, code/data identity, reachability, and runtime values remain unproven.", "",
                  "| Candidate address | Bytes | Candidate decode |",
                  "| --- | --- | --- |"])
    for address, encoded, disassembly in context:
        lines.append(f"| `0x{address:04x}` | `{encoded}` | `{disassembly}` |")
    lines.extend(["", "These rows locate static references and transfers between the six-byte patch span and the four listed data cells. The values loaded from those cells are runtime state: their original bytes above do not establish the values at a call site. The bytes at the patch span are initially zero, so decoding them as a routine would be unsupported. No patched instruction stream is emitted.", ""])
    return "\n".join(lines)


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--archive", type=Path, required=True)
    parser.add_argument("--output", type=Path, required=True)
    args = parser.parse_args()
    try:
        output = output_path(args.output)
        image = read_image(args.archive)
        report = render(image, candidate_references(image), candidate_context(image))
        descriptor = os.open(output, os.O_WRONLY | os.O_CREAT | os.O_EXCL, 0o600)
        with os.fdopen(descriptor, "w", encoding="utf-8", newline="\n") as stream:
            stream.write(report)
    except (OSError, ValueError) as error:
        parser.error(str(error))
    print(f"Report written: {output} ({len(report.encode('utf-8'))} bytes, SHA-256 {sha256(report.encode('utf-8'))})")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
