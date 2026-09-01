#!/usr/bin/env python3
"""Run a read-only, operator-driven Millennium DOS recorder session.

This helper is not an emulator distribution, input injector, trace assembler,
or runtime component. It prepares a fresh external evidence directory around
an already reviewed DOSBox-X recorder. The operator must interact with the
visible emulator window; headless SDL and DOSBox-X AUTOTYPE are rejected.
"""

from __future__ import annotations

import argparse
import hashlib
import os
from pathlib import Path
import re
import stat
import subprocess
import sys
import threading
import time


ROOT = Path(__file__).resolve().parents[1]
EXPECTED_RELEASE_SHA256 = "e6e7044b25877fdf8b10d16d2f395886d9957953144ae15ca630cda9cab2a123"
EXPECTED_RELEASE_SIZE = 328_383
# The capture helper never accepts a merely executable emulator as a recorder:
# its hooks and input-receipt contract are part of the provenance boundary.
EXPECTED_RECORDER_SHA256 = "7b959f7aee3d2db0513db4f14e3075f306e798e25adaeeebd96aedd81aef65da"
RECORDER_PROTOCOLS = {
    # v11 is the existing exact callback-loop receipt. Its shape remains the
    # default so a new diagnostic cannot silently change prior evidence.
    "v11": ("11", EXPECTED_RECORDER_SHA256),
    # This separately built recorder retains the immediately preceding normal
    # core tuple at its own callback boundary. It is observational only.
    "v12-predecessor": ("12", "20a5ec331ca71e541d2f6d42c1ab49eca0fec5dabf298b6faf51fa45c63c24ed"),
    # v13 correlates only observer ordinals: a visible SDL key event and the
    # original title's next documented INT 21h/AH=06h poll. It neither reads
    # the DOS result nor captures/derives a title frame, audio, or action.
    "v13-title-poll": ("13", "07d80df74d303b519884d37dd474da071b414e98396e8ae030ad89256432521b"),
    # v14 retains a fixed normal-core instruction ring only when the known
    # DOSBox-X INT 6 callback boundary is reached. It is a separate recorder
    # file, leaving the v13 result grammar and prior evidence unchanged.
    "v14-normal-core-history": ("14", "748c1c934a78a28baef083fc352b552644f9665bc27fc032db0fdd7463ee5c63"),
    "v15-anomaly-entry": ("15", "0f74d8350ef61249d9ede9b11baa60133f407eaa863c60b66d18e86671bbc65e"),
    "v16-anomaly-entry": ("16", "ab5b1f42a486a7f768f33978c7504cc59fd86a4eb29f80a9041205d383ec91fa"),
    "v17-anomaly-entry": ("17", "d03e11ea48710e00d97c127f9b3d3ba5bc6f4227fafb7764e2b5950e50704ae6"),
    "v18-ivt-entry": ("18", "74420b4f8a0e2009f08b278ead1f9b36804404808b27895f327f204836d65e11"),
}
GAME_ROOT = "millennium-return-to-earth-2-2"
MACHINE_PROFILES = {"svga_s3", "ega"}
MIN_DURATION_SECONDS = 15
MAX_DURATION_SECONDS = 600
MAX_FOCUS_SETTLE_SECONDS = 120
# The reviewed recorder caps host input at 256 short text records.  Keep a
# generous, fixed ceiling here so a damaged or substituted recorder cannot
# turn a receipt status check into unbounded host-side I/O.
MAX_INPUT_RECEIPT_BYTES = 64 * 1024
MAX_INPUT_RECEIPT_RECORDS = 256
MAX_RAW_OBSERVATION_BYTES = 8 * 1024 * 1024
# A defective external recorder can print a tight exception loop. Retain a
# reviewable prefix while hashing and counting the complete byte stream, rather
# than letting terminal output or an evidence cache grow without limit.
MAX_RECORDER_CONSOLE_LOG_BYTES = 1024 * 1024
# Hashing a stream indefinitely still permits a defective recorder to consume
# host CPU and pipe bandwidth for the entire operator window.  Stop the
# recorder once it crosses this deliberately generous cap, retain its bounded
# prefix, and leave a rejected-but-reviewable receipt instead of silently
# treating the partial diagnostic as a normal timeout.
MAX_RECORDER_CONSOLE_TOTAL_BYTES = 64 * 1024 * 1024
# Receipt v2 binds the retained console prefix as well as the complete stream.
# A verifier must never treat a pre-v2 receipt as if that missing integrity
# property had been observed.
# The five reviewed result shapes are external recorder observations only.
# They are a finite grammar boundary, not a DOS ABI or game-state model.
MAX_RAW_RESULT_RECORDS = 256
MAX_NORMAL_CORE_HISTORY_BYTES = 1024
MAX_NORMAL_CORE_ANOMALY_BYTES = 1024
RAW_RESULT_LINE = re.compile(
    r"raw-result\t([1-9][0-9]*) ([1-9][0-9]*) "
    r"(?:image=(mill\.com|titles\.exe) pc=0x(020e|0213|0129) "
    r"(?:source-int=0x(21|91) source-ax=0x([0-9a-f]{4}) ax=0x([0-9a-f]{4})|"
    r"source-call=0x0511 ax=0x([0-9a-f]{4}))|"
    r"private-vector image=titles\.exe pc=0x0127 int=0x91 vector_ip=0x[0-9a-f]{4} vector_cs=0x[0-9a-f]{4}|"
    r"private-handler-entry int=0x91 cs=0x[0-9a-f]{4} ip=0x[0-9a-f]{4}|"
    r"private-handler-return int=0x91 caller=titles\.exe pc=0x0129 ax=0x[0-9a-f]{4} flags=0x[0-9a-f]{4}|"
    r"fault=unhandled-interrupt int=0x06 cs=0x[0-9a-f]{4} ip=0x[0-9a-f]{4} "
    r"ss=0x[0-9a-f]{4} sp=0x[0-9a-f]{4}(?: return_ip=0x[0-9a-f]{4} "
    r"return_cs=0x[0-9a-f]{4} return_flags=0x[0-9a-f]{4} code=0x[0-9a-f]{8})? "
    r"ax=0x[0-9a-f]{4} bx=0x[0-9a-f]{4} cx=0x[0-9a-f]{4} dx=0x[0-9a-f]{4})\n")
