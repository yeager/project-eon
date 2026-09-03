"""Contracts for cross-document preservation-ledger verification."""
from __future__ import annotations

import importlib.util
import json
from pathlib import Path
import shutil
import unittest

from eon_test_paths import temporary_directory


ROOT = Path(__file__).resolve().parents[1]
SPEC = importlib.util.spec_from_file_location("verify_preservation_ledger", ROOT / "tools" / "verify_preservation_ledger.py")
assert SPEC and SPEC.loader
TOOL = importlib.util.module_from_spec(SPEC)
SPEC.loader.exec_module(TOOL)


class PreservationLedgerVerifierTests(unittest.TestCase):
    def copied_docs(self) -> Path:
        temporary = temporary_directory()
        self.addCleanup(temporary.cleanup)
        directory = Path(temporary.name)
        shutil.copytree(ROOT / "docs", directory / "docs")
        return directory

    def test_current_repository_ledgers_are_cross_consistent(self) -> None:
        result = TOOL.verify(ROOT)
        self.assertEqual(result["releases"], 8)
        self.assertEqual(result["direct_media_sets"], 1)

    def test_rejects_direct_media_set_that_no_longer_matches_its_canonical_hash(self) -> None:
        root = self.copied_docs()
        path = root / "docs" / "release-manifest.json"
        data = json.loads(path.read_text(encoding="utf-8"))
        data["direct_media_sets"][0]["members"][0]["size"] += 1
        path.write_text(json.dumps(data), encoding="utf-8")
        with self.assertRaisesRegex(TOOL.LedgerError, "canonical hash"):
            TOOL.verify(root)

    def test_rejects_direct_media_set_that_crosses_release_identity(self) -> None:
        root = self.copied_docs()
        path = root / "docs" / "release-manifest.json"
        data = json.loads(path.read_text(encoding="utf-8"))
        data["direct_media_sets"][0]["language"] = "es"
        path.write_text(json.dumps(data), encoding="utf-8")
        with self.assertRaisesRegex(TOOL.LedgerError, "differs from its release identity"):
            TOOL.verify(root)

    def test_rejects_an_orphaned_parity_release(self) -> None:
        root = self.copied_docs()
        path = root / "docs" / "parity-matrix.json"
        data = json.loads(path.read_text(encoding="utf-8"))
        data["releases"].pop()
        path.write_text(json.dumps(data), encoding="utf-8")
        with self.assertRaisesRegex(TOOL.LedgerError, "parity matrix"):
            TOOL.verify(root)

    def test_rejects_a_static_span_outside_its_original_leaf(self) -> None:
        root = self.copied_docs()
        path = root / "docs" / "disassembly-inventory.json"
        data = json.loads(path.read_text(encoding="utf-8"))
        data["releases"][0]["static_spans"][0]["segments"][0]["length"] = 9_999_999
        path.write_text(json.dumps(data), encoding="utf-8")
        with self.assertRaisesRegex(TOOL.LedgerError, "unbounded or overlapping"):
            TOOL.verify(root)

    def test_rejects_a_cross_release_start_profile(self) -> None:
        root = self.copied_docs()
        path = root / "docs" / "disassembly-inventory.json"
        data = json.loads(path.read_text(encoding="utf-8"))
        data["releases"][0]["start_profile_id"] = "deuteros-amiga-clean-main-stage"
        path.write_text(json.dumps(data), encoding="utf-8")
        with self.assertRaisesRegex(TOOL.LedgerError, "invalid start profile"):
            TOOL.verify(root)


if __name__ == "__main__":
    unittest.main()
