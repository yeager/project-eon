import json
from pathlib import Path
import re
import unittest

ROOT = Path(__file__).resolve().parents[1]

class NativeCodeImageRegistryCoverageTests(unittest.TestCase):
    def test_every_manifest_image_is_registered_or_explicitly_excluded(self) -> None:
        manifest=json.loads((ROOT/"docs/complete-disassembly-manifest.json").read_text())
        source=(ROOT/"src/data/native_code_image_admission.cpp").read_text()
        exclusions=json.loads((ROOT/"docs/native-code-image-exclusions.json").read_text())
        self.assertEqual(exclusions["schema"],"project-eon.native-code-image-exclusions/v1")
        manifest_ids={image["span_id"] for release in manifest["releases"] for image in release["images"]}
        descriptor_ids=set(re.findall(r'NativeCodeImageDescriptor\{"[0-9a-f]{64}","([^"]+)"',source))
        rows=exclusions["images"];excluded_ids={row["image_id"] for row in rows}
        self.assertEqual(len(excluded_ids),len(rows))
        self.assertTrue(all(row["reason"]=="container-only-candidate" for row in rows))
        self.assertFalse(descriptor_ids&excluded_ids)
        self.assertEqual(descriptor_ids|excluded_ids,manifest_ids)
        diagnostics=(ROOT/"src/engine/native_code_image_diagnostics.cpp").read_text()
        declared_count=re.search(r"constexpr std::size_t excluded_image_count = (\d+);",diagnostics)
        self.assertIsNotNone(declared_count)
        self.assertEqual(int(declared_count.group(1)),len(rows))

if __name__=="__main__": unittest.main()
