#!/usr/bin/env python3
"""Run the unpinned DOSBox-X origin observer as an explicit experiment only.

No output from this tool is recovery-admissible. It requires the exact
experimental executable, an exact recognised release archive, and a visible
operator window. The operator closes DOSBox-X manually to flush the observer.
"""

from __future__ import annotations

import argparse
import hashlib
import importlib.util
import os
from pathlib import Path
import signal
import stat
import subprocess
import sys
import threading
import time
import uuid


ROOT = Path(__file__).resolve().parents[1]
TRV2_OBSERVER_ROOT = Path(
    "/home/trv2/.cache/project-eon-tools/dosbox-origin-observer-20261005-01")
TRV2_RECEIPTS = TRV2_OBSERVER_ROOT / "receipts"
TRV2_RUNS = Path("/home/trv2/.cache/project-eon-tools/dos-origin-experiment-runs")
GAME_ROOT = "millennium-return-to-earth-2-2"
MAX_DURATION = 600


def load_module(name: str, path: Path):
    spec = importlib.util.spec_from_file_location(name, path)
    if not spec or not spec.loader:
        raise RuntimeError(f"unable to load {path.name}")
    module = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(module)
    return module


CONTRACT = load_module("millennium_dos_origin_experiment", ROOT / "tools" / "millennium_dos_origin_experiment.py")
CAPTURE = load_module("run_millennium_dos_capture", ROOT / "tools" / "run_millennium_dos_capture.py")


class CaptureError(RuntimeError):
    """Preflight or experimental-run failure."""


def kill_recorder_group(process: subprocess.Popen[bytes]) -> None:
    """Stop the recorder and any UI helpers that inherited its output pipe."""
    try:
        os.killpg(process.pid, signal.SIGKILL)
    except ProcessLookupError:
        # The whole isolated process group has already exited.
        pass


def ensure_runs_root() -> None:
    parent = TRV2_RUNS.parent
    if not parent.is_dir() or parent.is_symlink() or parent.stat().st_uid != os.geteuid():
        raise CaptureError("external experiment cache parent is absent or not user-owned")
    if not TRV2_RUNS.exists() and not TRV2_RUNS.is_symlink():
        TRV2_RUNS.mkdir(mode=0o700)
    if not TRV2_RUNS.is_dir() or TRV2_RUNS.is_symlink():
        raise CaptureError("external experiment-runs cache is not a real directory")
    info = TRV2_RUNS.stat()
    if info.st_uid != os.geteuid() or info.st_mode & 0o077:
        raise CaptureError("experiment-runs cache must be owned by this user and mode 0700")


def require_experiment_output(output: Path, source: Path) -> Path:
    if not output.is_absolute() or output.exists() or output.is_symlink():
        raise CaptureError("output must be a new absolute directory")
    resolved = output.resolve(strict=False)
    if resolved == ROOT or ROOT in resolved.parents:
        raise CaptureError("output must stay outside the repository")
    if resolved == Path("/tmp") or Path("/tmp") in resolved.parents:
        raise CaptureError("output must not use /tmp")
    if source.parent == resolved or source.parent in resolved.parents:
        raise CaptureError("output must stay outside supplied original media")
    if resolved.parent != TRV2_RUNS or not resolved.name.startswith("origin-"):
        raise CaptureError("output must be a named direct child of the external experiment-runs cache")
    ensure_runs_root()
    return resolved


def isolated_environment(output: Path, inherited: dict[str, str]) -> dict[str, str]:
    CAPTURE.require_visible_operator_input(inherited)
    keep = {"HOME", "USER", "LOGNAME", "LANG", "LC_ALL", "DISPLAY", "WAYLAND_DISPLAY",
            "XAUTHORITY", "XDG_RUNTIME_DIR", "DBUS_SESSION_BUS_ADDRESS"}
    environment = {key: value for key, value in inherited.items() if key in keep}
    environment.update({
        "PATH": "/usr/local/bin:/usr/bin:/bin",
        "TMPDIR": str(output / "tmp"),
        "XDG_CONFIG_HOME": str(output / "xdg-config"),
        "XDG_CACHE_HOME": str(output / "xdg-cache"),
    })
    return environment


