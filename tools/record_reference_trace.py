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
MAX_PROVENANCE_ARTIFACT_SIZE = 8 * 1024 * 1024
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
    "millennium-dos-en-gx-startup-v2": {
        "game": "millennium", "platform": "dos", "language": "en",
        "sha256": "e6e7044b25877fdf8b10d16d2f395886d9957953144ae15ca630cda9cab2a123",
        "size": 328383,
    },
    "millennium-dos-en-title-init-v2": {
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
    "deuteros-amiga-en-main-copy-loop-v3": {
        "game": "deuteros", "platform": "amiga", "language": "en",
        "sha256": "f4dc8dd1c27c5d389837783becd9b95ab09b78baf40e94e39e2b7e590e470e04",
        "size": 4066771,
        "source_media_sha256": "6ea0cc68d3af37203a885032eddf7c28e839e6abb59d8c9cd3792f1308bdec38",
        "source_stage_sha256": "a82c0d6a12e156e0832d632a6c40dd58713a00b611dbcba7289aa16b0969a0a6",
    },
    "deuteros-amiga-en-title-bridge-v3": {
        "game": "deuteros", "platform": "amiga", "language": "en",
        "sha256": "f4dc8dd1c27c5d389837783becd9b95ab09b78baf40e94e39e2b7e590e470e04",
        "size": 4066771,
        "source_media_sha256": "6ea0cc68d3af37203a885032eddf7c28e839e6abb59d8c9cd3792f1308bdec38",
        "source_stage_sha256": "48d65260e9b5f5cbf8d8b3675a178c81b8764810b61a6a2539a56dcb40a8de03",
    },
}
V4_ADAPTERS = {
    "deuteros-amiga-en-title-display-v4": {
        "game": "deuteros", "platform": "amiga", "language": "en",
        "sha256": "f4dc8dd1c27c5d389837783becd9b95ab09b78baf40e94e39e2b7e590e470e04",
        "size": 4066771,
        "source_media_sha256": "6ea0cc68d3af37203a885032eddf7c28e839e6abb59d8c9cd3792f1308bdec38",
        "source_stage_sha256": "48d65260e9b5f5cbf8d8b3675a178c81b8764810b61a6a2539a56dcb40a8de03",
    },
}
V5_ADAPTERS = {
    "deuteros-amiga-en-title-display-artifacts-v5": {
        "game": "deuteros", "platform": "amiga", "language": "en",
        "sha256": "f4dc8dd1c27c5d389837783becd9b95ab09b78baf40e94e39e2b7e590e470e04",
        "size": 4066771,
        "source_media_sha256": "6ea0cc68d3af37203a885032eddf7c28e839e6abb59d8c9cd3792f1308bdec38",
        "source_stage_sha256": "48d65260e9b5f5cbf8d8b3675a178c81b8764810b61a6a2539a56dcb40a8de03",
    },
}
V5_ARTIFACTS = (
    # These are external observation files, not original game media. The
    # assembler only copies them into a new evidence receipt after securing
    # and rehashing each one; Project Eon later cross-binds their identities
    # to the independently parsed v5 event checkpoints.
    ("copper_list", "copper-list.bin", 88),
    ("rgb4_palette", "palette-rgb4.bin", 40),
    ("bitplanes", "bitplanes.bin", 32000),
    ("rgba_palette", "palette-rgba8888.bin", 80),
    ("rgba_frame", "frame-rgba8888.bin", 256000),
    ("pcm", "audio-s16le.bin", MAX_PROVENANCE_ARTIFACT_SIZE),
)


def registered_adapters() -> dict[str, tuple[str, dict[str, object]]]:
    """Return every exact capture-admission row, labelled by trace format.

    This registry is deliberately the same fixed provenance information used
    by assembly.  It has no recorder backend and contains no event payload,
    original byte, or runtime result.
    """
    return {
        **{name: ("project-eon-reference-trace-v2", details)
           for name, details in V2_ADAPTERS.items()},
        **{name: ("project-eon-reference-trace-v3", details)
           for name, details in V3_ADAPTERS.items()},
        **{name: ("project-eon-reference-trace-v4", details)
           for name, details in V4_ADAPTERS.items()},
        **{name: ("project-eon-reference-trace-v5", details)
           for name, details in V5_ADAPTERS.items()},
    }


