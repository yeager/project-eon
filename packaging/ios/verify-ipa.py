#!/usr/bin/env python3
"""Verify an unsigned Project Eon iPadOS IPA after it has been archived.

The packager validates the source application before creating the archive.  This
independent check deliberately reads the resulting IPA instead: it makes the
uploaded artifact, rather than its build staging directory, the preservation
boundary.  It never extracts or executes the application.
"""

from __future__ import annotations

import argparse
import plistlib
import stat
import sys
import zipfile
from pathlib import PurePosixPath


CATALOGS = (
    "ar", "de", "el", "en_GB", "es", "fi", "fr", "hi", "it", "ja", "ko",
    "nl", "no", "pl", "pt_BR", "ru", "sv", "tr", "uk", "zh_CN",
)
FONTS = (
    "NotoSans-Regular.ttf", "NotoSansArabic-Regular.ttf",
    "NotoSansDevanagari-Regular.ttf", "NotoSansJP-Regular.otf",
    "NotoSansKR-Regular.otf", "NotoSansSC-Regular.otf", "OFL-1.1.txt",
)
ROOT = "Payload/ProjectEon.app/"
MEDIA_SUFFIXES = (".zip", ".adf", ".st", ".msa", ".stx", ".img", ".exe", ".com")


def fail(message: str) -> None:
    raise ValueError(message)


def is_symlink(info: zipfile.ZipInfo) -> bool:
    return stat.S_IFMT(info.external_attr >> 16) == stat.S_IFLNK


def validate_member(info: zipfile.ZipInfo) -> None:
    name = info.filename
    if not name or "\\" in name or name.startswith("/"):
        fail(f"unsafe IPA member path: {name!r}")
    # PurePosixPath intentionally normalizes doubled separators; inspect the
    # archive spelling first so a later extraction tool cannot choose a
    # different traversal interpretation.
    raw_parts = name[:-1].split("/") if name.endswith("/") else name.split("/")
    if not raw_parts or any(part in ("", ".", "..") for part in raw_parts):
        fail(f"unsafe IPA member path: {name!r}")
    path = PurePosixPath(name)
    if any(part in ("", ".", "..") for part in path.parts):
        fail(f"unsafe IPA member path: {name!r}")
    if is_symlink(info):
        fail(f"IPA must not contain a symbolic link: {name}")
    if info.flag_bits & 0x1:
        fail(f"IPA must not contain encrypted members: {name}")
    if name in ("Payload/", ROOT):
        return
    if not name.startswith(ROOT):
        fail(f"IPA member is outside the Project Eon application bundle: {name}")
    lower_parts = tuple(part.casefold() for part in path.parts)
    if "data" in lower_parts or name.casefold().endswith(MEDIA_SUFFIXES):
        fail(f"IPA contains possible original game media: {name}")


def required_paths() -> set[str]:
    required = {
        ROOT + "Info.plist",
        ROOT + "project-eon",
        ROOT + "Resources/assets/cards/millennium.png",
        ROOT + "Resources/assets/cards/deuteros.png",
    }
    required.update(ROOT + "Resources/assets/fonts/" + font for font in FONTS)
    required.update(ROOT + "Resources/po/" + catalog + ".po" for catalog in CATALOGS)
    return required


def validate_plist(raw: bytes) -> None:
    try:
        plist = plistlib.loads(raw)
    except (plistlib.InvalidFileException, ValueError) as error:
        fail(f"IPA Info.plist is invalid: {error}")
    expected = {
        "CFBundleExecutable": "project-eon",
        "CFBundleIdentifier": "com.yeager.projecteon",
        "CFBundlePackageType": "APPL",
    }
    for key, value in expected.items():
        if plist.get(key) != value:
            fail(f"IPA Info.plist has unexpected {key}")
    if plist.get("LSRequiresIPhoneOS") is not True:
        fail("IPA Info.plist does not require iPhoneOS")
    if 2 not in plist.get("UIDeviceFamily", []):
        fail("IPA Info.plist does not declare iPad support")
    capabilities = plist.get("UIRequiredDeviceCapabilities", [])
    if "arm64" not in capabilities:
        fail("IPA Info.plist does not require arm64")


def validate_arm64_macho(raw: bytes) -> None:
    # MH_MAGIC_64 and CPU_TYPE_ARM64, both little endian.  iPadOS CI builds
    # one arm64 binary; accepting a host executable here would make a sideload
    # artifact appear usable when it cannot launch on an iPad.
    if len(raw) < 8 or raw[:4] != b"\xcf\xfa\xed\xfe" or raw[4:8] != b"\x0c\x00\x00\x01":
        fail("IPA executable is not a 64-bit arm64 Mach-O binary")


def verify(ipa: str) -> None:
    try:
        with zipfile.ZipFile(ipa) as archive:
            infos = archive.infolist()
            if not infos:
                fail("IPA is empty")
            names: set[str] = set()
            for info in infos:
                validate_member(info)
                if info.filename in names:
                    fail(f"IPA contains duplicate member: {info.filename}")
                names.add(info.filename)
            missing = sorted(required_paths() - names)
            if missing:
                fail("IPA is missing required member(s): " + ", ".join(missing))
            validate_plist(archive.read(ROOT + "Info.plist"))
            validate_arm64_macho(archive.read(ROOT + "project-eon"))
            bad_payloads = [
                name for name in names
                if name.startswith("Payload/") and name not in ("Payload/", ROOT)
                and not name.startswith(ROOT)
            ]
            if bad_payloads:
                fail("IPA contains an unexpected Payload member: " + bad_payloads[0])
    except (OSError, zipfile.BadZipFile) as error:
        fail(f"cannot read IPA: {error}")


def main() -> int:
    parser = argparse.ArgumentParser(description="verify a Project Eon unsigned iPadOS IPA")
    parser.add_argument("ipa", help="IPA artifact to verify without extracting it")
    arguments = parser.parse_args()
    try:
        verify(arguments.ipa)
    except ValueError as error:
        print(f"IPA verification failed: {error}", file=sys.stderr)
        return 1
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
