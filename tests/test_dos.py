import unittest

from eon.dos import ascii_strings, initial_near_jump, interrupt_counts


class DosAnalysisTests(unittest.TestCase):
    def test_entry_jump_after_segment_setup(self):
        # At file offset 4, E9 + 0x10 transfers to file offset 0x17.
        jump = initial_near_jump(b"\x0e\x1f\x0e\x07\xe9\x10\x00" + bytes(32))
        self.assertEqual(jump.file_offset, 0x17)
        self.assertEqual(jump.load_address, 0x117)

    def test_entry_jump_wraps_like_8086_ip(self):
        jump = initial_near_jump(b"\x0e\x1f\x0e\x07\xe9\xa9\xd1")
        self.assertEqual(jump.file_offset, 0xD1B0)
        self.assertEqual(jump.load_address, 0xD2B0)

    def test_interrupt_inventory(self):
        self.assertEqual(interrupt_counts(b"\xcd\x21\x90\xcd\x91\xcd\x21"), {0x21: 2, 0x91: 1})

    def test_ascii_strings_include_offsets(self):
        self.assertEqual(ascii_strings(b"\0HELLO\0xx\0", 4), [(1, "HELLO")])


if __name__ == "__main__":
    unittest.main()
