from pathlib import Path
import tempfile,unittest
from tools.reproduce_millennium_disassembly import output_dir
class Tests(unittest.TestCase):
 def test_external_empty_output_only(self):
  cache=Path('/home/yeager/.cache/project-eon-tools/tests');cache.mkdir(parents=True,exist_ok=True)
  with tempfile.TemporaryDirectory(dir=cache) as d:
   p=Path(d);self.assertEqual(output_dir(p),p.resolve());(p/'x').write_text('x')
   with self.assertRaises(ValueError):output_dir(p)
 def test_repository_and_tmp_rejected(self):
  with self.assertRaises(ValueError):output_dir(Path('/tmp'))
  with self.assertRaises(ValueError):output_dir(Path(__file__).resolve().parents[1])
if __name__=='__main__':unittest.main()
