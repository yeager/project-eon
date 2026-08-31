"""Contracts for the 20-language launcher catalog validation route."""

from __future__ import annotations

from pathlib import Path
import unittest


ROOT = Path(__file__).resolve().parents[1]
CMAKE = (ROOT / "CMakeLists.txt").read_text(encoding="utf-8")
WORKFLOW = (ROOT / ".github" / "workflows" / "build.yml").read_text(encoding="utf-8")
PO_README = (ROOT / "po" / "README.md").read_text(encoding="utf-8")
CATALOGS = ("ar", "de", "el", "en_GB", "es", "fi", "fr", "hi", "it", "ja",
            "ko", "nl", "no", "pl", "pt_BR", "ru", "sv", "tr", "uk", "zh_CN")


class L10nTargetTests(unittest.TestCase):
    def test_cmake_target_validates_the_complete_catalog_set_without_source_output(self) -> None:
        self.assertIn("add_custom_target(l10n", CMAKE)
        self.assertIn("cmake/verify_l10n.cmake", CMAKE)
        self.assertIn("eon-l10n", CMAKE)
        self.assertIn("never rewrites translation", PO_README)
        for catalog in CATALOGS:
            self.assertIn(catalog, CMAKE)

    def test_every_shipped_catalog_has_required_utf8_gettext_metadata(self) -> None:
        for catalog in CATALOGS:
            text = (ROOT / "po" / f"{catalog}.po").read_text(encoding="utf-8")
            self.assertIn("Project-Id-Version: Project Eon\\n", text)
            self.assertIn(f"Language: {catalog}\\n", text)
            self.assertIn("Content-Type: text/plain; charset=UTF-8\\n", text)
            self.assertIn("Content-Transfer-Encoding: 8bit\\n", text)
            self.assertNotIn("#, fuzzy", text)

    def test_linux_ci_installs_gettext_and_runs_the_target(self) -> None:
        self.assertIn("cmake ninja-build gettext zlib1g-dev", WORKFLOW)
        self.assertIn("ninja -C build l10n", WORKFLOW)


if __name__ == "__main__":
    unittest.main()
