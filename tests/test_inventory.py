import io
from pathlib import Path
import tempfile
import unittest
import zipfile

from eon.inventory import classify, inventory


class InventoryTests(unittest.TestCase):
    def test_signature_classification(self):
        self.assertEqual(classify("GAME.EXE", b"MZ" + bytes(20)), "dos-mz-executable")
        self.assertEqual(classify("disk.adf", bytes(901_120)), "amiga-adf")
        self.assertEqual(classify("disk.st", bytes(10)), "atari-st-disk")

    def test_nested_archive(self):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            nested = io.BytesIO()
            with zipfile.ZipFile(nested, "w") as inner:
                inner.writestr("DISK.ADF", bytes(901_120))
            with zipfile.ZipFile(root / "game.zip", "w") as outer:
                outer.writestr("disk.zip", nested.getvalue())
            result = inventory(root)
            self.assertEqual(result["counts"], {"amiga-adf": 1})


if __name__ == "__main__":
    unittest.main()

