"""Keep CI source-built dependencies immutable and reviewable."""

from __future__ import annotations

from pathlib import Path
import unittest


WORKFLOW = (Path(__file__).resolve().parents[1] / ".github" / "workflows" / "build.yml").read_text(
    encoding="utf-8"
)


class CiDependencyPinTests(unittest.TestCase):
    def test_source_dependencies_use_immutable_object_ids(self) -> None:
        expected = {
            "SDL3_REF": "147a8ee32dbf9ac02f3794964490687b6bbda1bc",
            "ZLIB_REF": "51b7f2abdade71cd9bb0e7a373ef2610ec6f9daf",
            "LIBPNG_REF": "2b978915d82377df13fcbb1fb56660195ded868a",
        }
        for name, object_id in expected.items():
            with self.subTest(dependency=name):
                self.assertIn(f"{name}: {object_id}", WORKFLOW)
                self.assertIn(f"checkout --detach FETCH_HEAD", WORKFLOW)

    def test_workflow_has_no_mutable_dependency_clone(self) -> None:
        self.assertNotIn("git clone", WORKFLOW)
        self.assertIn('fetch --depth 1 --filter=blob:none origin "$SDL3_REF"', WORKFLOW)
        self.assertIn("fetch --depth 1 origin $env:ZLIB_REF", WORKFLOW)
        self.assertIn('fetch --depth 1 origin "$LIBPNG_REF"', WORKFLOW)


if __name__ == "__main__":
    unittest.main()
