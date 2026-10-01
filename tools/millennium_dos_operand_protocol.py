"""Experimental schema-25 DOS title-operand receipt contract.

This deterministic parser validates one fixed observer record. It cannot
promote an experimental observer to preservation or recovery evidence.
"""
from __future__ import annotations

import hashlib
import os
from pathlib import Path
import re
import stat

SCHEMA_VERSION = "25"
CAPTURE_RECEIPT_VERSION = SCHEMA_VERSION
PROTOCOL = "v25-dos-title-operand"
RECORDER_ADMISSION = "experimental-observer-not-for-recovery"
OBSERVATION_NAME = "operand-observation.raw"
MAX_OBSERVATION_BYTES = 512
IMAGE_NAME = "TITLES.EXE"
IMAGE_SIZE = 7022
IMAGE_SHA256 = "3cc57f2b12a0da44dd43220f44f06a05b9e3f009bcf008b7bb87622a5988cbe6"
SPAN_SHA256 = "ed46676eb54a03e725cbb96371e4fd13852a350ba5b027e5c59dda07c78b8ecf"

# Keep the receipt deliberately small until a separately reviewed runner and
# pinned external recorder binary define additional provenance fields.
FIELDS = {
    "capture_receipt_version",
    "recorder_protocol",
    "recorder_admission",
    "operand_observation",
    "operand_observation_sha256",
    "operand_observation_bytes",
}

_PREFIX = (
    "schema=project-eon.dos-title-operand-observation/v1;"
    "protocol=v25-dos-title-operand;"
)
_PRESENT_PREFIX = (
    _PREFIX
    + "state=present;event=operand-read-observed-during-execution;"
    + "image_name=TITLES.EXE;image_size=7022;"
    + f"image_sha256={IMAGE_SHA256};"
)
_PRESENT_SUFFIX = (
    ";site_ip=13e9;file_offset=12e9;span_size=9;"
    + f"span_sha256={SPAN_SHA256};"
)
_PRESENT_RE = re.compile(
    re.escape(_PRESENT_PREFIX)
    + r"cs=([0-9a-f]{4})"
    + re.escape(_PRESENT_SUFFIX)
    + r"opcode=8a4401;ds=([0-9a-f]{4});si=([0-9a-f]{4});"
      r"logical_offset=([0-9a-f]{4});operand=([0-9a-f]{2})\n"
)
_ABSENT = _PREFIX + "state=absent\n"
_LOWER_HEX_256 = re.compile(r"[0-9a-f]{64}\Z")
_CANONICAL_UNSIGNED = re.compile(r"(?:0|[1-9][0-9]*)\Z")


def parse_observation(data: bytes) -> dict[str, str]:
    """Parse the exact bounded ASCII record and return its semantic fields."""
    if not isinstance(data, bytes) or len(data) > MAX_OBSERVATION_BYTES:
        raise ValueError("operand observation exceeds its byte bound")
    try:
        text = data.decode("ascii")
    except UnicodeDecodeError as error:
        raise ValueError("operand observation is not ASCII") from error

    if text == _ABSENT:
        return {"state": "absent"}

    match = _PRESENT_RE.fullmatch(text)
    if not match:
        raise ValueError("operand observation does not match the frozen grammar")
    cs, ds, si, logical_offset, operand = match.groups()
    expected_offset = (int(si, 16) + 1) & 0xFFFF
    if int(logical_offset, 16) != expected_offset:
        raise ValueError("operand logical offset does not match DS:SI+1")
    return {
        "state": "present",
        "cs": cs,
        "ds": ds,
        "si": si,
        "logical_offset": logical_offset,
        "operand": operand,
    }


