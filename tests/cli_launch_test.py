#!/usr/bin/env python3
"""Exercise every explicit Project Eon platform start against real media.

This is intentionally configured only for developer builds that provide
EON_REAL_DATA_DIR.  CI never receives commercial media, while preservation
workstations gain a repeatable guard against a platform silently falling back
to another release or failing before its SDL event loop starts.
"""

from __future__ import annotations

import os
import json
from pathlib import Path
import hashlib
import re
import subprocess
import sys
import tempfile


ORIGINAL_MEDIA_SUFFIXES = frozenset({".zip", ".adf", ".msa", ".st", ".stx"})


def media_snapshot(directory: Path, *, original_media_only: bool = False) -> dict[Path, str]:
    """Return a content snapshot without trusting timestamps or filenames.

    A user media directory can legitimately contain unrelated downloads.  The
    preservation assertion therefore snapshots only recognised container
    classes when checking supplied media, while test-pack fixtures retain a
    complete snapshot.
    """
    snapshot: dict[Path, str] = {}
    for path in sorted(directory.rglob("*")):
        relative = path.relative_to(directory)
        if path.is_dir():
            if original_media_only:
                continue
            snapshot[relative] = "directory"
            continue
        if not path.is_file():
            if original_media_only:
                continue
            snapshot[relative] = "other"
            continue
        if original_media_only and path.suffix.lower() not in ORIGINAL_MEDIA_SUFFIXES:
            continue
        digest = hashlib.sha256()
        with path.open("rb") as source:
            for block in iter(lambda: source.read(1024 * 1024), b""):
                digest.update(block)
        snapshot[relative] = f"file:{digest.hexdigest()}"
    return snapshot


def snapshot_difference(before: dict[Path, str], after: dict[Path, str]) -> str:
    """Format a bounded, actionable difference for an immutability failure."""
    changed = sorted(path for path in before.keys() | after.keys() if before.get(path) != after.get(path))
    return ", ".join(
        f"{path}: {before.get(path, '<missing>')} -> {after.get(path, '<missing>')}"
        for path in changed[:8]
    ) or "no supported-media content difference"


def write_reference_trace(
    directory: Path,
    source_archive: Path,
    source_sha256: str,
    game: str,
    platform: str,
    adapter: str,
    events: str,
    *,
    source_media_sha256: str | None = None,
    source_stage_sha256: str | None = None,
) -> Path:
    """Create a temporary, provenance-bound CLI trace-validation input.

    The event records below exercise only the declared external grammar. They
    are not game data, a captured session, or an emulation/replay request;
    the source identity and size always come from a supplied original archive.
    """
    events_path = directory / f"{adapter}-events.eontrace"
    events_path.write_text(events, encoding="ascii")
    event_bytes = events_path.read_bytes()
    fields = [
        ("format", "project-eon-reference-trace-v4" if adapter.endswith("-v4")
            else "project-eon-reference-trace-v3" if adapter.endswith("-v3")
            else "project-eon-reference-trace-v2"),
        ("adapter", adapter),
        ("event_file", events_path.name),
        ("event_size", str(len(event_bytes))),
        ("event_sha256", hashlib.sha256(event_bytes).hexdigest()),
        ("game", game),
        ("platform", platform),
        ("language", "en"),
        ("source_release_sha256", source_sha256),
        ("source_release_size", str(source_archive.stat().st_size)),
    ]
    if source_media_sha256 is not None and source_stage_sha256 is not None:
        fields.extend((
            ("source_media_sha256", source_media_sha256),
            ("source_stage_sha256", source_stage_sha256),
        ))
    zero_sha256 = "0" * 64
    fields.extend((
        ("capture_start_utc", "2026-08-29T00:00:00Z"),
        ("capture_end_utc", "2026-08-29T00:00:01Z"),
        ("emulator_name", "project-eon-cli-test"),
        ("emulator_version", "1"),
        ("emulator_sha256", zero_sha256),
        ("config_sha256", zero_sha256),
        ("command_tail_sha256", zero_sha256),
        ("input_timeline_sha256", zero_sha256),
    ))
    manifest_path = directory / f"{adapter}-manifest.eontrace"
    manifest_path.write_text(
        "".join(f"{key}\t{value}\n" for key, value in fields), encoding="ascii"
    )
    return manifest_path


def deuteros_title_bridge_events() -> str:
    """Return a grammar fixture for the exact v3 bridge contract.

    The records are external-observation syntax only. They contain no Amiga
    media, screenshot, audio payload, input injection, or replay state.
    """
    return (
        "event\t1 10 exec-return site=0x00040450 exec_base_address=0x00000004 vector=-0x0096 result_d0=0x00000000 result_sr=0x2000\n"
        "event\t2 20 exec-return site=0x00040450 exec_base_address=0x00000004 vector=-0x009c result_d0=0x00000000 result_sr=0x2000\n"
        "event\t3 30 open-library-return site=0x0001ed80 name_address=0x0001ed02 exec_base_address=0x00000004 vector=-0x0228 result_d0=0x00012fec result_sr=0x2000\n"
        "event\t4 40 graphics-call site=0x0004069a graphics_base_address=0x00012fec vector=-0x00c0\n"
        "event\t5 50 custom-register-call site=0x0004046c base=0x00dff000 offset=0x0040 value=0x7fff\n"
        "event\t6 60 custom-register-return site=0x0004046c base=0x00dff000 offset=0x0040 value=0x7fff result_d0=0x00000000 result_sr=0x2000\n"
        "event\t7 70 graphics-return site=0x0004069a graphics_base_address=0x00012fec vector=-0x00c0 result_d0=0x00000000 result_sr=0x2000\n"
        "event\t8 80 callback-registration-return site=0x0001ef74 callback=0x0001f056 exec_base_address=0x00000004 vector=-0x01ce result_d0=0x00000000 result_sr=0x2000\n"
        "event\t9 90 queue-snapshot phase=pre queue_address=0x0001eec0 queue_bytes=0000000000000000000000000000000000000000 pending_address=0x0001eed6 pending_word=0x0000 source_table_address=0x0001ee20 source_table_size=160 source_table_sha256=2f00ffdf05ab28379e97e91e98fa764e45769d7ea55363846543becf7552e265\n"
        "event\t10 100 callback-entry site=0x0001f056 incoming_a0=0x00001000 frame_04_0d=00000000000000000000\n"
        "event\t11 110 queue-snapshot phase=post queue_address=0x0001eec0 queue_bytes=0000000000000000000000000000000000000000 pending_address=0x0001eed6 pending_word=0x0000 source_table_address=0x0001ee20 source_table_size=160 source_table_sha256=2f00ffdf05ab28379e97e91e98fa764e45769d7ea55363846543becf7552e265\n"
        "event\t12 120 selector-entry site=0x0001fe7a incoming_d0=0x00000000\n"
        "event\t13 130 local-call call_site=0x0001fe84 callee=0x0001fea8 return_pc=0x0001fe88\n"
        "event\t14 140 local-return call_site=0x0001fe84 callee=0x0001fea8 return_pc=0x0001fe88 result_d0=0x00000000 result_sr=0x2000\n"
        "event\t15 150 local-call call_site=0x0001fe92 callee=0x0001fea8 return_pc=0x0001fe96\n"
        "event\t16 160 local-return call_site=0x0001fe92 callee=0x0001fea8 return_pc=0x0001fe96 result_d0=0x00000000 result_sr=0x2000\n"
        "event\t17 170 dispatch-snapshot phase=pre site=0x0001fbe6 cell_1f98c=0x00 cell_1f98e=0x00 cell_1f99c=0x00000000 cell_1f974=0x00000000 cell_1f970=0x00000000 cell_1f96c=0x00000000 cell_1f994=0x00000000 cell_1f998=0x00000000\n"
        "event\t18 180 dispatch-snapshot phase=post site=0x0001fbe6 cell_1f98c=0x00 cell_1f98e=0x00 cell_1f99c=0x00000000 cell_1f974=0x00000000 cell_1f970=0x00000000 cell_1f96c=0x00000000 cell_1f994=0x00000000 cell_1f998=0x00000000\n"
    )


