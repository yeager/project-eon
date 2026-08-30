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
EXPECTED_RECORDER_SHA256 = "ab53ed0ef1d921b7379f1668013da39b3a2d0bb41faa1eb6a7a5eb8a15f50325"
GAME_ROOT = "millennium-return-to-earth-2-2"
MIN_DURATION_SECONDS = 15
MAX_DURATION_SECONDS = 600
# The reviewed recorder caps host input at 256 short text records.  Keep a
# generous, fixed ceiling here so a damaged or substituted recorder cannot
# turn a receipt status check into unbounded host-side I/O.
MAX_INPUT_RECEIPT_BYTES = 64 * 1024
MAX_RAW_OBSERVATION_BYTES = 8 * 1024 * 1024
# A defective external recorder can print a tight exception loop. Retain a
# reviewable prefix while hashing and counting the complete byte stream, rather
# than letting terminal output or an evidence cache grow without limit.
MAX_RECORDER_CONSOLE_LOG_BYTES = 1024 * 1024
# Receipt v2 binds the retained console prefix as well as the complete stream.
# A verifier must never treat a pre-v2 receipt as if that missing integrity
# property had been observed.
CAPTURE_RECEIPT_VERSION = "2"


class CaptureError(RuntimeError):
    """A local preflight failure that must not create a capture receipt."""


class RecorderConsoleStatus:
    """Small importlib-safe value object for a bounded console receipt."""

    def __init__(self, total_bytes: int, sha256: str, retained_bytes: int, retained_sha256: str) -> None:
        self.total_bytes = total_bytes
        self.sha256 = sha256
        self.retained_bytes = retained_bytes
        self.retained_sha256 = retained_sha256

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
    if not output.is_absolute():
        raise CaptureError("output path must be absolute")
    if output.exists() or output.is_symlink():
        raise CaptureError("output directory must not exist")
    if is_system_tmp_path(output):
        raise CaptureError("output must not use /tmp; use a Project Eon cache path")
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


def validate_recorder(path: Path) -> tuple[str, int]:
    digest, size = sha256_file(path)
    if digest != EXPECTED_RECORDER_SHA256:
        raise CaptureError("recorder hash does not match the reviewed DOSBox-X build")
    return digest, size


def require_visible_operator_input(environment: dict[str, str]) -> None:
    if environment.get("SDL_VIDEODRIVER", "").lower() == "dummy":
        raise CaptureError("headless SDL is forbidden; a physical operator must use the visible emulator window")
    if not environment.get("DISPLAY") and not environment.get("WAYLAND_DISPLAY"):
        raise CaptureError("a visible X11 or Wayland display is required for physical input capture")


