#!/usr/bin/env python3
"""Verify an external Project Eon capture receipt without reading game media."""
from __future__ import annotations

import argparse
import hashlib
import importlib.util
import os
from pathlib import Path
import stat


ROOT = Path(__file__).resolve().parents[1]
CAPTURE_RECEIPT_VERSIONS = {"2", "3", "4", "5", "6", "7", "8", "9", "10", "11", "12", "13", "14", "15", "16", "17", "18", "19", "20", "21", "22", "23", "24", "25", "26", "27", "28", "29", "30", "31", "32"}


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
    if info.st_size > 64 * 1024:
        raise ValueError("run-status.txt exceeds the bounded receipt contract")
    fd = os.open(path, os.O_RDONLY | getattr(os, "O_NOFOLLOW", 0) | getattr(os, "O_NONBLOCK", 0))
    with os.fdopen(fd, "rb") as stream:
        if not stat.S_ISREG(os.fstat(stream.fileno()).st_mode):
            raise ValueError("run-status.txt must remain a regular file")
        data = stream.read(64 * 1024 + 1)
    if len(data) > 64 * 1024:
        raise ValueError("run-status.txt exceeds the bounded receipt contract")
    fields: dict[str, str] = {}
    # The runner emits an empty post-input summary for a diagnostic capture
    # when no input-linked records exist.  Keep that one state explicit while
    # continuing to reject blank identities and other malformed fields.
    empty_allowed = {"raw_pc_post_input_site_counts", "late_input_pc_site_counts",
                     "driver_load_return_site_counts"}
    for line in data.decode("utf-8").splitlines():
        if line.count("=") != 1:
            raise ValueError("receipt contains an invalid line")
        key, value = line.split("=", 1)
        if not key or (not value and key not in empty_allowed) or key in fields:
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
    if version in {"4", "5", "6", "7", "8", "9", "10", "11", "12", "13", "14", "15", "16", "17", "18", "19", "20", "21", "22", "23", "24", "26", "27", "28", "29", "30", "31", "32"} and fields.get("recorder_console_over_limit") != "false":
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
    raw_format = ("v9-v19-phased" if version in {"29", "30", "31", "32"} else
                  "v9-v18-phased" if version == "28" else
                  "v9-v16-phased" if version in {"26", "27"} else
                  "v9-v16" if version == "24" else
                  "v9" if version in {"9", "10", "11", "23"} else
                  "v7" if version in {"7", "8"} else "legacy")
    if version in {"24", "26", "27", "28", "29", "30", "31", "32"}:
        tool_hash = load_tool("run_deuteros_amiga_capture")
        if version == "32":
            expected_identity = (tool_hash.TRV2_RECORDER_V22_SHA256,
                                 tool_hash.TRV2_RECORDER_V22_SIZE)
            expected_protocol = "deuteros-amiga-fsuae-v22"
        elif version == "31":
            expected_identity = (tool_hash.TRV2_RECORDER_V21_SHA256,
                                 tool_hash.TRV2_RECORDER_V21_SIZE)
            expected_protocol = "deuteros-amiga-fsuae-v21"
        elif version == "30":
            expected_identity = (tool_hash.TRV2_RECORDER_V20_SHA256,
                                 tool_hash.TRV2_RECORDER_V20_SIZE)
            expected_protocol = "deuteros-amiga-fsuae-v20"
        elif version == "29":
            expected_identity = (tool_hash.TRV2_RECORDER_V19_SHA256,
                                 tool_hash.TRV2_RECORDER_V19_SIZE)
            expected_protocol = "deuteros-amiga-fsuae-v19"
        elif version == "28":
            expected_identity = (tool_hash.TRV2_RECORDER_V18_SHA256,
                                 tool_hash.TRV2_RECORDER_V18_SIZE)
            expected_protocol = "deuteros-amiga-fsuae-v18"
        elif version == "27":
            expected_identity = (tool_hash.TRV2_RECORDER_V17_SHA256,
                                 tool_hash.TRV2_RECORDER_V17_SIZE)
            expected_protocol = "deuteros-amiga-fsuae-v17"
        else:
            expected_identity = (tool_hash.TRV2_RECORDER_V16_SHA256,
                                 tool_hash.TRV2_RECORDER_V16_SIZE)
            expected_protocol = "deuteros-amiga-fsuae-v16"
        require_identity(fields, "recorder", expected_identity)
        if fields.get("recorder_protocol") != expected_protocol:
            raise ValueError("recorder protocol does not match the receipt schema")
    if version in {"7", "8", "9", "10", "11", "23", "24", "26", "27", "28", "29", "30", "31", "32"} and fields.get("raw_pc_format") != raw_format:
        raise ValueError("raw_pc format does not match the reviewed recorder contract")
    counts = tool.parse_raw_pc_observations(directory / "raw-pc.txt", raw_format)
    expected_records = str(sum(counts.values()))
    expected_sites = ",".join(
        f"0x{site:08x}:{counts[site]}" for site in tool.raw_pc_sites_for_format(raw_format) if site in counts)
    if (fields.get("raw_pc_records"), fields.get("raw_pc_site_counts")) != (
            expected_records, expected_sites):
        raise ValueError("raw_pc grammar/count receipt mismatch")
    if raw_format in {"v9", "v9-v16", "v9-v16-phased", "v9-v18-phased", "v9-v19-phased"}:
        pre_key, post_key = "raw_pc_pre_input_site_counts", "raw_pc_post_input_site_counts"
        if pre_key in fields or post_key in fields:
            if pre_key not in fields or post_key not in fields:
                raise ValueError("raw_pc phase-count receipt is incomplete")
            pre_counts, post_counts = tool.parse_raw_pc_phase_counts(directory / "raw-pc.txt", raw_format)
            expected_pre = ",".join(
                f"0x{site:08x}:{pre_counts[site]}" for site in tool.raw_pc_sites_for_format(raw_format) if pre_counts.get(site, 0))
            expected_post = ",".join(
                f"0x{site:08x}:{post_counts[site]}" for site in tool.raw_pc_sites_for_format(raw_format) if post_counts.get(site, 0))
            if (fields[pre_key], fields[post_key]) != (expected_pre, expected_post):
                raise ValueError("raw_pc phase-count receipt mismatch")


