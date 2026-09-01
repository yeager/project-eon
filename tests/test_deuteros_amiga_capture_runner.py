"""Safety contracts for the external, operator-driven Amiga capture helper."""

from __future__ import annotations

import hashlib
import importlib.util
import io
from pathlib import Path
from types import SimpleNamespace
import unittest
from unittest import mock

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

    def test_nested_disk_archives_are_pinned_before_their_adfs_are_mounted(self) -> None:
        self.assertEqual(TOOL.EXPECTED_DISK1_ARCHIVE_SIZE, 449_666)
        self.assertEqual(TOOL.EXPECTED_DISK2_ARCHIVE_SIZE, 490_962)
        self.assertNotEqual(TOOL.EXPECTED_DISK1_ARCHIVE_SHA256,
                            TOOL.EXPECTED_DISK1_SHA256)
        self.assertNotEqual(TOOL.EXPECTED_DISK2_ARCHIVE_SHA256,
                            TOOL.EXPECTED_DISK2_SHA256)

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

    def test_timing_profile_is_finite_and_bound_into_configuration(self) -> None:
        configuration = TOOL.recorder_config(
            Path("/safe/disk1.adf"), Path("/safe/disk2.adf"), Path("/safe/kickstart.rom"),
            Path("/safe/capture"), "warp")
        self.assertIn("warp_mode = 1", configuration)
        with self.assertRaisesRegex(TOOL.CaptureError, "finite profile set"):
            TOOL.recorder_config(
                Path("/safe/disk1.adf"), Path("/safe/disk2.adf"), Path("/safe/kickstart.rom"),
                Path("/safe/capture"), "unbounded")

    def test_unmount_requires_the_exact_fuse_view_to_disappear(self) -> None:
        with temporary_directory() as directory:
            mountpoint = Path(directory) / "capture-view"
            mountpoint.mkdir()
            self.assertFalse(TOOL.mountpoint_is_active(mountpoint))
            with mock.patch.object(TOOL.subprocess, "run", side_effect=(
                    SimpleNamespace(returncode=0, stdout=str(mountpoint) + "\n"),
                    SimpleNamespace(returncode=1, stdout=""),
                    SimpleNamespace(returncode=0, stdout=str(mountpoint) + "\n"),
            )):
                with self.assertRaisesRegex(TOOL.CaptureError, "unable to unmount"):
                    TOOL.unmount(mountpoint)

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
            observed = b"host-input 1 frame=2 line=3 action=4 state=1\n"
            receipt.write_bytes(observed)
            status = TOOL.input_receipt_status(receipt)
            self.assertIn("host_input_receipt=present\n", status)
            self.assertIn(f"host_input_receipt_bytes={len(observed)}\n", status)
            self.assertIn("host_input_receipt_records=1\n", status)
            receipt.write_bytes(b"host-input 2 frame=2 line=3 action=4 state=1\n")
            with self.assertRaisesRegex(TOOL.CaptureError, "ordinals"):
                TOOL.input_receipt_status(receipt)
            receipt.write_bytes(b"host-input 1 frame=2 line=3 action=key state=down\n")
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

    def test_live_input_delivery_observer_never_parses_a_file_being_appended(self) -> None:
        with temporary_directory() as directory:
            receipt = Path(directory) / "host-input-receipt.txt"
            self.assertFalse(TOOL.input_delivery_file_observed(receipt))
            receipt.write_bytes(b"host-input 1 frame=2")
            self.assertTrue(TOOL.input_delivery_file_observed(receipt))
            receipt.unlink()
            receipt.symlink_to("missing")
            with self.assertRaisesRegex(TOOL.CaptureError, "regular non-symlink"):
                TOOL.input_delivery_file_observed(receipt)

    def test_focus_settle_duration_is_exposed_by_argument_parsing(self) -> None:
        arguments = TOOL.parse_arguments((
            "--source-release", "/release.zip", "--kickstart-archive", "/kickstart.zip",
            "--recorder", "/recorder", "--output", "/capture", "--focus-settle-seconds", "0",
        ))
        self.assertEqual(arguments.focus_settle_seconds, 0)

    def test_raw_recorder_observation_is_hash_bound_and_bounded(self) -> None:
        with temporary_directory() as directory:
            raw = Path(directory) / "raw-pc.txt"
            receipt = Path(directory) / "host-input-receipt.txt"
            receipt.write_bytes(b"host-input 1 frame=2 line=3 action=4 state=1\n")
            self.assertEqual(TOOL.raw_observation_status(raw, "raw_pc"), "raw_pc=absent\n")
            observed = (b"raw-pc 1 cycles=1 pc=0x000210d4 opcode=0x4e75 "
                        b"d0=0x00000000 a0=0x00000000 a6=0x00000000 sr=0x0000\n"
                        b"raw-pc 2 cycles=2 pc=0x000210d4 opcode=0x4e75 "
                        b"d0=0x00000000 a0=0x00000000 a6=0x00000000 sr=0x0000\n")
            raw.write_bytes(observed)
            status = TOOL.raw_observation_status(raw, "raw_pc")
            self.assertIn("raw_pc_sha256=", status)
            self.assertIn("raw_pc_records=2\n", status)
            self.assertIn("raw_pc_site_counts=0x000210d4:2\n", status)
            v7_observed = (b"raw-pc 1 cycles=1 pc=0x000210d4 ir_opcode=0x4e75 memory_opcode=0x4e75 "
                           b"d0=0x00000000 a0=0x00000000 a6=0x00000000 sr=0x0000\n")
            raw.write_bytes(v7_observed)
            v7_status = TOOL.raw_observation_status(raw, "raw_pc", "v7")
            self.assertIn("raw_pc_format=v7\n", v7_status)
            self.assertIn("raw_pc_records=1\n", v7_status)
            self.assertIn("raw_pc_opcode_pairs=0x000210d4:4e75/4e75\n", v7_status)
            v9_observed = (b"raw-pc 1 cycles=1 pc=0x000210d4 ir_opcode=0x4e75 memory_opcode=0x4e75 "
                           b"d0=0x00000000 a0=0x00000000 a6=0x00000000 sr=0x0000 "
                           b"input_ordinal=0 input_frame=0\n")
            raw.write_bytes(v9_observed)
            v9_status = TOOL.raw_observation_status(raw, "raw_pc", "v9")
            self.assertIn("raw_pc_format=v9\n", v9_status)
            self.assertIn("raw_pc_input_links=0\n", v9_status)
            self.assertEqual(TOOL.raw_pc_input_chronology_status(raw, receipt),
                             "raw_pc_input_chronology=none\n")
            raw.write_bytes(v9_observed.replace(b"input_ordinal=0 input_frame=0", b"input_ordinal=1 input_frame=2"))
            self.assertEqual(TOOL.raw_pc_input_chronology_status(raw, receipt),
                             "raw_pc_input_chronology=linked\nraw_pc_input_chronology_records=1\n")
            raw.write_bytes(v9_observed.replace(b"input_ordinal=0 input_frame=0", b"input_ordinal=1 input_frame=3"))
            with self.assertRaisesRegex(TOOL.CaptureError, "does not match"):
                TOOL.raw_pc_input_chronology_status(raw, receipt)
            raw.write_bytes(v9_observed.replace(b"input_ordinal=0 input_frame=0", b"input_ordinal=0 input_frame=1"))
            with self.assertRaisesRegex(TOOL.CaptureError, "frame zero"):
                TOOL.raw_observation_status(raw, "raw_pc", "v9")
            with self.assertRaisesRegex(TOOL.CaptureError, "invalid recorder record"):
                TOOL.raw_observation_status(raw, "raw_pc")
            raw.write_bytes(b"raw-pc 2 cycles=1 pc=0x000210d4 opcode=0x4e75 "
                            b"d0=0x00000000 a0=0x00000000 a6=0x00000000 sr=0x0000\n")
            with self.assertRaisesRegex(TOOL.CaptureError, "ordinals"):
                TOOL.raw_observation_status(raw, "raw_pc")
            raw.write_bytes(b"raw-pc 1 cycles=1 pc=0x00000001 opcode=0x4e75 "
                            b"d0=0x00000000 a0=0x00000000 a6=0x00000000 sr=0x0000\n")
            with self.assertRaisesRegex(TOOL.CaptureError, "unreviewed probe"):
                TOOL.raw_observation_status(raw, "raw_pc")
            raw.unlink()
            raw.symlink_to("missing")
            with self.assertRaisesRegex(TOOL.CaptureError, "regular non-symlink"):
                TOOL.raw_observation_status(raw, "raw_pc")
            raw.unlink()
            raw.write_bytes(b"x" * (TOOL.MAX_RAW_OBSERVATION_BYTES + 1))
            with self.assertRaisesRegex(TOOL.CaptureError, "bounded recorder contract"):
                TOOL.raw_observation_status(raw, "raw_pc")

    def test_title_display_receipt_is_title_armed_bounded_and_hash_bound(self) -> None:
        with temporary_directory() as directory:
            root = Path(directory)
            display = root / "title-display.txt"
            input_receipt = root / "host-input-receipt.txt"
            input_receipt.write_text(
                "host-input 1 frame=2 line=3 action=4 state=1\n", encoding="ascii")
            display.write_text(
                "display-arm 1 cycles=10 site=0x0001eda6 input_ordinal=0 input_frame=0\n"
                "display-write 2 cycles=11 vpos=1 hpos=2 origin=cpu register=0x0080 value=0x1234 input_ordinal=0 input_frame=0\n"
                "display-write 3 cycles=12 vpos=2 hpos=3 origin=copper register=0x0180 value=0xabcd input_ordinal=1 input_frame=2\n",
                encoding="ascii")
            status = TOOL.title_display_receipt_status(display, input_receipt)
            self.assertIn("title_display=present\n", status)
            self.assertIn("title_display_format=v10\n", status)
            self.assertIn("title_display_records=3\n", status)
            self.assertIn("title_display_register_counts=0x0080:1,0x0180:1\n", status)
            self.assertIn("title_display_input_chronology=linked\n", status)
            display.write_text(
                "display-arm 1 cycles=10 site=0x0001eda6 input_ordinal=0 input_frame=0\n"
                "display-write 2 cycles=11 vpos=1 hpos=2 origin=cpu register=0x0081 value=0x1234 input_ordinal=0 input_frame=0\n",
                encoding="ascii")
            with self.assertRaisesRegex(TOOL.CaptureError, "unreviewed display register"):
                TOOL.title_display_receipt_status(display, input_receipt)
            display.write_text(
                "display-arm 1 cycles=10 site=0x0001eda6 input_ordinal=0 input_frame=0\n"
                "display-write 3 cycles=11 vpos=1 hpos=2 origin=cpu register=0x0080 value=0x1234 input_ordinal=0 input_frame=0\n",
                encoding="ascii")
            with self.assertRaisesRegex(TOOL.CaptureError, "ordinals"):
                TOOL.title_display_receipt_status(display, input_receipt)
            display.write_text(
                "display-arm 1 cycles=10 site=0x0001eda6 input_ordinal=0 input_frame=0\n"
                "display-write 2 cycles=11 vpos=1 hpos=2 origin=cpu register=0x0080 value=0x1234 input_ordinal=1 input_frame=3\n",
                encoding="ascii")
            with self.assertRaisesRegex(TOOL.CaptureError, "does not match"):
                TOOL.title_display_receipt_status(display, input_receipt)

    def test_console_transcript_is_hashed_but_disk_bounded(self) -> None:
        with temporary_directory() as directory:
            path = Path(directory) / "recorder-console.log"
            observed = b"x" * (TOOL.MAX_RECORDER_CONSOLE_LOG_BYTES + 17)
            over_limit = TOOL.threading.Event()
            status = TOOL.capture_bounded_console(io.BytesIO(observed), path, over_limit)
            self.assertEqual(status.total_bytes, len(observed))
            self.assertEqual(status.retained_bytes, TOOL.MAX_RECORDER_CONSOLE_LOG_BYTES)
            self.assertTrue(status.truncated)
            self.assertEqual(status.sha256, hashlib.sha256(observed).hexdigest())
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
                observed = b"x" * 17
                status = TOOL.capture_bounded_console(io.BytesIO(observed), path, over_limit)
                self.assertTrue(over_limit.is_set())
                self.assertTrue(status.over_limit)
                self.assertEqual(status.total_bytes, len(observed))
            finally:
                TOOL.MAX_RECORDER_CONSOLE_TOTAL_BYTES = original_limit

    def test_identity_status_retains_a_reviewable_capture_preimage(self) -> None:
        for name in ("source_release", "kickstart_archive", "disk1_archive", "disk2_archive",
                     "recorder", "configuration"):
            status = TOOL.identity_status(name, ("a" * 64, 123))
            self.assertEqual(status, f"{name}_sha256=" + "a" * 64
                             + f"\n{name}_bytes=123\n")


if __name__ == "__main__":
    unittest.main()
