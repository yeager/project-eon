#!/usr/bin/env python3
"""Run a read-only, operator-driven Deuteros Amiga recorder session.

This helper prepares external evidence around a separately reviewed FS-UAE
recorder. It is not an emulator distribution, input injector, trace
assembler, or Project Eon runtime component. A physical operator must use the
visible FS-UAE window; headless SDL, debugger routes, and playback are not
admitted.
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
EXPECTED_RELEASE_SHA256 = "f4dc8dd1c27c5d389837783becd9b95ab09b78baf40e94e39e2b7e590e470e04"
EXPECTED_RELEASE_SIZE = 4_066_771
EXPECTED_KICKSTART_SHA256 = "c9521c114900633c09317ca6ff979db7b9df34d3cb537de062f5d51811c42c04"
# This is the supplied ZIP size, not the 262,144-byte ROM payload size.
EXPECTED_KICKSTART_SIZE = 143_269
EXPECTED_RECORDER_SHA256 = "0e0bfb1fe73a6f37dc38992b39e34e355564adc516106c399c8be86fb38232ec"
EXPECTED_DISK1_SHA256 = "6ea0cc68d3af37203a885032eddf7c28e839e6abb59d8c9cd3792f1308bdec38"
EXPECTED_DISK2_SHA256 = "99909db1e190be02e049084743af44f00e331be6bf2d97b4831ada5fe4c30b4a"
EXPECTED_DISK1_ARCHIVE_SHA256 = "7ecaa0457ad2b61b417bbe62943a4a11b4d164acfbc5a5097e95f8f7d1360533"
EXPECTED_DISK1_ARCHIVE_SIZE = 449_666
EXPECTED_DISK2_ARCHIVE_SHA256 = "b98ee3c36141773485c5e03dd8bb4aa59784eaf08a1363fa6a2951a5eb5fdc0a"
EXPECTED_DISK2_ARCHIVE_SIZE = 490_962
EXPECTED_ROM_SHA256 = "ee05862d8102a08436ac4056da7d549db31625c7d47b24dfb7b3c9a5c113ca53"
DISK1_ARCHIVE = "Deuteros - The Next Millennium (1991)(Activision)(M3)(Disk 1 of 2).zip"
DISK2_ARCHIVE = "Deuteros - The Next Millennium (1991)(Activision)(M3)(Disk 2 of 2).zip"
DISK1_IMAGE = "Deuteros - The Next Millennium (1991)(Activision)(M3)(Disk 1 of 2).adf"
DISK2_IMAGE = "Deuteros - The Next Millennium (1991)(Activision)(M3)(Disk 2 of 2).adf"
KICKSTART_IMAGE = "Kickstart v1.3 r34.005 (1987-12)(Commodore)(A500-A1000-A2000-CDTV)[!].rom"
MIN_DURATION_SECONDS = 15
MAX_DURATION_SECONDS = 600
MAX_FOCUS_SETTLE_SECONDS = 120
# A finite timing profile makes a reachability diagnostic reproducible without
# confusing it with time-faithful capture evidence. Warp never establishes
# original timing, gameplay, or title-screen behaviour.
TIMING_PROFILES = {"realtime": "0", "warp": "1"}
CAPTURE_INTENTS = {"diagnostic-no-input", "physical-input"}
# The reviewed delivery observer writes a bounded physical-input receipt.
# Never hash an arbitrary-size file merely because a recorder path was set.
MAX_INPUT_RECEIPT_BYTES = 64 * 1024
MAX_INPUT_RECEIPT_RECORDS = 256
# Raw recorder output and console diagnostics are external evidence, not
# unbounded host storage.  A broken emulator must not be able to exhaust the
# operator's terminal, disk, or cache while a capture is being reviewed.
MAX_RAW_OBSERVATION_BYTES = 8 * 1024 * 1024
# 2,048 fixed-format writes can exceed 128 KiB; this remains a strict cap
# above the reviewed finite grammar without rejecting a complete observation.
MAX_TITLE_DISPLAY_RECEIPT_BYTES = 512 * 1024
MAX_RECORDER_CONSOLE_LOG_BYTES = 1024 * 1024
# Retaining only a prefix protects disk, but a pathological recorder can still
# consume host CPU and pipe bandwidth indefinitely while the runner hashes it.
# Signal the owner at this generous cap so it can terminate the child without
# mistaking a partial runaway diagnostic for a normal timed-out capture.
MAX_RECORDER_CONSOLE_TOTAL_BYTES = 64 * 1024 * 1024
# The reviewed recorder's raw-PC observer is intentionally finite and only
# exposes these investigation sites.  This list is a grammar boundary, not an
# interpretation of the observed instructions or their ABI effects.
RAW_PC_SITES = (
    0x000210D4, 0x00040450, 0x0004046C, 0x0004069A, 0x0001ED80,
    0x0001EDA6, 0x0001EF74, 0x0001F056, 0x0001F182, 0x0001FE7A,
    0x0001FE84, 0x0001FE88, 0x0001FE92, 0x0001FE96, 0x0001FBE6,
)
MAX_RAW_RECORDS = 4096
MAX_RAW_RECORDS_PER_SITE = 128
MAX_TITLE_DISPLAY_WRITES = 2048
MAX_TITLE_DISPLAY_WRITES_PER_REGISTER = 64
RAW_PC_LEGACY_LINE = re.compile(
    r"raw-pc ([1-9][0-9]*) cycles=([0-9]+) pc=0x([0-9a-f]{8}) "
    r"opcode=0x([0-9a-f]{4}) d0=0x([0-9a-f]{8}) a0=0x([0-9a-f]{8}) "
    r"a6=0x([0-9a-f]{8}) sr=0x([0-9a-f]{4})\n")
# The cycle-exact core keeps a prefetched IR word separately from the memory
# word at its current PC. Receipt v7 records both values and never presents
# the former as though it were a direct original-media byte read.
RAW_PC_V7_LINE = re.compile(
    r"raw-pc ([1-9][0-9]*) cycles=([0-9]+) pc=0x([0-9a-f]{8}) "
    r"ir_opcode=0x([0-9a-f]{4}) memory_opcode=0x([0-9a-f]{4}) "
    r"d0=0x([0-9a-f]{8}) a0=0x([0-9a-f]{8}) a6=0x([0-9a-f]{8}) sr=0x([0-9a-f]{4})\n")
# v9 binds a raw CPU sample only to the most recent recorder-confirmed
# host-to-core delivery. It never states that the guest polled or accepted it.
RAW_PC_V9_LINE = re.compile(
    r"raw-pc ([1-9][0-9]*) cycles=([0-9]+) pc=0x([0-9a-f]{8}) "
    r"ir_opcode=0x([0-9a-f]{4}) memory_opcode=0x([0-9a-f]{4}) "
    r"d0=0x([0-9a-f]{8}) a0=0x([0-9a-f]{8}) a6=0x([0-9a-f]{8}) sr=0x([0-9a-f]{4}) "
    r"input_ordinal=([0-9]+) input_frame=(-?[0-9]+)\n")
# v10 has one explicit arm at the recovered title site, followed by bounded
# writes to a deliberately small custom-chip display register allow-list.  It
# records neither bitplane contents nor a claim that a title frame was shown.
TITLE_DISPLAY_ARM_LINE = re.compile(
    r"display-arm 1 cycles=([0-9]+) site=0x0001eda6 "
    r"input_ordinal=([0-9]+) input_frame=(-?[0-9]+)\n")
TITLE_DISPLAY_WRITE_LINE = re.compile(
    r"display-write ([1-9][0-9]*) cycles=([0-9]+) vpos=([0-9]+) hpos=([0-9]+) "
    r"origin=(cpu|copper) register=0x([0-9a-f]{4}) value=0x([0-9a-f]{4}) "
    r"input_ordinal=([0-9]+) input_frame=(-?[0-9]+)\n")
TITLE_DISPLAY_REGISTERS = frozenset((
    0x0080, 0x0082, 0x008E, 0x0090, 0x0092, 0x0094,
    *range(0x00E0, 0x00F0, 2), 0x0100, 0x0108, 0x010A,
    *range(0x0180, 0x01C0, 2),
))
# The reviewed FS-UAE host-delivery observer prints raw signed integer action
# fields. They remain opaque delivery observations; this grammar proves only
# that an external receipt has not been hand-edited into an arbitrary file.
HOST_INPUT_LINE = re.compile(
    r"host-input ([1-9][0-9]*) frame=(-?[0-9]+) line=(-?[0-9]+) "
    r"action=(-?[0-9]+) state=(-?[0-9]+)\n")
# Receipt v6 additionally binds the finite recorder timing profile. Older
# evidence remains verifiable without pretending it has the newer field.
CAPTURE_RECEIPT_VERSION = "11"


class CaptureError(RuntimeError):
    """A local preflight failure that must not create an admitted capture."""


class RecorderConsoleStatus:
    """Hash-bound identity for a bounded external console transcript."""

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


def validate_identity(path: Path, label: str, expected_hash: str, expected_size: int) -> tuple[str, int]:
    digest, size = sha256_file(path)
    if digest != expected_hash or size != expected_size:
        raise CaptureError(f"{label} is not the exact recognised source")
    return digest, size


def validate_recorder(path: Path) -> tuple[str, int]:
    digest, size = sha256_file(path)
    if digest != EXPECTED_RECORDER_SHA256:
        raise CaptureError("recorder hash does not match the reviewed FS-UAE binary")
    return digest, size


def is_system_tmp_path(path: Path) -> bool:
    """Reject the operator's `/tmp` spelling before host path resolution.

    `/tmp` becomes `/private/tmp` on macOS and a drive-rooted path on Windows,
    so a resolved-path-only test would make the external-evidence contract
    host dependent.
    """
    normalized = path.as_posix().replace("\\", "/")
    if normalized == "/tmp" or normalized.startswith("/tmp/"):
        return True
    parts = path.parts
    return bool(path.anchor and len(parts) > 1 and parts[1].casefold() == "tmp")


def reject_unsafe_output(*sources: Path, output: Path) -> Path:
    # Reject this contractual spelling before Windows can classify its
    # POSIX-looking form as relative to the current drive.
    if is_system_tmp_path(output):
        raise CaptureError("output must not use /tmp; use a Project Eon cache path")
    if not output.is_absolute():
        raise CaptureError("output path must be absolute")
    if output.exists() or output.is_symlink():
        raise CaptureError("output directory must not exist")
    resolved = output.resolve(strict=False)
    if resolved == ROOT or ROOT in resolved.parents:
        raise CaptureError("output must stay outside the repository")
    if any(source.parent == resolved or source.parent in resolved.parents for source in sources):
        raise CaptureError("output must stay outside supplied-media directories")
    if not resolved.parent.is_dir() or resolved.parent.is_symlink():
        raise CaptureError("output parent must be an existing non-symlink directory")
    return resolved


def require_visible_operator_input(environment: dict[str, str]) -> None:
    if environment.get("SDL_VIDEODRIVER", "").lower() == "dummy":
        raise CaptureError("headless SDL is forbidden; a physical operator must use the visible emulator window")
    if not environment.get("DISPLAY") and not environment.get("WAYLAND_DISPLAY"):
        raise CaptureError("a visible X11 or Wayland display is required for physical input capture")


def mount_options(mountpoint: Path) -> set[str]:
    completed = subprocess.run(
        ["findmnt", "-T", str(mountpoint), "-no", "OPTIONS"], check=True,
        capture_output=True, text=True)
    return set(completed.stdout.strip().split(","))


def mountpoint_is_active(mountpoint: Path) -> bool:
    """Return whether this exact path, not an ancestor, is still mounted."""
    try:
        completed = subprocess.run(
            ["findmnt", "-n", "--mountpoint", str(mountpoint), "-o", "TARGET"],
            check=False, capture_output=True, text=True)
    except FileNotFoundError:
        # This Linux-specific FUSE cleanup probe has no Windows equivalent.
        # A physical Amiga capture cannot run there, while test preflight must
        # remain a safe negative result rather than a host-tool crash.
        return False
    return completed.returncode == 0 and completed.stdout.strip() == str(mountpoint)


def unmount(mountpoint: Path) -> None:
    """Unmount and prove the FUSE view is gone before reusing the evidence root."""
    if not mountpoint_is_active(mountpoint):
        return
    completed = subprocess.run(["fusermount", "-u", str(mountpoint)], check=False,
                               capture_output=True, text=True)
    if completed.returncode != 0 or mountpoint_is_active(mountpoint):
        raise CaptureError(f"unable to unmount read-only capture view: {mountpoint}")


def mount_read_only(source: Path, mountpoint: Path) -> None:
    subprocess.run(["archivemount", "-o", "ro", str(source), str(mountpoint)], check=True)
    required = {"ro", "nosuid", "nodev", "default_permissions"}
    if not required <= mount_options(mountpoint):
        unmount(mountpoint)
        raise CaptureError("archivemount did not report required read-only safety options")


def write_exclusive(path: Path, content: str) -> None:
    with path.open("x", encoding="utf-8", newline="\n") as stream:
        stream.write(content)
        stream.flush()
        os.fsync(stream.fileno())


def input_receipt_status(path: Path) -> str:
    """Describe a recorder-created host-input receipt without creating one.

    An absent receipt proves neither a user action nor a game-input outcome;
    it only means the recorder did not observe a host input event.  Keeping
    that distinction in the external status prevents a no-input preflight
    from looking like an interactive capture.
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


