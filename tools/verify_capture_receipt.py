#!/usr/bin/env python3
"""Verify an external Project Eon capture receipt without reading game media."""
from __future__ import annotations

import argparse
import hashlib
import importlib.util
from pathlib import Path
import stat


ROOT = Path(__file__).resolve().parents[1]
CAPTURE_RECEIPT_VERSIONS = {"2", "3", "4", "5", "6", "7", "8", "9", "10", "11", "12", "13", "14", "15", "16", "17", "18", "19", "20"}


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
    if version in {"4", "5", "6", "7", "8", "9", "10", "11", "12", "13", "14", "15", "16", "17", "18", "19", "20"} and fields.get("recorder_console_over_limit") != "false":
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


def verify_deuteros_raw_pc_summary(fields: dict[str, str], directory: Path, version: str) -> None:
    """Verify v3's raw-recorder grammar/count receipt without inferring ABI."""
    if fields.get("raw_pc") != "present":
        return
    tool = load_tool("run_deuteros_amiga_capture")
    raw_format = "v9" if version == "9" else "v7" if version in {"7", "8"} else "legacy"
    if version in {"7", "8", "9"} and fields.get("raw_pc_format") != raw_format:
        raise ValueError("raw_pc format does not match the reviewed recorder contract")
    counts = tool.parse_raw_pc_observations(directory / "raw-pc.txt", raw_format)
    expected_records = str(sum(counts.values()))
    expected_sites = ",".join(
        f"0x{site:08x}:{counts[site]}" for site in tool.RAW_PC_SITES if site in counts)
    if (fields.get("raw_pc_records"), fields.get("raw_pc_site_counts")) != (
            expected_records, expected_sites):
        raise ValueError("raw_pc grammar/count receipt mismatch")


def verify_deuteros_raw_pc_opcode_pairs(fields: dict[str, str], directory: Path,
                                        raw_format: str = "v7") -> None:
    """Recompute a v7/v9 opaque IR/memory-pair summary without inferring an ABI."""
    tool = load_tool("run_deuteros_amiga_capture")
    _, pairs = tool.parse_raw_pc_summary(directory / "raw-pc.txt", raw_format)
    expected = ",".join(
        f"0x{site:08x}:" + "+".join(
            f"{ir:04x}/{memory:04x}" for ir, memory in sorted(pairs[site]))
        for site in tool.RAW_PC_SITES if site in pairs)
    if fields.get("raw_pc_opcode_pairs") != expected:
        raise ValueError("raw_pc opcode-pair receipt mismatch")


def verify_deuteros_raw_pc_input_chronology(fields: dict[str, str], directory: Path) -> None:
    """Recompute v9's delivery-before-sample chronology, never guest input state."""
    tool = load_tool("run_deuteros_amiga_capture")
    raw = directory / "raw-pc.txt"
    input_receipt = directory / "host-input-receipt.txt"
    links = tool.parse_raw_pc_input_links(raw)
    expected_links = sum(ordinal != 0 for ordinal, _ in links)
    if (fields.get("raw_pc_input_links"), fields.get("raw_pc_last_input_ordinal")) != (
            str(expected_links), str(links[-1][0] if links else 0)):
        raise ValueError("raw_pc input-link receipt mismatch")
    try:
        expected_status = tool.raw_pc_input_chronology_status(raw, input_receipt)
    except tool.CaptureError as error:
        raise ValueError(f"raw_pc input chronology is invalid: {error}") from error
    expected_fields = dict(line.split("=", 1) for line in expected_status.splitlines())
    if any(fields.get(key) != value for key, value in expected_fields.items()):
        raise ValueError("raw_pc input chronology receipt mismatch")


def verify_deuteros_host_input_summary(fields: dict[str, str], directory: Path) -> None:
    if fields.get("host_input_receipt") != "present":
        return
    tool = load_tool("run_deuteros_amiga_capture")
    count = tool.parse_host_input_receipt(directory / "host-input-receipt.txt")
    if fields.get("host_input_receipt_records") != str(count):
        raise ValueError("host-input receipt grammar/count mismatch")


