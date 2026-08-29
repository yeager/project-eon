#!/usr/bin/env python3
"""Assemble externally recorded Project Eon reference-trace evidence safely.

This tool is deliberately not an emulator, recorder, trace normalizer, or
runtime input.  It only copies a user-supplied *external* event stream into a
new evidence directory after binding it to an unchanged, recognised original
release.  The resulting manifest must still be independently reviewed and is
accepted by Project Eon only through ``--reference-trace``.
"""

from __future__ import annotations

import argparse
import datetime as dt
import hashlib
import json
import os
from pathlib import Path
import shutil
import stat
import sys
import tempfile


ROOT = Path(__file__).resolve().parents[1]
MANIFEST = ROOT / "docs" / "release-manifest.json"
MAX_EVENT_SIZE = 256 * 1024 * 1024
MAX_METADATA_SIZE = 64 * 1024
MAX_LINE_SIZE = 4096
SHA256_HEX = set("0123456789abcdef")

COMMON_METADATA = {
    "format", "game", "platform", "language", "capture_start_utc",
    "capture_end_utc", "emulator_name", "emulator_version", "emulator_sha256",
    "config_sha256", "command_tail_sha256", "input_timeline_sha256",
}
V2_ADAPTERS = {
    "millennium-dos-en-startup-v1": {
        "game": "millennium", "platform": "dos", "language": "en",
        "sha256": "e6e7044b25877fdf8b10d16d2f395886d9957953144ae15ca630cda9cab2a123",
        "size": 328383,
    },
    "deuteros-atari-st-boot-v1": {"game": "deuteros", "platform": "atari_st", "language": "en",
                                   "sha256": "c6856d0a7ccda925289c60f0675e7aaed616f8a0289c74698e87e1ee11e6c653",
                                   "size": 3021682,
                                   "source_media_sha256": "aba874134807360ccde0ff98d6b82a965f57dcae5800b5b54394472522ef5bee",
                                   "source_stage_sha256": "2489256511e857a4a1b20d413b4f869edaae1f4df7f62ce869e324cad40e81d7"},
    "millennium-amiga-en-defjam-bootstrap-v1": {
        "game": "millennium", "platform": "amiga", "language": "en",
        "sha256": "2e27d7aeb8b8b7f2a75eda45b456ab42775a706aa85516c85e61ce94ec9eb400",
        "size": 2558009,
    },
    "deuteros-amiga-en-title-stage-v1": {"game": "deuteros", "platform": "amiga", "language": "en",
                                           "sha256": "f4dc8dd1c27c5d389837783becd9b95ab09b78baf40e94e39e2b7e590e470e04",
                                           "size": 4066771,
                                           "source_media_sha256": "6ea0cc68d3af37203a885032eddf7c28e839e6abb59d8c9cd3792f1308bdec38",
                                           "source_stage_sha256": "48d65260e9b5f5cbf8d8b3675a178c81b8764810b61a6a2539a56dcb40a8de03"},
}
V3_ADAPTERS = {
    "deuteros-amiga-en-title-bridge-v3": {
        "game": "deuteros", "platform": "amiga", "language": "en",
        "sha256": "f4dc8dd1c27c5d389837783becd9b95ab09b78baf40e94e39e2b7e590e470e04",
        "size": 4066771,
        "source_media_sha256": "6ea0cc68d3af37203a885032eddf7c28e839e6abb59d8c9cd3792f1308bdec38",
        "source_stage_sha256": "48d65260e9b5f5cbf8d8b3675a178c81b8764810b61a6a2539a56dcb40a8de03",
    },
}


class EvidenceError(RuntimeError):
    """A bounded input/provenance failure; never an excuse to make a trace."""


def require_absolute_regular_file(path: Path, label: str, maximum: int | None = None) -> os.stat_result:
    if not path.is_absolute():
        raise EvidenceError(f"{label} path must be absolute")
    try:
        info = path.lstat()
    except OSError as error:
        raise EvidenceError(f"Unable to stat {label}: {error}") from error
    if stat.S_ISLNK(info.st_mode) or not stat.S_ISREG(info.st_mode):
        raise EvidenceError(f"{label} must be a non-symlink regular file")
    if maximum is not None and info.st_size > maximum:
        raise EvidenceError(f"{label} exceeds {maximum} bytes")
    return info


