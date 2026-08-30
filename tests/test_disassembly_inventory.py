import json
from pathlib import Path
import unittest

ROOT = Path(__file__).resolve().parents[1]

class DisassemblyInventoryTests(unittest.TestCase):
    def test_inventory_covers_exact_releases_with_bounded_profiles(self):
        inventory = json.loads((ROOT / "docs/disassembly-inventory.json").read_text())
        manifest = json.loads((ROOT / "docs/release-manifest.json").read_text())
        self.assertEqual(inventory["schema"], "project-eon.disassembly-inventory/v2")
        releases = {row["sha256"] for row in manifest["releases"]}
        profiles = {row["id"]: row for row in manifest["parser_profiles"]}
        self.assertEqual({row["release_sha256"] for row in inventory["releases"]}, releases)
        for row in inventory["releases"]:
            self.assertIn(row["cpu"], {"i8086", "m68000"})
            self.assertTrue(row["unresolved"])
            self.assertTrue(row["coverage"])
            self.assertIn(row["start_profile_id"], row["coverage"])
            self.assertEqual(profiles[row["start_profile_id"]]["release_sha256"], row["release_sha256"])
            for profile_id in row["coverage"]:
                self.assertEqual(profiles[profile_id]["release_sha256"], row["release_sha256"])
            for span in row.get("static_spans", []):
                self.assertIn(span["cpu"], {"i8086", "m68000"})
                self.assertEqual(span["cpu"], row["cpu"])
                self.assertEqual(span["coverage_kind"], "linear-candidate-unclassified")
                self.assertRegex(span["leaf_sha256"], r"^[0-9a-f]{64}$")
                self.assertRegex(span["report_sha256"], r"^[0-9a-f]{64}$")
                self.assertGreater(span["report_lines"], 0)
                self.assertTrue(span["boundary"])
                self.assertTrue(span["segments"])
                known_leaf = any(profile["release_sha256"] == row["release_sha256"]
                                 and profile["leaf_sha256"] == span["leaf_sha256"]
                                 for profile in profiles.values())
                self.assertTrue(known_leaf)
                previous_end = -1
                for segment in span["segments"]:
                    self.assertGreaterEqual(segment["source_offset"], 0)
                    self.assertGreater(segment["length"], 0)
                    self.assertGreaterEqual(segment["runtime_address"], 0)
                    self.assertNotEqual("entry_address" in segment, "entry_offset" in segment)
                    if "entry_address" in segment:
                        self.assertGreaterEqual(segment["entry_address"], segment["runtime_address"])
                        self.assertLess(segment["entry_address"],
                                        segment["runtime_address"] + segment["length"])
                    else:
                        self.assertGreaterEqual(segment["entry_offset"], 0)
                        self.assertLess(segment["entry_offset"], segment["length"])
                    self.assertGreaterEqual(segment["source_offset"], previous_end)
                    previous_end = segment["source_offset"] + segment["length"]

if __name__ == "__main__":
    unittest.main()
