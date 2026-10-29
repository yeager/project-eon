import argparse
import unittest

from tools.scan_atari_stx_address_literals import aligned_longword_offsets, file_spec


class AtariStxAddressLiteralTests(unittest.TestCase):
    def test_reports_only_even_offset_longwords_in_half_open_range(self):
        data = bytes.fromhex("00 07 70 01 00 07 72 01 00 07 72 02")
        self.assertEqual(aligned_longword_offsets(data, 0x77000, 0x77202), (
            (0, 0x77001),
            (4, 0x77201),
        ))

    def test_address_range_excludes_upper_bound_and_rejects_bad_ranges(self):
        data = bytes.fromhex("00 00 07 72 02")
        self.assertEqual(aligned_longword_offsets(data, 0x77000, 0x77202), ())
        with self.assertRaisesRegex(ValueError, "half-open"):
            aligned_longword_offsets(data, 0x77202, 0x77000)

    def test_file_spec_requires_name_and_sha256(self):
        self.assertEqual(file_spec("MILL22A.INF=" + "a" * 64),
            ("MILL22A.INF", "a" * 64))
        for invalid in ("MILL22A.INF", "= " + "a" * 64, "FILE=" + "z" * 64):
            with self.subTest(invalid=invalid):
                with self.assertRaises(argparse.ArgumentTypeError):
                    file_spec(invalid)


if __name__ == "__main__":
    unittest.main()
