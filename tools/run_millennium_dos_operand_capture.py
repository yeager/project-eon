#!/usr/bin/env python3
"""Run the experimental schema-25 DOS operand observer with visible input.

This is a diagnostics-only, Linux/trv2 development runner. It does not make
recovery evidence. Original archives are hashed in place and mounted read-only;
they are never unpacked, copied, installed, or modified by this helper.

The external DOSBox-X application binary is pinned below for this explicitly
experimental protocol. Its identity does not admit it as a preservation
recorder or make schema-25 receipts recovery evidence.
"""
from __future__ import annotations

import argparse
import hashlib
import importlib.util
import os
from pathlib import Path, PurePosixPath
import re
import stat
import subprocess
import sys


def load_tool(name: str):
    spec = importlib.util.spec_from_file_location(name, Path(__file__).with_name(name + ".py"))
    if spec is None or spec.loader is None:
        raise RuntimeError(f"cannot load {name}")
    module = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(module)
    return module


legacy = load_tool("run_millennium_dos_capture")
terminal = load_tool("run_millennium_dos_terminal_capture")
protocol = load_tool("millennium_dos_operand_protocol")

OUTPUT_ROOT = PurePosixPath("/home/trv2/.cache/project-eon-tools/operand-captures")
OBSERVER_SOURCE_COMMIT = "234797680781567e18c374c9e62da24de5423db0"
OBSERVER_PATCH_SHA256 = "0829e00b19dba748ab7c9400a44e647002cd9a2cdfe88ccd700ad52d46c396b8"
# Reviewed trv2 experimental build; this is not a preservation-recorder pin.
EXPECTED_RECORDER_SHA256: str | None = "58cbb12e9baaae22877908193fdf72b72ed3f3f42b5b76bb9a91bc0b093fd245"
EXPECTED_RECORDER_SIZE: int | None = 125_297_584
MIN_DURATION_SECONDS = 15
MAX_DURATION_SECONDS = 600


def require_pinned_recorder() -> tuple[str, int]:
    digest = EXPECTED_RECORDER_SHA256
    size = EXPECTED_RECORDER_SIZE
    if (not isinstance(digest, str) or not re.fullmatch(r"[0-9a-f]{64}", digest)
            or not isinstance(size, int) or size <= 0):
        raise ValueError("schema-25 DOSBox-X executable has no reviewed pinned SHA-256 and size")
    return digest, size


def output_directory(source: Path, output: Path) -> Path:
    spelling = str(output)
    output_posix = PurePosixPath(spelling)
    if (not output.is_absolute() or str(output_posix) != spelling
            or output_posix.parent != OUTPUT_ROOT
            or not re.fullmatch(r"[A-Za-z0-9][A-Za-z0-9._-]*", output_posix.name)):
        raise ValueError("output must be a fresh named child of the external operand-captures cache")
    if output.exists() or output.is_symlink():
        raise ValueError("output directory must be fresh")
    for parent in (output, *output.parents):
        if parent.exists() or parent.is_symlink():
            info = parent.lstat()
            if not stat.S_ISDIR(info.st_mode) or stat.S_ISLNK(info.st_mode):
                raise ValueError("output ancestors must be real directories")
    root = output.parent
    if not root.is_dir():
        raise ValueError("external operand-captures cache must already exist")
    info = root.stat()
    if info.st_uid != os.geteuid() or info.st_mode & 0o022:
        raise ValueError("output parent must be privately controlled by the current user")
    return legacy.reject_unsafe_output(source, output)


def build_configuration(output: Path, machine_profile: str) -> str:
    game_root = output / "archive-ro" / legacy.GAME_ROOT
    base = legacy.recorder_config(
        game_root, machine_profile, release_sha256=legacy.EXPECTED_RELEASE_SHA256)
    sidecar = output / protocol.OBSERVATION_NAME
    return (base + "[project-eon-diagnostics-v25]\n"
            "enabled=true\n"
            f"sidecar_path={sidecar}\n")