def verify_deuteros_raw_pc_opcode_pairs(fields: dict[str, str], directory: Path,
                                        raw_format: str = "v7") -> None:
    """Recompute a v7/v9 opaque IR/memory-pair summary without inferring an ABI."""
    tool = load_tool("run_deuteros_amiga_capture")
    _, pairs = tool.parse_raw_pc_summary(directory / "raw-pc.txt", raw_format)
    expected = ",".join(
        f"0x{site:08x}:" + "+".join(
            f"{ir:04x}/{memory:04x}" for ir, memory in sorted(pairs[site]))
        for site in tool.raw_pc_sites_for_format(raw_format) if site in pairs)
    if fields.get("raw_pc_opcode_pairs") != expected:
        raise ValueError("raw_pc opcode-pair receipt mismatch")


def verify_deuteros_raw_pc_input_chronology(fields: dict[str, str], directory: Path,
                                            raw_format: str = "v9") -> None:
    """Recompute v9's delivery-before-sample chronology, never guest input state."""
    tool = load_tool("run_deuteros_amiga_capture")
    raw = directory / "raw-pc.txt"
    input_receipt = directory / "host-input-receipt.txt"
    links = tool.parse_raw_pc_input_links(raw, raw_format)
    expected_links = sum(ordinal != 0 for ordinal, _ in links)
    if (fields.get("raw_pc_input_links"), fields.get("raw_pc_last_input_ordinal")) != (
            str(expected_links), str(links[-1][0] if links else 0)):
        raise ValueError("raw_pc input-link receipt mismatch")
    try:
        expected_status = tool.raw_pc_input_chronology_status(raw, input_receipt, raw_format)
    except tool.CaptureError as error:
        raise ValueError(f"raw_pc input chronology is invalid: {error}") from error
    expected_fields = dict(line.split("=", 1) for line in expected_status.splitlines())
    if any(fields.get(key) != value for key, value in expected_fields.items()):
        raise ValueError("raw_pc input chronology receipt mismatch")


