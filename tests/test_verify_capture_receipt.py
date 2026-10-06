"""Negative contracts for external capture-receipt verification."""
from __future__ import annotations
import contextlib
import hashlib
import importlib.util
import io
from pathlib import Path
import sys
import unittest
from unittest import mock
from eon_test_paths import CanonicalReceiptPath, LfTextFixtureWrites, temporary_directory

ROOT = Path(__file__).resolve().parents[1]
SPEC = importlib.util.spec_from_file_location("verify_capture_receipt", ROOT / "tools" / "verify_capture_receipt.py")
assert SPEC and SPEC.loader
TOOL = importlib.util.module_from_spec(SPEC)
SPEC.loader.exec_module(TOOL)


class ReceiptVerifierTests(LfTextFixtureWrites, unittest.TestCase):
    def test_schema31_zero_route_observation_status_is_exact_and_schema_bound(self) -> None:
        runner = TOOL.load_tool("run_deuteros_amiga_capture")
        with temporary_directory() as directory:
            root = Path(directory)
            fields = {"capture_receipt_version": "31"}
            fields.update(dict(line.split("=", 1) for line in
                               runner.zero_route_observation_status(
                                   root / "zero-route-observation.txt",
                                   root / "host-input-receipt.txt").splitlines()))
            TOOL.verify_deuteros_zero_route_observation(fields, root)
            fields["zero_route_observation_records"] = "145"
            with self.assertRaisesRegex(ValueError, "hash/count receipt mismatch"):
                TOOL.verify_deuteros_zero_route_observation(fields, root)
            fields["capture_receipt_version"] = "30"
            with self.assertRaisesRegex(ValueError, "requires receipt schema 31"):
                TOOL.verify_deuteros_zero_route_observation(fields, root)

    def test_schema31_identity_pins_v21_and_rejects_v20(self) -> None:
        runner = TOOL.load_tool("run_deuteros_amiga_capture")
        identity = {
            "recorder_sha256": runner.TRV2_RECORDER_V21_SHA256,
            "recorder_bytes": str(runner.TRV2_RECORDER_V21_SIZE),
            "recorder_protocol": "deuteros-amiga-fsuae-v21",
        }
        self.assertEqual(TOOL.verify_deuteros_recorder_identity(identity, "31", runner), None)
        self.assertEqual(runner.reviewed_recorder_hashes()["reviewed-fs-uae-trv2-v21"],
                         "2fc7f47425d0fa005bb59bf41eaeccf32d1cba284dee4f227e7e723b853e1b35")
        with self.assertRaisesRegex(ValueError, "v21 receipt schema"):
            TOOL.verify_deuteros_recorder_identity({
                **identity, "recorder_protocol": "deuteros-amiga-fsuae-v20",
            }, "31", runner)
        with self.assertRaisesRegex(ValueError, "identity"):
            TOOL.verify_deuteros_recorder_identity({
                **identity, "recorder_sha256": runner.TRV2_RECORDER_V20_SHA256,
                "recorder_bytes": str(runner.TRV2_RECORDER_V20_SIZE),
            }, "31", runner)

    def test_schema32_identity_pins_v22_and_late_display_summary(self) -> None:
        runner = TOOL.load_tool("run_deuteros_amiga_capture")
        identity = {
            "recorder_sha256": runner.TRV2_RECORDER_V22_SHA256,
            "recorder_bytes": str(runner.TRV2_RECORDER_V22_SIZE),
            "recorder_protocol": "deuteros-amiga-fsuae-v22",
        }
        self.assertIsNone(TOOL.verify_deuteros_recorder_identity(identity, "32", runner))
        with self.assertRaisesRegex(ValueError, "v22 receipt schema"):
            TOOL.verify_deuteros_recorder_identity({
                **identity, "recorder_protocol": "deuteros-amiga-fsuae-v21",
            }, "32", runner)
        with temporary_directory() as directory:
            root = Path(directory)
            CanonicalReceiptPath(root / "host-input-receipt.txt").write_text("".join(
                f"host-input {ordinal} frame=5031 line=0 action=157 state=1\n"
                for ordinal in range(1, 22)), encoding="ascii")
            CanonicalReceiptPath(root / "late-display.txt").write_text(
                "late-display-write 1 cycles=100 vpos=54 hpos=104 origin=copper "
                "register=0x0090 value=0x40c1 input_ordinal=21 input_frame=5031\n",
                encoding="ascii")
            fields = {"capture_receipt_version": "32"}
            fields.update(dict(line.split("=", 1) for line in
                runner.late_display_receipt_status(
                    root / "late-display.txt", root / "host-input-receipt.txt").splitlines()))
            TOOL.verify_deuteros_late_display(fields, root)
            fields["late_display_records"] = "2"
            with self.assertRaisesRegex(ValueError, "grammar/count"):
                TOOL.verify_deuteros_late_display(fields, root)
    def test_experimental_driver_load_return_receipt_is_bound_to_sidecar(self) -> None:
        runner = TOOL.load_tool("run_millennium_dos_capture")
        protocol = "millennium-dos-en-driver-load-return-v1"
        with temporary_directory() as directory:
            root = Path(directory)
            sidecar = CanonicalReceiptPath(root / "driver-load-return.raw")
            sidecar.write_text(
                "driver-load-return-v1 ordinal=1 image=mill.com cs=0e70 pc=0315 "
                "ax=0010 bx=0002 cx=0010 dx=0000 si=0000 di=0000 ds=0e70 es=0e70 "
                "ss=0e70 sp=ff00 flags=0000 buffer=ds0:16:" + "a" * 64 + "\n"
                "driver-load-return-end count=1 overflow=0\n", encoding="ascii")
            fields = {"recorder_protocol": protocol}
            fields.update(dict(line.split("=", 1) for line in
                               runner.driver_load_return_status(sidecar, protocol).splitlines()))
            TOOL.verify_millennium_driver_load_returns(fields, root)
            fields["driver_load_return_records"] = "2"
            with self.assertRaisesRegex(ValueError, "hash/count mismatch"):
                TOOL.verify_millennium_driver_load_returns(fields, root)
            fields["recorder_protocol"] = "v21-int93-installation"
            with self.assertRaisesRegex(ValueError, "exact recorder protocol"):
                TOOL.verify_millennium_driver_load_returns(fields, root)

    def test_driver_loader_experiment_verifies_only_with_explicit_experimental_opt_in(self) -> None:
        runner = TOOL.load_tool("run_millennium_dos_capture")
        protocol = "millennium-dos-en-driver-load-return-v1"
        with temporary_directory() as directory:
            root = Path(directory)
            configuration = root / "recorder.conf"
            configuration.write_text("machine=svga_s3\n", encoding="ascii")
            console = root / "recorder-console.log"
            console_payload = b"diagnostic observer stopped\n"
            console.write_bytes(console_payload)
            sidecar = CanonicalReceiptPath(root / "driver-load-return.raw")
            sidecar.write_text(
                "driver-load-return-v1 ordinal=1 image=mill.com cs=0e70 pc=02d4 "
                "ax=0001 bx=0002 cx=0010 dx=0000 si=0000 di=0000 ds=0e70 es=0e70 "
                "ss=0e70 sp=ff00 flags=0000 buffer=none\n"
                "driver-load-return-end count=1 overflow=0\n", encoding="ascii")
            sidecar_status = dict(line.split("=", 1) for line in
                                  runner.driver_load_return_status(sidecar, protocol).splitlines())
            config_digest, config_size = runner.sha256_file(configuration)
            recorder_hash = "57020c1138879a6f53394f592b6f88bcd69875bfc06c98f7cd9b9a64372e8404"
            console_hash = hashlib.sha256(console_payload).hexdigest()
            fields = {
                "capture_receipt_version": "22",
                "recorder_protocol": protocol,
                "recorder_admission": "experimental-observer-not-for-recovery",
                "source_release_sha256": runner.EXPECTED_RELEASE_SHA256,
                "source_release_bytes": str(runner.EXPECTED_RELEASE_SIZE),
                "recorder_sha256": recorder_hash,
                "recorder_bytes": "15809768",
                "events_raw": "not-collected",
                "results_raw": "not-collected",
                "host_input_receipt": "absent",
                "host_input_observed_during_capture": "false",
                "title_input_checkpoint": "not-collected",
                "capture_intent": "diagnostic-no-input",
                "capture_intent_input_requirement": "forbidden",
                "machine_profile": "svga_s3",
                "termination_reason": "emulator-exit",
                "exit_status": "0",
                "configuration_sha256": config_digest,
                "configuration_bytes": str(config_size),
                "recorder_console": "present",
                "recorder_console_sha256": console_hash,
                "recorder_console_total_bytes": str(len(console_payload)),
                "recorder_console_retained_bytes": str(len(console_payload)),
                "recorder_console_retained_sha256": console_hash,
                "recorder_console_truncated": "false",
                "recorder_console_over_limit": "false",
                **sidecar_status,
            }
            status = root / "run-status.txt"
            status.write_text("".join(f"{key}={value}\n" for key, value in fields.items()),
                              encoding="utf-8")
            with self.assertRaisesRegex(ValueError, "not recovery-admissible"):
                TOOL.verify("millennium-dos", root)
            self.assertEqual(TOOL.verify("millennium-dos", root,
                                         allow_experimental_observer=True), "22")
            fields["recorder_sha256"] = (
                "942f30f2199350a51d0ff1e7c024f4e226d29b5576dc6ea0d61b7556c12468e8")
            fields["recorder_bytes"] = "132933552"
            status.write_text("".join(f"{key}={value}\n" for key, value in fields.items()),
                              encoding="utf-8")
            self.assertEqual(TOOL.verify("millennium-dos", root,
                                         allow_experimental_observer=True), "22")
            fields["recorder_bytes"] = "15809768"
            status.write_text("".join(f"{key}={value}\n" for key, value in fields.items()),
                              encoding="utf-8")
            with self.assertRaisesRegex(ValueError, "binary size"):
                TOOL.verify("millennium-dos", root, allow_experimental_observer=True)

    def test_main_rejects_a_runner_capture_error_without_a_traceback(self) -> None:
        output = io.StringIO()
        with (mock.patch.object(sys, "argv", ["verify_capture_receipt.py", "--kind", "deuteros-amiga",
                                               "--capture", "/nonexistent"]),
              mock.patch.object(TOOL, "verify", side_effect=RuntimeError("invalid recorder record")),
              contextlib.redirect_stdout(output)):
            self.assertEqual(TOOL.main(), 2)
        self.assertEqual(output.getvalue(), "CAPTURE RECEIPT REJECTED  invalid recorder record\n")

    def test_receipt_requires_current_schema(self) -> None:
        with self.assertRaisesRegex(ValueError, "receipt schema"):
            TOOL.require_receipt_schema({})
        with self.assertRaisesRegex(ValueError, "receipt schema"):
            TOOL.require_receipt_schema({"capture_receipt_version": "1"})
        self.assertEqual(TOOL.require_receipt_schema({"capture_receipt_version": "2"}), "2")
        self.assertEqual(TOOL.require_receipt_schema({"capture_receipt_version": "3"}), "3")
        self.assertEqual(TOOL.require_receipt_schema({"capture_receipt_version": "4"}), "4")
        self.assertEqual(TOOL.require_receipt_schema({"capture_receipt_version": "6"}), "6")
        self.assertEqual(TOOL.require_receipt_schema({"capture_receipt_version": "7"}), "7")
        self.assertEqual(TOOL.require_receipt_schema({"capture_receipt_version": "8"}), "8")
        self.assertEqual(TOOL.require_receipt_schema({"capture_receipt_version": "9"}), "9")
        self.assertEqual(TOOL.require_receipt_schema({"capture_receipt_version": "10"}), "10")
        self.assertEqual(TOOL.require_receipt_schema({"capture_receipt_version": "11"}), "11")
        self.assertEqual(TOOL.require_receipt_schema({"capture_receipt_version": "12"}), "12")
        self.assertEqual(TOOL.require_receipt_schema({"capture_receipt_version": "13"}), "13")
        self.assertEqual(TOOL.require_receipt_schema({"capture_receipt_version": "20"}), "20")
        self.assertEqual(TOOL.require_receipt_schema({"capture_receipt_version": "21"}), "21")
        self.assertEqual(TOOL.require_receipt_schema({"capture_receipt_version": "22"}), "22")
        self.assertEqual(TOOL.require_receipt_schema({"capture_receipt_version": "23"}), "23")
        self.assertEqual(TOOL.require_receipt_schema({"capture_receipt_version": "26"}), "26")
        self.assertEqual(TOOL.require_receipt_schema({"capture_receipt_version": "29"}), "29")

    def test_v23_deuteros_source_contract_binds_standalone_pair_without_outer_zip(self) -> None:
        runner = TOOL.load_tool("run_deuteros_amiga_capture")
        fields = {
            "content_release_sha256": runner.EXPECTED_RELEASE_SHA256,
            "source_layout": runner.SOURCE_LAYOUT_STANDALONE,
            "source_container": "two-independent-zip-files",
            "disk1_archive_sha256": runner.EXPECTED_DISK1_ARCHIVE_SHA256,
            "disk1_archive_bytes": str(runner.EXPECTED_DISK1_ARCHIVE_SIZE),
            "disk2_archive_sha256": runner.EXPECTED_DISK2_ARCHIVE_SHA256,
            "disk2_archive_bytes": str(runner.EXPECTED_DISK2_ARCHIVE_SIZE),
        }
        TOOL.verify_deuteros_source_contract(fields, "23", runner)
        tampered = dict(fields, disk1_archive_sha256=runner.EXPECTED_DISK2_ARCHIVE_SHA256,
                        disk1_archive_bytes=str(runner.EXPECTED_DISK2_ARCHIVE_SIZE))
        with self.assertRaisesRegex(ValueError, "disk1_archive identity"):
            TOOL.verify_deuteros_source_contract(tampered, "23", runner)
        tampered = dict(fields, source_release_sha256=runner.EXPECTED_RELEASE_SHA256,
                        source_release_bytes=str(runner.EXPECTED_RELEASE_SIZE))
        with self.assertRaisesRegex(ValueError, "must not claim"):
            TOOL.verify_deuteros_source_contract(tampered, "23", runner)
        tampered = dict(fields, content_release_sha256="0" * 64)
        with self.assertRaisesRegex(ValueError, "content-release"):
            TOOL.verify_deuteros_source_contract(tampered, "23", runner)

    def test_v23_deuteros_receipt_runs_v11_semantic_gates(self) -> None:
        runner = TOOL.load_tool("run_deuteros_amiga_capture")
        with temporary_directory() as directory:
            root = Path(directory)
            config = root / "deuteros-amiga-capture.fs-uae"
            config.write_text("warp_mode = 0\n", encoding="utf-8")
            console = b"visible emulator output\n"
            (root / "recorder-console.log").write_bytes(console)
            config_hash, config_size = hashlib.sha256(config.read_bytes()).hexdigest(), config.stat().st_size
            console_hash = hashlib.sha256(console).hexdigest()
            fields = {
                "capture_receipt_version": "23",
                "source_layout": runner.SOURCE_LAYOUT_STANDALONE,
                "source_container": "two-independent-zip-files",
                "content_release_sha256": runner.EXPECTED_RELEASE_SHA256,
                "disk1_archive_sha256": runner.EXPECTED_DISK1_ARCHIVE_SHA256,
                "disk1_archive_bytes": str(runner.EXPECTED_DISK1_ARCHIVE_SIZE),
                "disk2_archive_sha256": runner.EXPECTED_DISK2_ARCHIVE_SHA256,
                "disk2_archive_bytes": str(runner.EXPECTED_DISK2_ARCHIVE_SIZE),
                "kickstart_archive_sha256": runner.EXPECTED_KICKSTART_SHA256,
                "kickstart_archive_bytes": str(runner.EXPECTED_KICKSTART_SIZE),
                "recorder_sha256": runner.EXPECTED_RECORDER_SHA256,
                "recorder_bytes": "17",
                "configuration_sha256": config_hash,
                "configuration_bytes": str(config_size),
                "timing_profile": "realtime",
                "raw_pc": "absent",
                "host_input_receipt": "absent",
                "title_display": "absent",
                "capture_intent": "diagnostic-no-input",
                "capture_intent_input_requirement": "forbidden",
                "host_input_observed_during_capture": "false",
                "recorder_console": "present",
                "recorder_console_sha256": console_hash,
                "recorder_console_total_bytes": str(len(console)),
                "recorder_console_retained_bytes": str(len(console)),
                "recorder_console_retained_sha256": console_hash,
                "recorder_console_over_limit": "false",
                "recorder_console_truncated": "false",
            }
            status = root / "run-status.txt"
            status.write_text("".join(f"{key}={value}\n" for key, value in fields.items()), encoding="utf-8")
            with mock.patch.object(TOOL, "verify_deuteros_title_display",
                                  wraps=TOOL.verify_deuteros_title_display) as display_gate:
                TOOL.verify("deuteros-amiga", root)
            display_gate.assert_called_once_with(fields, root)

            pre_v16_hashes = [digest for digest in runner.reviewed_recorder_hashes().values()
                              if digest != runner.TRV2_RECORDER_V16_SHA256]
            for reviewed_hash in pre_v16_hashes:
                fields["recorder_sha256"] = reviewed_hash
                status.write_text("".join(f"{key}={value}\n" for key, value in fields.items()), encoding="utf-8")
                TOOL.verify("deuteros-amiga", root)
            fields["recorder_sha256"] = runner.TRV2_RECORDER_V16_SHA256
            status.write_text("".join(f"{key}={value}\n" for key, value in fields.items()), encoding="utf-8")
            with self.assertRaisesRegex(ValueError, "requires receipt schema 24"):
                TOOL.verify("deuteros-amiga", root)
            fields["recorder_sha256"] = "0" * 64
            status.write_text("".join(f"{key}={value}\n" for key, value in fields.items()), encoding="utf-8")
            with self.assertRaisesRegex(ValueError, "recorder identity"):
                TOOL.verify("deuteros-amiga", root)
            fields["recorder_sha256"] = runner.EXPECTED_RECORDER_SHA256

            fields["capture_intent_input_requirement"] = "required"
            status.write_text("".join(f"{key}={value}\n" for key, value in fields.items()), encoding="utf-8")
            with self.assertRaisesRegex(ValueError, "capture intent"):
                TOOL.verify("deuteros-amiga", root)

            fields["capture_intent_input_requirement"] = "forbidden"
            fields["recorder_console_over_limit"] = "true"
            status.write_text("".join(f"{key}={value}\n" for key, value in fields.items()), encoding="utf-8")
            with self.assertRaisesRegex(ValueError, "safety cap"):
                TOOL.verify("deuteros-amiga", root)

    def test_v23_deuteros_raw_and_timing_gates_keep_the_v11_contract(self) -> None:
        runner = TOOL.load_tool("run_deuteros_amiga_capture")
        with temporary_directory() as directory:
            root = Path(directory)
            raw = root / "raw-pc.txt"
            raw.write_text("legacy raw record\n", encoding="ascii")
            with self.assertRaisesRegex(ValueError, "format does not match"):
                TOOL.verify_deuteros_raw_pc_summary({"raw_pc": "present", "raw_pc_format": "legacy"},
                                                     root, "23")
            (root / "deuteros-amiga-capture.fs-uae").write_text("warp_mode = 1\n", encoding="utf-8")
            with self.assertRaisesRegex(ValueError, "does not match"):
                TOOL.verify_deuteros_timing_profile({"timing_profile": "realtime"}, root)
            with self.assertRaisesRegex(ValueError, "safety cap"):
                TOOL.verify_console_admission({"recorder_console_over_limit": "true"}, "23")

    def test_capture_intent_rejects_a_receipt_that_disagrees_with_its_declared_session(self) -> None:
        millennium = TOOL.load_tool("run_millennium_dos_capture")
        fields = {
            "capture_intent": "physical-input", "capture_intent_input_requirement": "required",
            "host_input_receipt": "present", "host_input_observed_during_capture": "true",
        }
        TOOL.verify_capture_intent(fields, millennium)
        fields["host_input_observed_during_capture"] = "false"
        with self.assertRaisesRegex(ValueError, "does not match"):
            TOOL.verify_capture_intent(fields, millennium)

    def test_receipt_rejects_duplicate_or_malformed_fields(self) -> None:
        with temporary_directory() as directory:
            path = Path(directory) / "run-status.txt"
            path.write_text("a=b\na=c\n", encoding="utf-8")
            with self.assertRaisesRegex(ValueError, "duplicate"):
                TOOL.receipt(path)
            path.write_text("not-a-field\n", encoding="utf-8")
            with self.assertRaisesRegex(ValueError, "invalid"):
                TOOL.receipt(path)

    def test_receipt_allows_only_empty_no_input_phase_summary(self) -> None:
        with temporary_directory() as directory:
            path = Path(directory) / "run-status.txt"
            path.write_text(
                "raw_pc_pre_input_site_counts=0x00021822:128\n"
                "raw_pc_post_input_site_counts=\n", encoding="utf-8")
            self.assertEqual(
                TOOL.receipt(path)["raw_pc_post_input_site_counts"], "")
            path.write_text("recorder_sha256=\n", encoding="utf-8")
            with self.assertRaisesRegex(ValueError, "empty"):
                TOOL.receipt(path)

    def test_optional_artifact_is_hash_bound_and_rejects_symlink(self) -> None:
        with temporary_directory() as directory:
            root = Path(directory)
            artifact = root / "raw-pc.txt"
            payload = b"raw observation\n"
            artifact.write_bytes(payload)
            fields = {"raw_pc": "present", "raw_pc_sha256": hashlib.sha256(payload).hexdigest(),
                      "raw_pc_bytes": str(len(payload))}
            TOOL.verify_file(fields, root, "raw_pc", "raw-pc.txt")
            fields["raw_pc_sha256"] = "0" * 64
            with self.assertRaisesRegex(ValueError, "mismatch"):
                TOOL.verify_file(fields, root, "raw_pc", "raw-pc.txt")
            artifact.unlink()
            artifact.symlink_to("missing")
            with self.assertRaisesRegex(ValueError, "unsafe"):
                TOOL.verify_file({"raw_pc": "present", "raw_pc_sha256": "0" * 64,
                                  "raw_pc_bytes": "0"}, root, "raw_pc", "raw-pc.txt")

    def test_v21_int93_installation_summary_rejects_mismatched_target(self) -> None:
        with temporary_directory() as directory:
            root = Path(directory)
            runner = TOOL.load_tool("run_millennium_dos_capture")
            sidecar = root / "int93-installation.raw"
            sidecar.write_bytes((
                "int93-installation-v1 image=2200ad.exe pc=0x4175 vector=0x93 "
                "ds=0x4567 dx=0x89ab target_preimage=0xdeadbeef "
                "vector_ip=0x89ab vector_cs=0x4567\n").encode("ascii"))
            fields = dict(line.split("=", 1) for line in runner.int93_installation_status(
                sidecar, "v21-int93-installation").splitlines())
            TOOL.verify_millennium_int93_installation(fields, root)
            fields["int93_installation_target_preimage"] = "0x00000000"
            with self.assertRaisesRegex(ValueError, "installation receipt mismatch"):
                TOOL.verify_millennium_int93_installation(fields, root)

    def test_v3_deuteros_raw_summary_is_recomputed_from_strict_records(self) -> None:
        with temporary_directory() as directory:
            root = Path(directory)
            raw = root / "raw-pc.txt"
            raw.write_text(
                "raw-pc 1 cycles=1 pc=0x000210d4 opcode=0x4e75 d0=0x00000000 "
                "a0=0x00000000 a6=0x00000000 sr=0x0000\n", encoding="ascii")
            fields = {"raw_pc": "present", "raw_pc_records": "1",
                      "raw_pc_site_counts": "0x000210d4:1"}
            TOOL.verify_deuteros_raw_pc_summary(fields, root, "3")
            fields["raw_pc_site_counts"] = "0x000210d4:2"
            with self.assertRaisesRegex(ValueError, "grammar/count"):
                TOOL.verify_deuteros_raw_pc_summary(fields, root, "3")

    def test_v7_deuteros_raw_summary_requires_separate_ir_and_memory_words(self) -> None:
        with temporary_directory() as directory:
            root = Path(directory)
            raw = root / "raw-pc.txt"
            raw.write_text(
                "raw-pc 1 cycles=1 pc=0x000210d4 ir_opcode=0x4e75 memory_opcode=0x4e75 "
                "d0=0x00000000 a0=0x00000000 a6=0x00000000 sr=0x0000\n", encoding="ascii")
            fields = {"raw_pc": "present", "raw_pc_format": "v7", "raw_pc_records": "1",
                      "raw_pc_site_counts": "0x000210d4:1"}
            TOOL.verify_deuteros_raw_pc_summary(fields, root, "7")
            fields["raw_pc_format"] = "legacy"
            with self.assertRaisesRegex(ValueError, "format"):
                TOOL.verify_deuteros_raw_pc_summary(fields, root, "7")

    def test_v8_deuteros_raw_summary_recomputes_opaque_opcode_pairs(self) -> None:
        with temporary_directory() as directory:
            root = Path(directory)
            raw = root / "raw-pc.txt"
            raw.write_text(
                "raw-pc 1 cycles=1 pc=0x0001fe84 ir_opcode=0x7202 memory_opcode=0x7202 "
                "d0=0x00000000 a0=0x00000000 a6=0x00000000 sr=0x0000\n"
                "raw-pc 2 cycles=2 pc=0x0001fe84 ir_opcode=0x7203 memory_opcode=0x7202 "
                "d0=0x00000000 a0=0x00000000 a6=0x00000000 sr=0x0000\n", encoding="ascii")
            fields = {"raw_pc_opcode_pairs": "0x0001fe84:7202/7202+7203/7202"}
            TOOL.verify_deuteros_raw_pc_opcode_pairs(fields, root)
            fields["raw_pc_opcode_pairs"] = "0x0001fe84:7202/7202"
            with self.assertRaisesRegex(ValueError, "opcode-pair"):
                TOOL.verify_deuteros_raw_pc_opcode_pairs(fields, root)

    def test_v9_deuteros_opcode_pairs_use_the_v9_grammar(self) -> None:
        with temporary_directory() as directory:
            root = Path(directory)
            (root / "raw-pc.txt").write_text(
                "raw-pc 1 cycles=1 pc=0x0001fe84 ir_opcode=0x7202 memory_opcode=0x7202 "
                "d0=0x00000000 a0=0x00000000 a6=0x00000000 sr=0x0000 "
                "input_ordinal=0 input_frame=0\n", encoding="ascii")
            fields = {"raw_pc_opcode_pairs": "0x0001fe84:7202/7202"}
            TOOL.verify_deuteros_raw_pc_opcode_pairs(fields, root, "v9")

    def test_v9_deuteros_input_chronology_requires_exact_prior_delivery(self) -> None:
        with temporary_directory() as directory:
            root = Path(directory)
            (root / "host-input-receipt.txt").write_text(
                "host-input 1 frame=2 line=3 action=4 state=1\n", encoding="ascii")
            (root / "raw-pc.txt").write_text(
                "raw-pc 1 cycles=1 pc=0x0001fe84 ir_opcode=0x7202 memory_opcode=0x7202 "
                "d0=0x00000000 a0=0x00000000 a6=0x00000000 sr=0x0000 input_ordinal=0 input_frame=0\n"
                "raw-pc 2 cycles=2 pc=0x0001fe84 ir_opcode=0x7202 memory_opcode=0x7202 "
                "d0=0x00000000 a0=0x00000000 a6=0x00000000 sr=0x0000 input_ordinal=1 input_frame=2\n",
                encoding="ascii")
            fields = {"raw_pc": "present", "raw_pc_format": "v9", "raw_pc_records": "2",
                      "raw_pc_site_counts": "0x0001fe84:2", "raw_pc_input_links": "1",
                      "raw_pc_last_input_ordinal": "1", "raw_pc_input_chronology": "linked",
                      "raw_pc_input_chronology_records": "1"}
            TOOL.verify_deuteros_raw_pc_summary(fields, root, "9")
            TOOL.verify_deuteros_raw_pc_input_chronology(fields, root)
            fields["raw_pc_input_chronology_records"] = "2"
            with self.assertRaisesRegex(ValueError, "chronology receipt"):
                TOOL.verify_deuteros_raw_pc_input_chronology(fields, root)

    def test_v24_deuteros_uses_v16_only_site_set_and_exact_recorder_identity(self) -> None:
        runner = TOOL.load_tool("run_deuteros_amiga_capture")
        with temporary_directory() as directory:
            root = Path(directory)
            (root / "raw-pc.txt").write_text(
                "raw-pc 1 cycles=1 pc=0x000218cc ir_opcode=0x4e75 memory_opcode=0x4e75 "
                "d0=0x00000000 a0=0x00000000 a6=0x00000000 sr=0x0000 "
                "input_ordinal=1 input_frame=2\n", encoding="ascii")
            (root / "host-input-receipt.txt").write_text(
                "host-input 1 frame=2 line=3 action=4 state=1\n", encoding="ascii")
            fields = {"raw_pc": "present", "raw_pc_format": "v9-v16",
                      "raw_pc_records": "1", "raw_pc_site_counts": "0x000218cc:1",
                      "raw_pc_input_links": "1", "raw_pc_last_input_ordinal": "1",
                      "raw_pc_input_chronology": "linked",
                      "raw_pc_input_chronology_records": "1",
                      "recorder_sha256": runner.TRV2_RECORDER_V16_SHA256,
                      "recorder_bytes": str(runner.TRV2_RECORDER_V16_SIZE),
                      "recorder_protocol": "deuteros-amiga-fsuae-v16"}
            TOOL.verify_deuteros_raw_pc_summary(fields, root, "24")
            TOOL.verify_deuteros_raw_pc_input_chronology(fields, root, "v9-v16")
            (root / "host-input-receipt.txt").write_text(
                "host-input 1 frame=3 line=3 action=4 state=1\n", encoding="ascii")
            with self.assertRaisesRegex(ValueError, "input chronology is invalid"):
                TOOL.verify_deuteros_raw_pc_input_chronology(fields, root, "v9-v16")
            (root / "host-input-receipt.txt").write_text(
                "host-input 1 frame=2 line=3 action=4 state=1\n", encoding="ascii")
            fields["raw_pc_format"] = "v9"
            with self.assertRaisesRegex(RuntimeError, "unreviewed probe site"):
                TOOL.verify_deuteros_raw_pc_summary(fields, root, "23")
            fields["raw_pc_format"] = "v9-v16"
            fields["recorder_sha256"] = runner.TRV2_RECORDER_V15_SHA256
            with self.assertRaisesRegex(ValueError, "identity"):
                TOOL.verify_deuteros_raw_pc_summary(fields, root, "24")

    def test_v26_v16_raw_pc_cap_is_128_per_phase_and_256_per_site(self) -> None:
        runner = TOOL.load_tool("run_deuteros_amiga_capture")
        with temporary_directory() as directory:
            root = Path(directory)
            raw = root / "raw-pc.txt"
            def row(ordinal: int, cycle: int, input_ordinal: int, frame: int) -> str:
                return (f"raw-pc {ordinal} cycles={cycle} pc=0x000218cc ir_opcode=0x4e75 "
                        f"memory_opcode=0x4e75 d0=0x00000000 a0=0x00000000 a6=0x00000000 "
                        f"sr=0x0000 input_ordinal={input_ordinal} input_frame={frame}\n")
            payload = "".join(row(i, i, 0, 0) for i in range(1, 129))
            payload += "".join(row(i, i, 1, 10) for i in range(129, 257))
            raw.write_text(payload, encoding="ascii")
            fields = {
                "raw_pc": "present", "raw_pc_format": "v9-v16-phased",
                "raw_pc_records": "256", "raw_pc_site_counts": "0x000218cc:256",
                "raw_pc_pre_input_site_counts": "0x000218cc:128",
                "raw_pc_post_input_site_counts": "0x000218cc:128",
                "recorder_sha256": runner.TRV2_RECORDER_V16_SHA256,
                "recorder_bytes": str(runner.TRV2_RECORDER_V16_SIZE),
                "recorder_protocol": "deuteros-amiga-fsuae-v16",
            }
            TOOL.verify_deuteros_raw_pc_summary(fields, root, "26")
            counts = runner.parse_raw_pc_observations(raw, "v9-v16-phased")
            self.assertEqual(counts, {0x000218CC: 256})
            with self.assertRaisesRegex(runner.CaptureError, "per-site recorder cap"):
                runner.parse_raw_pc_observations(raw, "v9-v16")

            raw.write_text(payload + row(257, 257, 1, 10), encoding="ascii")
            with self.assertRaisesRegex(runner.CaptureError, "per-site phase cap"):
                runner.parse_raw_pc_observations(raw, "v9-v16-phased")
            raw.write_text("".join(row(i, i, 0, 0) for i in range(1, 130)), encoding="ascii")
            with self.assertRaisesRegex(runner.CaptureError, "per-site phase cap"):
                runner.parse_raw_pc_observations(raw, "v9-v16-phased")

    def test_v27_binds_selector_cells_to_raw_sample_and_host_delivery(self) -> None:
        runner = TOOL.load_tool("run_deuteros_amiga_capture")
        with temporary_directory() as directory:
            root = Path(directory)
            (root / "raw-pc.txt").write_text(
                "raw-pc 1 cycles=10 pc=0x0001fbe6 ir_opcode=0x4a39 memory_opcode=0x4a39 "
                "d0=0x00000000 a0=0x00000000 a6=0x00000000 sr=0x0000 "
                "input_ordinal=1 input_frame=2\n", encoding="ascii")
            (root / "host-input-receipt.txt").write_text(
                "host-input 1 frame=2 line=3 action=149 state=1\n", encoding="ascii")
            (root / "selector-dispatch.txt").write_text(
                "selector-dispatch 1 raw_ordinal=1 cycles=10 pc=0x0001fbe6 "
                "cell_1f98c=0x00 cell_1f98e=0x01 input_ordinal=1 input_frame=2\n",
                encoding="ascii")
            fields = {"raw_pc": "present", "raw_pc_format": "v9-v16-phased",
                      "raw_pc_records": "1", "raw_pc_site_counts": "0x0001fbe6:1",
                      "raw_pc_pre_input_site_counts": "",
                      "raw_pc_post_input_site_counts": "0x0001fbe6:1",
                      "capture_receipt_version": "27",
                      "recorder_sha256": runner.TRV2_RECORDER_V17_SHA256,
                      "recorder_bytes": str(runner.TRV2_RECORDER_V17_SIZE),
                      "recorder_protocol": "deuteros-amiga-fsuae-v17"}
            TOOL.verify_deuteros_raw_pc_summary(fields, root, "27")
            selector_fields = fields | {
                "selector_dispatch": "present",
                **dict(line.split("=", 1) for line in runner.selector_dispatch_status(
                    root / "selector-dispatch.txt", root / "raw-pc.txt",
                    root / "host-input-receipt.txt").splitlines())}
            TOOL.verify_deuteros_selector_dispatch(selector_fields, root)
            (root / "selector-dispatch.txt").write_text(
                "selector-dispatch 1 raw_ordinal=1 cycles=11 pc=0x0001fbe6 "
                "cell_1f98c=0x00 cell_1f98e=0x01 input_ordinal=1 input_frame=2\n",
                encoding="ascii")
            with self.assertRaisesRegex(ValueError, "selector-dispatch receipt"):
                TOOL.verify_deuteros_selector_dispatch(selector_fields, root)

    def test_v27_source_contract_keeps_standalone_archive_provenance(self) -> None:
        runner = TOOL.load_tool("run_deuteros_amiga_capture")
        fields = {
            "content_release_sha256": runner.EXPECTED_RELEASE_SHA256,
            "source_layout": runner.SOURCE_LAYOUT_STANDALONE,
            "source_container": "two-independent-zip-files",
            "disk1_archive_sha256": runner.EXPECTED_DISK1_ARCHIVE_SHA256,
            "disk1_archive_bytes": str(runner.EXPECTED_DISK1_ARCHIVE_SIZE),
            "disk2_archive_sha256": runner.EXPECTED_DISK2_ARCHIVE_SHA256,
            "disk2_archive_bytes": str(runner.EXPECTED_DISK2_ARCHIVE_SIZE),
        }
        TOOL.verify_deuteros_source_contract(fields, "27", runner)

    def test_v28_binds_btst_branch_opcode_chronology_and_v18_recorder(self) -> None:
        runner = TOOL.load_tool("run_deuteros_amiga_capture")
        with temporary_directory() as directory:
            root = Path(directory)
            (root / "raw-pc.txt").write_text(
                "raw-pc 1 cycles=9 pc=0x0002185e ir_opcode=0x0839 memory_opcode=0x0839 "
                "d0=0x00000000 a0=0x00000000 a6=0x00000000 sr=0x0004 "
                "input_ordinal=1 input_frame=2\n"
                "raw-pc 2 cycles=10 pc=0x00021866 ir_opcode=0x6608 memory_opcode=0x6608 "
                "d0=0x00000000 a0=0x00000000 a6=0x00000000 sr=0x0004 "
                "input_ordinal=1 input_frame=2\n", encoding="ascii")
            (root / "host-input-receipt.txt").write_text(
                "host-input 1 frame=2 line=3 action=149 state=1\n", encoding="ascii")
            fields = {
                "capture_receipt_version": "28",
                "raw_pc": "present", "raw_pc_format": "v9-v18-phased",
                "raw_pc_records": "2", "raw_pc_site_counts": "0x0002185e:1,0x00021866:1",
                "raw_pc_pre_input_site_counts": "",
                "raw_pc_post_input_site_counts": "0x0002185e:1,0x00021866:1",
                "raw_pc_opcode_pairs": "0x0002185e:0839/0839,0x00021866:6608/6608",
                "raw_pc_input_links": "2", "raw_pc_last_input_ordinal": "1",
                "raw_pc_input_chronology": "linked",
                "raw_pc_input_chronology_records": "2",
                "recorder_sha256": runner.TRV2_RECORDER_V18_SHA256,
                "recorder_bytes": str(runner.TRV2_RECORDER_V18_SIZE),
                "recorder_protocol": "deuteros-amiga-fsuae-v18",
            }
            TOOL.verify_deuteros_recorder_identity(fields, "28", runner)
            TOOL.verify_deuteros_raw_pc_summary(fields, root, "28")
            TOOL.verify_deuteros_raw_pc_opcode_pairs(fields, root, "v9-v18-phased")
            TOOL.verify_deuteros_raw_pc_input_chronology(fields, root, "v9-v18-phased")
            selector_fields = fields | {"selector_dispatch": "absent"}
            TOOL.verify_deuteros_selector_dispatch(selector_fields, root)
            fields["recorder_protocol"] = "deuteros-amiga-fsuae-v17"
            with self.assertRaisesRegex(ValueError, "v18 receipt schema"):
                TOOL.verify_deuteros_recorder_identity(fields, "28", runner)

    def test_v28_source_contract_keeps_standalone_archive_provenance(self) -> None:
        runner = TOOL.load_tool("run_deuteros_amiga_capture")
        fields = {
            "content_release_sha256": runner.EXPECTED_RELEASE_SHA256,
            "source_layout": runner.SOURCE_LAYOUT_STANDALONE,
            "source_container": "two-independent-zip-files",
            "disk1_archive_sha256": runner.EXPECTED_DISK1_ARCHIVE_SHA256,
            "disk1_archive_bytes": str(runner.EXPECTED_DISK1_ARCHIVE_SIZE),
            "disk2_archive_sha256": runner.EXPECTED_DISK2_ARCHIVE_SHA256,
            "disk2_archive_bytes": str(runner.EXPECTED_DISK2_ARCHIVE_SIZE),
        }
        TOOL.verify_deuteros_source_contract(fields, "28", runner)

    def test_v29_binds_late_input_window_and_selector_to_host_receipts(self) -> None:
        runner = TOOL.load_tool("run_deuteros_amiga_capture")
        with temporary_directory() as directory:
            root = Path(directory)
            (root / "host-input-receipt.txt").write_text("".join(
                f"host-input {i} frame={i * 10} line=3 action=149 state=1\n"
                for i in range(1, 10)), encoding="ascii")
            (root / "late-input-pc.txt").write_text(
                "late-pc 1 cycles=90 pc=0x0001fbe6 ir_opcode=0x4a39 memory_opcode=0x4a39 "
                "d0=0x00000000 a0=0x00000000 a6=0x00000000 sr=0x0000 "
                "input_ordinal=9 input_frame=90\n", encoding="ascii")
            (root / "late-selector-dispatch.txt").write_text(
                "late-selector-dispatch 1 late_raw_ordinal=1 cycles=90 pc=0x0001fbe6 "
                "cell_1f98c=0x00 cell_1f98e=0x01 input_ordinal=9 input_frame=90\n",
                encoding="ascii")
            fields = {"capture_receipt_version": "29"}
            statuses = (runner.late_raw_pc_status(
                root / "late-input-pc.txt", root / "host-input-receipt.txt")
                + runner.late_selector_dispatch_status(
                    root / "late-selector-dispatch.txt", root / "late-input-pc.txt",
                    root / "host-input-receipt.txt"))
            fields.update(dict(line.split("=", 1) for line in statuses.splitlines()))
            TOOL.verify_deuteros_late_sidecars(fields, root)
            TOOL.verify_deuteros_recorder_identity({
                "recorder_sha256": runner.TRV2_RECORDER_V19_SHA256,
                "recorder_bytes": str(runner.TRV2_RECORDER_V19_SIZE),
                "recorder_protocol": "deuteros-amiga-fsuae-v19",
            }, "29", runner)
            wrong = dict(fields, late_input_pc_last_input_ordinal="8")
            with self.assertRaisesRegex(ValueError, "grammar/count receipt"):
                TOOL.verify_deuteros_late_sidecars(wrong, root)
            bad_identity = {
                "recorder_sha256": runner.TRV2_RECORDER_V19_SHA256,
                "recorder_bytes": str(runner.TRV2_RECORDER_V19_SIZE),
                "recorder_protocol": "deuteros-amiga-fsuae-v18",
            }
            with self.assertRaisesRegex(ValueError, "v19 receipt schema"):
                TOOL.verify_deuteros_recorder_identity(bad_identity, "29", runner)

    def test_v30_binds_v20_identity_and_two_late_only_sites(self) -> None:
        runner = TOOL.load_tool("run_deuteros_amiga_capture")
        self.assertEqual(TOOL.require_receipt_schema({"capture_receipt_version": "30"}), "30")
        with temporary_directory() as directory:
            root = Path(directory)
            host = root / "host-input-receipt.txt"
            host.write_text("".join(
                f"host-input {i} frame={i * 10} line=3 action=149 state=1\n"
                for i in range(1, 10)), encoding="ascii")
            late = root / "late-input-pc.txt"
            late.write_text(
                "late-pc 1 cycles=90 pc=0x0001fc22 ir_opcode=0x4e75 memory_opcode=0x4e75 "
                "d0=0x00000000 a0=0x00000000 a6=0x00000000 sr=0x0000 "
                "input_ordinal=9 input_frame=90\n", encoding="ascii")
            fields = {"capture_receipt_version": "30"}
            statuses = (runner.late_raw_pc_status(late, host, "v20")
                        + runner.late_selector_dispatch_status(
                            root / "late-selector-dispatch.txt", late, host, "v20"))
            fields.update(dict(line.split("=", 1) for line in statuses.splitlines()))
            TOOL.verify_deuteros_late_sidecars(fields, root)
            v20_identity = {
                "recorder_sha256": runner.TRV2_RECORDER_V20_SHA256,
                "recorder_bytes": str(runner.TRV2_RECORDER_V20_SIZE),
                "recorder_protocol": "deuteros-amiga-fsuae-v20",
            }
            TOOL.verify_deuteros_recorder_identity(v20_identity, "30", runner)
            with self.assertRaisesRegex(ValueError, "unreviewed probe site"):
                TOOL.verify_deuteros_late_sidecars(
                    {**fields, "capture_receipt_version": "29"}, root)
            with self.assertRaisesRegex(ValueError, "v20 receipt schema"):
                TOOL.verify_deuteros_recorder_identity(
                    {**v20_identity, "recorder_protocol": "deuteros-amiga-fsuae-v19"},
                    "30", runner)
            with self.assertRaisesRegex(ValueError, "identity"):
                TOOL.verify_deuteros_recorder_identity(
                    {**v20_identity, "recorder_sha256": runner.TRV2_RECORDER_V19_SHA256},
                    "30", runner)

    def test_v29_full_capture_receipt_recomputes_all_observation_hashes(self) -> None:
        runner = TOOL.load_tool("run_deuteros_amiga_capture")
        with temporary_directory() as directory:
            root = Path(directory)
            config = root / "deuteros-amiga-capture.fs-uae"
            config.write_text("warp_mode = 0\n", encoding="ascii")
            raw_payload = (
                "raw-pc 1 cycles=10 pc=0x000210d4 ir_opcode=0x4e75 memory_opcode=0x4e75 "
                "d0=0x00000000 a0=0x00000000 a6=0x00000000 sr=0x0000 "
                "input_ordinal=0 input_frame=0\n"
                "raw-pc 2 cycles=20 pc=0x0001fbe6 ir_opcode=0x4a39 memory_opcode=0x4a39 "
                "d0=0x00000000 a0=0x00000000 a6=0x00000000 sr=0x0000 "
                "input_ordinal=9 input_frame=90\n").encode("ascii")
            host_payload = "".join(
                f"host-input {i} frame={i * 10} line=3 action=149 state=1\n"
                for i in range(1, 10)).encode("ascii")
            late_payload = (
                "late-pc 1 cycles=20 pc=0x0001fbe6 ir_opcode=0x4a39 memory_opcode=0x4a39 "
                "d0=0x00000000 a0=0x00000000 a6=0x00000000 sr=0x0000 "
                "input_ordinal=9 input_frame=90\n").encode("ascii")
            late_selector_payload = (
                "late-selector-dispatch 1 late_raw_ordinal=1 cycles=20 pc=0x0001fbe6 "
                "cell_1f98c=0x00 cell_1f98e=0x01 input_ordinal=9 input_frame=90\n").encode("ascii")
            console = b"visible fs-uae session\n"
            (root / "raw-pc.txt").write_bytes(raw_payload)
            (root / "host-input-receipt.txt").write_bytes(host_payload)
            (root / "late-input-pc.txt").write_bytes(late_payload)
            (root / "late-selector-dispatch.txt").write_bytes(late_selector_payload)
            (root / "recorder-console.log").write_bytes(console)
            for filename in ("release-outer-ro", "disk1-ro", "disk2-ro", "kickstart-ro"):
                (root / filename).mkdir()

            def file_status(key: str, payload: bytes) -> dict[str, str]:
                return {key: "present", key + "_sha256": hashlib.sha256(payload).hexdigest(),
                        key + "_bytes": str(len(payload))}

            fields = {
                "capture_receipt_version": "29",
                "recorder_protocol": "deuteros-amiga-fsuae-v19",
                "source_layout": runner.SOURCE_LAYOUT_STANDALONE,
                "source_container": "two-independent-zip-files",
                "content_release_sha256": runner.EXPECTED_RELEASE_SHA256,
                "timing_profile": "realtime",
                "capture_intent": "physical-input",
                "capture_intent_input_requirement": "required",
                "host_input_observed_during_capture": "true",
                "exit_status": "124", "start_unix": "1", "end_unix": "2",
                "recorder_sha256": runner.TRV2_RECORDER_V19_SHA256,
                "recorder_bytes": str(runner.TRV2_RECORDER_V19_SIZE),
                "kickstart_archive_sha256": runner.EXPECTED_KICKSTART_SHA256,
                "kickstart_archive_bytes": str(runner.EXPECTED_KICKSTART_SIZE),
                "disk1_archive_sha256": runner.EXPECTED_DISK1_ARCHIVE_SHA256,
                "disk1_archive_bytes": str(runner.EXPECTED_DISK1_ARCHIVE_SIZE),
                "disk2_archive_sha256": runner.EXPECTED_DISK2_ARCHIVE_SHA256,
                "disk2_archive_bytes": str(runner.EXPECTED_DISK2_ARCHIVE_SIZE),
                "raw_pc_format": "v9-v19-phased", "raw_pc_records": "2",
                "raw_pc_site_counts": "0x000210d4:1,0x0001fbe6:1",
                "raw_pc_pre_input_site_counts": "0x000210d4:1",
                "raw_pc_post_input_site_counts": "0x0001fbe6:1",
                "raw_pc_opcode_pairs": "0x000210d4:4e75/4e75,0x0001fbe6:4a39/4a39",
                "raw_pc_input_links": "1", "raw_pc_last_input_ordinal": "9",
                "raw_pc_input_chronology": "linked", "raw_pc_input_chronology_records": "1",
                "host_input_receipt_records": "9",
                "recorder_console": "present",
                "recorder_console_sha256": hashlib.sha256(console).hexdigest(),
                "recorder_console_total_bytes": str(len(console)),
                "recorder_console_retained_bytes": str(len(console)),
                "recorder_console_retained_sha256": hashlib.sha256(console).hexdigest(),
                "recorder_console_truncated": "false", "recorder_console_over_limit": "false",
                **file_status("raw_pc", raw_payload),
                **file_status("host_input_receipt", host_payload),
                "title_display": "absent",
            }
            fields.update(dict(line.split("=", 1) for line in
                               runner.late_raw_pc_status(root / "late-input-pc.txt",
                                                         root / "host-input-receipt.txt").splitlines()))
            fields.update(dict(line.split("=", 1) for line in
                               runner.late_selector_dispatch_status(
                                   root / "late-selector-dispatch.txt", root / "late-input-pc.txt",
                                   root / "host-input-receipt.txt").splitlines()))
            fields.update(file_status("late_input_pc", late_payload))
            fields.update(file_status("late_selector_dispatch", late_selector_payload))
            fields.update(file_status("configuration", config.read_bytes()))
            status = root / "run-status.txt"
            status.write_text("".join(f"{key}={value}\n" for key, value in fields.items()),
                              encoding="utf-8")
            self.assertEqual(TOOL.verify("deuteros-amiga", root), "29")

            v20_late_payload = late_payload + (
                "late-pc 2 cycles=21 pc=0x0001fc22 ir_opcode=0x4e75 memory_opcode=0x4e75 "
                "d0=0x00000000 a0=0x00000000 a6=0x00000000 sr=0x0000 "
                "input_ordinal=9 input_frame=90\n").encode("ascii")
            (root / "late-input-pc.txt").write_bytes(v20_late_payload)
            fields["capture_receipt_version"] = "30"
            fields["recorder_protocol"] = "deuteros-amiga-fsuae-v20"
            fields["recorder_sha256"] = runner.TRV2_RECORDER_V20_SHA256
            fields["recorder_bytes"] = str(runner.TRV2_RECORDER_V20_SIZE)
            v20_late_status = (runner.late_raw_pc_status(
                root / "late-input-pc.txt", root / "host-input-receipt.txt", "v20")
                + runner.late_selector_dispatch_status(
                    root / "late-selector-dispatch.txt", root / "late-input-pc.txt",
                    root / "host-input-receipt.txt", "v20"))
            for key in tuple(fields):
                if key == "late_input_pc" or key.startswith("late_input_pc_") \
                        or key == "late_selector_dispatch" or key.startswith("late_selector_dispatch_"):
                    del fields[key]
            fields.update(dict(line.split("=", 1) for line in v20_late_status.splitlines()))
            fields.update(file_status("late_input_pc", v20_late_payload))
            status.write_text("".join(f"{key}={value}\n" for key, value in fields.items()),
                              encoding="utf-8")
            self.assertEqual(TOOL.verify("deuteros-amiga", root), "30")

            fields["capture_receipt_version"] = "32"
            fields["recorder_protocol"] = "deuteros-amiga-fsuae-v22"
            fields["recorder_sha256"] = runner.TRV2_RECORDER_V22_SHA256
            fields["recorder_bytes"] = str(runner.TRV2_RECORDER_V22_SIZE)
            v22_status = (runner.zero_route_observation_status(
                root / "zero-route-observation.txt", root / "host-input-receipt.txt")
                + runner.late_display_receipt_status(
                    root / "late-display.txt", root / "host-input-receipt.txt"))
            fields.update(dict(line.split("=", 1) for line in v22_status.splitlines()))
            status.write_text("".join(f"{key}={value}\n" for key, value in fields.items()),
                              encoding="utf-8")
            self.assertEqual(TOOL.verify("deuteros-amiga", root), "32")

    def test_schema23_identity_rejects_v16_even_without_raw_observations(self) -> None:
        runner = TOOL.load_tool("run_deuteros_amiga_capture")
        fields = {"recorder_sha256": runner.TRV2_RECORDER_V16_SHA256,
                  "recorder_bytes": str(runner.TRV2_RECORDER_V16_SIZE)}
        with self.assertRaisesRegex(ValueError, "requires receipt schema 24"):
            TOOL.verify_deuteros_recorder_identity(fields, "23", runner)
        historical_v15 = {"recorder_sha256": runner.TRV2_RECORDER_V15_SHA256,
                          "recorder_bytes": "62014944"}
        TOOL.verify_deuteros_recorder_identity(historical_v15, "23", runner)
        labelled_v15 = {**historical_v15, "recorder_protocol": "deuteros-amiga-fsuae-v15"}
        TOOL.verify_deuteros_recorder_identity(labelled_v15, "23", runner)

    def test_v10_deuteros_title_display_summary_is_recomputed(self) -> None:
        with temporary_directory() as directory:
            root = Path(directory)
            (root / "host-input-receipt.txt").write_text(
                "host-input 1 frame=2 line=3 action=4 state=1\n", encoding="ascii")
            display = root / "title-display.txt"
            display.write_text(
                "display-arm 1 cycles=10 site=0x0001eda6 input_ordinal=0 input_frame=0\n"
                "display-write 2 cycles=11 vpos=1 hpos=2 origin=cpu register=0x0080 value=0x1234 input_ordinal=1 input_frame=2\n",
                encoding="ascii")
            capture = TOOL.load_tool("run_deuteros_amiga_capture")
            fields = dict(line.split("=", 1) for line in
                capture.title_display_receipt_status(display, root / "host-input-receipt.txt").splitlines())
            TOOL.verify_deuteros_title_display(fields, root)
            fields["title_display_writes"] = "2"
            with self.assertRaisesRegex(ValueError, "grammar/count"):
                TOOL.verify_deuteros_title_display(fields, root)

    def test_v5_deuteros_host_input_summary_is_recomputed_from_strict_records(self) -> None:
        with temporary_directory() as directory:
            root = Path(directory)
            receipt = root / "host-input-receipt.txt"
            receipt.write_text("host-input 1 frame=2 line=3 action=4 state=1\n", encoding="ascii")
            fields = {"host_input_receipt": "present", "host_input_receipt_records": "1"}
            TOOL.verify_deuteros_host_input_summary(fields, root)
            fields["host_input_receipt_records"] = "2"
            with self.assertRaisesRegex(ValueError, "grammar/count"):
                TOOL.verify_deuteros_host_input_summary(fields, root)

    def test_v5_millennium_host_input_summary_is_recomputed_from_strict_records(self) -> None:
        with temporary_directory() as directory:
            root = Path(directory)
            receipt = root / "host-input-receipt.raw"
            receipt.write_text("host-key 1 ticks=2 state=down scancode=0x1 sym=0x2 mod=0x0\n", encoding="ascii")
            fields = {"host_input_receipt": "present", "host_input_receipt_records": "1"}
            TOOL.verify_millennium_host_input_summary(fields, root)
            fields["host_input_receipt_records"] = "2"
            with self.assertRaisesRegex(ValueError, "grammar/count"):
                TOOL.verify_millennium_host_input_summary(fields, root)

    def test_console_requires_a_bounded_retained_file(self) -> None:
        with temporary_directory() as directory:
            root = Path(directory)
            (root / "recorder-console.log").write_bytes(b"x")
            fields = {"recorder_console": "present", "recorder_console_retained_bytes": "1",
                      "recorder_console_retained_sha256": hashlib.sha256(b"x").hexdigest(),
                      "recorder_console_total_bytes": "2", "recorder_console_sha256": "a" * 64}
            TOOL.verify_console(fields, root)
            fields["recorder_console_retained_bytes"] = "2"
            with self.assertRaisesRegex(ValueError, "mismatch"):
                TOOL.verify_console(fields, root)
            fields["recorder_console_retained_bytes"] = "1"
            fields["recorder_console_sha256"] = "z" * 64
            with self.assertRaisesRegex(ValueError, "mismatch"):
                TOOL.verify_console(fields, root)

    def test_v4_rejects_a_console_safety_overrun(self) -> None:
        TOOL.verify_console_admission({"recorder_console_over_limit": "false"}, "4")
        TOOL.verify_console_admission({"recorder_console_over_limit": "false"}, "5")
        TOOL.verify_console_admission({"recorder_console_over_limit": "false"}, "6")
        TOOL.verify_console_admission({"recorder_console_over_limit": "false"}, "7")
        TOOL.verify_console_admission({"recorder_console_over_limit": "false"}, "8")
        TOOL.verify_console_admission({"recorder_console_over_limit": "false"}, "9")
        TOOL.verify_console_admission({"recorder_console_over_limit": "false"}, "10")
        TOOL.verify_console_admission({"recorder_console_over_limit": "false"}, "11")
        TOOL.verify_console_admission({"recorder_console_over_limit": "false"}, "12")
        TOOL.verify_console_admission({"recorder_console_over_limit": "false"}, "13")
        for version in ("14", "15", "16", "17", "18", "19"):
            with self.subTest(version=version):
                TOOL.verify_console_admission({"recorder_console_over_limit": "false"}, version)
        TOOL.verify_console_admission({"recorder_console_over_limit": "false"}, "20")
        TOOL.verify_console_admission({}, "3")
        with self.assertRaisesRegex(ValueError, "safety cap"):
            TOOL.verify_console_admission({"recorder_console_over_limit": "true"}, "4")
        with self.assertRaisesRegex(ValueError, "safety cap"):
            TOOL.verify_console_admission({"recorder_console_over_limit": "true"}, "5")
        with self.assertRaisesRegex(ValueError, "safety cap"):
            TOOL.verify_console_admission({}, "4")

    def test_v13_recomputes_host_key_to_title_poll_chronology(self) -> None:
        with temporary_directory() as directory:
            root = Path(directory)
            results = root / "results.raw"
            keys = root / "host-input-receipt.raw"
            keys.write_bytes((
                "host-key 1 ticks=2 state=down scancode=0x1 sym=0x2 mod=0x0\n"
                "host-key 2 ticks=3 state=up scancode=0x1 sym=0x2 mod=0x0\n").encode("ascii"))
            results.write_bytes((
                "raw-result\t1 1 title-input-poll image=titles.exe pc=0x0d0a "
                "host_key_ordinal=2 ah=0x06 dl=0xff\n").encode("ascii"))
            capture = TOOL.load_tool("run_millennium_dos_capture")
            fields = dict(line.split("=", 1) for line in
                capture.title_input_checkpoint_status(results, keys, "v13-title-poll").splitlines())
            fields.update(dict(line.split("=", 1) for line in
                capture.raw_result_status(results, "results_raw", "v13-title-poll").splitlines()
                if line.startswith("results_raw_title_input") or line.startswith("results_raw_last_host")))
            TOOL.verify_millennium_title_input_checkpoint(fields, root)
            fields["title_input_checkpoint"] = "accepted"
            with self.assertRaisesRegex(ValueError, "checkpoint receipt mismatch"):
                TOOL.verify_millennium_title_input_checkpoint(fields, root)

    def test_v14_requires_a_valid_normal_core_history_for_early_stop(self) -> None:
        with temporary_directory() as directory:
            root = Path(directory)
            history = root / "normal-core-history.raw"
            capture = TOOL.load_tool("run_millennium_dos_capture")
            history.write_bytes(("normal-core-history-v1 count=16 entries="
                + ",".join(capture.KNOWN_V14_NORMAL_CORE_HISTORY) + "\n").encode("ascii"))
            (root / "results.raw").write_bytes(capture.KNOWN_V11_EARLY_STOP_RAW)
            fields = dict(line.split("=", 1) for line in
                capture.normal_core_history_status(history, "v14-normal-core-history").splitlines())
            fields["termination_reason"] = "known-unhandled-interrupt"
            fields.update(dict(line.split("=", 1) for line in
                capture.normal_core_history_boundary_status(
                    history, root / "results.raw", "v14-normal-core-history",
                    "known-unhandled-interrupt").splitlines()))
            TOOL.verify_millennium_normal_core_history(fields, root)
            fields["normal_core_history"] = "absent"
            with self.assertRaisesRegex(ValueError, "normal-core history receipt mismatch"):
                TOOL.verify_millennium_normal_core_history(fields, root)

    def test_v6_millennium_machine_profile_matches_generated_configuration(self) -> None:
        with temporary_directory() as directory:
            root = Path(directory)
            (root / "recorder.conf").write_text("[dosbox]\nmachine=ega\n", encoding="utf-8")
            TOOL.verify_millennium_machine_profile({"machine_profile": "ega"}, root)
            with self.assertRaisesRegex(ValueError, "does not match"):
                TOOL.verify_millennium_machine_profile({"machine_profile": "svga_s3"}, root)

    def test_millennium_termination_reason_is_bound_to_its_receipt_version(self) -> None:
        with temporary_directory() as directory:
            root = Path(directory)
            (root / "results.raw").write_text(
                "raw-result\t1 1 image=mill.com pc=0x020e source-int=0x21 source-ax=0x2591 ax=0x2591\n"
                "raw-result\t2 2 image=mill.com pc=0x0213 source-call=0x0511 ax=0x0000\n"
                "raw-result\t3 3 private-vector image=titles.exe pc=0x0127 int=0x91 vector_ip=0x0000 vector_cs=0x087e\n"
                "raw-result\t4 4 private-handler-entry int=0x91 cs=0x087e ip=0x0000\n"
                "raw-result\t5 5 private-handler-return int=0x91 caller=titles.exe pc=0x0129 ax=0x0101 flags=0x7202\n"
                "raw-result\t6 6 image=titles.exe pc=0x0129 source-int=0x91 source-ax=0x0000 ax=0x0101\n"
                "raw-result\t7 7 image=titles.exe pc=0x0129 source-int=0x91 source-ax=0x0000 ax=0x0000\n"
                "raw-result\t8 8 fault=unhandled-interrupt int=0x06 cs=0xf000 ip=0xca64 "
                "ss=0x0a8d sp=0xc9bf ax=0x00a0 bx=0x6101 cx=0x178b dx=0x6101\n",
                encoding="ascii")
            TOOL.verify_millennium_termination(
                {"termination_reason": "known-unhandled-interrupt", "exit_status": "126",
                 "results_raw": "present"}, root, "10")
            (root / "results.raw").write_bytes(TOOL.load_tool("run_millennium_dos_capture").KNOWN_V11_EARLY_STOP_RAW)
            TOOL.verify_millennium_termination(
                {"termination_reason": "known-unhandled-interrupt", "exit_status": "126",
                 "results_raw": "present"}, root, "11")
            altered = bytearray((root / "results.raw").read_bytes())
            altered[-17] ^= 0x01
            (root / "results.raw").write_bytes(altered)
            with self.assertRaisesRegex(ValueError, "exact v11"):
                TOOL.verify_millennium_termination(
                    {"termination_reason": "known-unhandled-interrupt", "exit_status": "126",
                     "results_raw": "present"}, root, "11")
            (root / "results.raw").write_text("raw-result\t1 1 fault=unhandled-interrupt int=0x06 cs=0xf000 ip=0xca64 ss=0x0a8d sp=0xc9bf ax=0x00a0 bx=0x6101 cx=0x178b dx=0x6101\n", encoding="ascii")
            with self.assertRaisesRegex(ValueError, "exact v10"):
                TOOL.verify_millennium_termination(
                    {"termination_reason": "known-unhandled-interrupt", "exit_status": "126",
                     "results_raw": "present"}, root, "10")
        with self.assertRaisesRegex(ValueError, "termination reason"):
            TOOL.verify_millennium_termination(
                {"termination_reason": "known-unhandled-interrupt", "exit_status": "124"}, Path("/"), "11")
        with self.assertRaisesRegex(ValueError, "invalid termination"):
            TOOL.verify_millennium_termination(
                {"termination_reason": "made-up", "exit_status": "0"}, Path("/"), "11")

    def test_v6_deuteros_timing_profile_matches_generated_configuration(self) -> None:
        with temporary_directory() as directory:
            root = Path(directory)
            (root / "deuteros-amiga-capture.fs-uae").write_text("warp_mode = 1\n", encoding="utf-8")
            TOOL.verify_deuteros_timing_profile({"timing_profile": "warp"}, root)
            with self.assertRaisesRegex(ValueError, "does not match"):
                TOOL.verify_deuteros_timing_profile({"timing_profile": "realtime"}, root)


if __name__ == "__main__":
    unittest.main()
