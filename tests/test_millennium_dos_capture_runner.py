"""Safety contracts for the external, operator-driven DOS capture helper."""

from __future__ import annotations

import hashlib
import importlib.util
import io
from pathlib import Path
import unittest

from eon_test_paths import temporary_directory


ROOT = Path(__file__).resolve().parents[1]
SPEC = importlib.util.spec_from_file_location(
    "run_millennium_dos_capture", ROOT / "tools" / "run_millennium_dos_capture.py")
assert SPEC and SPEC.loader
TOOL = importlib.util.module_from_spec(SPEC)
SPEC.loader.exec_module(TOOL)


class MillenniumDosCaptureRunnerTests(unittest.TestCase):
    def test_rejects_headless_or_missing_visible_display(self) -> None:
        with self.assertRaisesRegex(TOOL.CaptureError, "headless SDL"):
            TOOL.require_visible_operator_input({"SDL_VIDEODRIVER": "dummy", "DISPLAY": ":1"})
        with self.assertRaisesRegex(TOOL.CaptureError, "visible X11 or Wayland"):
            TOOL.require_visible_operator_input({})
        TOOL.require_visible_operator_input({"WAYLAND_DISPLAY": "wayland-0"})

    def test_generated_configuration_handles_real_mode_wrap_and_never_injects_input(self) -> None:
        configuration = TOOL.recorder_config(Path("/safe/read-only/game-root"))
        self.assertIn("core=normal", configuration)
        self.assertIn("segment limits=false", configuration)
        self.assertIn("machine=svga_s3", configuration)
        self.assertIn('mount c "/safe/read-only/game-root"', configuration)
        self.assertNotIn("AUTOTYPE", configuration)
        self.assertNotIn("KEYBOARD_AddKey", configuration)

    def test_machine_profiles_are_finite_and_rendered_verbatim(self) -> None:
        self.assertIn("machine=ega", TOOL.recorder_config(Path("/safe/read-only/game-root"), "ega"))
        with self.assertRaisesRegex(TOOL.CaptureError, "finite profile"):
            TOOL.recorder_config(Path("/safe/read-only/game-root"), "vgaonly")

    def test_output_rejects_repository_media_and_system_temp_paths(self) -> None:
        with temporary_directory() as directory:
            root = Path(directory)
            media = root / "Downloads"
            media.mkdir()
            source = media / "owned-release.zip"
            source.write_bytes(b"test boundary bytes")
            with self.assertRaisesRegex(TOOL.CaptureError, "supplied-media"):
                TOOL.reject_unsafe_output(source.resolve(), (media / "capture").resolve())
            with self.assertRaisesRegex(TOOL.CaptureError, "/tmp"):
                TOOL.reject_unsafe_output(source.resolve(), Path("/tmp/eon-capture"))
            with self.assertRaisesRegex(TOOL.CaptureError, "repository"):
                TOOL.reject_unsafe_output(source.resolve(), ROOT / "capture-output")

    def test_source_hash_is_exact_and_rejects_other_bytes(self) -> None:
        with temporary_directory() as directory:
            source = Path(directory) / "owned-release.zip"
            source.write_bytes(b"test boundary bytes")
            original_hash = TOOL.EXPECTED_RELEASE_SHA256
            original_size = TOOL.EXPECTED_RELEASE_SIZE
            try:
                TOOL.EXPECTED_RELEASE_SHA256 = hashlib.sha256(source.read_bytes()).hexdigest()
                TOOL.EXPECTED_RELEASE_SIZE = source.stat().st_size
                self.assertEqual(TOOL.validate_source_release(source.resolve()),
                                 (TOOL.EXPECTED_RELEASE_SHA256, TOOL.EXPECTED_RELEASE_SIZE))
                TOOL.EXPECTED_RELEASE_SHA256 = "0" * 64
                with self.assertRaisesRegex(TOOL.CaptureError, "exact recognised"):
                    TOOL.validate_source_release(source.resolve())
            finally:
                TOOL.EXPECTED_RELEASE_SHA256 = original_hash
                TOOL.EXPECTED_RELEASE_SIZE = original_size

    def test_recorder_hash_is_exact_and_rejects_another_executable(self) -> None:
        with temporary_directory() as directory:
            recorder = Path(directory) / "reviewed-recorder"
            recorder.write_bytes(b"recorder boundary bytes")
            original_hash = TOOL.EXPECTED_RECORDER_SHA256
            try:
                TOOL.EXPECTED_RECORDER_SHA256 = hashlib.sha256(recorder.read_bytes()).hexdigest()
                self.assertEqual(TOOL.validate_recorder(recorder),
                                 (TOOL.EXPECTED_RECORDER_SHA256, recorder.stat().st_size))
                TOOL.EXPECTED_RECORDER_SHA256 = "0" * 64
                with self.assertRaisesRegex(TOOL.CaptureError, "reviewed DOSBox-X"):
                    TOOL.validate_recorder(recorder)
            finally:
                TOOL.EXPECTED_RECORDER_SHA256 = original_hash

    def test_identity_status_retains_the_complete_capture_preimage(self) -> None:
        for name in ("source_release", "recorder", "configuration"):
            status = TOOL.identity_status(name, ("a" * 64, 123))
            self.assertEqual(status, f"{name}_sha256=" + "a" * 64
                             + f"\n{name}_bytes=123\n")

    def test_input_receipt_status_never_invents_an_empty_timeline(self) -> None:
        with temporary_directory() as directory:
            receipt = Path(directory) / "host-input-receipt.raw"
            self.assertEqual(TOOL.input_receipt_status(receipt), "host_input_receipt=absent\n")
            receipt.write_bytes(b"")
            self.assertEqual(TOOL.input_receipt_status(receipt), "host_input_receipt=empty\n")
            observed = b"host-key 1 ticks=2 state=down scancode=0x1 sym=0x2 mod=0x0\n"
            receipt.write_bytes(observed)
            status = TOOL.input_receipt_status(receipt)
            self.assertIn("host_input_receipt=present\n", status)
            self.assertIn(f"host_input_receipt_bytes={len(observed)}\n", status)
            self.assertIn("host_input_receipt_records=1\n", status)
            receipt.write_bytes(b"host-key 2 ticks=2 state=down scancode=0x1 sym=0x2 mod=0x0\n")
            with self.assertRaisesRegex(TOOL.CaptureError, "ordinals"):
                TOOL.input_receipt_status(receipt)
            receipt.write_bytes(b"host-key 1 ticks=2 state=pressed scancode=0x1 sym=0x2 mod=0x0\n")
            with self.assertRaisesRegex(TOOL.CaptureError, "invalid recorder record"):
                TOOL.input_receipt_status(receipt)
            receipt.unlink()
            receipt.symlink_to("missing")
            with self.assertRaisesRegex(TOOL.CaptureError, "regular non-symlink"):
                TOOL.input_receipt_status(receipt)
            receipt.unlink()
            receipt.write_bytes(b"x" * (TOOL.MAX_INPUT_RECEIPT_BYTES + 1))
            with self.assertRaisesRegex(TOOL.CaptureError, "bounded recorder contract"):
                TOOL.input_receipt_status(receipt)

    def test_raw_observation_status_is_hash_bound_and_bounded(self) -> None:
        with temporary_directory() as directory:
            path = Path(directory) / "events.raw"
            self.assertEqual(TOOL.raw_observation_status(path, "events_raw"), "events_raw=absent\n")
            path.write_bytes(b"event\n")
            self.assertIn("events_raw_sha256=", TOOL.raw_observation_status(path, "events_raw"))
            results = Path(directory) / "results.raw"
            results.write_bytes(b"raw-result\t1 1 image=mill.com pc=0x020e source-int=0x21 source-ax=0x2591 ax=0x2591\n")
            status = TOOL.raw_observation_status(results, "results_raw")
            self.assertIn("results_raw_records=1\n", status)
            self.assertIn("results_raw_shapes=mill.com:020e:1\n", status)
            results.write_bytes(
                b"raw-result\t1 1 private-vector image=titles.exe pc=0x0127 int=0x91 "
                b"vector_ip=0x1234 vector_cs=0xabcd\n")
            status = TOOL.raw_observation_status(results, "results_raw")
            self.assertIn("results_raw_shapes=private-vector:1\n", status)
            results.write_bytes(b"raw-result\t1 1 private-handler-entry int=0x91 cs=0xabcd ip=0x1234\n")
            status = TOOL.raw_observation_status(results, "results_raw")
            self.assertIn("results_raw_shapes=private-handler-entry:1\n", status)
            results.write_bytes(
                b"raw-result\t1 1 private-handler-return int=0x91 caller=titles.exe pc=0x0129 "
                b"ax=0x0101 flags=0x0002\n")
            status = TOOL.raw_observation_status(results, "results_raw")
            self.assertIn("results_raw_shapes=private-handler-return:1\n", status)
            results.write_bytes(b"raw-result\t2 2 image=mill.com pc=0x020e source-int=0x21 source-ax=0x2591 ax=0x2591\n")
            with self.assertRaisesRegex(TOOL.CaptureError, "counters"):
                TOOL.raw_observation_status(results, "results_raw")
            path.write_bytes(b"x" * (TOOL.MAX_RAW_OBSERVATION_BYTES + 1))
            with self.assertRaisesRegex(TOOL.CaptureError, "bounded recorder contract"):
                TOOL.raw_observation_status(path, "events_raw")

    def test_known_unhandled_interrupt_requires_complete_v10_raw_sequence(self) -> None:
        with temporary_directory() as directory:
            results = Path(directory) / "results.raw"
            self.assertFalse(TOOL.known_unhandled_interrupt_observed(results))
            results.write_bytes(
                b"raw-result\t1 1 fault=unhandled-interrupt int=0x06 cs=0xf000 ip=0xca64 "
                b"ss=0x0a8d sp=0xc9bf ax=0x00a0 bx=0x6101 cx=0x178b dx=0x6101")
            self.assertFalse(TOOL.known_unhandled_interrupt_observed(results))
            results.write_bytes(results.read_bytes() + b"\n")
            self.assertFalse(TOOL.known_unhandled_interrupt_observed(results))
            prefix = (
                b"raw-result\t1 1 image=mill.com pc=0x020e source-int=0x21 source-ax=0x2591 ax=0x2591\n"
                b"raw-result\t2 2 image=mill.com pc=0x0213 source-call=0x0511 ax=0x0000\n"
                b"raw-result\t3 3 private-vector image=titles.exe pc=0x0127 int=0x91 vector_ip=0x0000 vector_cs=0x087e\n"
                b"raw-result\t4 4 private-handler-entry int=0x91 cs=0x087e ip=0x0000\n"
                b"raw-result\t5 5 private-handler-return int=0x91 caller=titles.exe pc=0x0129 ax=0x0101 flags=0x7202\n"
                b"raw-result\t6 6 image=titles.exe pc=0x0129 source-int=0x91 source-ax=0x0000 ax=0x0101\n"
                b"raw-result\t7 7 image=titles.exe pc=0x0129 source-int=0x91 source-ax=0x0000 ax=0x0000\n"
                b"raw-result\t8 8 fault=unhandled-interrupt int=0x06 cs=0xf000 ip=0xca64 "
                b"ss=0x0a8d sp=0xc9bf ax=0x00a0 bx=0x6101 cx=0x178b dx=0x6101\n")
            results.write_bytes(prefix)
            self.assertTrue(TOOL.known_unhandled_interrupt_observed(results))
            results.write_bytes(results.read_bytes() +
                b"raw-result\t9 9 fault=unhandled-interrupt int=0x06 cs=0xf000 ip=0xca64 "
                b"ss=0x0a8d sp=0xc9bf ax=0x00a0 bx=0x6101 cx=0x178b dx=0x6101\n")
            self.assertFalse(TOOL.known_unhandled_interrupt_observed(results))
            results.write_bytes(b"not a recorder record\n")
            self.assertFalse(TOOL.known_unhandled_interrupt_observed(results))

    def test_console_transcript_is_hashed_but_disk_bounded(self) -> None:
        """A recorder exception loop must not make cache or terminal output unbounded."""
        with temporary_directory() as directory:
            path = Path(directory) / "recorder-console.log"
            bytes_to_observe = b"x" * (TOOL.MAX_RECORDER_CONSOLE_LOG_BYTES + 17)
            over_limit = TOOL.threading.Event()
            status = TOOL.capture_bounded_console(io.BytesIO(bytes_to_observe), path, over_limit)
            self.assertEqual(status.total_bytes, len(bytes_to_observe))
            self.assertEqual(status.retained_bytes, TOOL.MAX_RECORDER_CONSOLE_LOG_BYTES)
            self.assertTrue(status.truncated)
            self.assertEqual(status.sha256, hashlib.sha256(bytes_to_observe).hexdigest())
            self.assertEqual(path.stat().st_size, TOOL.MAX_RECORDER_CONSOLE_LOG_BYTES)
            self.assertFalse(status.over_limit)
            receipt = TOOL.recorder_console_status(status)
            self.assertIn("recorder_console_truncated=true", receipt)
            self.assertIn("recorder_console_over_limit=false", receipt)

    def test_console_total_safety_cap_signals_for_child_termination(self) -> None:
        with temporary_directory() as directory:
            path = Path(directory) / "recorder-console.log"
            over_limit = TOOL.threading.Event()
            original_limit = TOOL.MAX_RECORDER_CONSOLE_TOTAL_BYTES
            try:
                TOOL.MAX_RECORDER_CONSOLE_TOTAL_BYTES = 16
                bytes_to_observe = b"x" * 17
                status = TOOL.capture_bounded_console(io.BytesIO(bytes_to_observe), path, over_limit)
                self.assertTrue(over_limit.is_set())
                self.assertTrue(status.over_limit)
                self.assertEqual(status.total_bytes, len(bytes_to_observe))
                self.assertEqual(path.stat().st_size, len(bytes_to_observe))
            finally:
                TOOL.MAX_RECORDER_CONSOLE_TOTAL_BYTES = original_limit


if __name__ == "__main__":
    unittest.main()
