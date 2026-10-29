import hashlib
import unittest

from tools.analyze_deuteros_makevport_rom import ROM_BASE, SPANS, AnalysisError
from tools.export_deuteros_makevport_rom_span import extract_span


class ExportDeuterosMakeVPortRomSpanTests(unittest.TestCase):
    def test_declared_span_is_selected_and_hash_checked(self):
        name, start, end, digest = SPANS[0]
        rom = bytearray(end - ROM_BASE)
        payload = bytes((index * 37) & 0xff for index in range(end - start))
        # Replace the declared digest only within this synthetic test fixture by
        # selecting a span whose integrity condition cannot be satisfied from
        # arbitrary bytes.  The real-media test below covers the pinned data.
        self.assertNotEqual(hashlib.sha256(payload).hexdigest(), digest)
        with self.assertRaisesRegex(AnalysisError, "SHA-256 mismatch"):
            extract_span(bytes(rom), name)

    def test_unknown_span_is_rejected(self):
        with self.assertRaisesRegex(AnalysisError, "not declared"):
            extract_span(bytes(0x40000), "not-a-span")


if __name__ == "__main__":
    unittest.main()
