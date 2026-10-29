#!/usr/bin/env python3
"""Reproduce the source-derived Millennium Atari title palette metadata.

Reads the exact Equinox disk and `MILL22A.INF` in memory. It does not extract
or modify original media and emits only offsets, hashes and palette words.
"""

from __future__ import annotations

import argparse
import hashlib
import json
from pathlib import Path
from zipfile import ZipFile

from analyze_atari_st_prg import fat12_member


DISK_SHA256 = "3f090651ee586cf32a3f37f41b748ba36c78799e7bf761b66ddca2352579afe7"
CONFIG_SHA256 = "74d7d630779fd811aedcdbe31b14e54198eb9ffd673df512dd70b6165c4a37b6"
CONFIG_BYTES = 7506
SOURCE_OFFSET = 0x16C
PALETTE_OFFSET = 0xF28
SOURCE_BYTES = 96
PALETTE_WORDS = 16
PASSES = 7


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--archive", type=Path, required=True)
    parser.add_argument("--disk-member", required=True)
    args = parser.parse_args()

    with ZipFile(args.archive) as archive:
        disk = archive.read(args.disk_member)
    disk_hash = hashlib.sha256(disk).hexdigest()
    if disk_hash != DISK_SHA256:
        raise SystemExit("exact Equinox Atari ST disk SHA-256 mismatch")
    config = fat12_member(disk, "MILL22A.INF")
    config_hash = hashlib.sha256(config).hexdigest()
    if len(config) != CONFIG_BYTES or config_hash != CONFIG_SHA256:
        raise SystemExit("exact MILL22A.INF size or SHA-256 mismatch")

    source = bytearray(config[SOURCE_OFFSET:SOURCE_OFFSET + SOURCE_BYTES])
    palette = [
        int.from_bytes(config[PALETTE_OFFSET + index * 2:PALETTE_OFFSET + index * 2 + 2], "big")
        for index in range(PALETTE_WORDS)
    ]
    if len(source) != SOURCE_BYTES or len(palette) != PALETTE_WORDS:
        raise SystemExit("palette source ranges are incomplete")

    for _ in range(PASSES):
        for group in range(PALETTE_WORDS):
            carry_weight = 0x0100
            for lane in range(3):
                index = group * 6 + lane * 2
                total = source[index] + source[index + 1]
                if total > 0xFF:
                    palette[group] = (palette[group] + carry_weight) & 0xFFFF
                source[index] = total & 0xFF
                carry_weight >>= 4

    palette_bytes = b"".join(word.to_bytes(2, "big") for word in palette)
    result = {
        "disk_sha256": disk_hash,
        "file": "MILL22A.INF",
        "file_sha256": config_hash,
        "file_bytes": len(config),
        "source_offset": f"+0x{SOURCE_OFFSET:x}",
        "source_bytes": SOURCE_BYTES,
        "initial_palette_offset": f"+0x{PALETTE_OFFSET:x}",
        "initial_palette_sha256": hashlib.sha256(
            config[PALETTE_OFFSET:PALETTE_OFFSET + PALETTE_WORDS * 2]
        ).hexdigest(),
        "pass_count": PASSES,
        "palette_words_rgb3": [f"{word:04x}" for word in palette],
        "palette_rgb3_components": [
            [(word >> 8) & 7, (word >> 4) & 7, word & 7] for word in palette
        ],
        "final_palette_sha256": hashlib.sha256(palette_bytes).hexdigest(),
    }
    print(json.dumps(result, indent=2, sort_keys=True))


if __name__ == "__main__":
    main()
