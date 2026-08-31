#!/usr/bin/env python3
"""Verify an external Project Eon capture receipt without reading game media."""
from __future__ import annotations

import argparse
import hashlib
import importlib.util
from pathlib import Path
import stat


ROOT = Path(__file__).resolve().parents[1]
CAPTURE_RECEIPT_VERSIONS = {"2", "3", "4", "5", "6", "7", "8", "9"}


def load_tool(name: str):
    spec = importlib.util.spec_from_file_location(name, ROOT / "tools" / f"{name}.py")
    if not spec or not spec.loader:
        raise RuntimeError(f"unable to load {name}")
    module = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(module)
    return module


def digest(path: Path) -> tuple[str, int]:
    value = hashlib.sha256()
    size = 0
    with path.open("rb") as stream:
        for block in iter(lambda: stream.read(1024 * 1024), b""):
            value.update(block)
            size += len(block)
    return value.hexdigest(), size


def receipt(path: Path) -> dict[str, str]:
    info = path.lstat()
    if stat.S_ISLNK(info.st_mode) or not stat.S_ISREG(info.st_mode):
        raise ValueError("run-status.txt must be a regular non-symlink file")
    fields: dict[str, str] = {}
    for line in path.read_text(encoding="utf-8").splitlines():
        if line.count("=") != 1:
            raise ValueError("receipt contains an invalid line")
        key, value = line.split("=", 1)
        if not key or not value or key in fields:
            raise ValueError("receipt has an empty or duplicate field")
        fields[key] = value
    return fields


def require_identity(fields: dict[str, str], prefix: str, expected: tuple[str, int]) -> None:
    actual = (fields.get(prefix + "_sha256"), fields.get(prefix + "_bytes"))
    if actual != (expected[0], str(expected[1])):
        raise ValueError(f"{prefix} identity does not match the reviewed contract")


def require_receipt_schema(fields: dict[str, str]) -> str:
    version = fields.get("capture_receipt_version")
    if version not in CAPTURE_RECEIPT_VERSIONS:
        raise ValueError(
            "capture receipt schema is unsupported; rerun the physical capture with the current receipt format")
    return version


def is_sha256(value: str | None) -> bool:
    return value is not None and len(value) == 64 and all(character in "0123456789abcdef" for character in value)


def verify_file(fields: dict[str, str], directory: Path, key: str, filename: str) -> None:
    state = fields.get(key)
    path = directory / filename
    if state == "absent":
        if path.exists() or path.is_symlink(): raise ValueError(f"{key} is unexpectedly present")
        return
    if state not in {"present", "empty"}: raise ValueError(f"{key} has invalid state")
    info = path.lstat()
    if stat.S_ISLNK(info.st_mode) or not stat.S_ISREG(info.st_mode): raise ValueError(f"{key} is unsafe")
    actual = digest(path)
    if state == "empty" and actual[1] != 0: raise ValueError(f"{key} is not empty")
    if state == "present" and (fields.get(key + "_sha256"), fields.get(key + "_bytes")) != (actual[0], str(actual[1])):
        raise ValueError(f"{key} hash or size mismatch")


def verify_console(fields: dict[str, str], directory: Path) -> None:
    path = directory / "recorder-console.log"
    info = path.lstat()
    if stat.S_ISLNK(info.st_mode) or not stat.S_ISREG(info.st_mode): raise ValueError("recorder console is unsafe")
    actual = digest(path)
    try:
        total = int(fields["recorder_console_total_bytes"])
    except (KeyError, ValueError):
        raise ValueError("recorder console receipt has no valid total")
    if (fields.get("recorder_console") != "present" or fields.get("recorder_console_retained_bytes") != str(actual[1])
            or fields.get("recorder_console_retained_sha256") != actual[0]
            or total < actual[1] or not is_sha256(fields.get("recorder_console_sha256"))):
        raise ValueError("recorder console receipt mismatch")


def verify_console_admission(fields: dict[str, str], version: str) -> None:
    """Reject a v4+ recorder runaway without rewriting retained evidence."""
    if version in {"4", "5", "6", "7", "8", "9"} and fields.get("recorder_console_over_limit") != "false":
        raise ValueError("recorder console exceeded its safety cap; capture is not admitted")


def verify_millennium_machine_profile(fields: dict[str, str], directory: Path) -> None:
    """Bind v6's finite display-machine label to the generated config text."""
    profile = fields.get("machine_profile")
    tool = load_tool("run_millennium_dos_capture")
    if profile not in tool.MACHINE_PROFILES:
        raise ValueError("machine profile is not in the reviewed finite profile set")
    configuration = (directory / "recorder.conf").read_text(encoding="utf-8")
    if f"machine={profile}\n" not in configuration:
        raise ValueError("machine profile does not match the generated configuration")