V12_PREDECESSOR_FAULT_LINE = re.compile(
    r"raw-result\t([1-9][0-9]*) ([1-9][0-9]*) fault=unhandled-interrupt int=0x06 "
    r"cs=0x[0-9a-f]{4} ip=0x[0-9a-f]{4} ss=0x[0-9a-f]{4} sp=0x[0-9a-f]{4} "
    r"return_ip=0x[0-9a-f]{4} return_cs=0x[0-9a-f]{4} return_flags=0x[0-9a-f]{4} "
    r"code=0x[0-9a-f]{8} ax=0x[0-9a-f]{4} bx=0x[0-9a-f]{4} cx=0x[0-9a-f]{4} "
    r"dx=0x[0-9a-f]{4} predecessor_valid=[01] predecessor_cs=0x[0-9a-f]{4} "
    r"predecessor_ip=0x[0-9a-f]{4} predecessor_code=0x[0-9a-f]{8} "
    r"predecessor_recognised_image=[01]\n")
TITLE_INPUT_POLL_LINE = re.compile(
    r"raw-result\t([1-9][0-9]*) ([1-9][0-9]*) title-input-poll "
    r"image=titles\.exe pc=0x0d0a host_key_ordinal=([0-9]+) ah=0x06 dl=0xff\n")
NORMAL_CORE_HISTORY_LINE = re.compile(
    r"normal-core-history-v1 count=([1-9]|1[0-6]) entries="
    r"([0-9a-f]{4}:[0-9a-f]{4}:[0-9a-f]{8}(?:,[0-9a-f]{4}:[0-9a-f]{4}:[0-9a-f]{8}){0,15})\n")
KNOWN_V14_NORMAL_CORE_HISTORY = tuple(
    [f"0e70:{ip:04x}:00000000" for ip in range(0x18e4, 0x1901, 2)]
    + ["f000:ca60:fe380300"])
NORMAL_CORE_ANOMALY_LINE = re.compile(
    r"normal-core-anomaly-v1 target=([0-9a-f]{4}:[0-9a-f]{4}) ss=([0-9a-f]{4}) "
    r"sp=([0-9a-f]{4}) prior_count=([1-9]|1[0-6]) entries="
    r"([0-9a-f]{4}:[0-9a-f]{4}:[0-9a-f]{8}(?:,[0-9a-f]{4}:[0-9a-f]{4}:[0-9a-f]{8}){0,15})\n")
KNOWN_V18_IVT_PRIOR = (
    "0a8d:051f:80fa0174", "0a8d:0522:740bb950", "0a8d:052f:f7e15903", "0a8d:0531:5903c159",
    "0a8d:0532:03c159c3", "0a8d:0534:59c31e06", "0a8d:0535:c31e061e", "0a8d:076b:8bd0b403",
    "0a8d:076d:b403e8bd", "0a8d:076f:e8bdf95b", "0a8d:012f:1e565755", "0a8d:0130:56575506",
    "0a8d:0131:575506cd", "0a8d:0132:5506cd93", "0a8d:0133:06cd9307", "0a8d:0134:cd93075d")
# The recorder emits SDL key data as opaque lowercase hexadecimal fields. The
# grammar authenticates the bounded external receipt shape only; it does not
# claim DOS accepted a key or assign its original-game meaning.
HOST_KEY_LINE = re.compile(
    r"host-key ([1-9][0-9]*) ticks=([0-9]+) state=(down|up) "
    r"scancode=0x([0-9a-f]+) sym=0x([0-9a-f]+) mod=0x([0-9a-f]+)\n")
# v9 binds the first post-handler caller re-entry to v8's observed endpoint.
# Its AX and FLAGS are raw machine state only; they are not a private ABI or a
# runtime result contract.
CAPTURE_RECEIPT_VERSION = "11"
TERMINATION_REASONS = {"emulator-exit", "timeout", "console-safety-cap", "known-unhandled-interrupt"}


class CaptureError(RuntimeError):
    """A local preflight failure that must not create a capture receipt."""


class RecorderConsoleStatus:
    """Small importlib-safe value object for a bounded console receipt."""

    def __init__(self, total_bytes: int, sha256: str, retained_bytes: int,
                 retained_sha256: str, over_limit: bool) -> None:
        self.total_bytes = total_bytes
        self.sha256 = sha256
        self.retained_bytes = retained_bytes
        self.retained_sha256 = retained_sha256
        self.over_limit = over_limit

    @property
    def truncated(self) -> bool:
        return self.total_bytes != self.retained_bytes


def sha256_file(path: Path) -> tuple[str, int]:
    digest = hashlib.sha256()
    retained_digest = hashlib.sha256()
    size = 0
    with path.open("rb") as stream:
        for block in iter(lambda: stream.read(1024 * 1024), b""):
            digest.update(block)
            size += len(block)
    return digest.hexdigest(), size


def require_absolute_regular_file(path: Path, label: str, *, executable: bool = False) -> Path:
    if not path.is_absolute():
        raise CaptureError(f"{label} path must be absolute")
    try:
        info = path.lstat()
    except OSError as error:
        raise CaptureError(f"Unable to stat {label}: {error}") from error
    if stat.S_ISLNK(info.st_mode) or not stat.S_ISREG(info.st_mode):
        raise CaptureError(f"{label} must be a non-symlink regular file")
    if executable and not os.access(path, os.X_OK):
        raise CaptureError(f"{label} is not executable")
    return path


