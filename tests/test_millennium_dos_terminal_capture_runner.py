"""Mocked safety tests for the experimental Millennium terminal runner."""

from __future__ import annotations

import contextlib
import hashlib
import importlib.util
import io
import os
from pathlib import Path
import signal
import stat
import sys
from types import SimpleNamespace
import unittest
from unittest import mock

from eon_test_paths import temporary_directory

KILL_SIGNAL = getattr(signal, "SIGKILL", signal.SIGTERM)


ROOT = Path(__file__).resolve().parents[1]
SPEC = importlib.util.spec_from_file_location(
    "run_millennium_dos_terminal_capture", ROOT / "tools" / "run_millennium_dos_terminal_capture.py")
assert SPEC and SPEC.loader
TOOL = importlib.util.module_from_spec(SPEC)
SPEC.loader.exec_module(TOOL)


class ChunkStream:
    def __init__(self, *chunks: bytes, error: BaseException | None = None):
        self.chunks = list(chunks)
        self.error = error
        self.read_count = 0
        self.closed = False

    def read(self, _size: int) -> bytes:
        self.read_count += 1
        if self.error:
            raise self.error
        return self.chunks.pop(0) if self.chunks else b""

    def close(self) -> None:
        self.closed = True


class InlineThread:
    """Run the reader deterministically while preserving the Thread contract."""
    def __init__(self, *, target, name=None, daemon=None):
        self.target = target
        self.name = name
        self.daemon = daemon
        self.alive = False

    def start(self):
        self.alive = True
        try:
            self.target()
        finally:
            self.alive = False

    def join(self, timeout=None):
        del timeout

    def is_alive(self):
        return self.alive


class FakeProcess:
    def __init__(self, stdout, *, natural_exit=True):
        self.stdout = stdout
        self.pid = 43210
        self.returncode = None
        self.natural_exit = natural_exit
        self.wait_calls = []

    def poll(self):
        if self.returncode is not None:
            return self.returncode
        if self.natural_exit:
            self.returncode = 0
            return 0
        return None

    def wait(self, timeout=None):
        self.wait_calls.append(timeout)
        if self.returncode is None:
            self.returncode = -KILL_SIGNAL
        return self.returncode


