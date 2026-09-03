"""Regression checks for the non-launchable direct-media evidence boundary."""

from pathlib import Path
import unittest


ROOT = Path(__file__).resolve().parents[1]


class DirectMediaBoundaryTests(unittest.TestCase):
    def test_scanner_uses_profile_leaf_identity_without_promoting_it_to_release(self):
        source = (ROOT / "src" / "platform" / "game_data.cpp").read_text(encoding="utf-8")
        header = (ROOT / "src" / "platform" / "game_data.hpp").read_text(encoding="utf-8")
        self.assertIn("parser_profile_manifest()", source)
        self.assertIn("is_manifest_leaf(fingerprint, size)", source)
        self.assertIn("unbound_direct_media_.push_back", source)
        self.assertIn("std::sort(unbound_direct_media_", source)
        self.assertIn("struct UnboundDirectMedia", header)
        self.assertIn("const std::vector<UnboundDirectMedia>& unbound_direct_media()", header)
        self.assertIn("DirectMediaSetManifestEntry", source)
        self.assertIn("verify_direct_set(directory, set)", source)
        self.assertIn("std::map<std::string, VerifiedReleaseMedia::DirectAssetReference>", source)
        self.assertIn("read_exact_regular_file(found->second.path, found->second.size)", source)
        self.assertIn("VerifiedReleaseMedia::borrow", source)
        self.assertIn("mutable std::map<std::string, std::vector<std::uint8_t>> borrowed_assets_", header)
        self.assertIn("return std::span<const std::uint8_t>(inserted->second)", source)
        self.assertIn("to_hex(sha256(bytes)) != expected_asset_sha256", source)
        self.assertNotIn("std::map<std::string, std::vector<std::uint8_t>> direct_assets_", header)
        self.assertIn("ReleaseMediaLayout::verified_directory", source)

    def test_documentation_forbids_leaf_to_release_substitution(self):
        documentation = (ROOT / "docs" / "PRESERVATION.md").read_text(encoding="utf-8")
        self.assertIn("### Direct-media evidence boundary", documentation)
        self.assertIn("does not create a `ReleaseArchive`", documentation)
        self.assertIn("complete-set identity", documentation)
        self.assertIn("unique unbound direct-media leaves", documentation.replace("\n", " "))
        self.assertIn("now a runtime admission source", documentation)
        self.assertIn("directory is never misrepresented as an outer archive", documentation)
        self.assertIn("retains only its immutable hash, expected size and", documentation)


if __name__ == "__main__":
    unittest.main()
