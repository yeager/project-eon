#!/usr/bin/env python3
"""Run the reviewed experimental terminal-only DOS observer with manual input.

This Linux/trv2 development protocol is never recovery-admissible. It sends
no input or graceful-stop event: the operator must close the visible window.
Timeout and console-limit kills are rejected aborts, even if files remain.
"""
from __future__ import annotations

import argparse
import hashlib
import importlib.util
import os
from pathlib import Path
import re
import signal
import stat
import subprocess
import sys
import threading
import time


def load_tool(name: str):
    spec = importlib.util.spec_from_file_location(name, Path(__file__).with_name(name + ".py"))
    if spec is None or spec.loader is None:
        raise RuntimeError(f"cannot load {name}")
    module = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(module)
    return module


legacy = load_tool("run_millennium_dos_capture")
protocol = load_tool("millennium_dos_terminal_protocol")


def output_directory(source: Path, output: Path) -> Path:
    if (not output.is_absolute() or output.parent != protocol.OUTPUT_ROOT
            or not re.fullmatch(r"[A-Za-z0-9][A-Za-z0-9._-]*", output.name)):
        raise ValueError("output must be a fresh direct child of the reviewed terminal-captures cache")
    for parent in (output.parent, *output.parent.parents):
        info = parent.lstat()
        if not stat.S_ISDIR(info.st_mode) or stat.S_ISLNK(info.st_mode):
            raise ValueError("output ancestors must be real directories")
    info = output.parent.stat()
    if info.st_uid != os.geteuid() or info.st_mode & 0o022:
        raise ValueError("output parent must be privately controlled by the current user")
    return legacy.reject_unsafe_output(source, output)


def isolated_environment(output: Path, inherited: dict[str, str]) -> dict[str, str]:
    legacy.require_visible_operator_input(inherited)
    # Preserve display/authentication and the user's existing HOME value, but
    # do not inherit DOSBox options, preload libraries, or recorder overrides.
    keep = {"HOME", "USER", "LOGNAME", "LANG", "LC_ALL", "DISPLAY", "WAYLAND_DISPLAY",
            "XAUTHORITY", "XDG_RUNTIME_DIR", "DBUS_SESSION_BUS_ADDRESS"}
    environment = {key: value for key, value in inherited.items() if key in keep}
    environment.update({"PATH": "/usr/local/bin:/usr/bin:/bin",
                        "TMPDIR": str(output / "scratch"),
                        "XDG_CONFIG_HOME": str(output / "xdg-config"),
                        "XDG_CACHE_HOME": str(output / "xdg-cache")})
    return environment


def drain_console(stream, path: Path, abort: threading.Event) -> tuple[int, bool]:
    """Retain the complete successful console, bounded by the protocol cap."""
    total = 0
    retained = 0
    with path.open("xb") as destination:
        while True:
            block = stream.read(64 * 1024)
            if not block:
                break
            total += len(block)
            keep = block[:max(0, protocol.MAX_CONSOLE_BYTES - retained)]
            if keep:
                destination.write(keep)
                retained += len(keep)
            if total > protocol.MAX_CONSOLE_BYTES:
                abort.set()
        destination.flush()
        os.fsync(destination.fileno())
    return total, total > protocol.MAX_CONSOLE_BYTES


def abort_process(process) -> None:
    """Kill this isolated process group only on a rejected/failed run."""
    try:
        os.killpg(process.pid, signal.SIGKILL)
    except ProcessLookupError:
        pass
    process.wait(timeout=5)


