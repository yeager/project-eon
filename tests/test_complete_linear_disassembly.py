import hashlib
from pathlib import Path
import tempfile
import unittest
from types import SimpleNamespace
from zipfile import ZIP_STORED, ZipFile

from tools.analyze_dos import disassemble_linear
from tools.analyze_m68k import read_source, render_instructions, require_external_output


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

    def test_m68k_direct_zip_source_requires_all_declared_hashes(self):
        cache = Path("/home/yeager/.cache/project-eon-tools/tests")
        cache.mkdir(parents=True, exist_ok=True)
        with tempfile.TemporaryDirectory(dir=cache) as temporary:
            archive = Path(temporary) / "clean.zip"
            adf = b"\0" * 32
            with ZipFile(archive, "w", compression=ZIP_STORED) as output:
                output.writestr("clean.adf", adf)
            args = SimpleNamespace(
                archive=archive, adf=None,
                archive_sha256=hashlib.sha256(archive.read_bytes()).hexdigest(),
                nested_member=None, nested_sha256=None, member="clean.adf",
                member_sha256=hashlib.sha256(adf).hexdigest(),
            )
            self.assertEqual(read_source(args), (adf, "clean.zip!clean.adf"))
            args.member_sha256 = "0" * 64
            with self.assertRaises(SystemExit):
                read_source(args)

    def test_m68k_reports_reject_checkout_and_system_scratch(self):
        root = Path(__file__).resolve().parents[1]
        with self.assertRaises(ValueError):
            require_external_output(root / "forbidden-m68k-report.md")
        with self.assertRaises(ValueError):
            require_external_output(Path("/tmp/project-eon-m68k-report.md"))