def is_system_tmp_path(path: Path) -> bool:
    """Reject the lexical `/tmp` route before platform-specific resolution.

    macOS resolves `/tmp` through `/private/tmp`, while Windows resolves a
    POSIX-looking `Path('/tmp/...')` on its current drive.  The capture
    contract names the route supplied by the operator, so checking only a
    resolved path silently weakened it on both hosts.
    """
    normalized = path.as_posix().replace("\\", "/")
    if normalized == "/tmp" or normalized.startswith("/tmp/"):
        return True
    parts = path.parts
    return bool(path.anchor and len(parts) > 1 and parts[1].casefold() == "tmp")


def reject_unsafe_output(source: Path, output: Path) -> Path:
    # Preserve the no-/tmp contract before Windows reclassifies a POSIX-style
    # spelling as a current-drive relative path.
    if is_system_tmp_path(output):
        raise CaptureError("output must not use /tmp; use a Project Eon cache path")
    if not output.is_absolute():
        raise CaptureError("output path must be absolute")
    if output.exists() or output.is_symlink():
        raise CaptureError("output directory must not exist")
    resolved = output.resolve(strict=False)
    if resolved == ROOT or ROOT in resolved.parents:
        raise CaptureError("output must stay outside the repository")
    if source.parent == resolved or source.parent in resolved.parents:
        raise CaptureError("output must stay outside the supplied-media directory")
    parent = resolved.parent
    if not parent.is_dir() or parent.is_symlink():
        raise CaptureError("output parent must be an existing non-symlink directory")
    return resolved


def validate_source_release(source: Path) -> tuple[str, int]:
    digest, size = sha256_file(source)
    if digest != EXPECTED_RELEASE_SHA256 or size != EXPECTED_RELEASE_SIZE:
        raise CaptureError("source release is not the exact recognised English Millennium DOS archive")
    return digest, size


def validate_recorder(path: Path, expected_sha256: str | None = None) -> tuple[str, int]:
    if expected_sha256 is None:
        expected_sha256 = EXPECTED_RECORDER_SHA256
    digest, size = sha256_file(path)
    if digest != expected_sha256:
        raise CaptureError("recorder hash does not match the reviewed DOSBox-X build")
    return digest, size


def require_visible_operator_input(environment: dict[str, str]) -> None:
    if environment.get("SDL_VIDEODRIVER", "").lower() == "dummy":
        raise CaptureError("headless SDL is forbidden; a physical operator must use the visible emulator window")
    if not environment.get("DISPLAY") and not environment.get("WAYLAND_DISPLAY"):
        raise CaptureError("a visible X11 or Wayland display is required for physical input capture")


def recorder_config(game_root: Path, machine_profile: str = "svga_s3") -> str:
    """Return the fixed, evidence-reviewed configuration with one read-only game root.

    Millennium's 16-bit startup reaches a documented word copy from the last
    offset in a segment.  The pinned recorder's default ``segment limits=true``
    turns that real-mode wrapping operation into an endless #GP diagnostic
    loop.  Disable only that emulator compatibility check; this does not
    alter guest memory, the original archive, or the physical-input policy.
    The resulting configuration remains an explicit, hash-bound capture
    preimage rather than an implicit runtime fallback.
    """
    if machine_profile not in MACHINE_PROFILES:
        raise CaptureError("machine profile is not in the reviewed finite profile set")
    return "\n".join((
        "[sdl]",
        "fullscreen=false",
        "output=surface",
        "vsync=false",
        "",
        "[dosbox]",
        f"machine={machine_profile}",
        "memsize=16",
        "",
        "[render]",
        "aspect=false",
        "",
        "[cpu]",
        "core=normal",
        "cycles=max",
        "segment limits=false",
        "",
        "[sblaster]",
        "sbtype=none",
        "",
        "[autoexec]",
        "@echo off",
        f'mount c "{game_root.as_posix()}"',
        "",
    ))


def mount_options(mountpoint: Path) -> str:
    completed = subprocess.run(
        ["findmnt", "-T", str(mountpoint), "-no", "OPTIONS"], check=True,
        capture_output=True, text=True)
    return completed.stdout.strip()


def mount_read_only(source: Path, mountpoint: Path) -> None:
    subprocess.run(["archivemount", "-o", "ro", str(source), str(mountpoint)], check=True)
    options = mount_options(mountpoint)
    required = {"ro", "nosuid", "nodev"}
    if not required <= set(options.split(",")):
        subprocess.run(["fusermount", "-u", str(mountpoint)], check=False)
        raise CaptureError("archivemount did not report required ro,nosuid,nodev options")


def unmount(mountpoint: Path) -> None:
    subprocess.run(["fusermount", "-u", str(mountpoint)], check=False,
                   stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)


def write_exclusive(path: Path, content: str) -> None:
    with path.open("x", encoding="utf-8", newline="\n") as stream:
        stream.write(content)
        stream.flush()
        os.fsync(stream.fileno())


def input_receipt_status(path: Path) -> str:
    """Describe a recorder-created host-input receipt without creating one.

    A missing receipt is useful negative evidence: the visible recorder ran,
    but it did not observe a host key in its SDL event loop.  It must never be
    represented as an empty, generated input timeline.  A receipt is still
    only a host-side observation, not proof that the DOS program accepted an
    input.
    """
    try:
        info = path.lstat()
    except FileNotFoundError:
        return "host_input_receipt=absent\n"
    if stat.S_ISLNK(info.st_mode) or not stat.S_ISREG(info.st_mode):
        raise CaptureError("host-input receipt is not a regular non-symlink file")
    if info.st_size > MAX_INPUT_RECEIPT_BYTES:
        raise CaptureError("host-input receipt exceeds the bounded recorder contract")
    if info.st_size == 0:
        return "host_input_receipt=empty\n"
    record_count = parse_host_input_receipt(path)
    digest, size = sha256_file(path)
    return ("host_input_receipt=present\n"
            f"host_input_receipt_sha256={digest}\n"
            f"host_input_receipt_bytes={size}\n"
            f"host_input_receipt_records={record_count}\n")