def isolated_environment(output: Path, inherited: dict[str, str]) -> dict[str, str]:
    # Keep only display/session credentials required by a visible SDL window.
    keep = {"HOME", "USER", "LOGNAME", "LANG", "LC_ALL", "DISPLAY", "WAYLAND_DISPLAY",
            "XAUTHORITY", "XDG_RUNTIME_DIR", "DBUS_SESSION_BUS_ADDRESS"}
    environment = {key: value for key, value in inherited.items() if key in keep}
    environment.update({"PATH": "/usr/local/bin:/usr/bin:/bin",
                        "TMPDIR": str(output / "scratch"),
                        "XDG_CONFIG_HOME": str(output / "xdg-config"),
                        "XDG_CACHE_HOME": str(output / "xdg-cache")})
    return environment


def mount_archive(source: Path, mountpoint: Path, mounted_state: list[bool]) -> None:
    subprocess.run(["archivemount", "-o", "ro", str(source), str(mountpoint)], check=True)
    # Transfer mount ownership to the caller immediately after the successful
    # mount command, before any post-mount verification can fail.
    mounted_state[0] = True
    options = subprocess.run(["findmnt", "-T", str(mountpoint), "-no", "OPTIONS"], check=True,
                             capture_output=True, text=True).stdout.strip().split(",")
    if not {"ro", "nosuid", "nodev"} <= set(options):
        raise ValueError("original archive mount lacks required ro,nosuid,nodev options")


def unmount_archive(mountpoint: Path) -> None:
    subprocess.run(["fusermount", "-u", str(mountpoint)], check=True,
                   stdout=subprocess.DEVNULL, stderr=subprocess.PIPE)


