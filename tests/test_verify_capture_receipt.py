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


if __name__ == "__main__":
    unittest.main()
