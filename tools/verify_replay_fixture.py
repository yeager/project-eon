#!/usr/bin/env python3
"""Verify one externally retained Project Eon replay checkpoint fixture.

This verifier intentionally validates provenance and opaque bytes only.  It
does not decode a frame, play audio, load game state, emulate an input, or
open user-supplied game media.
"""
from __future__ import annotations

import argparse
import hashlib
import json
from pathlib import Path
import re
import stat


ROOT = Path(__file__).resolve().parents[1]
FORMAT = "project-eon-replay-fixture-v1"
MANIFEST_NAME = "fixture.eonfixture"
SHA256 = re.compile(r"^[0-9a-f]{64}$")
DECIMAL = re.compile(r"^(0|[1-9][0-9]*)$")
KINDS = {"frame": 16 * 1024 * 1024, "audio": 64 * 1024 * 1024,
         "state": 16 * 1024 * 1024, "input": 64 * 1024}
REQUIRED_FIELDS = {
    "format", "kind", "source_release_sha256", "source_release_size",
    "capture_sha256", "checkpoint_sequence", "checkpoint_tick",
    "payload_file", "payload_sha256", "payload_bytes",
}


def digest(path: Path) -> tuple[str, int]:
    value = hashlib.sha256()
    size = 0
    with path.open("rb") as stream:
        for block in iter(lambda: stream.read(1024 * 1024), b""):
            value.update(block)
            size += len(block)
    return value.hexdigest(), size


def regular_file(path: Path, description: str) -> None:
    info = path.lstat()
    if stat.S_ISLNK(info.st_mode) or not stat.S_ISREG(info.st_mode):
        raise ValueError(f"{description} must be a regular non-symlink file")


def read_manifest(path: Path) -> dict[str, str]:
    regular_file(path, MANIFEST_NAME)
    fields: dict[str, str] = {}
    for line in path.read_text(encoding="utf-8").splitlines():
        if line.count("=") != 1:
            raise ValueError("fixture manifest contains an invalid line")
        key, value = line.split("=", 1)
        if not key or not value or key in fields:
            raise ValueError("fixture manifest has an empty or duplicate field")
        fields[key] = value
    if set(fields) != REQUIRED_FIELDS:
        raise ValueError("fixture manifest has unknown, missing, or incomplete fields")
    return fields


def known_releases() -> dict[str, int]:
    manifest_path = ROOT / "docs" / "release-manifest.json"
    with manifest_path.open(encoding="utf-8") as stream:
        document = json.load(stream)
    if document.get("schema") != "project-eon.release-manifest/v1":
        raise ValueError("repository release manifest schema is unsupported")
    result: dict[str, int] = {}
    for release in document.get("releases", []):
        identity, size = release.get("sha256"), release.get("size")
        if not isinstance(identity, str) or not SHA256.fullmatch(identity) or not isinstance(size, int) or size <= 0:
            raise ValueError("repository release manifest contains an invalid release identity")
        if identity in result:
            raise ValueError("repository release manifest duplicates a release identity")
        result[identity] = size
    return result


def safe_basename(value: str) -> bool:
    return value not in {"", ".", ".."} and Path(value).name == value and "/" not in value and "\\" not in value


def verify(directory: Path) -> dict[str, str]:
    if not directory.is_absolute() or directory.is_symlink() or not directory.is_dir():
        raise ValueError("fixture directory must be an absolute non-symlink directory")
    fields = read_manifest(directory / MANIFEST_NAME)
    if fields["format"] != FORMAT:
        raise ValueError("fixture format is unsupported")
    kind = fields["kind"]
    if kind not in KINDS:
        raise ValueError("fixture kind is unsupported")
    source_hash = fields["source_release_sha256"]
    if not SHA256.fullmatch(source_hash) or not SHA256.fullmatch(fields["capture_sha256"]):
        raise ValueError("fixture provenance hash is not lower-case SHA-256")
    if not DECIMAL.fullmatch(fields["source_release_size"]):
        raise ValueError("fixture source release size is not canonical decimal")
    known_size = known_releases().get(source_hash)
    if known_size is None or fields["source_release_size"] != str(known_size):
        raise ValueError("fixture source release identity is not recognised")
    for field in ("checkpoint_sequence", "checkpoint_tick", "payload_bytes"):
        if not DECIMAL.fullmatch(fields[field]):
            raise ValueError(f"fixture {field} is not canonical decimal")
    if fields["checkpoint_sequence"] == "0":
        raise ValueError("fixture checkpoint sequence must be positive")
    if not safe_basename(fields["payload_file"]):
        raise ValueError("fixture payload filename is unsafe")
    if not SHA256.fullmatch(fields["payload_sha256"]):
        raise ValueError("fixture payload hash is not lower-case SHA-256")
    payload = directory / fields["payload_file"]
    regular_file(payload, "fixture payload")
    actual_hash, actual_size = digest(payload)
    if actual_size > KINDS[kind]:
        raise ValueError("fixture payload exceeds its kind-specific safety limit")
    if (fields["payload_sha256"], fields["payload_bytes"]) != (actual_hash, str(actual_size)):
        raise ValueError("fixture payload hash or size mismatch")
    return fields


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--fixture", type=Path, required=True,
                        help="absolute directory containing fixture.eonfixture and its opaque payload")
    args = parser.parse_args()
    try:
        fields = verify(args.fixture)
    except (OSError, ValueError, json.JSONDecodeError) as error:
        print(f"REPLAY FIXTURE REJECTED  {error}")
        return 2
    print("REPLAY FIXTURE VERIFIED  " + fields["kind"] + "  " + str(args.fixture))
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
