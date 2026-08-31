"""Keep public Project Eon instructions portable and consistently English."""

from pathlib import Path
import unittest


ROOT = Path(__file__).resolve().parents[1]
PUBLIC_TEXT = (
    ROOT / "README.md",
    ROOT / "docs" / "PRESERVATION.md",
    ROOT / "docs" / "COMPLETION_PLAN.md",
    ROOT / "docs" / "WORK_QUEUE.md",
)


class DownloadsWordingTests(unittest.TestCase):
    def test_public_media_instructions_do_not_name_a_localized_downloads_folder(self):
        for path in PUBLIC_TEXT:
            with self.subTest(path=path.name):
                self.assertNotIn("Hämtningar", path.read_text(encoding="utf-8"))

    def test_readme_uses_the_portable_downloads_spelling(self):
        readme = (ROOT / "README.md").read_text(encoding="utf-8")
        self.assertIn("$HOME/Downloads", readme)


if __name__ == "__main__":
    unittest.main()
