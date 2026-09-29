"""Synthetic contract tests; no emulator or commercial data is used."""
import hashlib
import importlib.util
from pathlib import Path, PurePosixPath, PureWindowsPath
import unittest

from eon_test_paths import temporary_directory

SPEC = importlib.util.spec_from_file_location(
    "terminal_protocol", Path(__file__).resolve().parents[1] /
    "tools/millennium_dos_terminal_protocol.py")
TOOL = importlib.util.module_from_spec(SPEC)
SPEC.loader.exec_module(TOOL)


def observation(**overrides):
    values = dict.fromkeys(TOOL.SCALAR_NAMES, 0xffff)
    values.update(prefix_ip=0x134, interrupt=6, callback_cs=0xf000,
                  callback_ip=0xca64, stub_ip=0xca60, callback_index=3)
    values.update(overrides)
    return ("eon-int6-development-v1" + "".join(
        f"\t{key}={values[key]:04x}" for key in TOOL.SCALAR_NAMES) + "\n").encode()


def keys(count):
    return "".join(f"host-key {i} ticks=4294967295 state=down "
                   "scancode=0xffffffff sym=0xffffffff mod=0xffff\n"
                   for i in range(1, count + 1)).encode()


class TerminalProtocolTests(unittest.TestCase):
    def fixture(self, directory, count=0):
        original = TOOL.OUTPUT_ROOT / "synthetic-contract-test"
        legacy = TOOL._legacy()
        fields = dict(capture_receipt_version="24", recorder_protocol=TOOL.PROTOCOL,
                      recorder_admission=TOOL.RECORDER_ADMISSION,
                      capture_directory=str(original), machine_profile="svga_s3",
                      capture_intent="operator-input" if count else "diagnostic-no-key-delivery",
                      input_origin="unclassified-sdl-queue", input_timestamp="sdl2-key.timestamp-u32",
                      input_scope="guest-lifetime-including-internal-reboots",
                      environment_policy="isolated-xdg-no-overrides-v1",
                      observer_source_commit="234797680781567e18c374c9e62da24de5423db0",
                      observer_patch_sha256="23951d5c7cab7d18206f7f15eac352bc2901ab8bbd56b9693e621796aeb9efe5",
                      operator_procedure="visible-manual-window-close",
                      exit_status="0", termination_reason="emulator-exit",
                      host_input_records=str(count), start_unix="1000.1", end_unix="1001.2",
                      max_duration_seconds="15", source_release_sha256=legacy.EXPECTED_RELEASE_SHA256,
                      source_release_bytes=str(legacy.EXPECTED_RELEASE_SIZE),
                      recorder_sha256=TOOL.RECORDER_SHA256, recorder_bytes=str(TOOL.RECORDER_SIZE))
        payloads = dict(configuration=TOOL.build_configuration(original, "svga_s3").encode(),
                        int6_observation=observation(), host_input_receipt=keys(count),
                        recorder_console=b"synthetic console\n" + TOOL.SUCCESS_MARKER)
        for prefix, payload in payloads.items():
            self.replace(directory, fields, prefix, payload)
        return fields

    def replace(self, directory, fields, prefix, payload):
        (directory / TOOL.ARTIFACTS[prefix][0]).write_bytes(payload)
        fields[prefix + "_sha256"] = hashlib.sha256(payload).hexdigest()
        fields[prefix + "_bytes"] = str(len(payload))

    def verify(self, fields, directory):
        TOOL.verify_fields(fields, directory, allow_experimental_observer=True)

    def test_explicit_experimental_opt_in_and_empty_receipt(self):
        with temporary_directory() as temporary:
            directory = Path(temporary).resolve()
            fields = self.fixture(directory)
            with self.assertRaises(ValueError):
                TOOL.verify_fields(fields, directory)
            self.verify(fields, directory)
            fields = self.fixture(directory, 256)
            self.verify(fields, directory)

    def test_exact_fields_identities_procedure_and_times(self):
        with temporary_directory() as temporary:
            directory = Path(temporary).resolve()
            original = self.fixture(directory)
            mutations = dict(capture_receipt_version="23", recorder_protocol="v21",
                             recorder_admission="recovery", environment_policy="inherit",
                             observer_source_commit="unknown", observer_patch_sha256="0" * 64,
                             operator_procedure="synthetic-input",
                             source_release_sha256="0" * 64, recorder_bytes="01",
                             recorder_sha256="0" * 64, exit_status="86",
                             termination_reason="timeout", host_input_records="01",
                             capture_intent="operator-input", start_unix="NaN",
                             end_unix="999", max_duration_seconds="601",
                             capture_directory=str(TOOL.OUTPUT_ROOT / "x" / "y"))
            for name, value in mutations.items():
                with self.subTest(name=name), self.assertRaises(ValueError):
                    self.verify(dict(original, **{name: value}), directory)
            for value in ("Infinity", "1e3", "-1", "1045.2"):
                with self.subTest(time=value), self.assertRaises(ValueError):
                    self.verify(dict(original, end_unix=value), directory)
            with self.assertRaises(ValueError):
                self.verify(dict(original, extra="unreviewed"), directory)
            missing = original.copy()
            del missing["host_input_receipt_sha256"]
            with self.assertRaises(ValueError):
                self.verify(missing, directory)

    def test_rehashed_invalid_payloads_remain_rejected(self):
        with temporary_directory() as temporary:
            directory = Path(temporary).resolve()
            cases = [("configuration", b"unreviewed configuration\n"),
                     ("int6_observation", observation(interrupt=7)),
                     ("host_input_receipt", keys(1)),
                     ("recorder_console", b"truncated"),
                     ("recorder_console", TOOL.SUCCESS_MARKER * 2),
                     ("recorder_console", b"\r" + TOOL.SUCCESS_MARKER),
                     ("recorder_console", b"embedded " + TOOL.SUCCESS_MARKER)]
            for prefix, payload in cases:
                fields = self.fixture(directory)
                self.replace(directory, fields, prefix, payload)
                with self.subTest(prefix=prefix, payload=payload[:20]), self.assertRaises(ValueError):
                    self.verify(fields, directory)

    def test_dispatch_requires_opt_in_and_correct_kind(self):
        spec = importlib.util.spec_from_file_location(
            "capture_verifier", Path(__file__).resolve().parents[1] / "tools/verify_capture_receipt.py")
        verifier = importlib.util.module_from_spec(spec)
        spec.loader.exec_module(verifier)
        with temporary_directory() as temporary:
            directory = Path(temporary).resolve()
            fields = self.fixture(directory)
            (directory / "run-status.txt").write_text(
                "".join(f"{key}={value}\n" for key, value in fields.items()))
            with self.assertRaises(ValueError):
                verifier.verify("millennium-dos", directory)
            verifier.verify("millennium-dos", directory, allow_experimental_observer=True)
            with self.assertRaises(ValueError):
                verifier.verify("deuteros-amiga", directory, allow_experimental_observer=True)
            (directory / "run-status.txt").write_bytes(b"x" * (64 * 1024 + 1))
            with self.assertRaises(ValueError):
                verifier.receipt(directory / "run-status.txt")

    def test_scalar_grammar_fixed_boundary_and_opaque_values(self):
        self.assertEqual(TOOL.parse_int6(observation(prefix_cs=0))["prefix_cs"], 0)
        for name in ("prefix_ip", "interrupt", "callback_cs", "callback_ip", "stub_ip", "callback_index"):
            with self.subTest(name=name), self.assertRaises(ValueError):
                TOOL.parse_int6(observation(**{name: 0}))
        for payload in (observation().upper(), observation()[:-1], observation() + b"\n",
                        observation() + b"x" * TOOL.MAX_INT6_BYTES):
            with self.assertRaises(ValueError):
                TOOL.parse_int6(payload)

    def test_host_key_grammar_caps_and_unsigned_bits(self):
        for count in (0, 1, 256):
            self.assertEqual(TOOL.parse_host_input(keys(count)), count)
        for payload in (keys(257), keys(1).replace(b"host-key 1", b"host-key 2"),
                        keys(1).replace(b"4294967295", b"4294967296"),
                        keys(1).replace(b"0xffffffff", b"0x100000000"),
                        keys(1).replace(b"mod=0xffff", b"mod=0x10000"),
                        b"x" * (TOOL.MAX_HOST_INPUT_BYTES + 1)):
            with self.assertRaises(ValueError):
                TOOL.parse_host_input(payload)

    def test_bounded_reader_rejects_symlinks_and_nonregular(self):
        with temporary_directory() as temporary:
            directory = Path(temporary).resolve()
            payload = directory / "payload"
            payload.write_bytes(b"abc")
            self.assertEqual(TOOL.read_regular(payload, 3), b"abc")
            for path, limit in ((payload, 2), (directory, 100)):
                with self.assertRaises((ValueError, OSError)):
                    TOOL.read_regular(path, limit)
            with self.assertRaises(ValueError):
                TOOL.read_regular(Path("relative"), 100)
            link = directory / "link"
            try:
                link.symlink_to(payload)
            except OSError as error:
                if getattr(error, "winerror", None) != 1314:
                    raise
            else:
                with self.assertRaises(ValueError):
                    TOOL.read_regular(link, 3)

    def test_linux_provenance_is_independent_of_host_path_flavour(self):
        original = PurePosixPath(str(TOOL.OUTPUT_ROOT)) / "portable-case"
        self.assertEqual(TOOL.validate_capture_directory(original), original)
        self.assertIn(str(original / "archive-ro"),
                      TOOL.build_configuration(original, "svga_s3"))
        for path in (PureWindowsPath(str(original)),
                     PurePosixPath(str(TOOL.OUTPUT_ROOT) + "/bad\\name")):
            with self.assertRaises(ValueError):
                TOOL.validate_capture_directory(path)
        with temporary_directory() as temporary:
            directory = Path(temporary).resolve()
            fields = self.fixture(directory)
            for spelling in (str(original) + "/", str(TOOL.OUTPUT_ROOT) + "//portable-case",
                             str(TOOL.OUTPUT_ROOT) + "/./portable-case"):
                with self.subTest(spelling=spelling), self.assertRaises(ValueError):
                    self.verify(dict(fields, capture_directory=spelling), directory)


if __name__ == "__main__":
    unittest.main()
