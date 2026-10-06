"""Strict parser and receipt contract for the experimental DOS origin observer.

The v2 format binds the first CS=0e70 fetch context to a preceding, exact
TITLES.EXE DOS LOADNGO identity when present. It does not establish causal
transfer, original instruction provenance, guest memory contents, or gameplay.
"""

from __future__ import annotations

import hashlib
import importlib.util
import os
from pathlib import Path
import re
import stat


RECORDER_SHA256 = "2e03888c5a6d56f0707030ac29a1774223905cd1c43c7cfb114dc1979ac6601f"
RECORDER_BYTES = 132_982_144
PATCH_SHA256 = "3984407abea160f97be42e134b400614163c78800c7e217d113c7ec4d861208e"
SOURCE_RELEASE_SHA256 = "e6e7044b25877fdf8b10d16d2f395886d9957953144ae15ca630cda9cab2a123"
SOURCE_RELEASE_BYTES = 328_383
RECEIPT_MARKER = "EON_DOS_ORIGIN_EXPERIMENT_WRITE_OK"
SCHEMA = "eon-dos-origin-experiment-v2"
GAME_ROOT = "millennium-return-to-earth-2-2"
TITLES_SHA256 = "3cc57f2b12a0da44dd43220f44f06a05b9e3f009bcf008b7bb87622a5988cbe6"
TITLES_BYTES = 7_022
MAX_RAW_BYTES = 512
LINE = re.compile(
    rb"eon-dos-origin-experiment-v2\t"
    rb"title_valid=([01])\ttitle_entry_cs=([0-9a-f]{4})\ttitle_entry_ip=([0-9a-f]{4})\t"
    rb"title_size=([0-9a-f]{8})\ttitle_sha256=([0-9a-f]{64})\t"
    rb"fetches_after_title_load=(0|[1-9][0-9]{0,9})\t"
    rb"previous_valid=([01])\tprevious_cs=([0-9a-f]{4})\tprevious_ip=([0-9a-f]{4})\t"
    rb"previous_opcode_valid=([01])\tprevious_opcode=([0-9a-f]{2})\t"
    rb"current_cs=([0-9a-f]{4})\tcurrent_ip=([0-9a-f]{4})\t"
    rb"current_opcode_valid=([01])\tcurrent_opcode=([0-9a-f]{2})\t"
    rb"transfer_class=(no-predecessor|same-segment-entry|cross-segment-entry)\n"
)


class ExperimentError(ValueError):
    """Malformed or unsafe experimental evidence."""


def sha256_file(path: Path) -> tuple[str, int]:
    digest = hashlib.sha256()
    size = 0
    with path.open("rb") as stream:
        for chunk in iter(lambda: stream.read(1024 * 1024), b""):
            size += len(chunk)
            digest.update(chunk)
    return digest.hexdigest(), size


