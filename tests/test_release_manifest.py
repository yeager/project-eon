import json
from pathlib import Path
import unittest


ROOT = Path(__file__).resolve().parents[1]


class ReleaseManifestTests(unittest.TestCase):
    def test_profile_spans_are_bounded_and_hash_addressed(self):
        manifest = json.loads((ROOT / "docs" / "release-manifest.json").read_text())
        self.assertEqual(manifest["schema"], "project-eon.release-manifest/v1")
        releases = {entry["sha256"]: entry for entry in manifest["releases"]}
        self.assertEqual(len(releases), 6)
        self.assertEqual(set(releases), {entry["sha256"] for entry in manifest["releases"]})
        for entry in manifest["releases"]:
            self.assertEqual(len(entry["sha256"]), 64)
            self.assertGreater(entry["size"], 0)
        profiles = manifest["parser_profiles"]
        self.assertEqual(len({profile["id"] for profile in profiles}), len(profiles))
        for profile in profiles:
            self.assertIn(profile["release_sha256"], releases)
            self.assertEqual(len(profile["leaf_sha256"]), 64)
            self.assertGreater(profile["leaf_size"], 0)
            self.assertGreaterEqual(profile["offset"], 0)
            self.assertGreater(profile["length"], 0)
            self.assertLessEqual(profile["offset"] + profile["length"], profile["leaf_size"])

    def test_manifest_explicitly_keeps_variant_profiles_separate(self):
        manifest = json.loads((ROOT / "docs" / "release-manifest.json").read_text())
        profiles = {profile["id"]: profile for profile in manifest["parser_profiles"]}
        self.assertNotEqual(
            profiles["millennium-dos-title-flow"]["release_sha256"],
            profiles["millennium-dos-spanish-startup"]["release_sha256"],
        )
        self.assertNotEqual(
            profiles["millennium-dos-title-flow"]["leaf_sha256"],
            profiles["millennium-dos-spanish-startup"]["leaf_sha256"],
        )


if __name__ == "__main__":
    unittest.main()
