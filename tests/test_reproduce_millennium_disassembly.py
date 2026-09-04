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
 def test_direct_config_recipe_is_present_and_keeps_address_unproven(self):
  root=Path(__file__).resolve().parents[1]
  recipe=(root/'tools/reproduce_millennium_disassembly.py').read_text()
  analyzer=(root/'tools/analyze_atari_st_config.py').read_text()
  self.assertIn('millennium-atari-config.md',recipe)
  self.assertIn('file-image-relative, unrelocated',analyzer)
  self.assertIn('args.nested_member is None',analyzer)
  self.assertIn('millennium-atari-boot.md',recipe)
  self.assertIn('disk-relative',(root/'tools/disassemble_m68k_range.py').read_text())
if __name__=='__main__':unittest.main()
