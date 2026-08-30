import hashlib
import unittest

from tools.disassemble_m68k_range import disassemble


class M68kRangeDisassemblyTests(unittest.TestCase):
    def test_complete_instruction_is_rendered_at_supplied_runtime_address(self):
        # M68000 RTS. The tool's source-range hash is intentionally a caller
        # supplied identity check; this checks that the decoded address itself
        # is not silently rebased to a file offset.
        self.assertEqual(hashlib.sha256(b"\x4e\x75").hexdigest(),
                         "1ceeabf0c6a5a30bad12cdac0e3ab015a7188a42e6aebb556aad00bb9cd693ad")
        self.assertEqual(disassemble(b"\x4e\x75", 0x1200), ["00001200  rts"])

    def test_undecodable_tail_is_preserved_as_explicit_byte(self):
        self.assertEqual(
            disassemble(b"\x4e\x75\xff", 0x1200),
            ["00001200  rts", "00001202  .byte      0xff ; code/data-unclassified"],
        )
