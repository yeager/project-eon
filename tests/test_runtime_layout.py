"""Cross-platform launcher resource and read-only data-location contracts.

These checks deliberately inspect packaging/source contracts rather than using
commercial media. They keep the installed layouts aligned with the runtime's
search order on Linux, macOS, Windows and iPadOS.
"""

from __future__ import annotations

from pathlib import Path
import unittest


ROOT = Path(__file__).resolve().parents[1]
CMAKE = (ROOT / "CMakeLists.txt").read_text(encoding="utf-8")
I18N = (ROOT / "src" / "i18n.cpp").read_text(encoding="utf-8")
LAUNCHER = (ROOT / "src" / "launcher.cpp").read_text(encoding="utf-8")
MAIN = (ROOT / "src" / "main.cpp").read_text(encoding="utf-8")
WORKFLOW = (ROOT / ".github" / "workflows" / "build.yml").read_text(encoding="utf-8")
INNO = (ROOT / "packaging" / "windows" / "project-eon.iss").read_text(encoding="utf-8")
README = (ROOT / "README.md").read_text(encoding="utf-8")
PRESERVATION = (ROOT / "docs" / "PRESERVATION.md").read_text(encoding="utf-8")


class RuntimeLayoutTests(unittest.TestCase):
    def test_default_data_locations_are_read_only_lookup_contracts(self) -> None:
        self.assertIn('absolute_executable.parent_path() / "data"', LAUNCHER)
        self.assertIn('std::filesystem::path(home) / ".projecteon"', LAUNCHER)
        self.assertNotIn("create_directories", LAUNCHER)
        self.assertIn("Without --data/--data-dir, game data is read from ~/.projecteon", LAUNCHER)
        self.assertIn('std::filesystem::path(home) / "Documents" / "ProjectEon"', LAUNCHER)
        self.assertIn("Documents/ProjectEon", README)
        self.assertIn("Documents/ProjectEon", PRESERVATION)

    def test_linux_install_layout_matches_runtime_search_order(self) -> None:
        self.assertIn('install(DIRECTORY assets/cards DESTINATION "${CMAKE_INSTALL_BINDIR}/assets")', CMAKE)
        self.assertIn('install(DIRECTORY assets/fonts DESTINATION "${CMAKE_INSTALL_BINDIR}/assets")', CMAKE)
        self.assertIn('install(DIRECTORY po/ DESTINATION "${CMAKE_INSTALL_DATADIR}/project-eon/po")', CMAKE)
        self.assertIn('executable_directory / "po"', I18N)
        self.assertIn('executable_directory / ".." / "share" / "project-eon" / "po"', I18N)
        self.assertIn('base / "assets" / "cards"', MAIN)
        self.assertIn('base / "assets" / "fonts"', MAIN)

    def test_apple_bundle_layout_matches_runtime_search_order(self) -> None:
        self.assertIn('MACOSX_PACKAGE_LOCATION "Resources/assets/cards"', CMAKE)
        self.assertIn('MACOSX_PACKAGE_LOCATION "Resources/assets/fonts"', CMAKE)
        self.assertIn('MACOSX_PACKAGE_LOCATION "Resources/po"', CMAKE)
        self.assertIn('base / "Resources" / "assets" / "cards"', MAIN)
        self.assertIn('base / "Resources" / "assets" / "fonts"', MAIN)
        self.assertIn('executable_directory / ".." / "Resources" / "po"', I18N)
        self.assertIn('executable_directory / "Resources" / "po"', I18N)
        self.assertIn('mkdir -p "$APP/Contents/MacOS/assets/cards" "$APP/Contents/Resources/po"', WORKFLOW)

    def test_public_docs_distinguish_stx_metadata_reads_from_flattening(self) -> None:
        self.assertIn("physical media such as STX remain in their original container form", README)
        self.assertIn("file-payload extraction, boot interpretation", PRESERVATION)
        self.assertIn("read-only metadata traversal", PRESERVATION)
        self.assertNotIn("STX flattening, filesystem traversal, boot interpretation", PRESERVATION)

    def test_windows_stage_matches_runtime_search_order_without_data_directory(self) -> None:
        self.assertIn('Copy-Item po/*.po dist/po/', WORKFLOW)
        self.assertIn('Copy-Item $sdlTtf.FullName dist/SDL3_ttf.dll', WORKFLOW)
        self.assertIn('Source: "{#StagingDir}\\SDL3_ttf.dll"', INNO)
        self.assertIn('Source: "{#StagingDir}\\po\\*"; DestDir: "{app}\\po"', INNO)
        self.assertNotIn('DestDir: "{app}\\data', INNO)


if __name__ == "__main__":
    unittest.main()