def parse_raw(data: bytes) -> dict[str, int | str]:
    if not data or len(data) > MAX_RAW_BYTES:
        raise ExperimentError("origin receipt is empty or exceeds the bounded schema")
    match = LINE.fullmatch(data)
    if not match:
        raise ExperimentError("origin receipt does not match the exact v1 grammar")
    (title_valid, title_entry_cs, title_entry_ip, title_size, title_sha256,
     fetches_after_title_load, previous_valid, previous_cs, previous_ip, previous_opcode_valid, previous_opcode,
     current_cs, current_ip, current_opcode_valid, current_opcode, transfer_class) = match.groups()
    values: dict[str, int | str] = {
        "title_valid": int(title_valid),
        "title_entry_cs": int(title_entry_cs, 16),
        "title_entry_ip": int(title_entry_ip, 16),
        "title_size": int(title_size, 16),
        "title_sha256": title_sha256.decode("ascii"),
        "fetches_after_title_load": int(fetches_after_title_load),
        "previous_valid": int(previous_valid),
        "previous_cs": int(previous_cs, 16),
        "previous_ip": int(previous_ip, 16),
        "previous_opcode_valid": int(previous_opcode_valid),
        "previous_opcode": int(previous_opcode, 16),
        "current_cs": int(current_cs, 16),
        "current_ip": int(current_ip, 16),
        "current_opcode_valid": int(current_opcode_valid),
        "current_opcode": int(current_opcode, 16),
        "transfer_class": transfer_class.decode("ascii"),
    }
    if values["title_valid"] != 1:
        raise ExperimentError("record lacks the exact hash-validated TITLES.EXE load")
    if (values["title_size"], values["title_sha256"], values["title_entry_ip"]) != (
            TITLES_BYTES, TITLES_SHA256, 0x0100):
        raise ExperimentError("DOS EXEC title identity or planned entry does not match the exact image")
    if values["current_cs"] != 0x0E70 or values["current_opcode_valid"] != 1:
        raise ExperimentError("record does not contain the completed CS=0e70 opcode fetch")
    if values["previous_valid"] != values["previous_opcode_valid"]:
        raise ExperimentError("predecessor and opcode validity flags disagree")
    if values["previous_valid"] == 0:
        if any(values[key] for key in ("previous_cs", "previous_ip", "previous_opcode")):
            raise ExperimentError("absent predecessor fields must be zero")
        if values["transfer_class"] != "no-predecessor":
            raise ExperimentError("absent predecessor has an invalid class")
    else:
        expected = ("same-segment-entry" if values["previous_cs"] == values["current_cs"]
                    else "cross-segment-entry")
        if values["transfer_class"] != expected:
            raise ExperimentError("predecessor segment does not match the finite class")
    return values


def read_raw(path: Path) -> tuple[bytes, dict[str, int | str]]:
    info = path.lstat()
    if stat.S_ISLNK(info.st_mode) or not stat.S_ISREG(info.st_mode):
        raise ExperimentError("raw origin receipt must be a regular non-symlink file")
    if info.st_size > MAX_RAW_BYTES:
        raise ExperimentError("raw origin receipt exceeds the bounded schema")
    fd = os.open(path, os.O_RDONLY | getattr(os, "O_NOFOLLOW", 0) | getattr(os, "O_NONBLOCK", 0))
    with os.fdopen(fd, "rb") as stream:
        if not stat.S_ISREG(os.fstat(stream.fileno()).st_mode):
            raise ExperimentError("raw origin receipt changed file type while opening")
        data = stream.read(MAX_RAW_BYTES + 1)
    return data, parse_raw(data)