def capture_intent_status(intent: str, receipt_status: str, observed_during_capture: bool) -> str:
    """Fail closed when the stated physical-input/no-input session disagrees.

    The receipt only proves FS-UAE's host-to-core delivery observation.  It
    does not infer that Deuteros polled, accepted, or interpreted that input.
    """
    if intent not in CAPTURE_INTENTS:
        raise CaptureError("capture intent is not in the reviewed finite set")
    fields = dict(line.split("=", 1) for line in receipt_status.splitlines())
    state = fields.get("host_input_receipt")
    if intent == "physical-input":
        if state != "present" or not observed_during_capture:
            raise CaptureError("physical-input capture requires an observed non-empty host-input receipt")
        requirement = "required"
    else:
        if state not in {"absent", "empty"} or observed_during_capture:
            raise CaptureError("diagnostic-no-input capture must not retain host input")
        requirement = "forbidden"
    return (f"capture_intent={intent}\n"
            f"capture_intent_input_requirement={requirement}\n")


def capture_operator_instructions(intent: str) -> tuple[str, str, str]:
    """Return visible instructions that cannot contradict the capture intent."""
    if intent == "physical-input":
        return (
            "CAPTURE PREPARED  read-only original media; physical operator input required",
            "Focus the visible FS-UAE window by clicking it yourself; do not use terminal or automation input. Then press and release ordinary mapped keys only in that window (for example Return, Space, or arrows).",
            "No debugger, playback, injected host event, or guest-memory edit is permitted.",
        )
    if intent == "diagnostic-no-input":
        return (
            "CAPTURE PREPARED  read-only original media; diagnostic no-input capture",
            "The FS-UAE window remains visible for observation only. Do not click it or press any key in it.",
            "No host input, debugger, playback, injected host event, or guest-memory edit is permitted.",
        )
    raise CaptureError("capture intent is not in the reviewed finite set")


