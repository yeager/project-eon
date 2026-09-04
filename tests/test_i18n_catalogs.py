"""Keep the shipped launcher translations complete and reviewable.

The native reader intentionally consumes PO source directly, so this lightweight
test checks the source catalog contract without requiring gettext tooling in
CI. Original game bytes stay in verified media, while every recovered string
shown to a player is represented by a stable presentation message in the same
catalog contract for Original and Modern mode.
"""

from __future__ import annotations

import ast
import json
from pathlib import Path
import re
import unittest


ROOT = Path(__file__).resolve().parents[1]
PO = ROOT / "po"
LAUNCHER_SOURCE = (ROOT / "src" / "main.cpp").read_text(encoding="utf-8")
GAME_TEXT_SOURCE = (ROOT / "src" / "game_text_localization.cpp").read_text(encoding="utf-8")
CATALOGS = {
    "ar", "de", "el", "en_GB", "es", "fi", "fr", "hi", "it", "ja",
    "ko", "nl", "no", "pl", "pt_BR", "ru", "sv", "tr", "uk", "zh_CN",
}
PLACEHOLDER_PREFIX = re.compile(
    r"^(?:ARABIC|ARABISKA|DEUTSCH|ELLINIKA|ENGLISH|ESPANOL|ESPAGNOL|"
    r"FRANCAIS|HINDI|ITALIANO|JAPANESE|KOREAN|NEDERLANDS|NORSK|POLSKI|"
    r"PORTUGUES|RUSSIAN|SVENSKA|TURKCE|UKRAINIAN|CHINESE):\s",
    re.IGNORECASE,
)

# These are names or intentionally stable technical/product labels, not
# untranslated launcher prose. Keeping this allow-list per catalogue makes a
# newly added English fallback visible in review and in CI.
INTENTIONALLY_IDENTICAL_TRANSLATIONS = {
    "ar": {"AMIGA", "ATARI ST", "CRT", "DEUTEROS", "DOS", "MILLENNIUM 2.2"},
    "de": {"AMIGA", "ATARI ST", "CRT", "DEUTEROS", "DOS", "MILLENNIUM 2.2", "MODERN", "ORIGINAL", "ORIGINAL 4:3", "PROJECT EON", "SCANLINES"},
    "el": {"AMIGA", "ATARI ST", "CRT", "DEUTEROS", "DOS", "MILLENNIUM 2.2", "PROJECT EON"},
    "es": {"AMIGA", "ATARI ST", "CRT", "DEUTEROS", "DOS", "MILLENNIUM 2.2", "ORIGINAL", "ORIGINAL 4:3", "PROJECT EON"},
    "fi": {"AMIGA", "ATARI ST", "CRT", "DEUTEROS", "DOS", "MILLENNIUM 2.2", "PROJECT EON"},
    "fr": {"AMIGA", "ATARI ST", "CRT", "DEUTEROS", "DOS", "MILLENNIUM 2.2", "ORIGINAL", "ORIGINAL 4:3", "PAGE", "PROJECT EON"},
    "hi": {"AMIGA", "ATARI ST", "CRT", "DEUTEROS", "DOS", "MILLENNIUM 2.2"},
    "it": {"AMIGA", "ATARI ST", "CRT", "DEUTEROS", "DOS", "MILLENNIUM 2.2", "PROJECT EON"},
    "ja": {"AMIGA", "ATARI ST", "CRT", "DEUTEROS", "DOS", "MILLENNIUM 2.2"},
    "ko": {"AMIGA", "ATARI ST", "CRT", "DEUTEROS", "DOS", "MILLENNIUM 2.2"},
    "nl": {"AMIGA", "ATARI ST", "CRT", "DEUTEROS", "DOS", "MILLENNIUM 2.2", "MODERN", "PLATFORM: ", "PROJECT EON", "Platform: "},
    "no": {"AMIGA", "ATARI ST", "CRT", "DEUTEROS", "DOS", "MILLENNIUM 2.2", "ORIGINAL", "ORIGINAL 4:3", "PROJECT EON"},
    "pl": {"AMIGA", "ATARI ST", "CRT", "DEUTEROS", "DOS", "MILLENNIUM 2.2", "PROJECT EON"},
    "pt_BR": {"AMIGA", "ATARI ST", "CRT", "DEUTEROS", "DOS", "MILLENNIUM 2.2", "ORIGINAL", "ORIGINAL 4:3", "PROJECT EON"},
    "ru": {"AMIGA", "ATARI ST", "CRT", "DEUTEROS", "DOS", "MILLENNIUM 2.2"},
    "sv": {"AMIGA", "ATARI ST", "CRT", "DEUTEROS", "DOS", "MILLENNIUM 2.2", "MODERN", "ORIGINAL", "ORIGINAL 4:3", "PROJECT EON"},
    "tr": {"AMIGA", "ATARI ST", "CRT", "DEUTEROS", "DOS", "MILLENNIUM 2.2", "MODERN", "PLATFORM: ", "PROJECT EON", "Platform: "},
    "uk": {"AMIGA", "ATARI ST", "CRT", "DEUTEROS", "DOS", "MILLENNIUM 2.2"},
    "zh_CN": {"AMIGA", "ATARI ST", "CRT", "DEUTEROS", "DOS", "MILLENNIUM 2.2"},
}

