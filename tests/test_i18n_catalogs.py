"""Keep the shipped launcher translations complete and reviewable.

The native reader intentionally consumes PO source directly, so this lightweight
test checks the source catalog contract without requiring gettext tooling in
CI.  It checks Project Eon's own UI only; original game prose stays in the
verified media and is deliberately outside this catalog.
"""

from __future__ import annotations

import ast
from pathlib import Path
import unittest


ROOT = Path(__file__).resolve().parents[1]
PO = ROOT / "po"
CATALOGS = {
    "ar", "de", "el", "en_GB", "es", "fi", "fr", "hi", "it", "ja",
    "ko", "nl", "no", "pl", "pt_BR", "ru", "sv", "tr", "uk", "zh_CN",
}


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


if __name__ == "__main__":
    unittest.main()