def recorder_config(game_root: Path) -> str:
    """Return the fixed, evidence-reviewed configuration with one read-only game root.

    Millennium's 16-bit startup reaches a documented word copy from the last
    offset in a segment.  The pinned recorder's default ``segment limits=true``
    turns that real-mode wrapping operation into an endless #GP diagnostic
    loop.  Disable only that emulator compatibility check; this does not
    alter guest memory, the original archive, or the physical-input policy.
    The resulting configuration remains an explicit, hash-bound capture
    preimage rather than an implicit runtime fallback.
    """
    return "\n".join((
        "[sdl]",
        "fullscreen=false",
        "output=surface",
        "vsync=false",
        "",
        "[dosbox]",
        "machine=svga_s3",
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
        f'mount c "{game_root}"',
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
    digest, size = sha256_file(path)
    return ("host_input_receipt=present\n"
            f"host_input_receipt_sha256={digest}\n"
            f"host_input_receipt_bytes={size}\n")


def raw_observation_status(path: Path, name: str) -> str:
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
    digest, size = sha256_file(path)
    return f"{name}=present\n{name}_sha256={digest}\n{name}_bytes={size}\n"


def capture_bounded_console(stream, path: Path) -> RecorderConsoleStatus:
    """Drain an emulator console while retaining only a bounded evidence prefix.

    The full transcript is hashed and counted as it is read. This preserves a
    stable identity for a pathological recorder output without keeping an
    unbounded error loop on disk or blocking the emulator on a full pipe.
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
            if retained < MAX_RECORDER_CONSOLE_LOG_BYTES:
                keep = chunk[:MAX_RECORDER_CONSOLE_LOG_BYTES - retained]
                destination.write(keep)
                retained_digest.update(keep)
                retained += len(keep)
        destination.flush()
        os.fsync(destination.fileno())
    return RecorderConsoleStatus(total, digest.hexdigest(), retained, retained_digest.hexdigest())


def recorder_console_status(status: RecorderConsoleStatus) -> str:
    return ("recorder_console=present\n"
            f"recorder_console_sha256={status.sha256}\n"
            f"recorder_console_total_bytes={status.total_bytes}\n"
            f"recorder_console_retained_bytes={status.retained_bytes}\n"
            f"recorder_console_retained_sha256={status.retained_sha256}\n"
            f"recorder_console_truncated={'true' if status.truncated else 'false'}\n")


def identity_status(name: str, identity: tuple[str, int]) -> str:
    """Retain the exact capture preimage without retaining supplied media."""
    digest, size = identity
    return f"{name}_sha256={digest}\n{name}_bytes={size}\n"


def run_capture(args: argparse.Namespace) -> Path:
    source = require_absolute_regular_file(Path(args.source_release), "source release")
    recorder = require_absolute_regular_file(Path(args.recorder), "recorder", executable=True)
    recorder_identity = validate_recorder(recorder)
    output = reject_unsafe_output(source, Path(args.output))
    if not MIN_DURATION_SECONDS <= args.duration_seconds <= MAX_DURATION_SECONDS:
        raise CaptureError(f"duration must be between {MIN_DURATION_SECONDS} and {MAX_DURATION_SECONDS} seconds")
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
        write_exclusive(configuration, recorder_config(game_root))
        configuration_identity = sha256_file(configuration)
        command = [str(recorder), "-conf", str(configuration), "-fastlaunch", "-c", "c:",
                   "-c", "mill.com 0"]
        write_exclusive(command_tail, " ".join(command) + "\n")
        environment.update({
            "PROJECT_EON_DOSBOX_X_RECORD": str(output / "events.raw"),
            "PROJECT_EON_DOSBOX_X_RESULT": str(output / "results.raw"),
            "PROJECT_EON_DOSBOX_X_INPUT_RECORD": str(output / "host-input-receipt.raw"),
        })
        print("CAPTURE PREPARED  read-only original archive; physical operator input required")
        print(f"Press keys only in the visible DOSBox-X window within {args.duration_seconds} seconds.")
        print("No AUTOTYPE, debugger input, or guest-memory injection is permitted.")
        started = time.time()
        process = subprocess.Popen(command, env=environment, stdout=subprocess.PIPE,
                                   stderr=subprocess.STDOUT)
        assert process.stdout is not None
        console_path = output / "recorder-console.log"
        console_result: list[RecorderConsoleStatus] = []
        console_errors: list[BaseException] = []

        def drain_console() -> None:
            try:
                console_result.append(capture_bounded_console(process.stdout, console_path))
            except BaseException as error:  # Propagate after process cleanup.
                console_errors.append(error)

        console_thread = threading.Thread(target=drain_console, name="project-eon-dos-console", daemon=True)
        console_thread.start()
        try:
            exit_status = process.wait(timeout=args.duration_seconds)
        except subprocess.TimeoutExpired:
            process.kill()
            process.wait()
            exit_status = 124
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
        observation_status = (raw_observation_status(output / "events.raw", "events_raw")
                              + raw_observation_status(output / "results.raw", "results_raw"))
        write_exclusive(output / "run-status.txt",
                        f"capture_receipt_version={CAPTURE_RECEIPT_VERSION}\n"
                        f"exit_status={exit_status}\nstart_unix={started:.6f}\nend_unix={ended:.6f}\n"
                        + identity_status("source_release", (after_hash, after_size))
                        + identity_status("recorder", recorder_identity)
                        + identity_status("configuration", configuration_identity)
                        + receipt_status + observation_status + recorder_console_status(console_result[0]))
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
