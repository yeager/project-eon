import importlib.util
import json
from pathlib import Path
import unittest

ROOT = Path(__file__).resolve().parents[1]
SPEC = importlib.util.spec_from_file_location(
    "generate_disassembly_candidate_inventory",
    ROOT / "tools/generate_disassembly_candidate_inventory.py")
assert SPEC and SPEC.loader
TOOL = importlib.util.module_from_spec(SPEC)
SPEC.loader.exec_module(TOOL)


class GenerateDisassemblyCandidateInventoryTests(unittest.TestCase):
    def test_metadata_ledgers_retain_mapped_and_unmapped_candidates(self) -> None:
        releases = json.loads((ROOT / "docs/release-manifest.json").read_text())
        inventory = json.loads((ROOT / "docs/disassembly-inventory.json").read_text())
        generated = TOOL.generate(releases, inventory)
        candidates = [candidate for release in generated["releases"]
                      for candidate in release["candidates"]]
        self.assertEqual(sum(row["status"] == "mapped" for row in candidates), 13)
        self.assertEqual(sum(row["status"] == "discovered-unmapped" for row in candidates), 7)
        killer = next(row for row in candidates
                      if row["profile_id"] == "deuteros-atari-killer-boot")
        self.assertEqual(killer["status"], "mapped")
        self.assertEqual(killer["mapped_span_ids"],
                         ["deuteros-atari-killer-boot-linear"])
        self.assertEqual(killer["leaf_sha256"],
                         "5501ce3fd79c9b37cf695692a8012267db23dacd8a2cc64c0c7b7e4305971193")
        self.assertEqual((killer["source_offset"], killer["length"]), (0, 512))
        self.assertEqual(killer["code_candidate_kind"], "boot")
        self.assertEqual(killer["load_status"], "unproven")
        container = next(row for row in candidates
                         if row["profile_id"] == "millennium-atari-equinox-direct-prg-chain")
        self.assertEqual(container["status"], "discovered-unmapped")
        self.assertEqual(container["code_candidate_kind"], "container-with-mapped-members")
        self.assertTrue(container["member_span_ids"])
        self.assertFalse(container["mapped_span_ids"])
        self.assertEqual(container["load_status"], "unproven")
        atari_boot = next(row for row in candidates
                          if row["profile_id"] == "millennium-atari-equinox-direct-bootstrap")
        self.assertEqual(atari_boot["status"], "mapped")
        self.assertEqual(atari_boot["code_candidate_kind"], "boot")
        self.assertEqual(atari_boot["load_status"], "unproven")

    def test_cross_release_profile_is_rejected(self) -> None:
        releases = {"parser_profiles": [{"id": "candidate", "release_sha256": "a",
                                           "leaf_sha256": "0" * 64,
                                           "offset": 0, "length": 1}]}
        inventory = {"releases": [{"release_sha256": "b", "coverage": ["candidate"],
                                     "static_spans": []}]}
        with self.assertRaisesRegex(TOOL.CandidateError, "cross-release"):
            TOOL.generate(releases, inventory)


if __name__ == "__main__":
    unittest.main()
