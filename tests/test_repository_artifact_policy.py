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

    def test_rejects_unfenced_raw_instruction_report_body(self) -> None:
        with tempfile.TemporaryDirectory() as temporary:
            root = Path(temporary)
            (root / "report.txt").write_text(
                "Reverse-engineering notes\n00102a: 4e 75  rts\n",
                encoding="utf-8",
            )
            self.assertEqual(
                forbidden_tracked_content(["report.txt"], root),
                ["report.txt"],
            )

    def test_rejects_large_byte_initializer_in_new_production_source(self) -> None:
        byte_literals = ", ".join(f"0x{value:02x}" for value in range(64))
        source = (
            "#include <array>\n#include <cstdint>\n"
            "constexpr std::array<std::uint8_t, 64> extracted{{"
            f"{byte_literals}}};\n"
        ).encode("ascii")
        self.assertEqual(forbidden_tracked_content(
            ["src/data/new_anchor.cpp"],
            blob_reader=lambda _path: source,
        ), ["src/data/new_anchor.cpp"])

    def test_rejects_unreviewed_small_production_constant_table(self) -> None:
        byte_literals = ", ".join(f"0x{value:02x}" for value in range(32))
        source = (
            "#include <array>\n#include <cstdint>\n"
            "constexpr std::array<std::uint8_t, 32> palette{{"
            f"{byte_literals}}};\n"
        ).encode("ascii")
        self.assertEqual(forbidden_tracked_content(
            ["src/ui/palette.cpp"],
            blob_reader=lambda _path: source,
        ), ["src/ui/palette.cpp"])

    def test_allows_explicitly_reviewed_public_format_constant(self) -> None:
        source = (
            "#include <array>\n#include <cstdint>\n"
            "// EON_ARTIFACT_POLICY_ALLOW: public PNG file signature\n"
            "constexpr std::array<std::uint8_t, 8> signature{{"
            "0x89, 0x50, 0x4e, 0x47, 0x0d, 0x0a, 0x1a, 0x0a}};\n"
        ).encode("ascii")
        self.assertEqual(forbidden_tracked_content(
            ["src/data/png.cpp"], blob_reader=lambda _path: source,
        ), [])

    def test_rejects_large_byte_initializer_in_known_production_source(self) -> None:
        byte_literals = ", ".join("0x90" for _ in range(133))
        source = (
            "#include <array>\n#include <cstdint>\n"
            "constexpr std::array<std::uint8_t, 133> expected{{"
            f"{byte_literals}}};\n"
        ).encode("ascii")
        self.assertEqual(forbidden_tracked_content(
            ["src/data/millennium_amiga_loader.cpp"],
            blob_reader=lambda _path: source,
        ), ["src/data/millennium_amiga_loader.cpp"])

    def test_rejects_hex_sized_byte_initializer(self) -> None:
        source = (
            "#include <array>\n#include <cstdint>\n"
            "constexpr std::array<std::uint8_t, 0x02> extracted{{"
            "0x4e, 0x75}};\n"
        ).encode("ascii")
        self.assertEqual(forbidden_tracked_content(
            ["src/data/short_return.cpp"], blob_reader=lambda _path: source,
        ), ["src/data/short_return.cpp"])

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