def parse_host_input_records(path: Path) -> list[tuple[int, int]]:
    """Validate finite host-to-core delivery records and retain ordinal/frame."""
    try:
        text = path.read_text(encoding="ascii")
    except UnicodeDecodeError as error:
        raise CaptureError("host-input receipt is not ASCII recorder output") from error
    if not text.endswith("\n"):
        raise CaptureError("host-input receipt has a truncated final record")
    records: list[tuple[int, int]] = []
    for expected, line in enumerate(text.splitlines(keepends=True), start=1):
        match = HOST_INPUT_LINE.fullmatch(line)
        if not match:
            raise CaptureError("host-input receipt contains an invalid recorder record")
        if int(match.group(1)) != expected:
            raise CaptureError("host-input receipt record ordinals are not contiguous")
        records.append((int(match.group(1)), int(match.group(2))))
        if len(records) > MAX_INPUT_RECEIPT_RECORDS:
            raise CaptureError("host-input receipt exceeds the recorder record cap")
    return records


def parse_host_input_receipt(path: Path) -> int:
    """Validate only the recorder's finite host-to-core delivery grammar."""
    return len(parse_host_input_records(path))


def raw_observation_status(path: Path, name: str, raw_format: str = "legacy") -> str:
    """Bind strict raw-PC observations without assigning runtime semantics."""
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
    site_counts, site_opcode_pairs = parse_raw_pc_summary(path, raw_format)
    digest, size = sha256_file(path)
    ordered_counts = ",".join(
        f"0x{site:08x}:{site_counts[site]}" for site in RAW_PC_SITES if site in site_counts)
    status = (f"{name}=present\n{name}_sha256={digest}\n{name}_bytes={size}\n"
              f"{name}_format={raw_format}\n{name}_records={sum(site_counts.values())}\n"
              f"{name}_site_counts={ordered_counts}\n")
    if raw_format in {"v7", "v9"}:
        ordered_pairs = ",".join(
            f"0x{site:08x}:" + "+".join(
                f"{ir:04x}/{memory:04x}" for ir, memory in sorted(site_opcode_pairs[site]))
            for site in RAW_PC_SITES if site in site_opcode_pairs)
        status += f"{name}_opcode_pairs={ordered_pairs}\n"
    if raw_format == "v9":
        links = parse_raw_pc_input_links(path)
        status += (f"{name}_input_links={sum(ordinal != 0 for ordinal, _ in links)}\n"
                   f"{name}_last_input_ordinal={links[-1][0] if links else 0}\n")
    return status


