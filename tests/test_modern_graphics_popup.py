"""Guard Project Eon's renderer-only Modern graphics settings overlay."""

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

    def test_every_visible_popup_label_is_explicitly_catalogued_before_rendering(self) -> None:
        """The F10 dialog is launcher chrome, not original in-game prose."""
        start = SOURCE.index("void draw_modern_graphics_popup")
        end = SOURCE.index("bool inside(", start)
        popup = SOURCE[start:end]
        self.assertIn("const eon::Translator& translator", popup)
        self.assertIn("translator.translate(message)", popup)
        for message in (
            "MODERN GRAPHICS SETTINGS",
            "UP/DOWN: SELECT   LEFT/RIGHT: CHANGE   F10: CLOSE",
            "SETTINGS APPLY TO SDL RENDERING ONLY.",
        ):
            with self.subTest(message=message):
                self.assertIn(f'tr("{message}")', popup)
        self.assertIn("tr(names[index])", popup)
        self.assertIn("tr(display_aspect_names.at(settings.aspect_ratio_index))", popup)
        self.assertIn('tr(settings.pixel_reconstruction ? "SCALE2X (MEMORY ONLY)"', popup)
        self.assertIn('tr(settings.smooth_scaling ? "ON" : "OFF")', popup)
        self.assertIn('tr(settings.scanlines ? "ON" : "OFF")', popup)
        self.assertIn('tr(settings.frame ? "ON" : "OFF")', popup)

    def test_popup_controls_only_renderer_options(self) -> None:
        for option in ("output_resolution_index", "aspect_ratio_index", "smooth_scaling", "scanlines", "frame"):
            self.assertIn(option, SOURCE)
        self.assertIn("SDL_SetWindowSize(window, resolution.width, resolution.height)", SOURCE)
        self.assertIn("aspect_viewport", SOURCE)
        self.assertIn("display_aspect_ratios", SOURCE)
        self.assertIn("width / ratio", SOURCE)
        self.assertIn("width = height * ratio", SOURCE)
        self.assertIn("draw_scanlines", SOURCE)
        self.assertIn("draw_modern_surface_frame", SOURCE)

    def test_deuteros_modern_reconstruction_is_cached_per_verified_vm_tick(self) -> None:
        # The host may present several times between 20 ms opening-VM ticks.
        # Reusing the transient source/Scale2x texture avoids regenerating
        # pixels from the same original frame without introducing a disk cache.
        self.assertIn("deuteros_preview_source_tick", SOURCE)
        self.assertIn("deuteros_modern_preview_attempted_tick", SOURCE)
        self.assertIn("deuteros_modern_preview_source_tick", SOURCE)
        self.assertIn("deuteros_opening->ticks()", SOURCE)
        self.assertIn("deuteros_opening->rgba_frame()", SOURCE)
        self.assertIn("reconstruct_rgba_scale2x(*frame", SOURCE)
        self.assertNotIn("deuteros_modern_preview_source_tick = source_tick;\n                }", SOURCE)

    def test_popup_is_modal_for_gamepad_navigation(self) -> None:
        self.assertIn("SDL_GAMEPAD_BUTTON_DPAD_UP", SOURCE)
        self.assertIn("SDL_GAMEPAD_BUTTON_DPAD_DOWN", SOURCE)
        modal = SOURCE.index("if (show_modern_graphics_settings) {")
        self.assertIn("SDL_GAMEPAD_BUTTON_BACK", SOURCE[modal:])
        self.assertIn("                continue;", SOURCE[modal:])
        self.assertLess(modal, SOURCE.index("millennium_title_session->poll_console(true)"))

    def test_f10_does_not_signal_the_unrecovered_title_handoff(self) -> None:
        title_poll = SOURCE.index("millennium_title_session->poll_console(true)")
        f10_guard = SOURCE.rfind("event.key.key == SDLK_F10", 0, title_poll)
        text_input = SOURCE.rfind("event.type == SDL_EVENT_TEXT_INPUT", 0, title_poll)
        self.assertGreaterEqual(f10_guard, 0)
        self.assertGreaterEqual(text_input, 0)
        self.assertLess(f10_guard, text_input)


if __name__ == "__main__":
    unittest.main()
