import pathlib
import shutil
import subprocess
import tempfile
import unittest


ROOT = pathlib.Path(__file__).resolve().parents[1]
SCRIPT = ROOT / "packaging" / "ios" / "package-ipa.sh"


@unittest.skipUnless(shutil.which("bash") and shutil.which("unzip"), "requires bash and unzip")
class IosPackagingTests(unittest.TestCase):
    def test_creates_payload_with_relative_output(self):
        with tempfile.TemporaryDirectory() as temporary:
            root = pathlib.Path(temporary)
            app = root / "ProjectEon.app"
            app.mkdir()
            (app / "ProjectEon").touch()
            subprocess.run(["bash", str(SCRIPT), str(app), "project-eon.ipa"],
                           cwd=root, check=True)
            ipa = root / "project-eon.ipa"
            self.assertTrue(ipa.is_file())
            listing = subprocess.check_output(["unzip", "-l", str(ipa)], text=True)
            self.assertIn("Payload/ProjectEon.app/ProjectEon", listing)

    def test_rejects_original_media(self):
        with tempfile.TemporaryDirectory() as temporary:
            root = pathlib.Path(temporary)
            app = root / "ProjectEon.app"
            data = app / "data"
            data.mkdir(parents=True)
            (data / "original.adf").touch()
            result = subprocess.run(["bash", str(SCRIPT), str(app), str(root / "bad.ipa")],
                                    capture_output=True, text=True)
            self.assertNotEqual(result.returncode, 0)
            self.assertIn("refusing to package", result.stderr)


if __name__ == "__main__":
    unittest.main()
