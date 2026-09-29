"""Isolated experimental terminal recorder contract; never recovery admission."""
from __future__ import annotations

from decimal import Decimal, InvalidOperation
import hashlib
import importlib.util
import os
from pathlib import Path, PurePosixPath
import re
import stat

CAPTURE_RECEIPT_VERSION = "24"
RECORDER_PROTOCOL = "v24-terminal-int6"
RECORDER_ADMISSION = "experimental-observer-not-for-recovery"
EXPECTED_RECORDER_SHA256 = "776a02c687951fdc88a65834719ba7cf0124c879c371b278f8117f203cd4375f"
EXPECTED_RECORDER_SIZE = 132_945_208
SCHEMA_VERSION = CAPTURE_RECEIPT_VERSION
PROTOCOL = RECORDER_PROTOCOL
RECORDER_SHA256 = EXPECTED_RECORDER_SHA256
RECORDER_SIZE = EXPECTED_RECORDER_SIZE
OUTPUT_ROOT = PurePosixPath("/home/trv2/.cache/project-eon-tools/recorder-recovery-20260929/terminal-captures")
SUCCESS_MARKER = b"EON_TERMINAL_DEVELOPMENT_WRITE_OK\n"
CONFIGURATION_NAME = "dosbox-x-terminal.conf"
MAX_CONSOLE_BYTES = 64 * 1024 * 1024
MAX_HOST_INPUT_BYTES = 64 * 1024
MAX_INT6_BYTES = 2048
MAX_CONFIGURATION_BYTES = 64 * 1024
MAX_DURATION_SECONDS = 600
MIN_DURATION_SECONDS = 15
POST_EXIT_ALLOWANCE_SECONDS = 30
ARTIFACTS = {
    "configuration": (CONFIGURATION_NAME, MAX_CONFIGURATION_BYTES),
    "int6_observation": ("int6-observation.raw", MAX_INT6_BYTES),
    "host_input_receipt": ("host-input-receipt.raw", MAX_HOST_INPUT_BYTES),
    "recorder_console": ("recorder-console.log", MAX_CONSOLE_BYTES),
}
SCALAR_NAMES = ("prefix_cs", "prefix_ip", "interrupt", "callback_cs", "callback_ip",
                "stub_ip", "callback_index", "ss", "sp", "return_ip", "return_cs",
                "return_flags", "ax", "bx", "cx", "dx")
INT6_LINE = re.compile("eon-int6-development-v1" + "".join(
    rf"\t{name}=([0-9a-f]{{4}})" for name in SCALAR_NAMES) + r"\n")
DECIMAL = re.compile(r"(?:0|[1-9][0-9]*)(?:\.[0-9]+)?")
UNSIGNED = re.compile(r"(?:0|[1-9][0-9]*)")
FIELDS = {
    "capture_receipt_version", "recorder_protocol", "recorder_admission", "capture_directory",
    "machine_profile", "capture_intent", "input_origin", "input_timestamp", "input_scope",
    "exit_status", "termination_reason", "host_input_records", "start_unix", "end_unix",
    "max_duration_seconds", "environment_policy", "observer_source_commit",
    "observer_patch_sha256", "operator_procedure",
} | {f"{prefix}_{suffix}" for prefix in ("source_release", "recorder", *ARTIFACTS)
     for suffix in ("sha256", "bytes")}


def _legacy():
    path = Path(__file__).with_name("run_millennium_dos_capture.py")
    spec = importlib.util.spec_from_file_location("eon_terminal_legacy", path)
    assert spec and spec.loader
    module = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(module)
    return module


def _capture_path(path: Path | PurePosixPath) -> PurePosixPath:
    spelling = str(path)
    path = PurePosixPath(spelling)
    if (not path.is_absolute() or path.parent != OUTPUT_ROOT
            or str(path) != spelling or "\\" in spelling
            or not re.fullmatch(r"[A-Za-z0-9][A-Za-z0-9._-]*", path.name)):
        raise ValueError("capture directory must be a named child of the reviewed external root")
    return path


