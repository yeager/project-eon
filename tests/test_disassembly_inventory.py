import json
from pathlib import Path
import unittest

ROOT = Path(__file__).resolve().parents[1]

class DisassemblyInventoryTests(unittest.TestCase):
    def test_inventory_covers_exact_releases_with_bounded_profiles(self):
        inventory = json.loads((ROOT / "docs/disassembly-inventory.json").read_text())
        manifest = json.loads((ROOT / "docs/release-manifest.json").read_text())
        self.assertEqual(inventory["schema"], "project-eon.disassembly-inventory/v1")
        releases = {row["sha256"] for row in manifest["releases"]}
        profiles = {row["id"]: row for row in manifest["parser_profiles"]}
        self.assertEqual({row["release_sha256"] for row in inventory["releases"]}, releases)
        for row in inventory["releases"]:
            self.assertIn(row["cpu"], {"i8086", "m68000"})
            self.assertTrue(row["unresolved"])
            self.assertTrue(row["coverage"])
            for profile_id in row["coverage"]:
                self.assertEqual(profiles[profile_id]["release_sha256"], row["release_sha256"])

if __name__ == "__main__":
    unittest.main()
