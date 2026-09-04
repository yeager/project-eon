from pathlib import Path
import subprocess
import sys
import unittest

from tools.reproduce_deuteros_disassembly import identity
from eon_test_paths import temporary_directory


class DeuterosDisassemblyReproductionTests(unittest.TestCase):
    def test_script_is_directly_invocable_from_the_checkout(self):
        completed = subprocess.run(
            (sys.executable, "tools/reproduce_deuteros_disassembly.py", "--help"),
            cwd=Path(__file__).resolve().parents[1], text=True,
            capture_output=True, check=False)
        self.assertEqual(completed.returncode, 0, completed.stderr)
        self.assertIn("--atari-replicants-archive", completed.stdout)
        self.assertIn("--atari-killer-archive", completed.stdout)

    def test_external_report_identity_is_content_bound(self):
        with temporary_directory() as temporary:
            report = Path(temporary) / "deuteros-disassembly-identity-fixture.md"
            report.write_bytes(b"one\ntwo\n")
            self.assertEqual(identity(report), {
                "file": report.name,
                "sha256": "c3f9c8c283a2b1f2f1896f27a01cbe3cddc0c9d93f752e4639035a0f5b36f6e8",
                "lines": 2,
            })


if __name__ == "__main__":
    unittest.main()
