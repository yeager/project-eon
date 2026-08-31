"""Negative contracts for external capture-receipt verification."""
from __future__ import annotations
import hashlib
import importlib.util
from pathlib import Path
import unittest
from eon_test_paths import temporary_directory

ROOT = Path(__file__).resolve().parents[1]
SPEC = importlib.util.spec_from_file_location("verify_capture_receipt", ROOT / "tools" / "verify_capture_receipt.py")
assert SPEC and SPEC.loader
TOOL = importlib.util.module_from_spec(SPEC)
SPEC.loader.exec_module(TOOL)


class ReceiptVerifierTests(unittest.TestCase):
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
        TOOL.verify_console_admission({}, "3")
        with self.assertRaisesRegex(ValueError, "safety cap"):
            TOOL.verify_console_admission({"recorder_console_over_limit": "true"}, "4")
        with self.assertRaisesRegex(ValueError, "safety cap"):
            TOOL.verify_console_admission({"recorder_console_over_limit": "true"}, "5")
        with self.assertRaisesRegex(ValueError, "safety cap"):
            TOOL.verify_console_admission({}, "4")

    def test_v6_millennium_machine_profile_matches_generated_configuration(self) -> None:
        with temporary_directory() as directory:
            root = Path(directory)
            (root / "recorder.conf").write_text("[dosbox]\nmachine=ega\n", encoding="utf-8")
            TOOL.verify_millennium_machine_profile({"machine_profile": "ega"}, root)
            with self.assertRaisesRegex(ValueError, "does not match"):
                TOOL.verify_millennium_machine_profile({"machine_profile": "svga_s3"}, root)

    def test_v10_millennium_termination_reason_is_bound_to_early_stop_status(self) -> None:
        with temporary_directory() as directory:
            root = Path(directory)
            (root / "results.raw").write_text(
                "raw-result\t1 1 fault=unhandled-interrupt int=0x06 cs=0xf000 ip=0xca64 "
                "ss=0x0a8d sp=0xc9bf ax=0x00a0 bx=0x6101 cx=0x178b dx=0x6101\n",
                encoding="ascii")
            TOOL.verify_millennium_termination(
                {"termination_reason": "known-unhandled-interrupt", "exit_status": "126",
                 "results_raw": "present"}, root)
            (root / "results.raw").write_text(
                "raw-result\t1 1 fault=unhandled-interrupt int=0x06 cs=0xf000 ip=0xca64 "
                "ss=0x0a8d sp=0xc9bf ax=0x00a0 bx=0x6101 cx=0x178b dx=0x6101\n"
                "raw-result\t2 2 fault=unhandled-interrupt int=0x06 cs=0xf000 ip=0xca64 "
                "ss=0x0a8d sp=0xc9bf ax=0x00a0 bx=0x6101 cx=0x178b dx=0x6101\n",
                encoding="ascii")
            with self.assertRaisesRegex(ValueError, "exactly one"):
                TOOL.verify_millennium_termination(
                    {"termination_reason": "known-unhandled-interrupt", "exit_status": "126",
                     "results_raw": "present"}, root)
        with self.assertRaisesRegex(ValueError, "termination reason"):
            TOOL.verify_millennium_termination(
                {"termination_reason": "known-unhandled-interrupt", "exit_status": "124"}, Path("/"))
        with self.assertRaisesRegex(ValueError, "invalid termination"):
            TOOL.verify_millennium_termination(
                {"termination_reason": "made-up", "exit_status": "0"}, Path("/"))

    def test_v6_deuteros_timing_profile_matches_generated_configuration(self) -> None:
        with temporary_directory() as directory:
            root = Path(directory)
            (root / "deuteros-amiga-capture.fs-uae").write_text("warp_mode = 1\n", encoding="utf-8")
            TOOL.verify_deuteros_timing_profile({"timing_profile": "warp"}, root)
            with self.assertRaisesRegex(ValueError, "does not match"):
                TOOL.verify_deuteros_timing_profile({"timing_profile": "realtime"}, root)


if __name__ == "__main__":
    unittest.main()
