import json
from pathlib import Path
import re
import unittest


ROOT = Path(__file__).resolve().parents[1]
FORMAT = ROOT / "docs" / "REFERENCE_TRACE_FORMAT.md"
TRACE_VALIDATOR = ROOT / "src" / "data" / "reference_trace.cpp"


class ReferenceTraceFormatTests(unittest.TestCase):
    """Keep the public capture contract aligned with trace admission code."""

    adapters = {
        "millennium-dos-en-startup-v1": {
            "game": "millennium", "platform": "dos", "language": "en", "size": 328383,
            "release": "e6e7044b25877fdf8b10d16d2f395886d9957953144ae15ca630cda9cab2a123",
        },
        "millennium-dos-en-gx-startup-v2": {
            "game": "millennium", "platform": "dos", "language": "en", "size": 328383,
            "release": "e6e7044b25877fdf8b10d16d2f395886d9957953144ae15ca630cda9cab2a123",
        },
        "millennium-dos-en-title-init-v2": {
            "game": "millennium", "platform": "dos", "language": "en", "size": 328383,
            "release": "e6e7044b25877fdf8b10d16d2f395886d9957953144ae15ca630cda9cab2a123",
        },
        "deuteros-atari-st-boot-v1": {
            "game": "deuteros", "platform": "atari-st", "language": "en", "size": 3021682,
            "release": "c6856d0a7ccda925289c60f0675e7aaed616f8a0289c74698e87e1ee11e6c653",
            "media": "aba874134807360ccde0ff98d6b82a965f57dcae5800b5b54394472522ef5bee",
            "stage": "2489256511e857a4a1b20d413b4f869edaae1f4df7f62ce869e324cad40e81d7",
        },
        "millennium-amiga-en-defjam-bootstrap-v1": {
            "game": "millennium", "platform": "amiga", "language": "en", "size": 2558009,
            "release": "2e27d7aeb8b8b7f2a75eda45b456ab42775a706aa85516c85e61ce94ec9eb400",
        },
        "deuteros-amiga-en-title-stage-v1": {
            "game": "deuteros", "platform": "amiga", "language": "en", "size": 4066771,
            "release": "f4dc8dd1c27c5d389837783becd9b95ab09b78baf40e94e39e2b7e590e470e04",
            "media": "6ea0cc68d3af37203a885032eddf7c28e839e6abb59d8c9cd3792f1308bdec38",
            "stage": "48d65260e9b5f5cbf8d8b3675a178c81b8764810b61a6a2539a56dcb40a8de03",
        },
        "deuteros-amiga-en-main-copy-loop-v3": {
            "game": "deuteros", "platform": "amiga", "language": "en", "size": 4066771,
            "release": "f4dc8dd1c27c5d389837783becd9b95ab09b78baf40e94e39e2b7e590e470e04",
            "media": "6ea0cc68d3af37203a885032eddf7c28e839e6abb59d8c9cd3792f1308bdec38",
            "stage": "a82c0d6a12e156e0832d632a6c40dd58713a00b611dbcba7289aa16b0969a0a6",
        },
        "deuteros-amiga-en-title-bridge-v3": {
            "game": "deuteros", "platform": "amiga", "language": "en", "size": 4066771,
            "release": "f4dc8dd1c27c5d389837783becd9b95ab09b78baf40e94e39e2b7e590e470e04",
            "media": "6ea0cc68d3af37203a885032eddf7c28e839e6abb59d8c9cd3792f1308bdec38",
            "stage": "48d65260e9b5f5cbf8d8b3675a178c81b8764810b61a6a2539a56dcb40a8de03",
        },
        "deuteros-amiga-en-title-display-v4": {
            "game": "deuteros", "platform": "amiga", "language": "en", "size": 4066771,
            "release": "f4dc8dd1c27c5d389837783becd9b95ab09b78baf40e94e39e2b7e590e470e04",
            "media": "6ea0cc68d3af37203a885032eddf7c28e839e6abb59d8c9cd3792f1308bdec38",
            "stage": "48d65260e9b5f5cbf8d8b3675a178c81b8764810b61a6a2539a56dcb40a8de03",
        },
        "deuteros-amiga-en-title-display-artifacts-v5": {
            "game": "deuteros", "platform": "amiga", "language": "en", "size": 4066771,
            "release": "f4dc8dd1c27c5d389837783becd9b95ab09b78baf40e94e39e2b7e590e470e04",
            "media": "6ea0cc68d3af37203a885032eddf7c28e839e6abb59d8c9cd3792f1308bdec38",
            "stage": "48d65260e9b5f5cbf8d8b3675a178c81b8764810b61a6a2539a56dcb40a8de03",
        },
    }

    recovery_boundaries = {
        "millennium-dos-en-startup-v1": (
            "millennium-dos-launcher", "millennium-dos-title-flow", "millennium-dos-game-flow",
        ),
        "millennium-dos-en-gx-startup-v2": (
            "millennium-dos-game-flow", "millennium-dos-gx-overlay",
        ),
        "millennium-dos-en-title-init-v2": (
            "millennium-dos-launcher", "millennium-dos-title-flow",
        ),
        "deuteros-atari-st-boot-v1": (
            "deuteros-atari-protected-boot", "deuteros-atari-first-stage",
        ),
        "millennium-amiga-en-defjam-bootstrap-v1": (
            "millennium-amiga-defjam-bootstrap", "millennium-amiga-shared-resident",
        ),
        "deuteros-amiga-en-title-stage-v1": (
            "deuteros-amiga-main-stage", "deuteros-amiga-title-handoff",
        ),
        "deuteros-amiga-en-main-copy-loop-v3": ("deuteros-amiga-main-stage",),
        "deuteros-amiga-en-title-bridge-v3": (
            "deuteros-amiga-main-stage", "deuteros-amiga-title-handoff",
        ),
        "deuteros-amiga-en-title-display-v4": (
            "deuteros-amiga-main-stage", "deuteros-amiga-title-handoff",
        ),
        "deuteros-amiga-en-title-display-artifacts-v5": (
            "deuteros-amiga-main-stage", "deuteros-amiga-title-handoff",
        ),
    }

    def test_registry_rows_match_release_manifest(self):
        manifest = json.loads((ROOT / "docs" / "release-manifest.json").read_text(encoding="utf-8"))
        releases = {release["sha256"]: release for release in manifest["releases"]}
        for adapter, identity in self.adapters.items():
            with self.subTest(adapter=adapter):
                release = releases[identity["release"]]
                self.assertEqual(
                    (release["game"], release["platform"].replace("_", "-"), release["language"], release["size"]),
                    (identity["game"], identity["platform"], identity["language"], identity["size"]),
                )

    def test_every_accepted_versioned_adapter_is_documented_with_its_identity(self):
        code = TRACE_VALIDATOR.read_text(encoding="utf-8")
        documented = FORMAT.read_text(encoding="utf-8")
        accepted = set(re.findall(r'fields\.at\("adapter"\) == "([a-z0-9-]+)"', code))
        self.assertEqual(accepted, set(self.adapters))
        for adapter, identity in self.adapters.items():
            with self.subTest(adapter=adapter):
                self.assertIn(f"`{adapter}`", documented)
                self.assertIn(identity["release"], documented)
                self.assertIn(str(identity["size"]), documented)
                if "media" in identity:
                    self.assertIn(identity["media"], documented)
                    self.assertIn(identity["stage"], documented)

    def test_declarative_adapter_boundary_maps_are_documented_and_compiled(self):
        code = TRACE_VALIDATOR.read_text(encoding="utf-8")
        documented = FORMAT.read_text(encoding="utf-8")
        for adapter, boundaries in self.recovery_boundaries.items():
            with self.subTest(adapter=adapter):
                self.assertIn(f'AdapterRecoveryMap{{"{adapter}"', code)
                self.assertIn(f"`{adapter}`", documented)
                for boundary in boundaries:
                    self.assertIn(f'"{boundary}"', code)
                    self.assertIn(f"`{boundary}`", documented)

    def test_title_display_v5_requires_real_hash_bound_artifacts_without_runtime_replay(self):
        code = TRACE_VALIDATOR.read_text(encoding="utf-8")
        documented = FORMAT.read_text(encoding="utf-8")
        self.assertIn("validate_deuteros_amiga_title_display_artifacts_v5", code)
        self.assertIn("input-timeline.txt", code)
        self.assertIn("copper-list.bin", code)
        self.assertIn("palette-rgb4.bin", code)
        self.assertIn("bitplanes.bin", code)
        self.assertIn("palette-rgba8888.bin", code)
        self.assertIn("frame-rgba8888.bin", code)
        self.assertIn("audio-s16le.bin", code)
        self.assertIn("pcm_size != channels * sample_frames * 2U", code)
        self.assertIn("artifacts verified; diagnostics only", (ROOT / "src" / "main.cpp").read_text(encoding="utf-8"))
        self.assertIn("No artifact byte is decoded, rendered, replayed", documented)

    def test_documented_dos_schema_includes_the_2200ad_private_wrapper_site(self):
        documented = FORMAT.read_text(encoding="utf-8")
        validator = (ROOT / "src" / "data" / "millennium_dos_reference_trace.cpp").read_text(
            encoding="utf-8"
        )
        site = "image=2200ad.exe pc=0x0124 int=0x91 ax=0x001f es=cs bx=0xd19e"
        self.assertIn(site, documented)
        for literal in ("2200ad.exe", "0x0124", "0x001f", "0xd19e"):
            self.assertIn(literal, validator)

    def test_dosbox_x_recorder_contract_keeps_observation_hooks_non_mutating(self):
        contract = (ROOT / "docs" / "MILLENNIUM_DOS_DOSBOX_X_RECORDER.md").read_text(
            encoding="utf-8"
        )
        for literal in (
            "234797680781567e18c374c9e62da24de5423db0",
            "CPU_Interrupt(Bitu,Bitu,uint32_t)",
            "CPU_Core_Normal_Run", "DOS_Execute(const char*,...)",
            "core=normal", "MILL.COM:0x02cf", "millennium-dos-en-startup-v1",
        ):
            with self.subTest(literal=literal):
                self.assertIn(literal, contract)
        self.assertIn("must never change original media, guest memory, CPU registers", contract)
        self.assertIn("not an admitted trace", contract)

    def test_millennium_setup_site_is_not_misreported_as_the_interrupt_opcode(self):
        documented = FORMAT.read_text(encoding="utf-8")
        validator = (ROOT / "src" / "data" / "millennium_dos_reference_trace.cpp").read_text(
            encoding="utf-8"
        )
        self.assertIn("setup site (`MOV AX,0x2591`)", documented)
        self.assertIn("actual `CD 21` opcode at `0x020c`", documented)
        self.assertIn("MOV AX,0x2591 setup-site identifier", validator)
        self.assertIn("CD 21 instruction at 0x020c", validator)

    def test_deuteros_amiga_next_step_capture_contract_keeps_unknown_abi_external(self):
        documented = FORMAT.read_text(encoding="utf-8")
        self.assertIn("Required capture contract before a Deuteros Amiga runtime increment", documented)
        for literal in ("$40450", "$1ed80", "$1ef74", "$1f056", "$1fe7a", "$1fea8",
                        "$1eec0..$1eed3", "$1eed6", "$1ee20..$1eebf", "$1f98c"):
            with self.subTest(literal=literal):
                self.assertIn(literal, documented)
        self.assertIn("**not** an extension of the accepted\nv2 grammar", documented)
        self.assertIn("are not\nruntime inputs", documented)

    def test_deuteros_amiga_title_bridge_v3_is_diagnostic_only(self):
        documented = FORMAT.read_text(encoding="utf-8")
        validator = (ROOT / "src" / "data" / "deuteros_amiga_title_bridge_reference_trace.cpp").read_text(
            encoding="utf-8"
        )
        self.assertIn("deuteros-amiga-en-title-bridge-v3", documented)
        self.assertIn("never supplies\nthem to a title-stage session", documented)
        self.assertIn("callback-registration-return", validator)
        self.assertIn("queue-snapshot", validator)
        self.assertIn("dispatch-snapshot", validator)

    def test_research_protocol_distinguishes_generic_v1_from_strict_v2_adapters(self):
        research = (ROOT / "docs" / "research.md").read_text(encoding="utf-8")
        self.assertIn("Generic v1 verifies", research)
        self.assertIn("v2 adapters additionally validate literal, hash-pinned", research)
        self.assertNotIn("Its first version\nverifies a trace's hash", research)

    def test_research_does_not_mistake_a_zip_mount_for_a_dos_game_launch(self):
        research = (ROOT / "docs" / "research.md").read_text(encoding="utf-8")
        self.assertIn("does **not** expose\n`MILL.COM` as an executable DOS file", research)
        self.assertIn("`Bad command or\nfilename`", research)
        self.assertIn("not a\ngame launch", research)

    def test_read_only_fuse_observation_is_not_promoted_to_runtime_evidence(self):
        research = (ROOT / "docs" / "research.md").read_text(encoding="utf-8")
        self.assertIn("Archivemount/FUSE read-only view", research)
        self.assertIn("without extracting or copying", research)
        self.assertIn("not an Eon reference trace", research)
        self.assertIn("nor authorizes an Eon runtime transition", research)
        self.assertIn("emulator/configuration\nfailure observation", research)
        self.assertIn("not a statement about original-game behavior", research)
        self.assertIn("automated Xvfb key injection was not observed by the guest", research)
        self.assertIn("does not validate that\nconfiguration as a remedy", research)


if __name__ == "__main__":
    unittest.main()