def input_delivery_file_observed(path: Path) -> bool:
    """Report a recorder-created receipt without parsing a live append stream.

    This is only an operator-facing liveness signal.  The complete receipt is
    still grammar-checked and hash-bound after DOSBox-X exits, so a partial
    write can never become capture evidence.
    """
    try:
        info = path.lstat()
    except FileNotFoundError:
        return False
    if stat.S_ISLNK(info.st_mode) or not stat.S_ISREG(info.st_mode):
        raise CaptureError("live host-input receipt is not a regular non-symlink file")
    return info.st_size > 0


def parse_host_input_receipt(path: Path) -> int:
    """Validate the finite SDL host-key receipt without decoding DOS input."""
    try:
        text = path.read_text(encoding="ascii")
    except UnicodeDecodeError as error:
        raise CaptureError("host-input receipt is not ASCII recorder output") from error
    if not text.endswith("\n"):
        raise CaptureError("host-input receipt has a truncated final record")
    count = 0
    for expected, line in enumerate(text.splitlines(keepends=True), start=1):
        match = HOST_KEY_LINE.fullmatch(line)
        if not match:
            raise CaptureError("host-input receipt contains an invalid recorder record")
        if int(match.group(1)) != expected:
            raise CaptureError("host-input receipt record ordinals are not contiguous")
        count = expected
        if count > MAX_INPUT_RECEIPT_RECORDS:
            raise CaptureError("host-input receipt exceeds the recorder record cap")
    return count


def raw_observation_status(path: Path, name: str, recorder_protocol: str = "v11") -> str:
    """Bind an optional recorder-owned raw log without reading it unbounded."""
    try:
        info = path.lstat()
    except FileNotFoundError:
        return f"{name}=absent\n"
    if stat.S_ISLNK(info.st_mode) or not stat.S_ISREG(info.st_mode):
        raise CaptureError(f"{name} is not a regular non-symlink file")
    if info.st_size > MAX_RAW_OBSERVATION_BYTES:
        raise CaptureError(f"{name} exceeds the bounded recorder contract")
    if info.st_size == 0:
        return f"{name}=empty\n"
    if name == "results_raw":
        return raw_result_status(path, name, recorder_protocol)
    digest, size = sha256_file(path)
    return f"{name}=present\n{name}_sha256={digest}\n{name}_bytes={size}\n"


def normal_core_history_entries(path: Path) -> tuple[str, ...]:
    """Return one canonical recorder-owned history record as opaque tuples."""
    try:
        info = path.lstat()
    except FileNotFoundError:
        raise CaptureError("normal-core history is absent")
    if stat.S_ISLNK(info.st_mode) or not stat.S_ISREG(info.st_mode):
        raise CaptureError("normal-core history is not a regular non-symlink file")
    if not 0 < info.st_size <= MAX_NORMAL_CORE_HISTORY_BYTES:
        raise CaptureError("normal-core history exceeds the bounded recorder contract")
    try:
        raw = path.read_bytes()
        text = raw.decode("ascii")
    except UnicodeDecodeError as error:
        raise CaptureError("normal-core history is not ASCII recorder output") from error
    if not raw.endswith(b"\n") or b"\r" in raw:
        raise CaptureError("normal-core history is not a canonical LF recorder record")
    match = NORMAL_CORE_HISTORY_LINE.fullmatch(text)
    if not match:
        raise CaptureError("normal-core history contains an invalid recorder record")
    entries = match.group(2).split(",")
    if len(entries) != int(match.group(1)):
        raise CaptureError("normal-core history count does not match entries")
    return tuple(entries)


def normal_core_history_status(path: Path, recorder_protocol: str) -> str:
    """Bind v14's one bounded normal-core sidecar without decoding game state."""
    if recorder_protocol != "v14-normal-core-history":
        return ""
    try:
        entries = normal_core_history_entries(path)
    except CaptureError as error:
        if str(error) == "normal-core history is absent":
            return "normal_core_history=absent\n"
        raise
    digest, size = sha256_file(path)
    return ("normal_core_history=present\n"
            f"normal_core_history_sha256={digest}\n"
            f"normal_core_history_bytes={size}\n"
            f"normal_core_history_entries={len(entries)}\n")


def normal_core_history_boundary_status(history_path: Path, results_path: Path,
                                        recorder_protocol: str,
                                        termination_reason: str) -> str:
    """Admit V14's observed fault boundary without assigning it game meaning."""
    if recorder_protocol != "v14-normal-core-history" or termination_reason != "known-unhandled-interrupt":
        return ""
    if not known_v11_early_stop_receipt(results_path):
        raise CaptureError("v14 history requires the exact known callback fault receipt")
    entries = normal_core_history_entries(history_path)
    if entries != KNOWN_V14_NORMAL_CORE_HISTORY:
        raise CaptureError("v14 history does not match the observed zero-context callback boundary")
    return ("normal_core_history_boundary=observed-zero-context-to-default-callback\n"
            f"normal_core_history_first={entries[0]}\n"
            f"normal_core_history_last={entries[-1]}\n")


