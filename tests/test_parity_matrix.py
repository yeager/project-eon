import json
from pathlib import Path
import unittest


ROOT = Path(__file__).resolve().parents[1]


class ParityMatrixTests(unittest.TestCase):
    def test_release_rows_exactly_cover_the_recognised_manifest(self):
        manifest = json.loads((ROOT / "docs" / "release-manifest.json").read_text(encoding="utf-8"))
        matrix = json.loads((ROOT / "docs" / "parity-matrix.json").read_text(encoding="utf-8"))
        self.assertEqual(matrix["schema"], "project-eon.parity-matrix/v1")
        release_hashes = {entry["sha256"] for entry in manifest["releases"]}
        rows = matrix["releases"]
        self.assertEqual({row["release_sha256"] for row in rows}, release_hashes)
        self.assertEqual(len(rows), len(release_hashes))
        statuses = set(matrix["states"])
        fields = {"recognition", "bootstrap", "gameplay", "input", "rendering", "audio", "saves", "completion"}
        for row in rows:
            self.assertEqual(set(row) - {"release_sha256"}, fields)
            self.assertTrue(all(row[field] in statuses for field in fields))
            self.assertEqual(row["recognition"], "verified")


if __name__ == "__main__":
    unittest.main()