def main() -> int:
    if len(sys.argv) != 3:
        raise SystemExit("usage: cli_launch_test.py <project-eon> <real-data-dir>")

    executable = Path(sys.argv[1])
    data_directory = Path(sys.argv[2])
    if not executable.is_file():
        raise SystemExit(f"Project Eon executable not found: {executable}")
    if not data_directory.is_dir():
        raise SystemExit(f"Real data directory not found: {data_directory}")

    test_tmpdir = os.environ.get("EON_TEST_TMPDIR")
    if not test_tmpdir:
        raise SystemExit("EON_TEST_TMPDIR is required; tests must not use the system temporary directory")
    temporary_root = Path(test_tmpdir)
    temporary_root.mkdir(parents=True, exist_ok=True)

    before = media_snapshot(data_directory, original_media_only=True)
    environment = os.environ | {"SDL_VIDEODRIVER": "dummy"}
    # Keep the explicit spelling usable for scripts and package integrations;
    # it must select the same original data directory without creating or
    # transforming anything.
    data_dir_inspection = subprocess.run(
        (str(executable), "--data-dir", str(data_directory), "--inspect"),
        env=environment, check=False, capture_output=True, text=True,
    )
    if data_dir_inspection.returncode != 0 or "VERIFIED  " not in data_dir_inspection.stdout:
        raise SystemExit(
            "--data-dir did not inspect the supplied original media:\n"
            f"{data_dir_inspection.stderr}"
        )
    # This all-platform test intentionally requires the complete canonical
    # archive corpus. An installed direct-media set has a different identity
    # shape and belongs to cli_direct_directory_test.py, not this test.
    corpus_inspection = subprocess.run(
        (str(executable), "--data", str(data_directory), "--inspect-json"),
        env=environment, check=False, capture_output=True, text=True,
    )
    try:
        corpus_payload = json.loads(corpus_inspection.stdout)
    except json.JSONDecodeError as error:
        raise SystemExit(f"archive-corpus preflight did not emit JSON: {error}") from error
    expected_corpus_releases = {
        ("Millennium 2.2", "DOS", "en"),
        ("Millennium 2.2", "DOS", "es"),
        ("Millennium 2.2", "Amiga", "en"),
        ("Millennium 2.2", "Atari ST", "en"),
        ("Deuteros", "Amiga", "en"),
        ("Deuteros", "Atari ST", "en"),
    }
    observed_corpus_releases = {
        (release.get("game"), release.get("platform"), release.get("language"))
        for release in corpus_payload.get("releases", [])
    }
    if (corpus_inspection.returncode != 0
            or corpus_payload.get("schema") != "project-eon.inspect/v1"
            or observed_corpus_releases != expected_corpus_releases):
        raise SystemExit(
            "EON_REAL_DATA_DIR requires the complete six-release canonical archive corpus; "
            "use EON_DIRECT_DATA_DIR for recognised installed direct media.\n"
            f"expected {sorted(expected_corpus_releases)}, got {sorted(observed_corpus_releases)}"
        )
    # Both hash-recognised DOS editions expose their own immutable 2200AD4.BIN
    # topology and original-text provenance. The report must not omit Spanish
    # diagnostics or silently reuse the English offsets/hashes.
    required_static_data_reports = (
        "2200AD4.BIN static text: 435 original pointers to 434 raw records; source SHA-256 "
        "1919e5776616ca0ec8b70232c82c152451c4c917791cd84a2eade97c8a47e47d",
        "Spanish 2200AD4.BIN static text: 435 original pointers to 434 raw records; source SHA-256 "
        "8865ba3c9e6ed535c7f9a97a725629d850bc1a765666d40db6a1b81e3e181e31",
        "Control-text provenance: pointers 0x12a7/0x12ac",
        "Spanish control-text provenance: pointers 0x1351/0x1359",
    )
    if any(report not in data_dir_inspection.stdout for report in required_static_data_reports):
        raise SystemExit(
            "DOS static-data inspection did not retain distinct English/Spanish provenance:\n"
            f"{data_dir_inspection.stdout}"
        )

    # The Modern PNG profile is bound to the English title resource. A
    # process-level parse failure proves a Spanish FAT12 selection cannot
    # quietly admit English renderer art before any SDL or media operation.
    cross_edition_pack = subprocess.run(
        (str(executable), "--data", str(data_directory), "--game", "millennium",
            "--platform", "dos", "--release-language", "es", "--presentation", "modern",
            "--modern-pack", "pack.eonmodern"),
        env=environment, check=False, capture_output=True, text=True,
    )
    if (cross_edition_pack.returncode != 2
            or "no cross-edition art fallback is permitted" not in cross_edition_pack.stderr):
        raise SystemExit(
            "Spanish media selection did not reject an English-only Modern pack:\n"
            f"{cross_edition_pack.stdout}\n{cross_edition_pack.stderr}"
        )
    if "INSPECTION  read-only provenance scan; original media stays in place" not in data_dir_inspection.stdout:
        raise SystemExit("--inspect did not identify its read-only provenance boundary")
    if "RECOVERY MAP  " not in data_dir_inspection.stdout:
        raise SystemExit("--inspect did not report the hash-bound recovery map")
    if "State-1 display-service boundary: Disk 1 +0x9d800" not in data_dir_inspection.stdout:
        raise SystemExit("--inspect did not report the Deuteros Atari display-service boundary")

    # The JSON report is a complete release-exact preservation index, not a
    # filename inventory. Check every supplied release, including bootstrap
    # platforms, so a future serializer cannot quietly omit a CPU, source
    # boundary, or documentation link while its human-oriented sibling still
    # appears plausible.
    inspect_json = subprocess.run(
        (str(executable), "--data", str(data_directory), "--inspect-json"),
        env=environment, check=False, capture_output=True, text=True,
    )
    try:
        inspect_payload = json.loads(inspect_json.stdout)
    except json.JSONDecodeError as error:
        raise SystemExit(f"--inspect-json did not emit JSON: {error}") from error
    expected_json_coverage = {
        ("Millennium 2.2", "DOS", "en"): "RECOVERED STARTUP",
        ("Millennium 2.2", "DOS", "es"): "BOOTSTRAP ONLY",
        ("Millennium 2.2", "Amiga", "en"): "BOOTSTRAP ONLY",
        ("Millennium 2.2", "Atari ST", "en"): "BOOTSTRAP ONLY",
        ("Deuteros", "Amiga", "en"): "RECOVERED OPENING",
        ("Deuteros", "Atari ST", "en"): "BOOTSTRAP ONLY",
    }
    reported_json_coverage = {
        (release["game"], release["platform"], release["language"]): release["coverage"]
        for release in inspect_payload.get("releases", [])
    }
    if (inspect_json.returncode != 0
            or inspect_payload.get("schema") != "project-eon.inspect/v1"
            or reported_json_coverage != expected_json_coverage):
        raise SystemExit(
            "--inspect-json did not retain the complete release/coverage inventory:\n"
            f"{inspect_json.stdout}\n{inspect_json.stderr}"
        )
    for release in inspect_payload["releases"]:
        for boundary in release["recovery_boundaries"]:
            required_boundary_fields = {
                "id", "profile", "cpu", "source_address", "evidence_level",
                "runtime_status", "documentation_anchor",
            }
            if (not required_boundary_fields <= boundary.keys()
                    or not boundary["cpu"]
                    or not boundary["documentation_anchor"].startswith("PRESERVATION.md#")):
                raise SystemExit(f"--inspect-json recovery boundary lost preservation fields: {boundary}")
        for function in release["function_map"]:
            required_function_fields = {
                "id", "profile", "cpu", "source_asset_sha256", "source_span_sha256", "source_offset",
                "runtime_address", "evidence_level", "uncertainty", "runtime_status",
                "documentation_anchor", "address_space",
            }
            if (not required_function_fields <= function.keys()
                    or not function["documentation_anchor"].startswith("PRESERVATION.md#")
                    or function["address_space"] not in {"runtime", "image-relative-unrelocated"}
                    or (function["address_space"] == "runtime"
                        and not function["runtime_address"].startswith("$"))
                    or (function["address_space"] == "image-relative-unrelocated"
                        and not function["runtime_address"].startswith("+0x"))):
                raise SystemExit(f"--inspect-json function lost preservation fields: {function}")
    if any(token in inspect_json.stdout for token in (str(data_directory), "Hämtningar", "Downloads")):
        raise SystemExit("--inspect-json exposed a local original-media path")

    launch_check_json = subprocess.run(
        (str(executable), "--data", str(data_directory), "--game", "millennium",
            "--platform", "dos", "--presentation", "original", "--launch-check-json"),
        env=environment, check=False, capture_output=True, text=True,
    )
    try:
        launch_check_payload = json.loads(launch_check_json.stdout)
    except json.JSONDecodeError as error:
        raise SystemExit(f"--launch-check-json did not emit JSON: {error}") from error
    if (launch_check_json.returncode != 0
            or launch_check_payload != {
                "schema": "project-eon.launch-check/v1",
                "release": {
                    "game": "Millennium 2.2", "platform": "DOS", "language": "en",
                    "sha256": "e6e7044b25877fdf8b10d16d2f395886d9957953144ae15ca630cda9cab2a123",
                },
                "presentation": "original",
                "display": {"resolution": "1280x720", "aspect": "original"},
                "coverage": "RECOVERED STARTUP",
                "runtime_admission": "READY",
                "runtime_session": {
                    "kind": "MILLENNIUM DOS TITLE",
                    "boundary": "RECOVERED PRESENTATION BOUNDARY",
                    "capabilities": {
                        "decoded_presentation": True,
                        "audio_observations": False,
                        "admitted_input": True,
                    },
                },
            }):
        raise SystemExit(
            "--launch-check-json did not report the exact admitted release without SDL:\n"
            f"{launch_check_json.stdout}\n{launch_check_json.stderr}"
        )

    # The fuller native diagnostic must cross the same hash-bound gate as the
    # compact launch check while retaining all declarative recovery provenance.
    # It remains a no-SDL, no-emulator report: paths and original bytes must
    # not become a convenient side channel merely because the function map is
    # useful to preservation tooling.
    runtime_diagnostics = subprocess.run(
        (str(executable), "--data", str(data_directory), "--game", "millennium",
            "--platform", "dos", "--presentation", "original",
            "--runtime-diagnostics-json"),
        env=environment, check=False, capture_output=True, text=True,
    )
    try:
        runtime_diagnostics_payload = json.loads(runtime_diagnostics.stdout)
    except json.JSONDecodeError as error:
        raise SystemExit(
            f"--runtime-diagnostics-json did not emit JSON: {error}"
        ) from error
    recovery = runtime_diagnostics_payload.get("recovery", {})
    if (runtime_diagnostics.returncode != 0
            or runtime_diagnostics_payload.get("schema")
                != "project-eon.runtime-diagnostics/v1"
            or runtime_diagnostics_payload.get("release") != launch_check_payload["release"]
            or runtime_diagnostics_payload.get("presentation") != "original"
            or runtime_diagnostics_payload.get("display") != launch_check_payload["display"]
            or runtime_diagnostics_payload.get("runtime_admission")
                != launch_check_payload["runtime_admission"]
            or runtime_diagnostics_payload.get("runtime_session")
                != launch_check_payload["runtime_session"]
            or runtime_diagnostics_payload.get("atari_bootstrap_checkpoint") is not None
            or recovery.get("coverage") != launch_check_payload["coverage"]
            or recovery.get("trace_admission") != "not-loaded"
            or not recovery.get("startup_boundary")
            or not recovery.get("boundaries")
            or not recovery.get("function_map")
            or any(token in runtime_diagnostics.stdout
                for token in (str(data_directory), "Hämtningar", "Downloads"))):
        raise SystemExit(
            "--runtime-diagnostics-json did not preserve the admitted native release boundary:\n"
            f"{runtime_diagnostics.stdout}\n{runtime_diagnostics.stderr}"
        )
    for function in recovery["function_map"]:
        required_function_fields = {
            "id", "profile", "cpu", "source_asset_sha256", "source_span_sha256",
            "source_offset", "runtime_address", "address_space", "evidence_level",
            "uncertainty", "runtime_status", "documentation_anchor",
        }
        if (not required_function_fields <= function.keys()
                or not function["documentation_anchor"].startswith("PRESERVATION.md#")):
            raise SystemExit(
                "--runtime-diagnostics-json function map lost preservation provenance: "
                f"{function}"
            )

    atari_runtime_diagnostics = subprocess.run(
        (str(executable), "--data", str(data_directory), "--game", "deuteros",
            "--platform", "atari-st", "--release-language", "en", "--release-sha256",
            "c6856d0a7ccda925289c60f0675e7aaed616f8a0289c74698e87e1ee11e6c653",
            "--runtime-diagnostics-json"),
        env=environment, check=False, capture_output=True, text=True,
    )
    try:
        atari_runtime_payload = json.loads(atari_runtime_diagnostics.stdout)
    except json.JSONDecodeError as error:
        raise SystemExit(
            f"Atari --runtime-diagnostics-json did not emit JSON: {error}"
        ) from error
    atari_checkpoint = atari_runtime_payload.get("atari_bootstrap_checkpoint")
    if (atari_runtime_diagnostics.returncode != 0
            or atari_runtime_payload.get("runtime_session", {}).get("kind")
                != "DEUTEROS ATARI ST BOOTSTRAP"
            or not isinstance(atari_checkpoint, dict)
            or atari_checkpoint.get("relocated_dispatcher_address") != "$1ec4"
            or atari_checkpoint.get("state1_raw_request_count") != 84
            or atari_checkpoint.get("state1_display_service") != {
                "branch_relative_offset": "+0x48000",
                "service_relative_offset": "+0x489c6",
                "xbios_selector": "$5",
            }
            or any(token in atari_runtime_diagnostics.stdout
                for token in (str(data_directory), "Hämtningar", "Downloads"))):
        raise SystemExit(
            "Atari runtime diagnostics lost the hash-bound display-service checkpoint:\n"
            f"{atari_runtime_diagnostics.stdout}\n{atari_runtime_diagnostics.stderr}"
        )

    # An explicit original-language selection narrows the release universe
    # before choosing a default outer hash.  Spanish is unique in this real
    # data set even though English remains the no-language default, so it
    # must start without an unnecessary hash flag and without selecting its
    # English sibling first.
    spanish_language_launch = subprocess.run(
        (str(executable), "--data", str(data_directory), "--game", "millennium",
            "--platform", "dos", "--release-language", "es", "--launch-check-json"),
        env=environment, check=False, capture_output=True, text=True,
    )
    try:
        spanish_language_payload = json.loads(spanish_language_launch.stdout)
    except json.JSONDecodeError as error:
        raise SystemExit(
            f"Spanish language-scoped launch check did not emit JSON: {error}"
        ) from error
    if (spanish_language_launch.returncode != 0
            or spanish_language_payload.get("release", {}).get("language") != "es"
            or spanish_language_payload.get("release", {}).get("sha256")
                != "b40cc2f2c39cdb476b4a82bda7bffed1c80decdfb7fe41b1a38bf54343e0c0a4"):
        raise SystemExit(
            "an explicit Spanish original-language selection did not resolve its unique release:\n"
            f"{spanish_language_launch.stdout}\n{spanish_language_launch.stderr}"
        )

    # A platform card is not itself a release identity.  Exercise every
    # recognised archive through the directory scanner with both immutable
    # fields explicit.  This matters most for Millennium DOS: English and
    # Spanish are simultaneously present, and a launch request must not use
    # the English default, scan order, or a sibling language after the user
    # selected an exact outer-container hash.  The expected session facts are
    # deliberately narrow admission metadata, never an assertion about game
    # behavior, input semantics beyond the declared boundary, or frame parity.
    exact_release_contracts = (
        ("millennium", "dos", "en",
            "e6e7044b25877fdf8b10d16d2f395886d9957953144ae15ca630cda9cab2a123",
            "Millennium 2.2", "DOS", "RECOVERED STARTUP", "MILLENNIUM DOS TITLE",
            "RECOVERED PRESENTATION BOUNDARY",
            {"decoded_presentation": True, "audio_observations": False, "admitted_input": True}),
        ("millennium", "dos", "es",
            "b40cc2f2c39cdb476b4a82bda7bffed1c80decdfb7fe41b1a38bf54343e0c0a4",
            "Millennium 2.2", "DOS", "BOOTSTRAP ONLY", "MILLENNIUM DOS TITLE",
            "RECOVERED PRESENTATION BOUNDARY",
            {"decoded_presentation": True, "audio_observations": False, "admitted_input": True}),
        ("millennium", "amiga", "en",
            "2e27d7aeb8b8b7f2a75eda45b456ab42775a706aa85516c85e61ce94ec9eb400",
            "Millennium 2.2", "Amiga", "BOOTSTRAP ONLY", "MILLENNIUM AMIGA BOOTSTRAP",
            "BOOTSTRAP BOUNDARY",
            {"decoded_presentation": False, "audio_observations": False, "admitted_input": False}),
        ("millennium", "atari-st", "en",
            "ba1174123a0531abeab5788f4ac87a3c2500696bf1c87a7efd209441b3ebdf01",
            "Millennium 2.2", "Atari ST", "BOOTSTRAP ONLY", "MILLENNIUM ATARI ST BOOTSTRAP",
            "BOOTSTRAP BOUNDARY",
            {"decoded_presentation": False, "audio_observations": False, "admitted_input": False}),
        ("deuteros", "amiga", "en",
            "f4dc8dd1c27c5d389837783becd9b95ab09b78baf40e94e39e2b7e590e470e04",
            "Deuteros", "Amiga", "RECOVERED OPENING", "DEUTEROS AMIGA OPENING",
            "RECOVERED PRESENTATION BOUNDARY",
            {"decoded_presentation": True, "audio_observations": True, "admitted_input": True}),
        ("deuteros", "atari-st", "en",
            "c6856d0a7ccda925289c60f0675e7aaed616f8a0289c74698e87e1ee11e6c653",
            "Deuteros", "Atari ST", "BOOTSTRAP ONLY", "DEUTEROS ATARI ST BOOTSTRAP",
            "BOOTSTRAP BOUNDARY",
            {"decoded_presentation": False, "audio_observations": False, "admitted_input": False}),
    )
    for (game, platform, language, sha256, display_game, display_platform,
            coverage, session_kind, session_boundary, capabilities) in exact_release_contracts:
        exact_launch = subprocess.run(
            (str(executable), "--data", str(data_directory), "--game", game,
                "--platform", platform, "--release-language", language,
                "--release-sha256", sha256, "--presentation", "original",
                "--launch-check-json"),
            env=environment, check=False, capture_output=True, text=True,
        )
        try:
            exact_payload = json.loads(exact_launch.stdout)
        except json.JSONDecodeError as error:
            raise SystemExit(
                f"{game}/{platform}/{language} exact launch check did not emit JSON: {error}"
            ) from error
        expected_payload = {
            "schema": "project-eon.launch-check/v1",
            "release": {
                "game": display_game, "platform": display_platform,
                "language": language, "sha256": sha256,
            },
            "presentation": "original",
            "display": {"resolution": "1280x720", "aspect": "original"},
            "coverage": coverage,
            "runtime_admission": "READY",
            "runtime_session": {
                "kind": session_kind,
                "boundary": session_boundary,
                "capabilities": capabilities,
            },
        }
        if exact_launch.returncode != 0 or exact_payload != expected_payload:
            raise SystemExit(
                f"{game}/{platform}/{language} did not preserve its exact launch identity:\n"
                f"{exact_launch.stdout}\n{exact_launch.stderr}"
            )

        # Modern is a renderer/input/accessibility envelope over this exact
        # Original admission. Repeat every supplied platform identity, not
        # just the primary Millennium DOS route, and permit only the explicit
        # presentation field to differ before an SDL loop exists.
        modern_exact_launch = subprocess.run(
            (str(executable), "--data", str(data_directory), "--game", game,
                "--platform", platform, "--release-language", language,
                "--release-sha256", sha256, "--presentation", "modern",
                "--launch-check-json"),
            env=environment, check=False, capture_output=True, text=True,
        )
        try:
            modern_exact_payload = json.loads(modern_exact_launch.stdout)
        except json.JSONDecodeError as error:
            raise SystemExit(
                f"{game}/{platform}/{language} Modern launch check did not emit JSON: {error}"
            ) from error
        modern_expected_payload = {**expected_payload, "presentation": "modern"}
        if modern_exact_launch.returncode != 0 or modern_exact_payload != modern_expected_payload:
            raise SystemExit(
                f"{game}/{platform}/{language} Modern launch changed its admitted session:\n"
                f"{modern_exact_launch.stdout}\n{modern_exact_launch.stderr}"
            )

    # Original and Modern must pass through the same release/session admission
    # gate. Modern is an explicitly labelled renderer choice, never a sibling
    # release, an implicit pack selection, or a changed game-state contract.
    modern_launch_check = subprocess.run(
        (str(executable), "--data", str(data_directory), "--game", "millennium",
            "--platform", "dos", "--presentation", "modern", "--launch-check-json"),
        env=environment, check=False, capture_output=True, text=True,
    )
    try:
        modern_launch_payload = json.loads(modern_launch_check.stdout)
    except json.JSONDecodeError as error:
        raise SystemExit(f"Modern launch check did not emit JSON: {error}") from error
    if (modern_launch_check.returncode != 0
            or modern_launch_payload.get("presentation") != "modern"
            or modern_launch_payload.get("release") != launch_check_payload["release"]
            or modern_launch_payload.get("runtime_session")
                != launch_check_payload["runtime_session"]):
        raise SystemExit(
            "Modern launch check did not retain the Original release/session identity:\n"
            f"{modern_launch_check.stdout}\n{modern_launch_check.stderr}"
        )

    # Resolution and aspect ratio are explicit renderer geometry. A widescreen
    # request must be visible in the diagnostic while preserving the same
    # original identity and recovered session in both presentation modes.
    for presentation in ("original", "modern"):
        display_launch = subprocess.run(
            (str(executable), "--data", str(data_directory), "--game", "millennium",
                "--platform", "dos", "--presentation", presentation,
                "--resolution", "1920x1080", "--aspect", "widescreen", "--launch-check-json"),
            env=environment, check=False, capture_output=True, text=True,
        )
        try:
            display_payload = json.loads(display_launch.stdout)
        except json.JSONDecodeError as error:
            raise SystemExit(f"{presentation} display launch check did not emit JSON: {error}") from error
        if (display_launch.returncode != 0
                or display_payload.get("presentation") != presentation
                or display_payload.get("display") != {"resolution": "1920x1080", "aspect": "widescreen"}
                or display_payload.get("release") != launch_check_payload["release"]
                or display_payload.get("runtime_session") != launch_check_payload["runtime_session"]):
            raise SystemExit(
                f"{presentation} display request changed the recovered launch session:\n"
                f"{display_launch.stdout}\n{display_launch.stderr}"
            )

    # A hash-language mismatch must fail before an SDL loop, rather than
    # normalize one field from a different recognised DOS release.  No output
    # JSON means callers cannot mistake a rejected candidate for a launch of
    # the requested original container.
    crossed_identity = subprocess.run(
        (str(executable), "--data", str(data_directory), "--game", "millennium",
            "--platform", "dos", "--release-language", "es", "--release-sha256",
            "e6e7044b25877fdf8b10d16d2f395886d9957953144ae15ca630cda9cab2a123",
            "--launch-check-json"),
        env=environment, check=False, capture_output=True, text=True,
    )
    if (crossed_identity.returncode != 4 or crossed_identity.stdout
            or "one exact verified original release" not in crossed_identity.stderr
            or "SDL_Init" in crossed_identity.stderr):
        raise SystemExit(
            "a crossed Millennium DOS language/hash launch identity was not rejected before SDL:\n"
            f"{crossed_identity.stdout}\n{crossed_identity.stderr}"
        )

    # External emulator mounting must never turn Eon's own bounded archive
    # inventory into a write path.  Exercise the explicit leaf-manifest mode
    # against real media; the final content snapshot below is the direct
    # regression assertion for both scanner and inventory reads.
    inventory_inspection = subprocess.run(
        (str(executable), "--data", str(data_directory), "--inspect", "--inventory",
            "--game", "deuteros", "--platform", "amiga"),
        env=environment, check=False, capture_output=True, text=True,
    )
    if (inventory_inspection.returncode != 0
            or "ARCHIVE INVENTORY  " not in inventory_inspection.stdout
            or "read in place only" not in inventory_inspection.stdout):
        raise SystemExit(
            "--inspect --inventory did not provide its bounded read-only preservation manifest:\n"
            f"{inventory_inspection.stdout}\n{inventory_inspection.stderr}"
        )
    reported_releases = {
        line.removeprefix("VERIFIED  ") for line in data_dir_inspection.stdout.splitlines()
        if line.startswith("VERIFIED  ")
    }
    expected_reported_releases = {
        "Millennium 2.2 / DOS / en",
        "Millennium 2.2 / DOS / es",
        "Millennium 2.2 / Amiga / en",
        "Millennium 2.2 / Atari ST / en",
        "Deuteros / Amiga / en",
        "Deuteros / Atari ST / en",
    }
    if reported_releases != expected_reported_releases:
        raise SystemExit(
            "full --inspect report did not cover exactly the supported releases:\n"
            f"expected {sorted(expected_reported_releases)}, got {sorted(reported_releases)}"
        )
    expected_platform_admission = {
        "PLATFORM ADMISSION  Millennium 2.2 / DOS / READY / RECOVERED STARTUP / 2 verified original languages; English default",
        "PLATFORM ADMISSION  Millennium 2.2 / Amiga / READY / BOOTSTRAP ONLY / 1 verified original language",
        "PLATFORM ADMISSION  Millennium 2.2 / Atari ST / READY / BOOTSTRAP ONLY / 1 verified original language",
        "PLATFORM ADMISSION  Deuteros / Amiga / READY / RECOVERED OPENING / 1 verified original language",
        "PLATFORM ADMISSION  Deuteros / Atari ST / READY / BOOTSTRAP ONLY / 1 verified original language",
    }
    reported_platform_admission = {
        line for line in data_dir_inspection.stdout.splitlines()
        if line.startswith("PLATFORM ADMISSION  ")
    }
    if reported_platform_admission != expected_platform_admission:
        raise SystemExit(
            "full --inspect report did not expose the exact launcher admission states:\n"
            f"expected {sorted(expected_platform_admission)}, got {sorted(reported_platform_admission)}"
        )

    targeted_inspection = subprocess.run(
        (str(executable), "--data", str(data_directory), "--inspect", "--game", "millennium",
            "--platform", "dos"),
        env=environment, check=False, capture_output=True, text=True,
    )
    targeted_lines = [line for line in targeted_inspection.stdout.splitlines()
        if line.startswith("VERIFIED  ")]
    if (targeted_inspection.returncode != 0 or not targeted_lines
            or any("Millennium 2.2 / DOS / " not in line for line in targeted_lines)):
        raise SystemExit(
            "targeted inspection did not report only the requested original release:\n"
            f"{targeted_inspection.stdout}\n{targeted_inspection.stderr}"
        )

    # This flag identifies the original release, not the launcher UI. Shell
    # integrations use --inspect to discover exact media, so Spanish must not
    # list a sibling English release as an implicit fallback.
    spanish_inspection = subprocess.run(
        (str(executable), "--data", str(data_directory), "--inspect", "--game", "millennium",
            "--platform", "dos", "--release-language", "es"),
        env=environment, check=False, capture_output=True, text=True,
    )
    spanish_lines = [line for line in spanish_inspection.stdout.splitlines()
        if line.startswith("VERIFIED  ")]
    if (spanish_inspection.returncode != 0
            or spanish_lines != ["VERIFIED  Millennium 2.2 / DOS / es"]):
        raise SystemExit(
            "release-language inspection did not select exactly the Spanish original:\n"
            f"{spanish_inspection.stdout}\n{spanish_inspection.stderr}"
        )
    unavailable_language_inspection = subprocess.run(
        (str(executable), "--data", str(data_directory), "--inspect", "--game", "deuteros",
            "--platform", "amiga", "--release-language", "es"),
        env=environment, check=False, capture_output=True, text=True,
    )
    if (unavailable_language_inspection.returncode != 5
            or "No recognised original release matches the requested inspection filters."
                not in unavailable_language_inspection.stderr
            or "VERIFIED  " in unavailable_language_inspection.stdout):
        raise SystemExit(
            "unavailable release-language inspection silently selected another original:\n"
            f"{unavailable_language_inspection.stdout}\n{unavailable_language_inspection.stderr}"
        )

    source_match = re.search(
        r"VERIFIED  Millennium 2\.2 / DOS / [^\n]+\n\s*([0-9a-f]{64})",
        targeted_inspection.stdout,
    )
    if source_match is None:
        raise SystemExit("targeted inspection did not print its hash-bound DOS source identity")

    # Exercise every v2 adapter through the public CLI with its exact original
    # source archive. This covers the generic and stage-pinned manifest
    # variants together with adapter-specific reporting; no trace is replayed.
    archive_by_sha256 = {
        digest.removeprefix("file:"): data_directory / relative
        for relative, digest in before.items()
        if digest.startswith("file:") and relative.suffix.lower() == ".zip"
    }
    atari_archive = archive_by_sha256.get(
        "ba1174123a0531abeab5788f4ac87a3c2500696bf1c87a7efd209441b3ebdf01"
    )
    if atari_archive is None:
        raise SystemExit("The real-media fixture lacks the verified Equinox Millennium Atari archive")
    # The config listing is a read-only, file-relative disassembly candidate.
    # It must reproduce the committed external-report identity without
    # selecting either competing Fread load address as a runtime base.
    with tempfile.TemporaryDirectory(dir=temporary_root) as analysis_root:
        report = Path(analysis_root) / "mill22a-linear.md"
        analysis = subprocess.run(
            (sys.executable, str(Path(__file__).resolve().parents[1] / "tools" / "analyze_atari_st_config.py"),
             "--archive", str(atari_archive),
             "--nested-member", "Millenium 2.2 (1989)(Electric Dreams)[cr Equinox][one disk].zip",
             "--disk-member", "Millenium 2.2 (1989)(Electric Dreams)[cr Equinox][one disk].st",
             "--disk-sha256", "3f090651ee586cf32a3f37f41b748ba36c78799e7bf761b66ddca2352579afe7",
             "--file-sha256", "74d7d630779fd811aedcdbe31b14e54198eb9ffd673df512dd70b6165c4a37b6",
             "--output", str(report)),
            env=environment, check=False, capture_output=True, text=True,
        )
        if analysis.returncode != 0 or not report.is_file():
            raise SystemExit(f"MILL22A.INF candidate disassembly failed:\n{analysis.stderr}")
        report_bytes = report.read_bytes()
        if (hashlib.sha256(report_bytes).hexdigest()
                != "0ffc140d1bcde59f2a715a60da04d3493c8c278a1a823023c5e585658cbe415b"
                or report_bytes.count(b"\n") != 2493
                or b"file-image-relative, unrelocated" not in report_bytes):
            raise SystemExit("MILL22A.INF report did not retain its file-relative preservation identity")
    dos_archive = archive_by_sha256.get(
        "e6e7044b25877fdf8b10d16d2f395886d9957953144ae15ca630cda9cab2a123"
    )
    if dos_archive is None:
        raise SystemExit("The real-media fixture lacks the verified English Millennium DOS archive")
    # Save inspection can consume the original archive directly. This is the
    # practical preservation route when a user has only their supplied ZIP:
    # the runtime verifies the complete archive, reads the known leaf in
    # memory, and must not materialize a commercial save beside it.
    archive_save_report = subprocess.run(
        (str(executable), "--inspect-save", str(dos_archive)),
        env=environment, check=False, capture_output=True, text=True,
    )
    after_archive_save = media_snapshot(data_directory, original_media_only=True)
    if (archive_save_report.returncode != 0
            or "verified English Millennium DOS archive" not in archive_save_report.stdout
            or "[0] +00=0x8100 +04=0x0 +06=0x0 +08=0x2292" not in archive_save_report.stdout
            or "[37] +00=0x8600 +04=0x0 +06=0x0 +08=0x0" not in archive_save_report.stdout
            or after_archive_save != before):
        raise SystemExit(
            "archive-backed DOS save inspection did not remain hash-bound and read-only:\n"
            f"{archive_save_report.stdout}\n{archive_save_report.stderr}\n"
            f"media difference: {snapshot_difference(before, after_archive_save)}"
        )
    trace_specs = (
        (
            "millennium", "dos", "millennium-dos-en-gx-startup-v2",
            "e6e7044b25877fdf8b10d16d2f395886d9957953144ae15ca630cda9cab2a123",
            "event\t1 10 private-return image=2200ad.exe pc=0x0129 int=0x91 ax=0x0000\n"
            "event\t2 20 mode-read image=2200ad.exe pc=0xd349 address=0xda05 value=0x03\n"
            "event\t3 30 adapter-return image=2200gx.exe pc=0x00ed op=retf return_pc=0xd376\n"
            "event\t4 40 local-return image=2200ad.exe call_pc=0xd376 return_pc=0xd379\n"
            "event\t5 50 local-return image=2200ad.exe call_pc=0xd379 return_pc=0xd37c\n"
            "event\t6 60 local-return image=2200ad.exe call_pc=0xd37c return_pc=0xd37f\n"
            "event\t7 70 local-return image=2200ad.exe call_pc=0xd37f return_pc=0xd382\n"
            "event\t8 80 local-return image=2200ad.exe call_pc=0xd382 return_pc=0xd385\n"
            "event\t9 90 local-return image=2200ad.exe call_pc=0xd385 return_pc=0xd388\n"
            "event\t10 100 mode-read image=2200ad.exe pc=0xd388 address=0xda05 value=0x01\n",
            None, None,
            "1 private-return, 2 mode-read, 1 adapter-return, 6 local-return observations; call-free transient overlay admitted through second private-INT boundary)",
        ),
        (
            "millennium", "dos", "millennium-dos-en-startup-v1",
            "e6e7044b25877fdf8b10d16d2f395886d9957953144ae15ca630cda9cab2a123",
            "event\t1 10 interrupt image=mill.com pc=0x0209 int=0x21 ax=0x2591 dx=0x0000\n"
            "event\t2 20 file image=mill.com pc=0x02cf op=driver-load path=mcga.bin\n"
            "event\t3 30 exec image=mill.com pc=0x0337 int=0x21 ax=0x4b00 path=titles.exe\n"
            "event\t4 40 interrupt image=titles.exe pc=0x0127 int=0x91 ax=0x0000 es=cs bx=0x1ac4\n",
            None, None, "2 interrupt, 1 file, 1 EXEC observations; diagnostics only)",
        ),
        # This is the exact five-record, externally captured title-init
        # observation documented in MILLENNIUM_DOS_CAPTURE.md.  The test
        # rehashes the supplied archive and exercises the public CLI only;
        # it does not replay either private-vector return or use it as game
        # input, state, rendering, or a substitute for original media.
        (
            "millennium", "dos", "millennium-dos-en-title-init-v2",
            "e6e7044b25877fdf8b10d16d2f395886d9957953144ae15ca630cda9cab2a123",
            "event\t1 1 file image=mill.com pc=0x02cf op=driver-load path=mcga.bin\n"
            "event\t2 2 interrupt image=mill.com pc=0x0209 int=0x21 ax=0x2591 dx=0x0000\n"
            "event\t3 3 interrupt image=titles.exe pc=0x0127 int=0x91 ax=0x0000 es=cs bx=0x1ac4\n"
            "event\t4 4 private-return image=titles.exe pc=0x0129 int=0x91 ax=0x0101\n"
            "event\t5 5 private-return image=titles.exe pc=0x0129 int=0x91 ax=0x0000\n",
            None, None, "2 interrupt, 1 file, 2 private-return observations; diagnostics only)",
        ),
        (
            "millennium", "amiga", "millennium-amiga-en-defjam-bootstrap-v1",
            "2e27d7aeb8b8b7f2a75eda45b456ab42775a706aa85516c85e61ce94ec9eb400",
            "event\t1 10 cpu image=bootstrap-loader pc=0x702e4 op=jsr-indirect a3=0x41000\n"
            "event\t2 20 cpu image=bootstrap-loader pc=0x70320 op=jmp-indirect a3=0x68000 d6=0xa8d398fb\n",
            None, None, "2 CPU handoff observations; diagnostics only)",
        ),
        (
            "deuteros", "atari-st", "deuteros-atari-st-boot-v1",
            "c6856d0a7ccda925289c60f0675e7aaed616f8a0289c74698e87e1ee11e6c653",
            "event\t1 10 trap pc=0x00001edc incoming_a7=0x00001000 incoming_sr=0x2700 selector=0x0026 callback=0x00001fa6 return_pc=0x00001ede return_a7=0x0000100c return_sr=0x2000 return_d0=0x00000000\n"
            "event\t2 20 callback entry_pc=0x00001fa6 incoming_a7=0x00001000 stack_longword=0x00001ede outgoing_a7=0x0007b000 return_pc=0x00001ede return_a7=0x0007affc return_sr=0x2000 return_d0=0x00001ede\n"
            "event\t3 30 state ram_25f4=0x00071100 ram_25f4_provenance=aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa ram_25fc=0x00000001 ram_25fc_provenance=bbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbb branch_pc=0x00001ef2 state_word=0x0001\n"
            "event\t4 40 table base=0x00001eac shifted_index=0x0002 target_a1=0x00001f50 entry_pc=0x00001f50 return_pc=0x00001f08 return_d1=0x00000000 return_d2=0x00000000\n"
            "event\t5 50 frame site=0x00001e9c input_frame=0008 result_frame=00000000\n"
            "event\t6 60 raw-reader entry_pc=0x00001e60 trap_pc=0x00001e9c call_a7=0x00002000 return_pc=0x00001e9e return_a7=0x00002014 return_sr=0x2000 return_d0=0x00000000\n",
            "aba874134807360ccde0ff98d6b82a965f57dcae5800b5b54394472522ef5bee",
            "2489256511e857a4a1b20d413b4f869edaae1f4df7f62ce869e324cad40e81d7",
            "1 TRAP, 1 callback, 1 frame, 1 state, 1 table, 1 raw-reader observations; diagnostics only)",
        ),
        (
            "deuteros", "amiga", "deuteros-amiga-en-title-stage-v1",
            "f4dc8dd1c27c5d389837783becd9b95ab09b78baf40e94e39e2b7e590e470e04",
            "event\t1 10 exec site=0x00040450 exec_base_address=0x00000004 vector=-0x0096 result_d0=0x00000000 result_sr=0x2000\n"
            "event\t2 20 open-library site=0x0001ed80 name_address=0x0001ed02 exec_base_address=0x00000004 vector=-0x0228 result_d0=0x00012fec result_sr=0x2000\n"
            "event\t3 30 graphics site=0x0004069a graphics_base_address=0x00012fec vector=-0x00c0 result_d0=0x00000000 result_sr=0x2000\n"
            "event\t4 40 custom-register site=0x0004046c base=0x00dff000 offset=0x0040 value=0x7fff result_d0=0x00000000 result_sr=0x2000\n"
            "event\t5 50 callback site=0x0001ef74 callback=0x0001f056 exec_base_address=0x00000004 vector=-0x01ce result_d0=0x00000000 result_sr=0x2000\n"
            "event\t6 60 callback site=0x0001f056 incoming_a0=0x00001000 result_d0=0x00000001 result_sr=0x2000\n",
            "6ea0cc68d3af37203a885032eddf7c28e839e6abb59d8c9cd3792f1308bdec38",
            "48d65260e9b5f5cbf8d8b3675a178c81b8764810b61a6a2539a56dcb40a8de03",
            "1 Exec, 1 OpenLibrary, 1 graphics, 1 custom-register, 2 callback observations; diagnostics only)",
        ),
        (
            "deuteros", "amiga", "deuteros-amiga-en-title-bridge-v3",
            "f4dc8dd1c27c5d389837783becd9b95ab09b78baf40e94e39e2b7e590e470e04",
            deuteros_title_bridge_events(),
            "6ea0cc68d3af37203a885032eddf7c28e839e6abb59d8c9cd3792f1308bdec38",
            "48d65260e9b5f5cbf8d8b3675a178c81b8764810b61a6a2539a56dcb40a8de03",
            "2 Exec return, 1 OpenLibrary return, 1/1 graphics call/return, 1/1 custom-register call/return, 2 queue snapshot, 1 callback entry, 2 dispatch snapshot observations; diagnostics only)",
        ),
        (
            "deuteros", "amiga", "deuteros-amiga-en-title-display-v4",
            "f4dc8dd1c27c5d389837783becd9b95ab09b78baf40e94e39e2b7e590e470e04",
            deuteros_title_bridge_events()
            + "event\t19 190 display-layout site=0x0001eda6 base_source_address=0x00012ff4 base_destination_a=0x0001f168 base_destination_b=0x0001f164 display_base=0x0000ab00 display_list=0x00000420 copper_list_sha256=cf827847c13dbeafeea72c86f2c4fb90a6d717bf548f0914b2f203abb94293f6\n"
            + "event\t20 200 bitplane-layout site=0x0001f182 base_pointer_address=0x0001f168 bitplane_count=0x0004 plane0=0x0000b5f0 plane1=0x0000d530 plane2=0x0000f470 plane3=0x000113b0 plane_stride=0x1f40 bplcon0=0x4200 bpl1mod=0x0000 bpl2mod=0x0000 ddfstrt=0x0038 ddfstop=0x00d0 width_pixels=0x0140 height_lines=0x00c8 bytes_per_row=0x0028 modulo=0x0000\n"
            + "event\t21 210 palette-checkpoint site=0x0001eda6 source_address=0x0001ed24 destination_address=0x00012ecc word_count=0x0014 rgb4_sha256=5903a1c83619d7667c04ac1f3c923dfaa3a1ce0d090d6fd95109616a9b506a55 rgba_palette_format=rgba8888-rgb4-expanded-nibbles rgba_palette_sha256=0000000000000000000000000000000000000000000000000000000000000000\n"
            + "event\t22 220 input-checkpoint callback_site=0x0001f056 selector_site=0x0001fe7a queue_sha256=0000000000000000000000000000000000000000000000000000000000000000 input_timeline_sha256=0000000000000000000000000000000000000000000000000000000000000000\n"
            + "event\t23 230 frame-checkpoint display_base=0x0000ab00 rgba_width=0x0140 rgba_height=0x00c8 rgba_format=rgba8888-row-major bitplanes_sha256=fad588ff5f6e0ec471cb4889987dab4a40c11d7da6e532564d48475149c68490 rgba_sha256=0000000000000000000000000000000000000000000000000000000000000000\n"
            + "event\t24 240 audio-checkpoint sample_rate=0x00002710 channels=0x02 sample_frames=0x00000001 pcm_format=s16le-interleaved pcm_sha256=0000000000000000000000000000000000000000000000000000000000000000\n",
            "6ea0cc68d3af37203a885032eddf7c28e839e6abb59d8c9cd3792f1308bdec38",
            "48d65260e9b5f5cbf8d8b3675a178c81b8764810b61a6a2539a56dcb40a8de03",
            "1 display-layout, 1 bitplane-layout, 1 palette, 1 input, 1 frame, 1 audio checkpoints; diagnostics only, no title replay)",
        ),
        (
            "deuteros", "amiga", "deuteros-amiga-en-main-copy-loop-v3",
            "f4dc8dd1c27c5d389837783becd9b95ab09b78baf40e94e39e2b7e590e470e04",
            "event\t1 10 main-copy-loop-pc pc=0x000210d4 opcode=0x51c8\n",
            "6ea0cc68d3af37203a885032eddf7c28e839e6abb59d8c9cd3792f1308bdec38",
            "a82c0d6a12e156e0832d632a6c40dd58713a00b611dbcba7289aa16b0969a0a6",
            "0 interrupt, 0 file, 0 EXEC observations; diagnostics only)",
        ),
    )
    trace_recovery_boundaries = {
        "millennium-dos-en-gx-startup-v2": (
            "millennium-dos-game-flow", "millennium-dos-gx-overlay",
        ),
        "millennium-dos-en-startup-v1": (
            "millennium-dos-launcher", "millennium-dos-title-flow", "millennium-dos-game-flow",
        ),
        "millennium-dos-en-title-init-v2": (
            "millennium-dos-launcher", "millennium-dos-title-flow",
        ),
        "millennium-amiga-en-defjam-bootstrap-v1": (
            "millennium-amiga-defjam-bootstrap", "millennium-amiga-shared-resident",
        ),
        "deuteros-atari-st-boot-v1": (
            "deuteros-atari-protected-boot", "deuteros-atari-first-stage",
        ),
        "deuteros-amiga-en-title-stage-v1": (
            "deuteros-amiga-main-stage", "deuteros-amiga-title-handoff",
        ),
        "deuteros-amiga-en-title-bridge-v3": (
            "deuteros-amiga-main-stage", "deuteros-amiga-title-handoff",
        ),
        "deuteros-amiga-en-title-display-v4": (
            "deuteros-amiga-main-stage", "deuteros-amiga-title-handoff",
        ),
        "deuteros-amiga-en-main-copy-loop-v3": ("deuteros-amiga-main-stage",),
    }
    with tempfile.TemporaryDirectory(dir=temporary_root) as temporary_trace_root:
        trace_root = Path(temporary_trace_root)
        for (game, platform, adapter, source_sha256, events, media_sha256,
             stage_sha256, expected_diagnostics) in trace_specs:
            source_archive = archive_by_sha256.get(source_sha256)
            if source_archive is None:
                raise SystemExit(f"The real-media fixture lacks {adapter}'s exact source archive")
            manifest = write_reference_trace(
                trace_root, source_archive, source_sha256, game, platform, adapter, events,
                source_media_sha256=media_sha256, source_stage_sha256=stage_sha256,
            )
            trace_report = subprocess.run(
                (str(executable), "--data", str(data_directory), "--game", game,
                    "--platform", platform, "--reference-trace", str(manifest)),
                env=environment, check=False, capture_output=True, text=True,
            )
            if (trace_report.returncode != 0
                    or "REFERENCE TRACE VERIFIED  provenance-only; no replay performed" not in trace_report.stdout
                    or ("capture fingerprints config=" + "0" * 64
                        + " command-tail=" + "0" * 64
                        + " input-timeline=" + "0" * 64) not in trace_report.stdout
                    or f"adapter {adapter} (" not in trace_report.stdout
                    or expected_diagnostics not in trace_report.stdout
                    or any(f"RECOVERY MAP {boundary} at " not in trace_report.stdout
                           for boundary in trace_recovery_boundaries[adapter])
                    or ((media_sha256 is not None)
                        and (f"source media {media_sha256}" not in trace_report.stdout
                             or f"source stage {stage_sha256}" not in trace_report.stdout))):
                raise SystemExit(
                    f"{adapter} was not admitted and reported through the public CLI:\n"
                    f"{trace_report.stdout}\n{trace_report.stderr}"
                )
            trace_json_report = subprocess.run(
                (str(executable), "--data", str(data_directory), "--game", game,
                    "--platform", platform, "--reference-trace", str(manifest),
                    "--reference-trace-json"),
                env=environment, check=False, capture_output=True, text=True,
            )
            try:
                trace_json = json.loads(trace_json_report.stdout)
            except json.JSONDecodeError as error:
                raise SystemExit(
                    f"{adapter} did not produce valid reference-trace diagnostics JSON:\n"
                    f"{trace_json_report.stdout}\n{trace_json_report.stderr}"
                ) from error
            if (trace_json_report.returncode != 0
                    or trace_json.get("schema") != "project-eon.reference-trace/v1"
                    or trace_json.get("release", {}).get("sha256") != source_sha256
                    or trace_json.get("adapter") != adapter
                    or trace_json.get("runtime_policy") != (
                        "transient-call-free-gx-startup"
                        if adapter == "millennium-dos-en-gx-startup-v2" else "diagnostics-only")
                    or trace_json.get("events", {}).get("sha256") != hashlib.sha256(events.encode("ascii")).hexdigest()
                    or not trace_json.get("recovery_boundaries")
                    or str(manifest) in trace_json_report.stdout
                    or str(data_directory) in trace_json_report.stdout
                    or '"path"' in trace_json_report.stdout):
                raise SystemExit(
                    f"{adapter} leaked a local path or omitted hash-bound trace diagnostics:\n"
                    f"{trace_json_report.stdout}\n{trace_json_report.stderr}"
                )
            # A timestamp-shaped string is not enough provenance: calendar
            # validity is enforced before an otherwise hash-matched genuine
            # release can receive a trace report. This remains validation
            # only; it does not alter the original archive or replay an event.
            invalid_time_manifest = trace_root / f"{adapter}-invalid-time.eontrace"
            invalid_time_manifest.write_text(
                manifest.read_text(encoding="ascii").replace(
                    "capture_start_utc\t2026-08-29T00:00:00Z",
                    "capture_start_utc\t2026-02-29T00:00:00Z",
                ),
                encoding="ascii",
            )
            invalid_time_report = subprocess.run(
                (str(executable), "--data", str(data_directory), "--game", game,
                    "--platform", platform, "--reference-trace", str(invalid_time_manifest)),
                env=environment, check=False, capture_output=True, text=True,
            )
            if (invalid_time_report.returncode == 0
                    or "invalid versioned values" not in invalid_time_report.stderr):
                raise SystemExit(
                    f"{adapter} accepted an impossible UTC capture date:\n"
                    f"{invalid_time_report.stdout}\n{invalid_time_report.stderr}"
                )

    # Pack bytes are deliberately temporary test fixtures, outside supplied
    # media. They prove the CLI invokes the real read-only admission reader;
    # neither this test nor the runtime writes a Modern-pack cache.
    with tempfile.TemporaryDirectory(dir=temporary_root) as temporary_pack_root:
        pack_root = Path(temporary_pack_root)
        eligible_pack = pack_root / "test-modern-pack"
        eligible_pack.mkdir()
        asset = eligible_pack / "art.bin"
        asset.write_bytes(b"Project Eon temporary external Modern-pack test asset\n")
        asset_hash = hashlib.sha256(asset.read_bytes()).hexdigest()
        (eligible_pack / "pack.eonmodern").write_text(
            "schema\tproject-eon.modern-asset-pack/v1\n"
            "id\ttest-modern-pack\n"
            "version\t1\n"
            "license\tCC0-1.0\n"
            "provenance\tindependently-created\n"
            "game\tmillennium\n"
            "platform\tdos\n"
            f"source_release_sha256\t{source_match.group(1)}\n"
            f"asset\ttest-art art.bin {asset.stat().st_size} {asset_hash}\n",
            encoding="ascii",
        )
        rejected_pack = pack_root / "rejected-test-pack"
        rejected_pack.mkdir()
        (rejected_pack / "pack.eonmodern").write_text("not-a-manifest\n", encoding="ascii")
        packs_before = media_snapshot(pack_root)
        pack_report = subprocess.run(
            (str(executable), "--data", str(data_directory), "--inspect", "--game", "millennium",
                "--platform", "dos", "--modern-packs", str(pack_root)),
            env=environment, check=False, capture_output=True, text=True,
        )
        if (pack_report.returncode != 0
                or "MODERN PACK ELIGIBLE  test-modern-pack 1" not in pack_report.stdout
                or "MODERN PACK REJECTED" not in pack_report.stdout
                or "no pack is selected or rendered" not in pack_report.stdout
                or media_snapshot(pack_root) != packs_before):
            raise SystemExit(
                "explicit Modern-pack inspection did not report read-only eligibility and rejection:\n"
                f"{pack_report.stdout}\n{pack_report.stderr}"
            )

    # A syntactically valid but unavailable platform filter must fail clearly;
    # it is never permission to replace Atari ST with an Amiga/DOS report.
    unavailable_filter = subprocess.run(
        (str(executable), "--data", str(data_directory), "--inspect", "--game", "deuteros",
            "--platform", "dos"),
        env=environment, check=False, capture_output=True, text=True,
    )
    if (unavailable_filter.returncode != 5
            or "No recognised original release matches the requested inspection filters."
                not in unavailable_filter.stderr
            or "VERIFIED  " in unavailable_filter.stdout):
        raise SystemExit(
            "unavailable inspect filter did not fail without platform substitution:\n"
            f"{unavailable_filter.stdout}\n{unavailable_filter.stderr}"
        )
    unavailable_start = subprocess.run(
        (str(executable), "--data", str(data_directory), "--game", "deuteros", "--platform", "dos"),
        env=environment, check=False, capture_output=True, text=True,
    )
    if (unavailable_start.returncode != 4
            or "Requested original release is not present for the selected platform."
                not in unavailable_start.stderr
            or "no platform fallback was selected." not in unavailable_start.stderr):
        raise SystemExit(
            "unavailable direct launch did not explain its no-fallback boundary:\n"
            f"{unavailable_start.stdout}\n{unavailable_start.stderr}"
        )
    # The CLI has no platform-card page, so it must not silently reproduce the
    # old implicit DOS/Amiga preference when a direct game request omits it.
    ambiguous_start = subprocess.run(
        (str(executable), "--data", str(data_directory), "--game", "millennium"),
        env=environment, check=False, capture_output=True, text=True,
    )
    if (ambiguous_start.returncode != 2
            or "--game requires --platform for a direct launch" not in ambiguous_start.stderr
            or "SDL_Init" in ambiguous_start.stderr):
        raise SystemExit(
            "ambiguous direct launch did not stop before selecting a platform:\n"
            f"{ambiguous_start.stdout}\n{ambiguous_start.stderr}"
        )
    # English is the stable default for a selected platform when present.
    # The process must reach SDL rather than fail as if Spanish had been
    # silently selected; an explicit --release-language es remains tested
    # separately above.
    try:
        subprocess.run(
            (str(executable), "--data", str(data_directory), "--game", "millennium",
                "--platform", "dos", "--presentation", "original"),
            env=environment, check=False, capture_output=True, text=True, timeout=1,
        )
    except subprocess.TimeoutExpired:
        pass
    else:
        raise SystemExit("Millennium DOS did not enter its SDL loop with the English default release")
    # Every platform now has a bounded SDL-free launch check. It crosses the
    # same scanner identity, outer hash and native adapter gate as a real
    # launch, then exits before any host window/input/audio loop. This makes
    # original-media startup verification deterministic instead of relying on
    # a process timeout for each platform.
    starts: list[tuple[str, tuple[str, ...]]] = []
    for presentation in ("original", "modern"):
        for game, platform in (
            ("millennium", "dos"),
            ("millennium", "amiga"),
            ("millennium", "atari-st"),
            ("deuteros", "amiga"),
            ("deuteros", "atari-st"),
        ):
            starts.append((
                f"{game}/{platform}/{presentation}",
                (str(executable), "--data", str(data_directory), "--game", game,
                    "--platform", platform, "--presentation", presentation, "--launch-check"),
            ))
    for name, command in starts:
        completed = subprocess.run(command, env=environment, check=False,
            capture_output=True, text=True)
        if completed.returncode != 0 or "LAUNCH CHECK  " not in completed.stdout or " / READY" not in completed.stdout:
            raise SystemExit(
                f"{name} did not complete the shared runtime admission gate:\n"
                f"{completed.stdout}\n{completed.stderr}"
            )

    # An explicit Modern-pack path is part of the requested renderer input.
    # A rejected path must fail the CLI route rather than turn into a green
    # Original-only launch check with the pack silently discarded.
    original_ignores_modern_pack = subprocess.run(
        (str(executable), "--data", str(data_directory), "--game", "millennium",
            "--platform", "dos", "--presentation", "original",
            "--modern-pack", str(temporary_root / "missing-pack.eonmodern"), "--launch-check"),
        env=environment, check=False, capture_output=True, text=True,
    )
    if (original_ignores_modern_pack.returncode != 0
            or "LAUNCH CHECK  " not in original_ignores_modern_pack.stdout
            or "Modern asset pack" in original_ignores_modern_pack.stderr):
        raise SystemExit(
            "Original launch check accessed an irrelevant Modern pack:\n"
            f"{original_ignores_modern_pack.stdout}\n{original_ignores_modern_pack.stderr}"
        )

    rejected_modern_pack = subprocess.run(
        (str(executable), "--data", str(data_directory), "--game", "millennium",
            "--platform", "dos", "--presentation", "modern",
            "--modern-pack", str(temporary_root / "missing-pack.eonmodern"), "--launch-check"),
        env=environment, check=False, capture_output=True, text=True,
    )
    if (rejected_modern_pack.returncode != 5
            or "Modern asset pack rejected before launch:" not in rejected_modern_pack.stderr
            or "Modern asset pack required by the CLI selection was rejected" not in rejected_modern_pack.stderr):
        raise SystemExit(
            "an explicit rejected Modern pack did not fail the CLI launch check:\n"
            f"{rejected_modern_pack.stdout}\n{rejected_modern_pack.stderr}"
        )

    # An individual original archive is a supported --data source too.  Ask
    # the program itself for the content-addressed identity first, then launch
    # that exact game/platform pair from the same read-only archive path.
    platform_names = {"DOS": "dos", "Amiga": "amiga", "Atari ST": "atari-st"}
    archive_starts: list[tuple[str, tuple[str, ...]]] = []
    detected_releases: set[tuple[str, str, str]] = set()
    for archive in sorted(data_directory.rglob("*.zip")):
        inspected = subprocess.run(
            (str(executable), "--data", str(archive), "--inspect"),
            env=environment, check=False, capture_output=True, text=True,
        )
        if inspected.returncode != 0:
            continue
        line = next((value for value in inspected.stdout.splitlines()
            if value.startswith("VERIFIED  ")), None)
        if not line:
            continue
        game = "millennium" if "Millennium 2.2" in line else "deuteros"
        platform = next((value for label, value in platform_names.items()
            if f" / {label} / " in line), None)
        if platform is None:
            raise SystemExit(f"Could not parse inspected platform for {archive}:\n{line}")
        language = line.rsplit(" / ", 1)[-1]
        detected_releases.add((game, platform, language))
        startup_boundaries = [value for value in inspected.stdout.splitlines()
                              if value.startswith("          STARTUP BOUNDARY  ")]
        if len(startup_boundaries) != 1 or "; stops before " not in startup_boundaries[0]:
            raise SystemExit(
                f"{game}/{platform} did not report exactly one explicit startup boundary:\n"
                f"{inspected.stdout}"
            )
        expected_bootstrap = {
            ("millennium", "amiga"): (
                "bounded launcher bootstrap: resident entry 0x68000, raw resident SHA-256 "
                "d144abc05f891710dc99b30d87f020bd6e2ff7796ef86a847f07b8d97d55d18e"
            ),
            ("millennium", "atari-st"): (
                "bounded launcher bootstrap: executed 54 original longword copies and 257 original word "
                "copies to target 0x77000, stops before TRAP #1 at 0x7700e after 14 original "
                "Fopen-prefix bytes / 8 relative stack bytes; Fopen boundary MILL22A.inf"
            ),
            ("deuteros", "amiga"): (
                "Channel-request static continuation: ADF 0x7092, entry 0x21892; BSR 0x2189a -> 0x2229c, "
                "0x2189e -> 0x224a2; bit 6 loop 0x218c6 -> 0x218be; final 0x218c8 -> 0x217f6; SHA-256 "
                "120fba90e0b4fa9e96d8a6cf95fbac512d67d7daa42c3776ce0d3066b3f02ee9"
            ),
            ("deuteros", "atari-st"): (
                "bounded launcher bootstrap: first/second raw stages SHA-256 "
                "dad3594c53375bd8285ef33e2d685bd38a5b38d930f2ea1305d117d63667f168/"
                "2489256511e857a4a1b20d413b4f869edaae1f4df7f62ce869e324cad40e81d7"
            ),
        }.get((game, platform))
        if expected_bootstrap and expected_bootstrap not in inspected.stdout:
            raise SystemExit(
                f"{game}/{platform} bounded launcher bootstrap did not match supplied media:\n"
                f"{inspected.stdout}"
            )
        expected_atari_launch_boundary = {
            ("millennium", "atari-st"): (
                "ATARI LAUNCH BOUNDARY  bootstrap only; stops before GEMDOS TRAP #1/Fopen result, "
                "input, and later launcher control flow"
            ),
            ("deuteros", "atari-st"): (
                "ATARI LAUNCH BOUNDARY  protected bootstrap only; stops before XBIOS/callback behavior, "
                "state selection, title, and gameplay"
            ),
        }.get((game, platform))
        if expected_atari_launch_boundary and expected_atari_launch_boundary not in inspected.stdout:
            raise SystemExit(
                f"{game}/{platform} did not report its exact Atari launch boundary:\n"
                f"{inspected.stdout}"
            )
        if game == "millennium" and platform == "amiga":
            expected_post_negative_d3 = (
                "post-negative-D3 terminal: entry 0x685fe; byte stores 0x7b3b5/0x7b3bc; "
                "BNE 0x68612 -> 0x68616, zero RTS 0x68614; BPL 0x68616 -> boundary 0x6861a, "
                "negative RTS 0x68618; SHA-256 "
                "a45ff5eca6e3594574b464574fa0aae3027bd2ea11472770708c96f4d21b56cc"
            )
            if expected_post_negative_d3 not in inspected.stdout:
                raise SystemExit(
                    "Millennium Amiga post-negative-D3 terminal did not match supplied media:\n"
                    f"{inspected.stdout}"
                )
            expected_post_negative_d3_continuation = (
                "post-negative-D3 continuation: entry 0x6861a (disk 0x16a1a, 54 bytes); "
                "BCC 0x68636 -> 0x6863a, BCS 0x68642 -> 0x68650, BMI 0x68644 -> 0x68694; "
                "terminal JMP 0x6864a -> 0x7bef0; SHA-256 "
                "d3f6b63090429e11fb3a77e4573817649e2bb7996d06811ea2751078794534ce"
            )
            if expected_post_negative_d3_continuation not in inspected.stdout:
                raise SystemExit(
                    "Millennium Amiga post-negative-D3 continuation did not match supplied media:\n"
                    f"{inspected.stdout}"
                )
        if game == "deuteros" and platform == "amiga":
            expected_callback_boundary = (
                "Title callback boundary: registration 0x1ef74 -> callback 0x1f056; Exec base 0x4 "
                "vector -0x1ce; byte-one table 0x1ee20 +0xa0 -> queue 0x1eec0, pending 0x1eed6; "
                "byte-two gate 0x1ee16 -> service 0x20118 "
                "(static provenance only; no Exec/callback/input execution)"
            )
            if expected_callback_boundary not in inspected.stdout:
                raise SystemExit(
                    "Deuteros Amiga title callback boundary did not match supplied media:\n"
                    f"{inspected.stdout}"
                )
        if game == "millennium" and platform == "dos" and language == "English":
            expected_gx_overlay = (
                "2200GX.EXE overlay evidence: name 0x11c2, loader 0x11ce reads segment cell 0x118; "
                "calls 0x11d1 -> 0x53a/0x11e4 -> 0x574/0x11ec -> 0x596; caller 0xd335 -> 0x11ce; "
                "adapter 0x6c52 RETF 0x6c68 to overlay offset 0x0; SHA-256 "
                "093f8416de6d23837d2faf82360ef79777c2c2bf146619aafad87626c61ab6fb"
            )
            if expected_gx_overlay not in inspected.stdout:
                raise SystemExit(
                    "Millennium DOS GX overlay evidence did not match supplied media:\n"
                    f"{inspected.stdout}"
                )
            expected_gx_dispatcher = (
                "2200GX.EXE dispatcher: entry +0x0, table +0x15; selector 0xe/0xf/0x12/0x14 "
                "-> 0x90/0x9f/0x97/0xa7; near RET then far RETF +0x14; SHA-256 "
                "f4d657fcbdda23d7f0fdf2bbf48405d0a04e8b8149df064607f49042525fbd55"
            )
            if expected_gx_dispatcher not in inspected.stdout:
                raise SystemExit(
                    "Millennium DOS GX dispatcher evidence did not match supplied media:\n"
                    f"{inspected.stdout}"
                )
            expected_gx_selector = (
                "2200GX selector prefix: 2200AD 0xd343 reads 0xda05; 0x3/0x4/0x2/default "
                "-> AX 0xe/0x12/0x14/0xf, stores DX at 0x4b6e, CALL 0xd373 -> 0x6c52; SHA-256 "
                "571626e83b0787401f89c8586c12dfb4d4221c44e0a9786727d2314b09327091"
            )
            if expected_gx_selector not in inspected.stdout:
                raise SystemExit(
                    "Millennium DOS GX selector evidence did not match supplied media:\n"
                    f"{inspected.stdout}"
                )
            if ("Original VOC bank: 14 hash-verified voices, 37100 unsigned PCM samples at "
                    "10000 Hz/6024 Hz (inspection only; no event mapping, driver ABI, or playback)"
                    not in inspected.stdout):
                raise SystemExit(
                    "Millennium DOS original VOC bank was not completely read in inspection:\n"
                    f"{inspected.stdout}"
                )
        if game == "millennium" and platform == "dos" and language == "Spanish":
            expected_spanish_ibm = (
                "Spanish IBM.COM handoff: caller 0x23d names 0x71d/0x728; calls 0x240/0x24c "
                "-> 0x339; JNE 0x245/0x251; SHA-256 "
                "84b7d158c770117aeaa07cb5ea2e7ed4a6bcc288d6b352d82569ff4d97b2fda9"
            )
            if expected_spanish_ibm not in inspected.stdout:
                raise SystemExit(
                    "Millennium Spanish DOS IBM.COM handoff did not match supplied media:\n"
                    f"{inspected.stdout}"
                )
            expected_spanish_isolation = (
                "Spanish isolation boundary: only this image's IBM.COM, TITLES.EXE, and 2200AD.EXE are "
                "reported; no English executable, state, or title path is substituted."
            )
            if expected_spanish_isolation not in inspected.stdout:
                raise SystemExit(
                    "Millennium Spanish DOS isolation boundary was not reported:\n"
                    f"{inspected.stdout}"
                )
        if game == "millennium" and platform == "atari-st":
            expected_atari_boundary = (
                "Atari ST trace boundary: next evidence must identify GEMDOS Fopen D0/handle behaviour, "
                "TRAP #14 and Line-A returns, configuration load address, and any codec, palette, or planar "
                "destination. No alternate ST image or DOS/Amiga asset is substituted."
            )
            if expected_atari_boundary not in inspected.stdout:
                raise SystemExit(
                    "Millennium Atari ST trace boundary was not reported:\n"
                    f"{inspected.stdout}"
                )
        if game == "deuteros" and platform == "atari-st":
            expected_atari_boundary = (
                "Atari ST trace boundary: next evidence must identify the XBIOS Floprd result, callback "
                "entry/return frame, dispatch word at RAM 0x1eaa, and selected vector D1/D2 returns. "
                "Reported raw-load plans are not performed and no Amiga or synthetic screen is used."
            )
            if expected_atari_boundary not in inspected.stdout:
                raise SystemExit(
                    "Deuteros Atari ST trace boundary was not reported:\n"
                    f"{inspected.stdout}"
                )
        if game == "deuteros" and platform == "amiga":
            expected_callee = (
                "Channel-request first callee: ADF 0x7a9c, entry 0x2229c; bit 5 branch 0x222b4 -> 0x2232c; "
                "DBRA 0x222e0 -> 0x222be; vectors 0x222fc/0x22312; final services 0x2231e/0x22324 -> 0x21698; "
                "SHA-256 d1a162af50f92b60d03b1da4ab186a547e46d145b0599cfbbeff7fb5af324ac1"
            )
            if expected_callee not in inspected.stdout:
                raise SystemExit(
                    "Deuteros Amiga channel-request first callee did not match supplied media:\n"
                    f"{inspected.stdout}"
                )
            expected_second_callee = (
                "Channel-request second callee: ADF 0x7ca2, entry 0x224a2; longword 0x224e6 -> 0x6c; "
                "clears 0xdff0a8/0xdff0b8/0xdff0c8/0xdff0d8; 0xf -> 0xdff096; RTS 0x224ca; SHA-256 "
                "d4e9a1ee0065537a627cdd9ee8827f11d5fa28e0f860aacb21bbdc7e11784bd1"
            )
            if expected_second_callee not in inspected.stdout:
                raise SystemExit(
                    "Deuteros Amiga channel-request second callee did not match supplied media:\n"
                    f"{inspected.stdout}"
                )
            expected_following_service = (
                "Channel-request following service: ADF 0x825a, entry 0x22a5a; execution 0x22ab8, "
                "embedded table 0x22a6a, descriptors 0x22a6e stride 0xe; RTS 0x22b88; SHA-256 "
                "d5fdbdacd004d2cf377ea0dbaefb9d8b308ba23b568cfb3785456622bde49d19"
            )
            if expected_following_service not in inspected.stdout:
                raise SystemExit(
                    "Deuteros Amiga channel-request following service did not match supplied media:\n"
                    f"{inspected.stdout}"
                )
            expected_adjacent_entry = (
                "Channel-request adjacent entry: ADF 0x838a, entry 0x22b8a; test 0x22a30, "
                "zero 0x22b90 -> 0x22b94, early RTS 0x22b92; descriptors 0x22a6e stride 0xe; "
                "final RTS 0x22be8; SHA-256 "
                "10ed8be15c107dbb56ca98eb8d17ffd2bce3910dd169d67ba058447c9031b1ff"
            )
            if expected_adjacent_entry not in inspected.stdout:
                raise SystemExit(
                    "Deuteros Amiga channel-request adjacent entry did not match supplied media:\n"
                    f"{inspected.stdout}"
                )
        if game == "millennium" and platform == "atari-st":
            expected_auxiliary_resource = "auxiliary resource-name evidence: MILL22B.INF cluster "
            if expected_auxiliary_resource not in inspected.stdout:
                raise SystemExit(
                    "Millennium Atari ST auxiliary resource-name evidence did not match supplied media:\n"
                    f"{inspected.stdout}"
                )
            expected_config_census = (
                "supplied ST config scan: 7 images, 5 valid FAT12 volumes, 4 files named "
                "MILL22A.inf; 4 exact config hash and 1 exact MILENIUM.TOS hash"
            )
            if expected_config_census not in inspected.stdout:
                raise SystemExit(
                    "Millennium Atari ST exact launch-pair census did not match supplied media:\n"
                    f"{inspected.stdout}"
                )
            expected_stx = (
                "physical Disk 1 STX: SHA-256 "
                "081d8bc102b8c7669c5cb21abace9b08532bc0b34164f11465d0c87b63a422fd; "
                "80 track records, 800 identified sectors; T0/H0/S1 +0xc0, 512 bytes, SHA-256 "
                "d0601ec6e1bbea0d5f4d5ba37130148e6670225b6337d001f4d4e6b8fc45fd08; "
                "T1/H0/S9 +0x1570, 512 bytes, SHA-256 "
                "096869a11a3f601c587bb915c6c93d7985f8eb2185dc2d0f2839286df9905dad; "
                "literal +0xbe MILL22B.inf"
            )
            if expected_stx not in inspected.stdout:
                raise SystemExit(
                    "Millennium Atari ST physical STX provenance did not match supplied media:\n"
                    f"{inspected.stdout}"
                )
        if game == "deuteros" and platform == "atari-st":
            expected_variant_census = (
                "protected-media variant census: 11 supplied ST leaves, 10 720 KiB candidates, "
                "9 valid checksum/BPB boot profiles, 3 Replicants first-stage shapes, "
                "2 KILLER_BOOT markers, 1 nonstandard leaves; invalid envelope branch/BPB/checksum 1/0/0"
            )
            if expected_variant_census not in inspected.stdout:
                raise SystemExit(
                    "Deuteros Atari ST protected-media variant census did not match supplied media:\n"
                    f"{inspected.stdout}"
                )
            expected_state1 = (
                "Static state-1 raw-load plan: Disk 1 +0x55800 +0x5e400 "
                "-> RAM 0xb000 in 84 original reads; SHA-256 "
                "0d5ccb3a337fcbd4d34d34b3ad24f20c3bb2edca7e7b734b8abb14f6c0a30f47"
            )
            if expected_state1 not in inspected.stdout:
                raise SystemExit(
                    "Deuteros Atari ST state-1 raw-load plan did not match supplied media:\n"
                    f"{inspected.stdout}"
                )
            expected_skipped_ascii = (
                "State-1 skipped ASCII evidence: Disk 1 +0x9d800 BRA.W displacement 0x9c2; "
                "block +0x9d80a +0x438 (18 printable runs), SHA-256 "
                "8dd46e7c760a38d07273b18a4cbd3c03eb44a6b57c8c401580dd47fa4646484e"
            )
            if expected_skipped_ascii not in inspected.stdout:
                raise SystemExit(
                    "Deuteros Atari ST skipped ASCII evidence did not match supplied media:\n"
                    f"{inspected.stdout}"
                )
            expected_state5 = (
                "Static vector-5 raw-load plans: Disk 1 +0x55800 +0xb400 "
                "-> RAM 0xb000 in 10 original reads; SHA-256 "
                "9659b21315e5c0528020be0b41eb75d57428f41b3b632fabfebe16d34038d298; "
                "copy RAM 0x57a00 +0x9393 -> 0xb006; Disk 1 +0x60c00 +0x4c800 "
                "-> RAM 0x16400 in 68 original reads; SHA-256 "
                "6b3e27702649ac201c4ecf92ad54f40656fd4d8633fadf5790014da34ce03ac6"
            )
            if expected_state5 not in inspected.stdout:
                raise SystemExit(
                    "Deuteros Atari ST vector-5 raw-load plan did not match supplied media:\n"
                    f"{inspected.stdout}"
                )
            expected_killer_handoff = (
                "Disk 2 KILLER_BOOT handoff: setup +0xd8 +0x18 copies boot +0xf0 +0x28 -> RAM 0x8; "
                "JMP 0x12 enters relocated +0xa opcode 0x41fa; copied vector +0x8 is JMP (A0) through RAM $4; "
                "setup/relocated SHA-256 "
                "1ce81773d11374cac65ce69742a475e0731cbc8798f7c7bd374c04a2d2a7d150/"
                "21a5d61e2289fe2f2141d3710fad31faf42e96f59c5fba768819380e8f595a8d"
            )
            if expected_killer_handoff not in inspected.stdout:
                raise SystemExit(
                    "Deuteros Atari ST KILLER_BOOT handoff did not match supplied media:\n"
                    f"{inspected.stdout}"
                )
        archive_starts.append((
            f"archive/{game}/{platform}/{archive.name}",
            (str(executable), "--data", str(archive), "--game", game,
                "--platform", platform, "--presentation", "original", "--launch-check"),
        ))
    expected_releases = {
        ("millennium", "dos", "en"),
        ("millennium", "dos", "es"),
        ("millennium", "amiga", "en"),
        ("millennium", "atari-st", "en"),
        ("deuteros", "amiga", "en"),
        ("deuteros", "atari-st", "en"),
    }
    if detected_releases != expected_releases:
        raise SystemExit(
            "Did not find every supplied supported release as a direct --data input:\n"
            f"expected {sorted(expected_releases)}, got {sorted(detected_releases)}"
        )
    for name, command in archive_starts:
        completed = subprocess.run(command, env=environment, check=False,
            capture_output=True, text=True)
        if completed.returncode != 0 or "LAUNCH CHECK  " not in completed.stdout or " / READY" not in completed.stdout:
            raise SystemExit(
                f"{name} did not complete the shared runtime admission gate:\n"
                f"{completed.stdout}\n{completed.stderr}"
            )
    after = media_snapshot(data_directory, original_media_only=True)
    if after != before:
        raise SystemExit("Project Eon changed the supplied game-data directory")

    # The Unix default is a read-only lookup at ~/.projecteon.  It must not
    # bootstrap a missing user-data directory as a side effect of inspection.
    if os.name != "nt":
        with tempfile.TemporaryDirectory(dir=temporary_root) as temporary_home:
            isolated_environment = environment | {"HOME": temporary_home}
            missing_default = Path(temporary_home) / ".projecteon"
            completed = subprocess.run(
                (str(executable), "--inspect"), env=isolated_environment,
                check=False, capture_output=True, text=True,
            )
            if completed.returncode != 2 or missing_default.exists():
                raise SystemExit(
                    "Project Eon did not fail cleanly for a missing default data directory:\n"
                    f"{completed.stderr}"
                )
    # An existing but empty directory is distinct from a missing default path:
    # recognition completes, reports no release, and leaves that directory
    # untouched instead of preparing a placeholder collection.
    with tempfile.TemporaryDirectory(dir=temporary_root) as empty_directory:
        completed = subprocess.run(
            (str(executable), "--data", empty_directory, "--inspect"), env=environment,
            check=False, capture_output=True, text=True,
        )
        if (completed.returncode != 3
                or "No recognised original release archives found." not in completed.stderr
                or list(Path(empty_directory).iterdir())):
            raise SystemExit(
                "Project Eon did not reject an empty data directory without mutation:\n"
                f"{completed.stdout}\n{completed.stderr}"
            )
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