def normal_core_anomaly_status(path: Path, results_path: Path, recorder_protocol: str,
                               termination_reason: str) -> str:
    if recorder_protocol != "v18-ivt-entry": return ""
    try: raw = path.read_bytes()
    except FileNotFoundError: return "normal_core_anomaly=absent\n"
    if not 0 < len(raw) <= MAX_NORMAL_CORE_ANOMALY_BYTES or not raw.endswith(b"\n") or b"\r" in raw:
        raise CaptureError("normal-core anomaly is not a bounded canonical recorder record")
    try: match = NORMAL_CORE_ANOMALY_LINE.fullmatch(raw.decode("ascii"))
    except UnicodeDecodeError as error: raise CaptureError("normal-core anomaly is not ASCII recorder output") from error
    if not match or len(match.group(5).split(",")) != int(match.group(4)):
        raise CaptureError("normal-core anomaly contains an invalid recorder record")
    entries = tuple(match.group(5).split(","))
    if termination_reason == "known-unhandled-interrupt":
        if not known_v11_early_stop_receipt(results_path) or match.group(1) != "0000:0000" \
                or match.group(2) != "0a8d" or match.group(3) != "d9e2" or entries != KNOWN_V18_IVT_PRIOR:
            raise CaptureError("v18 anomaly does not match the observed INT 93h IVT boundary")
    digest, size = sha256_file(path)
    return ("normal_core_anomaly=present\n"
            f"normal_core_anomaly_sha256={digest}\nnormal_core_anomaly_bytes={size}\n"
            f"normal_core_anomaly_target={match.group(1)}\n")


def raw_result_labels(path: Path, recorder_protocol: str = "v11") -> list[str]:
    """Parse finite recorder diagnostics into non-semantic shape labels."""
    try:
        text = path.read_text(encoding="ascii")
    except UnicodeDecodeError as error:
        raise CaptureError("results_raw is not ASCII recorder output") from error
    if not text.endswith("\n"):
        raise CaptureError("results_raw has a truncated final record")
    labels: list[str] = []
    for expected, line in enumerate(text.splitlines(keepends=True), start=1):
        title_poll = (TITLE_INPUT_POLL_LINE.fullmatch(line)
    if recorder_protocol in {"v13-title-poll", "v14-normal-core-history", "v15-anomaly-entry", "v16-anomaly-entry", "v17-anomaly-entry", "v18-ivt-entry"} and " title-input-poll " in line
                      else None)
        match = title_poll or (V12_PREDECESSOR_FAULT_LINE.fullmatch(line)
            if recorder_protocol == "v12-predecessor" and " fault=" in line
            else RAW_RESULT_LINE.fullmatch(line))
        if not match:
            raise CaptureError("results_raw contains an invalid recorder record")
        if int(match.group(1)) != expected or int(match.group(2)) != expected:
            raise CaptureError("results_raw record counters are not contiguous")
        prefix = "raw-result\t" + str(expected) + " " + str(expected) + " "
        if title_poll:
            label = "title-input-poll"
        elif line.startswith(prefix + "fault="):
            label = "fault"
        elif line.startswith(prefix + "private-vector "):
            label = "private-vector"
        elif line.startswith(prefix + "private-handler-entry "):
            label = "private-handler-entry"
        elif line.startswith(prefix + "private-handler-return "):
            label = "private-handler-return"
        else:
            label = f"{match.group(3)}:{match.group(4)}"
        labels.append(label)
        if expected > MAX_RAW_RESULT_RECORDS:
            raise CaptureError("results_raw exceeds the recorder record cap")
    return labels


def title_input_poll_ordinals(path: Path, recorder_protocol: str = "v11") -> list[int]:
    """Read v13 chronology records without assigning a DOS input result.

    Each ordinal names the count of host SDL key events observed before this
    exact original INT 21h/AH=06h call. It is not an input-delivery receipt,
    and its absence cannot be replaced by a synthesized poll or frame.
    """
    if recorder_protocol not in {"v13-title-poll", "v14-normal-core-history", "v15-anomaly-entry", "v16-anomaly-entry", "v17-anomaly-entry", "v18-ivt-entry"}:
        return []
    try:
        text = path.read_text(encoding="ascii")
    except UnicodeDecodeError as error:
        raise CaptureError("results_raw is not ASCII recorder output") from error
    if not text.endswith("\n"):
        raise CaptureError("results_raw has a truncated final record")
    ordinals: list[int] = []
    for line in text.splitlines(keepends=True):
        match = TITLE_INPUT_POLL_LINE.fullmatch(line)
        if not match:
            continue
        ordinal = int(match.group(3))
        if ordinals and ordinal <= ordinals[-1]:
            raise CaptureError("title-input poll host-key ordinals are not strictly increasing")
        ordinals.append(ordinal)
        if len(ordinals) > 32:
            raise CaptureError("title-input poll observations exceed the recorder cap")
    return ordinals


def title_input_checkpoint_status(results_path: Path, input_receipt_path: Path,
                                  recorder_protocol: str = "v11") -> str:
    """Summarize v13's observable chronology without claiming delivery.

    A host key followed by a title poll proves neither DOS carry/AL nor a
    title action. The receipt calls it a correlation only, and rejects an
    impossible recorder ordinal beyond the independently recorded host keys.
    """
    if recorder_protocol not in {"v13-title-poll", "v14-normal-core-history", "v15-anomaly-entry", "v16-anomaly-entry", "v17-anomaly-entry", "v18-ivt-entry"}:
        return ""
    polls = title_input_poll_ordinals(results_path, recorder_protocol)
    try:
        info = input_receipt_path.lstat()
    except FileNotFoundError:
        host_keys = 0
        host_state = "absent"
    else:
        if stat.S_ISLNK(info.st_mode) or not stat.S_ISREG(info.st_mode):
            raise CaptureError("host-input receipt is not a regular non-symlink file")
        host_keys = parse_host_input_receipt(input_receipt_path) if info.st_size else 0
        host_state = "present" if info.st_size else "empty"
    if any(ordinal > host_keys for ordinal in polls):
        raise CaptureError("title-input poll names a host-key ordinal absent from the receipt")
    correlated = bool(polls and polls[-1] > 0)
    state = "host-key-and-poll" if correlated else "poll-only" if polls else (
        "host-key-only" if host_keys else "absent")
    return (f"title_input_checkpoint={state}\n"
            f"title_input_checkpoint_host_receipt={host_state}\n"
            f"title_input_checkpoint_polls={len(polls)}\n"
            f"title_input_checkpoint_last_host_key_ordinal={polls[-1] if polls else 0}\n")