def run_capture(args: argparse.Namespace) -> Path:
    if not args.experimental_observer:
        raise ValueError("schema 25 requires --experimental-observer")
    expected_hash, expected_size = require_pinned_recorder()
    if sys.platform != "linux":
        raise ValueError("this development observer is reviewed only for Linux/trv2")
    if not MIN_DURATION_SECONDS <= args.max_duration_seconds <= MAX_DURATION_SECONDS:
        raise ValueError("maximum duration must be 15–600 seconds")

    # Do not inspect or mount source media until the recorder identity is pinned.
    source = legacy.require_absolute_regular_file(Path(args.source_release), "source release")
    recorder = legacy.require_absolute_regular_file(Path(args.recorder), "recorder", executable=True)
    source_identity = legacy.validate_source_release(source)
    recorder_identity = legacy.validate_recorder(recorder, expected_hash)
    if recorder_identity[1] != expected_size:
        raise ValueError("recorder size differs from the reviewed schema-25 build")
    output = output_directory(source, Path(args.output))
    inherited = dict(os.environ)
    legacy.require_visible_operator_input(inherited)
    if inherited.get("SDL_VIDEODRIVER", "").lower() == "dummy":
        raise ValueError("headless SDL is forbidden; use the visible emulator window")

    output.mkdir(mode=0o700)
    for child in ("archive-ro", "scratch", "xdg-config", "xdg-cache"):
        (output / child).mkdir(mode=0o700)
    mountpoint = output / "archive-ro"
    mounted = [False]
    try:
        mount_archive(source, mountpoint, mounted)
        if not (mountpoint / legacy.GAME_ROOT).is_dir():
            raise ValueError("recognised archive lacks its expected game root")
        config_text = build_configuration(output, args.machine_profile)
        configuration = output / "dosbox-x-operand.conf"
        legacy.write_exclusive(configuration, config_text)
        configuration_bytes = config_text.encode("utf-8")
        command = [str(recorder), "-defaultconf", "-conf", str(configuration), "-fastlaunch",
                   "-c", "c:", "-c", "mill.com 0"]
        import json
        legacy.write_exclusive(output / "command-argv.json", json.dumps(command, indent=2) + "\n")
        print("EXPERIMENTAL OBSERVER: diagnostics only, never recovery evidence", flush=True)
        print("Use the visible window and ordinary manual input; close the window yourself.", flush=True)
        print("No AUTOTYPE, paste, scripted input, debugger input, or guest-memory injection.", flush=True)
        print("The deadline aborts the run; it does not close the window for you.", flush=True)
        environment = isolated_environment(output, inherited)
        status, reason, started, ended = terminal.supervise(
            command, output, environment, args.max_duration_seconds)
        if legacy.validate_source_release(source) != source_identity:
            raise ValueError("source archive identity changed during observation")
        if legacy.validate_recorder(recorder, expected_hash) != recorder_identity:
            raise ValueError("recorder identity changed during observation")
        if status != 0 or reason != "emulator-exit":
            raise ValueError(f"schema-25 observer rejected: {reason}, exit status {status}")

        payload = protocol.read_bounded(output / protocol.OBSERVATION_NAME)
        parsed = protocol.parse_observation(payload)
        fields = {
            "capture_receipt_version": protocol.SCHEMA_VERSION,
            "recorder_protocol": protocol.PROTOCOL,
            "recorder_admission": protocol.RECORDER_ADMISSION,
            "operand_observation": parsed["state"],
            "operand_observation_sha256": hashlib.sha256(payload).hexdigest(),
            "operand_observation_bytes": str(len(payload)),
        }
        # Preserve run timing and process status separately. The schema-25
        # receipt is intentionally minimal and never claims recorder admission.
        legacy.write_exclusive(output / "run-context.txt",
                               "run_protocol=v25-dos-title-operand\n"
                               f"source_release_sha256={source_identity[0]}\n"
                               f"source_release_bytes={source_identity[1]}\n"
                               f"recorder_sha256={recorder_identity[0]}\n"
                               f"recorder_bytes={recorder_identity[1]}\n"
                               f"observer_source_commit={OBSERVER_SOURCE_COMMIT}\n"
                               f"observer_patch_sha256={OBSERVER_PATCH_SHA256}\n"
                               f"machine_profile={args.machine_profile}\n"
                               "operator_procedure=visible-manual-input-and-window-close\n"
                               f"configuration_sha256={hashlib.sha256(configuration_bytes).hexdigest()}\n"
                               f"configuration_bytes={len(configuration_bytes)}\n"
                               f"max_duration_seconds={args.max_duration_seconds}\n"
                               "environment_policy=isolated-xdg-no-overrides-v1\n"
                               f"exit_status={status}\ntermination_reason={reason}\n"
                               f"start_unix={started:.6f}\nend_unix={ended:.6f}\n")
        protocol.verify_fields(fields, output, allow_experimental_observer=True)
        # Publish only after read-only media is successfully unmounted.
        unmount_archive(mountpoint)
        mounted[0] = False
        legacy.write_exclusive(output / "run-status.txt",
                               "".join(f"{key}={value}\n" for key, value in fields.items()))
        return output
    except BaseException as error:
        try:
            legacy.write_exclusive(output / "run-error.txt",
                                   f"REJECTED {type(error).__name__}: {str(error)[:2048]}\n")
        except OSError:
            pass
        raise
    finally:
        if mounted[0]:
            unmount_archive(mountpoint)


def parse_arguments(argv: list[str]) -> argparse.Namespace:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--source-release", required=True)
    parser.add_argument("--recorder", required=True)
    parser.add_argument("--output", required=True)
    parser.add_argument("--experimental-observer", action="store_true")
    parser.add_argument("--max-duration-seconds", type=int, default=120)
    parser.add_argument("--machine-profile", choices=sorted(legacy.MACHINE_PROFILES), default="svga_s3")
    return parser.parse_args(argv)


def main(argv: list[str] | None = None) -> int:
    try:
        output = run_capture(parse_arguments(sys.argv[1:] if argv is None else argv))
    except (OSError, ValueError, RuntimeError, subprocess.SubprocessError) as error:
        print(f"EXPERIMENTAL CAPTURE REJECTED  {error}", file=sys.stderr)
        return 2
    print(f"EXPERIMENTAL CAPTURE RETAINED  {output}  not for recovery")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