def mount_read_only(source: Path, mountpoint: Path, environment: dict[str, str]) -> None:
    subprocess.run(["archivemount", "-o", "ro", str(source), str(mountpoint)],
                   check=True, env=environment)
    try:
        options = subprocess.run(["findmnt", "-T", str(mountpoint), "-no", "OPTIONS"],
                                 check=True, capture_output=True, text=True,
                                 env=environment).stdout.strip().split(",")
    except subprocess.SubprocessError:
        subprocess.run(["fusermount", "-u", str(mountpoint)], check=False,
                       stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL,
                       env=environment)
        raise
    if not {"ro", "nosuid", "nodev"} <= set(options):
        subprocess.run(["fusermount", "-u", str(mountpoint)], check=False,
                       stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL,
                       env=environment)
        raise CaptureError("original archive mount lacks ro,nosuid,nodev options")


def unmount(mountpoint: Path, environment: dict[str, str]) -> None:
    subprocess.run(["fusermount", "-u", str(mountpoint)], check=True,
                   stdout=subprocess.DEVNULL, stderr=subprocess.PIPE,
                   env=environment)


def origin_recorder_configuration(game_root: Path) -> str:
    """Keep manual window close from opening DOSBox-X's external zenity prompt."""
    configuration = CAPTURE.recorder_config(game_root)
    anchor = "memsize=16\n"
    if configuration.count(anchor) != 1:
        raise CaptureError("reviewed DOSBox-X configuration anchor changed")
    configuration = configuration.replace(anchor, anchor + "quit warning=false\n", 1)
    # Bound the known broken candidate's repeated INT 6 logging without
    # changing guest state, device output, or normal-core execution.
    return configuration + "[log]\nlogfile=/dev/null\ncpu=never\n"


def copy_and_remove_candidate_receipt(candidate: Path, output: Path) -> tuple[bytes | None, str]:
    """Retain then remove only this run's exact regular UUID-named output."""
    if candidate.parent != TRV2_RECEIPTS or candidate.name != f"origin-{candidate.stem[7:]}.receipt" \
            or len(candidate.stem) != 39 or not candidate.stem.startswith("origin-") \
            or any(ch not in "0123456789abcdef" for ch in candidate.stem[7:]):
        raise CaptureError("candidate receipt path escaped its exact UUID namespace")
    try:
        info = candidate.lstat()
    except FileNotFoundError:
        return None, "absent"
    if stat.S_ISLNK(info.st_mode) or not stat.S_ISREG(info.st_mode) or info.st_size > 4096:
        return None, "unsafe-or-over-limit"
    fd = os.open(candidate, os.O_RDONLY | getattr(os, "O_NOFOLLOW", 0) | getattr(os, "O_NONBLOCK", 0))
    with os.fdopen(fd, "rb") as stream:
        if not stat.S_ISREG(os.fstat(stream.fileno()).st_mode):
            return None, "unsafe-or-over-limit"
        data = stream.read(4097)
    if len(data) > 4096:
        return None, "unsafe-or-over-limit"
    copied = output / "origin.raw"
    with copied.open("xb") as stream:
        stream.write(data)
        stream.flush()
        os.fsync(stream.fileno())
    # This name was generated for this run and is outside all supplied media.
    candidate.unlink()
    return data, "copied-and-removed"