def secure_open(path: Path) -> int:
    flags = os.O_RDONLY | getattr(os, "O_CLOEXEC", 0) | getattr(os, "O_NOFOLLOW", 0)
    try:
        return os.open(path, flags)
    except OSError as error:
        raise EvidenceError(f"Unable to open input without following links: {error}") from error


def hash_fd(fd: int) -> tuple[str, int, os.stat_result]:
    digest = hashlib.sha256()
    os.lseek(fd, 0, os.SEEK_SET)
    size = 0
    while True:
        chunk = os.read(fd, 1024 * 1024)
        if not chunk:
            break
        digest.update(chunk)
        size += len(chunk)
    os.lseek(fd, 0, os.SEEK_SET)
    return digest.hexdigest(), size, os.fstat(fd)


def same_file_identity(first: os.stat_result, second: os.stat_result) -> bool:
    return (first.st_dev, first.st_ino, first.st_size) == (second.st_dev, second.st_ino, second.st_size)


def open_checked_input(path: Path, label: str, maximum: int) -> tuple[int, os.stat_result]:
    initial = require_absolute_regular_file(path, label, maximum)
    fd = secure_open(path)
    try:
        opened = os.fstat(fd)
        if (not stat.S_ISREG(opened.st_mode) or opened.st_size > maximum
                or not same_file_identity(initial, opened)):
            raise EvidenceError(f"{label} changed while it was opened")
        return fd, opened
    except Exception:
        os.close(fd)
        raise


def read_checked_input(path: Path, label: str, maximum: int) -> bytes:
    fd, initial = open_checked_input(path, label, maximum)
    try:
        chunks: list[bytes] = []
        remaining = initial.st_size
        while remaining:
            chunk = os.read(fd, min(1024 * 1024, remaining))
            if not chunk:
                raise EvidenceError(f"Unable to read complete {label}")
            chunks.append(chunk)
            remaining -= len(chunk)
        if os.read(fd, 1) or not same_file_identity(initial, os.fstat(fd)):
            raise EvidenceError(f"{label} changed while it was read")
        return b"".join(chunks)
    finally:
        os.close(fd)


def is_sha256(value: str) -> bool:
    return len(value) == 64 and set(value) <= SHA256_HEX


def parse_utc(value: str) -> None:
    if len(value) != 20 or not value.endswith("Z"):
        raise EvidenceError("capture timestamps must be exact UTC YYYY-MM-DDTHH:MM:SSZ")
    try:
        dt.datetime.strptime(value, "%Y-%m-%dT%H:%M:%SZ")
    except ValueError as error:
        raise EvidenceError("capture timestamps must be real Gregorian UTC instants") from error


def valid_ascii(value: str) -> bool:
    return bool(value) and all(0x20 <= ord(character) <= 0x7e for character in value)


def parse_metadata(path: Path) -> dict[str, str]:
    data = read_checked_input(path, "metadata", MAX_METADATA_SIZE)
    if not data or not data.endswith(b"\n") or b"\r" in data:
        raise EvidenceError("metadata must be non-empty LF-terminated key<TAB>value records")
    try:
        lines = data.decode("utf-8").splitlines()
    except UnicodeDecodeError as error:
        raise EvidenceError("metadata must be UTF-8") from error
    fields: dict[str, str] = {}
    for line in lines:
        if not line or len(line) > MAX_LINE_SIZE or line.count("\t") != 1:
            raise EvidenceError("metadata must contain one bounded key<TAB>value record per line")
        key, value = line.split("\t", 1)
        if (not key or any(character not in "abcdefghijklmnopqrstuvwxyz0123456789_" for character in key)
                or not valid_ascii(value)):
            raise EvidenceError("metadata has an invalid key or non-ASCII value")
        if key in fields:
            raise EvidenceError("metadata has a duplicate key")
        fields[key] = value
    return fields


def release_identity(source_sha256: str, source_size: int) -> dict[str, object]:
    manifest = json.loads(MANIFEST.read_text(encoding="utf-8"))
    for release in manifest["releases"]:
        if release["sha256"] == source_sha256 and release["size"] == source_size:
            return release
    raise EvidenceError("source release is not a hash-recognised Project Eon release")


