import pathlib
import os
import shutil
import subprocess
import tempfile
import unittest


ROOT = pathlib.Path(__file__).resolve().parents[1]
SCRIPT = ROOT / "packaging" / "ios" / "package-ipa.sh"
WORKFLOW = ROOT / ".github" / "workflows" / "build.yml"


@unittest.skipUnless(os.name != "nt" and shutil.which("bash") and shutil.which("unzip"),
                     "requires POSIX bash and unzip")
class IosPackagingTests(unittest.TestCase):
    def test_ci_cross_compiles_png_dependencies_for_ios(self):
        workflow = WORKFLOW.read_text(encoding="utf-8")
        self.assertIn("package-ipados:", workflow)
        self.assertIn("-DCMAKE_SYSTEM_NAME=iOS", workflow)
        self.assertIn("github.com/madler/zlib.git", workflow)
        self.assertIn("github.com/pnggroup/libpng.git", workflow)
        self.assertIn("-DZLIB_ROOT=\"$IOS_PREFIX\"", workflow)
        self.assertIn("-DSDL3_DIR=\"$IOS_PREFIX/lib/cmake/SDL3\"", workflow)

    def test_ios_bundle_has_an_install_destination(self):
        cmake = (ROOT / "CMakeLists.txt").read_text(encoding="utf-8")
        self.assertIn('install(TARGETS project-eon BUNDLE DESTINATION ".")', cmake)

    def test_ios_resource_locations_match_runtime_lookups(self):
        main = (ROOT / "src" / "main.cpp").read_text(encoding="utf-8")
        i18n = (ROOT / "src" / "i18n.cpp").read_text(encoding="utf-8")
        self.assertIn('base / "Resources" / "assets" / "cards"', main)
        self.assertIn('executable_directory / "Resources" / "po"', i18n)

    def test_creates_payload_with_relative_output(self):
        with tempfile.TemporaryDirectory() as temporary:
            root = pathlib.Path(temporary)
            app = root / "ProjectEon.app"
            app.mkdir()
            (app / "ProjectEon").touch()
            for resource in ("Resources/assets/cards/millennium.png",
                             "Resources/assets/cards/deuteros.png",
                             "Resources/po/sv.po", "Resources/po/en_GB.po"):
                path = app / resource
                path.parent.mkdir(parents=True, exist_ok=True)
                path.touch()
            subprocess.run(["bash", str(SCRIPT), str(app), "project-eon.ipa"],
                           cwd=root, check=True)
            ipa = root / "project-eon.ipa"
            self.assertTrue(ipa.is_file())
            listing = subprocess.check_output(["unzip", "-l", str(ipa)], text=True)
            self.assertIn("Payload/ProjectEon.app/ProjectEon", listing)
            self.assertIn("Payload/ProjectEon.app/Resources/assets/cards/millennium.png", listing)
            self.assertIn("Payload/ProjectEon.app/Resources/po/sv.po", listing)

    def test_rejects_incomplete_bundle(self):
        with tempfile.TemporaryDirectory() as temporary:
            root = pathlib.Path(temporary)
            app = root / "ProjectEon.app"
            app.mkdir()
            result = subprocess.run(["bash", str(SCRIPT), str(app), str(root / "bad.ipa")],
                                    capture_output=True, text=True)
            self.assertNotEqual(result.returncode, 0)
            self.assertIn("incomplete iPad application", result.stderr)

    def test_rejects_original_media(self):
        with tempfile.TemporaryDirectory() as temporary:
            root = pathlib.Path(temporary)
            app = root / "ProjectEon.app"
            data = app / "data"
            data.mkdir(parents=True)
            (data / "original.adf").touch()
            result = subprocess.run(["bash", str(SCRIPT), str(app), str(root / "bad.ipa")],
                                    capture_output=True, text=True)
            self.assertNotEqual(result.returncode, 0)
            self.assertIn("refusing to package", result.stderr)


if __name__ == "__main__":
    unittest.main()
