import hashlib
from io import BytesIO
from pathlib import Path
import struct
import unittest
from zipfile import ZipFile

from eon_test_paths import temporary_directory
from tools import analyze_atari_st_stx_prg as tool


def set_fat12(fat: bytearray, cluster: int, value: int) -> None:
    offset = cluster + cluster // 2
    if cluster & 1:
        fat[offset] = (fat[offset] & 0x0f) | ((value << 4) & 0xf0)
        fat[offset + 1] = (value >> 4) & 0xff
    else:
        fat[offset] = value & 0xff
        fat[offset + 1] = (fat[offset + 1] & 0xf0) | ((value >> 8) & 0x0f)


def make_stx(file_size: int = 34, divergent_chain: bool = False) -> tuple[bytes, bytes]:
    text_size = file_size - 32
    program = bytearray(b"\x60\x1a" + text_size.to_bytes(4, "big") + bytes(22))
    program.extend((b"\x4e\x75" * ((text_size + 1) // 2))[:text_size])
    program.extend(bytes(4))  # Empty, terminated Atari PRG relocation stream.
    sectors = [bytearray(512) for _ in range(20)]
    boot = sectors[0]
    boot[11:13] = (512).to_bytes(2, "little")
    boot[13] = 1
    boot[14:16] = (1).to_bytes(2, "little")
    boot[16] = 2
    boot[17:19] = (16).to_bytes(2, "little")
    boot[19:21] = (20).to_bytes(2, "little")
    boot[22:24] = (1).to_bytes(2, "little")
    boot[24:26] = (10).to_bytes(2, "little")
    boot[26:28] = (1).to_bytes(2, "little")

    fat_a = bytearray(512)
    fat_b = bytearray(512)
    fat_a[:3] = fat_b[:3] = b"\xf0\xff\xff"
    set_fat12(fat_a, 2, 3 if file_size > 512 else 0xfff)
    set_fat12(fat_a, 3, 0xfff)
    set_fat12(fat_b, 2, (4 if divergent_chain else 3) if file_size > 512 else 0xfff)
    set_fat12(fat_b, 3, 0xfff)
    set_fat12(fat_b, 8, 0xfff)  # Deliberate unrelated mirror difference.
    sectors[1][:] = fat_a
    sectors[2][:] = fat_b

    entry = sectors[3]
    entry[:11] = b"EXEC    TOS"
    entry[11] = 0x20
    entry[26:28] = (2).to_bytes(2, "little")
    entry[28:32] = len(program).to_bytes(4, "little")
    sectors[4][:len(program[:512])] = program[:512]
    if len(program) > 512:
        sectors[5][:len(program[512:])] = program[512:]
        if divergent_chain:
            sectors[6][:len(program[512:])] = bytes([0xa5]) * len(program[512:])

    result = bytearray(b"RSY\0" + struct.pack("<H", 3) + bytes(4) + b"\x02" + bytes(5))
    for track in range(2):
        block_size = 16 + 10 * 16 + 10 * 512
        header = bytearray(16)
        struct.pack_into("<I", header, 0, block_size)
        struct.pack_into("<H", header, 8, 10)
        header[14] = track
        descriptors = bytearray()
        for index in range(10):
            descriptor = bytearray(16)
            struct.pack_into("<I", descriptor, 0, index * 512)
            descriptor[8:12] = bytes((track, 0, index + 1, 2))
            descriptors.extend(descriptor)
        result.extend(header)
        result.extend(descriptors)
        for index in range(10):
            result.extend(sectors[track * 10 + index])
    return bytes(result), bytes(program)


class AnalyzeAtariStStxPrgTests(unittest.TestCase):
    def test_retains_prg_relocations_without_applying_load_base(self):
        text = bytearray(0x20)
        text[2:6] = (0x1c).to_bytes(4, "big")
        text[0x1c:0x20] = (0xae).to_bytes(4, "big")
        program = (b"\x60\x1a" + len(text).to_bytes(4, "big") + bytes(22)
                   + text + (2).to_bytes(4, "big") + b"\x1a\0")
        self.assertEqual(tool.parse_relocations(program, len(text), 0, 0),
                         [(2, 0x1c), (0x1c, 0xae)])

    def test_admits_file_when_fat_mirrors_differ_elsewhere(self):
        image, program = make_stx()
        self.assertEqual(tool.stx_fat12_member(image, "EXEC.TOS"), program)

    def test_rejects_file_when_mirror_chains_or_payloads_differ(self):
        image, _ = make_stx(file_size=600, divergent_chain=True)
        with self.assertRaisesRegex(tool.StxAnalysisError, "FAT mirrors disagree"):
            tool.stx_fat12_member(image, "EXEC.TOS")

    def test_hash_bound_archive_produces_external_candidate_listing(self):
        with temporary_directory() as temporary:
            root = Path(temporary)
            image, program = make_stx()
            archive = root / "physical.zip"
            with ZipFile(archive, "w") as stream:
                stream.writestr("physical.stx", image)
            output = root / "exec-tos.md"
            result = tool.main([
                "--archive", str(archive),
                "--archive-sha256", hashlib.sha256(archive.read_bytes()).hexdigest(),
                "--disk-member", "physical.stx",
                "--disk-sha256", hashlib.sha256(image).hexdigest(),
                "--program", "EXEC.TOS",
                "--program-sha256", hashlib.sha256(program).hexdigest(),
                "--output", str(output),
            ])
            self.assertEqual(result, 0)
            report = output.read_text(encoding="utf-8")
            self.assertIn("both original FAT copies", report)
            self.assertIn("PRG image-relative", report)
            self.assertIn("Relocations: none", report)
            self.assertIn("rts", report.lower())


if __name__ == "__main__":
    unittest.main()
