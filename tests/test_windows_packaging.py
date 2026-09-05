"""Keep the Windows installer preservation-safe without running Inno Setup."""

from __future__ import annotations

from pathlib import Path
import re
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

    def test_load_time_runtime_libraries_are_not_optional(self) -> None:
        source = INSTALLER.read_text(encoding="utf-8")
        for runtime in ("SDL3_image.dll", "SDL3_ttf.dll", "libpng*.dll", "zlib*.dll"):
            with self.subTest(runtime=runtime):
                entry = next(line for line in source.splitlines() if runtime in line)
                self.assertNotIn("skipifsourcedoesntexist", entry)

    def test_ci_installs_the_final_artifact_and_smoke_tests_the_loader(self) -> None:
        workflow = (ROOT / ".github" / "workflows" / "build.yml").read_text(encoding="utf-8")
        self.assertIn("Verify installed Inno Setup package and runtime closure", workflow)
        self.assertIn("/VERYSILENT /SUPPRESSMSGBOXES /NORESTART /SP-", workflow)
        self.assertIn("installed package lacks staged file", workflow)
        self.assertIn("Get-FileHash -Algorithm SHA256", workflow)
        self.assertIn("installed package must not create a game-data directory", workflow)
        self.assertIn("did not retain its Windows default data boundary", workflow)
        self.assertIn("Data path does not exist:", workflow)
        self.assertIn("created its default game-data directory during lookup", workflow)
        self.assertIn("installed Project Eon executable did not load and print its CLI usage", workflow)

    def test_ci_rejects_unreviewed_files_before_inno_recurses_stage_directories(self) -> None:
        workflow = (ROOT / ".github" / "workflows" / "build.yml").read_text(encoding="utf-8")
        self.assertIn("Windows package stage contains unexpected file(s)", workflow)
        self.assertIn("$approved", workflow)
        self.assertIn("$relative -notin $approved", workflow)
        self.assertIn(".Replace('\\', '/')", workflow)
        self.assertIn("'^(libpng.+|zlib.*)\\.dll$'", workflow)

    def test_ci_font_allowlist_is_exactly_the_reviewed_bundle(self) -> None:
        workflow = (ROOT / ".github" / "workflows" / "build.yml").read_text(encoding="utf-8")
        match = re.search(r'^\s*\$fonts = @\(([^\n]+)\)$', workflow, re.MULTILINE)
        self.assertIsNotNone(match)
        staged_fonts = set(re.findall(r'"([^"]+)"', match.group(1)))
        reviewed_fonts = {
            path.name for path in (ROOT / "assets" / "fonts").iterdir() if path.is_file()
        }
        self.assertEqual(staged_fonts, reviewed_fonts)
        self.assertIn("README.md", staged_fonts)
        self.assertIn("OFL-1.1.txt", staged_fonts)
        # The recursive copy is safe only because approval remains per-file.
        self.assertNotIn('$required += "assets/fonts/*"', workflow)
        self.assertNotIn('$approved += "assets/fonts/*"', workflow)

    def test_ci_discovers_cmake_generated_runtime_dlls_before_ctest(self) -> None:
        workflow = (ROOT / ".github" / "workflows" / "build.yml").read_text(encoding="utf-8")
        self.assertIn("generatedRuntimeDirectories", workflow)
        self.assertIn("Get-ChildItem -Path build -Recurse -File -Filter *.dll", workflow)
        self.assertIn("STATUS_DLL_NOT_FOUND", workflow)


if __name__ == "__main__":
    unittest.main()