def build_configuration(capture_directory: Path | PurePosixPath, machine_profile: str) -> str:
    directory = _capture_path(capture_directory)
    legacy = _legacy()
    base = legacy.recorder_config(directory / "archive-ro" / legacy.GAME_ROOT,
                                  machine_profile, release_sha256=legacy.EXPECTED_RELEASE_SHA256)
    # The legacy generator finishes in this recorder-only section.
    return (base + f"terminal_output_path={directory / 'int6-observation.raw'}\n"
            + f"host_input_output_path={directory / 'host-input-receipt.raw'}\n"
            + "\n[dosbox]\nquit warning=false\n")


def read_bounded(path: Path, limit: int) -> bytes:
    """Read through a checked descriptor; reject symlinks, growth and truncation."""
    if not path.is_absolute():
        raise ValueError("artifact path must be absolute")
    for component in (path, *path.parents):
        if component.is_symlink():
            raise ValueError("artifact path must not contain symlinks")
    fd = os.open(path, os.O_RDONLY | getattr(os, "O_NOFOLLOW", 0) | getattr(os, "O_NONBLOCK", 0))
    try:
        before = os.fstat(fd)
        if not stat.S_ISREG(before.st_mode) or before.st_size > limit:
            raise ValueError("artifact must be a bounded regular file")
        with os.fdopen(fd, "rb", closefd=False) as stream:
            data = stream.read(limit + 1)
        after = os.fstat(fd)
        if (len(data) != before.st_size or len(data) > limit
                or (before.st_size, before.st_mtime_ns, before.st_ctime_ns)
                != (after.st_size, after.st_mtime_ns, after.st_ctime_ns)):
            raise ValueError("artifact changed while being read")
        return data
    finally:
        os.close(fd)


def parse_int6(data: bytes) -> dict[str, int]:
    if len(data) > MAX_INT6_BYTES:
        raise ValueError("INT6 observation exceeds its bound")
    match = INT6_LINE.fullmatch(data.decode("ascii"))
    if not match:
        raise ValueError("INT6 observation has an invalid development grammar")
    values = dict(zip(SCALAR_NAMES, (int(value, 16) for value in match.groups())))
    expected = {"prefix_ip": 0x134, "interrupt": 6, "callback_cs": 0xf000,
                "callback_ip": 0xca64, "stub_ip": 0xca60, "callback_index": 3}
    if any(values[key] != value for key, value in expected.items()):
        raise ValueError("INT6 observation is outside the reviewed boundary")
    return values


read_regular = read_bounded
validate_capture_directory = _capture_path


def file_identity(path: Path, limit: int) -> tuple[str, int]:
    data = read_bounded(path, limit)
    return hashlib.sha256(data).hexdigest(), len(data)


def parse_host_input(data: bytes) -> int:
    if len(data) > MAX_HOST_INPUT_BYTES:
        raise ValueError("host input exceeds its bound")
    if not data:
        return 0
    matcher = _legacy().HOST_KEY_LINE
    lines = data.decode("ascii").splitlines(keepends=True)
    if len(lines) > 256:
        raise ValueError("host input exceeds its record cap")
    for ordinal, line in enumerate(lines, 1):
        match = matcher.fullmatch(line)
        if not match or int(match.group(1)) != ordinal:
            raise ValueError("host input grammar or ordinal is invalid")
        if (int(match.group(2)) > 0xffffffff
                or any(int(match.group(index), 16) > 0xffffffff for index in (4, 5))
                or int(match.group(6), 16) > 0xffff):
            raise ValueError("host input field exceeds its SDL uint32 representation")
    return len(lines)


def _identity(fields: dict[str, str], prefix: str, digest: str, size: int) -> None:
    if (fields.get(prefix + "_sha256"), fields.get(prefix + "_bytes")) != (digest, str(size)):
        raise ValueError(f"{prefix} identity mismatch")