# Hardware and product names are proper nouns in every shipped language. The
# numeric forms are complete user-facing choices, not untranslated prose.
GAME_TEXT_PRODUCT_NAMES = {
    "Sound Blaster", "Covox Sound Master",
    "1 = Sound Blaster", "2 = Covox Sound Master",
}
CELESTIAL_PROPER_NAMES = {
    "Sun", "Mercury", "Venus", "Earth", "Mars", "Jupiter", "Saturn",
    "Uranus", "Neptune", "Pluto", "Moon", "Phobos", "Deimos",
    "Amalthea", "Io", "Europa", "Ganymede",
    "Callisto", "Leda", "Himalia", "Elara", "Pasiphae", "Mimas",
    "Enceladus", "Tethys", "Dione", "Rhea", "Titan", "Hyperion",
    "Iapetus", "Phoebe", "Miranda", "Ariel", "Umbriel", "Titania",
    "Oberon", "Triton", "Nereid", "Charon",
}
for _language in INTENTIONALLY_IDENTICAL_TRANSLATIONS:
    INTENTIONALLY_IDENTICAL_TRANSLATIONS[_language] |= GAME_TEXT_PRODUCT_NAMES


def po_messages(path: Path) -> dict[str, str]:
    """Parse the singular, UTF-8 PO subset accepted by src/i18n.cpp."""
    messages: dict[str, str] = {}
    message_id: str | None = None
    translation: str | None = None
    field: str | None = None

    def commit() -> None:
        nonlocal message_id, translation, field
        if message_id:
            messages[message_id] = translation or ""
        message_id = translation = field = None

    for raw in path.read_text(encoding="utf-8").splitlines() + [""]:
        line = raw.strip()
        if not line:
            commit()
        elif line.startswith("msgid "):
            commit()
            message_id = ast.literal_eval(line[6:])
            translation = ""
            field = "id"
        elif line.startswith("msgstr "):
            translation = ast.literal_eval(line[7:])
            field = "translation"
        elif line.startswith('"') and field == "id":
            message_id = (message_id or "") + ast.literal_eval(line)
        elif line.startswith('"') and field == "translation":
            translation = (translation or "") + ast.literal_eval(line)
    return messages


def po_message_ids(path: Path) -> list[str]:
    """Return IDs in source order so duplicate UI keys cannot hide in a dict."""
    message_ids: list[str] = []
    message_id: str | None = None
    field: str | None = None
    for raw in path.read_text(encoding="utf-8").splitlines() + [""]:
        line = raw.strip()
        if not line:
            if message_id:
                message_ids.append(message_id)
            message_id = field = None
        elif line.startswith("msgid "):
            if message_id:
                message_ids.append(message_id)
            message_id = ast.literal_eval(line[6:])
            field = "id"
        elif line.startswith('"') and field == "id":
            message_id = (message_id or "") + ast.literal_eval(line)
    return message_ids


