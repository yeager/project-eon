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

    def test_json_inspection_includes_only_hash_bound_function_map_facts(self) -> None:
        self.assertIn('argument == "--inspect-json"', LAUNCHER)
        self.assertIn("--inspect-json reports release-level diagnostics only", LAUNCHER)
        start = MAIN.index("void report_inspection_json")
        body = MAIN[start:MAIN.index("SDL_FRect aspect_viewport", start)]
        self.assertIn(r'\"project-eon.inspect/v1\"', body)
        self.assertIn(r'\"function_map\"', body)
        self.assertIn(r'\"source_kind\"', body)
        self.assertIn(r'\"symlink_rejected_entries\"', body)
        self.assertIn("runtime_diagnostics_for_release(release)", body)
        self.assertIn("diagnostics.functions", body)
        self.assertNotIn("release.path", body)

    def test_trace_json_is_diagnostics_only_and_omits_local_paths(self) -> None:
        self.assertIn('argument == "--reference-trace-json"', LAUNCHER)
        self.assertIn("--reference-trace-json requires --reference-trace", LAUNCHER)
        start = MAIN.index("void report_reference_trace_json")
        body = MAIN[start:MAIN.index("SDL_FRect aspect_viewport", start)]
        self.assertIn(r'\"project-eon.reference-trace/v1\"', body)
        self.assertIn(r'\"recovery_boundaries\"', body)
        self.assertIn(r'\"artifacts\"', body)
        self.assertNotIn("trace.manifest_path", body)
        self.assertNotIn("trace.events_path", body)
        self.assertNotIn("artifact.path", body)


if __name__ == "__main__":
    unittest.main()
