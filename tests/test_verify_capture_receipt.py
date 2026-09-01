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
from eon_test_paths import temporary_directory

ROOT = Path(__file__).resolve().parents[1]
SPEC = importlib.util.spec_from_file_location("verify_capture_receipt", ROOT / "tools" / "verify_capture_receipt.py")
assert SPEC and SPEC.loader
TOOL = importlib.util.module_from_spec(SPEC)
SPEC.loader.exec_module(TOOL)


class ReceiptVerifierTests(unittest.TestCase):
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
            sidecar.write_text(
                "int93-installation-v1 image=2200ad.exe pc=0x4175 vector=0x93 "
                "ds=0x4567 dx=0x89ab target_preimage=0xdeadbeef "
                "vector_ip=0x89ab vector_cs=0x4567\n", encoding="ascii")
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
            keys.write_text(
                "host-key 1 ticks=2 state=down scancode=0x1 sym=0x2 mod=0x0\n"
                "host-key 2 ticks=3 state=up scancode=0x1 sym=0x2 mod=0x0\n", encoding="ascii")
            results.write_text(
                "raw-result\t1 1 title-input-poll image=titles.exe pc=0x0d0a "
                "host_key_ordinal=2 ah=0x06 dl=0xff\n", encoding="ascii")
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
            history.write_text("normal-core-history-v1 count=16 entries="
                + ",".join(capture.KNOWN_V14_NORMAL_CORE_HISTORY) + "\n", encoding="ascii")
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
