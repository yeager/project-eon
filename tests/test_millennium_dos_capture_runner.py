"""Safety contracts for the external, operator-driven DOS capture helper."""

from __future__ import annotations

import hashlib
import importlib.util
import io
from pathlib import Path
import unittest
from unittest import mock

from eon_test_paths import CanonicalReceiptPath, LfTextFixtureWrites, temporary_directory


ROOT = Path(__file__).resolve().parents[1]
SPEC = importlib.util.spec_from_file_location(
    "run_millennium_dos_capture", ROOT / "tools" / "run_millennium_dos_capture.py")
assert SPEC and SPEC.loader
TOOL = importlib.util.module_from_spec(SPEC)
SPEC.loader.exec_module(TOOL)


class MillenniumDosCaptureRunnerTests(LfTextFixtureWrites, unittest.TestCase):
    @staticmethod
    def _driver_load_return(ordinal: int, pc: int, buffer: str = "none") -> str:
        return (f"driver-load-return-v1 ordinal={ordinal} image=mill.com cs=0e70 pc={pc:04x} "
                "ax=0001 bx=0002 cx=0010 dx=0000 si=0000 di=0000 ds=0e70 es=0e70 "
                f"ss=0e70 sp=ff00 flags=0000 buffer={buffer}\n")

    @staticmethod
    def _ah48_event(sequence: int, site: int, *, before_ax: int | None = None,
                    expected_ah: dict[int, int] | None = None) -> str:
        expected_ah = expected_ah or TOOL.AH48_EXPECTED_AH
        values = {name: (0x1000 + index) for index, name in enumerate(
            ("ax", "bx", "cx", "dx", "si", "di", "bp", "cs", "ip", "ds", "es", "ss", "sp", "flags"))}
        values.update({"ax": expected_ah[site] << 8 | 1, "cs": 0x0E70,
                       "ip": site + 2, "sp": 0xFF00, "flags": 0x7202})
        if before_ax is not None:
            values["ax"] = before_ax
        parts = [f"event", f"sequence={sequence}", f"site={site:04x}",
                 f"observed_return_ip={site + 2:04x}", "dispatch_return=before-IRET"]
        for phase in ("pre", "post"):
            for name, value in values.items():
                if phase == "post":
                    value = (value + 3) & 0xFFFF
                parts.append(f"{phase}_{name}={value:04x}")
        return "\t".join(parts) + "\n"

    def test_ah48_experimental_sidecar_is_bounded_hash_bound_and_diagnostic_only(self) -> None:
        protocol = TOOL.AH48_HANDLER_RETURN_PROTOCOL
        self.assertNotIn(protocol, TOOL.RECORDER_PROTOCOLS)
        digest = "3c3b6d9312d59cfada0f4b75466f90da5193e61e0450271cbe268fbb2b85c65c"
        self.assertEqual(TOOL.EXPERIMENTAL_OBSERVER_PROTOCOLS[protocol], {digest})
        self.assertEqual(TOOL.EXPERIMENTAL_OBSERVER_SIZES[protocol][digest], 122_502_752)
        self.assertEqual(TOOL.EXPERIMENTAL_OBSERVER_RECEIPT_VERSIONS[protocol], "22")
        with mock.patch.object(TOOL, "sha256_file", return_value=(digest, 122_502_752)):
            self.assertEqual(TOOL.validate_recorder(Path("/external/dosbox-x"), {digest}),
                             (digest, 122_502_752))
        args = TOOL.parse_arguments((
            "--source-release", "/release.zip", "--recorder", "/recorder", "--output", "/capture",
            "--capture-intent", "physical-input", "--recorder-protocol", protocol,
            "--experimental-observer",
        ))
        self.assertEqual(args.capture_intent, "physical-input")
        self.assertIn("visible manual input required", TOOL.experimental_ah48_operator_instructions()[0])
        self.assertIn("operating-system window controls", TOOL.experimental_ah48_operator_instructions()[1])

        with temporary_directory() as directory:
            root = Path(directory)
            generated = CanonicalReceiptPath(root / "dos-ah48-test.tsv")
            rows = "".join(self._ah48_event(sequence, site) for sequence, site in enumerate(
                (0x1B26, 0x1B2D, 0x1B38, 0x1B3F), start=1))
            payload = (TOOL.AH48_HEADER.decode("ascii") + rows + "end\tstatus=complete\tevents=4\n")
            generated.write_text(payload, encoding="ascii")
            generated.chmod(0o600)
            status = TOOL.ah48_handler_return_status(root, protocol)
            self.assertIn("ah48_return=present\n", status)
            self.assertIn("ah48_return_events=4\n", status)
            self.assertIn("ah48_return_site_counts=0x1b26:1,0x1b2d:1,0x1b38:1,0x1b3f:1\n", status)
            self.assertTrue((root / "dos-ah48-return.raw").is_file())
            self.assertFalse(generated.exists())

    def test_ah48_sidecar_rejects_wrong_sites_overflow_and_truncation(self) -> None:
        protocol = TOOL.AH48_HANDLER_RETURN_PROTOCOL
        with temporary_directory() as directory:
            root = Path(directory)
            header = TOOL.AH48_HEADER.decode("ascii")
            row = self._ah48_event(1, 0x1B2D)
            cases = (
                (row.replace("site=1b2d", "site=1b30", 1) + "end\tstatus=complete\tevents=1\n", "exact reviewed INT 21h site"),
                (row.replace("pre_ax=4801", "pre_ax=4a01", 1) + "end\tstatus=complete\tevents=1\n", "exact reviewed INT 21h site"),
                (row + "overflow\treason=event-limit\tevents=1\nend\tstatus=overflow\tevents=1\n", "overflowed"),
                (row, "completion records"),
            )
            for index, (body, message) in enumerate(cases):
                candidate = root / f"dos-ah48-{index}.tsv"
                candidate.write_text(header + body, encoding="ascii")
                candidate.chmod(0o600)
                with self.assertRaisesRegex(TOOL.CaptureError, message):
                    TOOL.ah48_handler_return_status(root, protocol)
                candidate.unlink()

    def test_ah48_v2_is_separate_and_accepts_only_the_third_allocator_site(self) -> None:
        protocol = TOOL.AH48_SITE_1B4F_RETURN_PROTOCOL
        self.assertNotIn(protocol, TOOL.RECORDER_PROTOCOLS)
        digest = "75ddc62f68817bfb37cb59d8a283ea06e89cf8d46104bf3458c0d99fcfa2b6ef"
        self.assertEqual(TOOL.EXPERIMENTAL_OBSERVER_PROTOCOLS[protocol], {digest})
        self.assertEqual(TOOL.EXPERIMENTAL_OBSERVER_SIZES[protocol][digest], 122_505_616)
        self.assertEqual(TOOL.AH48_SITE_1B4F_EXPECTED_AH, {0x1B4F: 0x48})
        with mock.patch.object(TOOL, "sha256_file", return_value=(digest, 122_505_616)):
            self.assertEqual(TOOL.validate_recorder(Path("/external/dosbox-x-v2"), {digest}),
                             (digest, 122_505_616))
        with temporary_directory() as directory:
            root = Path(directory)
            row = self._ah48_event(1, 0x1B4F, expected_ah=TOOL.AH48_SITE_1B4F_EXPECTED_AH)
            candidate = CanonicalReceiptPath(root / "dos-ah48-v2.tsv")
            candidate.write_text(TOOL.AH48_SITE_1B4F_HEADER.decode("ascii")
                                 + row + "end\tstatus=complete\tevents=1\n", encoding="ascii")
            candidate.chmod(0o600)
            status = TOOL.ah48_handler_return_status(root, protocol)
            self.assertIn("ah48_return_site_counts=0x1b4f:1\n", status)
            self.assertIn("ah48_return_boundary=dos-handler-before-iret-site-1b4f\n", status)

        wrong_site = row.replace("site=1b4f", "site=1b2d", 1)
        wrong_site = wrong_site.replace("observed_return_ip=1b51", "observed_return_ip=1b2f", 1)
        wrong_site = wrong_site.replace("pre_ip=1b51", "pre_ip=1b2f", 1)
        with temporary_directory() as directory:
            candidate = Path(directory) / "dos-ah48-v2-wrong-site.tsv"
            candidate.write_text(TOOL.AH48_SITE_1B4F_HEADER.decode("ascii") + wrong_site
                                 + "end\tstatus=complete\tevents=1\n", encoding="ascii")
            candidate.chmod(0o600)
            with self.assertRaisesRegex(TOOL.CaptureError, "exact reviewed INT 21h site"):
                TOOL.ah48_handler_return_status(candidate, protocol)

        wrong_header = TOOL.AH48_HEADER.decode("ascii") + self._ah48_event(1, 0x1B26)
        with temporary_directory() as directory:
            root = Path(directory)
            candidate = root / "dos-ah48-wrong-version.tsv"
            candidate.write_text(wrong_header + "end\tstatus=complete\tevents=1\n", encoding="ascii")
            candidate.chmod(0o600)
            with self.assertRaisesRegex(TOOL.CaptureError, "invalid header"):
                TOOL.ah48_handler_return_status(candidate, protocol)

    def test_ah48_v3_requires_exact_allocator_and_matching_free_sites(self) -> None:
        protocol = TOOL.AH48_SITES_1BCA_1BD5_RETURN_PROTOCOL
        self.assertNotIn(protocol, TOOL.RECORDER_PROTOCOLS)
        digest = "c8d0795e32e566a7a998ef9f26542fd85652f27b957144a06858181ae9379d41"
        self.assertEqual(TOOL.EXPERIMENTAL_OBSERVER_PROTOCOLS[protocol], {digest})
        self.assertEqual(TOOL.EXPERIMENTAL_OBSERVER_SIZES[protocol][digest], 134_030_152)
        self.assertEqual(TOOL.EXPERIMENTAL_OBSERVER_RECEIPT_VERSIONS[protocol], "22")
        with mock.patch.object(TOOL, "sha256_file", return_value=(digest, 134_030_152)):
            self.assertEqual(TOOL.validate_recorder(Path("/external/dosbox-x-v3"), {digest}),
                             (digest, 134_030_152))
        self.assertEqual(TOOL.AH48_SITES_1BCA_1BD5_EXPECTED_AH,
                         {0x1BCA: 0x48, 0x1BD5: 0x49})
        with temporary_directory() as directory:
            root = Path(directory)
            rows = (self._ah48_event(1, 0x1BCA,
                                     expected_ah=TOOL.AH48_SITES_1BCA_1BD5_EXPECTED_AH)
                    + self._ah48_event(2, 0x1BD5,
                                       expected_ah=TOOL.AH48_SITES_1BCA_1BD5_EXPECTED_AH))
            candidate = CanonicalReceiptPath(root / "dos-ah48-v3.tsv")
            candidate.write_text(TOOL.AH48_SITES_1BCA_1BD5_HEADER.decode("ascii")
                                 + rows + "end\tstatus=complete\tevents=2\n", encoding="ascii")
            candidate.chmod(0o600)
            status = TOOL.ah48_handler_return_status(root, protocol)
            self.assertIn("ah48_return_events=2\n", status)
            self.assertIn("ah48_return_site_counts=0x1bca:1,0x1bd5:1\n", status)
            self.assertIn("ah48_return_boundary=dos-handler-before-iret-sites-1bca-1bd5\n", status)

        for wrong_site, wrong_ah in ((0x1BCA, 0x49), (0x1BD3, 0x48)):
            with temporary_directory() as directory:
                root = Path(directory)
                record = self._ah48_event(1, wrong_site,
                    expected_ah={wrong_site: wrong_ah})
                candidate = CanonicalReceiptPath(root / "dos-ah48-wrong.tsv")
                candidate.write_text(TOOL.AH48_SITES_1BCA_1BD5_HEADER.decode("ascii")
                                     + record + "end\tstatus=complete\tevents=1\n",
                                     encoding="ascii")
                candidate.chmod(0o600)
                with self.assertRaisesRegex(TOOL.CaptureError,
                                            "exact reviewed INT 21h site|missing its exact"):
                    TOOL.ah48_handler_return_status(candidate, protocol)

    def test_ah48_observer_requires_private_direct_cache_output(self) -> None:
        with temporary_directory() as directory:
            home = Path(directory)
            cache = home / ".cache"
            cache.mkdir(mode=0o700)
            output = cache / "millennium-dos-capture-test"
            environment = {"HOME": str(home)}
            TOOL.require_ah48_cache_output(output, environment)
            with self.assertRaisesRegex(TOOL.CaptureError, "direct child"):
                TOOL.require_ah48_cache_output(output / "nested", environment)
            cache.chmod(0o770)
            with self.assertRaisesRegex(TOOL.CaptureError, "non-group/world-writable"):
                TOOL.require_ah48_cache_output(output, environment)

    def test_driver_load_return_sidecar_is_bounded_and_preserves_opaque_results(self) -> None:
        with temporary_directory() as directory:
            sidecar = CanonicalReceiptPath(Path(directory) / "driver-load-return.raw")
            self.assertEqual(TOOL.driver_load_return_status(sidecar, "millennium-dos-en-driver-load-return-v1"),
                             "driver_load_return=absent\n")
            self.assertEqual(TOOL.driver_load_return_status(sidecar, "v21-int93-installation"), "")
            payload = (self._driver_load_return(1, 0x02D4)
                       + self._driver_load_return(2, 0x0315, "ds0:4366:" + "a" * 64)
                       + "driver-load-return-end count=2 overflow=0\n")
            sidecar.write_text(payload, encoding="ascii")
            status = TOOL.driver_load_return_status(sidecar, "millennium-dos-en-driver-load-return-v1")
            self.assertIn("driver_load_return=present\n", status)
            self.assertIn("driver_load_return_records=2\n", status)
            self.assertIn("driver_load_return_site_counts=0x02d4:1,0x0315:1\n", status)
            self.assertIn("driver_load_return_buffer_digests=1\n", status)
            self.assertIn(hashlib.sha256(payload.encode("ascii")).hexdigest(), status)

            sidecar.write_text(self._driver_load_return(2, 0x02D4)
                               + "driver-load-return-end count=1 overflow=0\n", encoding="ascii")
            with self.assertRaisesRegex(TOOL.CaptureError, "ordinals"):
                TOOL.driver_load_return_status(sidecar, "millennium-dos-en-driver-load-return-v1")
            sidecar.write_text(self._driver_load_return(1, 0x02D5)
                               + "driver-load-return-end count=1 overflow=0\n", encoding="ascii")
            with self.assertRaisesRegex(TOOL.CaptureError, "unreviewed instruction site"):
                TOOL.driver_load_return_status(sidecar, "millennium-dos-en-driver-load-return-v1")
            sidecar.write_text(self._driver_load_return(1, 0x02D4, "ds0:32:" + "a" * 64)
                               + "driver-load-return-end count=1 overflow=0\n",
                               encoding="ascii")
            with self.assertRaisesRegex(TOOL.CaptureError, "detached from the read return"):
                TOOL.driver_load_return_status(sidecar, "millennium-dos-en-driver-load-return-v1")
            sidecar.write_text(self._driver_load_return(1, 0x0315, "ds0:4367:" + "a" * 64)
                               + "driver-load-return-end count=1 overflow=0\n",
                               encoding="ascii")
            with self.assertRaisesRegex(TOOL.CaptureError, "size bound"):
                TOOL.driver_load_return_status(sidecar, "millennium-dos-en-driver-load-return-v1")
            sidecar.write_text("driver-load-return-overflow cap=128\n"
                               "driver-load-return-end count=128 overflow=1\n", encoding="ascii")
            with self.assertRaisesRegex(TOOL.CaptureError, "overflowed"):
                TOOL.driver_load_return_status(sidecar, "millennium-dos-en-driver-load-return-v1")

            sidecar.write_text(self._driver_load_return(1, 0x02D4), encoding="ascii")
            with self.assertRaisesRegex(TOOL.CaptureError, "incomplete or overflowed"):
                TOOL.driver_load_return_status(sidecar, "millennium-dos-en-driver-load-return-v1")

            sidecar.write_bytes(b"x" * (TOOL.MAX_DRIVER_LOAD_RETURN_BYTES + 1))
            with self.assertRaisesRegex(TOOL.CaptureError, "bounded recorder contract"):
                TOOL.driver_load_return_status(sidecar, "millennium-dos-en-driver-load-return-v1")
            sidecar.unlink()
            sidecar.symlink_to("missing")
            with self.assertRaisesRegex(TOOL.CaptureError, "regular non-symlink"):
                TOOL.driver_load_return_status(sidecar, "millennium-dos-en-driver-load-return-v1")

    def test_rejects_headless_or_missing_visible_display(self) -> None:
        with self.assertRaisesRegex(TOOL.CaptureError, "headless SDL"):
            TOOL.require_visible_operator_input({"SDL_VIDEODRIVER": "dummy", "DISPLAY": ":1"})
        with self.assertRaisesRegex(TOOL.CaptureError, "visible X11 or Wayland"):
            TOOL.require_visible_operator_input({})
        TOOL.require_visible_operator_input({"WAYLAND_DISPLAY": "wayland-0"})

    def test_experimental_driver_loader_protocol_is_separate_from_recovery_pins(self) -> None:
        protocol = "millennium-dos-en-driver-load-return-v1"
        self.assertNotIn(protocol, TOOL.RECORDER_PROTOCOLS)
        accepted_hashes = TOOL.EXPERIMENTAL_OBSERVER_PROTOCOLS[protocol]
        self.assertEqual(len(accepted_hashes), 2)
        self.assertTrue(all(len(digest) == 64 for digest in accepted_hashes))
        self.assertEqual(set(TOOL.EXPERIMENTAL_OBSERVER_SIZES[protocol]), accepted_hashes)
        self.assertEqual(TOOL.EXPERIMENTAL_OBSERVER_RECEIPT_VERSIONS[protocol], "22")
        linux_hash = "942f30f2199350a51d0ff1e7c024f4e226d29b5576dc6ea0d61b7556c12468e8"
        with mock.patch.object(TOOL, "sha256_file", return_value=(linux_hash, 132_933_552)):
            self.assertEqual(TOOL.validate_recorder(Path("/external/dosbox-x"), accepted_hashes),
                             (linux_hash, 132_933_552))
        with mock.patch.object(TOOL, "sha256_file", return_value=("0" * 64, 132_933_552)):
            with self.assertRaisesRegex(TOOL.CaptureError, "does not match"):
                TOOL.validate_recorder(Path("/external/dosbox-x"), accepted_hashes)
        args = TOOL.parse_arguments((
            "--source-release", "/release.zip", "--recorder", "/recorder", "--output", "/capture",
            "--capture-intent", "diagnostic-no-input", "--recorder-protocol", protocol,
            "--experimental-observer",
        ))
        self.assertEqual(args.recorder_protocol, protocol)
        self.assertTrue(args.experimental_observer)
        self.assertIn("operating-system window controls",
                      TOOL.experimental_driver_return_operator_instructions()[1])

    def test_experimental_graceful_close_protocol_has_a_distinct_exact_pin(self) -> None:
        protocol = TOOL.DRIVER_LOAD_RETURN_GRACEFUL_CLOSE_PROTOCOL
        self.assertEqual(protocol, "millennium-dos-en-driver-load-return-v2")
        self.assertNotIn(protocol, TOOL.RECORDER_PROTOCOLS)
        accepted_hashes = TOOL.EXPERIMENTAL_OBSERVER_PROTOCOLS[protocol]
        self.assertEqual(accepted_hashes, {
            "a03a43372a31b89514007c5053a8890c2b13a653c1562c379c3eaee6edc030e6"})
        self.assertEqual(TOOL.EXPERIMENTAL_OBSERVER_SIZES[protocol], {
            "a03a43372a31b89514007c5053a8890c2b13a653c1562c379c3eaee6edc030e6": 132_934_416})
        self.assertEqual(TOOL.EXPERIMENTAL_OBSERVER_RECEIPT_VERSIONS[protocol], "22")
        config = TOOL.recorder_config(Path("/safe/read-only/game-root"), graceful_host_close=True)
        self.assertIn("quit warning=false", config)
        args = TOOL.parse_arguments((
            "--source-release", "/release.zip", "--recorder", "/recorder", "--output", "/capture",
            "--capture-intent", "diagnostic-no-input", "--recorder-protocol", protocol,
            "--experimental-observer",
        ))
        self.assertEqual(args.recorder_protocol, protocol)

    def test_generated_configuration_handles_real_mode_wrap_and_never_injects_input(self) -> None:
        configuration = TOOL.recorder_config(Path("/safe/read-only/game-root"))
        self.assertIn("core=normal", configuration)
        self.assertIn("segment limits=false", configuration)
        self.assertIn("machine=svga_s3", configuration)
        self.assertNotIn("quit warning=false", configuration)
        self.assertIn("[log]\nlogfile=/dev/null\ncpu=never\n", configuration)
        self.assertIn('mount c "/safe/read-only/game-root"', configuration)
        self.assertNotIn("AUTOTYPE", configuration)
        self.assertNotIn("KEYBOARD_AddKey", configuration)

    def test_machine_profiles_are_finite_and_rendered_verbatim(self) -> None:
        self.assertIn("machine=ega", TOOL.recorder_config(Path("/safe/read-only/game-root"), "ega"))
        with self.assertRaisesRegex(TOOL.CaptureError, "finite profile"):
            TOOL.recorder_config(Path("/safe/read-only/game-root"), "vgaonly")

    def test_v21_identity_section_is_config_bound_and_complete(self) -> None:
        configuration = TOOL.recorder_config(
            Path("/safe/read-only/game-root"), release_sha256=TOOL.EXPECTED_RELEASE_SHA256)
        self.assertIn("[project-eon-recorder-v21]", configuration)
        self.assertIn(f"outer_release_sha256={TOOL.EXPECTED_RELEASE_SHA256}", configuration)
        self.assertIn(f"direct_media_set_sha256={TOOL.EXPECTED_DIRECT_MEDIA_SET_SHA256}", configuration)
        self.assertIn(f"titles_exe_sha256={TOOL.EXPECTED_TITLES_EXE_SHA256}", configuration)
        self.assertIn(f"titles_exe_bytes={TOOL.EXPECTED_TITLES_EXE_SIZE}", configuration)
        self.assertIn(f"2200ad_exe_sha256={TOOL.EXPECTED_2200AD_EXE_SHA256}", configuration)
        self.assertIn(f"2200ad_exe_bytes={TOOL.EXPECTED_2200AD_EXE_SIZE}", configuration)

    def test_v21_identity_section_rejects_another_outer_release(self) -> None:
        with self.assertRaisesRegex(TOOL.CaptureError, "recognised DOS release hash"):
            TOOL.recorder_config(Path("/safe/read-only/game-root"), release_sha256="0" * 64)

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
                with self.assertRaisesRegex(TOOL.CaptureError, "expected SHA-256 " + "0" * 64):
                    TOOL.validate_recorder(recorder)
                with self.assertRaisesRegex(TOOL.CaptureError, hashlib.sha256(recorder.read_bytes()).hexdigest()):
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
        experimental = TOOL.parse_arguments((
            "--source-release", "/release.zip", "--recorder", "/recorder", "--output", "/capture",
            "--capture-intent", "diagnostic-no-input", "--recorder-protocol", "v21-int93-installation",
            "--experimental-observer",
        ))
        self.assertTrue(experimental.experimental_observer)
        self.assertEqual(TOOL.experimental_observer_raw_status(),
                         "events_raw=not-collected\nresults_raw=not-collected\n")
        with self.assertRaises(SystemExit), mock.patch("sys.stderr", new_callable=io.StringIO):
            TOOL.parse_arguments((
                "--source-release", "/release.zip", "--recorder", "/recorder", "--output", "/capture",
            ))

    def test_capture_intent_fails_closed_against_the_recorder_input_receipt(self) -> None:
        present = ("host_input_receipt=present\n"
                   "host_input_receipt_sha256=" + "a" * 64 + "\n"
                   "host_input_receipt_bytes=1\nhost_input_receipt_records=1\n")
        self.assertEqual(TOOL.capture_intent_status("physical-input", present, True),
                         "capture_intent=physical-input\ncapture_intent_input_requirement=required\n"
                         "capture_intent_input_observation=observed-during-capture\n")
        self.assertEqual(TOOL.capture_intent_status("diagnostic-no-input", "host_input_receipt=absent\n", False),
                         "capture_intent=diagnostic-no-input\ncapture_intent_input_requirement=forbidden\n"
                         "capture_intent_input_observation=none\n")
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

    def test_physical_input_never_uses_the_diagnostic_early_stop(self) -> None:
        with temporary_directory() as directory:
            results = Path(directory) / "results.raw"
            results.write_bytes(TOOL.KNOWN_V11_EARLY_STOP_RAW)
            self.assertTrue(TOOL.should_stop_for_known_unhandled_interrupt(
                "diagnostic-no-input", results))
            self.assertFalse(TOOL.should_stop_for_known_unhandled_interrupt(
                "physical-input", results))
            with self.assertRaisesRegex(TOOL.CaptureError, "finite set"):
                TOOL.should_stop_for_known_unhandled_interrupt("unreviewed", results)

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
            history.write_bytes(
                b"normal-core-history-v1 count=2 entries=0e70:18fe:00000000,f000:ca60:fe380300\n")
            status = TOOL.normal_core_history_status(history, "v14-normal-core-history")
            self.assertIn("normal_core_history=present\n", status)
            self.assertIn("normal_core_history_entries=2\n", status)
            history.write_bytes(
                b"normal-core-history-v1 count=1 entries=0e70:18fe:00000000,f000:ca60:fe380300\n")
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
            history.write_bytes(("normal-core-history-v1 count=16 entries="
                + ",".join(TOOL.KNOWN_V14_NORMAL_CORE_HISTORY) + "\n").encode("ascii"))
            results.write_bytes(TOOL.KNOWN_V11_EARLY_STOP_RAW)
            status = TOOL.normal_core_history_boundary_status(
                history, results, "v14-normal-core-history", "known-unhandled-interrupt")
            self.assertIn("normal_core_history_boundary=observed-zero-context-to-default-callback\n", status)
            self.assertIn("normal_core_history_first=0e70:18e4:00000000\n", status)
            history.write_bytes(("normal-core-history-v1 count=16 entries="
                + ",".join((*TOOL.KNOWN_V14_NORMAL_CORE_HISTORY[:-1], "f000:ca60:fe380301")) + "\n").encode("ascii"))
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
            transfer.write_bytes(transfer.read_bytes().replace(
                b"predecessor_valid=1", b"predecessor_valid=0"))
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
            installation.write_bytes((
                "int93-installation-v1 image=titles.exe pc=0x1163 vector=0x93 "
                "ds=0x1234 dx=0x5678 target_preimage=0x01020304 "
                "vector_ip=0x5678 vector_cs=0x1234\n").encode("ascii"))
            status = TOOL.int93_installation_status(installation, "v21-int93-installation")
            self.assertIn("int93_installation=present\n", status)
            self.assertIn("int93_installation_opcode_preimage=0xc5161c01b89325cd21\n", status)
            self.assertIn("int93_installation_vector_cs=0x1234\n", status)
            installation.write_bytes(installation.read_bytes().replace(
                b"vector_ip=0x5678", b"vector_ip=0x5679"))
            with self.assertRaisesRegex(TOOL.CaptureError, "DS:DX target"):
                TOOL.int93_installation_status(installation, "v21-int93-installation")
            installation.write_bytes(installation.read_bytes().replace(
                b"image=titles.exe pc=0x1163", b"image=titles.exe pc=0x115c"))
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