def parse_raw_pc_observations(path: Path, raw_format: str = "legacy") -> dict[int, int]:
    """Validate the recorder grammar and return reachability counts only.

    This deliberately does not use register values as recovered ABI facts.
    It makes malformed, reordered, over-cap, or unreviewed-site recorder
    output a failed external capture instead of an opaque hash-bound blob.
    """
    return parse_raw_pc_summary(path, raw_format)[0]


def _parse_raw_pc(path: Path, raw_format: str) -> tuple[dict[int, int], dict[int, set[tuple[int, int]]], list[tuple[int, int]]]:
    """Validate raw records and retain opaque v7/v9 fields per probe site."""
    if raw_format == "legacy":
        matcher = RAW_PC_LEGACY_LINE
    elif raw_format == "v7":
        matcher = RAW_PC_V7_LINE
    elif raw_format == "v9":
        matcher = RAW_PC_V9_LINE
    else:
        raise CaptureError("raw_pc format is not a reviewed recorder grammar")
    try:
        text = path.read_text(encoding="ascii")
    except UnicodeDecodeError as error:
        raise CaptureError("raw_pc is not ASCII recorder output") from error
    if not text.endswith("\n"):
        raise CaptureError("raw_pc has a truncated final record")
    counts: dict[int, int] = {}
    opcode_pairs: dict[int, set[tuple[int, int]]] = {}
    input_links: list[tuple[int, int]] = []
    previous_cycle = -1
    for expected_ordinal, line in enumerate(text.splitlines(keepends=True), start=1):
        match = matcher.fullmatch(line)
        if not match:
            raise CaptureError("raw_pc contains an invalid recorder record")
        ordinal, cycle, site = int(match.group(1)), int(match.group(2)), int(match.group(3), 16)
        if ordinal != expected_ordinal:
            raise CaptureError("raw_pc record ordinals are not contiguous")
        if cycle < previous_cycle:
            raise CaptureError("raw_pc cycles are not monotonic")
        previous_cycle = cycle
        if site not in RAW_PC_SITES:
            raise CaptureError("raw_pc uses an unreviewed probe site")
        counts[site] = counts.get(site, 0) + 1
        if raw_format in {"v7", "v9"}:
            opcode_pairs.setdefault(site, set()).add((int(match.group(4), 16), int(match.group(5), 16)))
        if raw_format == "v9":
            input_ordinal, input_frame = int(match.group(10)), int(match.group(11))
            if input_ordinal > MAX_INPUT_RECEIPT_RECORDS:
                raise CaptureError("raw_pc exceeds the input-recorder ordinal cap")
            if input_links and input_ordinal < input_links[-1][0]:
                raise CaptureError("raw_pc input ordinals are not monotonic")
            if input_ordinal == 0 and input_frame != 0:
                raise CaptureError("raw_pc no-input snapshot must use frame zero")
            input_links.append((input_ordinal, input_frame))
        if counts[site] > MAX_RAW_RECORDS_PER_SITE:
            raise CaptureError("raw_pc exceeds the per-site recorder cap")
        if expected_ordinal > MAX_RAW_RECORDS:
            raise CaptureError("raw_pc exceeds the recorder record cap")
    return counts, opcode_pairs, input_links


