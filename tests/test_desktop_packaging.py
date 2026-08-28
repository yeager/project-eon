"""Static contract for the Linux artifact verifier used in CI."""

from __future__ import annotations

from pathlib import Path
import subprocess
import unittest


ROOT = Path(__file__).resolve().parents[1]
VERIFIER = ROOT / "packaging" / "verify-desktop-package.sh"
WORKFLOW = ROOT / ".github" / "workflows" / "build.yml"


class DesktopPackagingTests(unittest.TestCase):
    def test_verifier_is_valid_shell_and_rejects_media_extensions(self) -> None:
        subprocess.run(["bash", "-n", str(VERIFIER)], check=True)
        source = VERIFIER.read_text(encoding="utf-8")
        for extension in ("zip", "adf", "st", "msa", "stx", "img", "exe", "com"):
            with self.subTest(extension=extension):
                self.assertIn(extension, source)
        self.assertIn("assets/cards/millennium.png", source)
        self.assertIn("po/sv.po", source)

    def test_linux_packaging_job_runs_the_artifact_verifier(self) -> None:
        workflow = WORKFLOW.read_text(encoding="utf-8")
        self.assertIn("Verify package contents contain no game media", workflow)
        self.assertIn("bash packaging/verify-desktop-package.sh package/deb/*.deb package/rpm/*.rpm", workflow)


if __name__ == "__main__":
    unittest.main()