def metadata_template(adapter: str) -> str:
    """Render an instructional template; it is not valid capture metadata.

    Angle-bracket values force the recorder operator to retain and hash the
    actual external configuration, command and input timeline.  The template
    must never be passed back to the assembler unchanged.
    """
    registered = registered_adapters()
    if adapter not in registered:
        raise EvidenceError("metadata template requires a registered adapter")
    format_name, identity = registered[adapter]
    rows = [
        ("format", format_name),
        ("adapter", adapter),
        ("game", str(identity["game"])),
        ("platform", str(identity["platform"])),
        ("language", str(identity["language"])),
    ]
    if "source_media_sha256" in identity:
        rows.extend((
            ("source_media_sha256", str(identity["source_media_sha256"])),
            ("source_stage_sha256", str(identity["source_stage_sha256"])),
        ))
    rows.extend((
        ("capture_start_utc", "<actual-utc-YYYY-MM-DDTHH:MM:SSZ>"),
        ("capture_end_utc", "<actual-utc-YYYY-MM-DDTHH:MM:SSZ>"),
        ("emulator_name", "<external-recorder-name>"),
        ("emulator_version", "<external-recorder-version>"),
        ("emulator_sha256", "<actual-lowercase-sha256>"),
        ("config_sha256", "<actual-lowercase-sha256>"),
        ("command_tail_sha256", "<actual-lowercase-sha256>"),
        ("input_timeline_sha256", "<actual-lowercase-sha256>"),
    ))
    return "".join(f"{key}\t{value}\n" for key, value in rows)


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


def require_absolute_directory(path: Path, label: str) -> os.stat_result:
    if not path.is_absolute():
        raise EvidenceError(f"{label} path must be absolute")
    try:
        info = path.lstat()
    except OSError as error:
        raise EvidenceError(f"Unable to stat {label}: {error}") from error
    if stat.S_ISLNK(info.st_mode) or not stat.S_ISDIR(info.st_mode):
        raise EvidenceError(f"{label} must be a non-symlink directory")
    return info


def secure_open(path: Path) -> int:
    # Windows opens file descriptors in text mode unless O_BINARY is stated.
    # Text-mode CRLF translation makes the byte count disagree with stat(),
    # which is fatal for evidence hashing and made the cross-platform tests
    # reject otherwise valid LF metadata. POSIX exposes no O_BINARY, so zero
    # keeps this exact byte-open contract portable.
    flags = (os.O_RDONLY | getattr(os, "O_BINARY", 0) | getattr(os, "O_CLOEXEC", 0)
             | getattr(os, "O_NOFOLLOW", 0))
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
        return read_opened_input(fd, initial, label)
    finally:
        os.close(fd)


def read_opened_input(fd: int, expected: os.stat_result, label: str) -> bytes:
    """Read one already identity-checked descriptor without a second pathname lookup."""
    os.lseek(fd, 0, os.SEEK_SET)
    chunks: list[bytes] = []
    remaining = expected.st_size
    while remaining:
        chunk = os.read(fd, min(1024 * 1024, remaining))
        if not chunk:
            raise EvidenceError(f"Unable to read complete {label}")
        chunks.append(chunk)
        remaining -= len(chunk)
    if os.read(fd, 1) or not same_file_identity(expected, os.fstat(fd)):
        raise EvidenceError(f"{label} changed while it was read")
    os.lseek(fd, 0, os.SEEK_SET)
    return b"".join(chunks)


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


def parse_metadata_bytes(data: bytes) -> dict[str, str]:
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


