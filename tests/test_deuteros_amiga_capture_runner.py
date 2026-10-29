"""Safety contracts for the external, operator-driven Amiga capture helper."""

from __future__ import annotations

import hashlib
import importlib.util
import io
import contextlib
from pathlib import Path
from pathlib import PurePosixPath
from types import SimpleNamespace
import unittest
from unittest import mock

from eon_test_paths import CanonicalReceiptPath, LfTextFixtureWrites, temporary_directory


ROOT = Path(__file__).resolve().parents[1]
SPEC = importlib.util.spec_from_file_location(
    "run_deuteros_amiga_capture", ROOT / "tools" / "run_deuteros_amiga_capture.py")
assert SPEC and SPEC.loader
TOOL = importlib.util.module_from_spec(SPEC)
SPEC.loader.exec_module(TOOL)


class DeuterosAmigaCaptureRunnerTests(LfTextFixtureWrites, unittest.TestCase):
    def test_zero_route_observer_sidecar_is_enabled_for_v21_through_v23(self) -> None:
        self.assertEqual(TOOL.RECORDER_ZERO_ROUTE_OBSERVATION_HASHES, frozenset((
            TOOL.TRV2_RECORDER_V21_SHA256,
            TOOL.TRV2_RECORDER_V22_SHA256,
            TOOL.TRV2_RECORDER_V23_SHA256,
        )))
        self.assertNotIn(TOOL.TRV2_RECORDER_V20_SHA256,
                         TOOL.RECORDER_ZERO_ROUTE_OBSERVATION_HASHES)

    def test_latch_write_receipt_is_bounded_exact_and_input_linked(self) -> None:
        with temporary_directory() as directory:
            root = Path(directory)
            sidecar = CanonicalReceiptPath(root / "latch-write.txt")
            host = CanonicalReceiptPath(root / "host-input-receipt.txt")
            host.write_text("host-input 1 frame=80 line=2 action=149 state=1\n", encoding="ascii")
            record = ("latch-write 1 cycles=100 site=0x00021868 next_pc=0x00021870 "
                      "opcode=0x13fc target=0x00021720 before=0x00 after=0x01 "
                      "input_ordinal=1 input_frame=80\n")
            sidecar.write_text(record, encoding="ascii")
            status = dict(line.split("=", 1) for line in
                          TOOL.latch_write_receipt_status(sidecar, host).splitlines())
            self.assertEqual(status["latch_write"], "present")
            self.assertEqual(status["latch_write_records"], "1")
            self.assertEqual(status["latch_write_input_chronology"], "linked")
            sidecar.write_text(record.replace("before=0x00", "before=0x01"), encoding="ascii")
            with self.assertRaisesRegex(TOOL.CaptureError, "invalid recorder record"):
                TOOL.latch_write_receipt_status(sidecar, host)
            sidecar.write_text(record, encoding="ascii")
            host.write_text("host-input 1 frame=81 line=2 action=149 state=1\n", encoding="ascii")
            with self.assertRaisesRegex(TOOL.CaptureError, "exact host-input chronology"):
                TOOL.latch_write_receipt_status(sidecar, host)

    def test_latch_write_receipt_accepts_absent_and_rejects_size_over_cap(self) -> None:
        with temporary_directory() as directory:
            root = Path(directory)
            sidecar = root / "latch-write.txt"
            host = root / "host-input-receipt.txt"
            self.assertIn("latch_write=absent\n", TOOL.latch_write_receipt_status(sidecar, host))
            sidecar.write_bytes(b"x" * (TOOL.MAX_LATCH_WRITE_BYTES + 1))
            with self.assertRaisesRegex(TOOL.CaptureError, "bounded recorder contract"):
                TOOL.latch_write_receipt_status(sidecar, host)

    @staticmethod
    def _zero_route_observation_payload() -> str:
        rows: list[str] = []
        cycle = 0

        def append(pc: int, address: int = 0, width: int = 0, value: int = 0,
                   valid: int = 0, cells_valid: int | None = None,
                   *, a0: int = 0x20000, a1: int = 0x30000,
                   a2: int = 0x31000, a4: int = 0x40000,
                   d2: int = 0x5A) -> None:
            nonlocal cycle
            cycle += 10
            if cells_valid is None:
                cells_valid = int(pc in {0x1FC22, 0x1FC28, 0x1FC2C})
            regs = " ".join(f"d{i}=0x{(d2 if i == 2 else i):08x}" for i in range(8))
            rows.append(
                f"zero-route-observation {len(rows) + 1} cycles={cycle} pc=0x{pc:08x} "
                f"input_ordinal=1 input_frame=10 {regs} a0=0x{a0:08x} a1=0x{a1:08x} "
                f"a2=0x{a2:08x} a4=0x{a4:08x} cell_1f98c=0x00 cell_1f98e=0x00 "
                f"cells_valid={cells_valid} mem_addr=0x{address:08x} mem_width={width} "
                f"mem_value=0x{value:08x} mem_valid={valid}\n")

        append(0x1FC22, 0x1F98E, 1, 0, 1)
        append(0x1FC28)
        append(0x1FC2C)
        for pc, address in ((0x1FC42, 0x1F99C), (0x1FC4A, 0x1F974),
                            (0x1FC50, 0x1F96C), (0x1FC56, 0x1F970)):
            append(pc, address, 4, 0x20000, 1)
        for row in range(8):
            glyph_address = 0x20000 + row
            append(0x1FC5E, glyph_address, 1, 0x41 + row, 1, a0=glyph_address)
            for plane in range(4):
                source_a = 0x30000 + row * 8 + plane * 2
                source_b = 0x31000 + row * 8 + plane * 2
                dest = 0x40000 + row * 0x28 + plane * 0x1F40
                write_value = (0x5A + row + plane) & 0xFF
                append(0x1FC6A, source_a, 2, 0x1234, 1, a1=source_a)
                append(0x1FC6C, source_b, 2, 0x5678, 1, a2=source_b)
                append(0x1FC74, dest, 1, write_value, 0, a4=dest, d2=write_value)
                append(0x1FC76, dest, 1, write_value, 1, a4=dest, d2=write_value)
        append(0x1FC88, 0x1F9A0, 4, 0x28, 1)
        append(0x1FC8E, 0x1F974, 4, 0x40000, 1)
        append(0x1FC94, 0x1F974, 4, 0x40028, 1)
        append(0x1FC9A)
        return "".join(rows)

    def test_zero_route_observation_binds_complete_invocation_and_input(self) -> None:
        payload = self._zero_route_observation_payload()
        with temporary_directory() as directory:
            root = Path(directory)
            sidecar = CanonicalReceiptPath(root / "zero-route-observation.txt")
            host = CanonicalReceiptPath(root / "host-input-receipt.txt")
            sidecar.write_text(payload, encoding="ascii")
            host.write_text("host-input 1 frame=10 line=3 action=149 state=1\n", encoding="ascii")
            invocations = TOOL.parse_zero_route_observation(sidecar, host)
            self.assertEqual(len(invocations), 1)
            self.assertEqual(len(invocations[0]), 147)
            branch_sample = next(row for row in invocations[0] if row["pc"] == 0x1FC28)
            self.assertEqual(branch_sample["cells_valid"], 1)
            self.assertEqual((branch_sample["cell_1f98c"], branch_sample["cell_1f98e"]), (0, 0))
            status = dict(line.split("=", 1) for line in
                          TOOL.zero_route_observation_status(sidecar, host).splitlines())
            self.assertEqual(status["zero_route_observation_records"], "147")
            self.assertEqual(status["zero_route_observation_invocations"], "1")
            self.assertEqual(status["zero_route_observation_sha256"],
                             hashlib.sha256(payload.encode("ascii")).hexdigest())

    def test_zero_route_observation_rejects_partial_memory_chronology_and_route(self) -> None:
        payload = self._zero_route_observation_payload()
        with temporary_directory() as directory:
            root = Path(directory)
            sidecar = CanonicalReceiptPath(root / "zero-route-observation.txt")
            host = CanonicalReceiptPath(root / "host-input-receipt.txt")
            host.write_text("host-input 1 frame=10 line=3 action=149 state=1\n", encoding="ascii")
            sidecar.write_text(payload.replace("pc=0x0001fc42", "pc=0x0001fc42", 1)
                               .replace("mem_addr=0x0001f99c", "mem_addr=0x0001f99d", 1),
                               encoding="ascii")
            with self.assertRaisesRegex(TOOL.CaptureError, "fixed-cell read"):
                TOOL.parse_zero_route_observation(sidecar, host)

            sidecar.write_text(payload.rsplit("zero-route-observation ", 1)[0], encoding="ascii")
            with self.assertRaisesRegex(TOOL.CaptureError, "complete start-through-return"):
                TOOL.parse_zero_route_observation(sidecar, host)

            sidecar.write_text(payload, encoding="ascii")
            host.write_text("host-input 1 frame=11 line=3 action=149 state=1\n", encoding="ascii")
            with self.assertRaisesRegex(TOOL.CaptureError, "chronology"):
                TOOL.parse_zero_route_observation(sidecar, host)

    def test_zero_route_observation_rejects_unbounded_sidecar(self) -> None:
        with temporary_directory() as directory:
            sidecar = Path(directory) / "zero-route-observation.txt"
            sidecar.write_bytes(b"x" * (TOOL.MAX_ZERO_ROUTE_OBSERVATION_BYTES + 1))
            with self.assertRaisesRegex(TOOL.CaptureError, "bounded recorder contract"):
                TOOL.parse_zero_route_observation(sidecar, Path(directory) / "unused-host-input.txt")

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

    def test_rejects_inaccessible_x11_display_before_capture(self) -> None:
        environment = {"DISPLAY": ":6", "XAUTHORITY": "/cache/Xauthority6"}
        with mock.patch.object(TOOL.shutil, "which", return_value="/usr/bin/xdpyinfo"), \
                mock.patch.object(TOOL.subprocess, "run",
                    return_value=SimpleNamespace(returncode=1)) as probe:
            with self.assertRaisesRegex(TOOL.CaptureError, "check DISPLAY and XAUTHORITY"):
                TOOL.require_visible_operator_input(environment)
        probe.assert_called_once_with(["/usr/bin/xdpyinfo", "-display", ":6"],
            env=environment, capture_output=True, text=True, timeout=5, check=False)

    def test_accepts_accessible_x11_display(self) -> None:
        environment = {"DISPLAY": ":6", "XAUTHORITY": "/cache/Xauthority6"}
        with mock.patch.object(TOOL.shutil, "which", return_value="/usr/bin/xdpyinfo"), \
                mock.patch.object(TOOL.subprocess, "run",
                    return_value=SimpleNamespace(returncode=0)) as probe:
            TOOL.require_visible_operator_input(environment)
        probe.assert_called_once()

    def test_generated_configuration_locks_media_and_disables_debug_routes(self) -> None:
        configuration = TOOL.recorder_config(
            PurePosixPath("/safe/disk1.adf"), PurePosixPath("/safe/disk2.adf"),
            PurePosixPath("/safe/kickstart.rom"), PurePosixPath("/safe/capture"))
        self.assertIn("amiga_model = A500", configuration)
        self.assertIn("floppy_drive_0 = /safe/disk1.adf", configuration)
        self.assertNotIn("floppy_drive_1 =", configuration)
        self.assertIn("floppy_image_0 = /safe/disk1.adf", configuration)
        self.assertIn("floppy_image_1 = /safe/disk2.adf", configuration)
        self.assertIn("floppy_write_protect = 1", configuration)
        self.assertIn("console_debugger = 0", configuration)
        self.assertIn("use_debugger = 0", configuration)
        self.assertIn("warp_mode = 0", configuration)
        self.assertNotIn("playback_file", configuration.lower())

    def test_timing_profile_is_finite_and_bound_into_configuration(self) -> None:
        configuration = TOOL.recorder_config(
            PurePosixPath("/safe/disk1.adf"), PurePosixPath("/safe/disk2.adf"),
            PurePosixPath("/safe/kickstart.rom"), PurePosixPath("/safe/capture"), "warp")
        self.assertIn("warp_mode = 1", configuration)
        with self.assertRaisesRegex(TOOL.CaptureError, "finite profile set"):
            TOOL.recorder_config(
                PurePosixPath("/safe/disk1.adf"), PurePosixPath("/safe/disk2.adf"),
                PurePosixPath("/safe/kickstart.rom"), PurePosixPath("/safe/capture"), "unbounded")

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
                with self.assertRaisesRegex(TOOL.CaptureError, "expected SHA-256 " + "0" * 64):
                    TOOL.validate_recorder(recorder)
                with self.assertRaisesRegex(TOOL.CaptureError, hashlib.sha256(recorder.read_bytes()).hexdigest()):
                    TOOL.validate_recorder(recorder)
            finally:
                TOOL.EXPECTED_RECORDER_SHA256 = original_hash

    def test_additional_reviewed_build_does_not_replace_the_historical_pin(self) -> None:
        with temporary_directory() as directory:
            recorder = Path(directory) / "second-reviewed-build"
            recorder.write_bytes(b"second recorder boundary bytes")
            digest = hashlib.sha256(recorder.read_bytes()).hexdigest()
            historical = TOOL.EXPECTED_RECORDER_SHA256
            with mock.patch.object(TOOL, "reviewed_recorder_hashes", return_value={
                "reviewed-fs-uae": historical, "reviewed-second-host": digest,
            }):
                self.assertEqual(TOOL.validate_recorder(recorder), (digest, recorder.stat().st_size))
                recorder.write_bytes(b"unreviewed changed build")
                with self.assertRaises(TOOL.CaptureError):
                    TOOL.validate_recorder(recorder)
            self.assertEqual(TOOL.EXPECTED_RECORDER_SHA256, historical)

    def test_trv2_v13_build_is_pinned_without_replacing_v12(self) -> None:
        hashes = TOOL.reviewed_recorder_hashes()
        self.assertEqual(hashes["reviewed-fs-uae-trv2-v12"], TOOL.TRV2_RECORDER_SHA256)
        self.assertEqual(hashes["reviewed-fs-uae-trv2-v13"], TOOL.TRV2_RECORDER_V13_SHA256)

    def test_trv2_v14_build_pin_coexists_with_v12_and_v13(self) -> None:
        hashes = TOOL.reviewed_recorder_hashes()
        self.assertEqual(hashes["reviewed-fs-uae-trv2-v12"], TOOL.TRV2_RECORDER_SHA256)
        self.assertEqual(hashes["reviewed-fs-uae-trv2-v13"], TOOL.TRV2_RECORDER_V13_SHA256)
        self.assertEqual(hashes["reviewed-fs-uae-trv2-v14"], TOOL.TRV2_RECORDER_V14_SHA256)
        self.assertEqual(
            TOOL.TRV2_RECORDER_V14_SHA256,
            "701d11b705dd36934712ab37df4e105a2d68bde4dea2642214f45012d6768acf",
        )

    def test_trv2_v15_build_pin_coexists_and_raw_site_is_admitted(self) -> None:
        hashes = TOOL.reviewed_recorder_hashes()
        self.assertEqual(hashes["reviewed-fs-uae-trv2-v14"], TOOL.TRV2_RECORDER_V14_SHA256)
        self.assertEqual(hashes["reviewed-fs-uae-trv2-v15"], TOOL.TRV2_RECORDER_V15_SHA256)
        self.assertEqual(
            TOOL.TRV2_RECORDER_V15_SHA256,
            "7160dfafbfe67b17db931065ab6f9853874591ea4af6c33ad51059b2b0f703df",
        )

    def test_trv2_v16_pin_adds_only_the_new_site_and_preserves_v15_grammar(self) -> None:
        hashes = TOOL.reviewed_recorder_hashes()
        self.assertEqual(hashes["reviewed-fs-uae-trv2-v15"], TOOL.TRV2_RECORDER_V15_SHA256)
        self.assertEqual(hashes["reviewed-fs-uae-trv2-v16"], TOOL.TRV2_RECORDER_V16_SHA256)
        self.assertEqual(TOOL.TRV2_RECORDER_V16_SIZE, 62_015_016)
        self.assertEqual(TOOL.raw_pc_sites_for_format("v9"), TOOL.RAW_PC_SITES)
        self.assertEqual(TOOL.raw_pc_sites_for_format("v9-v16"), (*TOOL.RAW_PC_SITES, 0x218CC))
        sample = ("raw-pc 1 cycles=1 pc=0x000218cc ir_opcode=0x4e75 memory_opcode=0x4e75 "
                  "d0=0x00000000 a0=0x00000000 a6=0x00000000 sr=0x0000 "
                  "input_ordinal=0 input_frame=0\n")
        with temporary_directory() as directory:
            raw = CanonicalReceiptPath(Path(directory) / "raw-pc.txt")
            raw.write_text(sample, encoding="ascii")
            with self.assertRaisesRegex(TOOL.CaptureError, "unreviewed probe site"):
                TOOL.parse_raw_pc_observations(raw, "v9")
            self.assertEqual(TOOL.parse_raw_pc_observations(raw, "v9-v16"), {0x218CC: 1})
            raw.write_text("".join(sample.replace("raw-pc 1 ", f"raw-pc {ordinal} ")
                                       for ordinal in range(1, 130)), encoding="ascii")
            with self.assertRaisesRegex(TOOL.CaptureError, "phase cap"):
                TOOL.parse_raw_pc_observations(raw, "v9-v16")
            raw.write_text("".join(
                f"raw-pc {ordinal} cycles={ordinal} pc=0x000218cc ir_opcode=0x4e75 "
                f"memory_opcode=0x4e75 d0=0x00000000 a0=0x00000000 a6=0x00000000 "
                f"sr=0x0000 input_ordinal={0 if ordinal <= 65 else 1} "
                f"input_frame={0 if ordinal <= 65 else 2}\n"
                for ordinal in range(1, 130)), encoding="ascii")
            with self.assertRaisesRegex(TOOL.CaptureError, "per-site recorder cap"):
                TOOL.parse_raw_pc_observations(raw, "v9-v16")

    def test_trv2_v17_pin_coexists_with_v15_v16_and_uses_phased_grammar(self) -> None:
        hashes = TOOL.reviewed_recorder_hashes()
        self.assertEqual(hashes["reviewed-fs-uae-trv2-v15"], TOOL.TRV2_RECORDER_V15_SHA256)
        self.assertEqual(hashes["reviewed-fs-uae-trv2-v16"], TOOL.TRV2_RECORDER_V16_SHA256)
        self.assertEqual(hashes["reviewed-fs-uae-trv2-v17"], TOOL.TRV2_RECORDER_V17_SHA256)
        self.assertEqual(TOOL.TRV2_RECORDER_V17_SIZE, 62_016_168)
        self.assertEqual(TOOL.raw_pc_sites_for_format("v9-v16-phased"),
                         (*TOOL.RAW_PC_SITES, 0x218CC))

    def test_trv2_v18_adds_hash_pinned_btst_branch_site_without_changing_v17(self) -> None:
        hashes = TOOL.reviewed_recorder_hashes()
        self.assertEqual(hashes["reviewed-fs-uae-trv2-v17"], TOOL.TRV2_RECORDER_V17_SHA256)
        self.assertEqual(hashes["reviewed-fs-uae-trv2-v18"], TOOL.TRV2_RECORDER_V18_SHA256)
        self.assertEqual(TOOL.TRV2_RECORDER_V18_SIZE, 62_016_152)
        self.assertEqual(TOOL.raw_pc_sites_for_format("v9-v16-phased"),
                         (*TOOL.RAW_PC_SITES, 0x218CC))
        self.assertEqual(TOOL.raw_pc_sites_for_format("v9-v18-phased"),
                         (*TOOL.RAW_PC_SITES, 0x218CC, 0x21866))
        sample = ("raw-pc 1 cycles=1 pc=0x0002185e ir_opcode=0x0839 memory_opcode=0x0839 "
                  "d0=0x00000000 a0=0x00000000 a6=0x00000000 sr=0x0004 "
                  "input_ordinal=1 input_frame=2\n"
                  "raw-pc 2 cycles=2 pc=0x00021866 ir_opcode=0x6608 memory_opcode=0x6608 "
                  "d0=0x00000000 a0=0x00000000 a6=0x00000000 sr=0x0004 "
                  "input_ordinal=1 input_frame=2\n")
        with temporary_directory() as directory:
            raw = CanonicalReceiptPath(Path(directory) / "raw-pc.txt")
            raw.write_text(sample, encoding="ascii")
            with self.assertRaisesRegex(TOOL.CaptureError, "unreviewed probe site"):
                TOOL.parse_raw_pc_observations(raw, "v9-v16-phased")
            self.assertEqual(TOOL.parse_raw_pc_observations(raw, "v9-v18-phased"),
                             {0x2185E: 1, 0x21866: 1})
            raw.write_text(sample.replace("pc=0x00021866 ir_opcode=0x6608 memory_opcode=0x6608",
                                          "pc=0x00021866 ir_opcode=0x6608 memory_opcode=0x4e75"),
                           encoding="ascii")
            with self.assertRaisesRegex(TOOL.CaptureError, "unexpected memory opcode"):
                TOOL.parse_raw_pc_observations(raw, "v9-v18-phased")
            raw.write_text(sample.replace(
                "raw-pc 2 cycles=2 pc=0x00021866 ir_opcode=0x6608 memory_opcode=0x6608 "
                "d0=0x00000000 a0=0x00000000 a6=0x00000000 sr=0x0004 "
                "input_ordinal=1 input_frame=2",
                "raw-pc 2 cycles=2 pc=0x00021866 ir_opcode=0x6608 memory_opcode=0x6608 "
                "d0=0x00000000 a0=0x00000000 a6=0x00000000 sr=0x0004 "
                "input_ordinal=2 input_frame=3"), encoding="ascii")
            with self.assertRaisesRegex(TOOL.CaptureError, "same input-linked bit-test"):
                TOOL.parse_raw_pc_observations(raw, "v9-v18-phased")

    def test_v19_late_pc_window_is_distinct_bounded_and_input_linked(self) -> None:
        self.assertEqual(TOOL.reviewed_recorder_hashes()["reviewed-fs-uae-trv2-v19"],
                         TOOL.TRV2_RECORDER_V19_SHA256)
        self.assertEqual(TOOL.TRV2_RECORDER_V19_SIZE, 62_020_024)
        self.assertEqual(TOOL.raw_pc_sites_for_format("v9-v19-phased"),
                         (*TOOL.RAW_PC_V18_SITES, 0x1FEA8))
        rows = []
        for ordinal in range(1, 11):
            rows.append(f"host-input {ordinal} frame={ordinal * 10} line=3 action=149 state=1\n")
        with temporary_directory() as directory:
            root = Path(directory)
            host = CanonicalReceiptPath(root / "host-input-receipt.txt")
            host.write_text("".join(rows), encoding="ascii")
            late = CanonicalReceiptPath(root / "late-input-pc.txt")
            late.write_text(
                "late-pc 1 cycles=90 pc=0x0001fe84 ir_opcode=0x7202 memory_opcode=0x7202 "
                "d0=0x00000000 a0=0x00000000 a6=0x00000000 sr=0x0000 "
                "input_ordinal=9 input_frame=90\n"
                "late-pc 2 cycles=91 pc=0x0001fea8 ir_opcode=0x4e75 memory_opcode=0x4e75 "
                "d0=0x00000000 a0=0x00000000 a6=0x00000000 sr=0x0000 "
                "input_ordinal=10 input_frame=100\n", encoding="ascii")
            parsed = TOOL.parse_late_raw_pc_receipt(late)
            self.assertEqual([(row[2], row[3]) for row in parsed], [(0x1FE84, 9), (0x1FEA8, 10)])
            status = TOOL.late_raw_pc_status(late, host)
            self.assertIn("late_input_pc_records=2\n", status)
            self.assertIn("late_input_pc_last_input_ordinal=10\n", status)

            late.write_text(late.read_text(encoding="ascii").replace(
                "input_ordinal=10 input_frame=100", "input_ordinal=10 input_frame=99"), encoding="ascii")
            with self.assertRaisesRegex(TOOL.CaptureError, "host-input receipt"):
                TOOL.late_raw_pc_status(late, host)

            late.write_text("".join(
                f"late-pc {i} cycles={i} pc=0x0001fea8 ir_opcode=0x4e75 memory_opcode=0x4e75 "
                f"d0=0x00000000 a0=0x00000000 a6=0x00000000 sr=0x0000 "
                f"input_ordinal={i + 8} input_frame={i}\n" for i in range(1, 98)), encoding="ascii")
            with self.assertRaisesRegex(TOOL.CaptureError, "per-site sample cap"):
                TOOL.parse_late_raw_pc_receipt(late)

    def test_v19_late_selector_is_bijective_with_late_dispatch_pc(self) -> None:
        with temporary_directory() as directory:
            root = Path(directory)
            late = CanonicalReceiptPath(root / "late-input-pc.txt")
            host = CanonicalReceiptPath(root / "host-input-receipt.txt")
            selector = CanonicalReceiptPath(root / "late-selector-dispatch.txt")
            host.write_text("".join(
                f"host-input {i} frame={i * 10} line=3 action=149 state=1\n"
                for i in range(1, 10)), encoding="ascii")
            late.write_text(
                "late-pc 1 cycles=90 pc=0x0001fbe6 ir_opcode=0x4a39 memory_opcode=0x4a39 "
                "d0=0x00000000 a0=0x00000000 a6=0x00000000 sr=0x0000 "
                "input_ordinal=9 input_frame=90\n", encoding="ascii")
            selector.write_text(
                "late-selector-dispatch 1 late_raw_ordinal=1 cycles=90 pc=0x0001fbe6 "
                "cell_1f98c=0x00 cell_1f98e=0x01 input_ordinal=9 input_frame=90\n",
                encoding="ascii")
            status = TOOL.late_selector_dispatch_status(selector, late, host)
            self.assertIn("late_selector_dispatch_records=1\n", status)
            selector.unlink()
            with self.assertRaisesRegex(TOOL.CaptureError, "omits a reached"):
                TOOL.late_selector_dispatch_status(selector, late, host)

    def test_v20_late_selector_targets_are_version_scoped_and_bounded(self) -> None:
        self.assertEqual(TOOL.reviewed_recorder_hashes()["reviewed-fs-uae-trv2-v20"],
                         TOOL.TRV2_RECORDER_V20_SHA256)
        self.assertEqual(TOOL.TRV2_RECORDER_V20_SIZE, 62_020_224)
        self.assertEqual(TOOL.LATE_RAW_PC_V20_SITES,
                         (*TOOL.LATE_RAW_PC_V19_SITES, 0x1FC22, 0x1FC9C))
        self.assertEqual(TOOL._late_raw_pc_contract("v19"),
                         (TOOL.LATE_RAW_PC_V19_SITES, 26 * TOOL.MAX_LATE_RAW_RECORDS_PER_SITE))
        self.assertEqual(TOOL._late_raw_pc_contract("v20"),
                         (TOOL.LATE_RAW_PC_V20_SITES, 28 * TOOL.MAX_LATE_RAW_RECORDS_PER_SITE))
        with temporary_directory() as directory:
            root = Path(directory)
            host = CanonicalReceiptPath(root / "host-input-receipt.txt")
            host.write_text("".join(
                f"host-input {i} frame={i * 10} line=3 action=149 state=1\n"
                for i in range(1, 10)), encoding="ascii")
            late = CanonicalReceiptPath(root / "late-input-pc.txt")
            for index, site in enumerate((0x1FC22, 0x1FC9C), start=1):
                late.write_text(
                    f"late-pc 1 cycles={index} pc=0x{site:08x} ir_opcode=0x4e75 memory_opcode=0x4e75 "
                    "d0=0x00000000 a0=0x00000000 a6=0x00000000 sr=0x0000 "
                    "input_ordinal=9 input_frame=90\n", encoding="ascii")
                with self.assertRaisesRegex(TOOL.CaptureError, "unreviewed probe site"):
                    TOOL.parse_late_raw_pc_receipt(late, "v19")
                parsed = TOOL.parse_late_raw_pc_receipt(late, "v20")
                self.assertEqual(parsed[0][2], site)
                status = TOOL.late_raw_pc_status(late, host, "v20")
                self.assertIn(f"0x{site:08x}:1", status)

            late.write_text(
                "late-pc 1 cycles=1 pc=0x0001fca6 ir_opcode=0x4e75 memory_opcode=0x4e75 "
                "d0=0x00000000 a0=0x00000000 a6=0x00000000 sr=0x0000 "
                "input_ordinal=9 input_frame=90\n", encoding="ascii")
            with self.assertRaisesRegex(TOOL.CaptureError, "unreviewed probe site"):
                TOOL.parse_late_raw_pc_receipt(late, "v20")
            with self.assertRaisesRegex(TOOL.CaptureError, "not a reviewed recorder contract"):
                TOOL.parse_late_raw_pc_receipt(late, "v21")

    def test_v17_selector_dispatch_is_joined_to_raw_pc_and_host_input(self) -> None:
        with temporary_directory() as directory:
            root = Path(directory)
            raw = CanonicalReceiptPath(root / "raw-pc.txt")
            host = CanonicalReceiptPath(root / "host-input-receipt.txt")
            dispatch = CanonicalReceiptPath(root / "selector-dispatch.txt")
            raw.write_text(
                "raw-pc 1 cycles=10 pc=0x0001fbe6 ir_opcode=0x4a39 memory_opcode=0x4a39 "
                "d0=0x00000000 a0=0x00000000 a6=0x00000000 sr=0x0000 "
                "input_ordinal=1 input_frame=2\n", encoding="ascii")
            host.write_text("host-input 1 frame=2 line=3 action=149 state=1\n", encoding="ascii")
            dispatch.write_text(
                "selector-dispatch 1 raw_ordinal=1 cycles=10 pc=0x0001fbe6 "
                "cell_1f98c=0x00 cell_1f98e=0x01 input_ordinal=1 input_frame=2\n",
                encoding="ascii")
            status = TOOL.selector_dispatch_status(dispatch, raw, host)
            self.assertIn("selector_dispatch=present\n", status)
            self.assertIn("selector_dispatch_records=1\n", status)
            self.assertIn("selector_dispatch_raw_pc_links=1\n", status)
            self.assertIn("selector_dispatch_input_links=1\n", status)

            dispatch.write_text(
                "selector-dispatch 1 raw_ordinal=1 cycles=11 pc=0x0001fbe6 "
                "cell_1f98c=0x00 cell_1f98e=0x01 input_ordinal=1 input_frame=2\n",
                encoding="ascii")
            with self.assertRaisesRegex(TOOL.CaptureError, "do not match raw_pc chronology"):
                TOOL.selector_dispatch_status(dispatch, raw, host)

            dispatch.unlink()
            with self.assertRaisesRegex(TOOL.CaptureError, "missing samples present in raw_pc"):
                TOOL.selector_dispatch_status(dispatch, raw, host)

    def test_v17_selector_dispatch_has_a_separate_128_per_phase_cap(self) -> None:
        with temporary_directory() as directory:
            path = Path(directory) / "selector-dispatch.txt"
            path.write_text("".join(
                f"selector-dispatch {i} raw_ordinal={i} cycles={i} pc=0x0001fbe6 "
                f"cell_1f98c=0x00 cell_1f98e=0x00 input_ordinal=0 input_frame=0\n"
                for i in range(1, 130)), encoding="ascii")
            with self.assertRaisesRegex(TOOL.CaptureError, "per-phase sample cap"):
                TOOL.parse_selector_dispatch_receipt(path)

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
            "--capture-intent", "diagnostic-no-input",
        ))
        self.assertEqual(arguments.focus_settle_seconds, 0)
        self.assertEqual(arguments.capture_intent, "diagnostic-no-input")
        with self.assertRaises(SystemExit), mock.patch("sys.stderr", new_callable=io.StringIO):
            TOOL.parse_arguments((
                "--source-release", "/release.zip", "--kickstart-archive", "/kickstart.zip",
                "--recorder", "/recorder", "--output", "/capture",
            ))

    def test_standalone_source_requires_both_ordered_disk_archives(self) -> None:
        common = ("--kickstart-archive", "/kickstart.zip", "--recorder", "/recorder",
                  "--output", "/capture", "--capture-intent", "diagnostic-no-input")
        arguments = TOOL.parse_arguments(("--disk1-archive", "/disk1.zip",
                                          "--disk2-archive", "/disk2.zip", *common))
        self.assertIsNone(arguments.source_release)
        self.assertEqual(arguments.disk1_archive, "/disk1.zip")
        self.assertEqual(arguments.disk2_archive, "/disk2.zip")
        for invalid in (("--disk1-archive", "/disk1.zip"),
                        ("--disk2-archive", "/disk2.zip"),
                        ("--source-release", "/release.zip", "--disk2-archive", "/disk2.zip"),
                        ("--source-release", "/release.zip", "--disk1-archive", "/disk1.zip")):
            with self.subTest(invalid=invalid), self.assertRaises(SystemExit), \
                    mock.patch("sys.stderr", new_callable=io.StringIO):
                TOOL.parse_arguments((*invalid, *common))

    def test_standalone_source_output_cannot_be_inside_either_media_directory(self) -> None:
        with temporary_directory() as directory:
            root = Path(directory)
            disk1_dir = root / "disk1-media"
            disk2_dir = root / "disk2-media"
            disk1_dir.mkdir()
            disk2_dir.mkdir()
            disk1 = disk1_dir / "disk1.zip"
            disk2 = disk2_dir / "disk2.zip"
            kickstart = root / "kickstart.zip"
            for source in (disk1, disk2, kickstart):
                source.write_bytes(b"supplied")
            for unsafe in (disk1_dir / "capture", disk2_dir / "capture"):
                with self.subTest(unsafe=unsafe), self.assertRaisesRegex(TOOL.CaptureError, "supplied-media"):
                    TOOL.reject_unsafe_output(disk1.resolve(), disk2.resolve(), kickstart.resolve(),
                                              output=unsafe.resolve())

    def test_swapped_standalone_archives_fail_before_any_mount(self) -> None:
        with temporary_directory() as directory:
            root = Path(directory)
            media = root / "media"
            cache = root / "cache"
            media.mkdir()
            cache.mkdir()
            disk1 = media / "disk1.zip"
            disk2 = media / "disk2.zip"
            kickstart = media / "kickstart.zip"
            recorder = root / "recorder"
            disk1.write_bytes(b"disk one")
            disk2.write_bytes(b"disk two")
            kickstart.write_bytes(b"kickstart")
            recorder.write_bytes(b"recorder")
            recorder.chmod(0o700)
            args = SimpleNamespace(source_release=None, disk1_archive=str(disk2),
                                   disk2_archive=str(disk1), kickstart_archive=str(kickstart),
                                   recorder=str(recorder), output=str(cache / "capture"),
                                   duration_seconds=15, focus_settle_seconds=0,
                                   capture_intent="diagnostic-no-input", timing_profile="realtime")
            with mock.patch.object(TOOL, "require_visible_operator_input"), \
                    mock.patch.object(TOOL, "mount_read_only") as mount, \
                    self.assertRaisesRegex(TOOL.CaptureError, "exact recognised"):
                TOOL.run_capture(args)
            mount.assert_not_called()
            self.assertFalse((cache / "capture").exists())

    def test_standalone_capture_mounts_ordered_inputs_and_rechecks_after_run(self) -> None:
        with temporary_directory() as directory:
            root = Path(directory)
            media = root / "media"
            cache = root / "cache"
            media.mkdir()
            cache.mkdir()
            disk1 = media / "disk1.zip"
            disk2 = media / "disk2.zip"
            kickstart = media / "kickstart.zip"
            recorder = root / "recorder"
            for path in (disk1, disk2, kickstart, recorder):
                path.write_bytes(b"placeholder")
            recorder.chmod(0o700)
            output = cache / "capture"
            args = SimpleNamespace(source_release=None, disk1_archive=str(disk1),
                                   disk2_archive=str(disk2), kickstart_archive=str(kickstart),
                                   recorder=str(recorder), output=str(output), duration_seconds=15,
                                   focus_settle_seconds=0, capture_intent="diagnostic-no-input",
                                   timing_profile="realtime")
            hash_calls: dict[str, int] = {}

            def identity(path, label, expected_hash, expected_size):
                if label in {"Deuteros disk 1 archive", "Deuteros disk 2 archive", "Kickstart archive"}:
                    hash_calls[label] = hash_calls.get(label, 0) + 1
                return expected_hash, expected_size

            process = SimpleNamespace(stdout=io.BytesIO(b"bounded test console\n"), poll=lambda: 0)
            with mock.patch.object(TOOL, "require_visible_operator_input"), \
                    mock.patch.object(TOOL, "validate_identity", side_effect=identity), \
                    mock.patch.object(TOOL, "validate_recorder", return_value=(TOOL.TRV2_RECORDER_V21_SHA256, TOOL.TRV2_RECORDER_V21_SIZE)), \
                    mock.patch.object(TOOL, "mount_read_only") as mount, \
                    mock.patch.object(TOOL, "unmount") as unmount, \
                    mock.patch.object(TOOL.subprocess, "Popen", return_value=process) as popen, \
                    contextlib.redirect_stdout(io.StringIO()):
                TOOL.run_capture(args)

            self.assertEqual(mount.call_args_list, [
                mock.call(disk1, output / "disk1-ro"),
                mock.call(disk2, output / "disk2-ro"),
                mock.call(kickstart, output / "kickstart-ro"),
            ])
            self.assertEqual(unmount.call_args_list, [
                mock.call(output / "kickstart-ro"),
                mock.call(output / "disk2-ro"),
                mock.call(output / "disk1-ro"),
            ])
            self.assertEqual(hash_calls, {"Deuteros disk 1 archive": 2,
                                          "Deuteros disk 2 archive": 2,
                                          "Kickstart archive": 2})
            recorder_environment = popen.call_args.kwargs["env"]
            self.assertEqual(recorder_environment["PROJECT_EON_FS_UAE_ZERO_ROUTE_RECORD"],
                             str(output / "zero-route-observation.txt"))
            receipt = (output / "run-status.txt").read_text(encoding="utf-8")
            self.assertIn("capture_receipt_version=31\n", receipt)
            self.assertIn("recorder_protocol=deuteros-amiga-fsuae-v21\n", receipt)
            self.assertIn("zero_route_observation=absent\n", receipt)
            self.assertIn("source_layout=standalone-zip-pair\n", receipt)
            self.assertIn("source_container=two-independent-zip-files\n", receipt)
            self.assertIn(f"content_release_sha256={TOOL.EXPECTED_RELEASE_SHA256}\n", receipt)
            self.assertNotIn("source_release_sha256=", receipt)
            self.assertNotIn("source_release_bytes=", receipt)

    def test_standalone_capture_rejects_disk_change_and_unmounts_all_views(self) -> None:
        with temporary_directory() as directory:
            root = Path(directory)
            media = root / "media"
            cache = root / "cache"
            media.mkdir()
            cache.mkdir()
            disk1, disk2, kickstart, recorder = (media / name for name in
                                                   ("disk1.zip", "disk2.zip", "kickstart.zip", "recorder"))
            for path in (disk1, disk2, kickstart, recorder):
                path.write_bytes(b"placeholder")
            recorder.chmod(0o700)
            output = cache / "capture"
            args = SimpleNamespace(source_release=None, disk1_archive=str(disk1),
                                   disk2_archive=str(disk2), kickstart_archive=str(kickstart),
                                   recorder=str(recorder), output=str(output), duration_seconds=15,
                                   focus_settle_seconds=0, capture_intent="diagnostic-no-input",
                                   timing_profile="realtime")
            disk1_checks = 0

            def identity(path, label, expected_hash, expected_size):
                nonlocal disk1_checks
                if label == "Deuteros disk 1 archive":
                    disk1_checks += 1
                    if disk1_checks == 2:
                        raise TOOL.CaptureError("disk 1 archive changed during capture")
                return expected_hash, expected_size

            process = SimpleNamespace(stdout=io.BytesIO(b"bounded test console\n"), poll=lambda: 0)
            unmount_calls = []

            def fail_one_unmount(path):
                unmount_calls.append(path)
                if path == output / "kickstart-ro":
                    raise OSError("injected cleanup fault")

            with mock.patch.object(TOOL, "require_visible_operator_input"), \
                    mock.patch.object(TOOL, "validate_identity", side_effect=identity), \
                    mock.patch.object(TOOL, "validate_recorder", return_value=(TOOL.EXPECTED_RECORDER_SHA256, 17)), \
                    mock.patch.object(TOOL, "mount_read_only") as mount, \
                    mock.patch.object(TOOL, "unmount", side_effect=fail_one_unmount), \
                    mock.patch.object(TOOL.subprocess, "Popen", return_value=process), \
                    contextlib.redirect_stdout(io.StringIO()), \
                    self.assertRaisesRegex(TOOL.CaptureError, "unable to clean up 1 read-only"):
                TOOL.run_capture(args)
            self.assertEqual(disk1_checks, 2)
            self.assertEqual(mount.call_count, 3)
            self.assertEqual(unmount_calls, [output / "kickstart-ro", output / "disk2-ro",
                                              output / "disk1-ro"])

    def test_capture_intent_fails_closed_against_the_recorder_input_receipt(self) -> None:
        present = ("host_input_receipt=present\n"
                   "host_input_receipt_sha256=" + "a" * 64 + "\n"
                   "host_input_receipt_bytes=1\nhost_input_receipt_records=1\n")
        self.assertEqual(TOOL.capture_intent_status("physical-input", present, True),
                         "capture_intent=physical-input\ncapture_intent_input_requirement=required\n")
        self.assertEqual(TOOL.capture_intent_status("diagnostic-no-input", "host_input_receipt=empty\n", False),
                         "capture_intent=diagnostic-no-input\ncapture_intent_input_requirement=forbidden\n")
        with self.assertRaisesRegex(TOOL.CaptureError, "requires"):
            TOOL.capture_intent_status("physical-input", "host_input_receipt=empty\n", False)
        with self.assertRaisesRegex(TOOL.CaptureError, "must not"):
            TOOL.capture_intent_status("diagnostic-no-input", present, True)

    def test_no_input_instructions_forbid_keys_instead_of_requesting_them(self) -> None:
        diagnostic = "\n".join(TOOL.capture_operator_instructions("diagnostic-no-input"))
        physical = "\n".join(TOOL.capture_operator_instructions("physical-input"))
        self.assertIn("Do not click it or press any key", diagnostic)
        self.assertNotIn("press and release", diagnostic)
        self.assertIn("press and release", physical)
        self.assertIn("Choose 1: ENGLISH only when the language selector is on screen", physical)
        self.assertIn("At the Disk 2 prompt, press F10 once", physical)
        late_physical = "\n".join(TOOL.capture_operator_instructions("physical-input", late_sampling=True))
        self.assertIn("late probe begins after host-input ordinal 8", late_physical)

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
            input_poll_observed = v9_observed.replace(b"0x000210d4", b"0x00021822")
            raw.write_bytes(input_poll_observed)
            input_poll_status = TOOL.raw_observation_status(raw, "raw_pc", "v9")
            self.assertIn("raw_pc_site_counts=0x00021822:1\n", input_poll_status)
            self.assertIn("raw_pc_opcode_pairs=0x00021822:4e75/4e75\n", input_poll_status)
            branch_observed = (v9_observed.replace(b"0x000210d4", b"0x0002182a")
                               .replace(b"ir_opcode=0x4e75 memory_opcode=0x4e75",
                                        b"ir_opcode=0x6608 memory_opcode=0x6608")
                               .replace(b"sr=0x0000", b"sr=0x0004"))
            raw.write_bytes(branch_observed)
            branch_status = TOOL.raw_observation_status(raw, "raw_pc", "v9")
            self.assertIn("raw_pc_site_counts=0x0002182a:1\n", branch_status)
            self.assertIn("raw_pc_opcode_pairs=0x0002182a:6608/6608\n", branch_status)
            display_base_observed = v9_observed.replace(b"0x000210d4", b"0x0001edac")
            raw.write_bytes(display_base_observed)
            display_base_status = TOOL.raw_observation_status(raw, "raw_pc", "v9")
            self.assertIn("raw_pc_site_counts=0x0001edac:1\n", display_base_status)
            raw.write_bytes(v9_observed)
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
            display = CanonicalReceiptPath(root / "title-display.txt")
            input_receipt = CanonicalReceiptPath(root / "host-input-receipt.txt")
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

    def test_late_display_receipt_is_separate_bounded_and_linked_after_intro(self) -> None:
        with temporary_directory() as directory:
            root = Path(directory)
            display = CanonicalReceiptPath(root / "late-display.txt")
            inputs = CanonicalReceiptPath(root / "host-input-receipt.txt")
            inputs.write_text("".join(
                f"host-input {ordinal} frame=5031 line=0 action=157 state=1\n"
                for ordinal in range(1, 23)), encoding="ascii")
            display.write_text(
                "late-display-write 1 cycles=100 vpos=54 hpos=104 origin=copper "
                "register=0x0090 value=0x40c1 input_ordinal=21 input_frame=5031\n"
                "late-display-write 2 cycles=110 vpos=55 hpos=108 origin=cpu "
                "register=0x0180 value=0x0000 input_ordinal=22 input_frame=5031\n",
                encoding="ascii")
            status = TOOL.late_display_receipt_status(display, inputs)
            self.assertIn("late_display=present\n", status)
            self.assertIn("late_display_records=2\n", status)
            self.assertIn("late_display_first_input_ordinal=21\n", status)
            self.assertIn("late_display_last_input_ordinal=22\n", status)
            self.assertIn("late_display_input_chronology=linked\n", status)
            display.write_text(
                "late-display-write 1 cycles=100 vpos=54 hpos=104 origin=copper "
                "register=0x0090 value=0x40c1 input_ordinal=20 input_frame=5031\n",
                encoding="ascii")
            with self.assertRaisesRegex(TOOL.CaptureError, "later-input window"):
                TOOL.late_display_receipt_status(display, inputs)
            display.write_text(
                "late-display-write 1 cycles=100 vpos=54 hpos=104 origin=copper "
                "register=0x0090 value=0x40c1 input_ordinal=21 input_frame=1\n",
                encoding="ascii")
            with self.assertRaisesRegex(TOOL.CaptureError, "does not match"):
                TOOL.late_display_receipt_status(display, inputs)

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
