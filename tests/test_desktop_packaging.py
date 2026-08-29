"""Static contract for the Linux artifact verifier used in CI."""

from __future__ import annotations

import os
from pathlib import Path
import subprocess
import unittest


ROOT = Path(__file__).resolve().parents[1]
VERIFIER = ROOT / "packaging" / "verify-desktop-package.sh"
WORKFLOW = ROOT / ".github" / "workflows" / "build.yml"


class DesktopPackagingTests(unittest.TestCase):
    def test_verifier_is_valid_shell_and_rejects_media_extensions(self) -> None:
        # The script itself is exercised by the Linux packaging job. Windows
        # still checks the preservation contract below, but has no POSIX shell
        # runtime to parse it with.
        if os.name != "nt":
            subprocess.run(["bash", "-n", str(VERIFIER)], check=True)
        source = VERIFIER.read_text(encoding="utf-8")
        for extension in ("zip", "adf", "st", "msa", "stx", "img", "exe", "com"):
            with self.subTest(extension=extension):
                self.assertIn(extension, source)
        self.assertIn("assets/cards/millennium.png", source)
        self.assertIn("NotoSansSC-Regular.otf", source)
        self.assertIn("OFL-1.1.txt", source)
        self.assertIn('for catalog in ar de el en_GB es fi fr hi it ja ko nl no pl pt_BR ru sv tr uk zh_CN', source)
        self.assertIn("localization catalog", source)

    def test_linux_packaging_job_runs_the_artifact_verifier(self) -> None:
        workflow = WORKFLOW.read_text(encoding="utf-8")
        self.assertIn("Verify package contents contain no game media", workflow)
        self.assertIn("bash packaging/verify-desktop-package.sh package/deb/*.deb package/rpm/*.rpm", workflow)

    def test_ci_validates_macos_archive_and_windows_runtime_stage(self) -> None:
        workflow = WORKFLOW.read_text(encoding="utf-8")
        self.assertIn('unzip -t "project-eon-macos-${{ matrix.arch }}.zip"', workflow)
        self.assertIn('cp -R assets/fonts "$APP/Contents/MacOS/assets/fonts"', workflow)
        self.assertIn('Copy-Item assets/fonts dist/assets/fonts -Recurse', workflow)
        self.assertIn("refusing macOS artifact with possible original game data", workflow)
        self.assertIn("Windows package stage lacks libpng runtime DLL", workflow)
        self.assertIn("refusing Windows package stage with possible original game data", workflow)


if __name__ == "__main__":
    unittest.main()
