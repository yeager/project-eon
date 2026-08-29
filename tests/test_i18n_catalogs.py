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
                blank = sorted(key for key in source if not catalog.get(key))
                self.assertEqual(missing, [])
                self.assertEqual(blank, [])

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
