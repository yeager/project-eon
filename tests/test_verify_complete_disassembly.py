import copy
import importlib.util
import json
from pathlib import Path
import unittest


ROOT = Path(__file__).resolve().parents[1]
SPEC = importlib.util.spec_from_file_location(
    "verify_complete_disassembly", ROOT / "tools" / "verify_complete_disassembly.py")
assert SPEC and SPEC.loader
TOOL = importlib.util.module_from_spec(SPEC)
SPEC.loader.exec_module(TOOL)


class CompleteDisassemblyManifestTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls) -> None:
        cls.manifest = json.loads((ROOT / "docs/complete-disassembly-manifest.json").read_text())
        cls.inventory = json.loads((ROOT / "docs/disassembly-inventory.json").read_text())
        cls.releases = json.loads((ROOT / "docs/release-manifest.json").read_text())

    def test_committed_manifest_enumerates_and_covers_the_recognized_corpus(self) -> None:
        totals = TOOL.verify(self.manifest, self.inventory, self.releases)
        self.assertEqual(totals, {"releases": 8, "images": 16,
                                  "ranges": 18, "bytes": 1122819,
                                  "mapped_candidates": 14,
                                  "unmapped_candidates": 6})
        index = TOOL.render_index(self.manifest, totals)
        self.assertIn("Raw disassembly listings remain outside the repository.", index)
        self.assertIn("millennium / dos / en", index)
        self.assertIn("deuteros / amiga / en", index)
        self.assertIn("Discovered but unmapped: `deuteros-atari-killer-boot`", index)

    def test_missing_release_or_image_is_rejected(self) -> None:
        manifest = copy.deepcopy(self.manifest)
        manifest["releases"].pop()
        with self.assertRaisesRegex(TOOL.ManifestError, "release set"):
            TOOL.verify(manifest, self.inventory, self.releases)
        manifest = copy.deepcopy(self.manifest)
        manifest["releases"][5]["images"].pop()
        with self.assertRaisesRegex(TOOL.ManifestError, "image set"):
            TOOL.verify(manifest, self.inventory, self.releases)

    def test_gap_overlap_hash_tool_and_address_drift_are_rejected(self) -> None:
        for field, value, message in (
            ("source_sha256", "0" * 64, "source hash"),
            ("architecture", "m68000", "architecture"),
            ("address_basis", "runtime-absolute", "address basis"),
        ):
            manifest = copy.deepcopy(self.manifest)
            manifest["releases"][5]["images"][0][field] = value
            with self.assertRaisesRegex(TOOL.ManifestError, message):
                TOOL.verify(manifest, self.inventory, self.releases)
        manifest = copy.deepcopy(self.manifest)
        manifest["releases"][5]["images"][0]["decoder"]["version"] = "unknown"
        with self.assertRaisesRegex(TOOL.ManifestError, "decoder"):
            TOOL.verify(manifest, self.inventory, self.releases)
        manifest = copy.deepcopy(self.manifest)
        manifest["releases"][1]["images"][0]["source_ranges"][0]["length"] -= 1
        with self.assertRaisesRegex(TOOL.ManifestError, "coverage gap"):
            TOOL.verify(manifest, self.inventory, self.releases)
        manifest = copy.deepcopy(self.manifest)
        ranges = manifest["releases"][1]["images"][0]["source_ranges"]
        ranges[1]["source_offset"] = ranges[0]["source_offset"] + 1
        with self.assertRaisesRegex(TOOL.ManifestError, "overlapping"):
            TOOL.verify(manifest, self.inventory, self.releases)

    def test_discovered_unmapped_candidate_cannot_disappear(self) -> None:
        manifest = copy.deepcopy(self.manifest)
        release = next(row for row in manifest["releases"]
                       if row["release_sha256"].startswith("c6856d0a"))
        release["discovered_unmapped_profile_ids"] = []
        with self.assertRaisesRegex(TOOL.ManifestError, "discovered-unmapped"):
            TOOL.verify(manifest, self.inventory, self.releases)


if __name__ == "__main__":
    unittest.main()
