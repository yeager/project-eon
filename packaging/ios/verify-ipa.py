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
MEDIA_SUFFIXES = (
    ".zip", ".adf", ".adz", ".dms", ".st", ".msa", ".stx", ".img", ".hfe", ".ipf",
    ".scp", ".ctr", ".lha", ".lzh", ".lzx", ".exe", ".com",
)


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
    # The iPadOS CMake configuration builds the SDL stack and all of its
    # dependencies statically. A Frameworks directory would therefore be an
    # unreviewed dynamic runtime closure, not a requirement of this sideload
    # artifact.
    if "frameworks" in lower_parts:
        fail(f"IPA contains an unexpected dynamic framework: {name}")


def required_paths() -> set[str]:
    required = {
        ROOT + "Info.plist",
        ROOT + "project-eon",
        ROOT + "Resources/assets/cards/millennium.png",
        ROOT + "Resources/assets/cards/deuteros.png",
        ROOT + "Resources/assets/cards/dos-platform-v1.png",
        ROOT + "Resources/assets/cards/amiga-platform-v1.png",
        ROOT + "Resources/assets/cards/atari-st-platform-v1.png",
        ROOT + "Resources/assets/cards/original-profile-v1.png",
        ROOT + "Resources/assets/cards/modern-profile-v1.png",
        ROOT + "Resources/assets/cards/custom-profile-v1.png",
        ROOT + "Resources/assets/branding/project-eon-logo-v1.png",
    }
    required.update(ROOT + "Resources/assets/fonts/" + font for font in FONTS)
    required.update(ROOT + "Resources/po/" + catalog + ".po" for catalog in CATALOGS)
    return required


def allowed_directories() -> set[str]:
    """Return the complete directory closure of the reviewed IPA payload.

    Blocking familiar disk-image suffixes is useful defence in depth, but it
    cannot prove that an arbitrary additional payload is not original media.
    Project Eon's unsigned iPad artifact has a deliberately finite resource
    contract, so make that contract executable: only its executable, plist,
    cards, branding, fonts, and translated UI catalogues may be regular
    members.  Directory entries are allowed only when they are parents of one
    of those reviewed files.
    """
    directories = {"Payload/", ROOT}
    for member in required_paths():
        parent = PurePosixPath(member).parent
        while str(parent) not in (".", ""):
            directories.add(str(parent) + "/")
            parent = parent.parent
    return directories


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
    # The IPA stays media-free. These keys make its read-only user-media
    # location reachable through Files rather than silently stranding it in
    # an app-private root.
    if plist.get("UIFileSharingEnabled") is not True:
        fail("IPA Info.plist does not enable Files sharing for user media")
    if plist.get("LSSupportsOpeningDocumentsInPlace") is not True:
        fail("IPA Info.plist does not support opening user media in place")


def validate_arm64_macho(raw: bytes) -> None:
    # A mach_header_64 is 32 bytes. Check the architecture and MH_EXECUTE,
    # rather than accepting the first eight bytes of an arbitrary file.
    if len(raw) < 32:
        fail("IPA executable is too short for a 64-bit arm64 Mach-O header")
    if raw[:4] != b"\xcf\xfa\xed\xfe" or raw[4:8] != b"\x0c\x00\x00\x01":
        fail("IPA executable is not a 64-bit arm64 Mach-O binary")
    if raw[12:16] != b"\x02\x00\x00\x00":
        fail("IPA arm64 Mach-O is not an executable image")
    command_count = int.from_bytes(raw[16:20], "little")
    commands_size = int.from_bytes(raw[20:24], "little")
    if command_count == 0 or commands_size == 0 or commands_size > len(raw) - 32:
        fail("IPA arm64 Mach-O has an invalid load-command region")

    # The IPA intentionally has no Frameworks directory: CMake builds SDL
    # and its dependencies statically for iPadOS.  Check the actual Mach-O
    # dependency commands as well as the archive paths, so a binary cannot
    # hide an @rpath or @loader_path dependency that would only fail after a
    # sideload.  System iPadOS frameworks and libraries remain valid.
    dylib_commands = {0x0C, 0x18, 0x1F, 0x20, 0x23}
    command_offset = 32
    command_end = command_offset + commands_size
    for _ in range(command_count):
        if command_offset + 8 > command_end:
            fail("IPA arm64 Mach-O load commands are truncated")
        command = int.from_bytes(raw[command_offset:command_offset + 4], "little")
        command_size = int.from_bytes(raw[command_offset + 4:command_offset + 8], "little")
        if command_size < 8 or command_offset + command_size > command_end:
            fail("IPA arm64 Mach-O has an invalid load command size")
        if command in dylib_commands:
            # struct dylib_command contains cmd/cmdsize and a dylib whose
            # name offset is relative to the start of this command.
            if command_size < 24:
                fail("IPA arm64 Mach-O has a truncated dynamic-library command")
            name_offset = int.from_bytes(raw[command_offset + 8:command_offset + 12], "little")
            if name_offset < 24 or name_offset >= command_size:
                fail("IPA arm64 Mach-O has an invalid dynamic-library name offset")
            name_end = raw.find(b"\0", command_offset + name_offset,
                                command_offset + command_size)
            if name_end < 0:
                fail("IPA arm64 Mach-O dynamic-library name is unterminated")
            try:
                library = raw[command_offset + name_offset:name_end].decode("utf-8", "strict")
            except UnicodeDecodeError as error:
                fail(f"IPA arm64 Mach-O dynamic-library name is invalid UTF-8: {error}")
            if not library.startswith(("/System/Library/", "/usr/lib/")):
                fail("IPA arm64 Mach-O references a non-system dynamic library: " + library)
        command_offset += command_size
    if command_offset != command_end:
        fail("IPA arm64 Mach-O load-command sizes do not match the header")


def verify(ipa: str) -> None:
    try:
        with zipfile.ZipFile(ipa) as archive:
            infos = archive.infolist()
            if not infos:
                fail("IPA is empty")
            allowed_files = required_paths()
            allowed_dirs = allowed_directories()
            names: set[str] = set()
            members: dict[str, zipfile.ZipInfo] = {}
            for info in infos:
                validate_member(info)
                if info.is_dir():
                    if info.filename not in allowed_dirs:
                        fail("IPA contains an unexpected directory: " + info.filename)
                elif info.filename not in allowed_files:
                    fail("IPA contains an unexpected payload file: " + info.filename)
                if info.filename in names:
                    fail(f"IPA contains duplicate member: {info.filename}")
                names.add(info.filename)
                members[info.filename] = info
            missing = sorted(required_paths() - names)
            if missing:
                fail("IPA is missing required member(s): " + ", ".join(missing))
            directories = sorted(path for path in required_paths() if members[path].is_dir())
            if directories:
                fail("IPA required member is a directory: " + ", ".join(directories))
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