def verify_deuteros_timing_profile(fields: dict[str, str], directory: Path) -> None:
    """Bind v6's finite timing label to the generated FS-UAE configuration."""
    profile = fields.get("timing_profile")
    tool = load_tool("run_deuteros_amiga_capture")
    if profile not in tool.TIMING_PROFILES:
        raise ValueError("timing profile is not in the reviewed finite profile set")
    configuration = (directory / "deuteros-amiga-capture.fs-uae").read_text(encoding="utf-8")
    if f"warp_mode = {tool.TIMING_PROFILES[profile]}\n" not in configuration:
        raise ValueError("timing profile does not match the generated configuration")


def verify_millennium_host_input_summary(fields: dict[str, str], directory: Path) -> None:
    if fields.get("host_input_receipt") != "present":
        return
    tool = load_tool("run_millennium_dos_capture")
    count = tool.parse_host_input_receipt(directory / "host-input-receipt.raw")
    if fields.get("host_input_receipt_records") != str(count):
        raise ValueError("host-input receipt grammar/count mismatch")


def verify_millennium_title_input_checkpoint(fields: dict[str, str], directory: Path) -> None:
    """Recompute v13 chronology without promoting it into guest input proof."""
    tool = load_tool("run_millennium_dos_capture")
    protocol = fields.get("recorder_protocol", "v13-title-poll")
    expected = tool.title_input_checkpoint_status(directory / "results.raw",
        directory / "host-input-receipt.raw", protocol)
    expected_fields = dict(line.split("=", 1) for line in expected.splitlines())
    for key, value in expected_fields.items():
        if fields.get(key) != value:
            raise ValueError("title-input checkpoint receipt mismatch")
    polls = tool.title_input_poll_ordinals(directory / "results.raw", protocol)
    if (fields.get("results_raw_title_input_polls"),
            fields.get("results_raw_last_host_key_ordinal")) != (
                str(len(polls)), str(polls[-1] if polls else 0)):
        raise ValueError("title-input poll raw-result summary mismatch")


def verify_millennium_termination(fields: dict[str, str], directory: Path, version: str) -> None:
    tool = load_tool("run_millennium_dos_capture")
    reason = fields.get("termination_reason")
    if reason not in tool.TERMINATION_REASONS:
        raise ValueError("Millennium capture has an invalid termination reason")
    expected_status = {
        "timeout": "124",
        "console-safety-cap": "125",
        "known-unhandled-interrupt": "126",
    }.get(reason)
    if expected_status is not None and fields.get("exit_status") != expected_status:
        raise ValueError("Millennium capture termination reason does not match exit status")
    if reason == "known-unhandled-interrupt":
        if fields.get("results_raw") != "present":
            raise ValueError("Millennium early stop requires a retained raw result log")
        if version == "10" and not tool.known_v10_early_stop_sequence(directory / "results.raw"):
            raise ValueError("Millennium early stop requires the exact v10 raw diagnostic sequence")
        if version == "11" and not tool.known_v11_early_stop_receipt(directory / "results.raw"):
            raise ValueError("Millennium early stop requires the exact v11 raw diagnostic receipt")
        if version == "12" and not tool.known_unhandled_interrupt_observed(
                directory / "results.raw", "v12-predecessor"):
            raise ValueError("Millennium early stop requires the bounded v12 predecessor diagnostic shape")
        if version in {"13", "14", "15", "16", "17", "18", "19", "20"} and not tool.known_unhandled_interrupt_observed(
                directory / "results.raw", fields["recorder_protocol"]):
            raise ValueError("Millennium early stop requires the exact v13 no-poll diagnostic receipt")


