#!/usr/bin/env python3
"""Exercise every explicit Project Eon platform start against real media.

This is intentionally configured only for developer builds that provide
EON_REAL_DATA_DIR.  CI never receives commercial media, while preservation
workstations gain a repeatable guard against a platform silently falling back
to another release or failing before its SDL event loop starts.
"""

from __future__ import annotations

import os
from pathlib import Path
import hashlib
import re
import subprocess
import sys
import tempfile


def media_snapshot(directory: Path) -> dict[Path, str]:
    """Return a content snapshot without trusting timestamps or filenames."""
    snapshot: dict[Path, str] = {}
    for path in sorted(directory.rglob("*")):
        relative = path.relative_to(directory)
        if path.is_dir():
            snapshot[relative] = "directory"
            continue
        if not path.is_file():
            snapshot[relative] = "other"
            continue
        digest = hashlib.sha256()
        with path.open("rb") as source:
            for block in iter(lambda: source.read(1024 * 1024), b""):
                digest.update(block)
        snapshot[relative] = f"file:{digest.hexdigest()}"
    return snapshot


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
        ("format", "project-eon-reference-trace-v2"),
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


def main() -> int:
    if len(sys.argv) != 3:
        raise SystemExit("usage: cli_launch_test.py <project-eon> <real-data-dir>")

    executable = Path(sys.argv[1])
    data_directory = Path(sys.argv[2])
    if not executable.is_file():
        raise SystemExit(f"Project Eon executable not found: {executable}")
    if not data_directory.is_dir():
        raise SystemExit(f"Real data directory not found: {data_directory}")

    before = media_snapshot(data_directory)
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
        "PLATFORM ADMISSION  Millennium 2.2 / DOS / RELEASE SELECTION REQUIRED / 2 verified original languages",
        "PLATFORM ADMISSION  Millennium 2.2 / Amiga / READY / 1 verified original language",
        "PLATFORM ADMISSION  Millennium 2.2 / Atari ST / READY / 1 verified original language",
        "PLATFORM ADMISSION  Deuteros / Amiga / READY / 1 verified original language",
        "PLATFORM ADMISSION  Deuteros / Atari ST / READY / 1 verified original language",
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
    trace_specs = (
        (
            "millennium", "dos", "millennium-dos-en-startup-v1",
            "e6e7044b25877fdf8b10d16d2f395886d9957953144ae15ca630cda9cab2a123",
            "event\t1 10 interrupt image=mill.com pc=0x0209 int=0x21 ax=0x2591 dx=0x0000\n"
            "event\t2 20 file image=mill.com pc=0x02cf op=driver-load path=mcga.bin\n"
            "event\t3 30 exec image=mill.com pc=0x0337 int=0x21 ax=0x4b00 path=titles.exe\n"
            "event\t4 40 interrupt image=titles.exe pc=0x0127 int=0x91 ax=0x0000 es=cs bx=0x1ac4\n",
            None, None, "2 interrupt, 1 file, 1 EXEC observations; diagnostics only)",
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
    )
    trace_recovery_boundaries = {
        "millennium-dos-en-startup-v1": (
            "millennium-dos-launcher", "millennium-dos-title-flow", "millennium-dos-game-flow",
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
    }
    with tempfile.TemporaryDirectory() as temporary_trace_root:
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
    with tempfile.TemporaryDirectory() as temporary_pack_root:
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
    ambiguous_edition_start = subprocess.run(
        (str(executable), "--data", str(data_directory), "--game", "millennium", "--platform", "dos"),
        env=environment, check=False, capture_output=True, text=True,
    )
    if (ambiguous_edition_start.returncode != 4
            or "multiple verified original-language releases" not in ambiguous_edition_start.stderr
            or "no edition fallback was selected" not in ambiguous_edition_start.stderr
            or "SDL_Init" in ambiguous_edition_start.stderr):
        raise SystemExit(
            "ambiguous direct launch did not stop before selecting a release language:\n"
            f"{ambiguous_edition_start.stdout}\n{ambiguous_edition_start.stderr}"
        )
    starts = [("start-menu", (str(executable), "--data", str(data_directory)))]
    for presentation in ("original", "modern"):
        for game, platform in (
            ("millennium", "dos"),
            ("millennium", "amiga"),
            ("millennium", "atari-st"),
            ("deuteros", "amiga"),
            ("deuteros", "atari-st"),
        ):
            language = ("--release-language", "en") if (game, platform) == ("millennium", "dos") else ()
            starts.append((
                f"{game}/{platform}/{presentation}",
                (str(executable), "--data", str(data_directory), "--game", game,
                    "--platform", platform, *language, "--presentation", presentation),
            ))
    for name, command in starts:
        try:
            completed = subprocess.run(
                command, env=environment, check=False, capture_output=True,
                text=True, timeout=2,
            )
        except subprocess.TimeoutExpired:
            continue
        raise SystemExit(
            f"{name} exited before its SDL loop (status {completed.returncode}):\n"
            f"{completed.stderr}"
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
                "--platform", platform, "--presentation", "original"),
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
        try:
            completed = subprocess.run(
                command, env=environment, check=False, capture_output=True,
                text=True, timeout=0.75,
            )
        except subprocess.TimeoutExpired:
            continue
        raise SystemExit(
            f"{name} exited before its SDL loop (status {completed.returncode}):\n"
            f"{completed.stderr}"
        )
    after = media_snapshot(data_directory)
    if after != before:
        raise SystemExit("Project Eon changed the supplied game-data directory")

    # The Unix default is a read-only lookup at ~/.projecteon.  It must not
    # bootstrap a missing user-data directory as a side effect of inspection.
    if os.name != "nt":
        with tempfile.TemporaryDirectory() as temporary_home:
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
    with tempfile.TemporaryDirectory() as empty_directory:
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