def parse_raw_pc_summary(path: Path, raw_format: str = "legacy") -> tuple[dict[int, int], dict[int, set[tuple[int, int]]]]:
    """Validate raw records and retain opaque IR/memory pairs per probe site."""
    counts, opcode_pairs, _ = _parse_raw_pc(path, raw_format)
    return counts, opcode_pairs


def parse_raw_pc_input_links(path: Path) -> list[tuple[int, int]]:
    """Return v9 delivery chronology without promoting it to guest input proof."""
    return _parse_raw_pc(path, "v9")[2]


def raw_pc_input_chronology_status(raw_path: Path, input_path: Path) -> str:
    """Cross-bind v9 CPU samples to prior recorder-confirmed host delivery."""
    links = parse_raw_pc_input_links(raw_path)
    if not any(ordinal for ordinal, _ in links):
        return "raw_pc_input_chronology=none\n"
    by_ordinal = dict(parse_host_input_records(input_path))
    for ordinal, frame in links:
        if ordinal and by_ordinal.get(ordinal) != frame:
            raise CaptureError("raw_pc input chronology does not match the host-input receipt")
    return ("raw_pc_input_chronology=linked\n"
            f"raw_pc_input_chronology_records={sum(ordinal != 0 for ordinal, _ in links)}\n")


def _validate_title_display_input_links(links: list[tuple[int, int]], input_path: Path) -> str:
    """Bind opaque display records to prior recorder delivery, never guest input."""
    if not any(ordinal for ordinal, _ in links):
        return "none"
    by_ordinal = dict(parse_host_input_records(input_path))
    for ordinal, frame in links:
        if ordinal and by_ordinal.get(ordinal) != frame:
            raise CaptureError("title-display input chronology does not match the host-input receipt")
    return "linked"


def parse_title_display_receipt(path: Path) -> tuple[int, dict[int, int], list[tuple[int, int]], int]:
    """Validate v10's title-armed custom-chip write receipt without display inference."""
    try:
        text = path.read_text(encoding="ascii")
    except UnicodeDecodeError as error:
        raise CaptureError("title-display receipt is not ASCII recorder output") from error
    if not text.endswith("\n"):
        raise CaptureError("title-display receipt has a truncated final record")
    lines = text.splitlines(keepends=True)
    if not lines:
        raise CaptureError("title-display receipt has no arm record")
    arm = TITLE_DISPLAY_ARM_LINE.fullmatch(lines[0])
    if not arm:
        raise CaptureError("title-display receipt must begin with the reviewed title arm")
    arm_cycles = int(arm.group(1))
    links = [(int(arm.group(2)), int(arm.group(3)))]
    if links[0][0] > MAX_INPUT_RECEIPT_RECORDS:
        raise CaptureError("title-display receipt exceeds the input-recorder ordinal cap")
    if links[0][0] == 0 and links[0][1] != 0:
        raise CaptureError("title-display no-input snapshot must use frame zero")
    counts: dict[int, int] = {}
    previous_cycles = arm_cycles
    for expected_ordinal, line in enumerate(lines[1:], start=2):
        match = TITLE_DISPLAY_WRITE_LINE.fullmatch(line)
        if not match:
            raise CaptureError("title-display receipt contains an invalid recorder record")
        ordinal, cycles, vpos, hpos, register = (int(match.group(1)), int(match.group(2)),
                                                   int(match.group(3)), int(match.group(4)),
                                                   int(match.group(6), 16))
        input_link = (int(match.group(8)), int(match.group(9)))
        if ordinal != expected_ordinal:
            raise CaptureError("title-display receipt record ordinals are not contiguous")
        if cycles < previous_cycles:
            raise CaptureError("title-display receipt cycles are not monotonic")
        if vpos > 0xffff or hpos > 0xffff:
            raise CaptureError("title-display receipt position exceeds the reviewed unsigned-16 range")
        if register not in TITLE_DISPLAY_REGISTERS:
            raise CaptureError("title-display receipt writes an unreviewed display register")
        if input_link[0] > MAX_INPUT_RECEIPT_RECORDS:
            raise CaptureError("title-display receipt exceeds the input-recorder ordinal cap")
        if input_link[0] == 0 and input_link[1] != 0:
            raise CaptureError("title-display no-input snapshot must use frame zero")
        if input_link[0] < links[-1][0]:
            raise CaptureError("title-display input ordinals are not monotonic")
        counts[register] = counts.get(register, 0) + 1
        if counts[register] > MAX_TITLE_DISPLAY_WRITES_PER_REGISTER:
            raise CaptureError("title-display receipt exceeds the per-register recorder cap")
        previous_cycles = cycles
        links.append(input_link)
        if expected_ordinal > MAX_TITLE_DISPLAY_WRITES + 1:
            raise CaptureError("title-display receipt exceeds the recorder record cap")
    return arm_cycles, counts, links, len(lines) - 1