def verify_deuteros_title_display(fields: dict[str, str], directory: Path) -> None:
    """Recompute v10's bounded display-write receipt without asserting a frame."""
    if fields.get("title_display") != "present":
        return
    tool = load_tool("run_deuteros_amiga_capture")
    if fields.get("title_display_format") != "v10":
        raise ValueError("title-display format does not match the reviewed recorder contract")
    arm_cycles, counts, links, writes = tool.parse_title_display_receipt(
        directory / "title-display.txt")
    try:
        expected_chronology = tool._validate_title_display_input_links(
            links, directory / "host-input-receipt.txt")
    except tool.CaptureError as error:
        raise ValueError(f"title-display input chronology is invalid: {error}") from error
    expected = {
        "title_display_records": str(writes + 1),
        "title_display_arm_cycles": str(arm_cycles),
        "title_display_writes": str(writes),
        "title_display_register_counts": ",".join(
            f"0x{register:04x}:{counts[register]}" for register in sorted(counts)),
        "title_display_input_links": str(sum(ordinal != 0 for ordinal, _ in links)),
        "title_display_last_input_ordinal": str(links[-1][0]),
        "title_display_input_chronology": expected_chronology,
        "title_display_input_chronology_records": str(sum(ordinal != 0 for ordinal, _ in links)),
    }
    if any(fields.get(key) != value for key, value in expected.items()):
        raise ValueError("title-display grammar/count receipt mismatch")


def verify_deuteros_selector_dispatch(fields: dict[str, str], directory: Path) -> None:
    """Cross-bind v17 selector-cell samples to raw PC and host delivery."""
    if fields.get("capture_receipt_version") not in {"27", "28"}:
        return
    tool = load_tool("run_deuteros_amiga_capture")
    try:
        expected_status = tool.selector_dispatch_status(
            directory / "selector-dispatch.txt", directory / "raw-pc.txt",
            directory / "host-input-receipt.txt",
            "v9-v18-phased" if fields.get("capture_receipt_version") == "28" else "v9-v16-phased")
    except tool.CaptureError as error:
        raise ValueError(f"selector-dispatch receipt is invalid: {error}") from error
    expected = dict(line.split("=", 1) for line in expected_status.splitlines())
    actual_keys = {key for key in fields if key == "selector_dispatch" or
                   key.startswith("selector_dispatch_")}
    if actual_keys != set(expected):
        raise ValueError("selector-dispatch receipt fields are incomplete or unexpected")
    if any(fields.get(key) != value for key, value in expected.items()):
        raise ValueError("selector-dispatch grammar/count receipt mismatch")


def verify_deuteros_late_sidecars(fields: dict[str, str], directory: Path) -> None:
    """Recompute late PC/cell joins with version-specific sites and exact fields."""
    version = fields.get("capture_receipt_version")
    if version not in {"29", "30", "31", "32"}:
        if any(key == "late_input_pc" or key.startswith("late_input_pc_")
               or key == "late_selector_dispatch" or key.startswith("late_selector_dispatch_")
               for key in fields):
            raise ValueError("late input sidecars require receipt schema 29 through 32")
        return
    tool = load_tool("run_deuteros_amiga_capture")
    late_version = "v20" if version in {"30", "31", "32"} else "v19"
    try:
        statuses = (tool.late_raw_pc_status(
            directory / "late-input-pc.txt", directory / "host-input-receipt.txt", late_version)
            + tool.late_selector_dispatch_status(
                directory / "late-selector-dispatch.txt", directory / "late-input-pc.txt",
                directory / "host-input-receipt.txt", late_version))
    except tool.CaptureError as error:
        raise ValueError(f"late input sidecar is invalid: {error}") from error
    expected = dict(line.split("=", 1) for line in statuses.splitlines())
    actual_keys = {key for key in fields if key == "late_input_pc" or
                   key.startswith("late_input_pc_") or key == "late_selector_dispatch" or
                   key.startswith("late_selector_dispatch_")}
    if actual_keys != set(expected):
        raise ValueError("late input sidecar fields are incomplete or unexpected")
    if any(fields.get(key) != value for key, value in expected.items()):
        raise ValueError("late input sidecar grammar/count receipt mismatch")


def verify_deuteros_zero_route_observation(fields: dict[str, str], directory: Path) -> None:
    """Recompute schema-31's strict diagnostic-only route receipt."""
    version = fields.get("capture_receipt_version")
    observation_keys = {key for key in fields if key == "zero_route_observation"
                        or key.startswith("zero_route_observation_")}
    if version not in {"31", "32"}:
        if observation_keys:
            raise ValueError("zero-route observation requires receipt schema 31")
        return
    tool = load_tool("run_deuteros_amiga_capture")
    try:
        status = tool.zero_route_observation_status(
            directory / "zero-route-observation.txt", directory / "host-input-receipt.txt")
    except tool.CaptureError as error:
        raise ValueError(f"zero-route observation is invalid: {error}") from error
    expected = dict(line.split("=", 1) for line in status.splitlines())
    if observation_keys != set(expected):
        raise ValueError("zero-route observation receipt fields are incomplete or unexpected")
    if any(fields.get(key) != value for key, value in expected.items()):
        raise ValueError("zero-route observation hash/count receipt mismatch")