def parse_metadata(path: Path) -> dict[str, str]:
    return parse_metadata_bytes(read_checked_input(path, "metadata", MAX_METADATA_SIZE))


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
    elif version == "project-eon-reference-trace-v4":
        adapter = fields.get("adapter")
        if adapter not in V4_ADAPTERS:
            raise EvidenceError("v4 metadata must name a registered adapter")
        expected = COMMON_METADATA | {"adapter", "source_media_sha256", "source_stage_sha256"}
        for key, expected_value in V4_ADAPTERS[adapter].items():
            if key in {"sha256", "size"}:
                if release.get(key) != expected_value:
                    raise EvidenceError(f"adapter does not match its exact source {key}")
                continue
            if fields.get(key) != expected_value:
                raise EvidenceError(f"adapter does not match its exact {key}")
    elif version == "project-eon-reference-trace-v5":
        adapter = fields.get("adapter")
        if adapter not in V5_ADAPTERS:
            raise EvidenceError("v5 metadata must name a registered adapter")
        expected = COMMON_METADATA | {"adapter", "source_media_sha256", "source_stage_sha256"}
        for key, expected_value in V5_ADAPTERS[adapter].items():
            if key in {"sha256", "size"}:
                if release.get(key) != expected_value:
                    raise EvidenceError(f"adapter does not match its exact source {key}")
                continue
            if fields.get(key) != expected_value:
                raise EvidenceError(f"adapter does not match its exact {key}")
    else:
        raise EvidenceError("format must be project-eon-reference-trace-v1, -v2, -v3, -v4, or -v5")
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


def reject_output_path(inputs: tuple[Path, ...], output: Path) -> None:
    # Preserve this lexical safety contract before Windows can classify a
    # POSIX-looking /tmp path as current-drive relative.
    normalized_output = output.as_posix().replace("\\", "/")
    output_parts = output.parts
    is_system_tmp = (normalized_output == "/tmp" or normalized_output.startswith("/tmp/")
                     or bool(output.anchor and len(output_parts) > 1
                             and output_parts[1].casefold() == "tmp"))
    if is_system_tmp:
        raise EvidenceError("output must not use /tmp; use a Project Eon cache path")
    if not output.is_absolute():
        raise EvidenceError("output path must be absolute")
    if output.exists() or output.is_symlink():
        raise EvidenceError("output directory must not exist")
    # Trace assembly creates a private staging directory beside the requested
    # receipt. Keep that transient work in the caller's Project Eon cache as
    # well: an output below /tmp would violate the no-system-temporary-files
    # contract even though the final receipt itself is user-owned evidence.
    # macOS resolves `/tmp` under `/private`, and Windows maps a POSIX-looking
    # `/tmp/...` path to the current drive. Preserve the operator's lexical
    # route before resolution so this contract has identical force on every
    # supported host.
    resolved = output.resolve(strict=False)
    # Inputs are regular files, so a distinct non-existent directory cannot
    # overlap their bytes.  Deliberately permit a sibling capture directory:
    # a user commonly keeps a read-only archive and its separately owned
    # evidence under one collection root.
    if output in inputs:
        raise EvidenceError("output directory must not name an input file")


def copy_checked_input(input_fd: int, expected: os.stat_result, output_path: Path,
                       label: str, maximum: int) -> tuple[str, int]:
    digest = hashlib.sha256()
    total = 0
    try:
        with os.fdopen(input_fd, "rb", closefd=False) as source, output_path.open("xb") as destination:
            while True:
                chunk = source.read(1024 * 1024)
                if not chunk:
                    break
                total += len(chunk)
                if total > maximum:
                    raise EvidenceError(f"{label} exceeds {maximum} bytes")
                digest.update(chunk)
                destination.write(chunk)
            destination.flush()
            os.fsync(destination.fileno())
    finally:
        os.lseek(input_fd, 0, os.SEEK_SET)
    if total == 0:
        raise EvidenceError(f"{label} must not be empty")
    if total != expected.st_size or not same_file_identity(expected, os.fstat(input_fd)):
        raise EvidenceError(f"{label} changed while it was copied")
    return digest.hexdigest(), total


def write_text_atomic(path: Path, content: str) -> None:
    temporary = path.with_name(path.name + ".tmp")
    # Windows rejects fsync on a read-only CRT descriptor (EBADF), even
    # though POSIX accepts it. Keep the durable write and the sync on the
    # same writable handle so capture assembly has identical semantics on all
    # supported hosts. ``x`` also makes an unexpected temporary path a hard
    # failure rather than replacing an operator-owned file.
    with temporary.open("x", encoding="utf-8", newline="\n") as stream:
        stream.write(content)
        stream.flush()
        os.fsync(stream.fileno())
    temporary.replace(path)