def title_display_receipt_status(path: Path, input_path: Path) -> str:
    """Hash-bind v10 display observations while admitting no display conclusion."""
    try:
        info = path.lstat()
    except FileNotFoundError:
        return "title_display=absent\n"
    if stat.S_ISLNK(info.st_mode) or not stat.S_ISREG(info.st_mode):
        raise CaptureError("title-display receipt is not a regular non-symlink file")
    if info.st_size > MAX_TITLE_DISPLAY_RECEIPT_BYTES:
        raise CaptureError("title-display receipt exceeds the bounded recorder contract")
    if info.st_size == 0:
        return "title_display=empty\n"
    arm_cycles, counts, links, writes = parse_title_display_receipt(path)
    chronology = _validate_title_display_input_links(links, input_path)
    digest, size = sha256_file(path)
    register_counts = ",".join(
        f"0x{register:04x}:{counts[register]}" for register in sorted(counts))
    return ("title_display=present\n"
            f"title_display_sha256={digest}\n"
            f"title_display_bytes={size}\n"
            "title_display_format=v10\n"
            f"title_display_records={writes + 1}\n"
            f"title_display_arm_cycles={arm_cycles}\n"
            f"title_display_writes={writes}\n"
            f"title_display_register_counts={register_counts}\n"
            f"title_display_input_links={sum(ordinal != 0 for ordinal, _ in links)}\n"
            f"title_display_last_input_ordinal={links[-1][0]}\n"
            f"title_display_input_chronology={chronology}\n"
            f"title_display_input_chronology_records={sum(ordinal != 0 for ordinal, _ in links)}\n")