def verify_fields(fields: dict[str, str], directory: Path,
                  allow_experimental_observer: bool = False) -> None:
    if not allow_experimental_observer:
        raise ValueError("experimental terminal observer is not admitted for recovery")
    if set(fields) != FIELDS or any(not isinstance(value, str) for value in fields.values()):
        raise ValueError("terminal receipt fields do not match its exact schema")
    required = {"capture_receipt_version": CAPTURE_RECEIPT_VERSION,
                "recorder_protocol": RECORDER_PROTOCOL, "recorder_admission": RECORDER_ADMISSION,
                "input_origin": "unclassified-sdl-queue", "input_timestamp": "sdl2-key.timestamp-u32",
                "input_scope": "guest-lifetime-including-internal-reboots",
                "environment_policy": "isolated-xdg-no-overrides-v1",
                "observer_source_commit": "234797680781567e18c374c9e62da24de5423db0",
                "observer_patch_sha256": "23951d5c7cab7d18206f7f15eac352bc2901ab8bbd56b9693e621796aeb9efe5",
                "operator_procedure": "visible-manual-window-close",
                "exit_status": "0", "termination_reason": "emulator-exit"}
    if any(fields[key] != value for key, value in required.items()):
        raise ValueError("terminal receipt does not establish successful experimental shutdown")
    original = _capture_path(PurePosixPath(fields["capture_directory"]))
    if str(original) != fields["capture_directory"]:
        raise ValueError("capture directory spelling must be canonical")
    legacy = _legacy()
    _identity(fields, "source_release", legacy.EXPECTED_RELEASE_SHA256, legacy.EXPECTED_RELEASE_SIZE)
    _identity(fields, "recorder", EXPECTED_RECORDER_SHA256, EXPECTED_RECORDER_SIZE)
    raw_duration = fields["max_duration_seconds"]
    if (len(raw_duration) > 3 or not UNSIGNED.fullmatch(raw_duration)
            or not MIN_DURATION_SECONDS <= int(raw_duration) <= MAX_DURATION_SECONDS):
        raise ValueError("capture duration is outside its bound")
    times = []
    for key in ("start_unix", "end_unix"):
        if not DECIMAL.fullmatch(fields[key]) or len(fields[key]) > 40:
            raise ValueError("capture time must be a finite nonnegative decimal")
        try:
            times.append(Decimal(fields[key]))
        except InvalidOperation as error:
            raise ValueError("invalid capture time") from error
    if not 0 <= times[1] - times[0] <= int(raw_duration) + POST_EXIT_ALLOWANCE_SECONDS:
        raise ValueError("capture elapsed time is inconsistent with its bound")
    artifacts = {}
    for prefix, (name, limit) in ARTIFACTS.items():
        payload = read_bounded(directory / name, limit)
        _identity(fields, prefix, hashlib.sha256(payload).hexdigest(), len(payload))
        artifacts[prefix] = payload
    expected_configuration = build_configuration(original, fields["machine_profile"]).encode("utf-8")
    if artifacts["configuration"] != expected_configuration:
        raise ValueError("configuration differs from the reviewed generation contract")
    parse_int6(artifacts["int6_observation"])
    records = parse_host_input(artifacts["host_input_receipt"])
    if fields["host_input_records"] != str(records):
        raise ValueError("host input count mismatch")
    intent = fields["capture_intent"]
    if ((intent == "operator-input" and records == 0)
            or (intent == "diagnostic-no-key-delivery" and records != 0)
            or intent not in {"operator-input", "diagnostic-no-key-delivery"}):
        raise ValueError("capture intent does not match observed SDL queue records")
    console = artifacts["recorder_console"]
    marker_at = console.find(SUCCESS_MARKER)
    if (console.count(SUCCESS_MARKER) != 1
            or (marker_at > 0 and console[marker_at - 1] != 0x0a)):
        raise ValueError("full console must retain exactly one terminal success marker")
