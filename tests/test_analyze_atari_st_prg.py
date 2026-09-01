import hashlib
from io import BytesIO
from pathlib import Path
import unittest
from zipfile import ZipFile

from eon_test_paths import temporary_directory
from tools import analyze_atari_st_prg as tool
from tools.extract_static_control_flow import main as extract_static_control_flow


def fixture_disk() -> tuple[bytes, bytes]:
    """Return a minimal FAT12 disk and its one rooted Atari PRG.

    This is deliberately synthetic test transport only. It proves that the
    tool's container and hash gates work before it is used on user media.
    """
    disk = bytearray(16 * 512)
    disk[11:13] = (512).to_bytes(2, "little")
    disk[13] = 1
    disk[14:16] = (1).to_bytes(2, "little")
    disk[16] = 1
    disk[17:19] = (16).to_bytes(2, "little")
    disk[19:21] = (16).to_bytes(2, "little")
    disk[22:24] = (1).to_bytes(2, "little")
    program = b"\x60\x1a" + (2).to_bytes(4, "big") + bytes(20) + b"\x00\x01" + b"\x4e\x75"
    root = 2 * 512
    disk[root:root + 8] = b"TEST    "
    disk[root + 8:root + 11] = b"PRG"
    disk[root + 26:root + 28] = (2).to_bytes(2, "little")
    disk[root + 28:root + 32] = len(program).to_bytes(4, "little")
    disk[3 * 512:3 * 512 + len(program)] = program
    return bytes(disk), program


class AnalyzeAtariStPrgTests(unittest.TestCase):
    def _arguments(self, archive: Path, archive_hash: str, output: Path) -> list[str]:
        disk, program = fixture_disk()
        return ["--archive", str(archive), "--archive-sha256", archive_hash,
                "--disk-member", "fixture.st", "--program", "TEST.PRG",
                "--disk-sha256", hashlib.sha256(disk).hexdigest(),
                "--program-sha256", hashlib.sha256(program).hexdigest(),
                "--output", str(output)]

    def test_direct_container_is_hash_bound_before_root_prg_read(self):
        with temporary_directory() as temporary:
            root = Path(temporary)
            disk, _ = fixture_disk()
            archive = root / "direct.zip"
            with ZipFile(archive, "w") as stream:
                stream.writestr("fixture.st", disk)
            output = root / "direct.md"
            self.assertEqual(tool.main(self._arguments(
                archive, hashlib.sha256(archive.read_bytes()).hexdigest(), output)), 0)
            report = output.read_text(encoding="utf-8")
            self.assertIn("direct.zip!fixture.st:TEST.PRG", report)

    def test_nested_carrier_authenticates_both_container_byte_streams(self):
        with temporary_directory() as temporary:
            root = Path(temporary)
            disk, _ = fixture_disk()
            nested_bytes = BytesIO()
            with ZipFile(nested_bytes, "w") as nested:
                nested.writestr("fixture.st", disk)
            carrier = root / "carrier.zip"
            with ZipFile(carrier, "w") as outer:
                outer.writestr("inner.zip", nested_bytes.getvalue())
            output = root / "nested.md"
            arguments = self._arguments(
                carrier, hashlib.sha256(carrier.read_bytes()).hexdigest(), output)
            arguments[6:6] = ["--nested-member", "inner.zip", "--nested-sha256",
                                hashlib.sha256(nested_bytes.getvalue()).hexdigest()]
            self.assertEqual(tool.main(arguments), 0)
            report = output.read_text(encoding="utf-8")
            self.assertIn("carrier.zip!inner.zip!fixture.st:TEST.PRG", report)

    def test_wrong_outer_hash_and_repository_output_are_rejected(self):
        with temporary_directory() as temporary:
            root = Path(temporary)
            disk, _ = fixture_disk()
            archive = root / "direct.zip"
            with ZipFile(archive, "w") as stream:
                stream.writestr("fixture.st", disk)
            with self.assertRaises(SystemExit):
                tool.main(self._arguments(archive, "0" * 64, root / "wrong.md"))
            with self.assertRaises(SystemExit):
                tool.main(self._arguments(
                    archive, hashlib.sha256(archive.read_bytes()).hexdigest(),
                    Path.cwd() / "forbidden-prg-report.md"))

    def test_static_flow_atari_carrier_requires_and_records_nested_identity(self):
        with temporary_directory() as temporary:
            root = Path(temporary)
            disk, program = fixture_disk()
            nested_bytes = BytesIO()
            with ZipFile(nested_bytes, "w") as nested:
                nested.writestr("fixture.st", disk)
            carrier = root / "carrier.zip"
            with ZipFile(carrier, "w") as outer:
                outer.writestr("inner.zip", nested_bytes.getvalue())
            output = root / "flow.json"
            base = ["--atari-prg-archive", str(carrier), "--archive-sha256",
                    hashlib.sha256(carrier.read_bytes()).hexdigest(), "--nested-member", "inner.zip",
                    "--disk-member", "fixture.st", "--program", "TEST.PRG", "--source-sha256",
                    hashlib.sha256(disk).hexdigest(), "--program-sha256",
                    hashlib.sha256(program).hexdigest(), "--range", "28:2:0:"
                    + hashlib.sha256(program[28:30]).hexdigest(), "--output", str(output)]
            self.assertEqual(extract_static_control_flow(base), 2)
            nested_hash = hashlib.sha256(nested_bytes.getvalue()).hexdigest()
            base[6:6] = ["--nested-sha256", nested_hash]
            self.assertEqual(extract_static_control_flow(base), 0)
            import json
            document = json.loads(output.read_text(encoding="utf-8"))["documents"][0]
            self.assertEqual(document["archive_sha256"], hashlib.sha256(carrier.read_bytes()).hexdigest())
            self.assertEqual(document["container_sha256"], hashlib.sha256(disk).hexdigest())


if __name__ == "__main__":
    unittest.main()
