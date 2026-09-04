from pathlib import Path
import unittest

from tools.reproduce_deuteros_disassembly import identity


class DeuterosDisassemblyReproductionTests(unittest.TestCase):
    def test_external_report_identity_is_content_bound(self):
        cache = Path("/home/yeager/.cache/project-eon-tools/tests")
        cache.mkdir(parents=True, exist_ok=True)
        report = cache / "deuteros-disassembly-identity-fixture.md"
        report.write_bytes(b"one\ntwo\n")
        try:
            self.assertEqual(identity(report), {
                "file": report.name,
                "sha256": "c3f9c8c283a2b1f2f1896f27a01cbe3cddc0c9d93f752e4639035a0f5b36f6e8",
                "lines": 2,
            })
        finally:
            report.unlink(missing_ok=True)


if __name__ == "__main__":
    unittest.main()
