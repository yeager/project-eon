"""Verify the reviewed, portable Noto launcher font bundle.

The test deliberately verifies files shipped by Project Eon rather than asking
fontconfig or an OS font API to supply a convenient local substitute.
"""

from __future__ import annotations

import hashlib
from pathlib import Path
import unittest


ROOT = Path(__file__).resolve().parents[1]
FONTS = ROOT / "assets" / "fonts"
EXPECTED_HASHES = {
    "NotoSans-Regular.ttf": "b85c38ecea8a7cfb39c24e395a4007474fa5a4fc864f6ee33309eb4948d232d5",
    "NotoSansArabic-Regular.ttf": "ceea25b464a656dc3b26849bab9356740401af62aedf1bfa8b7f0d9b75925b1b",
    "NotoSansDevanagari-Regular.ttf": "385e78e6359a9d88a0f243d53b1209d7548361ba2194e2b9ec779bcaa7e8949d",
    "NotoSansJP-Regular.otf": "dff723ba59d57d136764a04b9b2d03205544f7cd785a711442d6d2d085ac5073",
    "NotoSansKR-Regular.otf": "69975a0ac8472717870aefeab0a4d52739308d90856b9955313b2ad5e0148d68",
    "NotoSansSC-Regular.otf": "faa6c9df652116dde789d351359f3d7e5d2285a2b2a1f04a2d7244df706d5ea9",
}


class LauncherFontTests(unittest.TestCase):
    def test_exact_reviewed_font_set_and_hashes(self) -> None:
        actual = {path.name for path in FONTS.glob("*.*tf")}
        self.assertEqual(actual, set(EXPECTED_HASHES))
        for filename, expected_hash in EXPECTED_HASHES.items():
            with self.subTest(font=filename):
                self.assertEqual(
                    hashlib.sha256((FONTS / filename).read_bytes()).hexdigest(),
                    expected_hash,
                )

    def test_license_and_manifest_cover_every_font(self) -> None:
        license_text = (FONTS / "OFL-1.1.txt").read_text(encoding="utf-8")
        manifest = (FONTS / "README.md").read_text(encoding="utf-8")
        self.assertIn("SIL OPEN FONT LICENSE Version 1.1", license_text)
        self.assertIn("OFL-1.1", manifest)
        for filename, digest in EXPECTED_HASHES.items():
            with self.subTest(font=filename):
                self.assertIn(filename, manifest)
                self.assertIn(digest, manifest)

    def test_cmake_and_all_platform_packagers_stage_font_bundle(self) -> None:
        cmake = (ROOT / "CMakeLists.txt").read_text(encoding="utf-8")
        workflow = (ROOT / ".github" / "workflows" / "build.yml").read_text(encoding="utf-8")
        ios = (ROOT / "packaging" / "ios" / "package-ipa.sh").read_text(encoding="utf-8")
        desktop = (ROOT / "packaging" / "verify-desktop-package.sh").read_text(encoding="utf-8")
        self.assertIn('install(DIRECTORY assets/fonts DESTINATION "${CMAKE_INSTALL_BINDIR}/assets"', cmake)
        self.assertIn('MACOSX_PACKAGE_LOCATION "Resources/assets/fonts"', cmake)
        self.assertIn('cp -R assets/fonts "$APP/Contents/MacOS/assets/fonts"', workflow)
        self.assertIn('Copy-Item assets/fonts dist/assets/fonts -Recurse', workflow)
        self.assertIn('Resources/assets/fonts/$font', ios)
        self.assertIn('NotoSansSC-Regular.otf', ios)
        self.assertIn('assets/fonts/$font', desktop)
        self.assertIn('NotoSansSC-Regular.otf', desktop)

    def test_runtime_uses_only_the_complete_bundled_fallback_chain(self) -> None:
        source = (ROOT / "src" / "launcher_text.cpp").read_text(encoding="utf-8")
        main = (ROOT / "src" / "main.cpp").read_text(encoding="utf-8")
        self.assertIn("TTF_AddFallbackFont", source)
        self.assertIn("NotoSansArabic-Regular.ttf", source)
        self.assertIn("NotoSansDevanagari-Regular.ttf", source)
        self.assertIn("NotoSansSC-Regular.otf", source)
        self.assertIn("TTF_DIRECTION_RTL", source)
        self.assertIn('TTF_StringToTag("Deva")', source)
        self.assertIn("find_font_directory", main)
        self.assertIn('base / "assets" / "fonts"', main)
        self.assertIn('base / "Resources" / "assets" / "fonts"', main)
        self.assertNotIn("/usr/share/fonts", source)


if __name__ == "__main__":
    unittest.main()
