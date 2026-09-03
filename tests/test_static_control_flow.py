import hashlib
import json
from pathlib import Path
import unittest
from zipfile import ZipFile

from eon_test_paths import temporary_directory
from tools.extract_static_control_flow import (
    CLASSIFICATION, ControlFlowError, _require_external_output, build_sidecar,
    _read_verified_dos_directory, m68k_direct_edges, x86_direct_edges,
)


class StaticControlFlowTests(unittest.TestCase):
    def test_x86_records_only_direct_candidates(self):
        # call +2; jmp +2; short jmp +2; jz +2; int 21h; ret; call ax.
        edges = x86_direct_edges(bytes.fromhex("e80200e90200eb027402cd21c3ffd0"),
                                 source_offset=3, runtime_address=0x100)
        self.assertEqual([(edge["source_offset"], edge["runtime_address"], edge["kind"])
                          for edge in edges],
                         [(3, 0x100, "call"), (6, 0x103, "jump"), (9, 0x106, "jump"),
                          (11, 0x108, "conditional-jump"), (13, 0x10a, "interrupt"),
                          (15, 0x10c, "return")])
        self.assertEqual([edge.get("target_runtime_address") for edge in edges[:4]],
                         [0x105, 0x108, 0x10a, 0x10c])
        self.assertEqual(edges[4]["interrupt_vector"], 0x21)
        self.assertEqual(edges[5]["target"], "return-address-unproven")
        self.assertTrue(all(edge["classification"] == CLASSIFICATION for edge in edges))
        self.assertEqual(x86_direct_edges(b"\x0f", source_offset=0, runtime_address=0x100), [])

    def test_m68k_records_direct_absolute_and_displacement_candidates(self):
        # jsr absolute; jmp absolute; bsr/bra/bne +2; rts; jmp (a0); jsr +2(pc).
        edges = m68k_direct_edges(bytes.fromhex(
            "4eb9000012344ef9000012346102600266024e754ed04eba0002"),
            source_offset=0, runtime_address=0x1200)
        self.assertEqual([(edge["runtime_address"], edge["kind"], edge.get("target_runtime_address"))
                          for edge in edges],
                         [(0x1200, "call", 0x1234), (0x1206, "jump", 0x1234),
                          (0x120c, "call", 0x1210), (0x120e, "jump", 0x1212),
                          (0x1210, "conditional-jump", 0x1214), (0x1212, "return", None)])
        self.assertNotIn(0x1214, [edge["runtime_address"] for edge in edges])
        # Current Capstone's M68K binding loses PC-relative JSR displacement;
        # it must not become an invented direct target.
        self.assertNotIn(0x1216, [edge["runtime_address"] for edge in edges])
        self.assertEqual(edges[5]["target"], "return-address-unproven")

    def test_sidecar_marks_target_membership_without_upgrading_candidates(self):
        source = bytes.fromhex("e80200")
        document = build_sidecar("i8086", "0" * 64, "fixture", source,
                                 [(0, len(source), 0x100, hashlib.sha256(source).hexdigest())])
        edge = document["ranges"][0]["edges"][0]
        self.assertEqual(edge["target_scope"], "outside-declared-range")
        self.assertEqual(document["classification"], CLASSIFICATION)
        self.assertEqual(json.dumps(document, sort_keys=True), json.dumps(
            build_sidecar("i8086", "0" * 64, "fixture", source,
                          [(0, len(source), 0x100, hashlib.sha256(source).hexdigest())]), sort_keys=True))

    def test_image_relative_sidecar_does_not_claim_a_runtime_base(self):
        source = bytes.fromhex("6002")
        document = build_sidecar("m68000", "0" * 64, "fixture.prg", source,
                                 [(0, len(source), 0, hashlib.sha256(source).hexdigest())],
                                 address_space="image-relative-unrelocated")
        record = document["ranges"][0]
        edge = record["edges"][0]
        self.assertEqual(document["address_space"], "image-relative-unrelocated")
        self.assertIn("image_relative_address", record)
        self.assertNotIn("runtime_address", record)
        self.assertIn("target_image_relative_address", edge)
        self.assertNotIn("runtime_address", edge)

    def test_sidecar_retains_separate_carrier_provenance(self):
        source = b"\x90"
        document = build_sidecar("i8086", "1" * 64, "embedded!MILL.COM", source,
                                 [(0, 1, 0x100, hashlib.sha256(source).hexdigest())],
                                 source_kind="embedded-release-nested-disk-range",
                                 container_sha256="2" * 64, carrier_archive_sha256="3" * 64)
        self.assertEqual(document["archive_sha256"], "1" * 64)
        self.assertEqual(document["container_sha256"], "2" * 64)
        self.assertEqual(document["carrier_archive_sha256"], "3" * 64)

    def test_direct_media_sidecar_retains_its_complete_set_identity(self):
        source = b"\x90"
        document = build_sidecar("i8086", "1" * 64, "direct-media-set:fixture:MILL.COM", source,
                                 [(0, 1, 0x100, hashlib.sha256(source).hexdigest())],
                                 source_kind="verified-direct-media-member",
                                 direct_media_set_sha256="2" * 64)
        self.assertEqual(document["direct_media_set_sha256"], "2" * 64)
        with self.assertRaisesRegex(ControlFlowError, "direct-media set SHA-256"):
            build_sidecar("i8086", "1" * 64, "fixture", source,
                          [(0, 1, 0x100, hashlib.sha256(source).hexdigest())],
                          source_kind="verified-direct-media-member",
                          direct_media_set_sha256="bad")

    def test_destination_refuses_repository_tmp_and_existing_paths(self):
        with self.assertRaisesRegex(ControlFlowError, "outside /tmp"):
            _require_external_output(Path("/tmp/project-eon-static-flow.json"))
        with self.assertRaisesRegex(ControlFlowError, "outside the repository"):
            _require_external_output(Path.cwd() / "static-flow.json")
        with temporary_directory() as temporary:
            existing = Path(temporary) / "existing.json"
            existing.write_text("existing", encoding="utf-8")
            with self.assertRaisesRegex(ControlFlowError, "must not already exist"):
                _require_external_output(existing)

    def test_cli_hash_locks_exact_member_and_does_not_overwrite_output(self):
        with temporary_directory() as temporary:
            root = Path(temporary)
            archive = root / "source.zip"
            payload = b"\x90\xcd\x21"
            with ZipFile(archive, "w") as stream:
                stream.writestr("MILL.COM", payload)
            output = root / "sidecar.json"
            from tools.extract_static_control_flow import main
            self.assertEqual(main(["--dos-archive", str(archive), "--archive-sha256",
                                   hashlib.sha256(archive.read_bytes()).hexdigest(), "--member", "MILL.COM",
                                   "--output", str(output)]), 0)
            parsed = json.loads(output.read_text(encoding="utf-8"))
            self.assertEqual(parsed["documents"][0]["source_sha256"], hashlib.sha256(payload).hexdigest())
            self.assertEqual(parsed["documents"][0]["source_kind"], "archive-member")
            self.assertEqual(main(["--dos-archive", str(archive), "--archive-sha256", "0" * 64,
                                   "--member", "MILL.COM", "--output", str(root / "wrong.json")]), 2)
            self.assertEqual(main(["--dos-archive", str(archive), "--archive-sha256",
                                   hashlib.sha256(archive.read_bytes()).hexdigest(), "--member", "MILL.COM",
                                   "--output", str(output)]), 2)

    def test_direct_dos_directory_requires_a_complete_hash_bound_set(self):
        with temporary_directory() as temporary:
            root = Path(temporary)
            media = root / "media"
            media.mkdir()
            payload = b"\x90\xcd\x21"
            digest = hashlib.sha256(payload).hexdigest()
            release = "a" * 64
            serialization = f"MILL.COM\t{len(payload)}\t{digest}\n".encode("ascii")
            manifest = root / "release-manifest.json"
            manifest.write_text(json.dumps({
                "schema": "project-eon.release-manifest/v1",
                "releases": [{"sha256": release, "platform": "dos"}],
                "direct_media_sets": [{
                    "content_release_sha256": release,
                    "platform": "dos",
                    "set_sha256": hashlib.sha256(serialization).hexdigest(),
                    "members": [{"name": "MILL.COM", "size": len(payload), "sha256": digest}],
                }],
            }), encoding="utf-8")
            (media / "MILL.COM").write_bytes(payload)
            documents = _read_verified_dos_directory(media, release, ["MILL.COM"], manifest_path=manifest)
            self.assertEqual(documents, [("MILL.COM", payload, hashlib.sha256(serialization).hexdigest())])
            (media / "MILL.COM").write_bytes(b"changed")
            with self.assertRaisesRegex(ControlFlowError, "declared regular file"):
                _read_verified_dos_directory(media, release, ["MILL.COM"], manifest_path=manifest)


if __name__ == "__main__":
    unittest.main()
