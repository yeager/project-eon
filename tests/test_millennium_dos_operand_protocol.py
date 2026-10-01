"""Synthetic, media-free tests for the frozen DOS operand sidecar contract."""
from __future__ import annotations

import hashlib
import importlib.util
import contextlib
import io
import os
from pathlib import Path
import sys
import unittest
from unittest import mock

from eon_test_paths import temporary_directory

ROOT = Path(__file__).resolve().parents[1]


def load(name: str, path: Path):
    spec = importlib.util.spec_from_file_location(name, path)
    assert spec and spec.loader
    module = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(module)
    return module


PROTOCOL = load("millennium_dos_operand_protocol", ROOT / "tools/millennium_dos_operand_protocol.py")
VERIFIER = load("verify_capture_receipt", ROOT / "tools/verify_capture_receipt.py")
SAFE_DIRFD = (os.open in getattr(os, "supports_dir_fd", set())
              and hasattr(os, "O_DIRECTORY") and hasattr(os, "O_NOFOLLOW"))


def present_record(*, cs="1234", ds="3c80", si="0000", offset="0001", operand="7f") -> bytes:
    return (
        "schema=project-eon.dos-title-operand-observation/v1;"
        "protocol=v25-dos-title-operand;state=present;"
        "event=operand-read-observed-during-execution;image_name=TITLES.EXE;"
        "image_size=7022;"
        f"image_sha256={PROTOCOL.IMAGE_SHA256};"
        f"cs={cs};site_ip=13e9;file_offset=12e9;span_size=9;"
        f"span_sha256={PROTOCOL.SPAN_SHA256};opcode=8a4401;"
        f"ds={ds};si={si};logical_offset={offset};operand={operand}\n"
    ).encode("ascii")


def receipt_fields(payload: bytes, *, state="present") -> dict[str, str]:
    return {
        "capture_receipt_version": "25",
        "recorder_protocol": PROTOCOL.PROTOCOL,
        "recorder_admission": PROTOCOL.RECORDER_ADMISSION,
        "operand_observation": state,
        "operand_observation_sha256": hashlib.sha256(payload).hexdigest(),
        "operand_observation_bytes": str(len(payload)),
    }


