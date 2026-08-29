#!/usr/bin/env python3
"""Verify a downloaded Project Eon CI artifact integrity manifest.

This is intentionally independent from artifact creation.  It reads only the
manifest and regular files selected by the caller, never unpacks archives and
never opens user-supplied game media.  It is suitable both for CI's pre-upload
check and for a maintainer verifying a downloaded artifact directory.
"""

from __future__ import annotations

import argparse
import hashlib
import json
from pathlib import Path
import re
import sys
from typing import Any


REVISION_PATTERN = re.compile(r"[0-9a-f]{40}")
SHA256_PATTERN = re.compile(r"[0-9a-f]{64}")
CHUNK_SIZE = 1024 * 1024
MAX_MANIFEST_BYTES = 1024 * 1024


def sha256(path: Path) -> str:
    digest = hashlib.sha256()
    with path.open("rb") as source:
        for chunk in iter(lambda: source.read(CHUNK_SIZE), b""):
            digest.update(chunk)
    return digest.hexdigest()


def parse_arguments(argv: list[str]) -> argparse.Namespace:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--manifest", required=True, type=Path,
                        help="schema-1 JSON artifact manifest")
    parser.add_argument("--directory", required=True, type=Path,
                        help="directory containing the recorded artifacts")
    parser.add_argument("--expected-source-revision",
                        help="require this full lowercase Git commit ID")
    parser.add_argument("--require-exact-directory", action="store_true",
                        help="reject regular files not named by the manifest")
    arguments = parser.parse_args(argv)
    if (arguments.expected_source_revision is not None
            and not REVISION_PATTERN.fullmatch(arguments.expected_source_revision)):
        parser.error("--expected-source-revision must be a 40-character lowercase Git commit ID")
    return arguments


def fail(message: str) -> None:
    raise ValueError(message)


def checked_manifest(path: Path) -> dict[str, Any]:
    if path.is_symlink() or not path.is_file():
        fail("manifest must be a non-symlink regular file")
    if path.stat().st_size > MAX_MANIFEST_BYTES:
        fail("manifest exceeds the 1 MiB safety limit")
    try:
        document = json.loads(path.read_text(encoding="utf-8"))
    except (OSError, UnicodeDecodeError, json.JSONDecodeError) as error:
        fail(f"invalid manifest JSON: {error}")
    if not isinstance(document, dict) or set(document) != {
            "schema_version", "project", "source_revision", "artifacts"}:
        fail("manifest must contain exactly schema_version, project, source_revision, and artifacts")
    if document["schema_version"] != 1 or document["project"] != "project-eon":
        fail("unsupported Project Eon artifact manifest schema")
    if not isinstance(document["source_revision"], str) or not REVISION_PATTERN.fullmatch(document["source_revision"]):
        fail("manifest source_revision must be a 40-character lowercase Git commit ID")
    if not isinstance(document["artifacts"], list) or not document["artifacts"]:
        fail("manifest artifacts must be a non-empty array")
    return document


def verified_names(document: dict[str, Any]) -> list[str]:
    names: list[str] = []
    prior = ""
    for entry in document["artifacts"]:
        if not isinstance(entry, dict) or set(entry) != {"name", "size", "sha256"}:
            fail("each artifact must contain exactly name, size, and sha256")
        name, size, digest = entry["name"], entry["size"], entry["sha256"]
        if (not isinstance(name, str) or not name or "/" in name or "\\" in name
                or Path(name).name != name
                or name in {".", ".."}):
            fail("artifact name must be a safe basename")
        if not isinstance(size, int) or isinstance(size, bool) or size < 0:
            fail(f"artifact {name!r} has an invalid size")
        if not isinstance(digest, str) or not SHA256_PATTERN.fullmatch(digest):
            fail(f"artifact {name!r} has an invalid SHA-256")
        if name <= prior:
            fail("artifact entries must be uniquely sorted by name")
        prior = name
        names.append(name)
    return names


def main(argv: list[str]) -> int:
    arguments = parse_arguments(argv)
    if arguments.directory.is_symlink() or not arguments.directory.is_dir():
        fail("artifact directory must be a non-symlink directory")
    document = checked_manifest(arguments.manifest)
    if (arguments.expected_source_revision is not None
            and document["source_revision"] != arguments.expected_source_revision):
        fail("manifest source_revision does not match --expected-source-revision")
    names = verified_names(document)
    directory = arguments.directory.resolve()
    for entry in document["artifacts"]:
        artifact = directory / entry["name"]
        if artifact.is_symlink() or not artifact.is_file():
            fail(f"artifact is missing or not a non-symlink regular file: {entry['name']}")
        if artifact.stat().st_size != entry["size"]:
            fail(f"artifact size mismatch: {entry['name']}")
        if sha256(artifact) != entry["sha256"]:
            fail(f"artifact SHA-256 mismatch: {entry['name']}")
    if arguments.require_exact_directory:
        expected = set(names)
        if directory == arguments.manifest.resolve().parent:
            expected.add(arguments.manifest.resolve().name)
        unexpected = {entry.name for entry in directory.iterdir()} - expected
        if unexpected:
            fail("artifact directory contains unrecorded entries: " + ", ".join(sorted(unexpected)))
    print(f"verified {len(names)} Project Eon artifact(s) for {document['source_revision']}")
    return 0


if __name__ == "__main__":
    try:
        raise SystemExit(main(sys.argv[1:]))
    except ValueError as error:
        print(f"artifact manifest verification error: {error}", file=sys.stderr)
        raise SystemExit(2)
