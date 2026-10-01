"""Synthetic, media-free contracts for the experimental schema-25 runner."""
from __future__ import annotations

import importlib.util
import os
from pathlib import Path
import subprocess
import unittest
from unittest import mock

from eon_test_paths import temporary_directory

ROOT = Path(__file__).resolve().parents[1]


def load_runner():
    path = ROOT / "tools/run_millennium_dos_operand_capture.py"
    spec = importlib.util.spec_from_file_location("run_millennium_dos_operand_capture", path)
    assert spec and spec.loader
    module = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(module)
    return module


TOOL = load_runner()


class OperandCaptureRunnerTests(unittest.TestCase):
    def test_runner_is_fail_closed_before_source_media_inspection(self):
        self.assertEqual(TOOL.require_pinned_recorder(), (
            "58cbb12e9baaae22877908193fdf72b72ed3f3f42b5b76bb9a91bc0b093fd245",
            125_297_584,
        ))
        args = TOOL.parse_arguments([
            "--source-release", "/media/not-opened.zip",
            "--recorder", "/bin/not-opened",
            "--output", "/home/trv2/.cache/project-eon-tools/operand-captures/run-1",
            "--experimental-observer",
        ])
        with mock.patch.object(TOOL, "EXPECTED_RECORDER_SHA256", None), \
                mock.patch.object(TOOL, "EXPECTED_RECORDER_SIZE", None), \
                mock.patch.object(TOOL.legacy, "require_absolute_regular_file") as source_lookup:
            with self.assertRaisesRegex(ValueError, "no reviewed pinned SHA-256"):
                TOOL.run_capture(args)
            source_lookup.assert_not_called()

    def test_requires_explicit_experimental_switch(self):
        args = TOOL.parse_arguments([
            "--source-release", "/media/not-opened.zip",
            "--recorder", "/bin/not-opened",
            "--output", "/home/trv2/.cache/project-eon-tools/operand-captures/run-1",
        ])
        with self.assertRaisesRegex(ValueError, "requires --experimental-observer"):
            TOOL.run_capture(args)

    def test_configuration_keeps_recorder_v21_identity_and_opts_into_v25(self):
        with temporary_directory() as temporary:
            output = Path(temporary).resolve() / "run"
            config = TOOL.build_configuration(output, "svga_s3")
        self.assertIn("[project-eon-recorder-v21]", config)
        self.assertIn(f"outer_release_sha256={TOOL.legacy.EXPECTED_RELEASE_SHA256}", config)
        self.assertIn("[project-eon-diagnostics-v25]\nenabled=true\n", config)
        self.assertIn(f"sidecar_path={output / TOOL.protocol.OBSERVATION_NAME}\n", config)

    @unittest.skipUnless(os.name == "posix", "schema-25 output cache uses POSIX path semantics")
    def test_output_must_be_fresh_direct_child_of_external_cache(self):
        with temporary_directory() as temporary:
            root = Path(temporary).resolve()
            media = root / "media"
            media.mkdir()
            source = media / "release.zip"
            source.write_bytes(b"synthetic test fixture")
            cache = root / "external-cache" / "operand-captures"
            cache.mkdir(parents=True)
            TOOL.OUTPUT_ROOT = cache
            output = cache / "run-1"
            self.assertEqual(TOOL.output_directory(source, output), output)
            output.mkdir()
            with self.assertRaisesRegex(ValueError, "fresh"):
                TOOL.output_directory(source, output)
            with self.assertRaisesRegex(ValueError, "fresh named child"):
                TOOL.output_directory(source, cache / "../escape")

    def test_recorder_pin_requires_lowercase_sha256_and_positive_size(self):
        with mock.patch.object(TOOL, "EXPECTED_RECORDER_SHA256", "A" * 64), \
                mock.patch.object(TOOL, "EXPECTED_RECORDER_SIZE", 10):
            with self.assertRaisesRegex(ValueError, "no reviewed pinned SHA-256"):
                TOOL.require_pinned_recorder()
        with mock.patch.object(TOOL, "EXPECTED_RECORDER_SHA256", "a" * 64), \
                mock.patch.object(TOOL, "EXPECTED_RECORDER_SIZE", 0):
            with self.assertRaisesRegex(ValueError, "no reviewed pinned SHA-256"):
                TOOL.require_pinned_recorder()

    def test_findmnt_failure_after_mount_attempts_unmount(self):
        def fake_run(command, **kwargs):
            if command[0] == "archivemount":
                return mock.Mock(returncode=0)
            if command[0] == "findmnt":
                raise subprocess.CalledProcessError(1, command)
            if command[0] == "fusermount":
                return mock.Mock(returncode=0)
            self.fail(f"unexpected subprocess: {command!r}")

        mounted = [False]
        mountpoint = Path("/cache/archive-ro")
        with mock.patch.object(TOOL.subprocess, "run", side_effect=fake_run) as run:
            with self.assertRaises(subprocess.CalledProcessError):
                try:
                    TOOL.mount_archive(Path("/media/synthetic.zip"), mountpoint, mounted)
                finally:
                    if mounted[0]:
                        TOOL.unmount_archive(mountpoint)
        self.assertEqual([call.args[0][0] for call in run.call_args_list],
                         ["archivemount", "findmnt", "fusermount"])
        self.assertTrue(run.call_args_list[-1].kwargs["check"])


if __name__ == "__main__":
    unittest.main()
