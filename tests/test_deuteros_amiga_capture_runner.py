"""Safety contracts for the external, operator-driven Amiga capture helper."""

from __future__ import annotations

import hashlib
import importlib.util
import io
from pathlib import Path
import unittest

from eon_test_paths import temporary_directory


ROOT = Path(__file__).resolve().parents[1]
SPEC = importlib.util.spec_from_file_location(
    "run_deuteros_amiga_capture", ROOT / "tools" / "run_deuteros_amiga_capture.py")
assert SPEC and SPEC.loader
TOOL = importlib.util.module_from_spec(SPEC)
SPEC.loader.exec_module(TOOL)


class DeuterosAmigaCaptureRunnerTests(unittest.TestCase):
    def test_pinned_kickstart_archive_size_is_not_its_rom_payload_size(self) -> None:
        self.assertEqual(TOOL.EXPECTED_KICKSTART_SIZE, 143_269)
        self.assertNotEqual(TOOL.EXPECTED_KICKSTART_SIZE, 262_144)

    def test_rejects_headless_or_missing_visible_display(self) -> None:
        with self.assertRaisesRegex(TOOL.CaptureError, "headless SDL"):
            TOOL.require_visible_operator_input({"SDL_VIDEODRIVER": "dummy", "DISPLAY": ":1"})
        with self.assertRaisesRegex(TOOL.CaptureError, "visible X11 or Wayland"):
            TOOL.require_visible_operator_input({})
        TOOL.require_visible_operator_input({"WAYLAND_DISPLAY": "wayland-0"})

    def test_generated_configuration_locks_media_and_disables_debug_routes(self) -> None:
        configuration = TOOL.recorder_config(
            Path("/safe/disk1.adf"), Path("/safe/disk2.adf"), Path("/safe/kickstart.rom"),
            Path("/safe/capture"))
        self.assertIn("amiga_model = A500", configuration)
        self.assertIn("floppy_write_protect = 1", configuration)
        self.assertIn("console_debugger = 0", configuration)
        self.assertIn("use_debugger = 0", configuration)
        self.assertIn("warp_mode = 0", configuration)
        self.assertNotIn("playback_file", configuration.lower())

    def test_output_rejects_repository_media_and_system_temp_paths(self) -> None:
        with temporary_directory() as directory:
            root = Path(directory)
            media = root / "Downloads"
            media.mkdir()
            release = media / "deuteros.zip"
            kickstart = media / "kickstart.zip"
            release.write_bytes(b"release")
            kickstart.write_bytes(b"kickstart")
            with self.assertRaisesRegex(TOOL.CaptureError, "supplied-media"):
                TOOL.reject_unsafe_output(release.resolve(), kickstart.resolve(), output=(media / "capture").resolve())
            with self.assertRaisesRegex(TOOL.CaptureError, "/tmp"):
                TOOL.reject_unsafe_output(release.resolve(), kickstart.resolve(), output=Path("/tmp/eon-capture"))
            with self.assertRaisesRegex(TOOL.CaptureError, "repository"):
                TOOL.reject_unsafe_output(release.resolve(), kickstart.resolve(), output=ROOT / "capture-output")

    def test_identity_checks_reject_altered_bytes(self) -> None:
        with temporary_directory() as directory:
            source = Path(directory) / "owned-release.zip"
            source.write_bytes(b"test boundary bytes")
            expected_hash = hashlib.sha256(source.read_bytes()).hexdigest()
            self.assertEqual(TOOL.validate_identity(source.resolve(), "test source", expected_hash,
                                                    source.stat().st_size),
                             (expected_hash, source.stat().st_size))
            with self.assertRaisesRegex(TOOL.CaptureError, "exact recognised"):
                TOOL.validate_identity(source.resolve(), "test source", "0" * 64, source.stat().st_size)

    def test_recorder_identity_is_returned_only_for_the_reviewed_binary(self) -> None:
        with temporary_directory() as directory:
            recorder = Path(directory) / "reviewed-recorder"
            recorder.write_bytes(b"recorder boundary bytes")
            original_hash = TOOL.EXPECTED_RECORDER_SHA256
            try:
                TOOL.EXPECTED_RECORDER_SHA256 = hashlib.sha256(recorder.read_bytes()).hexdigest()
                self.assertEqual(TOOL.validate_recorder(recorder),
                                 (TOOL.EXPECTED_RECORDER_SHA256, recorder.stat().st_size))
                TOOL.EXPECTED_RECORDER_SHA256 = "0" * 64
                with self.assertRaisesRegex(TOOL.CaptureError, "reviewed FS-UAE"):
                    TOOL.validate_recorder(recorder)
            finally:
                TOOL.EXPECTED_RECORDER_SHA256 = original_hash

    def test_input_receipt_status_keeps_no_input_distinct_from_a_receipt(self) -> None:
        with temporary_directory() as directory:
            receipt = Path(directory) / "host-input-receipt.txt"
            self.assertEqual(TOOL.input_receipt_status(receipt), "host_input_receipt=absent\n")
            receipt.write_bytes(b"")
            self.assertEqual(TOOL.input_receipt_status(receipt), "host_input_receipt=empty\n")
            observed = b"host-input 1 frame=2 line=3 action=key state=down\n"
            receipt.write_bytes(observed)
            status = TOOL.input_receipt_status(receipt)
            self.assertIn("host_input_receipt=present\n", status)
            self.assertIn(f"host_input_receipt_bytes={len(observed)}\n", status)
            receipt.unlink()
            receipt.symlink_to("missing")
            with self.assertRaisesRegex(TOOL.CaptureError, "regular non-symlink"):
                TOOL.input_receipt_status(receipt)
            receipt.unlink()
            receipt.write_bytes(b"x" * (TOOL.MAX_INPUT_RECEIPT_BYTES + 1))
            with self.assertRaisesRegex(TOOL.CaptureError, "bounded recorder contract"):
                TOOL.input_receipt_status(receipt)

    def test_raw_recorder_observation_is_hash_bound_and_bounded(self) -> None:
        with temporary_directory() as directory:
            raw = Path(directory) / "raw-pc.txt"
            self.assertEqual(TOOL.raw_observation_status(raw, "raw_pc"), "raw_pc=absent\n")
            raw.write_bytes(b"pc 0x001000\n")
            self.assertIn("raw_pc_sha256=", TOOL.raw_observation_status(raw, "raw_pc"))
            raw.unlink()
            raw.symlink_to("missing")
            with self.assertRaisesRegex(TOOL.CaptureError, "regular non-symlink"):
                TOOL.raw_observation_status(raw, "raw_pc")
            raw.unlink()
            raw.write_bytes(b"x" * (TOOL.MAX_RAW_OBSERVATION_BYTES + 1))
            with self.assertRaisesRegex(TOOL.CaptureError, "bounded recorder contract"):
                TOOL.raw_observation_status(raw, "raw_pc")

    def test_console_transcript_is_hashed_but_disk_bounded(self) -> None:
        with temporary_directory() as directory:
            path = Path(directory) / "recorder-console.log"
            observed = b"x" * (TOOL.MAX_RECORDER_CONSOLE_LOG_BYTES + 17)
            status = TOOL.capture_bounded_console(io.BytesIO(observed), path)
            self.assertEqual(status.total_bytes, len(observed))
            self.assertEqual(status.retained_bytes, TOOL.MAX_RECORDER_CONSOLE_LOG_BYTES)
            self.assertTrue(status.truncated)
            self.assertEqual(status.sha256, hashlib.sha256(observed).hexdigest())
            self.assertEqual(path.stat().st_size, TOOL.MAX_RECORDER_CONSOLE_LOG_BYTES)
            receipt = TOOL.recorder_console_status(status)
            self.assertIn("recorder_console_truncated=true", receipt)

    def test_identity_status_retains_a_reviewable_capture_preimage(self) -> None:
        for name in ("source_release", "kickstart_archive", "recorder", "configuration"):
            status = TOOL.identity_status(name, ("a" * 64, 123))
            self.assertEqual(status, f"{name}_sha256=" + "a" * 64
                             + f"\n{name}_bytes=123\n")


if __name__ == "__main__":
    unittest.main()