def capture_bounded_console(stream, path: Path, over_limit: threading.Event) -> RecorderConsoleStatus:
    """Drain a console into fixed storage and signal if its total safety cap trips.

    Continue reading after the signal until the owner kills the child: otherwise
    the child could block on a full stdout pipe before the runner can preserve
    a coherent, bounded diagnostic receipt.
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
    """Place both pre- and post-capture identities in the external receipt."""
    digest, size = identity
    return f"{name}_sha256={digest}\n{name}_bytes={size}\n"


def recorder_config(disk1: Path, disk2: Path, kickstart: Path, output: Path,
                    timing_profile: str = "realtime") -> str:
    try:
        warp_mode = TIMING_PROFILES[timing_profile]
    except KeyError as error:
        raise CaptureError("timing profile is not in the reviewed finite profile set") from error
    return "\n".join((
        "# Ephemeral physical-input capture configuration; no debugger or playback.",
        "amiga_model = A500",
        f"kickstart_file = {kickstart}",
        f"floppy_drive_0 = {disk1}",
        f"floppy_drive_1 = {disk2}",
        "floppy_write_protect = 1",
        f"base_dir = {output / 'runtime'}",
        f"logs_dir = {output / 'logs'}",
        f"save_states_dir = {output / 'states'}",
        "fullscreen = 0",
        "window_width = 640",
        "window_height = 512",
        "console_debugger = 0",
        "use_debugger = 0",
        "uae_sound_output = interrupts",
        f"warp_mode = {warp_mode}",
        "",
    ))


def input_delivery_file_observed(path: Path) -> bool:
    """Return whether the recorder has begun an input-delivery receipt.

    This is deliberately only a live operator aid. The file remains subject to
    complete bounded grammar and hash validation after FS-UAE has stopped;
    parsing it while the recorder is appending could race a partial line.
    """
    try:
        info = path.lstat()
    except FileNotFoundError:
        return False
    if not stat.S_ISREG(info.st_mode) or stat.S_ISLNK(info.st_mode):
        raise CaptureError("live host-input receipt is not a regular non-symlink file")
    return info.st_size > 0


def run_capture(args: argparse.Namespace) -> Path:
    release = require_absolute_regular_file(Path(args.source_release), "source release")
    kickstart = require_absolute_regular_file(Path(args.kickstart_archive), "Kickstart archive")
    recorder = require_absolute_regular_file(Path(args.recorder), "recorder", executable=True)
    output = reject_unsafe_output(release, kickstart, output=Path(args.output))
    if not MIN_DURATION_SECONDS <= args.duration_seconds <= MAX_DURATION_SECONDS:
        raise CaptureError(f"duration must be between {MIN_DURATION_SECONDS} and {MAX_DURATION_SECONDS} seconds")
    if not 0 <= args.focus_settle_seconds <= MAX_FOCUS_SETTLE_SECONDS:
        raise CaptureError(f"focus-settle duration must be between 0 and {MAX_FOCUS_SETTLE_SECONDS} seconds")
    environment = dict(os.environ)
    require_visible_operator_input(environment)
    release_before = validate_identity(release, "source release", EXPECTED_RELEASE_SHA256, EXPECTED_RELEASE_SIZE)
    kickstart_before = validate_identity(kickstart, "Kickstart archive", EXPECTED_KICKSTART_SHA256, EXPECTED_KICKSTART_SIZE)
    recorder_identity = validate_recorder(recorder)
    output.mkdir(mode=0o700)
    mounts = [output / name for name in ("release-outer-ro", "disk1-ro", "disk2-ro", "kickstart-ro")]
    for mountpoint in mounts:
        mountpoint.mkdir(mode=0o700)
    mounted: list[Path] = []
    try:
        mount_read_only(release, mounts[0])
        mounted.append(mounts[0])
        disk1_archive = mounts[0] / DISK1_ARCHIVE
        disk2_archive = mounts[0] / DISK2_ARCHIVE
        disk1_archive_identity = validate_identity(
            disk1_archive, "Deuteros disk 1 nested archive", EXPECTED_DISK1_ARCHIVE_SHA256,
            EXPECTED_DISK1_ARCHIVE_SIZE)
        disk2_archive_identity = validate_identity(
            disk2_archive, "Deuteros disk 2 nested archive", EXPECTED_DISK2_ARCHIVE_SHA256,
            EXPECTED_DISK2_ARCHIVE_SIZE)
        mount_read_only(disk1_archive, mounts[1])
        mounted.append(mounts[1])
        mount_read_only(disk2_archive, mounts[2])
        mounted.append(mounts[2])
        mount_read_only(kickstart, mounts[3])
        mounted.append(mounts[3])
        disk1 = mounts[1] / DISK1_IMAGE
        disk2 = mounts[2] / DISK2_IMAGE
        rom = mounts[3] / KICKSTART_IMAGE
        validate_identity(disk1, "clean Deuteros disk 1", EXPECTED_DISK1_SHA256, 901_120)
        validate_identity(disk2, "clean Deuteros disk 2", EXPECTED_DISK2_SHA256, 901_120)
        validate_identity(rom, "Kickstart ROM", EXPECTED_ROM_SHA256, 262_144)
        configuration = output / "deuteros-amiga-capture.fs-uae"
        write_exclusive(configuration, recorder_config(
            disk1, disk2, rom, output, args.timing_profile))
        configuration_identity = sha256_file(configuration)
        command = [str(recorder), str(configuration)]
        write_exclusive(output / "command-tail.txt", " ".join(command) + "\n")
        environment.update({
            "PROJECT_EON_FS_UAE_RAW_RECORD": str(output / "raw-pc.txt"),
            "PROJECT_EON_FS_UAE_INPUT_RECORD": str(output / "host-input-receipt.txt"),
            "PROJECT_EON_FS_UAE_DISPLAY_RECORD": str(output / "title-display.txt"),
        })
        for instruction in capture_operator_instructions(args.capture_intent):
            print(instruction)
        print(f"The {args.focus_settle_seconds}-second focus-settle window begins now; the {args.duration_seconds}-second capture window follows.")
        started = time.time()
        process = subprocess.Popen(command, env=environment, stdout=subprocess.PIPE,
                                   stderr=subprocess.STDOUT)
        assert process.stdout is not None
        console_result: list[RecorderConsoleStatus] = []
        console_errors: list[BaseException] = []
        console_over_limit = threading.Event()

        def drain_console() -> None:
            try:
                console_result.append(capture_bounded_console(
                    process.stdout, output / "recorder-console.log", console_over_limit))
            except BaseException as error:  # Report it after process cleanup.
                console_errors.append(error)

        console_thread = threading.Thread(target=drain_console,
                                          name="project-eon-fs-uae-console", daemon=True)
        console_thread.start()
        settle_deadline = time.monotonic() + args.focus_settle_seconds
        deadline: float | None = None
        live_input_observed = False
        while True:
            exit_status = process.poll()
            if exit_status is not None:
                break
            now = time.monotonic()
            if deadline is None and now >= settle_deadline:
                deadline = now + args.duration_seconds
                requirement = ("physical input remains required" if args.capture_intent == "physical-input"
                               else "no host input is permitted")
                print(f"CAPTURE WINDOW ACTIVE  {args.duration_seconds} seconds; {requirement}.")
            if not live_input_observed and input_delivery_file_observed(output / "host-input-receipt.txt"):
                live_input_observed = True
                print("HOST INPUT DELIVERY OBSERVED  receipt will be fully validated after capture.")
            remaining = (settle_deadline - now if deadline is None else deadline - now)
            if console_over_limit.is_set():
                process.kill()
                process.wait()
                exit_status = 125
                break
            if remaining <= 0:
                process.kill()
                process.wait()
                exit_status = 124
                break
            console_over_limit.wait(timeout=min(0.1, remaining))
        console_thread.join()
        if console_errors:
            raise CaptureError(f"unable to retain bounded recorder console: {console_errors[0]}")
        if len(console_result) != 1:
            raise CaptureError("bounded recorder console did not produce exactly one receipt")
        ended = time.time()
        release_after = validate_identity(release, "source release", EXPECTED_RELEASE_SHA256, EXPECTED_RELEASE_SIZE)
        if release_before != release_after:
            raise CaptureError("source release changed during capture; evidence is rejected")
        kickstart_after = validate_identity(kickstart, "Kickstart archive", EXPECTED_KICKSTART_SHA256, EXPECTED_KICKSTART_SIZE)
        if kickstart_before != kickstart_after:
            raise CaptureError("Kickstart archive changed during capture; evidence is rejected")
        receipt_status = input_receipt_status(output / "host-input-receipt.txt")
        intent_status = capture_intent_status(args.capture_intent, receipt_status,
                                              live_input_observed)
        raw_path = output / "raw-pc.txt"
        input_path = output / "host-input-receipt.txt"
        observation_status = raw_observation_status(raw_path, "raw_pc", "v9")
        chronology_status = (raw_pc_input_chronology_status(raw_path, input_path)
                             if "raw_pc=present\n" in observation_status else "")
        display_status = title_display_receipt_status(output / "title-display.txt", input_path)
        write_exclusive(output / "run-status.txt",
                        f"capture_receipt_version={CAPTURE_RECEIPT_VERSION}\n"
                        f"timing_profile={args.timing_profile}\n"
                        f"focus_settle_seconds={args.focus_settle_seconds}\n"
                        f"host_input_observed_during_capture={'true' if live_input_observed else 'false'}\n"
                        f"exit_status={exit_status}\nstart_unix={started:.6f}\nend_unix={ended:.6f}\n"
                        + identity_status("source_release", release_after)
                        + identity_status("kickstart_archive", kickstart_after)
                        + identity_status("disk1_archive", disk1_archive_identity)
                        + identity_status("disk2_archive", disk2_archive_identity)
                        + identity_status("recorder", recorder_identity)
                        + identity_status("configuration", configuration_identity)
                        + intent_status + receipt_status + observation_status + chronology_status + display_status
                        + recorder_console_status(console_result[0]))
        if console_result[0].over_limit:
            raise CaptureError(
                "recorder console exceeded the 64 MiB safety cap; evidence was retained but is not admitted")
        print("CAPTURE FINISHED  external evidence only; host-input receipt status is in run-status.txt")
        return output
    finally:
        for mountpoint in reversed(mounted):
            unmount(mountpoint)


def parse_arguments(argv: list[str]) -> argparse.Namespace:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--source-release", required=True, help="Absolute recognised English Deuteros Amiga ZIP path")
    parser.add_argument("--kickstart-archive", required=True, help="Absolute recognised Kickstart 1.3 ZIP path")
    parser.add_argument("--recorder", required=True, help="Absolute reviewed FS-UAE recorder binary path")
    parser.add_argument("--output", required=True, help="New absolute cache directory for external capture evidence")
    parser.add_argument("--duration-seconds", type=int, default=120, help="Visible operator window duration (15-600; default: 120)")
    parser.add_argument("--focus-settle-seconds", type=int, default=10,
                        help="Manual visible-window focus time before capture (0-120; default: 10)")
    parser.add_argument("--capture-intent", choices=tuple(sorted(CAPTURE_INTENTS)), required=True,
                        help="Required operator declaration: physical-input or diagnostic-no-input")
    parser.add_argument("--timing-profile", choices=tuple(sorted(TIMING_PROFILES)), default="realtime",
                        help="Recorder timing profile (default: realtime; warp is diagnostic only)")
    return parser.parse_args(argv)


def main(argv: list[str] | None = None) -> int:
    try:
        output = run_capture(parse_arguments(sys.argv[1:] if argv is None else argv))
    except (CaptureError, OSError, subprocess.SubprocessError) as error:
        print(f"Deuteros Amiga capture preflight rejected: {error}", file=sys.stderr)
        return 2
    print(f"EXTERNAL CAPTURE DIRECTORY  {output}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
