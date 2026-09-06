#!/usr/bin/env python3
"""Reject original media and generated reverse-engineering output in Git."""

from __future__ import annotations

import subprocess
from pathlib import Path, PurePosixPath


ROOT = Path(__file__).resolve().parents[1]

FORBIDDEN_DIRECTORIES = {
    "analysis-work",
    "disassembly-output",
    "disassembly-report",
    "disassembly-reports",
    "game-data",
}
FORBIDDEN_SUFFIXES = {
    ".adf", ".bin", ".com", ".exe", ".i64", ".idb", ".img", ".lst",
    ".msa", ".objdump", ".prg", ".st", ".tos",
}
FORBIDDEN_GENERATED_ENDINGS = (
    ".disassembly.jsonl",
    ".disassembly.md",
    ".disassembly.txt",
)


def forbidden_tracked_paths(paths: list[str]) -> list[str]:
    rejected: list[str] = []
    for raw_path in paths:
        path = PurePosixPath(raw_path)
        lowered = raw_path.lower()
        if (FORBIDDEN_DIRECTORIES.intersection(path.parts)
                or path.suffix.lower() in FORBIDDEN_SUFFIXES
                or lowered.endswith(FORBIDDEN_GENERATED_ENDINGS)):
            rejected.append(raw_path)
    return sorted(rejected)


def tracked_paths() -> list[str]:
    result = subprocess.run(
        ["git", "ls-files", "-z"], cwd=ROOT, check=True,
        stdout=subprocess.PIPE,
    )
    return [entry.decode("utf-8") for entry in result.stdout.split(b"\0") if entry]


def main() -> int:
    rejected = forbidden_tracked_paths(tracked_paths())
    if rejected:
        print("Forbidden original-media or reverse-engineering artifacts are tracked:")
        for path in rejected:
            print(f"  {path}")
        return 1
    print("Repository artifact policy: PASS")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