def verify_millennium_normal_core_history(fields: dict[str, str], directory: Path) -> None:
    tool = load_tool("run_millennium_dos_capture")
    expected = tool.normal_core_history_status(directory / "normal-core-history.raw",
                                               "v14-normal-core-history")
    expected_fields = dict(line.split("=", 1) for line in expected.splitlines())
    if any(fields.get(key) != value for key, value in expected_fields.items()):
        raise ValueError("normal-core history receipt mismatch")
    if fields.get("termination_reason") == "known-unhandled-interrupt" and \
            fields.get("normal_core_history") != "present":
        raise ValueError("v14 early stop requires a normal-core history record")
    expected_boundary = tool.normal_core_history_boundary_status(
        directory / "normal-core-history.raw", directory / "results.raw",
        "v14-normal-core-history", fields.get("termination_reason", ""))
    expected_boundary_fields = dict(line.split("=", 1) for line in expected_boundary.splitlines())
    if any(fields.get(key) != value for key, value in expected_boundary_fields.items()):
        raise ValueError("normal-core history boundary receipt mismatch")


def verify_millennium_normal_core_anomaly(fields: dict[str, str], directory: Path) -> None:
    tool = load_tool("run_millennium_dos_capture")
    expected = tool.normal_core_anomaly_status(directory / "normal-core-anomaly.raw",
        directory / "results.raw", "v18-ivt-entry", fields.get("termination_reason", ""))
    expected_fields = dict(line.split("=", 1) for line in expected.splitlines())
    if any(fields.get(key) != value for key, value in expected_fields.items()):
        raise ValueError("normal-core anomaly receipt mismatch")


def verify_millennium_int93_vector(fields: dict[str, str], directory: Path) -> None:
    tool = load_tool("run_millennium_dos_capture")
    expected = tool.int93_vector_status(directory / "events.raw", directory / "results.raw",
        "v19-int93-vector", fields.get("termination_reason", ""))
    expected_fields = dict(line.split("=", 1) for line in expected.splitlines())
    if any(fields.get(key) != value for key, value in expected_fields.items()):
        raise ValueError("INT 93h vector receipt mismatch")


def verify_millennium_title_entry_transfer(fields: dict[str, str], directory: Path) -> None:
    tool = load_tool("run_millennium_dos_capture")
    expected = tool.title_entry_transfer_status(
        directory / "title-entry-transfer.raw", directory / "results.raw",
        "v20-title-entry-transfer", fields.get("termination_reason", ""))
    expected_fields = dict(line.split("=", 1) for line in expected.splitlines())
    if any(fields.get(key) != value for key, value in expected_fields.items()):
        raise ValueError("title-entry transfer receipt mismatch")


