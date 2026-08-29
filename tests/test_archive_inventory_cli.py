"""Keep the opt-in archive manifest a verified, read-only preservation tool."""

from pathlib import Path
import unittest


ROOT = Path(__file__).resolve().parents[1]
MAIN = (ROOT / "src" / "main.cpp").read_text(encoding="utf-8")
LAUNCHER = (ROOT / "src" / "launcher.cpp").read_text(encoding="utf-8")


class ArchiveInventoryCliTests(unittest.TestCase):
    def test_inventory_requires_inspection_and_is_documented(self) -> None:
        self.assertIn('argument == "--inventory"', LAUNCHER)
        self.assertIn("--inventory requires --inspect", LAUNCHER)
        self.assertIn("[--inventory]", LAUNCHER)

    def test_inventory_rehashes_before_bounded_leaf_report(self) -> None:
        start = MAIN.index("void report_verified_release_inventory")
        body = MAIN[start:MAIN.index("SDL_FRect aspect_viewport", start)]
        self.assertIn("inventory_verified_release(release)", body)
        self.assertIn("hash-addressed leaf asset", body)
        self.assertIn("read in place only", body)
        self.assertIn("asset.sha256", body)
        self.assertIn("asset.path", body)


if __name__ == "__main__":
    unittest.main()
