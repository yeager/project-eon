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
            "TOUCH: TAP ROW TO CHANGE   TAP OUTSIDE TO CLOSE",
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

    def test_deuteros_title_handoff_stops_host_vm_scheduling(self) -> None:
        """The retained frame is presentation evidence, never a fake title VM."""
        scheduler = SOURCE.index("constexpr std::uint64_t scheduler_period_ms = 20")
        renderer = SOURCE.index("const auto source_tick = deuteros_opening->ticks()", scheduler)
        scheduler_block = SOURCE[scheduler:renderer]
        self.assertIn("&& !deuteros_opening->title_handed_off())", scheduler_block)
        self.assertIn("unrecovered Exec/graphics boundary", scheduler_block)
        self.assertIn("SDL_ClearAudioStream(deuteros_audio_stream)", scheduler_block)

    def test_deuteros_title_panel_shows_only_session_provenance_before_exec(self) -> None:
        panel = SOURCE.index("const auto& title_stage = deuteros_opening->title_stage_session();")
        palette = SOURCE.index("graphics_setup_palette_evidence()", panel)
        handoff_panel = SOURCE[panel:palette]
        self.assertIn("title_stage->entry_prefix_state()", handoff_panel)
        self.assertIn("title_stage->exec_prelude()", handoff_panel)
        self.assertIn('prefix_provenance << "0x"', handoff_panel)
        self.assertIn("unresolved Exec read", handoff_panel)
        self.assertNotIn("title_stage->tick", handoff_panel)

    def test_deuteros_opening_panel_uses_raw_vm_observables(self) -> None:
        opening_panel = SOURCE.index('tr("AUTHENTIC AMIGA OPENING - ORIGINAL CHANNEL PROGRAM + PALETTE")')
        handoff = SOURCE.index("if (deuteros_title_resource)", opening_panel)
        panel = SOURCE[opening_panel:handoff]
        for observable in (
            "deuteros_opening->ticks()",
            "deuteros_opening->vblank_counter()",
            "deuteros_opening->palette_index()",
            "deuteros_opening->active_channel_count()",
            "deuteros_opening->input_gate()",
        ):
            with self.subTest(observable=observable):
                self.assertIn(observable, panel)
        self.assertIn("Machine-state telemetry", panel)

    def test_popup_is_modal_for_gamepad_navigation(self) -> None:
        self.assertIn("SDL_GAMEPAD_BUTTON_DPAD_UP", SOURCE)
        self.assertIn("SDL_GAMEPAD_BUTTON_DPAD_DOWN", SOURCE)
        modal = SOURCE.index("if (show_modern_graphics_settings) {")
        self.assertIn("SDL_GAMEPAD_BUTTON_BACK", SOURCE[modal:])
        self.assertIn("                continue;", SOURCE[modal:])
        self.assertLess(modal, SOURCE.index("millennium_title_session->poll_console(true)"))

    def test_popup_makes_custom_settings_usable_with_touch_without_game_input(self) -> None:
        """A Custom card opened by touch must be dismissible on an iPad."""
        modal = SOURCE.index("if (show_modern_graphics_settings) {")
        touch = SOURCE.index("event.type == SDL_EVENT_FINGER_DOWN", modal)
        handler = SOURCE[touch:SOURCE.index("                continue;", touch)]
        self.assertIn("SDL_RenderCoordinatesFromWindow", handler)
        self.assertIn("modern_graphics_popup_bounds", handler)
        self.assertIn("close_modern_graphics_settings();", handler)
        self.assertIn("modern_graphics_settings.focused_option = row;", handler)
        self.assertIn("change_modern_graphics_option(1);", handler)

    def test_f10_modal_cancels_a_held_deuteros_opening_signal(self) -> None:
        """Renderer chrome must not pass an old held key to `$14` behind it."""
        f10_guard = SOURCE.index("event.key.key == SDLK_F10")
        modal = SOURCE.index("if (show_modern_graphics_settings) {", f10_guard)
        f10_block = SOURCE[f10_guard:modal]
        self.assertIn("clear_deuteros_opening_input();", f10_block)
        self.assertIn("deuteros_input_pressed = false;", SOURCE)
        self.assertLess(f10_guard, modal)

    def test_f10_does_not_signal_the_unrecovered_title_handoff(self) -> None:
        title_poll = SOURCE.index("millennium_title_session->poll_console(true)")
        f10_guard = SOURCE.rfind("event.key.key == SDLK_F10", 0, title_poll)
        text_input = SOURCE.rfind("event.type == SDL_EVENT_TEXT_INPUT", 0, title_poll)
        self.assertGreaterEqual(f10_guard, 0)
        self.assertGreaterEqual(text_input, 0)
        self.assertLess(f10_guard, text_input)


if __name__ == "__main__":
    unittest.main()
