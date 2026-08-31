import json
from pathlib import Path
import subprocess
import sys
import unittest

ROOT = Path(__file__).resolve().parents[1]

class DisassemblyInventoryTests(unittest.TestCase):
    def test_status_document_describes_external_report_verifier(self):
        status = (ROOT / "docs/DISASSEMBLY_STATUS.md").read_text(encoding="utf-8")
        self.assertIn("tools/verify_disassembly_reports.py", status)
        self.assertIn("reachability, code/data classification", status)

    def test_mill22a_candidate_report_remains_file_relative_and_unproven(self):
        tool = (ROOT / "tools" / "analyze_atari_st_config.py").read_text(encoding="utf-8")
        status = (ROOT / "docs" / "DISASSEMBLY_STATUS.md").read_text(encoding="utf-8")
        self.assertIn("MILL22A.INF", tool)
        self.assertIn("output must not already exist", tool)
        self.assertIn("output must be outside /tmp", tool)
        self.assertIn("output must be outside the repository", tool)
        self.assertIn("file-image-relative, unrelocated", tool)
        self.assertIn("MILL22A.INF", status)

    def test_mill22a_tool_rejects_checkout_and_system_scratch_before_media_read(self):
        tool = ROOT / "tools" / "analyze_atari_st_config.py"
        arguments = (
            "--archive", "missing.zip", "--nested-member", "missing-inner.zip",
            "--disk-member", "missing.st", "--disk-sha256", "0" * 64,
            "--file-sha256", "1" * 64,
        )
        for output, expected in (
            (ROOT / "forbidden-report.md", "output must be outside the repository"),
            (Path("/tmp") / "forbidden-project-eon-report.md", "output must be outside /tmp"),
        ):
            with self.subTest(output=output):
                completed = subprocess.run(
                    (sys.executable, str(tool), *arguments, "--output", str(output)),
                    cwd=ROOT, check=False, capture_output=True, text=True,
                )
                self.assertNotEqual(completed.returncode, 0)
                self.assertIn(expected, completed.stderr)

    def test_inventory_covers_exact_releases_with_bounded_profiles(self):
        inventory = json.loads((ROOT / "docs/disassembly-inventory.json").read_text())
        manifest = json.loads((ROOT / "docs/release-manifest.json").read_text())
        self.assertEqual(inventory["schema"], "project-eon.disassembly-inventory/v2")
        releases = {row["sha256"] for row in manifest["releases"]}
        profiles = {row["id"]: row for row in manifest["parser_profiles"]}
        self.assertEqual({row["release_sha256"] for row in inventory["releases"]}, releases)
        for row in inventory["releases"]:
            self.assertIn(row["cpu"], {"i8086", "m68000"})
            self.assertTrue(row["unresolved"])
            self.assertTrue(row["coverage"])
            self.assertIn(row["start_profile_id"], row["coverage"])
            self.assertEqual(profiles[row["start_profile_id"]]["release_sha256"], row["release_sha256"])
            for profile_id in row["coverage"]:
                self.assertEqual(profiles[profile_id]["release_sha256"], row["release_sha256"])
            for span in row.get("static_spans", []):
                self.assertIn(span["cpu"], {"i8086", "m68000"})
                self.assertEqual(span["cpu"], row["cpu"])
                self.assertEqual(span["coverage_kind"], "linear-candidate-unclassified")
                self.assertRegex(span["leaf_sha256"], r"^[0-9a-f]{64}$")
                self.assertRegex(span["report_sha256"], r"^[0-9a-f]{64}$")
                self.assertGreater(span["report_lines"], 0)
                self.assertTrue(span["boundary"])
                self.assertTrue(span["segments"])
                known_leaf = any(profile["release_sha256"] == row["release_sha256"]
                                 and profile["leaf_sha256"] == span["leaf_sha256"]
                                 for profile in profiles.values())
                # A FAT12 member can be hash-addressed by the static ledger
                # without becoming a runtime parser-profile leaf. In that
                # case its containing disk profile must be named explicitly;
                # otherwise a report could silently detach from its original
                # image provenance.
                if not known_leaf:
                    self.assertIn("source_provenance_profile_id", span)
                    provenance = profiles[span["source_provenance_profile_id"]]
                    self.assertEqual(provenance["release_sha256"], row["release_sha256"])
                previous_end = -1
                for segment in span["segments"]:
                    self.assertGreaterEqual(segment["source_offset"], 0)
                    self.assertGreater(segment["length"], 0)
                    self.assertGreaterEqual(segment["runtime_address"], 0)
                    address_space = segment.get("address_space", "runtime")
                    self.assertIn(address_space, {"runtime", "image-relative-unrelocated"})
                    entry_kinds = {"entry_address", "entry_offset", "entry_status"} & segment.keys()
                    self.assertEqual(len(entry_kinds), 1)
                    if "entry_address" in entry_kinds:
                        self.assertEqual(address_space, "runtime")
                        self.assertGreaterEqual(segment["entry_address"], segment["runtime_address"])
                        self.assertLess(segment["entry_address"],
                                        segment["runtime_address"] + segment["length"])
                    elif "entry_offset" in entry_kinds:
                        self.assertGreaterEqual(segment["entry_offset"], 0)
                        self.assertLess(segment["entry_offset"], segment["length"])
                        if address_space == "image-relative-unrelocated":
                            self.assertEqual(segment["runtime_address"], 0)
                    else:
                        self.assertEqual(segment["entry_status"], "unproven")
                    self.assertGreaterEqual(segment["source_offset"], previous_end)
                    previous_end = segment["source_offset"] + segment["length"]

if __name__ == "__main__":
    unittest.main()
