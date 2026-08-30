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
import stat
import subprocess
import sys
import time


ROOT = Path(__file__).resolve().parents[1]
EXPECTED_RELEASE_SHA256 = "f4dc8dd1c27c5d389837783becd9b95ab09b78baf40e94e39e2b7e590e470e04"
EXPECTED_RELEASE_SIZE = 4_066_771
EXPECTED_KICKSTART_SHA256 = "c9521c114900633c09317ca6ff979db7b9df34d3cb537de062f5d51811c42c04"
# This is the supplied ZIP size, not the 262,144-byte ROM payload size.
EXPECTED_KICKSTART_SIZE = 143_269
EXPECTED_RECORDER_SHA256 = "727bba3ac4bc78558b964d0f572c488a419cd0985d803979e047381d2cf34f93"
EXPECTED_DISK1_SHA256 = "6ea0cc68d3af37203a885032eddf7c28e839e6abb59d8c9cd3792f1308bdec38"
EXPECTED_DISK2_SHA256 = "99909db1e190be02e049084743af44f00e331be6bf2d97b4831ada5fe4c30b4a"
EXPECTED_ROM_SHA256 = "ee05862d8102a08436ac4056da7d549db31625c7d47b24dfb7b3c9a5c113ca53"
DISK1_ARCHIVE = "Deuteros - The Next Millennium (1991)(Activision)(M3)(Disk 1 of 2).zip"
DISK2_ARCHIVE = "Deuteros - The Next Millennium (1991)(Activision)(M3)(Disk 2 of 2).zip"
DISK1_IMAGE = "Deuteros - The Next Millennium (1991)(Activision)(M3)(Disk 1 of 2).adf"
DISK2_IMAGE = "Deuteros - The Next Millennium (1991)(Activision)(M3)(Disk 2 of 2).adf"
KICKSTART_IMAGE = "Kickstart v1.3 r34.005 (1987-12)(Commodore)(A500-A1000-A2000-CDTV)[!].rom"
MIN_DURATION_SECONDS = 15
MAX_DURATION_SECONDS = 600


class CaptureError(RuntimeError):
    """A local preflight failure that must not create an admitted capture."""


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


def validate_identity(path: Path, label: str, expected_hash: str, expected_size: int) -> tuple[str, int]:
    digest, size = sha256_file(path)
    if digest != expected_hash or size != expected_size:
        raise CaptureError(f"{label} is not the exact recognised source")
    return digest, size


def validate_recorder(path: Path) -> None:
    digest, _ = sha256_file(path)
    if digest != EXPECTED_RECORDER_SHA256:
        raise CaptureError("recorder hash does not match the reviewed FS-UAE binary")


def reject_unsafe_output(*sources: Path, output: Path) -> Path:
    if not output.is_absolute():
        raise CaptureError("output path must be absolute")
    if output.exists() or output.is_symlink():
        raise CaptureError("output directory must not exist")
    resolved = output.resolve(strict=False)
    if resolved == ROOT or ROOT in resolved.parents:
        raise CaptureError("output must stay outside the repository")
    if str(resolved) == "/tmp" or Path("/tmp") in resolved.parents:
        raise CaptureError("output must not use /tmp; use a Project Eon cache path")
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


def unmount(mountpoint: Path) -> None:
    subprocess.run(["fusermount", "-u", str(mountpoint)], check=False,
                   stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)


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


def recorder_config(disk1: Path, disk2: Path, kickstart: Path, output: Path) -> str:
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
        "warp_mode = 0",
        "",
    ))


def run_capture(args: argparse.Namespace) -> Path:
    release = require_absolute_regular_file(Path(args.source_release), "source release")
    kickstart = require_absolute_regular_file(Path(args.kickstart_archive), "Kickstart archive")
    recorder = require_absolute_regular_file(Path(args.recorder), "recorder", executable=True)
    output = reject_unsafe_output(release, kickstart, output=Path(args.output))
    if not MIN_DURATION_SECONDS <= args.duration_seconds <= MAX_DURATION_SECONDS:
        raise CaptureError(f"duration must be between {MIN_DURATION_SECONDS} and {MAX_DURATION_SECONDS} seconds")
    environment = dict(os.environ)
    require_visible_operator_input(environment)
    release_before = validate_identity(release, "source release", EXPECTED_RELEASE_SHA256, EXPECTED_RELEASE_SIZE)
    kickstart_before = validate_identity(kickstart, "Kickstart archive", EXPECTED_KICKSTART_SHA256, EXPECTED_KICKSTART_SIZE)
    validate_recorder(recorder)
    output.mkdir(mode=0o700)
    mounts = [output / name for name in ("release-outer-ro", "disk1-ro", "disk2-ro", "kickstart-ro")]
    for mountpoint in mounts:
        mountpoint.mkdir(mode=0o700)
    mounted: list[Path] = []
    try:
        mount_read_only(release, mounts[0])
        mounted.append(mounts[0])
        mount_read_only(mounts[0] / DISK1_ARCHIVE, mounts[1])
        mounted.append(mounts[1])
        mount_read_only(mounts[0] / DISK2_ARCHIVE, mounts[2])
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
        write_exclusive(configuration, recorder_config(disk1, disk2, rom, output))
        command = [str(recorder), str(configuration)]
        write_exclusive(output / "command-tail.txt", " ".join(command) + "\n")
        environment.update({
            "PROJECT_EON_FS_UAE_RAW_RECORD": str(output / "raw-pc.txt"),
            "PROJECT_EON_FS_UAE_INPUT_RECORD": str(output / "host-input-receipt.txt"),
        })
        print("CAPTURE PREPARED  read-only original media; physical operator input required")
        print(f"Press keys only in the visible FS-UAE window within {args.duration_seconds} seconds.")
        print("No debugger, playback, injected host event, or guest-memory edit is permitted.")
        started = time.time()
        try:
            completed = subprocess.run(command, env=environment, timeout=args.duration_seconds, check=False)
            exit_status = completed.returncode
        except subprocess.TimeoutExpired:
            exit_status = 124
        ended = time.time()
        write_exclusive(output / "run-status.txt", f"exit_status={exit_status}\nstart_unix={started:.6f}\nend_unix={ended:.6f}\n")
        if release_before != validate_identity(release, "source release", EXPECTED_RELEASE_SHA256, EXPECTED_RELEASE_SIZE):
            raise CaptureError("source release changed during capture; evidence is rejected")
        if kickstart_before != validate_identity(kickstart, "Kickstart archive", EXPECTED_KICKSTART_SHA256, EXPECTED_KICKSTART_SIZE):
            raise CaptureError("Kickstart archive changed during capture; evidence is rejected")
        print("CAPTURE COMPLETE  external evidence only; assemble and validate separately")
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
