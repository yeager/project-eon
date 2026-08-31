import hashlib
import importlib.util
import json
from pathlib import Path
import unittest

from eon_test_paths import temporary_directory


ROOT = Path(__file__).resolve().parents[1]
SPEC = importlib.util.spec_from_file_location(
    "verify_disassembly_reports", ROOT / "tools" / "verify_disassembly_reports.py")
assert SPEC and SPEC.loader
TOOL = importlib.util.module_from_spec(SPEC)
SPEC.loader.exec_module(TOOL)


class VerifyDisassemblyReportsTests(unittest.TestCase):
    def write_inventory(self, root: Path, entries: list[tuple[str, bytes]]) -> Path:
        spans = [{"id": identifier, "report_sha256": hashlib.sha256(content).hexdigest(),
                  "report_lines": content.count(b"\n")} for identifier, content in entries]
        path = root / "inventory.json"
        path.write_text(json.dumps({"schema": "project-eon.disassembly-inventory/v2",
                                    "releases": [{"static_spans": spans}]}), encoding="utf-8")
        return path

    def test_hash_and_line_identity_accepts_reused_external_report(self) -> None:
        with temporary_directory() as temporary:
            root = Path(temporary)
            report = root / "report.md"
            content = b"one\ntwo\n"
            report.write_bytes(content)
            expected = TOOL.load_expected_spans(self.write_inventory(root, [("one", content), ("two", content)]))
            self.assertEqual(TOOL.verify_reports(expected, {"one": report, "two": report}), 1)

    def test_missing_or_changed_report_is_rejected(self) -> None:
        with temporary_directory() as temporary:
            root = Path(temporary)
            report = root / "report.md"
            content = b"one\n"
            report.write_bytes(content)
            expected = TOOL.load_expected_spans(self.write_inventory(root, [("one", content), ("two", content)]))
            with self.assertRaisesRegex(TOOL.ReportError, "span set"):
                TOOL.verify_reports(expected, {"one": report})
            report.write_bytes(b"changed\n")
            with self.assertRaisesRegex(TOOL.ReportError, "hash/line"):
                TOOL.verify_reports(expected, {"one": report, "two": report})

    def test_repository_and_tmp_paths_are_not_report_inputs(self) -> None:
        with self.assertRaisesRegex(TOOL.ReportError, "outside /tmp"):
            TOOL.require_external_report(Path("/tmp/report.md"))
        with self.assertRaisesRegex(TOOL.ReportError, "outside the repository"):
            TOOL.require_external_report(Path(__file__).resolve())


if __name__ == "__main__":
    unittest.main()