class OperandProtocolTests(unittest.TestCase):
    def test_parses_present_and_absent_canonical_records(self):
        parsed = PROTOCOL.parse_observation(present_record())
        self.assertEqual(parsed["state"], "present")
        self.assertEqual(parsed["logical_offset"], "0001")
        self.assertEqual(parsed["operand"], "7f")
        self.assertEqual(PROTOCOL.parse_observation(
            b"schema=project-eon.dos-title-operand-observation/v1;"
            b"protocol=v25-dos-title-operand;state=absent\n"), {"state": "absent"})
        wrapped = PROTOCOL.parse_observation(present_record(si="ffff", offset="0000"))
        self.assertEqual(wrapped["logical_offset"], "0000")

    def test_rejects_noncanonical_present_grammar(self):
        valid = present_record()
        invalid = (
            valid[:-1], valid + b"\n", valid.replace(b"cs=1234", b"cs=ABCD"),
            valid.replace(b"site_ip=13e9", b"site_ip=13E9"),
            valid.replace(b"opcode=8a4401", b"opcode=8A4401"),
            valid.replace(b"image_name=TITLES.EXE", b"image_name=titles.exe"),
            valid.replace(b"event=operand-read-observed-during-execution", b"event=instruction-retired"),
            valid.replace(b"span_sha256=" + PROTOCOL.SPAN_SHA256.encode(), b"span_sha256=" + b"0" * 64),
            valid.replace(b"image_sha256=" + PROTOCOL.IMAGE_SHA256.encode(), b"image_sha256=" + b"0" * 64),
            valid.replace(b"file_offset=12e9", b"file_offset=12EA"),
            valid.replace(b"span_size=9", b"span_size=09"),
            valid.replace(b";ds=3c80", b";si=3c80"),
            valid.replace(b";operand=7f", b";operand=7F"),
            valid.replace(b"logical_offset=0001", b"logical_offset=0002"),
            valid.replace(b"si=0000", b"si=ffff"),
        )
        for payload in invalid:
            with self.subTest(payload=payload[:100]), self.assertRaises(ValueError):
                PROTOCOL.parse_observation(payload)

    def test_rejects_non_ascii_and_oversize_payloads(self):
        with self.assertRaisesRegex(ValueError, "ASCII"):
            PROTOCOL.parse_observation(b"\xff")
        with self.assertRaisesRegex(ValueError, "bound"):
            PROTOCOL.parse_observation(b"x" * (PROTOCOL.MAX_OBSERVATION_BYTES + 1))

    @unittest.skipUnless(SAFE_DIRFD, "descriptor-relative no-follow reads are unavailable")
    def test_file_verification_binds_exact_diagnostics_only_receipt(self):
        with temporary_directory() as temporary:
            directory = Path(temporary).resolve()
            payload = present_record()
            (directory / PROTOCOL.OBSERVATION_NAME).write_bytes(payload)
            fields = receipt_fields(payload)
            PROTOCOL.verify_fields(fields, directory, allow_experimental_observer=True)
            for name, value in (
                ("recorder_admission", "pinned"),
                ("recorder_protocol", "v22"),
                ("capture_receipt_version", "24"),
                ("operand_observation", "absent"),
                ("operand_observation_sha256", "0" * 64),
                ("operand_observation_bytes", "01"),
                ("unexpected", "field"),
            ):
                changed = dict(fields)
                changed[name] = value
                with self.subTest(name=name), self.assertRaises(ValueError):
                    PROTOCOL.verify_fields(changed, directory, allow_experimental_observer=True)
            missing = dict(fields)
            del missing["operand_observation_bytes"]
            with self.assertRaisesRegex(ValueError, "exact schema"):
                PROTOCOL.verify_fields(missing, directory, allow_experimental_observer=True)
            with self.assertRaisesRegex(ValueError, "not admitted"):
                PROTOCOL.verify_fields(fields, directory)

    @unittest.skipUnless(SAFE_DIRFD, "descriptor-relative no-follow reads are unavailable")
    def test_absence_requires_the_explicit_absent_record(self):
        with temporary_directory() as temporary:
            directory = Path(temporary).resolve()
            payload = (
                b"schema=project-eon.dos-title-operand-observation/v1;"
                b"protocol=v25-dos-title-operand;state=absent\n")
            (directory / PROTOCOL.OBSERVATION_NAME).write_bytes(payload)
            fields = receipt_fields(payload, state="absent")
            PROTOCOL.verify_fields(fields, directory, allow_experimental_observer=True)
            with self.assertRaises(ValueError):
                PROTOCOL.verify_fields(dict(fields, operand_observation="present"), directory,
                                        allow_experimental_observer=True)

    @unittest.skipUnless(SAFE_DIRFD, "descriptor-relative no-follow reads are unavailable")
    def test_bounded_reader_rejects_symlink_nonregular_and_relative_path(self):
        with temporary_directory() as temporary:
            directory = Path(temporary).resolve()
            regular = directory / "regular"
            regular.write_bytes(b"data")
            self.assertEqual(PROTOCOL.read_bounded(regular), b"data")
            link = directory / "link"
            link.symlink_to(regular)
            with self.assertRaises((ValueError, OSError)):
                PROTOCOL.read_bounded(link)
            actual_directory = directory / "actual"
            actual_directory.mkdir()
            nested_payload = actual_directory / "payload"
            nested_payload.write_bytes(b"safe")
            alias = directory / "alias"
            alias.symlink_to(actual_directory, target_is_directory=True)
            with self.assertRaises((ValueError, OSError)):
                PROTOCOL.read_bounded(alias / "payload")
            with self.assertRaises((ValueError, OSError)):
                PROTOCOL.read_bounded(directory)
            with self.assertRaises(ValueError):
                PROTOCOL.read_bounded(Path("relative"))

    @unittest.skipUnless(SAFE_DIRFD, "descriptor-relative no-follow reads are unavailable")
    def test_schema25_dispatch_requires_explicit_experimental_opt_in(self):
        with temporary_directory() as temporary:
            directory = Path(temporary).resolve()
            payload = present_record()
            (directory / PROTOCOL.OBSERVATION_NAME).write_bytes(payload)
            fields = receipt_fields(payload)
            (directory / "run-status.txt").write_text(
                "".join(f"{key}={value}\n" for key, value in fields.items()), encoding="ascii")
            self.assertEqual(VERIFIER.require_receipt_schema(fields), "25")
            with self.assertRaisesRegex(ValueError, "not admitted"):
                VERIFIER.verify("millennium-dos", directory)
            VERIFIER.verify("millennium-dos", directory, allow_experimental_observer=True)
            with self.assertRaisesRegex(ValueError, "only supported"):
                VERIFIER.verify("deuteros-amiga", directory, allow_experimental_observer=True)

            output = io.StringIO()
            with (mock.patch.object(sys, "argv", [
                    "verify_capture_receipt.py", "--kind", "millennium-dos",
                    "--capture", str(directory), "--allow-experimental-observer"]),
                  contextlib.redirect_stdout(output)):
                self.assertEqual(VERIFIER.main(), 0)
            self.assertEqual(output.getvalue(),
                             "EXPERIMENTAL CAPTURE RECEIPT VERIFIED; NOT ADMITTED FOR RECOVERY  "
                             f"millennium-dos  {directory}\n")


if __name__ == "__main__":
    unittest.main()
