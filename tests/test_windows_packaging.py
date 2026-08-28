"""Keep the Windows installer preservation-safe without running Inno Setup."""

from __future__ import annotations

from pathlib import Path
import unittest


ROOT = Path(__file__).resolve().parents[1]
INSTALLER = ROOT / "packaging" / "windows" / "project-eon.iss"


class WindowsPackagingTests(unittest.TestCase):
    def test_installer_does_not_create_or_stage_a_game_data_directory(self) -> None:
        source = INSTALLER.read_text(encoding="utf-8")
        # The application may look up <install-dir>/data at run time, but a
        # missing data directory is an intentional preservation boundary.
        # Creating one during installation would make package behaviour differ
        # from a normal read-only lookup and suggests that media belongs there.
        self.assertNotIn("[Dirs]", source)
        self.assertNotIn('DestDir: "{app}\\data', source)
        self.assertNotIn('Source: "{#StagingDir}\\data', source)

    def test_installer_stages_libpng_for_sdl_image(self) -> None:
        source = INSTALLER.read_text(encoding="utf-8")
        self.assertIn('Source: "{#StagingDir}\\libpng*.dll"', source)


if __name__ == "__main__":
    unittest.main()
