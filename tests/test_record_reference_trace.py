"""Pure boundary tests for the external reference-trace assembler.

The tests use temporary non-game bytes and never execute an emulator, mount
media, or retain an event stream fixture in the repository.
"""

from __future__ import annotations

import importlib.util
import hashlib
import json
from pathlib import Path
import tempfile
import unittest


ROOT = Path(__file__).resolve().parents[1]
SPEC = importlib.util.spec_from_file_location(
    "record_reference_trace", ROOT / "tools" / "record_reference_trace.py")
assert SPEC and SPEC.loader
TOOL = importlib.util.module_from_spec(SPEC)
SPEC.loader.exec_module(TOOL)


def metadata_lines(**overrides: str) -> str:
    fields = {
        "format": "project-eon-reference-trace-v1",
        "game": "millennium",
        "platform": "dos",
        "language": "en",
        "capture_start_utc": "2026-08-29T00:00:00Z",
        "capture_end_utc": "2026-08-29T00:00:01Z",
        "emulator_name": "external-emulator",
        "emulator_version": "1.0",
        "emulator_sha256": "a" * 64,
        "config_sha256": "b" * 64,
        "command_tail_sha256": "c" * 64,
        "input_timeline_sha256": "d" * 64,
    }
    fields.update(overrides)
    return "".join(f"{key}\t{value}\n" for key, value in fields.items())


class RecordReferenceTraceTests(unittest.TestCase):
    def test_metadata_has_no_assembler_owned_fields(self):
        with tempfile.TemporaryDirectory() as directory:
            path = Path(directory) / "metadata.tsv"
            path.write_text(metadata_lines(event_size="1"), encoding="utf-8")
            with self.assertRaises(TOOL.EvidenceError):
                TOOL.validate_metadata(TOOL.parse_metadata(path.resolve()), {
                    "game": "millennium", "platform": "dos", "language": "en"})

    def test_v2_adapter_cannot_cross_release_identity(self):
        with tempfile.TemporaryDirectory() as directory:
            path = Path(directory) / "metadata.tsv"
            path.write_text(metadata_lines(
                format="project-eon-reference-trace-v2",
                adapter="millennium-dos-en-startup-v1",
                platform="amiga"), encoding="utf-8")
            with self.assertRaises(TOOL.EvidenceError):
                TOOL.validate_metadata(TOOL.parse_metadata(path.resolve()), {
                "game": "millennium", "platform": "amiga", "language": "en"})

    def test_v2_adapter_requires_its_exact_outer_release_identity(self):
        with tempfile.TemporaryDirectory() as directory:
            path = Path(directory) / "metadata.tsv"
            path.write_text(metadata_lines(
                format="project-eon-reference-trace-v2",
                adapter="millennium-dos-en-startup-v1"), encoding="utf-8")
            with self.assertRaisesRegex(TOOL.EvidenceError, "exact source sha256"):
                TOOL.validate_metadata(TOOL.parse_metadata(path.resolve()), {
                    "sha256": "0" * 64, "size": 328383, "game": "millennium",
                    "platform": "dos", "language": "en"})

    def test_metadata_read_is_bounded_after_secure_open(self):
        with tempfile.TemporaryDirectory() as directory:
            path = Path(directory) / "metadata.tsv"
            path.write_bytes(b"x" * (TOOL.MAX_METADATA_SIZE + 1))
            with self.assertRaisesRegex(TOOL.EvidenceError, "exceeds"):
                TOOL.parse_metadata(path.resolve())

    def test_assembly_uses_new_directory_and_keeps_source_unchanged(self):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            source = root / "owned-release.zip"
            events = root / "external-events.log"
            metadata = root / "metadata.tsv"
            output = root / "capture"
            original = b"not-game-media; temporary boundary input"
            source.write_bytes(original)
            events.write_bytes(b"external recorder observation\n")
            metadata.write_text(metadata_lines(), encoding="utf-8")
            original_identity = TOOL.release_identity
            TOOL.release_identity = lambda digest, size: {
                "sha256": digest, "size": size, "game": "millennium",
                "platform": "dos", "language": "en"}
            try:
                result = TOOL.assemble(type("Arguments", (), {
                    "source_release": str(source.resolve()), "events": str(events.resolve()),
                    "metadata": str(metadata.resolve()), "output": str(output.resolve())})())
            finally:
                TOOL.release_identity = original_identity
            self.assertEqual(result, output.resolve())
            self.assertEqual(source.read_bytes(), original)
            self.assertEqual((output / "events.eontrace").read_bytes(), events.read_bytes())
            self.assertTrue((output / "manifest.eontrace").read_text(encoding="utf-8").endswith("\n"))
            self.assertIn('"status": "assembled-not-admitted"',
                          (output / "receipt.json").read_text(encoding="utf-8"))
            receipt = json.loads((output / "receipt.json").read_text(encoding="utf-8"))
            self.assertEqual(receipt["tool"]["sha256"],
                             hashlib.sha256((ROOT / "tools" / "record_reference_trace.py").read_bytes()).hexdigest())

    def test_rejects_symlink_and_existing_output(self):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            target = root / "target"
            target.write_bytes(b"x")
            link = root / "link"
            link.symlink_to(target)
            with self.assertRaises(TOOL.EvidenceError):
                TOOL.require_absolute_regular_file(link.resolve().parent / "link", "event stream")
            output = root / "already-there"
            output.mkdir()
            with self.assertRaises(TOOL.EvidenceError):
                TOOL.reject_output_path(target.resolve(), target.resolve(), target.resolve(), output.resolve())

    def test_rejects_event_alias_of_original_release(self):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            source = root / "source"
            metadata = root / "metadata"
            source.write_bytes(b"x")
            metadata.write_text(metadata_lines(), encoding="utf-8")
            arguments = type("Arguments", (), {
                "source_release": str(source.resolve()), "events": str(source.resolve()),
                "metadata": str(metadata.resolve()), "output": str((root / "out").resolve())})()
            with self.assertRaisesRegex(TOOL.EvidenceError, "must not be the original"):
                TOOL.assemble(arguments)


if __name__ == "__main__":
    unittest.main()