def parse_raw_results(path: Path, recorder_protocol: str = "v11") -> dict[str, int]:
    """Validate only the recorder's known diagnostics-only result grammar."""
    counts: dict[str, int] = {}
    for label in raw_result_labels(path, recorder_protocol):
        counts[label] = counts.get(label, 0) + 1
    return counts


KNOWN_V10_EARLY_STOP_SEQUENCE = (
    "mill.com:020e", "mill.com:0213", "private-vector", "private-handler-entry",
    "private-handler-return", "titles.exe:0129", "titles.exe:0129", "fault",
)

# This is the complete, twice-observed recorder diagnostic that terminates the
# host process.  It is deliberately a recorder receipt, not a DOS interrupt
# contract: Eon neither consumes nor interprets these values at runtime.  A
# byte-exact gate prevents a different INT 6 or changed machine state from
# being silently classified as the known callback loop.
KNOWN_V11_EARLY_STOP_RAW = (
    b"raw-result\t1 1 image=mill.com pc=0x020e source-int=0x21 source-ax=0x2591 ax=0x2591\n"
    b"raw-result\t2 2 image=mill.com pc=0x0213 source-call=0x0511 ax=0x0000\n"
    b"raw-result\t3 3 private-vector image=titles.exe pc=0x0127 int=0x91 vector_ip=0x0000 vector_cs=0x087e\n"
    b"raw-result\t4 4 private-handler-entry int=0x91 cs=0x087e ip=0x0000\n"
    b"raw-result\t5 5 private-handler-return int=0x91 caller=titles.exe pc=0x0129 ax=0x0101 flags=0x7202\n"
    b"raw-result\t6 6 image=titles.exe pc=0x0129 source-int=0x91 source-ax=0x0000 ax=0x0101\n"
    b"raw-result\t7 7 image=titles.exe pc=0x0129 source-int=0x91 source-ax=0x0000 ax=0x0000\n"
    b"raw-result\t8 8 fault=unhandled-interrupt int=0x06 cs=0xf000 ip=0xca64 "
    b"ss=0x0a8d sp=0xc9bf return_ip=0x1900 return_cs=0x0e70 return_flags=0x7047 "
    b"code=0x00000000 ax=0x00a0 bx=0x6101 cx=0x178b dx=0x6101\n"
)


def known_v10_early_stop_sequence(path: Path) -> bool:
    """Check v10's observed diagnostic order without assigning it an ABI."""
    try:
        return tuple(raw_result_labels(path)) == KNOWN_V10_EARLY_STOP_SEQUENCE
    except CaptureError:
        return False


def known_v11_early_stop_receipt(path: Path) -> bool:
    """Check the complete observed callback-loop diagnostic without ABI inference."""
    try:
        return path.read_bytes() == KNOWN_V11_EARLY_STOP_RAW
    except OSError:
        return False


def raw_result_status(path: Path, name: str, recorder_protocol: str = "v11") -> str:
    counts = parse_raw_results(path, recorder_protocol)
    digest, size = sha256_file(path)
    summary = ",".join(f"{key}:{counts[key]}" for key in sorted(counts))
    status = (f"{name}=present\n{name}_sha256={digest}\n{name}_bytes={size}\n"
              f"{name}_records={sum(counts.values())}\n{name}_shapes={summary}\n")
    if recorder_protocol in {"v13-title-poll", "v14-normal-core-history", "v15-anomaly-entry", "v16-anomaly-entry", "v17-anomaly-entry", "v18-ivt-entry"}:
        polls = title_input_poll_ordinals(path, recorder_protocol)
        status += (f"{name}_title_input_polls={len(polls)}\n"
                   f"{name}_last_host_key_ordinal={polls[-1] if polls else 0}\n")
    return status


def known_unhandled_interrupt_observed(path: Path, recorder_protocol: str = "v11") -> bool:
    """Return whether the recorder completed its one known raw INT 6 record.

    This is a host-side early-stop guard for the already documented DOSBox-X
    callback loop.  It reads no guest memory and does not install a handler;
    it merely stops the external process once the recorder has durably written
    the bounded observation that cannot lead to a playable capture.
    """
    try:
        info = path.lstat()
    except FileNotFoundError:
        return False
    if stat.S_ISLNK(info.st_mode) or not stat.S_ISREG(info.st_mode):
        return False
    if info.st_size == 0 or info.st_size > MAX_RAW_OBSERVATION_BYTES:
        return False
    try:
        text = path.read_text(encoding="ascii")
    except UnicodeDecodeError:
        return False
    # A recorder append can be in progress while this helper polls.  Never
    # treat an unfinished line as an observation, and never modify that file.
    if not text.endswith("\n"):
        return False
    if recorder_protocol == "v11":
        return known_v11_early_stop_receipt(path)
    if recorder_protocol == "v12-predecessor":
        try:
            # This only stops the external callback loop after a complete
            # bounded shape. It does not admit the new values or make them a
            # private-interrupt result; two byte-identical receipts are still
            # required before any documentation or runtime use.
            return tuple(raw_result_labels(path, recorder_protocol)) == KNOWN_V10_EARLY_STOP_SEQUENCE
        except CaptureError:
            return False
    if recorder_protocol in {"v13-title-poll", "v14-normal-core-history", "v15-anomaly-entry", "v16-anomaly-entry", "v17-anomaly-entry", "v18-ivt-entry"}:
        # The legacy callback-loop receipt remains a bounded diagnostic stop
        # when no title poll was reached. Any poll-bearing run stays alive for
        # the operator's configured duration so it cannot be mistaken for a
        # completed input-to-frame capture.
        return known_v11_early_stop_receipt(path)
    return False


