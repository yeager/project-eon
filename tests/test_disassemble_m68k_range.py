import hashlib
from pathlib import Path
import tempfile
import unittest
from zipfile import ZIP_STORED, ZipFile

from tools.disassemble_m68k_range import disassemble, read_exact_member, require_external_output


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

    def test_direct_zip_member_requires_outer_and_disk_hashes(self):
        cache = Path("/home/yeager/.cache/project-eon-tools/tests")
        cache.mkdir(parents=True, exist_ok=True)
        with tempfile.TemporaryDirectory(dir=cache) as temporary:
            archive = Path(temporary) / "direct.zip"
            disk = b"\x4e\x75\x00\x00"
            with ZipFile(archive, "w", compression=ZIP_STORED) as output:
                output.writestr("exact.st", disk)
            outer_digest = hashlib.sha256(archive.read_bytes()).hexdigest()
            disk_digest = hashlib.sha256(disk).hexdigest()
            self.assertEqual(read_exact_member(archive, outer_digest, None, None,
                                               "exact.st", disk_digest), disk)
            with self.assertRaises(ValueError):
                read_exact_member(archive, outer_digest, None, None, "exact.st", "0" * 64)
            with self.assertRaises(ValueError):
                read_exact_member(archive, "0" * 64, None, None, "exact.st", disk_digest)

    def test_report_destination_rejects_checkout_and_system_scratch(self):
        root = Path(__file__).resolve().parents[1]
        with self.assertRaises(ValueError):
            require_external_output(root / "forbidden-linear-report.md")
        with self.assertRaises(ValueError):
            require_external_output(Path("/tmp/project-eon-linear-report.md"))
