"""Keep the shipped launcher translations complete and reviewable.

The native reader intentionally consumes PO source directly, so this lightweight
test checks the source catalog contract without requiring gettext tooling in
CI.  It checks Project Eon's own UI only; original game prose stays in the
verified media and is deliberately outside this catalog.
"""

from __future__ import annotations

import ast
from pathlib import Path
import re
import unittest


ROOT = Path(__file__).resolve().parents[1]
PO = ROOT / "po"
LAUNCHER_SOURCE = (ROOT / "src" / "main.cpp").read_text(encoding="utf-8")
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
            "RECOVERY MAP BOUNDARIES",
            "STARTUP BOUNDARY", "TRACE ADMISSION", "MODERN PACK", "PACK RENDER TARGETS",
            "NOT LOADED", "NOT SELECTED", "RENDERER SETTINGS", "FRAME PACING",
            "SDL VSYNC: ON", "SDL VSYNC: OFF",
            "DIAGNOSTICS ARE READ-ONLY; ORIGINAL DATA IS NOT MODIFIED.",
            "ENTER: VIEW FUNCTION MAP   F10 / ESC: BACK TO SETTINGS",
            "RECOVERY FUNCTION MAP", "UP/DOWN: PAGE   F10 / ESC: BACK TO DIAGNOSTICS",
            "PAGE", "NO HASH-BOUND FUNCTION ENTRIES FOR THIS RELEASE.",
            "DECLARATIVE DIAGNOSTICS ONLY; THIS DOES NOT EXECUTE ORIGINAL CODE.",
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
        }
        for language in {"ar", "el", "hi", "ja", "ko", "ru", "tr", "uk", "zh_CN"}:
            with self.subTest(language=language):
                catalog = po_messages(PO / f"{language}.po")
                self.assertTrue(all(catalog.get(label) not in {None, "", label}
                                    for label in labels))

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