def capture_bounded_console(stream, path: Path, over_limit: threading.Event) -> RecorderConsoleStatus:
    """Drain an emulator console while retaining only a bounded evidence prefix.

    The full transcript is hashed and counted as it is read. This preserves a
    stable identity for a pathological recorder output without keeping an
    unbounded error loop on disk or blocking the emulator on a full pipe.  It
    signals the owner as soon as the total safety cap is crossed, but continues
    draining until that owner terminates the child; stopping the reader first
    would deadlock the child on its stdout pipe.
    """
    digest = hashlib.sha256()
    retained_digest = hashlib.sha256()
    total = 0
    retained = 0
    with path.open("xb") as destination:
        while True:
            chunk = stream.read(64 * 1024)
            if not chunk:
                break
            total += len(chunk)
            digest.update(chunk)
            if total > MAX_RECORDER_CONSOLE_TOTAL_BYTES:
                over_limit.set()
            if retained < MAX_RECORDER_CONSOLE_LOG_BYTES:
                keep = chunk[:MAX_RECORDER_CONSOLE_LOG_BYTES - retained]
                destination.write(keep)
                retained_digest.update(keep)
                retained += len(keep)
        destination.flush()
        os.fsync(destination.fileno())
    return RecorderConsoleStatus(total, digest.hexdigest(), retained, retained_digest.hexdigest(),
                                 over_limit.is_set())


def recorder_console_status(status: RecorderConsoleStatus) -> str:
    return ("recorder_console=present\n"
            f"recorder_console_sha256={status.sha256}\n"
            f"recorder_console_total_bytes={status.total_bytes}\n"
            f"recorder_console_retained_bytes={status.retained_bytes}\n"
            f"recorder_console_retained_sha256={status.retained_sha256}\n"
            f"recorder_console_truncated={'true' if status.truncated else 'false'}\n"
            f"recorder_console_over_limit={'true' if status.over_limit else 'false'}\n")


def identity_status(name: str, identity: tuple[str, int]) -> str:
    """Retain the exact capture preimage without retaining supplied media."""
    digest, size = identity
    return f"{name}_sha256={digest}\n{name}_bytes={size}\n"


