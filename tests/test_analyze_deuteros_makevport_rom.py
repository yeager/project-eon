import unittest
from pathlib import Path
from zipfile import ZIP_STORED, ZipFile

from tools.analyze_deuteros_makevport_rom import (
    AnalysisError,
    ROM_BASE,
    ROM_SIZE,
    SPANS,
    disassemble_report,
    read_verified_rom,
)
from eon_test_paths import temporary_directory


class DeuterosMakeVPortRomAnalysisTests(unittest.TestCase):
    def test_pinned_spans_are_bounded_and_nonoverlapping(self):
        ordered = sorted(SPANS, key=lambda item: item[1])
        for name, start, end, digest in ordered:
            with self.subTest(name=name):
                self.assertGreater(start, ROM_BASE)
                self.assertGreater(end, start)
                self.assertLessEqual(end - ROM_BASE, ROM_SIZE)
                self.assertEqual(len(digest), 64)
        for left, right in zip(ordered, ordered[1:]):
            self.assertLessEqual(left[2], right[1])

    def test_unverified_rom_bytes_are_rejected_before_disassembly(self):
        with self.assertRaisesRegex(AnalysisError, "vector chain mismatch"):
            disassemble_report(bytes(ROM_SIZE))

    def test_archive_hash_is_required_before_rom_member_read(self):
        with temporary_directory() as temporary:
            archive_path = Path(temporary) / "foreign.zip"
            with ZipFile(archive_path, "w", compression=ZIP_STORED) as archive:
                archive.writestr("foreign.rom", b"not a Kickstart ROM")
            with self.assertRaisesRegex(AnalysisError, "archive size or SHA-256 mismatch"):
                read_verified_rom(archive_path)

    def test_supplied_kickstart_rom_reproduces_all_pinned_spans(self):
        rom_archive = (Path.home() / ".projecteon" /
            "Kickstart v1.3 r34.005 (1987-12)(Commodore)"
            "(A500-A1000-A2000-CDTV)[!].zip")
        if not rom_archive.is_file():
            self.skipTest("user-supplied Kickstart archive is unavailable")
        rom = read_verified_rom(rom_archive)
        report = disassemble_report(rom)
        self.assertIn(b"FUNCTION graphics_library_makevport", report)
        self.assertIn(b"MAKEVPORT_LVO -0x00d8", report)
        self.assertIn(b"RUNTIME 0xfc63bc..0xfc63cc", report)
        self.assertIn(b"RUNTIME 0xfcc678..0xfccff4", report)


if __name__ == "__main__":
    unittest.main()
