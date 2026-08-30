"""Safety contracts for the external, operator-driven Amiga capture helper."""

from __future__ import annotations

import hashlib
import importlib.util
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


if __name__ == "__main__":
    unittest.main()