class CatalogTests(unittest.TestCase):
    def test_declarative_game_text_map_matches_compiled_registry(self) -> None:
        document = json.loads((ROOT / "docs" / "game-text-map.json").read_text(encoding="utf-8"))
        self.assertEqual(document["schema"], 1)
        pattern = re.compile(
            r'GameTextDefinition\{Game::(\w+), Platform::(\w+),\s*'
            r'"([^"]+)", "([^"]+)", "([0-9a-f]{64})",\s*(\d+),\s*(\d+),\s*'
            r'"([^"]+)",\s*"([^"]+)"(?:,\s*"([^"]+)")?\}')
        compiled = []
        for match in pattern.finditer(GAME_TEXT_SOURCE):
            game, platform, key, leaf, digest, offset, size, original, message, source_language = match.groups()
            entry = {
                "id": key, "game": game, "platform": platform,
                "source_leaf": leaf, "source_sha256": digest,
                "source_offset": int(offset), "source_size": int(size),
                "original_text": original, "catalog_msgid": message,
            }
            if source_language:
                entry["source_language"] = source_language
            compiled.append(entry)
        self.assertTrue(compiled)
        self.assertEqual(document["entries"], compiled)

    def test_game_text_map_covers_both_original_celestial_tables(self) -> None:
        document = json.loads((ROOT / "docs" / "game-text-map.json").read_text(encoding="utf-8"))
        entries = document["entries"]
        self.assertEqual(len(entries), 92)
        self.assertEqual(len({entry["catalog_msgid"] for entry in entries}), 51)
        celestial = [entry for entry in entries if ".celestial." in entry["id"]]
        self.assertEqual(len(celestial), 82)
        by_language = {
            language: [entry for entry in celestial
                       if entry.get("source_language", document["default_source_language"])
                       == language]
            for language in ("en", "es")
        }
        self.assertEqual([len(by_language[language]) for language in ("en", "es")], [41, 41])
        self.assertEqual(
            {entry["id"] for entry in by_language["en"]},
            {entry["id"] for entry in by_language["es"]},
        )
        for language, rows in by_language.items():
            with self.subTest(language=language):
                ordered = sorted(rows, key=lambda entry: entry["source_offset"])
                self.assertTrue(all(
                    left["source_offset"] + left["source_size"] < right["source_offset"]
                    for left, right in zip(ordered, ordered[1:])
                ))

    def test_exactly_twenty_shipped_catalogs(self) -> None:
        self.assertEqual({path.stem for path in PO.glob("*.po")}, CATALOGS)

    def test_every_shipped_catalog_translates_every_launcher_message(self) -> None:
        source = po_messages(PO / "ProjectEon.pot")
        self.assertTrue(source)
        for language in sorted(CATALOGS):
            with self.subTest(language=language):
                catalog = po_messages(PO / f"{language}.po")
                missing = sorted(set(source) - set(catalog))
                extra = sorted(set(catalog) - set(source))
                blank = sorted(key for key in source if not catalog.get(key))
                self.assertEqual(missing, [])
                self.assertEqual(extra, [])
                self.assertEqual(blank, [])

    def test_direct_launcher_translation_calls_are_declared_in_the_pot(self) -> None:
        """A literal passed directly to ``tr`` cannot bypass all 20 catalogs.

        Dynamic diagnostic fragments remain intentionally outside this narrow
        static check, but every direct user-visible launcher literal must be
        declared by the one source catalog before it reaches rendering.
        """
        source_catalog = po_messages(PO / "ProjectEon.pot")
        direct_messages = set(re.findall(r'\btr\("((?:[^"\\]|\\.)*)"\)', LAUNCHER_SOURCE))
        self.assertTrue(direct_messages)
        self.assertEqual(sorted(direct_messages - set(source_catalog)), [])

    def test_card_and_selection_labels_are_catalogued_before_rendering(self) -> None:
        # These labels belong to Project Eon's shell, not to the supplied
        # game media. They therefore must not bypass the launcher catalog.
        labels = {
            "MILLENNIUM 2.2", "RETURN TO EARTH", "DEUTEROS", "THE NEXT MILLENNIUM",
            "DOS", "AMIGA", "ATARI ST", "ATARI BOOTSTRAP ONLY", "BOOTSTRAP ONLY", "UNKNOWN PLATFORM",
            "RECOVERY COVERAGE", "RECOVERED STARTUP", "RECOVERED OPENING",
            "SELECT AN ORIGINAL RELEASE", "CHOOSE A LANGUAGE; NO EDITION FALLBACK IS USED",
            "ENGLISH", "LANGUAGE", "SPANISH", "RELEASE IDENTITY IS FIXED AT LAUNCH",
            "ORIGINAL", "PRESERVATION PROFILE", "MODERN", "ENHANCED PROFILE",
            "CUSTOM", "TUNE MODERN SETTINGS",
        }
        source_catalog = po_messages(PO / "ProjectEon.pot")
        self.assertTrue(labels <= set(source_catalog))
        self.assertIn("tr(card.title)", LAUNCHER_SOURCE)
        self.assertIn("tr(card.subtitle)", LAUNCHER_SOURCE)
        self.assertIn("tr(launcher_game_label(selected))", LAUNCHER_SOURCE)
        self.assertIn("tr(launcher_platform_label(*active_platform))", LAUNCHER_SOURCE)
        for language in sorted(CATALOGS):
            with self.subTest(language=language):
                catalog = po_messages(PO / f"{language}.po")
                self.assertTrue(all(catalog.get(label) for label in labels))

    def test_modern_graphics_popup_labels_are_catalogued_in_every_language(self) -> None:
        """F10 settings are Eon's UI, so they cannot fall back to English."""
        labels = {
            "MODERN GRAPHICS SETTINGS",
            "UP/DOWN: SELECT   LEFT/RIGHT: CHANGE   F10: CLOSE",
            "TOUCH: TAP ROW TO CHANGE   TAP OUTSIDE TO CLOSE",
            "OUTPUT RESOLUTION", "ASPECT RATIO", "RENDER PACING",
            "VSYNC (DISPLAY)", "120 FPS (RENDER ONLY)", "UNCAPPED (RENDER ONLY)",
            "GRAPHICS PRESET", "CLEAN", "CRT", "CINEMATIC", "HIGH CONTRAST", "CUSTOM",
            "PIXEL RECONSTRUCTION", "SMOOTH SCALING", "SCANLINES", "MODERN FRAME",
            "MODERN ASSET PACK", "CHOOSE…", "READY", "REJECTED",
            "ORIGINAL 4:3", "SQUARE PIXELS 8:5", "WIDESCREEN 16:9",
            "SCALE2X (MEMORY ONLY)", "SCALE4X (MEMORY ONLY)", "OFF (ORIGINAL PIXELS)", "ON", "OFF",
            "SETTINGS APPLY TO SDL RENDERING ONLY.",
            "DEVELOPER DIAGNOSTICS", "OPEN", "MODERN RUNTIME DIAGNOSTICS",
            "F10 / ESC: BACK TO SETTINGS", "RELEASE IDENTITY", "RUNTIME ADMISSION",
            "REJECTED: IDENTITY", "REJECTED: ARCHIVE HASH", "REJECTED: ADAPTER",
            "NONE", "LAUNCH IDENTITY", "ORIGINAL MEDIA", "RUNTIME CAPABILITY",
            "ADAPTER CONSTRUCTION", "INPUT CONTRACT", "CHILD SESSION",
            "LIFECYCLE TRANSITION",
            "RECOVERY MAP BOUNDARIES",
            "STARTUP BOUNDARY", "TRACE ADMISSION", "MODERN PACK", "PACK RENDER TARGETS",
            "NOT LOADED", "NOT SELECTED", "RENDERER SETTINGS", "FRAME PACING",
            "SDL VSYNC: ON", "SDL VSYNC: OFF",
            "DIAGNOSTICS ARE READ-ONLY; ORIGINAL DATA IS NOT MODIFIED.",
            "ENTER: VIEW FUNCTION MAP   F10 / ESC: BACK TO SETTINGS",
            "RECOVERY FUNCTION MAP", "UP/DOWN: PAGE   F10 / ESC: BACK TO DIAGNOSTICS",
            "PAGE", "NO HASH-BOUND FUNCTION ENTRIES FOR THIS RELEASE.",
            "DECLARATIVE DIAGNOSTICS ONLY; THIS DOES NOT EXECUTE ORIGINAL CODE.",
            "CODE IMAGES", "EXCLUDED", "ACTIVE",
        }
        source_catalog = po_messages(PO / "ProjectEon.pot")
        self.assertTrue(labels <= set(source_catalog))
        for language in sorted(CATALOGS):
            with self.subTest(language=language):
                catalog = po_messages(PO / f"{language}.po")
                self.assertTrue(all(catalog.get(label) for label in labels))

    def test_runtime_rejection_vocabulary_is_localized_in_every_catalog(self) -> None:
        """Safe native admission reasons are launcher UI, never raw errors."""
        labels = {
            "NONE", "LAUNCH IDENTITY", "ORIGINAL MEDIA", "RUNTIME CAPABILITY",
            "ADAPTER CONSTRUCTION", "INPUT CONTRACT", "CHILD SESSION",
            "LIFECYCLE TRANSITION",
        }
        source_catalog = po_messages(PO / "ProjectEon.pot")
        self.assertTrue(labels <= set(source_catalog))
        for language in sorted(CATALOGS):
            with self.subTest(language=language):
                catalog = po_messages(PO / f"{language}.po")
                self.assertTrue(all(catalog.get(label) for label in labels))

    def test_function_map_page_has_no_english_fallback_in_completed_catalogs(self) -> None:
        """The whole F10 function-map page is Project Eon UI, not game text."""
        labels = {
            "ENTER: VIEW FUNCTION MAP   F10 / ESC: BACK TO SETTINGS",
            "RECOVERY FUNCTION MAP",
            "UP/DOWN: PAGE   F10 / ESC: BACK TO DIAGNOSTICS",
            "NO HASH-BOUND FUNCTION ENTRIES FOR THIS RELEASE.",
            "DECLARATIVE DIAGNOSTICS ONLY; THIS DOES NOT EXECUTE ORIGINAL CODE.",
            "MODERN PACK",
            "PACK RENDER TARGETS",
        }
        # English is the deliberate source-language default. Every other
        # shipped launcher catalogue must cover this whole Eon-only page.
        for language in CATALOGS - {"en_GB"}:
            with self.subTest(language=language):
                catalog = po_messages(PO / f"{language}.po")
                self.assertTrue(all(catalog.get(label) not in {None, "", label}
                                    for label in labels))

    def test_complete_f10_diagnostics_have_no_english_fallback(self) -> None:
        """The expanded F10 diagnostics are Eon UI in every script family."""
        labels = {
            "DEVELOPER DIAGNOSTICS", "MODERN RUNTIME DIAGNOSTICS",
            "F10 / ESC: BACK TO SETTINGS", "RELEASE IDENTITY",
            "RECOVERY MAP BOUNDARIES", "TRACE ADMISSION", "RENDERER SETTINGS",
            "FRAME PACING", "SDL VSYNC: ON", "SDL VSYNC: OFF",
            "DIAGNOSTICS ARE READ-ONLY; ORIGINAL DATA IS NOT MODIFIED.",
            "SCALE4X (MEMORY ONLY)", "NOT LOADED", "OPEN",
            "CODE IMAGES", "EXCLUDED", "ACTIVE",
        }
        # English is the source-language default; every other shipped
        # catalogue must translate this complete Eon-owned diagnostics page.
        for language in CATALOGS - {"en_GB"}:
            with self.subTest(language=language):
                catalog = po_messages(PO / f"{language}.po")
                self.assertTrue(all(catalog.get(label) not in {None, "", label}
                                    for label in labels))

    def test_common_title_pack_and_page_labels_are_localized(self) -> None:
        """These launcher labels are not original-game strings or asset names."""
        for language in CATALOGS - {"en_GB"}:
            with self.subTest(language=language, label="MODERN TITLE PACK"):
                catalog = po_messages(PO / f"{language}.po")
                self.assertNotEqual(catalog.get("MODERN TITLE PACK: "), "MODERN TITLE PACK: ")
        # `PAGE` is already the correct French spelling; all other translated
        # catalogues intentionally use their language's distinct UI label.
        for language in CATALOGS - {"en_GB", "fr"}:
            with self.subTest(language=language, label="PAGE"):
                catalog = po_messages(PO / f"{language}.po")
                self.assertNotEqual(catalog.get("PAGE"), "PAGE")

    def test_unselected_diagnostics_identity_is_translated_before_rendering(self) -> None:
        self.assertIn('diagnostics.release_identity = tr("NOT SELECTED");', LAUNCHER_SOURCE)
        source_catalog = po_messages(PO / "ProjectEon.pot")
        self.assertIn("NOT SELECTED", source_catalog)
        for language in sorted(CATALOGS):
            with self.subTest(language=language):
                self.assertTrue(po_messages(PO / f"{language}.po").get("NOT SELECTED"))

    def test_catalogs_do_not_use_language_name_prefixed_english_placeholders(self) -> None:
        for language in sorted(CATALOGS):
            with self.subTest(language=language):
                for message_id, translation in po_messages(PO / f"{language}.po").items():
                    self.assertIsNone(
                        PLACEHOLDER_PREFIX.match(translation),
                        f"{language} leaves a placeholder for {message_id!r}",
                    )

    def test_non_english_catalogs_have_only_reviewed_identical_labels(self) -> None:
        """All other equal source/translation pairs are untranslated UI regressions."""
        for language, allowed in INTENTIONALLY_IDENTICAL_TRANSLATIONS.items():
            with self.subTest(language=language):
                catalog = po_messages(PO / f"{language}.po")
                identical = {message_id for message_id, translation in catalog.items()
                             if message_id and message_id == translation}
                self.assertEqual(identical - CELESTIAL_PROPER_NAMES, allowed)

    def test_catalog_headers_and_keys_are_structurally_unambiguous(self) -> None:
        for language in sorted(CATALOGS):
            with self.subTest(language=language):
                source = (PO / f"{language}.po").read_text(encoding="utf-8")
                self.assertIn('"Content-Type: text/plain; charset=UTF-8\\n"', source)
                self.assertIn(f'"Language: {language}\\n"', source)
                message_ids = po_message_ids(PO / f"{language}.po")
                self.assertEqual(len(message_ids), len(set(message_ids)))

    def test_unicode_renderer_replaces_debug_renderer_for_shipped_catalogs(self) -> None:
        """All non-ASCII catalogs must use the bundled renderer, not host fonts."""
        source = (ROOT / "src" / "main.cpp").read_text(encoding="utf-8")
        self.assertIn("SDL_RenderDebugText", source)
        for language in ("ar", "ja", "zh_CN"):
            with self.subTest(language=language):
                translations = po_messages(PO / f"{language}.po").values()
                self.assertTrue(any(any(ord(character) > 0x7F for character in text)
                                    for text in translations))
        documentation = (PO / "README.md").read_text(encoding="utf-8")
        self.assertIn("Unicode rendering", documentation)
        self.assertIn("SDL_ttf", documentation)
        self.assertIn("bundled Noto fallback chain", documentation)

    def test_unicode_renderer_plan_covers_every_shipped_catalog(self) -> None:
        """The bundled renderer must not drop a catalog by accident."""
        plan = (ROOT / "docs" / "UNICODE_RENDERING.md").read_text(encoding="utf-8")
        self.assertIn("release-3.2.2", plan)
        self.assertIn("SDLTTF_HARFBUZZ", plan)
        self.assertIn("SDL3_ttf::SDL3_ttf", plan)
        self.assertIn("OFL-1.1", plan)
        for language in sorted(CATALOGS):
            with self.subTest(language=language):
                self.assertIn(f"`{language}`", plan)

    def test_variable_evidence_panel_uses_language_neutral_notation(self) -> None:
        """Addresses and bytes may vary, but launcher wording must use PO text."""
        source = (ROOT / "src" / "main.cpp").read_text(encoding="utf-8")
        panel_start = source.index('            draw_text(renderer, 64, 56, tr("LAUNCH REQUEST ACCEPTED"));')
        panel_end = source.index("        SDL_RenderPresent(renderer);", panel_start)
        panel = source[panel_start:panel_end]
        dynamic_english = {
            "ORIGINAL ACTION LOOP:", "HANDLER $", "REIMPLEMENTED PREFIX ONLY:",
            "NO SAVE WRITE, NO NATIVE CALL", "LEFT/RIGHT: TABLE PAGE",
            " length 0x", ", entry 0x",
        }
        for phrase in dynamic_english:
            with self.subTest(phrase=phrase):
                self.assertNotIn(phrase, panel)


if __name__ == "__main__":
    unittest.main()
