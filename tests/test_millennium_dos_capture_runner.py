"""Safety contracts for the external, operator-driven DOS capture helper."""

from __future__ import annotations

import hashlib
import importlib.util
import io
from pathlib import Path
import unittest
from unittest import mock

from eon_test_paths import temporary_directory


ROOT = Path(__file__).resolve().parents[1]
SPEC = importlib.util.spec_from_file_location(
    "run_millennium_dos_capture", ROOT / "tools" / "run_millennium_dos_capture.py")
assert SPEC and SPEC.loader
TOOL = importlib.util.module_from_spec(SPEC)
SPEC.loader.exec_module(TOOL)


class MillenniumDosCaptureRunnerTests(unittest.TestCase):
    def test_rejects_headless_or_missing_visible_display(self) -> None:
        with self.assertRaisesRegex(TOOL.CaptureError, "headless SDL"):
            TOOL.require_visible_operator_input({"SDL_VIDEODRIVER": "dummy", "DISPLAY": ":1"})
        with self.assertRaisesRegex(TOOL.CaptureError, "visible X11 or Wayland"):
            TOOL.require_visible_operator_input({})
        TOOL.require_visible_operator_input({"WAYLAND_DISPLAY": "wayland-0"})

    def test_generated_configuration_handles_real_mode_wrap_and_never_injects_input(self) -> None:
        configuration = TOOL.recorder_config(Path("/safe/read-only/game-root"))
        self.assertIn("core=normal", configuration)
        self.assertIn("segment limits=false", configuration)
        self.assertIn("machine=svga_s3", configuration)
        self.assertIn('mount c "/safe/read-only/game-root"', configuration)
        self.assertNotIn("AUTOTYPE", configuration)
        self.assertNotIn("KEYBOARD_AddKey", configuration)

    def test_machine_profiles_are_finite_and_rendered_verbatim(self) -> None:
        self.assertIn("machine=ega", TOOL.recorder_config(Path("/safe/read-only/game-root"), "ega"))
        with self.assertRaisesRegex(TOOL.CaptureError, "finite profile"):
            TOOL.recorder_config(Path("/safe/read-only/game-root"), "vgaonly")

    def test_output_rejects_repository_media_and_system_temp_paths(self) -> None:
        with temporary_directory() as directory:
            root = Path(directory)
            media = root / "Downloads"
            media.mkdir()
            source = media / "owned-release.zip"
            source.write_bytes(b"test boundary bytes")
            with self.assertRaisesRegex(TOOL.CaptureError, "supplied-media"):
                TOOL.reject_unsafe_output(source.resolve(), (media / "capture").resolve())
            with self.assertRaisesRegex(TOOL.CaptureError, "/tmp"):
                TOOL.reject_unsafe_output(source.resolve(), Path("/tmp/eon-capture"))
            with self.assertRaisesRegex(TOOL.CaptureError, "repository"):
                TOOL.reject_unsafe_output(source.resolve(), ROOT / "capture-output")

    def test_source_hash_is_exact_and_rejects_other_bytes(self) -> None:
        with temporary_directory() as directory:
            source = Path(directory) / "owned-release.zip"
            source.write_bytes(b"test boundary bytes")
            original_hash = TOOL.EXPECTED_RELEASE_SHA256
            original_size = TOOL.EXPECTED_RELEASE_SIZE
            try:
                TOOL.EXPECTED_RELEASE_SHA256 = hashlib.sha256(source.read_bytes()).hexdigest()
                TOOL.EXPECTED_RELEASE_SIZE = source.stat().st_size
                self.assertEqual(TOOL.validate_source_release(source.resolve()),
                                 (TOOL.EXPECTED_RELEASE_SHA256, TOOL.EXPECTED_RELEASE_SIZE))
                TOOL.EXPECTED_RELEASE_SHA256 = "0" * 64
                with self.assertRaisesRegex(TOOL.CaptureError, "exact recognised"):
                    TOOL.validate_source_release(source.resolve())
            finally:
                TOOL.EXPECTED_RELEASE_SHA256 = original_hash
                TOOL.EXPECTED_RELEASE_SIZE = original_size

    def test_recorder_hash_is_exact_and_rejects_another_executable(self) -> None:
        with temporary_directory() as directory:
            recorder = Path(directory) / "reviewed-recorder"
            recorder.write_bytes(b"recorder boundary bytes")
            original_hash = TOOL.EXPECTED_RECORDER_SHA256
            try:
                TOOL.EXPECTED_RECORDER_SHA256 = hashlib.sha256(recorder.read_bytes()).hexdigest()
                self.assertEqual(TOOL.validate_recorder(recorder),
                                 (TOOL.EXPECTED_RECORDER_SHA256, recorder.stat().st_size))
                TOOL.EXPECTED_RECORDER_SHA256 = "0" * 64
                with self.assertRaisesRegex(TOOL.CaptureError, "reviewed DOSBox-X"):
                    TOOL.validate_recorder(recorder)
            finally:
                TOOL.EXPECTED_RECORDER_SHA256 = original_hash

    def test_identity_status_retains_the_complete_capture_preimage(self) -> None:
        for name in ("source_release", "recorder", "configuration"):
            status = TOOL.identity_status(name, ("a" * 64, 123))
            self.assertEqual(status, f"{name}_sha256=" + "a" * 64
                             + f"\n{name}_bytes=123\n")

    def test_input_receipt_status_never_invents_an_empty_timeline(self) -> None:
        with temporary_directory() as directory:
            receipt = Path(directory) / "host-input-receipt.raw"
            self.assertEqual(TOOL.input_receipt_status(receipt), "host_input_receipt=absent\n")
            receipt.write_bytes(b"")
            self.assertEqual(TOOL.input_receipt_status(receipt), "host_input_receipt=empty\n")
            observed = b"host-key 1 ticks=2 state=down scancode=0x1 sym=0x2 mod=0x0\n"
            receipt.write_bytes(observed)
            status = TOOL.input_receipt_status(receipt)
            self.assertIn("host_input_receipt=present\n", status)
            self.assertIn(f"host_input_receipt_bytes={len(observed)}\n", status)
            self.assertIn("host_input_receipt_records=1\n", status)
            receipt.write_bytes(b"host-key 2 ticks=2 state=down scancode=0x1 sym=0x2 mod=0x0\n")
            with self.assertRaisesRegex(TOOL.CaptureError, "ordinals"):
                TOOL.input_receipt_status(receipt)
            receipt.write_bytes(b"host-key 1 ticks=2 state=pressed scancode=0x1 sym=0x2 mod=0x0\n")
            with self.assertRaisesRegex(TOOL.CaptureError, "invalid recorder record"):
                TOOL.input_receipt_status(receipt)
            receipt.unlink()
            receipt.symlink_to("missing")
            with self.assertRaisesRegex(TOOL.CaptureError, "regular non-symlink"):
                TOOL.input_receipt_status(receipt)
            receipt.unlink()
            receipt.write_bytes(b"x" * (TOOL.MAX_INPUT_RECEIPT_BYTES + 1))
            with self.assertRaisesRegex(TOOL.CaptureError, "bounded recorder contract"):
                TOOL.input_receipt_status(receipt)

    def test_live_input_observer_does_not_parse_a_receipt_while_the_recorder_writes(self) -> None:
        with temporary_directory() as directory:
            receipt = Path(directory) / "host-input-receipt.raw"
            self.assertFalse(TOOL.input_delivery_file_observed(receipt))
            receipt.write_bytes(b"host-key 1 ticks=2")
            self.assertTrue(TOOL.input_delivery_file_observed(receipt))
            receipt.unlink()
            receipt.symlink_to("missing")
            with self.assertRaisesRegex(TOOL.CaptureError, "regular non-symlink"):
                TOOL.input_delivery_file_observed(receipt)

    def test_physical_capture_arguments_default_to_a_focus_settle_window(self) -> None:
        arguments = TOOL.parse_arguments((
            "--source-release", "/release.zip", "--recorder", "/recorder", "--output", "/capture",
            "--capture-intent", "physical-input",
        ))
        self.assertEqual(arguments.focus_settle_seconds, 10)
        self.assertEqual(arguments.capture_intent, "physical-input")
        with self.assertRaises(SystemExit), mock.patch("sys.stderr", new_callable=io.StringIO):
            TOOL.parse_arguments((
                "--source-release", "/release.zip", "--recorder", "/recorder", "--output", "/capture",
            ))

    def test_capture_intent_fails_closed_against_the_recorder_input_receipt(self) -> None:
        present = ("host_input_receipt=present\n"
                   "host_input_receipt_sha256=" + "a" * 64 + "\n"
                   "host_input_receipt_bytes=1\nhost_input_receipt_records=1\n")
        self.assertEqual(TOOL.capture_intent_status("physical-input", present, True),
                         "capture_intent=physical-input\ncapture_intent_input_requirement=required\n")
        self.assertEqual(TOOL.capture_intent_status("diagnostic-no-input", "host_input_receipt=absent\n", False),
                         "capture_intent=diagnostic-no-input\ncapture_intent_input_requirement=forbidden\n")
        with self.assertRaisesRegex(TOOL.CaptureError, "requires"):
            TOOL.capture_intent_status("physical-input", "host_input_receipt=absent\n", False)
        with self.assertRaisesRegex(TOOL.CaptureError, "must not"):
            TOOL.capture_intent_status("diagnostic-no-input", present, True)

    def test_no_input_instructions_forbid_keys_instead_of_requesting_them(self) -> None:
        diagnostic = "\n".join(TOOL.capture_operator_instructions("diagnostic-no-input"))
        physical = "\n".join(TOOL.capture_operator_instructions("physical-input"))
        self.assertIn("Do not click it or press any key", diagnostic)
        self.assertNotIn("press and release", diagnostic)
        self.assertIn("press and release", physical)

    def test_raw_observation_status_is_hash_bound_and_bounded(self) -> None:
        with temporary_directory() as directory:
            path = Path(directory) / "events.raw"
            self.assertEqual(TOOL.raw_observation_status(path, "events_raw"), "events_raw=absent\n")
            path.write_bytes(b"event\n")
            self.assertIn("events_raw_sha256=", TOOL.raw_observation_status(path, "events_raw"))
            results = Path(directory) / "results.raw"
            results.write_bytes(b"raw-result\t1 1 image=mill.com pc=0x020e source-int=0x21 source-ax=0x2591 ax=0x2591\n")
            status = TOOL.raw_observation_status(results, "results_raw")
            self.assertIn("results_raw_records=1\n", status)
            self.assertIn("results_raw_shapes=mill.com:020e:1\n", status)
            results.write_bytes(
                b"raw-result\t1 1 private-vector image=titles.exe pc=0x0127 int=0x91 "
                b"vector_ip=0x1234 vector_cs=0xabcd\n")
            status = TOOL.raw_observation_status(results, "results_raw")
            self.assertIn("results_raw_shapes=private-vector:1\n", status)
            results.write_bytes(b"raw-result\t1 1 private-handler-entry int=0x91 cs=0xabcd ip=0x1234\n")
            status = TOOL.raw_observation_status(results, "results_raw")
            self.assertIn("results_raw_shapes=private-handler-entry:1\n", status)
            results.write_bytes(
                b"raw-result\t1 1 private-handler-return int=0x91 caller=titles.exe pc=0x0129 "
                b"ax=0x0101 flags=0x0002\n")
            status = TOOL.raw_observation_status(results, "results_raw")
            self.assertIn("results_raw_shapes=private-handler-return:1\n", status)
            results.write_bytes(b"raw-result\t2 2 image=mill.com pc=0x020e source-int=0x21 source-ax=0x2591 ax=0x2591\n")
            with self.assertRaisesRegex(TOOL.CaptureError, "counters"):
                TOOL.raw_observation_status(results, "results_raw")
            path.write_bytes(b"x" * (TOOL.MAX_RAW_OBSERVATION_BYTES + 1))
            with self.assertRaisesRegex(TOOL.CaptureError, "bounded recorder contract"):
                TOOL.raw_observation_status(path, "events_raw")

    def test_v13_title_poll_binds_only_a_prior_host_receipt_ordinal(self) -> None:
        """A poll chronology is not promoted to a DOS key result or title action."""
        with temporary_directory() as directory:
            root = Path(directory)
            results = root / "results.raw"
            keys = root / "host-input-receipt.raw"
            keys.write_bytes(
                b"host-key 1 ticks=2 state=down scancode=0x1 sym=0x2 mod=0x0\n"
                b"host-key 2 ticks=3 state=up scancode=0x1 sym=0x2 mod=0x0\n")
            results.write_bytes(
                b"raw-result\t1 1 title-input-poll image=titles.exe pc=0x0d0a "
                b"host_key_ordinal=2 ah=0x06 dl=0xff\n")
            self.assertEqual(TOOL.title_input_poll_ordinals(results, "v13-title-poll"), [2])
            status = TOOL.raw_result_status(results, "results_raw", "v13-title-poll")
            self.assertIn("results_raw_title_input_polls=1\n", status)
            self.assertIn("results_raw_last_host_key_ordinal=2\n", status)
            checkpoint = TOOL.title_input_checkpoint_status(results, keys, "v13-title-poll")
            self.assertIn("title_input_checkpoint=host-key-and-poll\n", checkpoint)
            self.assertNotIn("accepted", checkpoint)
            results.write_bytes(
                b"raw-result\t1 1 title-input-poll image=titles.exe pc=0x0d0a "
                b"host_key_ordinal=3 ah=0x06 dl=0xff\n")
            with self.assertRaisesRegex(TOOL.CaptureError, "absent from the receipt"):
                TOOL.title_input_checkpoint_status(results, keys, "v13-title-poll")
            results.write_bytes(
                b"raw-result\t1 1 title-input-poll image=titles.exe pc=0x0d0a "
                b"host_key_ordinal=2 ah=0x06 dl=0xff\n"
                b"raw-result\t2 2 title-input-poll image=titles.exe pc=0x0d0a "
                b"host_key_ordinal=2 ah=0x06 dl=0xff\n")
            with self.assertRaisesRegex(TOOL.CaptureError, "strictly increasing"):
                TOOL.title_input_poll_ordinals(results, "v13-title-poll")

    def test_known_unhandled_interrupt_requires_complete_v11_raw_receipt(self) -> None:
        with temporary_directory() as directory:
            results = Path(directory) / "results.raw"
            self.assertFalse(TOOL.known_unhandled_interrupt_observed(results))
            results.write_bytes(
                b"raw-result\t1 1 fault=unhandled-interrupt int=0x06 cs=0xf000 ip=0xca64 "
                b"ss=0x0a8d sp=0xc9bf ax=0x00a0 bx=0x6101 cx=0x178b dx=0x6101")
            self.assertFalse(TOOL.known_unhandled_interrupt_observed(results))
            results.write_bytes(results.read_bytes() + b"\n")
            self.assertFalse(TOOL.known_unhandled_interrupt_observed(results))
            results.write_bytes(TOOL.KNOWN_V11_EARLY_STOP_RAW)
            self.assertTrue(TOOL.known_unhandled_interrupt_observed(results))
            altered = bytearray(TOOL.KNOWN_V11_EARLY_STOP_RAW)
            altered[-17] ^= 0x01
            results.write_bytes(altered)
            self.assertFalse(TOOL.known_unhandled_interrupt_observed(results))
            results.write_bytes(TOOL.KNOWN_V11_EARLY_STOP_RAW +
                b"raw-result\t9 9 fault=unhandled-interrupt int=0x06 cs=0xf000 ip=0xca64 "
                b"ss=0x0a8d sp=0xc9bf ax=0x00a0 bx=0x6101 cx=0x178b dx=0x6101\n")
            self.assertFalse(TOOL.known_unhandled_interrupt_observed(results))
            results.write_bytes(b"not a recorder record\n")
            self.assertFalse(TOOL.known_unhandled_interrupt_observed(results))

    def test_v12_predecessor_stop_requires_the_complete_bounded_shape(self) -> None:
        with temporary_directory() as directory:
            results = Path(directory) / "results.raw"
            payload = TOOL.KNOWN_V11_EARLY_STOP_RAW.replace(
                b"ax=0x00a0 bx=0x6101 cx=0x178b dx=0x6101\n",
                b"ax=0x00a0 bx=0x6101 cx=0x178b dx=0x6101 predecessor_valid=1 "
                b"predecessor_cs=0xf000 predecessor_ip=0xca60 predecessor_code=0xfe380300 "
                b"predecessor_recognised_image=0\n")
            results.write_bytes(payload)
            self.assertTrue(TOOL.known_unhandled_interrupt_observed(results, "v12-predecessor"))
            results.write_bytes(payload.replace(b"predecessor_recognised_image=0", b"predecessor_recognised_image=2"))
            self.assertFalse(TOOL.known_unhandled_interrupt_observed(results, "v12-predecessor"))

    def test_v14_normal_core_history_is_one_bounded_ordered_sidecar(self) -> None:
        with temporary_directory() as directory:
            history = Path(directory) / "normal-core-history.raw"
            history.write_text(
                "normal-core-history-v1 count=2 entries=0e70:18fe:00000000,f000:ca60:fe380300\n",
                encoding="ascii")
            status = TOOL.normal_core_history_status(history, "v14-normal-core-history")
            self.assertIn("normal_core_history=present\n", status)
            self.assertIn("normal_core_history_entries=2\n", status)
            history.write_text(
                "normal-core-history-v1 count=1 entries=0e70:18fe:00000000,f000:ca60:fe380300\n",
                encoding="ascii")
            with self.assertRaisesRegex(TOOL.CaptureError, "count does not match"):
                TOOL.normal_core_history_status(history, "v14-normal-core-history")
            history.write_bytes(b"normal-core-history-v1 count=1 entries=f000:ca60:fe380300\r\n")
            with self.assertRaisesRegex(TOOL.CaptureError, "canonical LF"):
                TOOL.normal_core_history_status(history, "v14-normal-core-history")

    def test_v14_early_stop_requires_the_exact_observed_history_boundary(self) -> None:
        with temporary_directory() as directory:
            root = Path(directory)
            history = root / "normal-core-history.raw"
            results = root / "results.raw"
            history.write_text("normal-core-history-v1 count=16 entries="
                + ",".join(TOOL.KNOWN_V14_NORMAL_CORE_HISTORY) + "\n", encoding="ascii")
            results.write_bytes(TOOL.KNOWN_V11_EARLY_STOP_RAW)
            status = TOOL.normal_core_history_boundary_status(
                history, results, "v14-normal-core-history", "known-unhandled-interrupt")
            self.assertIn("normal_core_history_boundary=observed-zero-context-to-default-callback\n", status)
            self.assertIn("normal_core_history_first=0e70:18e4:00000000\n", status)
            history.write_text("normal-core-history-v1 count=16 entries="
                + ",".join((*TOOL.KNOWN_V14_NORMAL_CORE_HISTORY[:-1], "f000:ca60:fe380301")) + "\n",
                encoding="ascii")
            with self.assertRaisesRegex(TOOL.CaptureError, "does not match"):
                TOOL.normal_core_history_boundary_status(
                    history, results, "v14-normal-core-history", "known-unhandled-interrupt")

    def test_v19_int93_receipt_reads_only_a_bounded_regular_event_stream(self) -> None:
        """The vector boundary cannot turn an external sidecar into unbounded I/O."""
        with temporary_directory() as directory:
            root = Path(directory)
            events = root / "events.raw"
            results = root / "results.raw"
            results.write_bytes(TOOL.KNOWN_V11_EARLY_STOP_RAW)
            events.write_bytes(TOOL.KNOWN_V19_INT93_EVENT)
            self.assertIn("int93_vector=observed-zero-ivt-target\n", TOOL.int93_vector_status(
                events, results, "v19-int93-vector", "known-unhandled-interrupt"))
            events.unlink()
            events.symlink_to("missing")
            with self.assertRaisesRegex(TOOL.CaptureError, "regular non-symlink"):
                TOOL.int93_vector_status(events, results, "v19-int93-vector", "known-unhandled-interrupt")
            events.unlink()
            events.write_bytes(b"x" * (TOOL.MAX_RAW_OBSERVATION_BYTES + 1))
            with self.assertRaisesRegex(TOOL.CaptureError, "bounded recorder contract"):
                TOOL.int93_vector_status(events, results, "v19-int93-vector", "known-unhandled-interrupt")

    def test_v20_title_entry_transfer_is_one_bounded_raw_adjacency(self) -> None:
        with temporary_directory() as directory:
            root = Path(directory)
            transfer = root / "title-entry-transfer.raw"
            results = root / "results.raw"
            results.write_bytes(TOOL.KNOWN_V11_EARLY_STOP_RAW)
            transfer.write_bytes(TOOL.KNOWN_V20_TITLE_ENTRY_TRANSFER)
            status = TOOL.title_entry_transfer_status(
                transfer, results, "v20-title-entry-transfer", "known-unhandled-interrupt")
            self.assertIn("title_entry_transfer_declared_entry_cs=0x0e70\n", status)
            self.assertIn("title_entry_transfer_predecessor_code=0xca00f00e\n", status)
            transfer.write_text(transfer.read_text(encoding="ascii").replace(
                "predecessor_valid=1", "predecessor_valid=0"), encoding="ascii")
            with self.assertRaisesRegex(TOOL.CaptureError, "invalid recorder record"):
                TOOL.title_entry_transfer_status(
                    transfer, results, "v20-title-entry-transfer", "known-unhandled-interrupt")
            transfer.unlink()
            transfer.symlink_to("missing")
            with self.assertRaisesRegex(TOOL.CaptureError, "regular non-symlink"):
                TOOL.title_entry_transfer_status(
                    transfer, results, "v20-title-entry-transfer", "known-unhandled-interrupt")

    def test_v21_int93_installation_is_optional_but_binds_one_reviewed_transaction(self) -> None:
        with temporary_directory() as directory:
            root = Path(directory)
            installation = root / "int93-installation.raw"
            self.assertEqual(TOOL.int93_installation_status(
                installation, "v21-int93-installation"), "int93_installation=absent\n")
            installation.write_text(
                "int93-installation-v1 image=titles.exe pc=0x1163 vector=0x93 "
                "ds=0x1234 dx=0x5678 target_preimage=0x01020304 "
                "vector_ip=0x5678 vector_cs=0x1234\n", encoding="ascii")
            status = TOOL.int93_installation_status(installation, "v21-int93-installation")
            self.assertIn("int93_installation=present\n", status)
            self.assertIn("int93_installation_opcode_preimage=0xc5161c01b89325cd21\n", status)
            self.assertIn("int93_installation_vector_cs=0x1234\n", status)
            installation.write_text(installation.read_text(encoding="ascii").replace(
                "vector_ip=0x5678", "vector_ip=0x5679"), encoding="ascii")
            with self.assertRaisesRegex(TOOL.CaptureError, "DS:DX target"):
                TOOL.int93_installation_status(installation, "v21-int93-installation")
            installation.write_text(installation.read_text(encoding="ascii").replace(
                "image=titles.exe pc=0x1163", "image=titles.exe pc=0x115c"), encoding="ascii")
            with self.assertRaisesRegex(TOOL.CaptureError, "reviewed installer candidate"):
                TOOL.int93_installation_status(installation, "v21-int93-installation")
            installation.unlink()
            installation.symlink_to("missing")
            with self.assertRaisesRegex(TOOL.CaptureError, "regular non-symlink"):
                TOOL.int93_installation_status(installation, "v21-int93-installation")

    def test_console_transcript_is_hashed_but_disk_bounded(self) -> None:
        """A recorder exception loop must not make cache or terminal output unbounded."""
        with temporary_directory() as directory:
            path = Path(directory) / "recorder-console.log"
            bytes_to_observe = b"x" * (TOOL.MAX_RECORDER_CONSOLE_LOG_BYTES + 17)
            over_limit = TOOL.threading.Event()
            status = TOOL.capture_bounded_console(io.BytesIO(bytes_to_observe), path, over_limit)
            self.assertEqual(status.total_bytes, len(bytes_to_observe))
            self.assertEqual(status.retained_bytes, TOOL.MAX_RECORDER_CONSOLE_LOG_BYTES)
            self.assertTrue(status.truncated)
            self.assertEqual(status.sha256, hashlib.sha256(bytes_to_observe).hexdigest())
            self.assertEqual(path.stat().st_size, TOOL.MAX_RECORDER_CONSOLE_LOG_BYTES)
            self.assertFalse(status.over_limit)
            receipt = TOOL.recorder_console_status(status)
            self.assertIn("recorder_console_truncated=true", receipt)
            self.assertIn("recorder_console_over_limit=false", receipt)

    def test_console_total_safety_cap_signals_for_child_termination(self) -> None:
        with temporary_directory() as directory:
            path = Path(directory) / "recorder-console.log"
            over_limit = TOOL.threading.Event()
            original_limit = TOOL.MAX_RECORDER_CONSOLE_TOTAL_BYTES
            try:
                TOOL.MAX_RECORDER_CONSOLE_TOTAL_BYTES = 16
                bytes_to_observe = b"x" * 17
                status = TOOL.capture_bounded_console(io.BytesIO(bytes_to_observe), path, over_limit)
                self.assertTrue(over_limit.is_set())
                self.assertTrue(status.over_limit)
                self.assertEqual(status.total_bytes, len(bytes_to_observe))
                self.assertEqual(path.stat().st_size, len(bytes_to_observe))
            finally:
                TOOL.MAX_RECORDER_CONSOLE_TOTAL_BYTES = original_limit


if __name__ == "__main__":
    unittest.main()