def verify(kind: str, directory: Path) -> None:
    if not directory.is_absolute() or directory.is_symlink() or not directory.is_dir():
        raise ValueError("capture directory must be an absolute non-symlink directory")
    fields = receipt(directory / "run-status.txt")
    version = require_receipt_schema(fields)
    if kind == "millennium-dos":
        tool = load_tool("run_millennium_dos_capture")
        require_identity(fields, "source_release", (tool.EXPECTED_RELEASE_SHA256, tool.EXPECTED_RELEASE_SIZE))
        recorder_protocol = fields.get("recorder_protocol", "v11")
        if recorder_protocol not in tool.RECORDER_PROTOCOLS:
            raise ValueError("Millennium capture uses an unreviewed recorder protocol")
        if version == "12" and recorder_protocol != "v12-predecessor":
            raise ValueError("v12 receipt must retain its predecessor recorder protocol")
        if version == "13" and recorder_protocol != "v13-title-poll":
            raise ValueError("v13 receipt must retain its title-poll recorder protocol")
        if version == "14" and recorder_protocol != "v14-normal-core-history":
            raise ValueError("v14 receipt must retain its normal-core history recorder protocol")
        if version == "18" and recorder_protocol != "v18-ivt-entry":
            raise ValueError("v18 receipt must retain its IVT-entry recorder protocol")
        if version == "19" and recorder_protocol != "v19-int93-vector":
            raise ValueError("v19 receipt must retain its INT 93h recorder protocol")
        if version == "20" and recorder_protocol != "v20-title-entry-transfer":
            raise ValueError("v20 receipt must retain its title-entry transfer recorder protocol")
        if version not in {"12", "13", "14", "15", "16", "17", "18", "19", "20"} and recorder_protocol != "v11":
            raise ValueError("pre-v12 receipt must retain the v11 recorder protocol")
        require_identity(fields, "recorder", (tool.RECORDER_PROTOCOLS[recorder_protocol][1], int(fields["recorder_bytes"])))
        verify_file(fields, directory, "events_raw", "events.raw")
        verify_file(fields, directory, "results_raw", "results.raw")
        if version in {"3", "4", "5", "6", "7", "8", "9", "10", "11", "12", "13", "14", "15", "16", "17", "18", "19", "20"} and fields.get("results_raw") == "present":
            counts = tool.parse_raw_results(directory / "results.raw", recorder_protocol)
            shapes = ",".join(f"{key}:{counts[key]}" for key in sorted(counts))
            if (fields.get("results_raw_records"), fields.get("results_raw_shapes")) != (
                    str(sum(counts.values())), shapes):
                raise ValueError("results_raw grammar/count receipt mismatch")
        verify_file(fields, directory, "host_input_receipt", "host-input-receipt.raw")
        if version in {"5", "6", "7", "8", "9", "10", "11", "12", "13", "14", "15", "16", "17", "18", "19", "20"}:
            verify_millennium_host_input_summary(fields, directory)
        if version in {"6", "7", "8", "9", "10", "11", "12", "13", "14", "15", "16", "17", "18", "19", "20"}:
            verify_millennium_machine_profile(fields, directory)
        if version in {"10", "11", "12", "13", "14", "15", "16", "17", "18", "19", "20"}:
            verify_millennium_termination(fields, directory, version)
        if version in {"13", "14", "15", "16", "17", "18", "19", "20"}:
            if fields.get("results_raw") != "present":
                raise ValueError("v13 title-input checkpoint requires a raw result log")
            verify_millennium_title_input_checkpoint(fields, directory)
        if version == "14":
            verify_millennium_normal_core_history(fields, directory)
        if version == "18":
            verify_millennium_normal_core_anomaly(fields, directory)
        if version == "19":
            verify_millennium_int93_vector(fields, directory)
        if version == "20":
            verify_file(fields, directory, "title_entry_transfer", "title-entry-transfer.raw")
            verify_millennium_title_entry_transfer(fields, directory)
    else:
        tool = load_tool("run_deuteros_amiga_capture")
        require_identity(fields, "source_release", (tool.EXPECTED_RELEASE_SHA256, tool.EXPECTED_RELEASE_SIZE))
        require_identity(fields, "kickstart_archive", (tool.EXPECTED_KICKSTART_SHA256, tool.EXPECTED_KICKSTART_SIZE))
        require_identity(fields, "recorder", (tool.EXPECTED_RECORDER_SHA256, int(fields["recorder_bytes"])))
        verify_file(fields, directory, "raw_pc", "raw-pc.txt")
        verify_file(fields, directory, "host_input_receipt", "host-input-receipt.txt")
        if version in {"3", "4", "5", "6", "7", "8", "9"}:
            verify_deuteros_raw_pc_summary(fields, directory, version)
        if version in {"5", "6", "7", "8", "9"}:
            verify_deuteros_host_input_summary(fields, directory)
        if version in {"6", "7", "8", "9"}:
            verify_deuteros_timing_profile(fields, directory)
        if version == "8" and fields.get("raw_pc") == "present":
            verify_deuteros_raw_pc_opcode_pairs(fields, directory)
        if version == "9" and fields.get("raw_pc") == "present":
            verify_deuteros_raw_pc_opcode_pairs(fields, directory, "v9")
            verify_deuteros_raw_pc_input_chronology(fields, directory)
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
    # The bounded grammar helpers are intentionally shared with the capture
    # runners. They reject malformed recorder lines with their own
    # RuntimeError-derived CaptureError, which must be a normal fail-closed
    # receipt rejection here rather than an uncaught verifier traceback.
    except (OSError, ValueError, KeyError, RuntimeError) as error:
        print(f"CAPTURE RECEIPT REJECTED  {error}")
        return 2
    print(f"CAPTURE RECEIPT VERIFIED  {args.kind}  {args.capture}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