def verify_deuteros_late_display(fields: dict[str, str], directory: Path) -> None:
    """Recompute schema-32 later-input display writes and host chronology."""
    if fields.get("capture_receipt_version") != "32":
        if any(key == "late_display" or key.startswith("late_display_") for key in fields):
            raise ValueError("late display sidecar requires receipt schema 32")
        return
    tool = load_tool("run_deuteros_amiga_capture")
    try:
        status = tool.late_display_receipt_status(
            directory / "late-display.txt", directory / "host-input-receipt.txt")
    except tool.CaptureError as error:
        raise ValueError(f"late display sidecar is invalid: {error}") from error
    expected = dict(line.split("=", 1) for line in status.splitlines())
    actual_keys = {key for key in fields if key == "late_display" or key.startswith("late_display_")}
    if actual_keys != set(expected):
        raise ValueError("late display sidecar fields are incomplete or unexpected")
    if any(fields.get(key) != value for key, value in expected.items()):
        raise ValueError("late display sidecar grammar/count receipt mismatch")


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


def verify_capture_intent(fields: dict[str, str], tool) -> None:
    """Check the v11/v22 declared session type against the retained receipt.

    This remains a host-recorder contract only: successful verification cannot
    establish original-game input semantics.
    """
    intent = fields.get("capture_intent")
    if intent not in tool.CAPTURE_INTENTS:
        raise ValueError("capture intent is not in the reviewed finite set")
    status = fields.get("host_input_receipt")
    observed = fields.get("host_input_observed_during_capture")
    if intent == "physical-input":
        if status != "present" or observed != "true" or fields.get("capture_intent_input_requirement") != "required":
            raise ValueError("physical-input capture intent does not match the host-input receipt")
    elif status not in {"absent", "empty"} or observed != "false" \
            or fields.get("capture_intent_input_requirement") != "forbidden":
        raise ValueError("diagnostic-no-input capture intent does not match the host-input receipt")


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
        if version in {"13", "14", "15", "16", "17", "18", "19", "20", "21", "22"} and not tool.known_unhandled_interrupt_observed(
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


def verify_millennium_int93_installation(fields: dict[str, str], directory: Path) -> None:
    """Recompute V21's optional, bounded installer transaction receipt."""
    tool = load_tool("run_millennium_dos_capture")
    expected = tool.int93_installation_status(directory / "int93-installation.raw",
                                              "v21-int93-installation")
    expected_fields = dict(line.split("=", 1) for line in expected.splitlines())
    if any(fields.get(key) != value for key, value in expected_fields.items()):
        raise ValueError("INT 93h installation receipt mismatch")


def verify_millennium_driver_load_returns(fields: dict[str, str], directory: Path) -> None:
    """Recompute the experimental raw post-INT loader return receipt."""
    protocol = "millennium-dos-en-driver-load-return-v1"
    if fields.get("recorder_protocol") != protocol:
        if any(key == "driver_load_return" or key.startswith("driver_load_return_")
               for key in fields):
            raise ValueError("driver-load return sidecar requires its exact recorder protocol")
        return
    tool = load_tool("run_millennium_dos_capture")
    try:
        status = tool.driver_load_return_status(directory / "driver-load-return.raw", protocol)
    except tool.CaptureError as error:
        raise ValueError(f"driver-load return sidecar is invalid: {error}") from error
    expected = dict(line.split("=", 1) for line in status.splitlines())
    actual_keys = {key for key in fields if key == "driver_load_return"
                   or key.startswith("driver_load_return_")}
    if actual_keys != set(expected):
        raise ValueError("driver-load return receipt fields are incomplete or unexpected")
    if expected.get("driver_load_return") != "present":
        raise ValueError("experimental driver-load return sidecar is missing")
    if any(fields.get(key) != value for key, value in expected.items()):
        raise ValueError("driver-load return receipt hash/count mismatch")


def verify_deuteros_source_contract(fields: dict[str, str], version: str, tool) -> None:
    """Bind v23+ provenance to the physical source layout without inventing a ZIP."""
    if version not in {"23", "24", "26", "27", "28", "29", "30", "31", "32"}:
        require_identity(fields, "source_release", (tool.EXPECTED_RELEASE_SHA256, tool.EXPECTED_RELEASE_SIZE))
        return
    if fields.get("content_release_sha256") != tool.EXPECTED_RELEASE_SHA256:
        raise ValueError("Deuteros content-release identity does not match the reviewed set")
    layout = fields.get("source_layout")
    container = fields.get("source_container")
    if layout == tool.SOURCE_LAYOUT_RELEASE:
        if container != "single-outer-zip":
            raise ValueError("nested-release source container is invalid")
        require_identity(fields, "source_release", (tool.EXPECTED_RELEASE_SHA256, tool.EXPECTED_RELEASE_SIZE))
    elif layout == tool.SOURCE_LAYOUT_STANDALONE:
        if container != "two-independent-zip-files":
            raise ValueError("standalone source container is invalid")
        if "source_release_sha256" in fields or "source_release_bytes" in fields:
            raise ValueError("standalone source must not claim a physical outer-release ZIP")
    else:
        raise ValueError("Deuteros source layout is unreviewed")
    require_identity(fields, "disk1_archive", (tool.EXPECTED_DISK1_ARCHIVE_SHA256,
                                                 tool.EXPECTED_DISK1_ARCHIVE_SIZE))
    require_identity(fields, "disk2_archive", (tool.EXPECTED_DISK2_ARCHIVE_SHA256,
                                                 tool.EXPECTED_DISK2_ARCHIVE_SIZE))


def verify_deuteros_recorder_identity(fields: dict[str, str], version: str, tool) -> None:
    """Bind the recorder generation to its receipt grammar, preserving old receipts."""
    digest = fields.get("recorder_sha256")
    if version == "32":
        require_identity(fields, "recorder", (tool.TRV2_RECORDER_V22_SHA256,
                                                 tool.TRV2_RECORDER_V22_SIZE))
        if fields.get("recorder_protocol") != "deuteros-amiga-fsuae-v22":
            raise ValueError("recorder protocol does not match the v22 receipt schema")
        return
    if version == "31":
        if tool.TRV2_RECORDER_V21_SHA256 == "UNPINNED" or tool.TRV2_RECORDER_V21_SIZE <= 0:
            raise ValueError("schema-31 recorder identity is unavailable until v21 is independently pinned")
        require_identity(fields, "recorder", (tool.TRV2_RECORDER_V21_SHA256,
                                                 tool.TRV2_RECORDER_V21_SIZE))
        if fields.get("recorder_protocol") != "deuteros-amiga-fsuae-v21":
            raise ValueError("recorder protocol does not match the v21 receipt schema")
        return
    if version == "30":
        require_identity(fields, "recorder", (tool.TRV2_RECORDER_V20_SHA256,
                                                 tool.TRV2_RECORDER_V20_SIZE))
        if fields.get("recorder_protocol") != "deuteros-amiga-fsuae-v20":
            raise ValueError("recorder protocol does not match the v20 receipt schema")
        return
    if version == "29":
        require_identity(fields, "recorder", (tool.TRV2_RECORDER_V19_SHA256,
                                                 tool.TRV2_RECORDER_V19_SIZE))
        if fields.get("recorder_protocol") != "deuteros-amiga-fsuae-v19":
            raise ValueError("recorder protocol does not match the v19 receipt schema")
        return
    if version == "28":
        require_identity(fields, "recorder", (tool.TRV2_RECORDER_V18_SHA256,
                                                 tool.TRV2_RECORDER_V18_SIZE))
        if fields.get("recorder_protocol") != "deuteros-amiga-fsuae-v18":
            raise ValueError("recorder protocol does not match the v18 receipt schema")
        return
    if version == "27":
        require_identity(fields, "recorder", (tool.TRV2_RECORDER_V17_SHA256,
                                                 tool.TRV2_RECORDER_V17_SIZE))
        if fields.get("recorder_protocol") != "deuteros-amiga-fsuae-v17":
            raise ValueError("recorder protocol does not match the v17 receipt schema")
        return
    if version in {"24", "26"}:
        require_identity(fields, "recorder", (tool.TRV2_RECORDER_V16_SHA256,
                                                 tool.TRV2_RECORDER_V16_SIZE))
        if fields.get("recorder_protocol") != "deuteros-amiga-fsuae-v16":
            raise ValueError("recorder protocol does not match the v16 receipt schema")
        return
    if version == "23":
        # Older schema-23 receipts predate recorder_protocol. Preserve their
        # admitted v10-v15 identities, while preventing v16 from claiming the
        # narrower historical grammar when raw_pc is absent or omits $218cc.
        if digest == tool.TRV2_RECORDER_V16_SHA256:
            raise ValueError("v16 recorder requires receipt schema 24")
        if digest not in set(tool.reviewed_recorder_hashes().values()) - {tool.TRV2_RECORDER_V16_SHA256}:
            raise ValueError("schema 23 recorder identity is not a reviewed pre-v16 build")
        protocol = fields.get("recorder_protocol")
        if protocol is not None:
            if protocol != "deuteros-amiga-fsuae-v15":
                raise ValueError("recorder protocol does not match the v15 receipt schema")
            require_identity(fields, "recorder", (tool.TRV2_RECORDER_V15_SHA256, 62_014_944))
        return


def verify(kind: str, directory: Path, *, allow_experimental_observer: bool = False) -> str:
    if not directory.is_absolute() or directory.is_symlink() or not directory.is_dir():
        raise ValueError("capture directory must be an absolute non-symlink directory")
    fields = receipt(directory / "run-status.txt")
    version = require_receipt_schema(fields)
    if version in {"31", "32"} and kind != "deuteros-amiga":
        raise ValueError(f"capture receipt schema {version} is only supported for Deuteros Amiga")
    if version == "25":
        if kind != "millennium-dos":
            raise ValueError("operand DOS schema 25 is only supported for Millennium DOS")
        operand = load_tool("millennium_dos_operand_protocol")
        operand.verify_fields(fields, directory,
                              allow_experimental_observer=allow_experimental_observer)
        return version
    if version == "24" and kind == "millennium-dos":
        terminal = load_tool("millennium_dos_terminal_protocol")
        terminal.verify_fields(fields, directory,
                               allow_experimental_observer=allow_experimental_observer)
        return version
    if kind == "millennium-dos":
        if version == "23":
            raise ValueError("capture receipt schema 23 is only supported for Deuteros Amiga")
        tool = load_tool("run_millennium_dos_capture")
        require_identity(fields, "source_release", (tool.EXPECTED_RELEASE_SHA256, tool.EXPECTED_RELEASE_SIZE))
        recorder_protocol = fields.get("recorder_protocol", "v11")
        recorder_admission = fields.get("recorder_admission", "pinned")
        experimental_protocol = recorder_protocol in tool.EXPERIMENTAL_OBSERVER_PROTOCOLS
        if (recorder_protocol not in tool.RECORDER_PROTOCOLS
                and not (experimental_protocol and recorder_admission ==
                        "experimental-observer-not-for-recovery")):
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
        if version == "21" and recorder_protocol != "v21-int93-installation":
            raise ValueError("v21 receipt must retain its INT 93h installation recorder protocol")
        if version not in {"12", "13", "14", "15", "16", "17", "18", "19", "20", "21", "22"} and recorder_protocol != "v11":
            raise ValueError("pre-v12 receipt must retain the v11 recorder protocol")
        if recorder_admission == "experimental-observer-not-for-recovery":
            if not allow_experimental_observer:
                raise ValueError("experimental observer receipt is not recovery-admissible")
            expected_recorders = tool.EXPERIMENTAL_OBSERVER_PROTOCOLS.get(recorder_protocol)
            if expected_recorders is None:
                raise ValueError("experimental observer receipt uses an unreviewed protocol")
            if isinstance(expected_recorders, str):
                expected_recorders = frozenset({expected_recorders})
            if fields.get("recorder_sha256") not in expected_recorders:
                raise ValueError("experimental observer receipt uses an unreviewed binary")
            expected_recorder = fields["recorder_sha256"]
            expected_sizes = tool.EXPERIMENTAL_OBSERVER_SIZES.get(recorder_protocol, {})
            expected_size = expected_sizes.get(expected_recorder)
            if expected_size is not None and fields.get("recorder_bytes") != str(expected_size):
                raise ValueError("experimental observer receipt binary size does not match its hash")
        elif recorder_admission == "pinned":
            expected_recorder = tool.RECORDER_PROTOCOLS[recorder_protocol][1]
        else:
            raise ValueError("unknown recorder admission state")
        require_identity(fields, "recorder", (expected_recorder, int(fields["recorder_bytes"])))
        experimental_observer = recorder_admission == "experimental-observer-not-for-recovery"
        if experimental_observer:
            if fields.get("events_raw") != "not-collected" or fields.get("results_raw") != "not-collected":
                raise ValueError("experimental observer must not claim legacy raw streams")
            if fields.get("title_input_checkpoint") != "not-collected":
                raise ValueError("experimental observer must not claim title-input chronology")
        else:
            verify_file(fields, directory, "events_raw", "events.raw")
            verify_file(fields, directory, "results_raw", "results.raw")
        if not experimental_observer and version in {"3", "4", "5", "6", "7", "8", "9", "10", "11", "12", "13", "14", "15", "16", "17", "18", "19", "20", "21", "22"} and fields.get("results_raw") == "present":
            counts = tool.parse_raw_results(directory / "results.raw", recorder_protocol)
            shapes = ",".join(f"{key}:{counts[key]}" for key in sorted(counts))
            if (fields.get("results_raw_records"), fields.get("results_raw_shapes")) != (
                    str(sum(counts.values())), shapes):
                raise ValueError("results_raw grammar/count receipt mismatch")
        verify_file(fields, directory, "host_input_receipt", "host-input-receipt.raw")
        if version in {"5", "6", "7", "8", "9", "10", "11", "12", "13", "14", "15", "16", "17", "18", "19", "20", "21", "22"}:
            verify_millennium_host_input_summary(fields, directory)
        if version in {"6", "7", "8", "9", "10", "11", "12", "13", "14", "15", "16", "17", "18", "19", "20", "21", "22"}:
            verify_millennium_machine_profile(fields, directory)
        if version in {"10", "11", "12", "13", "14", "15", "16", "17", "18", "19", "20", "21", "22"}:
            verify_millennium_termination(fields, directory, version)
        if not experimental_observer and version in {"13", "14", "15", "16", "17", "18", "19", "20", "21", "22"}:
            if fields.get("results_raw") != "present":
                raise ValueError("v13 title-input checkpoint requires a raw result log")
            verify_millennium_title_input_checkpoint(fields, directory)
        if version == "14" or (version == "22" and recorder_protocol == "v14-normal-core-history"):
            verify_millennium_normal_core_history(fields, directory)
        if version == "18" or (version == "22" and recorder_protocol == "v18-ivt-entry"):
            verify_millennium_normal_core_anomaly(fields, directory)
        if version == "19" or (version == "22" and recorder_protocol == "v19-int93-vector"):
            verify_millennium_int93_vector(fields, directory)
        if version == "20" or (version == "22" and recorder_protocol == "v20-title-entry-transfer"):
            verify_file(fields, directory, "title_entry_transfer", "title-entry-transfer.raw")
            verify_millennium_title_entry_transfer(fields, directory)
        if version == "21" or (version == "22" and recorder_protocol == "v21-int93-installation"):
            if fields.get("int93_installation") == "present":
                verify_file(fields, directory, "int93_installation", "int93-installation.raw")
                verify_millennium_int93_installation(fields, directory)
            elif fields.get("int93_installation") != "absent":
                raise ValueError("INT 93h installation receipt has an invalid optional state")
        if version == "22" and recorder_protocol == "millennium-dos-en-driver-load-return-v1":
            verify_file(fields, directory, "driver_load_return", "driver-load-return.raw")
            verify_millennium_driver_load_returns(fields, directory)
        if version == "22":
            verify_capture_intent(fields, tool)
    else:
        tool = load_tool("run_deuteros_amiga_capture")
        verify_deuteros_source_contract(fields, version, tool)
        verify_deuteros_recorder_identity(fields, version, tool)
        require_identity(fields, "kickstart_archive", (tool.EXPECTED_KICKSTART_SHA256, tool.EXPECTED_KICKSTART_SIZE))
        if version not in {"23", "24", "26", "27", "28", "29", "30", "31", "32"}:
            require_identity(fields, "disk1_archive", (tool.EXPECTED_DISK1_ARCHIVE_SHA256,
                                                         tool.EXPECTED_DISK1_ARCHIVE_SIZE))
            require_identity(fields, "disk2_archive", (tool.EXPECTED_DISK2_ARCHIVE_SHA256,
                                                         tool.EXPECTED_DISK2_ARCHIVE_SIZE))
        if fields.get("recorder_sha256") not in tool.reviewed_recorder_hashes().values():
            raise ValueError("recorder identity is not a reviewed FS-UAE build")
        require_identity(fields, "recorder", (fields["recorder_sha256"], int(fields["recorder_bytes"])))
        verify_file(fields, directory, "raw_pc", "raw-pc.txt")
        verify_file(fields, directory, "host_input_receipt", "host-input-receipt.txt")
        if version in {"10", "11", "23", "24", "26", "27", "28", "29", "30", "31", "32"}:
            verify_file(fields, directory, "title_display", "title-display.txt")
        if version in {"27", "28"}:
            verify_file(fields, directory, "selector_dispatch", "selector-dispatch.txt")
        if version in {"29", "30", "31", "32"}:
            verify_file(fields, directory, "late_input_pc", "late-input-pc.txt")
            verify_file(fields, directory, "late_selector_dispatch", "late-selector-dispatch.txt")
        if version in {"3", "4", "5", "6", "7", "8", "9", "10", "11", "23", "24", "26", "27", "28", "29", "30", "31", "32"}:
            verify_deuteros_raw_pc_summary(fields, directory, version)
        if version in {"5", "6", "7", "8", "9", "10", "11", "23", "24", "26", "27", "28", "29", "30", "31", "32"}:
            verify_deuteros_host_input_summary(fields, directory)
        if version in {"6", "7", "8", "9", "10", "11", "23", "24", "26", "27", "28", "29", "30", "31", "32"}:
            verify_deuteros_timing_profile(fields, directory)
        if version == "8" and fields.get("raw_pc") == "present":
            verify_deuteros_raw_pc_opcode_pairs(fields, directory)
        if version in {"9", "10", "11", "23", "24", "26", "27", "28", "29", "30", "31", "32"} and fields.get("raw_pc") == "present":
            raw_format = ("v9-v19-phased" if version in {"29", "30", "31", "32"} else
                          "v9-v18-phased" if version == "28" else
                          "v9-v16-phased" if version in {"26", "27"} else
                          "v9-v16" if version == "24" else "v9")
            verify_deuteros_raw_pc_opcode_pairs(fields, directory, raw_format)
            verify_deuteros_raw_pc_input_chronology(fields, directory, raw_format)
        if version in {"10", "11", "23", "24", "26", "27", "28", "29", "30", "31", "32"}:
            verify_deuteros_title_display(fields, directory)
        if version in {"27", "28"}:
            verify_deuteros_selector_dispatch(fields, directory)
        verify_deuteros_late_sidecars(fields, directory)
        verify_deuteros_zero_route_observation(fields, directory)
        if version == "32":
            verify_file(fields, directory, "late_display", "late-display.txt")
            verify_deuteros_late_display(fields, directory)
        if version in {"11", "23", "24", "26", "27", "28", "29", "30", "31", "32"}:
            verify_capture_intent(fields, tool)
    verify_console(fields, directory)
    verify_console_admission(fields, version)
    config = directory / ("recorder.conf" if kind == "millennium-dos" else "deuteros-amiga-capture.fs-uae")
    actual = digest(config)
    if (fields.get("configuration_sha256"), fields.get("configuration_bytes")) != (actual[0], str(actual[1])):
        raise ValueError("configuration hash or size mismatch")
    return version


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--kind", choices=("millennium-dos", "deuteros-amiga"), required=True)
    parser.add_argument("--capture", type=Path, required=True)
    parser.add_argument("--allow-experimental-observer", action="store_true",
                        help="Verify integrity of an unpinned observer run; never admits it for recovery")
    args = parser.parse_args()
    try:
        verified_version = verify(args.kind, args.capture,
                                  allow_experimental_observer=args.allow_experimental_observer)
    # The bounded grammar helpers are intentionally shared with the capture
    # runners. They reject malformed recorder lines with their own
    # RuntimeError-derived CaptureError, which must be a normal fail-closed
    # receipt rejection here rather than an uncaught verifier traceback.
    except (OSError, ValueError, KeyError, RuntimeError) as error:
        print(f"CAPTURE RECEIPT REJECTED  {error}")
        return 2
    if verified_version == "25":
        print(f"EXPERIMENTAL CAPTURE RECEIPT VERIFIED; NOT ADMITTED FOR RECOVERY  {args.kind}  {args.capture}")
    else:
        print(f"CAPTURE RECEIPT VERIFIED  {args.kind}  {args.capture}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
