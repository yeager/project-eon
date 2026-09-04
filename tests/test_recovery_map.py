import json
from pathlib import Path
import re
import unittest


ROOT = Path(__file__).resolve().parents[1]


class RecoveryMapTests(unittest.TestCase):
    def test_function_map_is_hash_bound_to_existing_profiles_and_compiled_rows(self):
        release_manifest = json.loads((ROOT / "docs" / "release-manifest.json").read_text(encoding="utf-8"))
        function_map = json.loads((ROOT / "docs" / "function-map.json").read_text(encoding="utf-8"))
        self.assertEqual(function_map["schema"], "project-eon.function-map/v1")
        self.assertIn("not a hook table", function_map["purpose"])
        releases = {release["sha256"] for release in release_manifest["releases"]}
        profile_rows = {
            (profile["release_sha256"], profile["id"]): profile
            for profile in release_manifest["parser_profiles"]
        }
        profiles = set(profile_rows)
        entries = function_map["entries"]
        self.assertEqual(len({entry["id"] for entry in entries}), len(entries))
        headings = re.findall(r"^#{2,6}\s+(.+)$",
                              (ROOT / "docs" / "PRESERVATION.md").read_text(encoding="utf-8"), re.MULTILINE)
        anchors = {re.sub(r"[^a-z0-9 -]", "", heading.lower()).replace(" ", "-")
                   for heading in headings}
        for entry in entries:
            with self.subTest(entry=entry["id"]):
                self.assertIn(entry["release_sha256"], releases)
                self.assertIn((entry["release_sha256"], entry["parser_profile_id"]), profiles)
                self.assertEqual(entry["evidence_level"], "verified-static")
                self.assertRegex(entry["source_asset_sha256"], r"^[0-9a-f]{64}$")
                if "source_span_sha256" in entry:
                    self.assertRegex(entry["source_span_sha256"], r"^[0-9a-f]{64}$")
                profile = profile_rows[(entry["release_sha256"], entry["parser_profile_id"])]
                # A full-leaf parser profile has only one possible span hash.
                # Catch accidental insertion of a local callee-prefix hash
                # here, before runtime admission needs real media to expose it.
                if profile["offset"] == 0 and profile["length"] == profile["leaf_size"]:
                    self.assertEqual(
                        entry.get("source_span_sha256", entry["source_asset_sha256"]),
                        profile["leaf_sha256"],
                    )
                self.assertTrue(entry["source_offset"])
                address_space = entry.get("address_space", "runtime")
                self.assertIn(address_space, {"runtime", "image-relative-unrelocated"})
                self.assertTrue(entry["runtime_address"].startswith("$")
                                if address_space == "runtime"
                                else entry["runtime_address"].startswith("+0x"))
                self.assertTrue(entry["uncertainty"])
                self.assertTrue(entry["runtime_status"])
                self.assertIn(entry["documentation_anchor"].split("#", 1)[1], anchors)

        source = (ROOT / "src" / "data" / "function_map.cpp").read_text(encoding="utf-8")
        for entry in entries:
            with self.subTest(compiled=entry["id"]):
                for field in ("id", "release_sha256", "parser_profile_id", "source_asset_sha256",
                              "source_offset", "runtime_address", "uncertainty", "runtime_status"):
                    self.assertIn(f'"{entry[field]}"', source)
                if "source_span_sha256" in entry:
                    self.assertIn(f'"{entry["source_span_sha256"]}"', source)

    def test_compiled_function_map_exactly_matches_json(self):
        function_map = json.loads((ROOT / "docs" / "function-map.json").read_text(encoding="utf-8"))
        source = (ROOT / "src" / "data" / "function_map.cpp").read_text(encoding="utf-8")
        rows = re.findall(
            r'\{"([^"]+)", "([0-9a-f]{64})",\s+'
            r'"([^"]+)", Game::(deuteros|millennium), Platform::(amiga|atari_st|dos), "([a-z]+)", "([a-z0-9]+)",\s+'
            r'"([0-9a-f]{64})",\s+"([^"]+)", "([^"]+)", "([a-z-]+)",\s+'
            r'"([^"]+)",\s+"([^"]+)", "([^"]+)"(?:,\s+"([0-9a-f]{64})")?(?:,\s+"([^"]+)")?\}',
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
                "source_asset_sha256": source_asset_sha256,
                "source_span_sha256": source_span_sha256 or "",
                "source_offset": source_offset,
                "runtime_address": runtime_address,
                "evidence_level": evidence_level,
                "uncertainty": uncertainty,
                "runtime_status": runtime_status,
                "documentation_anchor": documentation_anchor,
                "address_space": address_space or "runtime",
            }
            for (entry_id, release_sha256, parser_profile_id, game, platform, language, cpu,
                 source_asset_sha256, source_offset, runtime_address, evidence_level, uncertainty,
                 runtime_status, documentation_anchor, source_span_sha256, address_space) in rows
        ]
        expected = [
            {**entry, "source_span_sha256": entry.get("source_span_sha256", entry["source_asset_sha256"]),
             "address_space": entry.get("address_space", "runtime")}
            for entry in function_map["entries"]
        ]
        self.assertEqual(compiled, expected)

    def test_map_is_hash_bound_to_existing_profiles(self):
        release_manifest = json.loads((ROOT / "docs" / "release-manifest.json").read_text(encoding="utf-8"))
        recovery_map = json.loads((ROOT / "docs" / "recovery-map.json").read_text(encoding="utf-8"))
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
        # Recovery-map coverage is intentionally one-to-one with bounded
        # parser profiles: a new parser cannot silently be left without a
        # hash-bound source location and preservation-document anchor.
        mapped_profiles = [
            (entry["release_sha256"], entry["parser_profile_id"])
            for entry in entries
        ]
        self.assertEqual(len(mapped_profiles), len(set(mapped_profiles)))
        self.assertEqual(set(mapped_profiles), profiles)
        headings = re.findall(r"^#{2,6}\s+(.+)$",
                              (ROOT / "docs" / "PRESERVATION.md").read_text(encoding="utf-8"), re.MULTILINE)
        anchors = {
            re.sub(r"[^a-z0-9 -]", "", heading.lower()).replace(" ", "-")
            for heading in headings
        }
        for entry in entries:
            self.assertIn(entry["release_sha256"], releases)
            self.assertIn((entry["release_sha256"], entry["parser_profile_id"]), profiles)
            self.assertIn(entry["cpu"], {"m68000", "i8086"})
            self.assertEqual(entry["evidence_level"], "verified-static")
            expected_runtime_status = (
                "trace-gated sparse GX startup session"
                if entry["parser_profile_id"] == "millennium-dos-gx-overlay"
                else "read-only parser and diagnostics"
            )
            self.assertEqual(entry["runtime_status"], expected_runtime_status)
            self.assertTrue(entry["documentation_anchor"].startswith("PRESERVATION.md#"))
            self.assertIn(entry["documentation_anchor"].split("#", 1)[1], anchors)

    def test_compiled_map_exactly_matches_json(self):
        recovery_map = json.loads((ROOT / "docs" / "recovery-map.json").read_text(encoding="utf-8"))
        source = (ROOT / "src" / "data" / "recovery_map.cpp").read_text(encoding="utf-8")
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
