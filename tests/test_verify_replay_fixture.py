"""Contracts for opaque external replay-fixture admission."""
from __future__ import annotations

import hashlib
import importlib.util
from pathlib import Path
import unittest

from eon_test_paths import temporary_directory


ROOT = Path(__file__).resolve().parents[1]
SPEC = importlib.util.spec_from_file_location("verify_replay_fixture", ROOT / "tools" / "verify_replay_fixture.py")
assert SPEC and SPEC.loader
TOOL = importlib.util.module_from_spec(SPEC)
SPEC.loader.exec_module(TOOL)

RELEASE = "e6e7044b25877fdf8b10d16d2f395886d9957953144ae15ca630cda9cab2a123"


def write_fixture(root: Path, *, kind: str = "frame", payload: bytes = b"fixture-only test bytes") -> Path:
    payload_name = "checkpoint.bin"
    (root / payload_name).write_bytes(payload)
    fields = {
        "format": TOOL.FORMAT,
        "kind": kind,
        "source_release_sha256": RELEASE,
        "source_release_size": "328383",
        "capture_sha256": "a" * 64,
        "checkpoint_sequence": "1",
        "checkpoint_tick": "0",
        "payload_file": payload_name,
        "payload_sha256": hashlib.sha256(payload).hexdigest(),
        "payload_bytes": str(len(payload)),
    }
    (root / TOOL.MANIFEST_NAME).write_text("".join(f"{key}={value}\n" for key, value in fields.items()), encoding="utf-8")
    return root


class ReplayFixtureVerifierTests(unittest.TestCase):
    def test_hash_bound_fixture_accepts_only_recognised_release(self) -> None:
        with temporary_directory() as directory:
            fields = TOOL.verify(write_fixture(Path(directory)))
            self.assertEqual(fields["kind"], "frame")

    def test_payload_change_or_symlink_is_rejected(self) -> None:
        with temporary_directory() as directory:
            root = write_fixture(Path(directory))
            payload = root / "checkpoint.bin"
            payload.write_bytes(b"changed fixture-only test bytes")
            with self.assertRaisesRegex(ValueError, "hash or size"):
                TOOL.verify(root)
            payload.unlink()
            payload.symlink_to("missing")
            with self.assertRaisesRegex(ValueError, "regular non-symlink"):
                TOOL.verify(root)

    def test_manifest_rejects_unknown_field_and_unsafe_payload_name(self) -> None:
        with temporary_directory() as directory:
            root = write_fixture(Path(directory))
            manifest = root / TOOL.MANIFEST_NAME
            manifest.write_text(manifest.read_text(encoding="utf-8") + "extra=value\n", encoding="utf-8")
            with self.assertRaisesRegex(ValueError, "unknown, missing, or incomplete"):
                TOOL.verify(root)
            write_fixture(root)
            content = manifest.read_text(encoding="utf-8").replace("payload_file=checkpoint.bin", "payload_file=../checkpoint.bin")
            manifest.write_text(content, encoding="utf-8")
            with self.assertRaisesRegex(ValueError, "unsafe"):
                TOOL.verify(root)

    def test_kind_specific_limit_and_canonical_checkpoint_fields_are_enforced(self) -> None:
        with temporary_directory() as directory:
            root = write_fixture(Path(directory), kind="input")
            manifest = root / TOOL.MANIFEST_NAME
            content = manifest.read_text(encoding="utf-8").replace("checkpoint_sequence=1", "checkpoint_sequence=0")
            manifest.write_text(content, encoding="utf-8")
            with self.assertRaisesRegex(ValueError, "sequence"):
                TOOL.verify(root)
            write_fixture(root, kind="input", payload=b"x" * (TOOL.KINDS["input"] + 1))
            with self.assertRaisesRegex(ValueError, "safety limit"):
                TOOL.verify(root)


if __name__ == "__main__":
    unittest.main()
