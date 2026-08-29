"""Guard the visible Millennium Amiga raw-loader provenance panel."""

from pathlib import Path
import unittest


ROOT = Path(__file__).resolve().parents[1]
SOURCE = (ROOT / "src" / "main.cpp").read_text(encoding="utf-8")


class MillenniumAmigaProvenancePanelTests(unittest.TestCase):
    def test_amiga_panel_uses_one_hash_validated_session(self) -> None:
        panel = SOURCE.index("if (*active_platform == eon::Platform::amiga && millennium_amiga_session)")
        end = SOURCE.index('draw_text(renderer, 64, 680', panel)
        panel = SOURCE[panel:end]
        for fact in (
            "millennium_amiga_session->plan()",
            "millennium_amiga_session->opaque_invocation_boundary()",
            "millennium_amiga_session->resident_entry()",
            "plan.first_stage.disk_offset",
            "plan.resident_stage.disk_offset",
            "handoff.first_stage_invocation_address",
            "resident.result_word_address",
        ):
            with self.subTest(fact=fact):
                self.assertIn(fact, panel)
        self.assertIn("opaque first stage", panel)
        self.assertNotIn("millennium_amiga_session->tick", panel)


if __name__ == "__main__":
    unittest.main()
