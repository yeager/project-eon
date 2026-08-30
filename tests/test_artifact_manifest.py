"""Contract tests for the release-free CI artifact integrity ledger."""

from __future__ import annotations

import hashlib
import json
from pathlib import Path
import subprocess
import sys
import unittest

from eon_test_paths import temporary_directory


ROOT = Path(__file__).resolve().parents[1]
SCRIPT = ROOT / "packaging" / "write-artifact-manifest.py"
VERIFIER = ROOT / "packaging" / "verify-artifact-manifest.py"
REVISION = "0123456789abcdef0123456789abcdef01234567"


class ArtifactManifestTests(unittest.TestCase):
    def test_manifest_records_download_verifiable_bytes_deterministically(self) -> None:
        with temporary_directory() as temporary:
            root = Path(temporary)
            first = root / "first.bin"
            second = root / "second.bin"
            first.write_bytes(b"first artifact\x00")
            second.write_bytes(b"second artifact\x01")
            output = root / "manifest.json"
            command = [sys.executable, str(SCRIPT), "--source-revision", REVISION,
                       "--output", str(output), str(second), str(first)]
            subprocess.run(command, check=True)
            document = json.loads(output.read_text(encoding="utf-8"))
            self.assertEqual(document["schema_version"], 1)
            self.assertEqual(document["project"], "project-eon")
            self.assertEqual(document["source_revision"], REVISION)
            self.assertEqual([entry["name"] for entry in document["artifacts"]],
                             ["first.bin", "second.bin"])
            self.assertEqual(document["artifacts"][0]["size"], len(first.read_bytes()))
            self.assertEqual(document["artifacts"][0]["sha256"],
                             hashlib.sha256(first.read_bytes()).hexdigest())
            self.assertNotIn(str(root), output.read_text(encoding="utf-8"))

    def test_rejects_unsafe_or_ambiguous_inputs(self) -> None:
        with temporary_directory() as temporary:
            root = Path(temporary)
            artifact = root / "artifact.bin"
            artifact.write_bytes(b"artifact")
            bad_revision = subprocess.run(
                [sys.executable, str(SCRIPT), "--source-revision", "main", "--output",
                 str(root / "manifest.json"), str(artifact)], text=True, capture_output=True)
            self.assertNotEqual(bad_revision.returncode, 0)
            self.assertIn("40-character lowercase", bad_revision.stderr)
            duplicate = subprocess.run(
                [sys.executable, str(SCRIPT), "--source-revision", REVISION, "--output",
                 str(root / "manifest.json"), str(artifact), str(artifact)], text=True,
                capture_output=True)
            self.assertEqual(duplicate.returncode, 2)
            self.assertIn("duplicate artifact name", duplicate.stderr)
            symlink = root / "artifact-link.bin"
            symlink.symlink_to(artifact)
            linked = subprocess.run(
                [sys.executable, str(SCRIPT), "--source-revision", REVISION, "--output",
                 str(root / "manifest.json"), str(symlink)], text=True, capture_output=True)
            self.assertEqual(linked.returncode, 2)
            self.assertIn("non-symlink regular file", linked.stderr)

    def test_verifier_detects_tampering_schema_and_unrecorded_files(self) -> None:
        with temporary_directory() as temporary:
            root = Path(temporary)
            artifact = root / "artifact.bin"
            artifact.write_bytes(b"verified artifact")
            manifest = root / "manifest.json"
            subprocess.run([sys.executable, str(SCRIPT), "--source-revision", REVISION,
                            "--output", str(manifest), str(artifact)], check=True)
            command = [sys.executable, str(VERIFIER), "--manifest", str(manifest),
                       "--directory", str(root), "--expected-source-revision", REVISION,
                       "--require-exact-directory"]
            verified = subprocess.run(command, text=True, capture_output=True)
            self.assertEqual(verified.returncode, 0, verified.stderr)
            self.assertIn("verified 1 Project Eon artifact", verified.stdout)
            artifact.write_bytes(b"tampered artifact")
            tampered = subprocess.run(command, text=True, capture_output=True)
            self.assertEqual(tampered.returncode, 2)
            self.assertIn("SHA-256 mismatch", tampered.stderr)
            artifact.write_bytes(b"verified artifact")
            (root / "unexpected.bin").write_bytes(b"not recorded")
            extra = subprocess.run(command, text=True, capture_output=True)
            self.assertEqual(extra.returncode, 2)
            self.assertIn("unrecorded entries", extra.stderr)
            (root / "unexpected.bin").unlink()
            document = json.loads(manifest.read_text(encoding="utf-8"))
            document["artifacts"][0]["sha256"] = "UPPERCASE"
            manifest.write_text(json.dumps(document), encoding="utf-8")
            malformed = subprocess.run(command, text=True, capture_output=True)
            self.assertEqual(malformed.returncode, 2)
            self.assertIn("invalid SHA-256", malformed.stderr)
            document["artifacts"][0]["sha256"] = hashlib.sha256(artifact.read_bytes()).hexdigest()
            document["artifacts"][0]["name"] = r"portable\\escape.bin"
            manifest.write_text(json.dumps(document), encoding="utf-8")
            unsafe_name = subprocess.run(command, text=True, capture_output=True)
            self.assertEqual(unsafe_name.returncode, 2)
            self.assertIn("safe basename", unsafe_name.stderr)
