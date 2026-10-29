#!/usr/bin/env python3
"""Pipe exact supplied Atari disk members to the bounded native save test."""
from __future__ import annotations

import hashlib
from pathlib import Path
import subprocess
import sys
import zipfile


SAVE_DISK = "Millenium 2.2 (1989)(Electric Dreams)(Disk 2 of 2)[a savegames].zip"
EQUINOX_DISK = "Millenium 2.2 (1989)(Electric Dreams)[cr Equinox][one disk].zip"
EXPECTED = {
    SAVE_DISK: "86c3d4de4998c4876ddcf330d8b66d49316de09718c7f34ad326fd881d834e44",
    EQUINOX_DISK: "0056e9fe1bae35ba61660a4b563772e4037e8a6390d1f579ec160044e80a1d69",
}
MEMBERS = {
    SAVE_DISK: ("Millenium 2.2 (1989)(Electric Dreams)(Disk 2 of 2)[a savegames].st",
        "16072aa944d08388f5341d29b164e69d6d16ba516142bbf5b9bf1d4da78330a0"),
    EQUINOX_DISK: ("Millenium 2.2 (1989)(Electric Dreams)[cr Equinox][one disk].st",
        "3f090651ee586cf32a3f37f41b748ba36c78799e7bf761b66ddca2352579afe7"),
}


def read_member(root: Path, archive_name: str) -> bytes:
    path = root / archive_name
    with path.open("rb") as stream:
        archive_bytes = stream.read()
    expected_archive_hash = EXPECTED[archive_name]
    if expected_archive_hash and hashlib.sha256(archive_bytes).hexdigest() != expected_archive_hash:
        raise SystemExit(f"unexpected original archive identity: {archive_name}")
    with zipfile.ZipFile(path) as archive:
        member_name, expected_member_hash = MEMBERS[archive_name]
        if archive.namelist() != [member_name]:
            raise SystemExit(f"unexpected archive member list: {archive_name}")
        member = archive.read(member_name)
    if hashlib.sha256(member).hexdigest() != expected_member_hash:
        raise SystemExit(f"unexpected original disk identity: {member_name}")
    return member


def main() -> int:
    if len(sys.argv) != 3:
        raise SystemExit("usage: millennium_atari_save_load_real_media_test.py TEST_BINARY DATA_DIR")
    test_binary = Path(sys.argv[1])
    data_dir = Path(sys.argv[2])
    media = read_member(data_dir, SAVE_DISK) + read_member(data_dir, EQUINOX_DISK)
    result = subprocess.run([str(test_binary), "--stdin-exact-save-disks"],
        input=media, stdout=subprocess.PIPE, stderr=subprocess.PIPE, check=False)
    if result.returncode != 0:
        sys.stderr.buffer.write(result.stdout)
        sys.stderr.buffer.write(result.stderr)
        return result.returncode
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
