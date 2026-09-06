import tempfile
import unittest
from pathlib import Path

from tools.verify_repository_artifacts import (
    forbidden_tracked_content,
    forbidden_tracked_paths,
    main,
)


class RepositoryArtifactPolicyTests(unittest.TestCase):
    def test_rejects_original_media_and_generated_reports(self) -> None:
        paths = [
            "local/game.adf",
            "analysis-work/member.bin",
            "reports/deuteros.disassembly.md",
            "reverse/millennium.objdump",
            "reverse/project.gpr.ghidra",
            "captures/memory.dump",
            "media/original.ipf",
            "media/original.zip",
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

    def test_rejects_instruction_listing_hidden_in_markdown(self) -> None:
        with tempfile.TemporaryDirectory() as temporary:
            root = Path(temporary)
            (root / "innocent.md").write_text(
                "# Technical note\n\n```asm\n00000100  rts\n```\n",
                encoding="utf-8",
            )
            self.assertEqual(
                forbidden_tracked_content(["innocent.md"], root),
                ["innocent.md"],
            )

    def test_allows_hash_addressed_preservation_metadata(self) -> None:
        with tempfile.TemporaryDirectory() as temporary:
            root = Path(temporary)
            (root / "ledger.md").write_text(
                "Span 0x100..0x120 has SHA-256 deadbeef and remains external.\n",
                encoding="utf-8",
            )
            self.assertEqual(forbidden_tracked_content(["ledger.md"], root), [])

    def test_checks_index_blob_instead_of_different_worktree_bytes(self) -> None:
        with tempfile.TemporaryDirectory() as temporary:
            root = Path(temporary)
            (root / "apparently-safe.md").write_text("safe worktree copy\n", encoding="utf-8")
            self.assertEqual(forbidden_tracked_content(
                ["apparently-safe.md"], root,
                blob_reader=lambda _path: b"```objdump\n00000100  rts\n```\n",
            ), ["apparently-safe.md"])


if __name__ == "__main__":
    unittest.main()
