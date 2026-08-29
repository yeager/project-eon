"""Keep CI dependencies and artifact supply chain immutable and reviewable."""

from __future__ import annotations

from pathlib import Path
import re
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

    def test_actions_are_pinned_to_full_immutable_commit_ids(self) -> None:
        # Action tags, like source dependency tags, are mutable repository
        # references. Every `uses:` reference must identify a full Git object
        # ID, with its reviewed release label retained as a human comment.
        action_references = re.findall(r"^[ \t]*-?[ \t]*uses:[ \t]*([^\s#]+)(?:[ \t]+#[ \t]*(.*))?$",
                                      WORKFLOW, flags=re.MULTILINE)
        self.assertGreater(len(action_references), 0)
        expected = {
            "actions/checkout": ("d23441a48e516b6c34aea4fa41551a30e30af803", "v6.1.0"),
            "actions/upload-artifact": ("043fb46d1a93c77aae656e7c1c64a875d1fc6a0a", "v7.0.1"),
            "gitleaks/gitleaks-action": ("e0c47f4f8be36e29cdc102c57e68cb5cbf0e8d1e", "v3.0.0"),
        }
        for reference, label in action_references:
            with self.subTest(reference=reference):
                owner_and_name, object_id = reference.split("@", maxsplit=1)
                self.assertRegex(object_id, r"^[0-9a-f]{40}$")
                self.assertIn(owner_and_name, expected)
                self.assertEqual((object_id, label), expected[owner_and_name])

    def test_uploaded_artifacts_include_download_verification_manifests(self) -> None:
        for manifest in (
            "package/project-eon-linux-artifacts.json",
            "project-eon-macos-${{ matrix.arch }}-artifacts.json",
            "installer/project-eon-windows-artifacts.json",
            "project-eon-ipados-arm64-unsigned-artifacts.json",
        ):
            with self.subTest(manifest=manifest):
                self.assertIn(manifest, WORKFLOW)
        self.assertEqual(WORKFLOW.count("packaging/write-artifact-manifest.py"), 4)
        self.assertEqual(WORKFLOW.count("packaging/verify-artifact-manifest.py"), 4)
        self.assertEqual(WORKFLOW.count("--require-exact-directory"), 4)


if __name__ == "__main__":
    unittest.main()
