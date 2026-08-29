import json
from pathlib import Path
import re
import unittest


ROOT = Path(__file__).resolve().parents[1]


class RecoveryMapTests(unittest.TestCase):
    def test_map_is_hash_bound_to_existing_profiles(self):
        release_manifest = json.loads((ROOT / "docs" / "release-manifest.json").read_text())
        recovery_map = json.loads((ROOT / "docs" / "recovery-map.json").read_text())
        self.assertEqual(recovery_map["schema"], "project-eon.recovery-map/v1")
        self.assertIn("no patches", recovery_map["purpose"])
        self.assertIn("Modern presentation", recovery_map["mode_boundary"])
        releases = {release["sha256"] for release in release_manifest["releases"]}
        profiles = {
            (profile["release_sha256"], profile["id"])
            for profile in release_manifest["parser_profiles"]
        }
        entries = recovery_map["entries"]
        self.assertEqual(len({entry["id"] for entry in entries}), len(entries))
        self.assertGreaterEqual(len(entries), 9)
        for entry in entries:
            self.assertIn(entry["release_sha256"], releases)
            self.assertIn((entry["release_sha256"], entry["parser_profile_id"]), profiles)
            self.assertIn(entry["cpu"], {"m68000", "i8086"})
            self.assertEqual(entry["evidence_level"], "verified-static")
            self.assertEqual(entry["runtime_status"], "read-only parser and diagnostics")
            self.assertTrue(entry["documentation_anchor"].startswith("PRESERVATION.md#"))

    def test_compiled_map_exactly_matches_json(self):
        recovery_map = json.loads((ROOT / "docs" / "recovery-map.json").read_text())
        source = (ROOT / "src" / "data" / "recovery_map.cpp").read_text()
        rows = re.findall(
            r'\{"([a-z0-9-]+)", "([0-9a-f]{64})", "([a-z0-9-]+)", '
            r'Game::(deuteros|millennium), Platform::(amiga|atari_st|dos), "([a-z]+)", '
            r'"([a-z0-9]+)", "([^"]+)", "([a-z-]+)", "([^"]+)", "([^"]+)"\}',
            source,
        )
        compiled = [
            {
                "id": entry_id,
                "release_sha256": release_sha256,
                "parser_profile_id": parser_profile_id,
                "game": game,
                "platform": platform,
                "language": language,
                "cpu": cpu,
                "source_address": source_address,
                "evidence_level": evidence_level,
                "runtime_status": runtime_status,
                "documentation_anchor": documentation_anchor,
            }
            for (entry_id, release_sha256, parser_profile_id, game, platform, language,
                 cpu, source_address, evidence_level, runtime_status, documentation_anchor) in rows
        ]
        self.assertEqual(compiled, recovery_map["entries"])


if __name__ == "__main__":
    unittest.main()
