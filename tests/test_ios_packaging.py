import pathlib
import os
import shutil
import subprocess
import unittest
import zipfile

from eon_test_paths import temporary_directory


ROOT = pathlib.Path(__file__).resolve().parents[1]
SCRIPT = ROOT / "packaging" / "ios" / "package-ipa.sh"
VERIFY = ROOT / "packaging" / "ios" / "verify-ipa.py"
WORKFLOW = ROOT / ".github" / "workflows" / "build.yml"
CATALOGS = ("ar", "de", "el", "en_GB", "es", "fi", "fr", "hi", "it", "ja", "ko", "nl",
            "no", "pl", "pt_BR", "ru", "sv", "tr", "uk", "zh_CN")
FONTS = ("NotoSans-Regular.ttf", "NotoSansArabic-Regular.ttf", "NotoSansDevanagari-Regular.ttf",
         "NotoSansJP-Regular.otf", "NotoSansKR-Regular.otf", "NotoSansSC-Regular.otf", "OFL-1.1.txt")
CARDS = ("millennium.png", "deuteros.png", "dos-platform-v1.png", "amiga-platform-v1.png",
         "atari-st-platform-v1.png", "original-profile-v1.png", "modern-profile-v1.png",
         "custom-profile-v1.png")
BRANDING = ("project-eon-logo-v1.png",)


@unittest.skipUnless(os.name != "nt" and shutil.which("bash") and shutil.which("unzip"),
                     "requires POSIX bash and unzip")
