"""Pure boundary tests for the external reference-trace assembler.

The tests use temporary non-game bytes and never execute an emulator, mount
media, or retain an event stream fixture in the repository.
"""

from __future__ import annotations

import importlib.util
import hashlib
import json
from pathlib import Path
import unittest

from eon_test_paths import temporary_directory


ROOT = Path(__file__).resolve().parents[1]
SPEC = importlib.util.spec_from_file_location(
    "record_reference_trace", ROOT / "tools" / "record_reference_trace.py")
assert SPEC and SPEC.loader
TOOL = importlib.util.module_from_spec(SPEC)
SPEC.loader.exec_module(TOOL)


def metadata_lines(**overrides: str) -> str:
    fields = {
        "format": "project-eon-reference-trace-v1",
        "game": "millennium",
        "platform": "dos",
        "language": "en",
        "capture_start_utc": "2026-08-29T00:00:00Z",
        "capture_end_utc": "2026-08-29T00:00:01Z",
        "emulator_name": "external-emulator",
        "emulator_version": "1.0",
        "emulator_sha256": "a" * 64,
        "config_sha256": "b" * 64,
        "command_tail_sha256": "c" * 64,
        "input_timeline_sha256": "d" * 64,
    }
    fields.update(overrides)
    return "".join(f"{key}\t{value}\n" for key, value in fields.items())


def write_metadata(path: Path, **overrides: str) -> None:
    """Write the required LF-only metadata bytes without host newline conversion."""
    path.write_bytes(metadata_lines(**overrides).encode("utf-8"))


def write_provenance_preimages(root: Path) -> tuple[Path, Path, Path, dict[str, str]]:
    """Create distinct non-media capture inputs with their real hashes."""
    configuration = root / "configuration"
    command_tail = root / "command-tail"
    input_timeline = root / "input-timeline"
    configuration.write_bytes(b"[cpu]\ncore=normal\n")
    command_tail.write_bytes(b"dosbox-x --conf recorder.conf\n")
    input_timeline.write_bytes(b"no guest input\n")
    overrides = {
        "config_sha256": hashlib.sha256(configuration.read_bytes()).hexdigest(),
        "command_tail_sha256": hashlib.sha256(command_tail.read_bytes()).hexdigest(),
        "input_timeline_sha256": hashlib.sha256(input_timeline.read_bytes()).hexdigest(),
    }
    return configuration, command_tail, input_timeline, overrides