def supervise(command: list[str], output: Path, environment: dict[str, str],
              duration: int) -> tuple[int, str, float, float]:
    started = time.time()
    process = subprocess.Popen(command, cwd=output, env=environment,
                               stdout=subprocess.PIPE, stderr=subprocess.STDOUT,
                               start_new_session=True)
    abort = threading.Event()
    errors: list[BaseException] = []
    results: list[tuple[int, bool]] = []
    assert process.stdout is not None

    def reader() -> None:
        try:
            results.append(drain_console(process.stdout, output / "recorder-console.log", abort))
        except BaseException as error:
            errors.append(error)
            abort.set()

    thread = threading.Thread(target=reader, name="eon-terminal-console", daemon=True)
    reason = "emulator-exit"
    completed = False
    aborted = False

    def terminate() -> None:
        nonlocal aborted
        if not aborted:
            abort_process(process)
            aborted = True

    try:
        thread.start()
        deadline = time.monotonic() + duration
        while process.poll() is None:
            if abort.is_set():
                reason = "console-abort"
                terminate()
                break
            if time.monotonic() >= deadline:
                reason = "timeout"
                terminate()
                break
            abort.wait(0.05)
        thread.join(timeout=5)
        if thread.is_alive():
            # A descendant retaining stdout is not a completed capture.
            terminate()
            thread.join(timeout=5)
            raise ValueError("console drain did not finish with the recorder")
        if errors:
            raise ValueError("console retention failed") from errors[0]
        if len(results) != 1 or results[0][1]:
            raise ValueError("console exceeded its bounded capture contract")
        completed = True
        return process.returncode, reason, started, time.time()
    finally:
        if not completed or process.poll() is None:
            terminate()
        if thread.is_alive():
            thread.join(timeout=5)
        if not thread.is_alive():
            process.stdout.close()


def operator_instructions(intent: str) -> tuple[str, ...]:
    if intent == "operator-input":
        action = "Use ordinary keys yourself in the visible window; then close that window manually."
    elif intent == "diagnostic-no-key-delivery":
        action = "Do not press keys; observe the visible window, then close it manually with its close button."
    else:
        raise ValueError("unsupported terminal capture intent")
    return ("EXPERIMENTAL OBSERVER  external diagnostics only, never recovery evidence", action,
            "No AUTOTYPE, clipboard paste, debugger input, scripted input, or guest-memory injection.",
            "SDL key receipts have unclassified origin; their presence alone does not prove physical input.",
            "The time limit is an abort deadline. Close the window before it expires; the runner never closes it for you.")


def unmount_checked(mountpoint: Path) -> None:
    subprocess.run(["fusermount", "-u", str(mountpoint)], check=True,
                   stdout=subprocess.DEVNULL, stderr=subprocess.PIPE)


def mount_archive(source: Path, mountpoint: Path) -> None:
    subprocess.run(["archivemount", "-o", "ro", str(source), str(mountpoint)], check=True)


