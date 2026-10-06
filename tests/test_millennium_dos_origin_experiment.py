"""Media-free tests for the diagnostics-only DOS origin experiment contract."""
from __future__ import annotations

import hashlib
import importlib.util
import os
from pathlib import Path
import signal
import unittest
from unittest import mock

from eon_test_paths import LfTextFixtureWrites, temporary_directory


ROOT = Path(__file__).resolve().parents[1]


def load(name: str, path: Path):
    spec = importlib.util.spec_from_file_location(name, path)
    assert spec and spec.loader
    module = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(module)
    return module


PROTOCOL = load("millennium_dos_origin_experiment",
                ROOT / "tools" / "millennium_dos_origin_experiment.py")
RUNNER = load("run_millennium_dos_origin_experiment",
              ROOT / "tools" / "run_millennium_dos_origin_experiment.py")


def raw_record(*, title_valid="1", title_entry_cs="0e70", title_entry_ip="0100",
               title_size="00001b6e",
               title_sha256="3cc57f2b12a0da44dd43220f44f06a05b9e3f009bcf008b7bb87622a5988cbe6",
               fetches_after_title_load="0", previous_valid="1", previous_cs="1234", previous_ip="abcd",
               previous_opcode_valid="1", previous_opcode="90", current_cs="0e70",
               current_ip="0100", current_opcode_valid="1", current_opcode="fa",
               transfer_class="cross-segment-entry") -> bytes:
    return (f"eon-dos-origin-experiment-v2\ttitle_valid={title_valid}"
            f"\ttitle_entry_cs={title_entry_cs}\ttitle_entry_ip={title_entry_ip}"
            f"\ttitle_size={title_size}\ttitle_sha256={title_sha256}"
            f"\tfetches_after_title_load={fetches_after_title_load}"
            f"\tprevious_valid={previous_valid}"
            f"\tprevious_cs={previous_cs}\tprevious_ip={previous_ip}"
            f"\tprevious_opcode_valid={previous_opcode_valid}\tprevious_opcode={previous_opcode}"
            f"\tcurrent_cs={current_cs}\tcurrent_ip={current_ip}"
            f"\tcurrent_opcode_valid={current_opcode_valid}\tcurrent_opcode={current_opcode}"
            f"\ttransfer_class={transfer_class}\n").encode("ascii")