class RecordReferenceTraceTests(unittest.TestCase):
    def test_test_metadata_fixture_preserves_lf_bytes_on_every_host(self):
        with temporary_directory() as directory:
            path = Path(directory) / "metadata.tsv"
            write_metadata(path)
            self.assertNotIn(b"\r", path.read_bytes())

    def test_metadata_has_no_assembler_owned_fields(self):
        with temporary_directory() as directory:
            path = Path(directory) / "metadata.tsv"
            write_metadata(path, event_size="1")
            with self.assertRaises(TOOL.EvidenceError):
                TOOL.validate_metadata(TOOL.parse_metadata(path.resolve()), {
                    "game": "millennium", "platform": "dos", "language": "en"})

    def test_v2_adapter_cannot_cross_release_identity(self):
        with temporary_directory() as directory:
            path = Path(directory) / "metadata.tsv"
            write_metadata(path,
                format="project-eon-reference-trace-v2",
                adapter="millennium-dos-en-startup-v1",
                platform="amiga")
            with self.assertRaises(TOOL.EvidenceError):
                TOOL.validate_metadata(TOOL.parse_metadata(path.resolve()), {
                "game": "millennium", "platform": "amiga", "language": "en"})

    def test_v2_adapter_requires_its_exact_outer_release_identity(self):
        with temporary_directory() as directory:
            path = Path(directory) / "metadata.tsv"
            write_metadata(path,
                format="project-eon-reference-trace-v2",
                adapter="millennium-dos-en-startup-v1")
            with self.assertRaisesRegex(TOOL.EvidenceError, "exact source sha256"):
                TOOL.validate_metadata(TOOL.parse_metadata(path.resolve()), {
                    "sha256": "0" * 64, "size": 328383, "game": "millennium",
                "platform": "dos", "language": "en"})

    def test_gx_v2_adapter_requires_the_clean_millennium_dos_release(self):
        with temporary_directory() as directory:
            path = Path(directory) / "metadata.tsv"
            write_metadata(path,
                format="project-eon-reference-trace-v2",
                adapter="millennium-dos-en-gx-startup-v2")
            identity = {
                "sha256": "e6e7044b25877fdf8b10d16d2f395886d9957953144ae15ca630cda9cab2a123",
                "size": 328383, "game": "millennium", "platform": "dos", "language": "en"}
            TOOL.validate_metadata(TOOL.parse_metadata(path.resolve()), identity)

    def test_v3_title_bridge_requires_the_exact_amiga_media_and_stage(self):
        with temporary_directory() as directory:
            path = Path(directory) / "metadata.tsv"
            write_metadata(path,
                format="project-eon-reference-trace-v3",
                adapter="deuteros-amiga-en-title-bridge-v3",
                game="deuteros", platform="amiga",
                source_media_sha256="6ea0cc68d3af37203a885032eddf7c28e839e6abb59d8c9cd3792f1308bdec38",
                source_stage_sha256="48d65260e9b5f5cbf8d8b3675a178c81b8764810b61a6a2539a56dcb40a8de03")
            TOOL.validate_metadata(TOOL.parse_metadata(path.resolve()), {
                "sha256": "f4dc8dd1c27c5d389837783becd9b95ab09b78baf40e94e39e2b7e590e470e04",
                "size": 4066771, "game": "deuteros", "platform": "amiga", "language": "en"})

    def test_v3_main_copy_loop_requires_the_exact_amiga_media_and_main_stage(self):
        with temporary_directory() as directory:
            path = Path(directory) / "metadata.tsv"
            write_metadata(path,
                format="project-eon-reference-trace-v3",
                adapter="deuteros-amiga-en-main-copy-loop-v3",
                game="deuteros", platform="amiga",
                source_media_sha256="6ea0cc68d3af37203a885032eddf7c28e839e6abb59d8c9cd3792f1308bdec38",
                source_stage_sha256="a82c0d6a12e156e0832d632a6c40dd58713a00b611dbcba7289aa16b0969a0a6")
            identity = {
                "sha256": "f4dc8dd1c27c5d389837783becd9b95ab09b78baf40e94e39e2b7e590e470e04",
                "size": 4066771, "game": "deuteros", "platform": "amiga", "language": "en"}
            TOOL.validate_metadata(TOOL.parse_metadata(path.resolve()), identity)
            write_metadata(path,
                format="project-eon-reference-trace-v3",
                adapter="deuteros-amiga-en-main-copy-loop-v3",
                game="deuteros", platform="amiga",
                source_media_sha256="6ea0cc68d3af37203a885032eddf7c28e839e6abb59d8c9cd3792f1308bdec38",
                source_stage_sha256="0" * 64)
            with self.assertRaisesRegex(TOOL.EvidenceError, "source_stage_sha256"):
                TOOL.validate_metadata(TOOL.parse_metadata(path.resolve()), identity)

    def test_v5_title_display_metadata_is_bound_but_artifacts_remain_assembler_owned(self):
        with temporary_directory() as directory:
            path = Path(directory) / "metadata.tsv"
            write_metadata(path,
                format="project-eon-reference-trace-v5",
                adapter="deuteros-amiga-en-title-display-artifacts-v5",
                game="deuteros", platform="amiga",
                source_media_sha256="6ea0cc68d3af37203a885032eddf7c28e839e6abb59d8c9cd3792f1308bdec38",
                source_stage_sha256="48d65260e9b5f5cbf8d8b3675a178c81b8764810b61a6a2539a56dcb40a8de03")
            identity = {
                "sha256": "f4dc8dd1c27c5d389837783becd9b95ab09b78baf40e94e39e2b7e590e470e04",
                "size": 4066771, "game": "deuteros", "platform": "amiga", "language": "en"}
            TOOL.validate_metadata(TOOL.parse_metadata(path.resolve()), identity)
            self.assertIn("project-eon-reference-trace-v5", TOOL.metadata_template(
                "deuteros-amiga-en-title-display-artifacts-v5"))
            write_metadata(path,
                format="project-eon-reference-trace-v5",
                adapter="deuteros-amiga-en-title-display-artifacts-v5",
                game="deuteros", platform="amiga",
                source_media_sha256="6ea0cc68d3af37203a885032eddf7c28e839e6abb59d8c9cd3792f1308bdec38",
                source_stage_sha256="48d65260e9b5f5cbf8d8b3675a178c81b8764810b61a6a2539a56dcb40a8de03",
                rgba_frame_sha256="0" * 64)
            with self.assertRaisesRegex(TOOL.EvidenceError, "assembler-owned"):
                TOOL.validate_metadata(TOOL.parse_metadata(path.resolve()), identity)

    def test_metadata_template_is_instructional_and_hash_bound_to_one_adapter(self):
        template = TOOL.metadata_template("deuteros-amiga-en-title-display-v4")
        self.assertIn("format\tproject-eon-reference-trace-v4\n", template)
        self.assertIn("adapter\tdeuteros-amiga-en-title-display-v4\n", template)
        self.assertIn("game\tdeuteros\nplatform\tamiga\nlanguage\ten\n", template)
        self.assertIn(
            "source_media_sha256\t6ea0cc68d3af37203a885032eddf7c28e839e6abb59d8c9cd3792f1308bdec38\n",
            template)
        self.assertIn("<actual-lowercase-sha256>", template)
        self.assertNotIn("event\t", template)
        with self.assertRaisesRegex(TOOL.EvidenceError, "registered adapter"):
            TOOL.metadata_template("unregistered-adapter")

    def test_metadata_template_cannot_be_used_as_assembly_metadata(self):
        with temporary_directory() as directory:
            path = Path(directory) / "metadata.tsv"
            path.write_bytes(TOOL.metadata_template("millennium-dos-en-startup-v1").encode("utf-8"))
            with self.assertRaises(TOOL.EvidenceError):
                TOOL.validate_metadata(TOOL.parse_metadata(path.resolve()), {
                    "sha256": "e6e7044b25877fdf8b10d16d2f395886d9957953144ae15ca630cda9cab2a123",
                    "size": 328383, "game": "millennium", "platform": "dos", "language": "en"})

    def test_metadata_read_is_bounded_after_secure_open(self):
        with temporary_directory() as directory:
            path = Path(directory) / "metadata.tsv"
            path.write_bytes(b"x" * (TOOL.MAX_METADATA_SIZE + 1))
            with self.assertRaisesRegex(TOOL.EvidenceError, "exceeds"):
                TOOL.parse_metadata(path.resolve())

    def test_assembly_uses_new_directory_and_keeps_source_unchanged(self):
        with temporary_directory() as directory:
            root = Path(directory)
            source = root / "owned-release.zip"
            events = root / "external-events.log"
            metadata = root / "metadata.tsv"
            output = root / "capture"
            configuration, command_tail, input_timeline, hashes = write_provenance_preimages(root)
            original = b"not-game-media; temporary boundary input"
            source.write_bytes(original)
            events.write_bytes(b"external recorder observation\n")
            write_metadata(metadata, **hashes)
            original_identity = TOOL.release_identity
            TOOL.release_identity = lambda digest, size: {
                "sha256": digest, "size": size, "game": "millennium",
                "platform": "dos", "language": "en"}
            try:
                result = TOOL.assemble(type("Arguments", (), {
                    "source_release": str(source.resolve()), "events": str(events.resolve()),
                    "metadata": str(metadata.resolve()), "config": str(configuration.resolve()),
                    "command_tail": str(command_tail.resolve()),
                    "input_timeline": str(input_timeline.resolve()), "output": str(output.resolve())})())
            finally:
                TOOL.release_identity = original_identity
            self.assertEqual(result, output.resolve())
            self.assertEqual(source.read_bytes(), original)
            self.assertEqual((output / "events.eontrace").read_bytes(), events.read_bytes())
            self.assertEqual((output / "configuration.preimage").read_bytes(), configuration.read_bytes())
            self.assertEqual((output / "command-tail.preimage").read_bytes(), command_tail.read_bytes())
            self.assertEqual((output / "input-timeline.preimage").read_bytes(), input_timeline.read_bytes())
            self.assertTrue((output / "manifest.eontrace").read_text(encoding="utf-8").endswith("\n"))
            self.assertIn('"status": "assembled-not-admitted"',
                          (output / "receipt.json").read_text(encoding="utf-8"))
            receipt = json.loads((output / "receipt.json").read_text(encoding="utf-8"))
            self.assertEqual(receipt["provenance"]["configuration"]["sha256"], hashes["config_sha256"])
            self.assertEqual(receipt["provenance"]["command_tail"]["sha256"], hashes["command_tail_sha256"])
            self.assertEqual(receipt["provenance"]["input_timeline"]["sha256"], hashes["input_timeline_sha256"])
            self.assertEqual(receipt["tool"]["sha256"],
                             hashlib.sha256((ROOT / "tools" / "record_reference_trace.py").read_bytes()).hexdigest())

    def test_v5_assembly_copies_all_named_artifacts_and_owns_their_manifest_fields(self):
        # These are deliberately non-game boundary bytes. The test replaces
        # the exact-release registry only inside this process so it can prove
        # assembler ownership without manufacturing or retaining media.
        with temporary_directory() as directory:
            root = Path(directory)
            source = root / "owned-release.zip"
            events = root / "external-events.log"
            metadata = root / "metadata.tsv"
            artifacts = root / "artifacts"
            artifacts.mkdir()
            output = root / "capture"
            configuration, command_tail, input_timeline, hashes = write_provenance_preimages(root)
            source.write_bytes(b"temporary non-game release identity")
            events.write_bytes(b"external recorder observation\n")
            artifact_bytes = {
                "copper-list.bin": b"c" * 88,
                "palette-rgb4.bin": b"p" * 40,
                "bitplanes.bin": b"b" * 32000,
                "palette-rgba8888.bin": b"r" * 80,
                "frame-rgba8888.bin": b"f" * 256000,
                "audio-s16le.bin": b"a" * 2,
            }
            for name, data in artifact_bytes.items():
                (artifacts / name).write_bytes(data)
            source_hash = hashlib.sha256(source.read_bytes()).hexdigest()
            write_metadata(metadata, **hashes,
                format="project-eon-reference-trace-v5",
                adapter="deuteros-amiga-en-title-display-artifacts-v5",
                game="deuteros", platform="amiga",
                source_media_sha256="0" * 64, source_stage_sha256="1" * 64)
            original_adapters = TOOL.V5_ADAPTERS
            original_identity = TOOL.release_identity
            TOOL.V5_ADAPTERS = {
                "deuteros-amiga-en-title-display-artifacts-v5": {
                    "game": "deuteros", "platform": "amiga", "language": "en",
                    "sha256": source_hash, "size": source.stat().st_size,
                    "source_media_sha256": "0" * 64, "source_stage_sha256": "1" * 64,
                }}
            TOOL.release_identity = lambda digest, size: {
                "sha256": digest, "size": size, "game": "deuteros",
                "platform": "amiga", "language": "en"}
            try:
                result = TOOL.assemble(type("Arguments", (), {
                    "source_release": str(source.resolve()), "events": str(events.resolve()),
                    "metadata": str(metadata.resolve()), "config": str(configuration.resolve()),
                    "command_tail": str(command_tail.resolve()), "input_timeline": str(input_timeline.resolve()),
                    "title_display_artifacts": str(artifacts.resolve()), "output": str(output.resolve())})())
            finally:
                TOOL.V5_ADAPTERS = original_adapters
                TOOL.release_identity = original_identity
            self.assertEqual(result, output.resolve())
            manifest = dict(line.split("\t", 1) for line in
                            (output / "manifest.eontrace").read_text(encoding="ascii").splitlines())
            self.assertEqual(manifest["input_timeline_file"], "input-timeline.txt")
            for field, filename, _ in TOOL.V5_ARTIFACTS:
                self.assertEqual(manifest[f"{field}_file"], filename)
                self.assertEqual(int(manifest[f"{field}_size"]), len(artifact_bytes[filename]))
                self.assertEqual(manifest[f"{field}_sha256"],
                                 hashlib.sha256(artifact_bytes[filename]).hexdigest())
                self.assertEqual((output / filename).read_bytes(), artifact_bytes[filename])

    def test_rejects_symlink_and_existing_output(self):
        with temporary_directory() as directory:
            root = Path(directory)
            target = root / "target"
            target.write_bytes(b"x")
            link = root / "link"
            link.symlink_to(target)
            with self.assertRaises(TOOL.EvidenceError):
                TOOL.require_absolute_regular_file(link.resolve().parent / "link", "event stream")
            output = root / "already-there"
            output.mkdir()
            with self.assertRaises(TOOL.EvidenceError):
                TOOL.reject_output_path((target.resolve(),), output.resolve())

    def test_rejects_event_alias_of_original_release(self):
        with temporary_directory() as directory:
            root = Path(directory)
            source = root / "source"
            metadata = root / "metadata"
            configuration, command_tail, input_timeline, hashes = write_provenance_preimages(root)
            source.write_bytes(b"x")
            write_metadata(metadata, **hashes)
            arguments = type("Arguments", (), {
                "source_release": str(source.resolve()), "events": str(source.resolve()),
                "metadata": str(metadata.resolve()), "config": str(configuration.resolve()),
                "command_tail": str(command_tail.resolve()),
                "input_timeline": str(input_timeline.resolve()), "output": str((root / "out").resolve())})()
            with self.assertRaisesRegex(TOOL.EvidenceError, "must not be the original"):
                TOOL.assemble(arguments)

    def test_assembly_rejects_provenance_bytes_that_do_not_match_metadata(self):
        with temporary_directory() as directory:
            root = Path(directory)
            source = root / "owned-release.zip"
            events = root / "external-events.log"
            metadata = root / "metadata.tsv"
            output = root / "capture"
            configuration, command_tail, input_timeline, hashes = write_provenance_preimages(root)
            source.write_bytes(b"not-game-media; temporary boundary input")
            events.write_bytes(b"external recorder observation\n")
            write_metadata(metadata, **hashes)
            configuration.write_bytes(b"[cpu]\ncore=dynamic\n")
            original_identity = TOOL.release_identity
            TOOL.release_identity = lambda digest, size: {
                "sha256": digest, "size": size, "game": "millennium",
                "platform": "dos", "language": "en"}
            try:
                arguments = type("Arguments", (), {
                    "source_release": str(source.resolve()), "events": str(events.resolve()),
                    "metadata": str(metadata.resolve()), "config": str(configuration.resolve()),
                    "command_tail": str(command_tail.resolve()),
                    "input_timeline": str(input_timeline.resolve()), "output": str(output.resolve())})()
                with self.assertRaisesRegex(TOOL.EvidenceError, "configuration SHA-256"):
                    TOOL.assemble(arguments)
            finally:
                TOOL.release_identity = original_identity

    def test_secure_open_requests_binary_mode_when_the_platform_supports_it(self):
        source = (ROOT / "tools" / "record_reference_trace.py").read_text(encoding="utf-8")
        self.assertIn('getattr(os, "O_BINARY", 0)', source)


if __name__ == "__main__":
    unittest.main()
