import pathlib
import os
import shutil
import subprocess
import tempfile
import unittest
import zipfile


ROOT = pathlib.Path(__file__).resolve().parents[1]
SCRIPT = ROOT / "packaging" / "ios" / "package-ipa.sh"
VERIFY = ROOT / "packaging" / "ios" / "verify-ipa.py"
WORKFLOW = ROOT / ".github" / "workflows" / "build.yml"
CATALOGS = ("ar", "de", "el", "en_GB", "es", "fi", "fr", "hi", "it", "ja", "ko", "nl",
            "no", "pl", "pt_BR", "ru", "sv", "tr", "uk", "zh_CN")
FONTS = ("NotoSans-Regular.ttf", "NotoSansArabic-Regular.ttf", "NotoSansDevanagari-Regular.ttf",
         "NotoSansJP-Regular.otf", "NotoSansKR-Regular.otf", "NotoSansSC-Regular.otf", "OFL-1.1.txt")


@unittest.skipUnless(os.name != "nt" and shutil.which("bash") and shutil.which("unzip"),
                     "requires POSIX bash and unzip")
class IosPackagingTests(unittest.TestCase):
    @staticmethod
    def create_complete_app(root: pathlib.Path) -> pathlib.Path:
        app = root / "ProjectEon.app"
        app.mkdir()
        # A minimal arm64 MH_MAGIC_64 header is sufficient for the archive
        # verifier; it never executes test payloads.
        (app / "project-eon").write_bytes(b"\xcf\xfa\xed\xfe\x0c\x00\x00\x01" + b"\0" * 24)
        (app / "Info.plist").write_text("""<?xml version=\"1.0\" encoding=\"UTF-8\"?>
<plist version=\"1.0\"><dict>
<key>CFBundleExecutable</key><string>project-eon</string>
<key>CFBundleIdentifier</key><string>com.yeager.projecteon</string>
<key>CFBundlePackageType</key><string>APPL</string>
<key>LSRequiresIPhoneOS</key><true/>
<key>UIDeviceFamily</key><array><integer>2</integer></array>
<key>UIRequiredDeviceCapabilities</key><array><string>arm64</string></array>
</dict></plist>""", encoding="utf-8")
        for resource in ("Resources/assets/cards/millennium.png",
                         "Resources/assets/cards/deuteros.png",
                         *(f"Resources/assets/fonts/{font}" for font in FONTS),
                         *(f"Resources/po/{catalog}.po" for catalog in CATALOGS)):
            path = app / resource
            path.parent.mkdir(parents=True, exist_ok=True)
            # BSD zip on macOS omits empty files from recursive archives in
            # some runner images.  These fixture resources stand for actual
            # bundled files, so make the test archive portable rather than
            # depending on that host-specific empty-file behaviour.
            path.write_bytes(b"fixture resource\n")
        return app

    def test_ci_cross_compiles_png_dependencies_for_ios(self):
        workflow = WORKFLOW.read_text(encoding="utf-8")
        self.assertIn("package-ipados:", workflow)
        self.assertIn("-DCMAKE_SYSTEM_NAME=iOS", workflow)
        self.assertIn("-DCMAKE_OSX_DEPLOYMENT_TARGET=15.0", workflow)
        self.assertIn("-DSDL_SHARED=OFF -DSDL_STATIC=ON", workflow)
        self.assertIn("github.com/madler/zlib.git", workflow)
        self.assertIn("github.com/pnggroup/libpng.git", workflow)
        self.assertIn("-DZLIB_ROOT=\"$IOS_PREFIX\"", workflow)
        self.assertIn("-DSDL3_DIR=\"$IOS_PREFIX/lib/cmake/SDL3\"", workflow)
        self.assertIn("packaging/ios/verify-ipa.py", workflow)

    def test_ci_stages_reviewed_resources_when_ninja_omits_ios_bundle_sources(self):
        workflow = WORKFLOW.read_text(encoding="utf-8")
        self.assertIn('mkdir -p "$APP/Resources/assets/cards"', workflow)
        self.assertIn('cp assets/cards/millennium.png assets/cards/deuteros.png', workflow)
        self.assertIn('cp po/{ar,de,el,en_GB,es,fi,fr,hi,it,ja,ko,nl,no,pl,pt_BR,ru,sv,tr,uk,zh_CN}.po', workflow)

    def test_ios_bundle_has_an_install_destination(self):
        cmake = (ROOT / "CMakeLists.txt").read_text(encoding="utf-8")
        self.assertIn('install(TARGETS project-eon BUNDLE DESTINATION ".")', cmake)
        self.assertIn('set(BUILD_SHARED_LIBS OFF CACHE BOOL "" FORCE)', cmake)
        self.assertIn('set(CMAKE_OSX_DEPLOYMENT_TARGET "15.0" CACHE STRING "" FORCE)', cmake)

    def test_ios_resource_locations_match_runtime_lookups(self):
        main = (ROOT / "src" / "main.cpp").read_text(encoding="utf-8")
        i18n = (ROOT / "src" / "i18n.cpp").read_text(encoding="utf-8")
        self.assertIn('base / "Resources" / "assets" / "cards"', main)
        self.assertIn('executable_directory / "Resources" / "po"', i18n)

    def test_creates_payload_with_relative_output(self):
        with tempfile.TemporaryDirectory() as temporary:
            root = pathlib.Path(temporary)
            app = self.create_complete_app(root)
            subprocess.run(["bash", str(SCRIPT), str(app), "project-eon.ipa"],
                           cwd=root, check=True)
            ipa = root / "project-eon.ipa"
            self.assertTrue(ipa.is_file())
            listing = subprocess.check_output(["unzip", "-l", str(ipa)], text=True)
            self.assertIn("Payload/ProjectEon.app/project-eon", listing)
            self.assertIn("Payload/ProjectEon.app/Resources/assets/cards/millennium.png", listing)
            self.assertIn("Payload/ProjectEon.app/Resources/assets/fonts/NotoSansSC-Regular.otf", listing)
            self.assertIn("Payload/ProjectEon.app/Resources/po/sv.po", listing)
            self.assertIn("Payload/ProjectEon.app/Resources/po/zh_CN.po", listing)

    def test_archive_verifier_rejects_post_package_media_injection(self):
        with tempfile.TemporaryDirectory() as temporary:
            root = pathlib.Path(temporary)
            app = self.create_complete_app(root)
            ipa = root / "project-eon.ipa"
            subprocess.run(["bash", str(SCRIPT), str(app), str(ipa)], check=True)
            with zipfile.ZipFile(ipa, "a") as archive:
                archive.writestr("Payload/ProjectEon.app/Resources/original.adf", b"real media is forbidden")
            result = subprocess.run(["python3", str(VERIFY), str(ipa)], capture_output=True, text=True)
            self.assertNotEqual(result.returncode, 0)
            self.assertIn("possible original game media", result.stderr)

    def test_archive_verifier_rejects_non_arm64_executable(self):
        with tempfile.TemporaryDirectory() as temporary:
            root = pathlib.Path(temporary)
            app = self.create_complete_app(root)
            (app / "project-eon").write_bytes(b"not a Mach-O")
            ipa = root / "project-eon.ipa"
            subprocess.run(["bash", str(SCRIPT), str(app), str(ipa)], check=False,
                           capture_output=True, text=True)
            self.assertFalse(ipa.exists())

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
