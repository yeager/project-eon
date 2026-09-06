#!/usr/bin/env python3
"""Reject original media and generated reverse-engineering output in Git."""

from __future__ import annotations

import re
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
    ".adf", ".bin", ".bndb", ".com", ".dms", ".dump", ".exe",
    ".ghidra", ".hdf", ".i64", ".id0", ".id1", ".idb", ".img",
    ".ipf", ".lha", ".lzh", ".lst", ".msa", ".nam", ".objdump",
    ".prg", ".raw", ".rom", ".st", ".til", ".tos", ".zip",
}
FORBIDDEN_GENERATED_ENDINGS = (
    ".disassembly.jsonl",
    ".disassembly.md",
    ".disassembly.txt",
)
FORBIDDEN_CONTENT = re.compile(rb"(?im)^```(?:asm|disassembly|objdump)\s*$")


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


def forbidden_tracked_content(paths: list[str], root: Path = ROOT,
                              blob_reader=None) -> list[str]:
    """Reject raw instruction listings even when their filename looks benign."""
    rejected: list[str] = []
    for raw_path in paths:
        try:
            data = blob_reader(raw_path) if blob_reader else (root / raw_path).read_bytes()
        except (OSError, ValueError, subprocess.CalledProcessError):
            continue
        if FORBIDDEN_CONTENT.search(data):
            rejected.append(raw_path)
    return sorted(rejected)


def indexed_blob(raw_path: str) -> bytes:
    """Read the exact staged blob so worktree edits cannot hide an artifact."""
    return subprocess.run(
        ["git", "show", f":{raw_path}"], cwd=ROOT, check=True,
        stdout=subprocess.PIPE, stderr=subprocess.DEVNULL,
    ).stdout


def main() -> int:
    paths = tracked_paths()
    rejected = sorted(set(forbidden_tracked_paths(paths) +
                          forbidden_tracked_content(paths, blob_reader=indexed_blob)))
    if rejected:
        print("Forbidden original-media or reverse-engineering artifacts are tracked:")
        for path in rejected:
            print(f"  {path}")
        return 1
    print("Repository artifact policy: PASS")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
