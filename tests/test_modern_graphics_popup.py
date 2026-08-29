"""Guard Project Eon's renderer-only modern graphics settings overlay."""

from pathlib import Path
import unittest


ROOT = Path(__file__).resolve().parents[1]
SOURCE = (ROOT / "src" / "main.cpp").read_text(encoding="utf-8")


class ModernGraphicsPopupTests(unittest.TestCase):
    def test_f10_opens_renderer_only_settings(self) -> None:
        self.assertIn("SDLK_F10", SOURCE)
        self.assertIn("show_modern_graphics_settings", SOURCE)
        self.assertIn("MODERN GRAPHICS SETTINGS", SOURCE)
        self.assertIn("SETTINGS APPLY TO SDL RENDERING ONLY.", SOURCE)

    def test_popup_controls_only_renderer_options(self) -> None:
        for option in ("smooth_scaling", "scanlines", "frame"):
            self.assertIn(option, SOURCE)
        self.assertIn("draw_scanlines", SOURCE)
        self.assertIn("draw_modern_surface_frame", SOURCE)

    def test_popup_is_modal_for_gamepad_navigation(self) -> None:
        self.assertIn("SDL_GAMEPAD_BUTTON_DPAD_UP", SOURCE)
        self.assertIn("SDL_GAMEPAD_BUTTON_DPAD_DOWN", SOURCE)
        self.assertIn("if (show_modern_graphics_settings) show_modern_graphics_settings = false;", SOURCE)
        self.assertIn("if (!show_modern_graphics_settings && screen == Screen::menu", SOURCE)

    def test_f10_does_not_signal_the_unrecovered_title_handoff(self) -> None:
        title_poll = SOURCE.index("millennium_title_session->poll_console(true)")
        exclusion = SOURCE.rfind("event.key.key != SDLK_F10", 0, title_poll)
        self.assertGreaterEqual(exclusion, 0)


if __name__ == "__main__":
    unittest.main()
