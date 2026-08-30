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
import time


ROOT = Path(__file__).resolve().parents[1]
EXPECTED_RELEASE_SHA256 = "e6e7044b25877fdf8b10d16d2f395886d9957953144ae15ca630cda9cab2a123"
EXPECTED_RELEASE_SIZE = 328_383
GAME_ROOT = "millennium-return-to-earth-2-2"
MIN_DURATION_SECONDS = 15
MAX_DURATION_SECONDS = 600


class CaptureError(RuntimeError):
    """A local preflight failure that must not create a capture receipt."""


def sha256_file(path: Path) -> tuple[str, int]:
    digest = hashlib.sha256()
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


def reject_unsafe_output(source: Path, output: Path) -> Path:
    if not output.is_absolute():
        raise CaptureError("output path must be absolute")
    if output.exists() or output.is_symlink():
        raise CaptureError("output directory must not exist")
    resolved = output.resolve(strict=False)
    if resolved == ROOT or ROOT in resolved.parents:
        raise CaptureError("output must stay outside the repository")
    if source.parent == resolved or source.parent in resolved.parents:
        raise CaptureError("output must stay outside the supplied-media directory")
    if str(resolved) == "/tmp" or Path("/tmp") in resolved.parents:
        raise CaptureError("output must not use /tmp; use a Project Eon cache path")
    parent = resolved.parent
    if not parent.is_dir() or parent.is_symlink():
        raise CaptureError("output parent must be an existing non-symlink directory")
    return resolved


def validate_source_release(source: Path) -> tuple[str, int]:
    digest, size = sha256_file(source)
    if digest != EXPECTED_RELEASE_SHA256 or size != EXPECTED_RELEASE_SIZE:
        raise CaptureError("source release is not the exact recognised English Millennium DOS archive")
    return digest, size


def require_visible_operator_input(environment: dict[str, str]) -> None:
    if environment.get("SDL_VIDEODRIVER", "").lower() == "dummy":
        raise CaptureError("headless SDL is forbidden; a physical operator must use the visible emulator window")
    if not environment.get("DISPLAY") and not environment.get("WAYLAND_DISPLAY"):
        raise CaptureError("a visible X11 or Wayland display is required for physical input capture")


def recorder_config(game_root: Path) -> str:
    """Return the fixed normal-core configuration with one read-only game root."""
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
    digest, size = sha256_file(path)
    return ("host_input_receipt=present\n"
            f"host_input_receipt_sha256={digest}\n"
            f"host_input_receipt_bytes={size}\n")


def run_capture(args: argparse.Namespace) -> Path:
    source = require_absolute_regular_file(Path(args.source_release), "source release")
    recorder = require_absolute_regular_file(Path(args.recorder), "recorder", executable=True)
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
        try:
            completed = subprocess.run(command, env=environment, timeout=args.duration_seconds,
                                       check=False)
            exit_status = completed.returncode
        except subprocess.TimeoutExpired:
            exit_status = 124
        ended = time.time()
        after_hash, after_size = validate_source_release(source)
        if (source_hash, source_size) != (after_hash, after_size):
            raise CaptureError("source archive changed during capture; evidence is rejected")
        receipt_status = input_receipt_status(output / "host-input-receipt.raw")
        write_exclusive(output / "run-status.txt",
                        f"exit_status={exit_status}\nstart_unix={started:.6f}\nend_unix={ended:.6f}\n"
                        + receipt_status)
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
