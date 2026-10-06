import unittest

from tools.scan_dos_transfer_targets import ScanError, scan_member


class DosTransferScanTests(unittest.TestCase):
    def test_finds_direct_near_call_at_linear_instruction_boundary(self):
        # CALL at 0x100 ends at 0x103; rel16 +3 targets 0x106.
        rows = scan_member("A.EXE", "a" * 64, b"\xe8\x03\x00\x90\x90\x90\x90", {0x106})
        self.assertEqual(len(rows), 1)
        self.assertEqual(rows[0]["file_offset"], 0)
        self.assertEqual(rows[0]["instruction_ip"], 0x100)
        self.assertEqual(rows[0]["target_ip"], 0x106)
        self.assertEqual(rows[0]["bytes_hex"], "e80300")

    def test_does_not_accept_call_bytes_inside_a_linear_instruction(self):
        # Starting at byte 1 would decode E8 00 00 as CALL 0x104, but byte 1
        # is the ModRM byte of the preceding SHR instruction.
        rows = scan_member("A.EXE", "a" * 64, b"\xd0\xe8\x00\x00\x00", {0x104})
        self.assertEqual(rows, [])

    def test_rejects_invalid_target_sets(self):
        with self.assertRaises(ScanError):
            scan_member("A.EXE", "a" * 64, b"\x90", set())
        with self.assertRaises(ScanError):
            scan_member("A.EXE", "a" * 64, b"\x90", {0x10000})


if __name__ == "__main__":
    unittest.main()
