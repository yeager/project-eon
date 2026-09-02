"""Contracts for the read-only reviewed-recorder locator."""
from __future__ import annotations

import hashlib
import importlib.util
import json
from pathlib import Path
import unittest

from eon_test_paths import temporary_directory


ROOT = Path(__file__).resolve().parents[1]
SPEC = importlib.util.spec_from_file_location("locate_capture_recorder", ROOT / "tools" / "locate_capture_recorder.py")
assert SPEC and SPEC.loader
TOOL = importlib.util.module_from_spec(SPEC)
SPEC.loader.exec_module(TOOL)


class LocateCaptureRecorderTests(unittest.TestCase):
    def test_root_rejects_repository_temp_and_symlink_routes(self) -> None:
        with self.assertRaisesRegex(TOOL.LocatorError, "repository"):
            TOOL.require_root(ROOT)
        with self.assertRaisesRegex(TOOL.LocatorError, "/tmp"):
            TOOL.require_root(Path("/tmp"))
        with temporary_directory() as temporary:
            root = Path(temporary)
            link = root / "link"
            link.symlink_to(root)
            with self.assertRaisesRegex(TOOL.LocatorError, "non-symlink"):
                TOOL.require_root(link)

    def test_locator_hashes_only_regular_executable_candidates(self) -> None:
        with temporary_directory() as temporary:
            root = Path(temporary)
            recorder = root / "reviewed-recorder"
            recorder.write_bytes(b"recorder bytes")
            recorder.chmod(0o700)
            ignored = root / "ignored"
            ignored.write_bytes(b"recorder bytes")
            original = TOOL.reviewed_hashes
            try:
                TOOL.reviewed_hashes = lambda kind, protocol: {"fixture": hashlib.sha256(recorder.read_bytes()).hexdigest()}
                self.assertEqual(TOOL.locate("millennium-dos", [root], None, 2), [{
                    "protocol": "fixture", "sha256": hashlib.sha256(recorder.read_bytes()).hexdigest(),
                    "bytes": len(recorder.read_bytes()), "path": str(recorder)}])
            finally:
                TOOL.reviewed_hashes = original

    def test_json_schema_is_stable(self) -> None:
        self.assertEqual(json.loads('{"schema":"project-eon.recorder-locator/v1"}')["schema"],
                         "project-eon.recorder-locator/v1")


if __name__ == "__main__":
    unittest.main()