def verify_deuteros_raw_pc_summary(fields: dict[str, str], directory: Path) -> None:
    """Verify v3's raw-recorder grammar/count receipt without inferring ABI."""
    if fields.get("raw_pc") != "present":
        return
    tool = load_tool("run_deuteros_amiga_capture")
    counts = tool.parse_raw_pc_observations(directory / "raw-pc.txt")
    expected_records = str(sum(counts.values()))
    expected_sites = ",".join(
        f"0x{site:08x}:{counts[site]}" for site in tool.RAW_PC_SITES if site in counts)
    if (fields.get("raw_pc_records"), fields.get("raw_pc_site_counts")) != (
            expected_records, expected_sites):
        raise ValueError("raw_pc grammar/count receipt mismatch")


def verify_deuteros_host_input_summary(fields: dict[str, str], directory: Path) -> None:
    if fields.get("host_input_receipt") != "present":
        return
    tool = load_tool("run_deuteros_amiga_capture")
    count = tool.parse_host_input_receipt(directory / "host-input-receipt.txt")
    if fields.get("host_input_receipt_records") != str(count):
        raise ValueError("host-input receipt grammar/count mismatch")


def verify_millennium_host_input_summary(fields: dict[str, str], directory: Path) -> None:
    if fields.get("host_input_receipt") != "present":
        return
    tool = load_tool("run_millennium_dos_capture")
    count = tool.parse_host_input_receipt(directory / "host-input-receipt.raw")
    if fields.get("host_input_receipt_records") != str(count):
        raise ValueError("host-input receipt grammar/count mismatch")


def verify(kind: str, directory: Path) -> None:
    if not directory.is_absolute() or directory.is_symlink() or not directory.is_dir():
        raise ValueError("capture directory must be an absolute non-symlink directory")
    fields = receipt(directory / "run-status.txt")
    version = require_receipt_schema(fields)
    if kind == "millennium-dos":
        tool = load_tool("run_millennium_dos_capture")
        require_identity(fields, "source_release", (tool.EXPECTED_RELEASE_SHA256, tool.EXPECTED_RELEASE_SIZE))
        require_identity(fields, "recorder", (tool.EXPECTED_RECORDER_SHA256, int(fields["recorder_bytes"])))
        verify_file(fields, directory, "events_raw", "events.raw")
        verify_file(fields, directory, "results_raw", "results.raw")
        if version in {"3", "4", "5", "6", "7", "8", "9"} and fields.get("results_raw") == "present":
            counts = tool.parse_raw_results(directory / "results.raw")
            shapes = ",".join(f"{key}:{counts[key]}" for key in sorted(counts))
            if (fields.get("results_raw_records"), fields.get("results_raw_shapes")) != (
                    str(sum(counts.values())), shapes):
                raise ValueError("results_raw grammar/count receipt mismatch")
        verify_file(fields, directory, "host_input_receipt", "host-input-receipt.raw")
        if version in {"5", "6", "7", "8", "9"}:
            verify_millennium_host_input_summary(fields, directory)
        if version in {"6", "7", "8", "9"}:
            verify_millennium_machine_profile(fields, directory)
    else:
        tool = load_tool("run_deuteros_amiga_capture")
        require_identity(fields, "source_release", (tool.EXPECTED_RELEASE_SHA256, tool.EXPECTED_RELEASE_SIZE))
        require_identity(fields, "kickstart_archive", (tool.EXPECTED_KICKSTART_SHA256, tool.EXPECTED_KICKSTART_SIZE))
        require_identity(fields, "recorder", (tool.EXPECTED_RECORDER_SHA256, int(fields["recorder_bytes"])))
        verify_file(fields, directory, "raw_pc", "raw-pc.txt")
        verify_file(fields, directory, "host_input_receipt", "host-input-receipt.txt")
        if version in {"3", "4", "5"}:
            verify_deuteros_raw_pc_summary(fields, directory)
        if version == "5":
            verify_deuteros_host_input_summary(fields, directory)
    verify_console(fields, directory)
    verify_console_admission(fields, version)
    config = directory / ("recorder.conf" if kind == "millennium-dos" else "deuteros-amiga-capture.fs-uae")
    actual = digest(config)
    if (fields.get("configuration_sha256"), fields.get("configuration_bytes")) != (actual[0], str(actual[1])):
        raise ValueError("configuration hash or size mismatch")


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--kind", choices=("millennium-dos", "deuteros-amiga"), required=True)
    parser.add_argument("--capture", type=Path, required=True)
    args = parser.parse_args()
    try: verify(args.kind, args.capture)
    except (OSError, ValueError, KeyError) as error:
        print(f"CAPTURE RECEIPT REJECTED  {error}")
        return 2
    print(f"CAPTURE RECEIPT VERIFIED  {args.kind}  {args.capture}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