def run_capture(args: argparse.Namespace) -> Path:
    if not args.experimental_observer:
        raise ValueError("this development observer requires --experimental-observer")
    if sys.platform != "linux":
        raise ValueError("this recorder build is reviewed only for Linux/trv2")
    if not 15 <= args.max_duration_seconds <= 600:
        raise ValueError("maximum duration must be 15–600 seconds")
    instructions = operator_instructions(args.capture_intent)
    source = legacy.require_absolute_regular_file(Path(args.source_release), "source release")
    recorder = legacy.require_absolute_regular_file(Path(args.recorder), "recorder", executable=True)
    source_identity = legacy.validate_source_release(source)
    recorder_identity = legacy.validate_recorder(recorder, protocol.RECORDER_SHA256)
    if recorder_identity[1] != protocol.RECORDER_SIZE:
        raise ValueError("recorder size differs from the reviewed build")
    output = output_directory(source, Path(args.output))
    environment = isolated_environment(output, dict(os.environ))
    configuration_text = protocol.build_configuration(output, args.machine_profile)
    output.mkdir(mode=0o700)
    for child in ("archive-ro", "scratch", "xdg-config", "xdg-cache"):
        (output / child).mkdir(mode=0o700)
    mountpoint = output / "archive-ro"
    mounted = False
    try:
        mount_archive(source, mountpoint)
        mounted = True
        if not {"ro", "nosuid", "nodev"} <= set(legacy.mount_options(mountpoint).split(",")):
            raise ValueError("archive mount lacks required ro,nosuid,nodev options")
        if not (mountpoint / legacy.GAME_ROOT).is_dir():
            raise ValueError("recognised archive lacks its expected game root")
        configuration = output / protocol.CONFIGURATION_NAME
        legacy.write_exclusive(configuration, configuration_text)
        command = [str(recorder), "-defaultconf", "-conf", str(configuration), "-fastlaunch",
                   "-c", "c:", "-c", "mill.com 0"]
        # Reviewable argv; this file is never evaluated as shell code.
        import json
        legacy.write_exclusive(output / "command-argv.json", json.dumps(command, indent=2) + "\n")
        for line in instructions:
            print(line, flush=True)
        status, reason, started, ended = supervise(command, output, environment, args.max_duration_seconds)
        if legacy.validate_source_release(source) != source_identity:
            raise ValueError("source archive identity changed during observation")
        if legacy.validate_recorder(recorder, protocol.RECORDER_SHA256) != recorder_identity:
            raise ValueError("recorder identity changed during observation")
        if status != 0 or reason != "emulator-exit":
            raise ValueError(f"terminal observer rejected: {reason}, exit status {status}")
        fields = {
            "capture_receipt_version": protocol.SCHEMA_VERSION,
            "recorder_protocol": protocol.PROTOCOL,
            "recorder_admission": "experimental-observer-not-for-recovery",
            "capture_directory": str(output), "machine_profile": args.machine_profile,
            "capture_intent": args.capture_intent, "input_origin": "unclassified-sdl-queue",
            "input_timestamp": "sdl2-key.timestamp-u32",
            "input_scope": "guest-lifetime-including-internal-reboots",
            "environment_policy": "isolated-xdg-no-overrides-v1",
            "observer_source_commit": "234797680781567e18c374c9e62da24de5423db0",
            "observer_patch_sha256": "23951d5c7cab7d18206f7f15eac352bc2901ab8bbd56b9693e621796aeb9efe5",
            "operator_procedure": "visible-manual-window-close",
            "exit_status": str(status), "termination_reason": reason,
            "start_unix": f"{started:.6f}", "end_unix": f"{ended:.6f}",
            "max_duration_seconds": str(args.max_duration_seconds),
        }
        for prefix, identity in (("source_release", source_identity), ("recorder", recorder_identity)):
            fields[prefix + "_sha256"], size = identity
            fields[prefix + "_bytes"] = str(size)
        artifacts = (("configuration", protocol.CONFIGURATION_NAME, 64*1024),
                     ("int6_observation", "int6-observation.raw", 2048),
                     ("host_input_receipt", "host-input-receipt.raw", 64*1024),
                     ("recorder_console", "recorder-console.log", protocol.MAX_CONSOLE_BYTES))
        for prefix, filename, limit in artifacts:
            data = protocol.read_regular(output / filename, limit)
            fields[prefix + "_sha256"] = hashlib.sha256(data).hexdigest()
            fields[prefix + "_bytes"] = str(len(data))
        input_path = output / "host-input-receipt.raw"
        fields["host_input_records"] = str(protocol.parse_host_input(
            protocol.read_regular(input_path, 64 * 1024)))
        protocol.verify_fields(fields, output, allow_experimental_observer=True)
        # Publish a successful receipt only after the original archive has
        # also been unmounted successfully.
        unmount_checked(mountpoint)
        mounted = False
        legacy.write_exclusive(output / "run-status.txt",
                               "".join(f"{key}={value}\n" for key, value in fields.items()))
        return output
    except BaseException as error:
        legacy.write_exclusive(output / "run-error.txt", f"REJECTED {type(error).__name__}: {str(error)[:2048]}\n")
        raise
    finally:
        if mounted:
            unmount_checked(mountpoint)


def parse_arguments(argv: list[str]) -> argparse.Namespace:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--source-release", required=True)
    parser.add_argument("--recorder", required=True)
    parser.add_argument("--output", required=True)
    parser.add_argument("--experimental-observer", action="store_true")
    parser.add_argument("--max-duration-seconds", type=int, default=120)
    parser.add_argument("--machine-profile", choices=sorted(legacy.MACHINE_PROFILES), default="svga_s3")
    parser.add_argument("--capture-intent", choices=("operator-input", "diagnostic-no-key-delivery"), required=True)
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