def validate_metadata(fields: dict[str, str], release: dict[str, object]) -> None:
    version = fields.get("format")
    if version == "project-eon-reference-trace-v1":
        expected = COMMON_METADATA
    elif version == "project-eon-reference-trace-v2":
        adapter = fields.get("adapter")
        if adapter not in V2_ADAPTERS:
            raise EvidenceError("v2 metadata must name a registered adapter")
        expected = COMMON_METADATA | {"adapter"}
        if adapter in {"deuteros-atari-st-boot-v1", "deuteros-amiga-en-title-stage-v1"}:
            expected |= {"source_media_sha256", "source_stage_sha256"}
        for key, expected_value in V2_ADAPTERS[adapter].items():
            if key in {"sha256", "size"}:
                if release.get(key) != expected_value:
                    raise EvidenceError(f"adapter does not match its exact source {key}")
                continue
            if fields.get(key) != expected_value:
                raise EvidenceError(f"adapter does not match its exact {key}")
    elif version == "project-eon-reference-trace-v3":
        adapter = fields.get("adapter")
        if adapter not in V3_ADAPTERS:
            raise EvidenceError("v3 metadata must name a registered adapter")
        expected = COMMON_METADATA | {"adapter", "source_media_sha256", "source_stage_sha256"}
        for key, expected_value in V3_ADAPTERS[adapter].items():
            if key in {"sha256", "size"}:
                if release.get(key) != expected_value:
                    raise EvidenceError(f"adapter does not match its exact source {key}")
                continue
            if fields.get(key) != expected_value:
                raise EvidenceError(f"adapter does not match its exact {key}")
    else:
        raise EvidenceError("format must be project-eon-reference-trace-v1, -v2, or -v3")
    if set(fields) != expected:
        raise EvidenceError("metadata has unknown, missing, or assembler-owned fields")
    for key in ("emulator_sha256", "config_sha256", "command_tail_sha256", "input_timeline_sha256"):
        if not is_sha256(fields[key]):
            raise EvidenceError(f"{key} must be a lower-case SHA-256")
    parse_utc(fields["capture_start_utc"])
    parse_utc(fields["capture_end_utc"])
    if fields["capture_end_utc"] < fields["capture_start_utc"]:
        raise EvidenceError("capture end precedes capture start")
    for key in ("game", "platform", "language"):
        if fields[key] != release[key]:
            raise EvidenceError(f"metadata {key} does not match the supplied recognised source")


def reject_output_path(source: Path, event: Path, metadata: Path, output: Path) -> None:
    if not output.is_absolute():
        raise EvidenceError("output path must be absolute")
    if output.exists() or output.is_symlink():
        raise EvidenceError("output directory must not exist")
    # Inputs are regular files, so a distinct non-existent directory cannot
    # overlap their bytes.  Deliberately permit a sibling capture directory:
    # a user commonly keeps a read-only archive and its separately owned
    # evidence under one collection root.
    if output in (source, event, metadata):
        raise EvidenceError("output directory must not name an input file")


def copy_event(input_fd: int, expected: os.stat_result, output_path: Path) -> tuple[str, int]:
    digest = hashlib.sha256()
    total = 0
    try:
        with os.fdopen(input_fd, "rb", closefd=False) as source, output_path.open("xb") as destination:
            while True:
                chunk = source.read(1024 * 1024)
                if not chunk:
                    break
                total += len(chunk)
                if total > MAX_EVENT_SIZE:
                    raise EvidenceError("event stream exceeds 268435456 bytes")
                digest.update(chunk)
                destination.write(chunk)
            destination.flush()
            os.fsync(destination.fileno())
    finally:
        os.lseek(input_fd, 0, os.SEEK_SET)
    if total == 0:
        raise EvidenceError("event stream must not be empty")
    if total != expected.st_size or not same_file_identity(expected, os.fstat(input_fd)):
        raise EvidenceError("event stream changed while it was copied")
    return digest.hexdigest(), total


def write_text_atomic(path: Path, content: str) -> None:
    temporary = path.with_name(path.name + ".tmp")
    temporary.write_text(content, encoding="utf-8", newline="\n")
    with temporary.open("rb") as stream:
        os.fsync(stream.fileno())
    temporary.replace(path)