def run_capture(args: argparse.Namespace) -> Path:
    source = require_absolute_regular_file(Path(args.source_release), "source release")
    recorder = require_absolute_regular_file(Path(args.recorder), "recorder", executable=True)
    receipt_version, recorder_hash = RECORDER_PROTOCOLS[args.recorder_protocol]
    recorder_identity = validate_recorder(recorder, recorder_hash)
    output = reject_unsafe_output(source, Path(args.output))
    if not MIN_DURATION_SECONDS <= args.duration_seconds <= MAX_DURATION_SECONDS:
        raise CaptureError(f"duration must be between {MIN_DURATION_SECONDS} and {MAX_DURATION_SECONDS} seconds")
    if not 0 <= args.focus_settle_seconds <= MAX_FOCUS_SETTLE_SECONDS:
        raise CaptureError(f"focus-settle duration must be between 0 and {MAX_FOCUS_SETTLE_SECONDS} seconds")
    environment = dict(os.environ)
    require_visible_operator_input(environment)
    source_hash, source_size = validate_source_release(source)
    output.mkdir(mode=0o700)
    mountpoint = output / "archive-ro"
    mountpoint.mkdir(mode=0o700)
    mounted = False
    try:
        mount_read_only(source, mountpoint)
        mounted = True
        game_root = mountpoint / GAME_ROOT
        if not game_root.is_dir():
            raise CaptureError("recognised archive does not expose its expected DOS game root")
        configuration = output / "recorder.conf"
        command_tail = output / "command-tail.txt"
        write_exclusive(configuration, recorder_config(game_root, args.machine_profile))
        configuration_identity = sha256_file(configuration)
        command = [str(recorder), "-conf", str(configuration), "-fastlaunch", "-c", "c:",
                   "-c", "mill.com 0"]
        write_exclusive(command_tail, " ".join(command) + "\n")
        environment.update({
            "PROJECT_EON_DOSBOX_X_RECORD": str(output / "events.raw"),
            "PROJECT_EON_DOSBOX_X_RESULT": str(output / "results.raw"),
            "PROJECT_EON_DOSBOX_X_INPUT_RECORD": str(output / "host-input-receipt.raw"),
        })
        if args.recorder_protocol in {"v14-normal-core-history", "v15-anomaly-entry", "v16-anomaly-entry", "v17-anomaly-entry", "v18-ivt-entry"}:
            environment["PROJECT_EON_DOSBOX_X_HISTORY_RECORD"] = str(output / "normal-core-history.raw")
        if args.recorder_protocol in {"v15-anomaly-entry", "v16-anomaly-entry", "v17-anomaly-entry", "v18-ivt-entry"}:
            environment["PROJECT_EON_DOSBOX_X_ANOMALY_RECORD"] = str(output / "normal-core-anomaly.raw")
        print("CAPTURE PREPARED  read-only original archive; physical operator input required")
        print("Focus the visible DOSBox-X window by clicking it yourself; do not use terminal or automation input.")
        print("Then press and release ordinary keys only in that window (for example Return, Space, or arrows).")
        print(f"The {args.focus_settle_seconds}-second focus-settle window begins now; the {args.duration_seconds}-second capture window follows.")
        print("No AUTOTYPE, debugger input, or guest-memory injection is permitted.")
        started = time.time()
        process = subprocess.Popen(command, env=environment, stdout=subprocess.PIPE,
                                   stderr=subprocess.STDOUT)
        assert process.stdout is not None
        console_path = output / "recorder-console.log"
        console_result: list[RecorderConsoleStatus] = []
        console_errors: list[BaseException] = []
        console_over_limit = threading.Event()

        def drain_console() -> None:
            try:
                console_result.append(capture_bounded_console(process.stdout, console_path,
                                                              console_over_limit))
            except BaseException as error:  # Propagate after process cleanup.
                console_errors.append(error)

        console_thread = threading.Thread(target=drain_console, name="project-eon-dos-console", daemon=True)
        console_thread.start()
        settle_deadline = time.monotonic() + args.focus_settle_seconds
        deadline: float | None = None
        termination_reason = "emulator-exit"
        live_input_observed = False
        while True:
            exit_status = process.poll()
            if exit_status is not None:
                break
            now = time.monotonic()
            if deadline is None and now >= settle_deadline:
                deadline = now + args.duration_seconds
                print(f"CAPTURE WINDOW ACTIVE  {args.duration_seconds} seconds; physical input remains required.")
            if not live_input_observed and input_delivery_file_observed(output / "host-input-receipt.raw"):
                live_input_observed = True
                print("HOST INPUT OBSERVED  receipt will be fully validated after capture.")
            if console_over_limit.is_set():
                process.kill()
                process.wait()
                exit_status = 125
                termination_reason = "console-safety-cap"
                break
            if known_unhandled_interrupt_observed(output / "results.raw", args.recorder_protocol):
                process.kill()
                process.wait()
                exit_status = 126
                termination_reason = "known-unhandled-interrupt"
                break
            remaining = (settle_deadline - now if deadline is None else deadline - now)
            if remaining <= 0:
                process.kill()
                process.wait()
                exit_status = 124
                termination_reason = "timeout"
                break
            console_over_limit.wait(timeout=min(0.1, remaining))
        console_thread.join()
        if console_errors:
            raise CaptureError(f"unable to retain bounded recorder console: {console_errors[0]}")
        if len(console_result) != 1:
            raise CaptureError("bounded recorder console did not produce exactly one receipt")
        ended = time.time()
        after_hash, after_size = validate_source_release(source)
        if (source_hash, source_size) != (after_hash, after_size):
            raise CaptureError("source archive changed during capture; evidence is rejected")
        receipt_status = input_receipt_status(output / "host-input-receipt.raw")
        observation_status = (raw_observation_status(output / "events.raw", "events_raw", args.recorder_protocol)
                              + raw_observation_status(output / "results.raw", "results_raw", args.recorder_protocol))
        history_status = normal_core_history_status(output / "normal-core-history.raw",
                                                    args.recorder_protocol)
        history_boundary_status = normal_core_history_boundary_status(
            output / "normal-core-history.raw", output / "results.raw", args.recorder_protocol,
            termination_reason)
        anomaly_status = normal_core_anomaly_status(output / "normal-core-anomaly.raw",
            output / "results.raw", args.recorder_protocol, termination_reason)
        title_checkpoint_status = title_input_checkpoint_status(output / "results.raw",
            output / "host-input-receipt.raw", args.recorder_protocol)
        write_exclusive(output / "run-status.txt",
                        f"capture_receipt_version={receipt_version}\n"
                        f"recorder_protocol={args.recorder_protocol}\n"
                        f"focus_settle_seconds={args.focus_settle_seconds}\n"
                        f"host_input_observed_during_capture={'true' if live_input_observed else 'false'}\n"
                        f"exit_status={exit_status}\ntermination_reason={termination_reason}\n"
                        f"start_unix={started:.6f}\nend_unix={ended:.6f}\n"
                        f"machine_profile={args.machine_profile}\n"
                        + identity_status("source_release", (after_hash, after_size))
                        + identity_status("recorder", recorder_identity)
                        + identity_status("configuration", configuration_identity)
                        + receipt_status + observation_status + history_status + history_boundary_status
                        + anomaly_status + title_checkpoint_status
                        + recorder_console_status(console_result[0]))
        if console_result[0].over_limit:
            raise CaptureError(
                "recorder console exceeded the 64 MiB safety cap; evidence was retained but is not admitted")
        print("CAPTURE FINISHED  external evidence only; host-input receipt status is in run-status.txt")
        return output
    except Exception:
        # A failed preflight is not an admitted capture, but preserve every
        # recorder/configuration by-product for review. Deleting external
        # evidence after a source-integrity failure would hide the reason it
        # was rejected.
        if mounted:
            unmount(mountpoint)
            mounted = False
        raise
    finally:
        if mounted:
            unmount(mountpoint)


def parse_arguments(argv: list[str]) -> argparse.Namespace:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--source-release", required=True,
                        help="Absolute recognised English Millennium DOS ZIP path")
    parser.add_argument("--recorder", required=True,
                        help="Absolute executable path to the externally reviewed DOSBox-X recorder")
    parser.add_argument("--output", required=True,
                        help="New absolute cache directory for external capture evidence")
    parser.add_argument("--duration-seconds", type=int, default=120,
                        help="Visible operator window duration (15-600; default: 120)")
    parser.add_argument("--focus-settle-seconds", type=int, default=10,
                        help="Manual visible-window focus time before capture (0-120; default: 10)")
    parser.add_argument("--machine-profile", choices=tuple(sorted(MACHINE_PROFILES)), default="svga_s3",
                        help="Explicit DOSBox-X video-machine profile (default: svga_s3)")
    parser.add_argument("--recorder-protocol", choices=tuple(sorted(RECORDER_PROTOCOLS)), default="v11",
                        help="Reviewed recorder output grammar (default: v11)")
    return parser.parse_args(argv)


def main(argv: list[str] | None = None) -> int:
    try:
        output = run_capture(parse_arguments(sys.argv[1:] if argv is None else argv))
    except (CaptureError, OSError, subprocess.SubprocessError) as error:
        print(f"Millennium DOS capture preflight rejected: {error}", file=sys.stderr)
        return 2
    print(f"EXTERNAL CAPTURE DIRECTORY  {output}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