class MillenniumDosTerminalCaptureRunnerTests(unittest.TestCase):
    def test_isolated_environment_keeps_display_but_drops_recorder_overrides(self) -> None:
        with temporary_directory() as directory:
            output = Path(directory) / "capture"
            inherited = {"HOME": "/home/operator", "DISPLAY": ":1", "XAUTHORITY": "/auth",
                         "DOSBOX": "evil", "DOSBOX_CONFIG": "/tmp/untrusted.conf",
                         "LD_PRELOAD": "/tmp/untrusted.so", "SDL_VIDEODRIVER": "x11"}
            with mock.patch.object(TOOL.legacy, "require_visible_operator_input") as visible:
                env = TOOL.isolated_environment(output, inherited)
            visible.assert_called_once_with(inherited)
            self.assertEqual(env["DISPLAY"], ":1")
            self.assertEqual(env["HOME"], "/home/operator")
            self.assertEqual(env["PATH"], "/usr/local/bin:/usr/bin:/bin")
            self.assertEqual(env["TMPDIR"], str(output / "scratch"))
            for key in ("DOSBOX", "DOSBOX_CONFIG", "LD_PRELOAD", "SDL_VIDEODRIVER"):
                self.assertNotIn(key, env)

    def test_visible_input_preflight_rejects_headless_or_dummy_sdl(self) -> None:
        with self.assertRaisesRegex(TOOL.legacy.CaptureError, "visible X11 or Wayland"):
            TOOL.isolated_environment(Path("/external/out"), {"HOME": "/home/test"})
        with self.assertRaisesRegex(TOOL.legacy.CaptureError, "headless SDL"):
            TOOL.isolated_environment(Path("/external/out"), {"DISPLAY": ":1", "SDL_VIDEODRIVER": "dummy"})

    def test_output_preflight_rejects_noncanonical_names_and_roots(self) -> None:
        with temporary_directory() as directory:
            root = Path(directory).resolve()
            with mock.patch.object(TOOL.protocol, "OUTPUT_ROOT", root):
                for output in (root / "../escape", root / "bad name", root / "", Path("relative")):
                    with self.subTest(output=output), self.assertRaises(ValueError):
                        TOOL.output_directory(root / "synthetic.zip", output)

    @unittest.skipUnless(hasattr(os, "geteuid"), "owner/mode gate is POSIX-specific")
    def test_output_preflight_requires_private_owned_real_ancestors(self) -> None:
        with temporary_directory() as directory:
            root = Path(directory).resolve()
            output = root / "capture-1"
            output_root_mode = stat.S_IMODE(root.stat().st_mode)
            try:
                root.chmod(0o700)
                source = root.parent / "synthetic-source.zip"
                with mock.patch.object(TOOL.protocol, "OUTPUT_ROOT", root), \
                     mock.patch.object(TOOL.legacy, "reject_unsafe_output", side_effect=lambda _s, p: p):
                    self.assertEqual(TOOL.output_directory(source, output), output)
                    root.chmod(0o722)
                    with self.assertRaisesRegex(ValueError, "privately controlled"):
                        TOOL.output_directory(source, root / "capture-2")
            finally:
                root.chmod(output_root_mode)

    def test_drain_console_retains_complete_stream_and_rejects_overflow(self) -> None:
        with temporary_directory() as directory:
            output = Path(directory) / "console.log"
            abort = TOOL.threading.Event()
            with mock.patch.object(TOOL.protocol, "MAX_CONSOLE_BYTES", 9):
                total, overflow = TOOL.drain_console(ChunkStream(b"1234", b"56789"), output, abort)
            self.assertEqual(total, 9)
            self.assertFalse(overflow)
            self.assertFalse(abort.is_set())
            self.assertEqual(output.read_bytes(), b"123456789")
            output.unlink()
            abort.clear()
            with mock.patch.object(TOOL.protocol, "MAX_CONSOLE_BYTES", 9):
                total, overflow = TOOL.drain_console(ChunkStream(b"123456", b"7890"), output, abort)
            self.assertEqual(total, 10)
            self.assertTrue(overflow)
            self.assertTrue(abort.is_set())
            self.assertEqual(output.read_bytes(), b"123456789")

    def _supervise(self, directory: Path, process: FakeProcess, *, duration=30,
                   monotonic=None, console_limit=1024):
        calls = []

        def killpg(pid, sig):
            calls.append((pid, sig))
            process.returncode = -sig

        with mock.patch.object(TOOL.subprocess, "Popen", return_value=process) as popen, \
             mock.patch.object(TOOL.threading, "Thread", InlineThread), \
             mock.patch.object(TOOL.os, "killpg", side_effect=killpg, create=True), \
             mock.patch.object(TOOL.signal, "SIGKILL", KILL_SIGNAL, create=True), \
             mock.patch.object(TOOL.protocol, "MAX_CONSOLE_BYTES", console_limit), \
             mock.patch.object(TOOL.time, "monotonic", side_effect=monotonic) if monotonic else contextlib.nullcontext():
            result = TOOL.supervise(["/reviewed/recorder"], directory,
                                    {"PATH": "/usr/bin"}, duration)
        return result, calls, popen

    def test_supervise_accepts_only_clean_manual_emulator_exit(self) -> None:
        with temporary_directory() as directory:
            process = FakeProcess(ChunkStream(b"visible run log\n"))
            (status, reason, started, ended), killed, popen = self._supervise(Path(directory), process)
            self.assertEqual((status, reason), (0, "emulator-exit"))
            self.assertLessEqual(started, ended)
            self.assertEqual(killed, [])
            self.assertEqual(popen.call_args.kwargs["start_new_session"], True)
            self.assertEqual(popen.call_args.kwargs["cwd"], Path(directory))
            self.assertEqual((Path(directory) / "recorder-console.log").read_bytes(), b"visible run log\n")

    def test_supervise_kills_timeout_and_console_overflow_process_groups(self) -> None:
        with temporary_directory() as directory:
            process = FakeProcess(ChunkStream(), natural_exit=False)
            result, killed, _ = self._supervise(Path(directory), process, duration=1,
                                                 monotonic=[10.0, 12.0])
            self.assertEqual((result[0], result[1]), (-KILL_SIGNAL, "timeout"))
            self.assertEqual(killed, [(process.pid, KILL_SIGNAL)])
            self.assertEqual(process.wait_calls, [5])

        with temporary_directory() as directory:
            process = FakeProcess(ChunkStream(b"123456"), natural_exit=False)
            with self.assertRaisesRegex(ValueError, "bounded capture contract"):
                self._supervise(Path(directory), process, console_limit=5)
            self.assertEqual(process.returncode, -KILL_SIGNAL)
            self.assertEqual((Path(directory) / "recorder-console.log").read_bytes(), b"12345")

    def test_supervise_reader_errors_kill_process_and_retain_no_receipt(self) -> None:
        with temporary_directory() as directory:
            process = FakeProcess(ChunkStream(error=OSError("read failed")), natural_exit=False)
            with mock.patch.object(TOOL.subprocess, "Popen", return_value=process), \
                 mock.patch.object(TOOL.threading, "Thread", InlineThread), \
                 mock.patch.object(TOOL.os, "killpg", side_effect=lambda _pid, sig: setattr(process, "returncode", -sig), create=True) as killpg, \
                 mock.patch.object(TOOL.signal, "SIGKILL", KILL_SIGNAL, create=True):
                with self.assertRaisesRegex(ValueError, "console retention failed"):
                    TOOL.supervise(["/reviewed/recorder"], Path(directory), {"PATH": "/usr/bin"}, 30)
            killpg.assert_called_once_with(process.pid, KILL_SIGNAL)
            self.assertEqual(process.wait_calls, [5])
            self.assertFalse((Path(directory) / "run-status.txt").exists())

    def test_supervise_thread_start_failure_kills_and_reaps_recorder(self) -> None:
        class StartFailureThread(InlineThread):
            def start(self):
                raise RuntimeError("thread start failed")

        with temporary_directory() as directory:
            process = FakeProcess(ChunkStream(), natural_exit=False)
            killed = []

            def killpg(pid, sig):
                killed.append((pid, sig))
                process.returncode = -sig

            with mock.patch.object(TOOL.subprocess, "Popen", return_value=process), \
                 mock.patch.object(TOOL.threading, "Thread", StartFailureThread), \
                 mock.patch.object(TOOL.os, "killpg", side_effect=killpg, create=True), \
                 mock.patch.object(TOOL.signal, "SIGKILL", KILL_SIGNAL, create=True):
                with self.assertRaisesRegex(RuntimeError, "thread start failed"):
                    TOOL.supervise(["/reviewed/recorder"], Path(directory), {"PATH": "/usr/bin"}, 30)
            self.assertEqual(killed, [(process.pid, KILL_SIGNAL)])
            self.assertEqual(process.wait_calls, [5])
            self.assertTrue(process.stdout.closed)

    def _run_capture(self, root: Path, output: Path, *, status=0, reason="emulator-exit",
                     capture_intent="operator-input", host_input_bytes=None):
        source = root / "synthetic-source.zip"
        recorder = root / "synthetic-recorder"
        source.write_bytes(b"synthetic source fixture")
        recorder.write_bytes(b"synthetic executable fixture")
        recorder.chmod(0o700)
        args = SimpleNamespace(experimental_observer=True, source_release=str(source.resolve()),
                               recorder=str(recorder.resolve()), output=str(output),
                               max_duration_seconds=120, machine_profile="svga_s3",
                               capture_intent=capture_intent)
        legacy = TOOL.legacy
        source_identity = (legacy.EXPECTED_RELEASE_SHA256, legacy.EXPECTED_RELEASE_SIZE)
        recorder_identity = (TOOL.protocol.RECORDER_SHA256, TOOL.protocol.RECORDER_SIZE)

        def mount(_source, mountpoint):
            (mountpoint / legacy.GAME_ROOT).mkdir(parents=True)

        def successful_supervise(_command, directory, _environment, _duration):
            values = {"prefix_cs": 0x1234, "prefix_ip": 0x134, "interrupt": 6,
                      "callback_cs": 0xf000, "callback_ip": 0xca64, "stub_ip": 0xca60,
                      "callback_index": 3, "ss": 0x1000, "sp": 0xff00, "return_ip": 0,
                      "return_cs": 0, "return_flags": 0x202, "ax": 1, "bx": 2, "cx": 3, "dx": 4}
            int6 = ("eon-int6-development-v1" + "".join(
                f"\t{name}={values[name]:04x}" for name in TOOL.protocol.SCALAR_NAMES) + "\n")
            default_input = (b"host-key 1 ticks=2 state=down scancode=0x1 sym=0x2 mod=0x0\n"
                             if capture_intent == "operator-input" else b"")
            host_input = default_input if host_input_bytes is None else host_input_bytes
            (directory / "int6-observation.raw").write_text(int6, encoding="ascii")
            (directory / "host-input-receipt.raw").write_bytes(host_input)
            (directory / "recorder-console.log").write_bytes(TOOL.protocol.SUCCESS_MARKER)
            return status, reason, 100.0, 110.0

        stack = contextlib.ExitStack()
        stack.enter_context(mock.patch.object(TOOL.sys, "platform", "linux"))
        stack.enter_context(mock.patch.object(TOOL, "output_directory", return_value=output))
        stack.enter_context(mock.patch.object(TOOL.protocol, "OUTPUT_ROOT", output.parent))
        stack.enter_context(mock.patch.object(legacy, "require_absolute_regular_file", side_effect=lambda path, *_a, **_k: path))
        stack.enter_context(mock.patch.object(legacy, "validate_source_release", return_value=source_identity))
        stack.enter_context(mock.patch.object(legacy, "validate_recorder", return_value=recorder_identity))
        stack.enter_context(mock.patch.object(legacy, "require_visible_operator_input"))
        mount_mock = stack.enter_context(mock.patch.object(TOOL, "mount_archive", side_effect=mount))
        stack.enter_context(mock.patch.object(legacy, "mount_options", return_value="ro,nosuid,nodev"))
        unmount_mock = stack.enter_context(mock.patch.object(TOOL, "unmount_checked"))
        supervise_mock = stack.enter_context(mock.patch.object(TOOL, "supervise", side_effect=successful_supervise))
        stack.enter_context(mock.patch.dict(TOOL.os.environ, {"DISPLAY": ":test", "HOME": str(root)}, clear=True))
        return args, stack, mount_mock, unmount_mock, supervise_mock

    @unittest.skipUnless(sys.platform == "linux", "full capture protocol is Linux-only")
    def test_successful_manual_exit_writes_schema_bound_paired_receipt(self) -> None:
        with temporary_directory() as directory:
            root = Path(directory)
            output_root = root / "terminal-captures"
            output_root.mkdir(mode=0o700)
            output = output_root / "run-01"
            args, stack, mount, unmount, supervise = self._run_capture(root, output)
            with stack, contextlib.redirect_stdout(io.StringIO()):
                result = TOOL.run_capture(args)
            self.assertEqual(result, output)
            mount.assert_called_once()
            unmount.assert_called_once_with(output / "archive-ro")
            self.assertEqual(supervise.call_args.args[0][-4:], ["-c", "c:", "-c", "mill.com 0"])
            self.assertNotIn("AUTOTYPE", " ".join(supervise.call_args.args[0]))
            self.assertNotIn("KEYBOARD_AddKey", " ".join(supervise.call_args.args[0]))
            fields = dict(line.split("=", 1) for line in (output / "run-status.txt").read_text().splitlines())
            self.assertEqual(fields["termination_reason"], "emulator-exit")
            self.assertEqual(fields["exit_status"], "0")
            self.assertEqual(fields["input_origin"], "unclassified-sdl-queue")
            self.assertEqual(fields["host_input_records"], "1")
            self.assertEqual(fields["observer_source_commit"], "234797680781567e18c374c9e62da24de5423db0")
            self.assertTrue((output / "int6-observation.raw").is_file())
            self.assertTrue((output / "host-input-receipt.raw").is_file())

    @unittest.skipUnless(sys.platform == "linux", "full capture protocol is Linux-only")
    def test_diagnostic_no_key_intent_allows_empty_but_not_keyed_receipt(self) -> None:
        with temporary_directory() as directory:
            root = Path(directory)
            output_root = root / "terminal-captures"
            output_root.mkdir(mode=0o700)
            output = output_root / "no-key"
            args, stack, _mount, _unmount, _supervise = self._run_capture(
                root, output, capture_intent="diagnostic-no-key-delivery")
            with stack, contextlib.redirect_stdout(io.StringIO()):
                TOOL.run_capture(args)
            fields = dict(line.split("=", 1) for line in (output / "run-status.txt").read_text().splitlines())
            self.assertEqual(fields["host_input_records"], "0")

    @unittest.skipUnless(sys.platform == "linux", "full capture protocol is Linux-only")
    def test_operator_input_intent_rejects_empty_paired_input_receipt(self) -> None:
        with temporary_directory() as directory:
            root = Path(directory)
            output_root = root / "terminal-captures"
            output_root.mkdir(mode=0o700)
            output = output_root / "empty-input"
            args, stack, _mount, unmount, _supervise = self._run_capture(
                root, output, capture_intent="operator-input", host_input_bytes=b"")
            with stack, contextlib.redirect_stdout(io.StringIO()), self.assertRaisesRegex(ValueError, "capture intent"):
                TOOL.run_capture(args)
            unmount.assert_called_once()
            self.assertFalse((output / "run-status.txt").exists())
            self.assertTrue((output / "run-error.txt").exists())

    @unittest.skipUnless(sys.platform == "linux", "full capture protocol is Linux-only")
    def test_unmount_failure_never_publishes_success_status(self) -> None:
        with temporary_directory() as directory:
            root = Path(directory)
            output_root = root / "terminal-captures"
            output_root.mkdir(mode=0o700)
            output = output_root / "unmount-error"
            args, stack, _mount, unmount, _supervise = self._run_capture(root, output)
            unmount.side_effect = [OSError("unmount failed"), None]
            with stack, contextlib.redirect_stdout(io.StringIO()), self.assertRaisesRegex(OSError, "unmount failed"):
                TOOL.run_capture(args)
            self.assertEqual(unmount.call_count, 2)
            self.assertFalse((output / "run-status.txt").exists())
            self.assertTrue((output / "run-error.txt").exists())

    @unittest.skipUnless(sys.platform == "linux", "full capture protocol is Linux-only")
    def test_timeout_or_nonzero_exit_is_rejected_and_unmounted(self) -> None:
        for status, reason in ((-KILL_SIGNAL, "timeout"), (1, "emulator-exit"), (0, "console-abort")):
            with self.subTest(status=status, reason=reason), temporary_directory() as directory:
                root = Path(directory)
                output_root = root / "terminal-captures"
                output_root.mkdir(mode=0o700)
                output = output_root / "rejected"
                args, stack, _mount, unmount, _supervise = self._run_capture(root, output, status=status, reason=reason)
                with stack, contextlib.redirect_stdout(io.StringIO()), self.assertRaisesRegex(ValueError, "terminal observer rejected"):
                    TOOL.run_capture(args)
                unmount.assert_called_once()
                self.assertTrue((output / "run-error.txt").exists())
                self.assertFalse((output / "run-status.txt").exists())

    def test_capture_preflight_failure_does_not_mount_or_create_output(self) -> None:
        with temporary_directory() as directory:
            root = Path(directory)
            output_root = root / "terminal-captures"
            output_root.mkdir(mode=0o700)
            output = output_root / "headless"
            args, stack, mount, _unmount, _supervise = self._run_capture(root, output)
            # Isolated environment invokes the real visible-display guard.
            stack.close()
            with mock.patch.object(TOOL.sys, "platform", "linux"), \
                 mock.patch.object(TOOL, "output_directory", return_value=output), \
                 mock.patch.object(TOOL.legacy, "require_absolute_regular_file", side_effect=lambda path, *_a, **_k: path), \
                 mock.patch.object(TOOL.legacy, "validate_source_release", return_value=("a"*64, 1)), \
                 mock.patch.object(TOOL.legacy, "validate_recorder", return_value=(TOOL.protocol.RECORDER_SHA256, TOOL.protocol.RECORDER_SIZE)), \
                 mock.patch.dict(TOOL.os.environ, {"HOME": str(root)}, clear=True), \
                 mock.patch.object(TOOL, "mount_archive") as mount:
                with self.assertRaisesRegex(TOOL.legacy.CaptureError, "visible X11 or Wayland"):
                    TOOL.run_capture(args)
            mount.assert_not_called()
            self.assertFalse(output.exists())


if __name__ == "__main__":
    unittest.main()
