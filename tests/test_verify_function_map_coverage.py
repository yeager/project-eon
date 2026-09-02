import importlib.util
import json
from pathlib import Path
import unittest

from eon_test_paths import temporary_directory


ROOT = Path(__file__).resolve().parents[1]
SPEC = importlib.util.spec_from_file_location(
    "verify_function_map_coverage", ROOT / "tools" / "verify_function_map_coverage.py")
assert SPEC and SPEC.loader
TOOL = importlib.util.module_from_spec(SPEC)
SPEC.loader.exec_module(TOOL)


class FunctionMapCoverageTests(unittest.TestCase):
    def write_function_map(self, root: Path, *, address: str = "$1010") -> Path:
        path = root / "function-map.json"
        path.write_text(json.dumps({"schema": "project-eon.function-map/v1", "entries": [{
            "id": "entry", "release_sha256": "a" * 64, "source_asset_sha256": "b" * 64,
            "cpu": "i8086", "runtime_address": address,
        }]}), encoding="utf-8")
        return path

    def write_sidecar(self, root: Path, *, start: int = 0x1000, length: int = 0x40) -> Path:
        path = root / "sidecar.json"
        path.write_text(json.dumps({"schema": "project-eon.static-control-flow-set/v1",
            "classification": "static-candidate-unclassified", "documents": [{
                "schema": "project-eon.static-control-flow/v1", "classification": "static-candidate-unclassified",
                "archive_sha256": "a" * 64, "source_sha256": "c" * 64, "source": "fixture",
                "source_kind": "archive-member", "cpu": "i8086", "ranges": [{
                    "runtime_address": start, "length": length, "sha256": "b" * 64, "source_offset": 0,
                    "edges": []}]}]}), encoding="utf-8")
        return path

    def test_exact_release_cpu_space_hash_and_address_bind(self):
        with temporary_directory() as temporary:
            root = Path(temporary)
            entries = TOOL.load_function_map(self.write_function_map(root))
            declared = TOOL.load_declared_ranges([self.write_sidecar(root)])
            self.assertEqual(TOOL.coverage(entries, declared), (["entry"], []))

    def test_outside_range_is_not_a_binding_and_complete_mode_rejects(self):
        with temporary_directory() as temporary:
            root = Path(temporary)
            entries = TOOL.load_function_map(self.write_function_map(root, address="$1040"))
            declared = TOOL.load_declared_ranges([self.write_sidecar(root)])
            self.assertEqual(TOOL.coverage(entries, declared), ([], ["entry"]))
            self.assertEqual(TOOL.main(["--function-map", str(root / "function-map.json"),
                                        "--sidecar", str(root / "sidecar.json"), "--require-complete"]), 2)

    def test_image_relative_rows_do_not_bind_runtime_ranges(self):
        with temporary_directory() as temporary:
            root = Path(temporary)
            function_map = self.write_function_map(root, address="+0x1010")
            payload = json.loads(function_map.read_text(encoding="utf-8"))
            payload["entries"][0]["address_space"] = "image-relative-unrelocated"
            function_map.write_text(json.dumps(payload), encoding="utf-8")
            entries = TOOL.load_function_map(function_map)
            declared = TOOL.load_declared_ranges([self.write_sidecar(root)])
            self.assertEqual(TOOL.coverage(entries, declared), ([], ["entry"]))

    def test_span_identity_can_bind_a_function_inside_a_broader_source_asset(self):
        with temporary_directory() as temporary:
            root = Path(temporary)
            function_map = self.write_function_map(root)
            payload = json.loads(function_map.read_text(encoding="utf-8"))
            payload["entries"][0]["source_span_sha256"] = "b" * 64
            payload["entries"][0]["source_asset_sha256"] = "d" * 64
            function_map.write_text(json.dumps(payload), encoding="utf-8")
            entries = TOOL.load_function_map(function_map)
            declared = TOOL.load_declared_ranges([self.write_sidecar(root)])
            self.assertEqual(TOOL.coverage(entries, declared), (["entry"], []))


if __name__ == "__main__":
    unittest.main()
