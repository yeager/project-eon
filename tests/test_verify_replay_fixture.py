"""Contracts for opaque external replay-fixture admission."""
from __future__ import annotations

import hashlib
import importlib.util
import io
from pathlib import Path
import sys
import unittest
from contextlib import redirect_stdout
from unittest import mock

from eon_test_paths import temporary_directory


ROOT = Path(__file__).resolve().parents[1]
SPEC = importlib.util.spec_from_file_location("verify_replay_fixture", ROOT / "tools" / "verify_replay_fixture.py")
assert SPEC and SPEC.loader
TOOL = importlib.util.module_from_spec(SPEC)
SPEC.loader.exec_module(TOOL)

RELEASE = "e6e7044b25877fdf8b10d16d2f395886d9957953144ae15ca630cda9cab2a123"
RECEIPT_FIELDS = {"source_release_sha256": RELEASE, "source_release_bytes": "328383"}
RECEIPT_HASH = "a" * 64


def write_fixture(root: Path, *, kind: str = "frame", payload: bytes = b"fixture-only test bytes",
                  capture_sha256: str = "a" * 64) -> Path:
    payload_name = "checkpoint.bin"
    (root / payload_name).write_bytes(payload)
    fields = {
        "format": TOOL.FORMAT,
        "kind": kind,
        "source_release_sha256": RELEASE,
        "source_release_size": "328383",
        "capture_sha256": capture_sha256,
        "checkpoint_sequence": "1",
        "checkpoint_tick": "0",
        "payload_file": payload_name,
        "payload_sha256": hashlib.sha256(payload).hexdigest(),
        "payload_bytes": str(len(payload)),
    }
    manifest = "".join(f"{key}={value}\n" for key, value in fields.items())
    (root / TOOL.MANIFEST_NAME).write_bytes(manifest.encode("utf-8"))
    return root