def assemble(args: argparse.Namespace) -> Path:
    source = Path(args.source_release)
    events = Path(args.events)
    metadata = Path(args.metadata)
    configuration = Path(args.config)
    command_tail = Path(args.command_tail)
    input_timeline = Path(args.input_timeline)
    title_display_artifacts_arg = getattr(args, "title_display_artifacts", None)
    title_display_artifacts = (Path(title_display_artifacts_arg)
                               if title_display_artifacts_arg else None)
    output = Path(args.output)
    source_initial = require_absolute_regular_file(source, "source release")
    event_initial = require_absolute_regular_file(events, "event stream", MAX_EVENT_SIZE)
    metadata_initial = require_absolute_regular_file(metadata, "metadata", MAX_METADATA_SIZE)
    configuration_initial = require_absolute_regular_file(
        configuration, "configuration", MAX_PROVENANCE_ARTIFACT_SIZE)
    command_tail_initial = require_absolute_regular_file(
        command_tail, "command tail", MAX_PROVENANCE_ARTIFACT_SIZE)
    input_timeline_initial = require_absolute_regular_file(
        input_timeline, "input timeline", MAX_PROVENANCE_ARTIFACT_SIZE)
    inputs = (source, events, metadata, configuration, command_tail, input_timeline)
    identities = ((source_initial.st_dev, source_initial.st_ino),
                  (event_initial.st_dev, event_initial.st_ino),
                  (metadata_initial.st_dev, metadata_initial.st_ino),
                  (configuration_initial.st_dev, configuration_initial.st_ino),
                  (command_tail_initial.st_dev, command_tail_initial.st_ino),
                  (input_timeline_initial.st_dev, input_timeline_initial.st_ino))
    if identities[0] == identities[1]:
        raise EvidenceError("event stream must not be the original source release")
    if len(set(identities)) != len(identities):
        raise EvidenceError("source, events, metadata, configuration, command tail, and input timeline must be distinct files")
    reject_output_path(inputs, output)
    source_fd = secure_open(source)
    event_fd = -1
    metadata_fd = -1
    configuration_fd = -1
    command_tail_fd = -1
    input_timeline_fd = -1
    v5_artifact_fds: list[int] = []
    try:
        source_hash, source_size, source_before = hash_fd(source_fd)
        if not stat.S_ISREG(source_before.st_mode) or not same_file_identity(source_initial, source_before):
            raise EvidenceError("source release changed while it was opened")
        release = release_identity(source_hash, source_size)
        metadata_fd, opened_metadata = open_checked_input(metadata, "metadata", MAX_METADATA_SIZE)
        if not same_file_identity(metadata_initial, opened_metadata):
            raise EvidenceError("metadata changed while it was opened")
        metadata_preimage_hash, _, hashed_metadata = hash_fd(metadata_fd)
        if not same_file_identity(opened_metadata, hashed_metadata):
            raise EvidenceError("metadata changed while it was hashed")
        fields = parse_metadata_bytes(read_opened_input(metadata_fd, opened_metadata, "metadata"))
        validate_metadata(fields, release)
        v5_artifact_inputs: list[tuple[str, str, int, os.stat_result, str, int]] = []
        if fields["format"] == "project-eon-reference-trace-v5":
            if title_display_artifacts is None:
                raise EvidenceError("v5 title-display assembly requires --title-display-artifacts")
            require_absolute_directory(title_display_artifacts, "title-display artifacts")
            for field, filename, maximum in V5_ARTIFACTS:
                path = title_display_artifacts / filename
                initial = require_absolute_regular_file(path, f"v5 {field} artifact", maximum)
                descriptor, opened = open_checked_input(path, f"v5 {field} artifact", maximum)
                if not same_file_identity(initial, opened):
                    os.close(descriptor)
                    raise EvidenceError(f"v5 {field} artifact changed while it was opened")
                digest, size, hashed = hash_fd(descriptor)
                if not same_file_identity(opened, hashed):
                    os.close(descriptor)
                    raise EvidenceError(f"v5 {field} artifact changed while it was hashed")
                v5_artifact_fds.append(descriptor)
                v5_artifact_inputs.append((field, filename, descriptor, opened, digest, size))
            all_identities = list(identities) + [
                (opened.st_dev, opened.st_ino) for _, _, _, opened, _, _ in v5_artifact_inputs]
            if len(set(all_identities)) != len(all_identities):
                raise EvidenceError("v5 artifacts must be distinct from every assembly input")
        elif title_display_artifacts is not None:
            raise EvidenceError("--title-display-artifacts requires v5 metadata")
        event_fd, opened_event = open_checked_input(events, "event stream", MAX_EVENT_SIZE)
        if not same_file_identity(event_initial, opened_event):
            raise EvidenceError("event stream changed while it was opened")
        provenance_inputs: list[tuple[str, int, os.stat_result, str, str]] = []
        for label, path, initial, expected_hash, output_name in (
                ("configuration", configuration, configuration_initial, fields["config_sha256"], "configuration.preimage"),
                ("command tail", command_tail, command_tail_initial, fields["command_tail_sha256"], "command-tail.preimage"),
                ("input timeline", input_timeline, input_timeline_initial,
                 fields["input_timeline_sha256"],
                 "input-timeline.txt" if fields["format"] == "project-eon-reference-trace-v5"
                 else "input-timeline.preimage")):
            descriptor, opened = open_checked_input(path, label, MAX_PROVENANCE_ARTIFACT_SIZE)
            if not same_file_identity(initial, opened):
                os.close(descriptor)
                raise EvidenceError(f"{label} changed while it was opened")
            digest, size, hashed = hash_fd(descriptor)
            if not same_file_identity(opened, hashed) or digest != expected_hash:
                os.close(descriptor)
                raise EvidenceError(f"{label} SHA-256 does not match metadata")
            provenance_inputs.append((label, descriptor, opened, digest, output_name))
            if label == "configuration":
                configuration_fd = descriptor
            elif label == "command tail":
                command_tail_fd = descriptor
            else:
                input_timeline_fd = descriptor
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
            event_hash, event_size = copy_checked_input(
                event_fd, opened_event, staging / "events.eontrace", "event stream", MAX_EVENT_SIZE)
            metadata_hash, metadata_size = copy_checked_input(
                metadata_fd, opened_metadata, staging / "capture-metadata.tsv", "metadata", MAX_METADATA_SIZE)
            if metadata_hash != metadata_preimage_hash:
                raise EvidenceError("metadata changed after it was parsed")
            provenance_receipt: dict[str, dict[str, object]] = {
                "metadata": {"path": "capture-metadata.tsv", "sha256": metadata_hash, "size": metadata_size},
            }
            for label, descriptor, opened, digest, output_name in provenance_inputs:
                copied_hash, copied_size = copy_checked_input(
                    descriptor, opened, staging / output_name, label, MAX_PROVENANCE_ARTIFACT_SIZE)
                if copied_hash != digest:
                    raise EvidenceError(f"{label} changed after its hash was checked")
                provenance_receipt[label.replace(" ", "_")] = {
                    "path": output_name, "sha256": copied_hash, "size": copied_size}
            artifact_manifest_fields: dict[str, str] = {}
            for field, filename, descriptor, opened, digest, size in v5_artifact_inputs:
                copied_hash, copied_size = copy_checked_input(
                    descriptor, opened, staging / filename, f"v5 {field} artifact",
                    MAX_PROVENANCE_ARTIFACT_SIZE)
                if copied_hash != digest or copied_size != size:
                    raise EvidenceError(f"v5 {field} artifact changed after its hash was checked")
                artifact_manifest_fields.update({
                    f"{field}_file": filename,
                    f"{field}_size": str(copied_size),
                    f"{field}_sha256": copied_hash,
                })
                provenance_receipt[f"v5_{field}"] = {
                    "path": filename, "sha256": copied_hash, "size": copied_size}
            source_after_hash, source_after_size, source_after = hash_fd(source_fd)
            if (source_hash, source_size, source_before.st_dev, source_before.st_ino) != (
                source_after_hash, source_after_size, source_after.st_dev, source_after.st_ino):
                raise EvidenceError("source release changed during trace assembly; no receipt issued")
            manifest_fields = dict(fields)
            manifest_fields.update({
                "event_file": "events.eontrace", "event_size": str(event_size), "event_sha256": event_hash,
                "source_release_sha256": source_hash, "source_release_size": str(source_size),
            })
            if fields["format"] == "project-eon-reference-trace-v5":
                manifest_fields["input_timeline_file"] = "input-timeline.txt"
                manifest_fields["input_timeline_size"] = str(
                    provenance_receipt["input_timeline"]["size"])
                manifest_fields.update(artifact_manifest_fields)
            order = ["format"] + (["adapter"] if "adapter" in manifest_fields else []) + [
                "event_file", "event_size", "event_sha256", "game", "platform", "language",
                "source_release_sha256", "source_release_size",
            ]
            if "source_media_sha256" in manifest_fields:
                order += ["source_media_sha256", "source_stage_sha256"]
            order += ["capture_start_utc", "capture_end_utc", "emulator_name", "emulator_version",
                      "emulator_sha256", "config_sha256", "command_tail_sha256", "input_timeline_sha256"]
            if fields["format"] == "project-eon-reference-trace-v5":
                order += ["input_timeline_file", "input_timeline_size"]
                for field, _, _ in V5_ARTIFACTS:
                    order += [f"{field}_file", f"{field}_size", f"{field}_sha256"]
            manifest_text = "".join(f"{key}\t{manifest_fields[key]}\n" for key in order)
            write_text_atomic(staging / "manifest.eontrace", manifest_text)
            receipt = {
                "schema": "project-eon.reference-trace-receipt/v1",
                "status": "assembled-not-admitted",
                "source_release": {"sha256_before": source_hash, "sha256_after": source_after_hash,
                                   "size_before": source_size, "size_after": source_after_size,
                                   "device": source_before.st_dev, "inode": source_before.st_ino},
                "event": {"path": "events.eontrace", "sha256": event_hash, "size": event_size},
                "provenance": provenance_receipt,
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
        if metadata_fd >= 0:
            os.close(metadata_fd)
        if configuration_fd >= 0:
            os.close(configuration_fd)
        if command_tail_fd >= 0:
            os.close(command_tail_fd)
        if input_timeline_fd >= 0:
            os.close(input_timeline_fd)
        for descriptor in v5_artifact_fds:
            os.close(descriptor)
        os.close(source_fd)
    return output


def parse_arguments(argv: list[str]) -> argparse.Namespace:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--source-release", help="Absolute user-owned original archive path")
    parser.add_argument("--events", help="Absolute external event stream path")
    parser.add_argument("--metadata", help="Absolute LF key<TAB>value metadata path")
    parser.add_argument("--config", help="Absolute external emulator configuration preimage")
    parser.add_argument("--command-tail", help="Absolute literal emulator command-tail preimage")
    parser.add_argument("--input-timeline", help="Absolute recorder input-timeline preimage")
    parser.add_argument("--title-display-artifacts",
                        help="Absolute directory holding the seven fixed v5 title-display capture artifacts")
    parser.add_argument("--output", help="New absolute evidence directory")
    parser.add_argument("--metadata-template", metavar="ADAPTER",
                        help="Print a non-validating instructional metadata template for one registered adapter")
    args = parser.parse_args(argv)
    if args.metadata_template:
        if any((args.source_release, args.events, args.metadata, args.config, args.command_tail,
                args.input_timeline, args.title_display_artifacts, args.output)):
            parser.error("--metadata-template cannot be combined with assembly inputs")
    elif not all((args.source_release, args.events, args.metadata, args.config, args.command_tail,
                  args.input_timeline, args.output)):
        parser.error("assembly requires --source-release, --events, --metadata, --config, --command-tail, --input-timeline and --output")
    return args


def main(argv: list[str] | None = None) -> int:
    try:
        args = parse_arguments(sys.argv[1:] if argv is None else argv)
        if args.metadata_template:
            print(metadata_template(args.metadata_template), end="")
            return 0
        output = assemble(args)
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