def verify_run(status_path: Path) -> dict[str, str]:
    """Verify run metadata and success marker before trusting a raw record."""
    if status_path.is_symlink() or not status_path.is_file():
        raise ExperimentError("experiment-status.txt must be a regular non-symlink file")
    fields: dict[str, str] = {}
    status, _ = read_bounded_file(status_path, 16 * 1024)
    if not status.endswith(b"\n") or b"\r" in status:
        raise ExperimentError("experiment status is not canonical LF-terminated ASCII")
    for line in status.decode("ascii").splitlines():
        if line.count("=") != 1:
            raise ExperimentError("experiment status has an invalid line")
        key, value = line.split("=", 1)
        if not key or not value or key in fields:
            raise ExperimentError("experiment status has an empty or duplicate field")
        fields[key] = value
    expected_keys = {
        "schema", "admission", "recorder_sha256", "recorder_bytes", "patch_sha256",
        "source_release_sha256", "source_release_bytes", "configuration_sha256", "configuration_bytes",
        "raw_receipt", "raw_sha256", "raw_bytes", "console_sha256", "console_bytes",
        "exit_status", "success_marker", "record_status",
    }
    if set(fields) != expected_keys:
        raise ExperimentError("experiment status fields do not match the exact schema")
    if fields["schema"] != SCHEMA or fields["admission"] != "experimental-only-not-recovery-admissible":
        raise ExperimentError("status does not identify the experimental-only protocol")
    if (fields["recorder_sha256"], fields["recorder_bytes"], fields["patch_sha256"],
            fields["source_release_sha256"], fields["source_release_bytes"]) != (
            RECORDER_SHA256, str(RECORDER_BYTES), PATCH_SHA256,
            SOURCE_RELEASE_SHA256, str(SOURCE_RELEASE_BYTES)):
        raise ExperimentError("recorder, patch, or source identity does not match the experiment contract")
    if fields["exit_status"] != "0" or fields["success_marker"] != "present":
        raise ExperimentError("recorder did not exit successfully with its write-success marker")
    raw_path = status_path.parent / fields["raw_receipt"]
    if raw_path.name != fields["raw_receipt"] or raw_path.name in {"", ".", ".."}:
        raise ExperimentError("raw receipt name must be a local basename")
    raw, _ = read_raw(raw_path)
    console_path = status_path.parent / "recorder-console.log"
    console, _ = read_bounded_file(console_path, 1024 * 1024)
    if RECEIPT_MARKER.encode("ascii") + b"\n" not in console.splitlines(keepends=True):
        raise ExperimentError("console lacks the exact recorder success marker")
    expected = {
        "raw_sha256": hashlib.sha256(raw).hexdigest(),
        "raw_bytes": str(len(raw)),
        "console_sha256": hashlib.sha256(console).hexdigest(),
        "console_bytes": str(len(console)),
    }
    if any(fields[key] != value for key, value in expected.items()):
        raise ExperimentError("raw receipt or console identity does not match status")
    if fields["record_status"] != "complete":
        raise ExperimentError("run was not recorded as complete")
    for key in ("source_release_sha256", "configuration_sha256"):
        if not re.fullmatch(r"[0-9a-f]{64}", fields[key]):
            raise ExperimentError(f"{key} is not a SHA-256 identity")
    configuration, configuration_identity = read_bounded_file(status_path.parent / "recorder.conf", 64 * 1024)
    if (fields["configuration_sha256"], fields["configuration_bytes"]) != (
            configuration_identity[0], str(configuration_identity[1])):
        raise ExperimentError("recorder configuration identity does not match status")
    capture_path = Path(__file__).resolve().parent / "run_millennium_dos_capture.py"
    spec = importlib.util.spec_from_file_location("origin_experiment_capture_contract", capture_path)
    if spec is None or spec.loader is None:
        raise ExperimentError("unable to load the DOS configuration contract")
    capture = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(capture)
    expected_configuration = capture.recorder_config(
        status_path.parent / "archive-ro" / GAME_ROOT)
    anchor = "memsize=16\n"
    if expected_configuration.count(anchor) != 1:
        raise ExperimentError("reviewed DOSBox-X configuration anchor changed")
    expected_configuration = (expected_configuration.replace(
        anchor, anchor + "quit warning=false\n", 1)
        + "[log]\nlogfile=/dev/null\ncpu=never\n").encode("utf-8")
    if configuration != expected_configuration:
        raise ExperimentError("recorder configuration differs from the exact read-only-archive profile")
    return fields


def read_bounded_file(path: Path, maximum: int) -> tuple[bytes, tuple[str, int]]:
    info = path.lstat()
    if stat.S_ISLNK(info.st_mode) or not stat.S_ISREG(info.st_mode) or info.st_size > maximum:
        raise ExperimentError(f"{path.name} is unsafe or exceeds its size limit")
    fd = os.open(path, os.O_RDONLY | getattr(os, "O_NOFOLLOW", 0) | getattr(os, "O_NONBLOCK", 0))
    with os.fdopen(fd, "rb") as stream:
        if not stat.S_ISREG(os.fstat(stream.fileno()).st_mode):
            raise ExperimentError(f"{path.name} changed file type while opening")
        data = stream.read(maximum + 1)
    if len(data) > maximum:
        raise ExperimentError(f"{path.name} exceeds its size limit")
    return data, (hashlib.sha256(data).hexdigest(), len(data))