class ReplayFixtureVerifierTests(unittest.TestCase):
    def test_cli_admits_fixture_only_after_capture_verifier_and_hash_match(self) -> None:
        with temporary_directory() as directory:
            root = Path(directory)
            fixture = root / "fixture"
            capture = root / "capture"
            fixture.mkdir()
            capture.mkdir()
            receipt_path = capture / "run-status.txt"
            receipt_path.write_bytes(("capture_receipt_version=22\n"
                                      f"source_release_sha256={RELEASE}\n"
                                      "source_release_bytes=328383\n").encode("ascii"))
            receipt_hash = hashlib.sha256(receipt_path.read_bytes()).hexdigest()
            write_fixture(fixture, capture_sha256=receipt_hash)

            receipt_tool = TOOL.load_capture_receipt_tool()
            with (mock.patch.object(receipt_tool, "verify") as verify_receipt,
                  mock.patch.object(TOOL, "load_capture_receipt_tool", return_value=receipt_tool),
                  mock.patch.object(sys, "argv", [
                      "verify_replay_fixture.py", "--fixture", str(fixture),
                      "--capture", str(capture), "--capture-kind", "deuteros-amiga"]),
                  redirect_stdout(io.StringIO()) as output):
                self.assertEqual(TOOL.main(), 0)
            verify_receipt.assert_called_once_with("deuteros-amiga", capture)
            self.assertIn("REPLAY FIXTURE VERIFIED  frame", output.getvalue())

    def test_cli_rejects_a_fixture_bound_to_a_different_receipt(self) -> None:
        with temporary_directory() as directory:
            root = Path(directory)
            fixture = root / "fixture"
            capture = root / "capture"
            fixture.mkdir()
            capture.mkdir()
            (capture / "run-status.txt").write_bytes(
                ("capture_receipt_version=22\n"
                 f"source_release_sha256={RELEASE}\n"
                 "source_release_bytes=328383\n").encode("ascii"))
            write_fixture(fixture)

            receipt_tool = TOOL.load_capture_receipt_tool()
            with (mock.patch.object(receipt_tool, "verify"),
                  mock.patch.object(TOOL, "load_capture_receipt_tool", return_value=receipt_tool),
                  mock.patch.object(sys, "argv", [
                      "verify_replay_fixture.py", "--fixture", str(fixture),
                      "--capture", str(capture), "--capture-kind", "millennium-dos"]),
                  redirect_stdout(io.StringIO()) as output):
                self.assertEqual(TOOL.main(), 2)
            self.assertIn("capture hash does not match", output.getvalue())

    def test_hash_bound_fixture_accepts_only_recognised_release(self) -> None:
        with temporary_directory() as directory:
            fields = TOOL.verify(write_fixture(Path(directory)), RECEIPT_FIELDS, RECEIPT_HASH)
            self.assertEqual(fields["kind"], "frame")

    def test_fixture_is_bound_to_receipt_hash_and_release_identity(self) -> None:
        receipt = {"source_release_sha256": RELEASE, "source_release_bytes": "328383"}
        with temporary_directory() as directory:
            root = write_fixture(Path(directory))
            with self.assertRaisesRegex(ValueError, "source release does not match"):
                TOOL.verify(root, {"source_release_sha256": "b" * 64,
                                   "source_release_bytes": "328383"}, RECEIPT_HASH)
            receipt_path = Path(directory).parent / "receipt.txt"
            receipt_path.write_bytes(b"receipt bytes")
            receipt_hash = hashlib.sha256(receipt_path.read_bytes()).hexdigest()
            (root / TOOL.MANIFEST_NAME).write_bytes(
                (root / TOOL.MANIFEST_NAME).read_bytes().replace(b"capture_sha256=" + b"a" * 64,
                                                                  f"capture_sha256={receipt_hash}".encode()))
            fields = TOOL.verify(root, receipt, receipt_hash)
            self.assertEqual(fields["capture_sha256"], receipt_hash)

    def test_payload_change_or_symlink_is_rejected(self) -> None:
        with temporary_directory() as directory:
            root = write_fixture(Path(directory))
            payload = root / "checkpoint.bin"
            payload.write_bytes(b"changed fixture-only test bytes")
            with self.assertRaisesRegex(ValueError, "hash or size"):
                TOOL.verify(root, RECEIPT_FIELDS, RECEIPT_HASH)
            payload.unlink()
            payload.symlink_to("missing")
            with self.assertRaisesRegex(ValueError, "regular non-symlink"):
                TOOL.verify(root, RECEIPT_FIELDS, RECEIPT_HASH)

    def test_manifest_rejects_unknown_field_and_unsafe_payload_name(self) -> None:
        with temporary_directory() as directory:
            root = write_fixture(Path(directory))
            manifest = root / TOOL.MANIFEST_NAME
            manifest.write_bytes((manifest.read_text(encoding="utf-8") + "extra=value\n").encode("utf-8"))
            with self.assertRaisesRegex(ValueError, "unknown, missing, or incomplete"):
                TOOL.verify(root, RECEIPT_FIELDS, RECEIPT_HASH)
            write_fixture(root)
            content = manifest.read_text(encoding="utf-8").replace("payload_file=checkpoint.bin", "payload_file=../checkpoint.bin")
            manifest.write_bytes(content.encode("utf-8"))
            with self.assertRaisesRegex(ValueError, "unsafe"):
                TOOL.verify(root, RECEIPT_FIELDS, RECEIPT_HASH)

    def test_fixture_directory_rejects_unlisted_files_and_directories(self) -> None:
        with temporary_directory() as directory:
            root = write_fixture(Path(directory))
            (root / "unlisted.bin").write_bytes(b"extra")
            with self.assertRaisesRegex(ValueError, "only its manifest and payload"):
                TOOL.verify(root, RECEIPT_FIELDS, RECEIPT_HASH)
            (root / "unlisted.bin").unlink()
            (root / "unlisted-dir").mkdir()
            with self.assertRaisesRegex(ValueError, "only its manifest and payload"):
                TOOL.verify(root, RECEIPT_FIELDS, RECEIPT_HASH)

    def test_manifest_requires_lf_records_and_a_distinct_payload_name(self) -> None:
        with temporary_directory() as directory:
            root = write_fixture(Path(directory))
            manifest = root / TOOL.MANIFEST_NAME
            manifest.write_bytes(manifest.read_bytes().replace(b"\n", b"\r\n"))
            with self.assertRaisesRegex(ValueError, "LF-terminated"):
                TOOL.verify(root, RECEIPT_FIELDS, RECEIPT_HASH)
            write_fixture(root)
            content = manifest.read_text(encoding="utf-8").replace(
                "payload_file=checkpoint.bin", f"payload_file={TOOL.MANIFEST_NAME}")
            manifest.write_bytes(content.encode("utf-8"))
            with self.assertRaisesRegex(ValueError, "conflicts with the manifest"):
                TOOL.verify(root, RECEIPT_FIELDS, RECEIPT_HASH)

    def test_oversized_manifest_is_rejected_before_decoding(self) -> None:
        with temporary_directory() as directory:
            root = write_fixture(Path(directory))
            manifest = root / TOOL.MANIFEST_NAME
            manifest.write_bytes(b"x" * (TOOL.MAX_MANIFEST_BYTES + 1))
            with self.assertRaisesRegex(ValueError, "manifest exceeds its safety limit"):
                TOOL.verify(root, RECEIPT_FIELDS, RECEIPT_HASH)

    def test_kind_specific_limit_and_canonical_checkpoint_fields_are_enforced(self) -> None:
        with temporary_directory() as directory:
            root = write_fixture(Path(directory), kind="input")
            manifest = root / TOOL.MANIFEST_NAME
            content = manifest.read_text(encoding="utf-8").replace("checkpoint_sequence=1", "checkpoint_sequence=0")
            manifest.write_bytes(content.encode("utf-8"))
            with self.assertRaisesRegex(ValueError, "sequence"):
                TOOL.verify(root, RECEIPT_FIELDS, RECEIPT_HASH)
            write_fixture(root, kind="input", payload=b"x" * (TOOL.KINDS["input"] + 1))
            with self.assertRaisesRegex(ValueError, "safety limit"):
                TOOL.verify(root, RECEIPT_FIELDS, RECEIPT_HASH)


if __name__ == "__main__":
    unittest.main()
