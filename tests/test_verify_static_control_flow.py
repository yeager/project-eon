import hashlib
import importlib.util
import json
from pathlib import Path
import unittest

from eon_test_paths import temporary_directory


ROOT = Path(__file__).resolve().parents[1]
SPEC = importlib.util.spec_from_file_location(
    "verify_static_control_flow", ROOT / "tools" / "verify_static_control_flow.py")
assert SPEC and SPEC.loader
TOOL = importlib.util.module_from_spec(SPEC)
SPEC.loader.exec_module(TOOL)


class VerifyStaticControlFlowTests(unittest.TestCase):
    def write_inventory(self, root: Path, content: bytes) -> Path:
        inventory = {
            "schema": "project-eon.disassembly-inventory/v2",
            "releases": [{"static_spans": [{"id": "first"}, {"id": "second"}]}],
            "control_flow_sidecars": [{"span_ids": ["first", "second"],
                                        "sha256": hashlib.sha256(content).hexdigest(),
                                        "lines": content.count(b"\n"),
                                        "classification": "static-candidate-unclassified"}],
        }
        path = root / "inventory.json"
        path.write_text(json.dumps(inventory), encoding="utf-8")
        return path

    def test_reused_sidecar_requires_hash_line_and_schema(self):
        with temporary_directory() as temporary:
            root = Path(temporary)
            content = b'{"schema":"project-eon.static-control-flow-set/v1"}\n'
            sidecar = root / "sidecar.json"
            sidecar.write_bytes(content)
            expected = TOOL.expected_sidecars(self.write_inventory(root, content))
            self.assertEqual(TOOL.verify_sidecars(expected, {"first": sidecar, "second": sidecar}), 1)
            sidecar.write_text('{"schema":"wrong"}\n', encoding="utf-8")
            with self.assertRaisesRegex(TOOL.ReportError, "hash/line"):
                TOOL.verify_sidecars(expected, {"first": sidecar, "second": sidecar})

    def test_unknown_or_duplicate_span_references_are_rejected(self):
        with temporary_directory() as temporary:
            root = Path(temporary)
            content = b'{}\n'
            inventory = self.write_inventory(root, content)
            payload = json.loads(inventory.read_text(encoding="utf-8"))
            payload["control_flow_sidecars"][0]["span_ids"] = ["missing"]
            inventory.write_text(json.dumps(payload), encoding="utf-8")
            with self.assertRaisesRegex(TOOL.ReportError, "span reference"):
                TOOL.expected_sidecars(inventory)


if __name__ == "__main__":
    unittest.main()