def read_bounded(path: Path, limit: int = MAX_OBSERVATION_BYTES) -> bytes:
    """Read a bounded regular file after no-follow dirfd traversal."""
    if not path.is_absolute():
        raise ValueError("operand observation path must be absolute")
    parts = path.parts
    if len(parts) < 2 or any(part in {"", ".", ".."} for part in parts[1:]):
        raise ValueError("operand observation path is not canonical")
    if (os.open not in getattr(os, "supports_dir_fd", set())
            or not hasattr(os, "O_DIRECTORY") or not hasattr(os, "O_NOFOLLOW")):
        raise ValueError("safe descriptor-relative observation reads are unavailable on this platform")
    directory_flags = (os.O_RDONLY | os.O_DIRECTORY | os.O_NOFOLLOW
                       | getattr(os, "O_CLOEXEC", 0))
    file_flags = (os.O_RDONLY | getattr(os, "O_NOFOLLOW", 0)
                  | getattr(os, "O_NONBLOCK", 0) | getattr(os, "O_CLOEXEC", 0))
    root_fd = os.open(os.sep, directory_flags)
    current_dir_fd = root_fd
    try:
        for component in parts[1:-1]:
            next_fd = os.open(component, directory_flags, dir_fd=current_dir_fd)
            if not stat.S_ISDIR(os.fstat(next_fd).st_mode):
                os.close(next_fd)
                raise ValueError("operand observation ancestor is not a directory")
            if current_dir_fd != root_fd:
                os.close(current_dir_fd)
            current_dir_fd = next_fd
        fd = os.open(parts[-1], file_flags, dir_fd=current_dir_fd)
    finally:
        if current_dir_fd != root_fd:
            os.close(current_dir_fd)
        os.close(root_fd)
    try:
        before = os.fstat(fd)
        if not stat.S_ISREG(before.st_mode) or before.st_size > limit:
            raise ValueError("operand observation must be a bounded regular file")
        chunks = []
        remaining = limit + 1
        while remaining:
            block = os.read(fd, remaining)
            if not block:
                break
            chunks.append(block)
            remaining -= len(block)
        data = b"".join(chunks)
        after = os.fstat(fd)
        if (len(data) != before.st_size or len(data) > limit
                or (before.st_size, before.st_mtime_ns, before.st_ctime_ns)
                != (after.st_size, after.st_mtime_ns, after.st_ctime_ns)):
            raise ValueError("operand observation changed while being read")
        return data
    finally:
        os.close(fd)


def _identity(fields: dict[str, str], digest: str, size: int) -> None:
    if (fields.get("operand_observation_sha256"),
            fields.get("operand_observation_bytes")) != (digest, str(size)):
        raise ValueError("operand observation identity mismatch")


def verify_fields(fields: dict[str, str], directory: Path,
                  *, allow_experimental_observer: bool = False) -> None:
    """Verify exact schema-25 receipt fields and its sidecar artifact."""
    if not allow_experimental_observer:
        raise ValueError("experimental operand observer is not admitted for recovery")
    if set(fields) != FIELDS or any(not isinstance(value, str) for value in fields.values()):
        raise ValueError("operand receipt fields do not match its exact schema")
    expected = {
        "capture_receipt_version": SCHEMA_VERSION,
        "recorder_protocol": PROTOCOL,
        "recorder_admission": RECORDER_ADMISSION,
    }
    if any(fields.get(key) != value for key, value in expected.items()):
        raise ValueError("operand receipt is not diagnostics-only schema 25")
    state = fields["operand_observation"]
    if state not in {"present", "absent"}:
        raise ValueError("operand observation state must be present or absent")

    payload = read_bounded(directory / OBSERVATION_NAME)
    parsed = parse_observation(payload)
    if parsed["state"] != state:
        raise ValueError("operand observation state does not match its sidecar")
    digest = hashlib.sha256(payload).hexdigest()
    _identity(fields, digest, len(payload))
    if not _LOWER_HEX_256.fullmatch(fields["operand_observation_sha256"]):
        raise ValueError("operand observation hash is not canonical lowercase SHA-256")
    if not _CANONICAL_UNSIGNED.fullmatch(fields["operand_observation_bytes"]):
        raise ValueError("operand observation size is not canonical unsigned decimal")
