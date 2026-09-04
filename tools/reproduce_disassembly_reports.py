#!/usr/bin/env python3
"""Rebuild Project Eon's complete external disassembly-report identity set.

The reports mechanically render original executable bytes and must remain
outside the checkout.  This driver takes explicit, user-supplied original
containers, passes their pinned identities to the existing bounded analyzers,
and finally verifies every report against the committed inventory.  It never
extracts, mounts, copies, or modifies game data.
"""

from __future__ import annotations

import argparse
import os
from pathlib import Path
import subprocess
import sys


ROOT = Path(__file__).resolve().parents[1]


class ReproductionError(ValueError):
    """One external report destination or analyzer invocation was rejected."""


def require_output_directory(path: Path) -> Path:
    """Admit one existing, empty external directory for newly rendered reports."""
    if not path.is_absolute():
        raise ReproductionError("output directory must be absolute")
    lexical = os.path.normpath(str(path))
    if lexical == "/tmp" or lexical.startswith("/tmp/") or lexical == "/private/tmp" or lexical.startswith("/private/tmp/"):
        raise ReproductionError("output directory must be outside /tmp")
    try:
        status = path.lstat()
    except OSError as error:
        raise ReproductionError(f"unable to stat output directory: {error}") from error
    if path.is_symlink() or not path.is_dir():
        raise ReproductionError("output directory must be an existing non-symlink directory")
    resolved = path.resolve(strict=True)
    if resolved == ROOT or ROOT in resolved.parents:
        raise ReproductionError("output directory must be outside the repository")
    if any(resolved.iterdir()):
        raise ReproductionError("output directory must be empty so every report is freshly rendered")
    return resolved


def run(arguments: list[str]) -> None:
    completed = subprocess.run((sys.executable, *arguments), cwd=ROOT, check=False,
                               text=True, capture_output=True)
    if completed.returncode == 0:
        return
    detail = completed.stderr.strip() or completed.stdout.strip() or "no diagnostic output"
    raise ReproductionError(f"analyzer rejected its hash-bound source: {detail}")


