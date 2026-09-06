import unittest

from tools.verify_repository_artifacts import forbidden_tracked_paths, main


class RepositoryArtifactPolicyTests(unittest.TestCase):
    def test_rejects_original_media_and_generated_reports(self) -> None:
        paths = [
            "local/game.adf",
            "analysis-work/member.bin",
            "reports/deuteros.disassembly.md",
            "reverse/millennium.objdump",
        ]
        self.assertEqual(forbidden_tracked_paths(paths), sorted(paths))

    def test_allows_preservation_metadata_tools_and_source(self) -> None:
        self.assertEqual(forbidden_tracked_paths([
            "docs/disassembly-inventory.json",
            "docs/COMPLETE_DISASSEMBLY.md",
            "tools/disassemble_m68k_range.py",
            "src/release_runtime.cpp",
        ]), [])

    def test_current_repository_passes(self) -> None:
        self.assertEqual(main(), 0)


if __name__ == "__main__":
    unittest.main()
