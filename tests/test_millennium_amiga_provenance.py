"""Guard the visible Millennium Amiga raw-loader provenance panel."""

from pathlib import Path
import unittest


ROOT = Path(__file__).resolve().parents[1]
SOURCE = (ROOT / "src" / "main.cpp").read_text(encoding="utf-8")


class MillenniumAmigaProvenancePanelTests(unittest.TestCase):
    def test_amiga_panel_uses_one_hash_validated_session(self) -> None:
        panel = SOURCE.index("if (*active_platform == eon::Platform::amiga && amiga_bootstrap)")
        end = SOURCE.index('draw_text(renderer, 64, 680', panel)
        panel = SOURCE[panel:end]
        for fact in (
            "amiga_bootstrap->plan",
            "amiga_bootstrap->opaque_invocation_boundary",
            "plan.first_stage.disk_offset",
            "plan.first_stage.length",
            "plan.first_stage.destination",
            "plan.resident_stage.disk_offset",
            "handoff.first_stage_invocation_address",
            "handoff.first_stage_target",
        ):
            with self.subTest(fact=fact):
                self.assertIn(fact, panel)
        self.assertIn("native Amiga boundary", panel)
        self.assertIn("NO CALL RETURN OR RUNTIME STATE", panel)
        self.assertNotIn("millennium_amiga_session->", panel)

    def test_cli_report_consumes_session_snapshot(self) -> None:
        report = SOURCE[SOURCE.index("void report_millennium_amiga("):
                        SOURCE.index("void report_millennium_atari_root_inventory(")]
        self.assertIn("bootstrap.plan()", report)
        self.assertIn("bootstrap.opaque_invocation_boundary()", report)
        self.assertNotIn("parse_millennium_amiga_resident_", report)


if __name__ == "__main__":
    unittest.main()