def assemble(args: argparse.Namespace) -> Path:
    source = Path(args.source_release)
    events = Path(args.events)
    metadata = Path(args.metadata)
    output = Path(args.output)
    source_initial = require_absolute_regular_file(source, "source release")
    event_initial = require_absolute_regular_file(events, "event stream", MAX_EVENT_SIZE)
    if (source_initial.st_dev, source_initial.st_ino) == (event_initial.st_dev, event_initial.st_ino):
        raise EvidenceError("event stream must not be the original source release")
    reject_output_path(source, events, metadata, output)
    source_fd = secure_open(source)
    event_fd = -1
    try:
        source_hash, source_size, source_before = hash_fd(source_fd)
        if not stat.S_ISREG(source_before.st_mode) or not same_file_identity(source_initial, source_before):
            raise EvidenceError("source release changed while it was opened")
        release = release_identity(source_hash, source_size)
        fields = parse_metadata(metadata)
        validate_metadata(fields, release)
        event_fd, opened_event = open_checked_input(events, "event stream", MAX_EVENT_SIZE)
        if not same_file_identity(event_initial, opened_event):
            raise EvidenceError("event stream changed while it was opened")
        tool_fd = secure_open(Path(__file__).resolve())
        try:
            tool_hash, _, _ = hash_fd(tool_fd)
        finally:
            os.close(tool_fd)
        staging_parent = output.parent
        if not staging_parent.is_dir() or staging_parent.is_symlink():
            raise EvidenceError("output parent must be an existing non-symlink directory")
        staging = Path(tempfile.mkdtemp(prefix=".eon-trace-", dir=staging_parent))
        try:
            os.chmod(staging, 0o700)
            event_hash, event_size = copy_event(event_fd, opened_event, staging / "events.eontrace")
            source_after_hash, source_after_size, source_after = hash_fd(source_fd)
            if (source_hash, source_size, source_before.st_dev, source_before.st_ino) != (
                source_after_hash, source_after_size, source_after.st_dev, source_after.st_ino):
                raise EvidenceError("source release changed during trace assembly; no receipt issued")
            manifest_fields = dict(fields)
            manifest_fields.update({
                "event_file": "events.eontrace", "event_size": str(event_size), "event_sha256": event_hash,
                "source_release_sha256": source_hash, "source_release_size": str(source_size),
            })
            order = ["format"] + (["adapter"] if "adapter" in manifest_fields else []) + [
                "event_file", "event_size", "event_sha256", "game", "platform", "language",
                "source_release_sha256", "source_release_size",
            ]
            if "source_media_sha256" in manifest_fields:
                order += ["source_media_sha256", "source_stage_sha256"]
            order += ["capture_start_utc", "capture_end_utc", "emulator_name", "emulator_version",
                      "emulator_sha256", "config_sha256", "command_tail_sha256", "input_timeline_sha256"]
            manifest_text = "".join(f"{key}\t{manifest_fields[key]}\n" for key in order)
            write_text_atomic(staging / "manifest.eontrace", manifest_text)
            receipt = {
                "schema": "project-eon.reference-trace-receipt/v1",
                "status": "assembled-not-admitted",
                "source_release": {"sha256_before": source_hash, "sha256_after": source_after_hash,
                                   "size_before": source_size, "size_after": source_after_size,
                                   "device": source_before.st_dev, "inode": source_before.st_ino},
                "event": {"path": "events.eontrace", "sha256": event_hash, "size": event_size},
                "manifest": {"path": "manifest.eontrace", "sha256": hashlib.sha256(manifest_text.encode()).hexdigest()},
                "tool": {"path": str(Path(__file__).resolve()), "sha256": tool_hash},
            }
            write_text_atomic(staging / "receipt.json", json.dumps(receipt, sort_keys=True, indent=2) + "\n")
            staging.replace(output)
        except Exception:
            shutil.rmtree(staging, ignore_errors=True)
            raise
    finally:
        if event_fd >= 0:
            os.close(event_fd)
        os.close(source_fd)
    return output


def parse_arguments(argv: list[str]) -> argparse.Namespace:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--source-release", required=True, help="Absolute user-owned original archive path")
    parser.add_argument("--events", required=True, help="Absolute external event stream path")
    parser.add_argument("--metadata", required=True, help="Absolute LF key<TAB>value metadata path")
    parser.add_argument("--output", required=True, help="New absolute evidence directory")
    return parser.parse_args(argv)


def main(argv: list[str] | None = None) -> int:
    try:
        output = assemble(parse_arguments(sys.argv[1:] if argv is None else argv))
    except EvidenceError as error:
        print(f"Reference trace assembly rejected: {error}", file=sys.stderr)
        return 2
    except OSError as error:
        print(f"Reference trace assembly failed: {error}", file=sys.stderr)
        return 2
    print(f"REFERENCE TRACE ASSEMBLED  external evidence only; no admission or replay: {output}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
