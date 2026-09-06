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
# A report body is still a raw listing when its author omits the Markdown
# fence.  Require an address, at least two encoded bytes, and a mnemonic-like
# token so ordinary hashes, tables, and prose addresses do not match.
RAW_INSTRUCTION_LINE = re.compile(
    rb"(?im)^\s*(?:0x)?[0-9a-f]{4,8}:\s+"
    rb"(?:[0-9a-f]{2}\s+){2,}[a-z][a-z0-9.]*\b"
)

# Exact original spans do not become safe merely because they are short.
# Production byte tables with two or more literals are therefore rejected by
# default. A genuinely public-format or independently specified constant may
# carry the narrowly reviewed marker immediately above its declaration.
BYTE_INITIALIZER_ALLOW_MARKER = b"EON_ARTIFACT_POLICY_ALLOW:"
BYTE_INITIALIZER = re.compile(
    rb"(?:std::array\s*<\s*std::uint8_t\s*,\s*[^>]+>|"
    rb"std::to_array\s*<\s*std::uint8_t\s*>\s*\()"
    rb"[^;={]*[({]{1,2}(.*?)[})]{1,2}\s*;",
    re.DOTALL,
)
BYTE_LITERAL = re.compile(
    rb"(?<![A-Za-z0-9_])0x[0-9a-fA-F]{1,2}(?![A-Za-z0-9_])"
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


def forbidden_tracked_content(paths: list[str], root: Path = ROOT,
                              blob_reader=None) -> list[str]:
    """Reject raw listings and unbudgeted production executable-byte arrays."""
    rejected: list[str] = []
    for raw_path in paths:
        try:
            data = blob_reader(raw_path) if blob_reader else (root / raw_path).read_bytes()
        except (OSError, ValueError, subprocess.CalledProcessError):
            continue
        if FORBIDDEN_CONTENT.search(data) or RAW_INSTRUCTION_LINE.search(data):
            rejected.append(raw_path)
            continue
        if raw_path.startswith("src/") and raw_path.endswith((".c", ".cpp", ".h", ".hpp")):
            forbidden_initializer = False
            for match in BYTE_INITIALIZER.finditer(data):
                if len(BYTE_LITERAL.findall(match.group(1))) < 2:
                    continue
                prefix = data[max(0, match.start() - 240):match.start()]
                if any(BYTE_INITIALIZER_ALLOW_MARKER in line
                       for line in prefix.splitlines()[-3:]):
                    continue
                forbidden_initializer = True
                break
            if forbidden_initializer:
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
