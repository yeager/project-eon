#!/usr/bin/env python3
"""Render the locally available, hash-recognised Millennium code images externally."""
from __future__ import annotations
import argparse, hashlib, os, subprocess, sys
from pathlib import Path

ROOT=Path(__file__).resolve().parents[1]
DOS=Path("millennium-return-to-earth-2-2")
AMIGA_HASH="ec0424445d494809d2661492e289af71b056a429dde13b053a472ccc8347d4dd"
ATARI_HASH="0056e9fe1bae35ba61660a4b563772e4037e8a6390d1f579ec160044e80a1d69"
def digest(p:Path)->str:
 h=hashlib.sha256()
 with p.open("rb") as f:
  for b in iter(lambda:f.read(1<<20),b""):h.update(b)
 return h.hexdigest()
def output_dir(p:Path)->Path:
 if not p.is_absolute() or str(p).startswith(("/tmp/","/private/tmp/")):raise ValueError("output must be absolute and outside /tmp")
 if p.is_symlink() or not p.is_dir() or any(p.iterdir()):raise ValueError("output must be an existing empty non-symlink directory")
 q=p.resolve();
 if q==ROOT or ROOT in q.parents:raise ValueError("output must be outside the repository")
 return q
def run(a:list[str])->None:
 c=subprocess.run([sys.executable,*a],cwd=ROOT,text=True,capture_output=True)
 if c.returncode:raise ValueError(c.stderr.strip() or c.stdout.strip())
def find_hash(root:Path,wanted:str)->Path:
 matches=[p for p in root.iterdir() if p.is_file() and not p.is_symlink() and digest(p)==wanted]
 if len(matches)!=1:raise ValueError(f"expected exactly one regular medium with SHA-256 {wanted}")
 return matches[0]
def main(argv=None)->int:
 p=argparse.ArgumentParser(description=__doc__);p.add_argument("--media-root",type=Path,required=True);p.add_argument("--output-directory",type=Path,required=True);a=p.parse_args(argv)
 try:
  out=output_dir(a.output_directory); dos=(a.media_root/DOS).resolve(strict=True)
  run(["tools/analyze_dos.py",str(dos),"--directory-set-sha256","d938cd6a611a83897a745b257a371613b73a7dddffb2d336ec2167a192803783","--member","MILL.COM","--member","TITLES.EXE","--member","2200AD.EXE","--member","2200GX.EXE","--member-sha256","MILL.COM=4edc491db60d18ba74cda380c7ce99705b262801298829b63b09932f23f8667e","--member-sha256","TITLES.EXE=3cc57f2b12a0da44dd43220f44f06a05b9e3f009bcf008b7bb87622a5988cbe6","--member-sha256","2200AD.EXE=427574e5f780b2a7b5c4207d167116dc44aea3fb67096fbf12a46c4f544a0a57","--member-sha256","2200GX.EXE=093f8416de6d23837d2faf82360ef79777c2c2bf146619aafad87626c61ab6fb","--complete-linear","--output",str(out/"millennium-dos-en.md")])
  amiga=find_hash(a.media_root,AMIGA_HASH); an=amiga.stem+".adf"
  run(["tools/disassemble_m68k_range.py","--archive",str(amiga),"--archive-sha256",AMIGA_HASH,"--member",an,"--member-sha256","8263e19b431b61c3c34363bb282703476145a45259c94132be82b529ec13b53c","--offset","0x6e000","--length","0x1510","--address","0x41000","--sha256","8eb4e5b1310b5f891697735653bc6365ea7a62b976482978c29dc6d87496b33c","--output",str(out/"millennium-amiga.md")])
  run(["tools/disassemble_m68k_range.py","--archive",str(amiga),"--archive-sha256",AMIGA_HASH,"--member",an,"--member-sha256","8263e19b431b61c3c34363bb282703476145a45259c94132be82b529ec13b53c","--offset","0x400","--length","0x400","--address","0x70000","--sha256","c31e59f83d6825a2da7a6fd5e3297a322993b0483105794fca449d97d3861e06","--output",str(out/"millennium-amiga-bootstrap.md")])
  atari=find_hash(a.media_root,ATARI_HASH); sn=atari.stem+".st"
  run(["tools/disassemble_m68k_range.py","--archive",str(atari),"--archive-sha256",ATARI_HASH,"--member",sn,"--member-sha256","3f090651ee586cf32a3f37f41b748ba36c78799e7bf761b66ddca2352579afe7","--offset","0","--length","512","--address","0","--address-basis","disk-relative","--sha256","fbe9e43b9c4e3e80dbb13c0e4616c949501de69cc7377b0b383cc695d10c0d95","--output",str(out/"millennium-atari-boot.md")])
  run(["tools/analyze_atari_st_prg.py","--archive",str(atari),"--archive-sha256",ATARI_HASH,"--disk-member",sn,"--disk-sha256","3f090651ee586cf32a3f37f41b748ba36c78799e7bf761b66ddca2352579afe7","--program-sha256","4584ddc459e3bf03e642f3156fbedb74aa33a847db4937beb5635eb492e93686","--output",str(out/"millennium-atari-prg.md")])
  run(["tools/analyze_atari_st_config.py","--archive",str(atari),"--archive-sha256",ATARI_HASH,"--disk-member",sn,"--disk-sha256","3f090651ee586cf32a3f37f41b748ba36c78799e7bf761b66ddca2352579afe7","--file-sha256","74d7d630779fd811aedcdbe31b14e54198eb9ffd673df512dd70b6165c4a37b6","--output",str(out/"millennium-atari-config.md")])
 except (OSError,ValueError) as e: print(f"MILLENNIUM DISASSEMBLY REJECTED  {e}",file=sys.stderr);return 2
 print(f"MILLENNIUM DISASSEMBLY RENDERED  {out}");return 0
if __name__=="__main__":raise SystemExit(main())
