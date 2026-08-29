#!/usr/bin/env python3
"""Write a deterministic, download-verifiable manifest for CI artifacts.

The manifest deliberately describes only the produced Project Eon artifacts.
It never reads original game media and does not publish, sign, or upload
anything. GitHub's artifact store remains the transport; a maintainer can
compare downloaded bytes to this adjacent SHA-256 ledger independently.
"""

from __future__ import annotations

import argparse
import hashlib
import json
from pathlib import Path
import re
import sys


REVISION_PATTERN = re.compile(r"[0-9a-f]{40}")
CHUNK_SIZE = 1024 * 1024


def sha256(path: Path) -> str:
    digest = hashlib.sha256()
    with path.open("rb") as source:
        for chunk in iter(lambda: source.read(CHUNK_SIZE), b""):
            digest.update(chunk)
    return digest.hexdigest()


def parse_arguments(argv: list[str]) -> argparse.Namespace:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--source-revision", required=True,
                        help="full lowercase Git commit ID built by CI")
    parser.add_argument("--output", required=True, type=Path,
                        help="JSON manifest to create")
    parser.add_argument("artifacts", type=Path, nargs="+",
                        help="regular artifact files to record")
    arguments = parser.parse_args(argv)
    if not REVISION_PATTERN.fullmatch(arguments.source_revision):
        parser.error("--source-revision must be a 40-character lowercase Git commit ID")
    return arguments


def main(argv: list[str]) -> int:
    arguments = parse_arguments(argv)
    entries: list[dict[str, int | str]] = []
    names: set[str] = set()
    if arguments.output.is_symlink():
        raise ValueError("the manifest output must not be a symlink")
    output = arguments.output.resolve()
    for artifact in arguments.artifacts:
        if artifact.is_symlink():
            raise ValueError(f"artifact must be a non-symlink regular file: {artifact}")
        resolved = artifact.resolve()
        if resolved == output:
            raise ValueError("the manifest cannot record itself")
        if not resolved.is_file():
            raise ValueError(f"artifact must be a non-symlink regular file: {artifact}")
        # Only record the basename: CI workspace paths are host-specific and
        # should not leak into a reproducibility record downloaded by users.
        if resolved.name in names:
            raise ValueError(f"duplicate artifact name: {resolved.name}")
        names.add(resolved.name)
        entries.append({
            "name": resolved.name,
            "size": resolved.stat().st_size,
            "sha256": sha256(resolved),
        })
    entries.sort(key=lambda entry: str(entry["name"]))
    payload = {
        "schema_version": 1,
        "project": "project-eon",
        "source_revision": arguments.source_revision,
        "artifacts": entries,
    }
    arguments.output.parent.mkdir(parents=True, exist_ok=True)
    arguments.output.write_text(json.dumps(payload, indent=2, sort_keys=True) + "\n",
                                encoding="utf-8")
    return 0


if __name__ == "__main__":
    try:
        raise SystemExit(main(sys.argv[1:]))
    except ValueError as error:
        print(f"artifact manifest error: {error}", file=sys.stderr)
        raise SystemExit(2)
