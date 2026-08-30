import unittest

from tools.analyze_dos import disassemble_linear
from tools.analyze_m68k import render_instructions


class CompleteLinearDisassemblyTests(unittest.TestCase):
    def test_x86_undecodable_tail_is_not_silently_dropped(self):
        self.assertEqual(
            disassemble_linear(b"\x90\x0f", 0x100),
            ["00100  nop", "00101  .byte    0x0f ; code/data-unclassified"],
        )

    def test_m68k_undecodable_tail_is_not_silently_dropped(self):
        from capstone import CS_ARCH_M68K, CS_MODE_BIG_ENDIAN, CS_MODE_M68K_000, Cs

        decoder = Cs(CS_ARCH_M68K, CS_MODE_BIG_ENDIAN | CS_MODE_M68K_000)
        self.assertEqual(
            render_instructions(decoder, b"\x4e\x75\xff", 0x1200, complete_linear=True),
            ["00001200  rts", "00001202  .byte      0xff ; code/data-unclassified"],
        )
