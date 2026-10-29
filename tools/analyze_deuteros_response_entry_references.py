#!/usr/bin/env python3
"""Find direct 68000 control-flow references to Deuteros' response-loop entry.

The canonical ZIP and ADF are read in place. The ADF member is held only in
memory; this tool never extracts or writes original media or disassembly.
"""

from __future__ import annotations

import argparse
import hashlib
import zipfile
from dataclasses import dataclass
from pathlib import Path


ZIP_SHA256 = "7ecaa0457ad2b61b417bbe62943a4a11b4d164acfbc5a5097e95f8f7d1360533"
ADF_SHA256 = "6ea0cc68d3af37203a885032eddf7c28e839e6abb59d8c9cd3792f1308bdec38"
ADF_MEMBER = "Deuteros - The Next Millennium (1991)(Activision)(M3)(Disk 1 of 2).adf"
STAGE_ADF_OFFSET = 0x6E000
STAGE_RUNTIME_BASE = 0x13000
STAGE_SIZE = 0x6CA00
STAGE_SHA256 = "48d65260e9b5f5cbf8d8b3675a178c81b8764810b61a6a2539a56dcb40a8de03"
RESPONSE_LOOP_ENTRY = 0x4077E


@dataclass(frozen=True)
class DirectReference:
    source: int
    kind: str


def _signed(value: int, bits: int) -> int:
    sign = 1 << (bits - 1)
    return (value ^ sign) - sign


def find_direct_references(
    stage: bytes, runtime_base: int, target: int
) -> list[DirectReference]:
    """Find direct 68000 branches, PC-relative calls/jumps and absolute refs."""
    found: set[DirectReference] = set()

    def word(offset: int) -> int:
        return int.from_bytes(stage[offset : offset + 2], "big")

    def longword(offset: int) -> int:
        return int.from_bytes(stage[offset : offset + 4], "big")

    for offset in range(0, len(stage) - 1, 2):
        address = runtime_base + offset
        opcode = word(offset)

        # BRA, BSR and all conditional branches. On 68000, a zero byte
        # displacement selects the following signed word.
        if opcode & 0xF000 == 0x6000:
            displacement = opcode & 0xFF
            if displacement == 0 and offset + 4 <= len(stage):
                destination = address + 2 + _signed(word(offset + 2), 16)
            else:
                destination = address + 2 + _signed(displacement, 8)
            if destination == target:
                found.add(DirectReference(address, "branch"))

        if opcode in (0x4EBA, 0x4EFA) and offset + 4 <= len(stage):
            destination = address + 2 + _signed(word(offset + 2), 16)
            if destination == target:
                found.add(DirectReference(address,
                    "PC-relative JSR" if opcode == 0x4EBA else "PC-relative JMP"))

        if opcode in (0x4EB9, 0x4EF9) and offset + 6 <= len(stage):
            if longword(offset + 2) == target:
                found.add(DirectReference(address,
                    "absolute JSR" if opcode == 0x4EB9 else "absolute JMP"))

        # LEA absolute-long references can materialize a direct code pointer
        # for a later indirect call. Retain them as candidates, not calls.
        if opcode & 0xF1FF == 0x41F9 and offset + 6 <= len(stage):
            if longword(offset + 2) == target:
                found.add(DirectReference(address, "LEA absolute pointer"))

    # Include raw pointer literals (which can be tables rather than code).
    target_bytes = target.to_bytes(4, "big")
    for offset in range(0, len(stage) - 3):
        if stage.startswith(target_bytes, offset):
            found.add(DirectReference(runtime_base + offset, "absolute pointer literal"))

    return sorted(found, key=lambda reference: (reference.source, reference.kind))


def analyze_archive(path: Path) -> tuple[str, str, str, list[DirectReference]]:
    archive_bytes = path.read_bytes()
    zip_hash = hashlib.sha256(archive_bytes).hexdigest()
    if zip_hash != ZIP_SHA256:
        raise ValueError("Disk 1 ZIP does not match the canonical preservation hash")

    with zipfile.ZipFile(path) as archive:
        if archive.namelist() != [ADF_MEMBER]:
            raise ValueError("Disk 1 archive member list differs from the admitted image")
        adf = archive.read(ADF_MEMBER)
    adf_hash = hashlib.sha256(adf).hexdigest()
    if adf_hash != ADF_SHA256:
        raise ValueError("Disk 1 ADF does not match the canonical preservation hash")
    stage = adf[STAGE_ADF_OFFSET : STAGE_ADF_OFFSET + STAGE_SIZE]
    stage_hash = hashlib.sha256(stage).hexdigest()
    if len(stage) != STAGE_SIZE or stage_hash != STAGE_SHA256:
        raise ValueError("Loaded title-stage span differs from the admitted image")
    references = find_direct_references(stage, STAGE_RUNTIME_BASE, RESPONSE_LOOP_ENTRY)
    return zip_hash, adf_hash, stage_hash, references


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("archive", type=Path, help="canonical Disk 1 ZIP, read-only")
    args = parser.parse_args()
    try:
        zip_hash, adf_hash, stage_hash, references = analyze_archive(args.archive)
    except (OSError, ValueError, zipfile.BadZipFile) as error:
        parser.error(str(error))
    print(f"zip_sha256={zip_hash}")
    print(f"adf_sha256={adf_hash}")
    print(f"stage_sha256={stage_hash}")
    print(f"target=0x{RESPONSE_LOOP_ENTRY:06x}")
    print(f"direct_reference_count={len(references)}")
    for reference in references:
        print(f"0x{reference.source:06x}\t{reference.kind}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