class OriginExperimentTests(LfTextFixtureWrites, unittest.TestCase):
    @unittest.skipUnless(os.name == "posix", "recorder process-group isolation is POSIX-specific")
    def test_timeout_kill_targets_the_isolated_recorder_process_group(self):
        process = mock.Mock(pid=4321)
        with mock.patch.object(RUNNER.os, "killpg") as killpg:
            RUNNER.kill_recorder_group(process)
        killpg.assert_called_once_with(4321, signal.SIGKILL)

    @unittest.skipUnless(os.name == "posix", "recorder process-group isolation is POSIX-specific")
    def test_timeout_kill_accepts_a_group_that_already_exited(self):
        process = mock.Mock(pid=4321)
        with mock.patch.object(RUNNER.os, "killpg", side_effect=ProcessLookupError):
            RUNNER.kill_recorder_group(process)

    def test_parses_finite_last_fetch_context_without_causal_claim(self):
        parsed = PROTOCOL.parse_raw(raw_record())
        self.assertEqual(parsed["transfer_class"], "cross-segment-entry")
        self.assertEqual(parsed["title_sha256"], PROTOCOL.TITLES_SHA256)
        self.assertEqual(parsed["title_entry_ip"], 0x0100)
        same = PROTOCOL.parse_raw(raw_record(previous_cs="0e70", transfer_class="same-segment-entry"))
        self.assertEqual(same["previous_cs"], 0x0E70)
        absent = PROTOCOL.parse_raw(raw_record(
            previous_valid="0", previous_cs="0000", previous_ip="0000",
            previous_opcode_valid="0", previous_opcode="00", transfer_class="no-predecessor"))
        self.assertEqual(absent["previous_valid"], 0)

    def test_rejects_noncanonical_and_inconsistent_records(self):
        valid = raw_record()
        invalid = (
            valid[:-1], valid + b"\n", valid.replace(b"0e70", b"0E70"),
            valid.replace(b"title_valid=1", b"title_valid=0"),
            valid.replace(PROTOCOL.TITLES_SHA256.encode(), b"0" * 64),
            valid.replace(b"title_entry_ip=0100", b"title_entry_ip=00ff"),
            valid.replace(b"title_size=00001b6e", b"title_size=00001b6f"),
            valid.replace(b"current_cs=0e70", b"current_cs=0e71"),
            valid.replace(b"current_opcode_valid=1", b"current_opcode_valid=0"),
            valid.replace(b"transfer_class=cross-segment-entry", b"transfer_class=same-segment-entry"),
            valid.replace(b"previous_opcode_valid=1", b"previous_opcode_valid=0"),
            valid.replace(b"previous_valid=1", b"previous_valid=0"),
            valid + b"untrusted=1\n",
        )
        for candidate in invalid:
            with self.subTest(candidate=candidate), self.assertRaises(PROTOCOL.ExperimentError):
                PROTOCOL.parse_raw(candidate)

    def test_verifier_requires_exact_status_console_marker_and_raw_hashes(self):
        with temporary_directory() as temp:
            root = Path(temp).resolve()
            raw = raw_record()
            console = ("diagnostic output\n" + PROTOCOL.RECEIPT_MARKER + "\n").encode("ascii")
            capture = load("origin_test_capture", ROOT / "tools" / "run_millennium_dos_capture.py")
            with mock.patch.object(RUNNER.CAPTURE, "recorder_config",
                                   return_value=capture.recorder_config(
                                       root / "archive-ro" / PROTOCOL.GAME_ROOT)):
                configuration = RUNNER.origin_recorder_configuration(
                    root / "archive-ro" / PROTOCOL.GAME_ROOT).encode("utf-8")
            (root / "origin.raw").write_bytes(raw)
            (root / "recorder-console.log").write_bytes(console)
            (root / "recorder.conf").write_bytes(configuration)
            values = {
                "schema": PROTOCOL.SCHEMA,
                "admission": "experimental-only-not-recovery-admissible",
                "recorder_sha256": PROTOCOL.RECORDER_SHA256,
                "recorder_bytes": str(PROTOCOL.RECORDER_BYTES),
                "patch_sha256": PROTOCOL.PATCH_SHA256,
                "source_release_sha256": PROTOCOL.SOURCE_RELEASE_SHA256,
                "source_release_bytes": str(PROTOCOL.SOURCE_RELEASE_BYTES),
                "configuration_sha256": hashlib.sha256(configuration).hexdigest(),
                "configuration_bytes": str(len(configuration)),
                "raw_receipt": "origin.raw",
                "raw_sha256": hashlib.sha256(raw).hexdigest(),
                "raw_bytes": str(len(raw)),
                "console_sha256": hashlib.sha256(console).hexdigest(),
                "console_bytes": str(len(console)),
                "exit_status": "0",
                "success_marker": "present",
                "record_status": "complete",
            }
            status = root / "experiment-status.txt"
            status.write_text("".join(f"{key}={value}\n" for key, value in values.items()),
                              encoding="ascii")
            self.assertEqual(PROTOCOL.verify_run(status)["admission"],
                             "experimental-only-not-recovery-admissible")

            values["success_marker"] = "absent"
            status.write_text("".join(f"{key}={value}\n" for key, value in values.items()),
                              encoding="ascii")
            with self.assertRaisesRegex(PROTOCOL.ExperimentError, "success marker"):
                PROTOCOL.verify_run(status)

    def test_runner_copies_and_removes_only_its_uuid_receipt(self):
        with temporary_directory() as temp:
            root = Path(temp).resolve()
            receipt_root = root / "receipts"
            receipt_root.mkdir()
            run_root = root / "run"
            run_root.mkdir()
            uuid = "0123456789abcdef0123456789abcdef"
            candidate = receipt_root / f"origin-{uuid}.receipt"
            payload = raw_record()
            candidate.write_bytes(payload)
            with mock.patch.object(RUNNER, "TRV2_RECEIPTS", receipt_root):
                copied, disposition = RUNNER.copy_and_remove_candidate_receipt(candidate, run_root)
            self.assertEqual(copied, payload)
            self.assertEqual(disposition, "copied-and-removed")
            self.assertEqual((run_root / "origin.raw").read_bytes(), payload)
            self.assertFalse(candidate.exists())

    def test_runner_isolates_recorder_overrides_but_keeps_visible_display(self):
        with mock.patch.object(RUNNER.CAPTURE, "require_visible_operator_input"):
            environment = RUNNER.isolated_environment(
                Path("/run/origin-test"),
                {"HOME": "/home/trv2", "DISPLAY": ":0", "DOSBOX": "untrusted",
                 "LD_PRELOAD": "/tmp/injected.so", "PROJECT_EON_DOS_ORIGIN_EXPERIMENT": "0"})
        self.assertEqual(environment["DISPLAY"], ":0")
        self.assertEqual(environment["HOME"], "/home/trv2")
        self.assertNotIn("LD_PRELOAD", environment)
        self.assertNotIn("DOSBOX", environment)
        self.assertNotIn("PROJECT_EON_DOS_ORIGIN_EXPERIMENT", environment)

    def test_origin_config_disables_only_dosbox_quit_warning(self):
        with mock.patch.object(RUNNER.CAPTURE, "recorder_config",
                               return_value="[dosbox]\nmachine=svga_s3\nmemsize=16\n"):
            configuration = RUNNER.origin_recorder_configuration(Path("/archive/game"))
        self.assertIn("[dosbox]\nmachine=svga_s3\nmemsize=16\nquit warning=false\n",
                      configuration)
        self.assertIn("[log]\nlogfile=/dev/null\ncpu=never\n", configuration)

    def test_origin_config_rejects_an_unreviewed_config_shape(self):
        with mock.patch.object(RUNNER.CAPTURE, "recorder_config",
                               return_value="[dosbox]\nmemsize=32\n"):
            with self.assertRaisesRegex(RUNNER.CaptureError, "configuration anchor"):
                RUNNER.origin_recorder_configuration(Path("/archive/game"))

    def test_output_must_be_new_and_scoped_to_candidate_cache(self):
        with temporary_directory() as temp:
            root = Path(temp).resolve()
            source_dir = root / "media"
            source_dir.mkdir()
            source = source_dir / "release.zip"
            source.write_bytes(b"owned bytes")
            outside = root / "capture"
            with self.assertRaisesRegex(RUNNER.CaptureError, "direct child of the external"):
                RUNNER.require_experiment_output(outside, source)
            existing = root / "existing"
            existing.mkdir()
            with self.assertRaisesRegex(RUNNER.CaptureError, "new absolute"):
                RUNNER.require_experiment_output(existing, source)


if __name__ == "__main__":
    unittest.main()