class IosPackagingTests(unittest.TestCase):
    @staticmethod
    def create_complete_app(root: pathlib.Path) -> pathlib.Path:
        app = root / "ProjectEon.app"
        app.mkdir()
        # A minimal arm64 MH_MAGIC_64 header is sufficient for the archive
        # verifier; it never executes test payloads.
        # Minimal but structurally valid arm64 mach_header_64: MH_EXECUTE,
        # one eight-byte load command, then its command bytes. The IPA
        # verifier never executes this fixture.
        (app / "project-eon").write_bytes(
            b"\xcf\xfa\xed\xfe\x0c\x00\x00\x01\0\0\0\0"
            + b"\x02\0\0\0\x01\0\0\0\x08\0\0\0\0\0\0\0\0\0\0\0"
            + b"\x01\0\0\0\x08\0\0\0"
        )
        (app / "Info.plist").write_text("""<?xml version=\"1.0\" encoding=\"UTF-8\"?>
<plist version=\"1.0\"><dict>
<key>CFBundleExecutable</key><string>project-eon</string>
<key>CFBundleIdentifier</key><string>com.yeager.projecteon</string>
<key>CFBundlePackageType</key><string>APPL</string>
<key>LSRequiresIPhoneOS</key><true/>
<key>UIDeviceFamily</key><array><integer>2</integer></array>
<key>UIRequiredDeviceCapabilities</key><array><string>arm64</string></array>
<key>UIFileSharingEnabled</key><true/>
<key>LSSupportsOpeningDocumentsInPlace</key><true/>
</dict></plist>""", encoding="utf-8")
        for resource in (*(f"Resources/assets/cards/{card}" for card in CARDS),
                         *(f"Resources/assets/branding/{logo}" for logo in BRANDING),
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
        self.assertIn('mkdir -p "$APP/Resources/assets/cards" "$APP/Resources/assets/branding"', workflow)
        self.assertIn('cp assets/cards/*.png "$APP/Resources/assets/cards/"', workflow)
        self.assertIn('cp assets/branding/project-eon-logo-v1.png "$APP/Resources/assets/branding/"', workflow)
        self.assertIn('cp po/{ar,de,el,en_GB,es,fi,fr,hi,it,ja,ko,nl,no,pl,pt_BR,ru,sv,tr,uk,zh_CN}.po', workflow)

    def test_ios_bundle_has_an_install_destination(self):
        cmake = (ROOT / "CMakeLists.txt").read_text(encoding="utf-8")
        self.assertIn('install(TARGETS project-eon BUNDLE DESTINATION ".")', cmake)
        self.assertIn('set(BUILD_SHARED_LIBS OFF CACHE BOOL "" FORCE)', cmake)
        self.assertIn('set(CMAKE_OSX_DEPLOYMENT_TARGET "15.0" CACHE STRING "" FORCE)', cmake)

    def test_ipa_packager_uses_only_project_eon_cache_scratch(self):
        source = SCRIPT.read_text(encoding="utf-8")
        self.assertIn("EON_IPA_PACKAGE_TEST_TMPDIR", source)
        self.assertIn("project-eon-tools/ipa-packaging", source)
        self.assertIn('mktemp -d "$scratch_root/eon-ipa.XXXXXXXX"', source)
        self.assertIn("outside /tmp", source)
        self.assertNotIn("stage=$(mktemp -d)", source)

    def test_ios_resource_locations_match_runtime_lookups(self):
        main = (ROOT / "src" / "main.cpp").read_text(encoding="utf-8")
        i18n = (ROOT / "src" / "i18n.cpp").read_text(encoding="utf-8")
        self.assertIn('base / "Resources" / "assets" / directory / filename', main)
        self.assertIn('load_branding_texture(renderer, "project-eon-logo-v1.png")', main)
        self.assertIn('executable_directory / "Resources" / "po"', i18n)

    def test_ios_keeps_user_media_out_of_the_ipa_but_files_visible_at_runtime(self):
        launcher = (ROOT / "src" / "launcher.cpp").read_text(encoding="utf-8")
        plist = (ROOT / "packaging" / "ios" / "Info.plist.in").read_text(encoding="utf-8")
        verifier = VERIFY.read_text(encoding="utf-8")
        self.assertIn('std::filesystem::path(home) / "Documents" / "ProjectEon"', launcher)
        self.assertIn("UIFileSharingEnabled", plist)
        self.assertIn("LSSupportsOpeningDocumentsInPlace", plist)
        self.assertIn("Files sharing for user media", verifier)

    def test_ios_verifier_rejects_archive_and_physical_media_variants(self):
        verifier = VERIFY.read_text(encoding="utf-8")
        for suffix in (".hfe", ".ipf", ".scp", ".ctr", ".lha", ".lzh", ".lzx"):
            with self.subTest(suffix=suffix):
                self.assertIn(f'"{suffix}"', verifier)
        self.assertIn("opening user media in place", verifier)

    def test_creates_payload_with_relative_output(self):
        with temporary_directory() as temporary:
            root = pathlib.Path(temporary)
            app = self.create_complete_app(root)
            subprocess.run(["bash", str(SCRIPT), str(app), "project-eon.ipa"],
                           cwd=root, check=True)
            ipa = root / "project-eon.ipa"
            self.assertTrue(ipa.is_file())
            listing = subprocess.check_output(["unzip", "-l", str(ipa)], text=True)
            self.assertIn("Payload/ProjectEon.app/project-eon", listing)
            self.assertIn("Payload/ProjectEon.app/Resources/assets/cards/millennium.png", listing)
            self.assertIn("Payload/ProjectEon.app/Resources/assets/cards/custom-profile-v1.png", listing)
            self.assertIn("Payload/ProjectEon.app/Resources/assets/branding/project-eon-logo-v1.png", listing)
            self.assertIn("Payload/ProjectEon.app/Resources/assets/fonts/NotoSansSC-Regular.otf", listing)
            self.assertIn("Payload/ProjectEon.app/Resources/po/sv.po", listing)
            self.assertIn("Payload/ProjectEon.app/Resources/po/zh_CN.po", listing)

    def test_archive_verifier_rejects_post_package_media_injection(self):
        with temporary_directory() as temporary:
            root = pathlib.Path(temporary)
            app = self.create_complete_app(root)
            ipa = root / "project-eon.ipa"
            subprocess.run(["bash", str(SCRIPT), str(app), str(ipa)], check=True)
            with zipfile.ZipFile(ipa, "a") as archive:
                archive.writestr("Payload/ProjectEon.app/Resources/original.adf", b"real media is forbidden")
            result = subprocess.run(["python3", str(VERIFY), str(ipa)], capture_output=True, text=True)
            self.assertNotEqual(result.returncode, 0)
            self.assertIn("possible original game media", result.stderr)

    def test_archive_verifier_rejects_post_package_dynamic_framework_injection(self):
        with temporary_directory() as temporary:
            root = pathlib.Path(temporary)
            app = self.create_complete_app(root)
            ipa = root / "project-eon.ipa"
            subprocess.run(["bash", str(SCRIPT), str(app), str(ipa)], check=True)
            with zipfile.ZipFile(ipa, "a") as archive:
                archive.writestr(
                    "Payload/ProjectEon.app/Frameworks/unreviewed.dylib", b"not allowed"
                )
            result = subprocess.run(["python3", str(VERIFY), str(ipa)], capture_output=True, text=True)
            self.assertNotEqual(result.returncode, 0)
            self.assertIn("unexpected dynamic framework", result.stderr)

    def test_archive_verifier_rejects_unknown_payload_file(self):
        """A suffix denylist alone cannot prove an IPA is media-free."""
        with temporary_directory() as temporary:
            root = pathlib.Path(temporary)
            app = self.create_complete_app(root)
            ipa = root / "project-eon.ipa"
            subprocess.run(["bash", str(SCRIPT), str(app), str(ipa)], check=True)
            with zipfile.ZipFile(ipa, "a") as archive:
                archive.writestr(
                    "Payload/ProjectEon.app/Resources/review-bypass.bin",
                    b"unreviewed bytes are not an IPA resource",
                )
            result = subprocess.run(["python3", str(VERIFY), str(ipa)], capture_output=True, text=True)
            self.assertNotEqual(result.returncode, 0)
            self.assertIn("unexpected payload file", result.stderr)

    def test_archive_verifier_rejects_unknown_payload_directory(self):
        with temporary_directory() as temporary:
            root = pathlib.Path(temporary)
            app = self.create_complete_app(root)
            ipa = root / "project-eon.ipa"
            subprocess.run(["bash", str(SCRIPT), str(app), str(ipa)], check=True)
            with zipfile.ZipFile(ipa, "a") as archive:
                archive.writestr("Payload/ProjectEon.app/Resources/unreviewed/", b"")
            result = subprocess.run(["python3", str(VERIFY), str(ipa)], capture_output=True, text=True)
            self.assertNotEqual(result.returncode, 0)
            self.assertIn("unexpected directory", result.stderr)

    def test_archive_verifier_rejects_non_arm64_executable(self):
        with temporary_directory() as temporary:
            root = pathlib.Path(temporary)
            app = self.create_complete_app(root)
            (app / "project-eon").write_bytes(b"not a Mach-O")
            ipa = root / "project-eon.ipa"
            subprocess.run(["bash", str(SCRIPT), str(app), str(ipa)], check=False,
                           capture_output=True, text=True)
            self.assertFalse(ipa.exists())

    def test_archive_verifier_rejects_arm64_header_without_executable_load_commands(self):
        with temporary_directory() as temporary:
            root = pathlib.Path(temporary)
            app = self.create_complete_app(root)
            # An arm64 magic/cputype prefix alone is not a launchable binary.
            (app / "project-eon").write_bytes(b"\xcf\xfa\xed\xfe\x0c\x00\x00\x01" + b"\0" * 24)
            ipa = root / "project-eon.ipa"
            subprocess.run(["bash", str(SCRIPT), str(app), str(ipa)], check=False,
                           capture_output=True, text=True)
            self.assertFalse(ipa.exists())

    def test_archive_verifier_rejects_hidden_non_system_macho_dependency(self):
        with temporary_directory() as temporary:
            root = pathlib.Path(temporary)
            app = self.create_complete_app(root)
            # LC_LOAD_DYLIB (0x0c) with an @rpath name is a load-time
            # dependency even if an archive contains no Frameworks directory.
            # The fixture is never executed; package-ipa must reject it while
            # inspecting the finished IPA.
            library = b"@rpath/libunreviewed.dylib\0"
            command_size = 24 + ((len(library) + 7) // 8) * 8
            command = (
                b"\x0c\0\0\0" + command_size.to_bytes(4, "little")
                + (24).to_bytes(4, "little") + b"\0" * 12
                + library.ljust(command_size - 24, b"\0")
            )
            header = (
                b"\xcf\xfa\xed\xfe\x0c\0\0\x01\0\0\0\0"
                + b"\x02\0\0\0\x01\0\0\0"
                + command_size.to_bytes(4, "little") + b"\0" * 8
            )
            (app / "project-eon").write_bytes(header + command)
            ipa = root / "bad.ipa"
            result = subprocess.run(["bash", str(SCRIPT), str(app), str(ipa)],
                                    capture_output=True, text=True)
            self.assertNotEqual(result.returncode, 0)
            self.assertFalse(ipa.exists())
            self.assertIn("non-system dynamic library", result.stderr)

    def test_archive_verifier_allows_system_macho_dependency(self):
        with temporary_directory() as temporary:
            root = pathlib.Path(temporary)
            app = self.create_complete_app(root)
            # Static SDL still legitimately uses iPadOS's system UIKit
            # framework. The verifier must distinguish that runtime contract
            # from a bundle-local dependency.
            library = b"/System/Library/Frameworks/UIKit.framework/UIKit\0"
            command_size = 24 + ((len(library) + 7) // 8) * 8
            command = (
                b"\x0c\0\0\0" + command_size.to_bytes(4, "little")
                + (24).to_bytes(4, "little") + b"\0" * 12
                + library.ljust(command_size - 24, b"\0")
            )
            header = (
                b"\xcf\xfa\xed\xfe\x0c\0\0\x01\0\0\0\0"
                + b"\x02\0\0\0\x01\0\0\0"
                + command_size.to_bytes(4, "little") + b"\0" * 8
            )
            (app / "project-eon").write_bytes(header + command)
            ipa = root / "system-framework.ipa"
            result = subprocess.run(["bash", str(SCRIPT), str(app), str(ipa)],
                                    capture_output=True, text=True)
            self.assertEqual(result.returncode, 0, result.stderr)
            self.assertTrue(ipa.is_file())

    def test_rejects_incomplete_bundle(self):
        with temporary_directory() as temporary:
            root = pathlib.Path(temporary)
            app = root / "ProjectEon.app"
            app.mkdir()
            result = subprocess.run(["bash", str(SCRIPT), str(app), str(root / "bad.ipa")],
                                    capture_output=True, text=True)
            self.assertNotEqual(result.returncode, 0)
            self.assertIn("incomplete iPad application", result.stderr)

    def test_rejects_original_media(self):
        with temporary_directory() as temporary:
            root = pathlib.Path(temporary)
            app = root / "ProjectEon.app"
            data = app / "data"
            data.mkdir(parents=True)
            (data / "original.adf").touch()
            result = subprocess.run(["bash", str(SCRIPT), str(app), str(root / "bad.ipa")],
                                    capture_output=True, text=True)
            self.assertNotEqual(result.returncode, 0)
            self.assertIn("refusing to package", result.stderr)

    def test_rejects_unexpected_dynamic_framework_before_archiving(self):
        with temporary_directory() as temporary:
            root = pathlib.Path(temporary)
            app = self.create_complete_app(root)
            (app / "Frameworks").mkdir()
            result = subprocess.run(["bash", str(SCRIPT), str(app), str(root / "bad.ipa")],
                                    capture_output=True, text=True)
            self.assertNotEqual(result.returncode, 0)
            self.assertIn("unexpected dynamic framework", result.stderr)


if __name__ == "__main__":
    unittest.main()