def main(argv: list[str] | None = None) -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--output-directory", type=Path, required=True)
    parser.add_argument("--millennium-dos-directory", type=Path, required=True,
                        help="Exact installed Millennium DOS directory")
    parser.add_argument("--millennium-dos-spanish-archive", type=Path, required=True)
    parser.add_argument("--millennium-amiga-archive", type=Path, required=True,
                        help="Combined Millennium Amiga carrier ZIP")
    parser.add_argument("--millennium-atari-archive", type=Path, required=True,
                        help="Combined Millennium Atari ST carrier ZIP")
    parser.add_argument("--deuteros-amiga-disk-archive", type=Path, required=True,
                        help="Exact direct clean Deuteros Amiga Disk 1 ZIP")
    parser.add_argument("--deuteros-atari-archive", type=Path, required=True)
    args = parser.parse_args(argv)
    try:
        output = require_output_directory(args.output_directory)
        output_for = lambda name: str(output / name)
        run(["tools/analyze_dos.py", str(args.millennium_dos_directory),
             "--directory-set-sha256", "d938cd6a611a83897a745b257a371613b73a7dddffb2d336ec2167a192803783",
             "--member", "MILL.COM", "--member", "TITLES.EXE", "--member", "2200AD.EXE",
             "--member", "2200GX.EXE",
             "--member-sha256", "MILL.COM=4edc491db60d18ba74cda380c7ce99705b262801298829b63b09932f23f8667e",
             "--member-sha256", "TITLES.EXE=3cc57f2b12a0da44dd43220f44f06a05b9e3f009bcf008b7bb87622a5988cbe6",
             "--member-sha256", "2200AD.EXE=427574e5f780b2a7b5c4207d167116dc44aea3fb67096fbf12a46c4f544a0a57",
             "--member-sha256", "2200GX.EXE=093f8416de6d23837d2faf82360ef79777c2c2bf146619aafad87626c61ab6fb",
             "--complete-linear", "--output", output_for("millennium-dos-en.md")])
        run(["tools/analyze_dos.py", "--fat12-archive", str(args.millennium_dos_spanish_archive),
             "--fat12-archive-sha256", "b40cc2f2c39cdb476b4a82bda7bffed1c80decdfb7fe41b1a38bf54343e0c0a4",
             "--fat12-member", "MRTE.IMG", "--fat12-member-sha256",
             "1cb7d399ab22110317b1c7486a575c00895f12a17268d0c984ac264a5695961d",
             "--member", "IBM.COM", "--member", "TITLES.EXE", "--member", "2200AD.EXE",
             "--member-sha256", "IBM.COM=84b7d158c770117aeaa07cb5ea2e7ed4a6bcc288d6b352d82569ff4d97b2fda9",
             "--member-sha256", "TITLES.EXE=02082c35e18cee330f7d1b88098f502e68011f7e47a3a649961f6f03d1d14fe7",
             "--member-sha256", "2200AD.EXE=9f7d6f28f71eb7f2f6bb48cb3977efbf45049fc74083f8cbc865ec25396330c6",
             "--complete-linear", "--output", output_for("millennium-dos-es.md")])
        run(["tools/disassemble_m68k_range.py", "--archive", str(args.millennium_amiga_archive),
             "--archive-sha256", "2e27d7aeb8b8b7f2a75eda45b456ab42775a706aa85516c85e61ce94ec9eb400",
             "--nested-member", "Millennium 2.2 (1989)(Software Studios)[cr Defjam - CCS - Spreadpoint].zip",
             "--nested-sha256", "ec0424445d494809d2661492e289af71b056a429dde13b053a472ccc8347d4dd",
             "--member", "Millennium 2.2 (1989)(Software Studios)[cr Defjam - CCS - Spreadpoint].adf",
             "--member-sha256", "8263e19b431b61c3c34363bb282703476145a45259c94132be82b529ec13b53c",
             "--offset", "0x16400", "--length", "0x2c000", "--address", "0x68000",
             "--sha256", "d144abc05f891710dc99b30d87f020bd6e2ff7796ef86a847f07b8d97d55d18e",
             "--output", output_for("millennium-amiga.md")])
        run(["tools/disassemble_m68k_range.py", "--archive", str(args.millennium_amiga_archive),
             "--archive-sha256", "2e27d7aeb8b8b7f2a75eda45b456ab42775a706aa85516c85e61ce94ec9eb400",
             "--nested-member", "Millennium 2.2 (1989)(Software Studios)[cr Defjam - CCS - Spreadpoint].zip",
             "--nested-sha256", "ec0424445d494809d2661492e289af71b056a429dde13b053a472ccc8347d4dd",
             "--member", "Millennium 2.2 (1989)(Software Studios)[cr Defjam - CCS - Spreadpoint].adf",
             "--member-sha256", "8263e19b431b61c3c34363bb282703476145a45259c94132be82b529ec13b53c",
             "--offset", "0x400", "--length", "0x400", "--address", "0x70000",
             "--sha256", "c31e59f83d6825a2da7a6fd5e3297a322993b0483105794fca449d97d3861e06",
             "--output", output_for("millennium-amiga-bootstrap.md")])
        common_atari = ["--archive", str(args.millennium_atari_archive), "--archive-sha256",
                        "ba1174123a0531abeab5788f4ac87a3c2500696bf1c87a7efd209441b3ebdf01",
                        "--nested-member", "Millenium 2.2 (1989)(Electric Dreams)[cr Equinox][one disk].zip",
                        "--nested-sha256", "0056e9fe1bae35ba61660a4b563772e4037e8a6390d1f579ec160044e80a1d69",
                        "--disk-member", "Millenium 2.2 (1989)(Electric Dreams)[cr Equinox][one disk].st",
                        "--disk-sha256", "3f090651ee586cf32a3f37f41b748ba36c78799e7bf761b66ddca2352579afe7"]
        run(["tools/analyze_atari_st_prg.py", *common_atari,
             "--program-sha256", "4584ddc459e3bf03e642f3156fbedb74aa33a847db4937beb5635eb492e93686",
             "--output", output_for("millennium-atari-prg.md")])
        run(["tools/analyze_atari_st_config.py", *common_atari,
             "--file-sha256", "74d7d630779fd811aedcdbe31b14e54198eb9ffd673df512dd70b6165c4a37b6",
             "--output", output_for("millennium-atari-config.md")])
        run(["tools/analyze_m68k.py", "--archive", str(args.deuteros_amiga_disk_archive),
             "--archive-sha256", "7ecaa0457ad2b61b417bbe62943a4a11b4d164acfbc5a5097e95f8f7d1360533",
             "--member", "Deuteros - The Next Millennium (1991)(Activision)(M3)(Disk 1 of 2).adf",
             "--member-sha256", "6ea0cc68d3af37203a885032eddf7c28e839e6abb59d8c9cd3792f1308bdec38",
             "--complete-linear", "--output", output_for("deuteros-amiga.md")])
        run(["tools/disassemble_m68k_range.py", "--archive", str(args.deuteros_atari_archive),
             "--archive-sha256", "c6856d0a7ccda925289c60f0675e7aaed616f8a0289c74698e87e1ee11e6c653",
             "--nested-member", "Deuteros (1991)(Activision)(M3)(Disk 1 of 2)[cr Replicants].zip",
             "--nested-sha256", "a9318feb83ff34b79f5a5ea1e5ffcb45828e4432ac75a859f55c3de97d724c93",
             "--member", "Deuteros (1991)(Activision)(M3)(Disk 1 of 2)[cr Replicants].st",
             "--member-sha256", "aba874134807360ccde0ff98d6b82a965f57dcae5800b5b54394472522ef5bee",
             "--offset", "0x4ec00", "--length", "0x1200", "--address", "0x1200",
             "--sha256", "d20784600c5fe3c8fb2005ec5d162d68ffa8f5a0f65d29fcd8a1d9ede2bafddc",
             "--output", output_for("deuteros-atari.md")])
        run(["tools/verify_disassembly_reports.py", "--report-directory", str(output)])
    except ReproductionError as error:
        print(f"DISASSEMBLY REPRODUCTION REJECTED  {error}", file=sys.stderr)
        return 2
    print(f"DISASSEMBLY REPRODUCTION VERIFIED  {output}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