def run(args: argparse.Namespace) -> Path:
    if not args.experimental_only:
        raise CaptureError("--experimental-only is required; this recorder is not admitted evidence")
    if sys.platform != "linux":
        raise CaptureError("the candidate is pinned to its trv2 Linux cache and is not portable")
    if not 15 <= args.duration_seconds <= MAX_DURATION:
        raise CaptureError(f"duration must be between 15 and {MAX_DURATION} seconds")
    source = CAPTURE.require_absolute_regular_file(Path(args.source_release), "source release")
    recorder = CAPTURE.require_absolute_regular_file(Path(args.recorder), "recorder", executable=True)
    source_identity = CAPTURE.validate_source_release(source)
    recorder_identity = CONTRACT.sha256_file(recorder)
    if recorder_identity != (CONTRACT.RECORDER_SHA256, CONTRACT.RECORDER_BYTES):
        raise CaptureError("recorder does not match the exact experimental binary identity")
    output = require_experiment_output(Path(args.output), source)
    CAPTURE.require_visible_operator_input(dict(os.environ))
    if not TRV2_RECEIPTS.is_dir() or TRV2_RECEIPTS.is_symlink():
        raise CaptureError("candidate's fixed external receipts directory is absent or unsafe")
    receipt_root = TRV2_RECEIPTS.resolve(strict=True)
    if receipt_root != TRV2_RECEIPTS or not receipt_root.is_dir():
        raise CaptureError("candidate's fixed external receipt path is not canonical")

    run_id = uuid.uuid4().hex
    candidate_receipt = receipt_root / f"origin-{run_id}.receipt"
    output.mkdir(mode=0o700)
    scratch = output / "tmp"
    scratch.mkdir(mode=0o700)
    (output / "xdg-config").mkdir(mode=0o700)
    (output / "xdg-cache").mkdir(mode=0o700)
    mountpoint = output / "archive-ro"
    mountpoint.mkdir(mode=0o700)
    environment = isolated_environment(output, dict(os.environ))
    mounted = False
    try:
        mount_read_only(source, mountpoint, environment)
        mounted = True
        game_root = mountpoint / GAME_ROOT
        if not game_root.is_dir():
            raise CaptureError("recognised archive does not expose the expected DOS game root")
        config_path = output / "recorder.conf"
        configuration = origin_recorder_configuration(game_root)
        CAPTURE.write_exclusive(config_path, configuration)
        config_identity = CONTRACT.sha256_file(config_path)
        command = [str(recorder), "-conf", str(config_path), "-fastlaunch", "-c", "c:", "-c", "mill.com 0"]
        CAPTURE.write_exclusive(output / "command-tail.txt", " ".join(command) + "\n")

        environment.update({
            "PROJECT_EON_DOS_ORIGIN_EXPERIMENT": "1",
            "PROJECT_EON_DOS_ORIGIN_OUTPUT": str(candidate_receipt),
        })
        console_path = output / "recorder-console.log"
        console_over_limit = threading.Event()
        console_result = []
        console_errors = []
        print("EXPERIMENTAL OBSERVER  output is never recovery-admissible evidence.")
        print("Use ordinary manual input in the visible window; close it manually to flush the record.")
        print("No AUTOTYPE, paste, scripted input, debugger input, or guest-memory injection.")
        print(f"CAPTURE WINDOW ACTIVE  up to {args.duration_seconds} seconds; close the window before timeout.")
        process = subprocess.Popen(command, env=environment, stdout=subprocess.PIPE,
                                   stderr=subprocess.STDOUT, start_new_session=True)
        assert process.stdout is not None

        def drain_console() -> None:
            try:
                console_result.append(CAPTURE.capture_bounded_console(
                    process.stdout, console_path, console_over_limit))
            except BaseException as error:
                console_errors.append(error)

        reader = threading.Thread(target=drain_console, name="eon-origin-console", daemon=True)
        reader.start()
        deadline = time.monotonic() + args.duration_seconds
        timed_out = False
        while process.poll() is None:
            if console_over_limit.is_set():
                kill_recorder_group(process)
                break
            if time.monotonic() >= deadline:
                timed_out = True
                kill_recorder_group(process)
                break
            console_over_limit.wait(0.1)
        exit_status = process.wait()
        reader.join()
        if console_errors or len(console_result) != 1:
            raise CaptureError("could not retain the bounded recorder console")
        source_after = CAPTURE.validate_source_release(source)
        if source_after != source_identity:
            raise CaptureError("original archive identity changed during the experiment")

        console_data, _ = CONTRACT.read_bounded_file(console_path, 1024 * 1024)
        marker_present = CONTRACT.RECEIPT_MARKER.encode("ascii") + b"\n" in console_data.splitlines(keepends=True)
        raw_ok = False
        raw_hash = "absent"
        raw_size = "0"
        raw_data, candidate_state = copy_and_remove_candidate_receipt(candidate_receipt, output)
        if raw_data is not None:
            raw_hash = hashlib.sha256(raw_data).hexdigest()
            raw_size = str(len(raw_data))
            try:
                CONTRACT.parse_raw(raw_data)
                raw_ok = True
            except CONTRACT.ExperimentError:
                raw_ok = False
        complete = exit_status == 0 and marker_present and raw_ok and not timed_out
        if complete:
            status = (
                f"schema={CONTRACT.SCHEMA}\n"
                "admission=experimental-only-not-recovery-admissible\n"
                f"recorder_sha256={recorder_identity[0]}\nrecorder_bytes={recorder_identity[1]}\n"
                f"patch_sha256={CONTRACT.PATCH_SHA256}\n"
                f"source_release_sha256={source_identity[0]}\nsource_release_bytes={source_identity[1]}\n"
                f"configuration_sha256={config_identity[0]}\n"
                f"configuration_bytes={config_identity[1]}\n"
                "raw_receipt=origin.raw\n"
                f"raw_sha256={raw_hash}\nraw_bytes={raw_size}\n"
                f"console_sha256={console_result[0].retained_sha256}\n"
                f"console_bytes={console_result[0].retained_bytes}\n"
                f"exit_status={exit_status}\nsuccess_marker=present\nrecord_status=complete\n")
            CAPTURE.write_exclusive(output / "experiment-status.txt", status)
        else:
            reason = ("console-safety-cap" if console_result[0].over_limit else
                      "timeout" if timed_out else "missing-success-marker-or-complete-record")
            CAPTURE.write_exclusive(output / "experiment-failure.txt",
                f"schema={CONTRACT.SCHEMA}\nadmission=experimental-only-not-recovery-admissible\n"
                f"exit_status={exit_status}\nsuccess_marker={'present' if marker_present else 'absent'}\n"
                f"candidate_receipt={'present' if candidate_receipt.exists() else 'absent'}\n"
                f"candidate_receipt_disposition={candidate_state}\n"
                f"failure_reason={reason}\n")
            raise CaptureError(f"experiment rejected: {reason}; retained files at {output}")
        print(f"EXPERIMENT COMPLETE  external diagnostics only: {output}")
        print("Observed values describe the last normal-core fetch before CS=0e70; they do not prove a causal transfer.")
        return output
    finally:
        if mounted:
            unmount(mountpoint, environment)
        try:
            if candidate_receipt.exists() or candidate_receipt.is_symlink():
                copy_and_remove_candidate_receipt(candidate_receipt, output)
        except (CaptureError, FileExistsError, OSError):
            pass


def parse_arguments(argv: list[str] | None = None) -> argparse.Namespace:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--source-release", required=True, help="Absolute exact recognised English DOS archive")
    parser.add_argument("--recorder", required=True, help="Absolute experimental DOSBox-X executable")
    parser.add_argument("--output", required=True, help="New directory under the candidate's external cache")
    parser.add_argument("--duration-seconds", type=int, default=180)
    parser.add_argument("--experimental-only", action="store_true",
                        help="Required explicit gate; this cannot produce recovery-admissible evidence")
    return parser.parse_args(argv)


def main(argv: list[str] | None = None) -> int:
    try:
        run(parse_arguments(sys.argv[1:] if argv is None else argv))
    except (CaptureError, CAPTURE.CaptureError, CONTRACT.ExperimentError,
            OSError, subprocess.SubprocessError) as error:
        print(f"DOS origin experiment rejected: {error}", file=sys.stderr)
        return 2
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
