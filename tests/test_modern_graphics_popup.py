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

    def test_diagnostics_page_is_a_modern_readout_not_a_guest_debugger(self) -> None:
        """F10 diagnostics must remain outside Original media and simulation."""
        start = SOURCE.index("struct ModernRuntimeDiagnostics")
        end = SOURCE.index("std::size_t output_resolution_index_for", start)
        diagnostics = SOURCE[start:end]
        self.assertIn("release_identity", diagnostics)
        self.assertIn("startup_boundary", diagnostics)
        self.assertIn("recovery_boundary_count", diagnostics)
        self.assertIn("trace_admission", diagnostics)
        self.assertIn("sdl_vsync", diagnostics)
        self.assertIn("truncated_identity_hash", diagnostics)
        self.assertNotIn("save", diagnostics)

        popup_start = SOURCE.index("void draw_modern_runtime_diagnostics_popup")
        popup = SOURCE[popup_start:SOURCE.index("bool inside(", popup_start)]
        for label in (
            "MODERN RUNTIME DIAGNOSTICS", "RELEASE IDENTITY",
            "STARTUP BOUNDARY", "RECOVERY MAP BOUNDARIES", "TRACE ADMISSION", "RENDERER SETTINGS",
            "FRAME PACING", "DIAGNOSTICS ARE READ-ONLY; ORIGINAL DATA IS NOT MODIFIED.",
        ):
            with self.subTest(label=label):
                self.assertIn(label, popup)
        self.assertIn("tr(rows[index].first)", popup)
        self.assertIn("SDL VSYNC: ON", popup)
        self.assertIn("SDL VSYNC: OFF", popup)
        self.assertIn("RECOVERY FUNCTION MAP", SOURCE)
        self.assertIn("release_has_recovery_map_entry", SOURCE)
        self.assertIn("startup_boundary_for_release", SOURCE)
        self.assertIn("report_startup_boundary", SOURCE)
        self.assertIn("DECLARATIVE DIAGNOSTICS ONLY; THIS DOES NOT EXECUTE ORIGINAL CODE.", SOURCE)

    def test_function_map_is_paged_read_only_provenance_not_a_hook_table(self) -> None:
        popup_start = SOURCE.index("void draw_recovery_function_map_popup")
        popup = SOURCE[popup_start:SOURCE.index("bool inside(", popup_start)]
        for field in ("id", "profile", "cpu", "source_asset_sha256", "source_offset",
                      "runtime_address", "evidence_level", "uncertainty", "runtime_status"):
            with self.subTest(field=field):
                self.assertIn(f"entry.{field}", popup)
        self.assertIn("rows_per_page = 3", popup)
        self.assertIn("UP/DOWN: PAGE", popup)
        self.assertNotIn("SDL_ShowOpenFileDialog", popup)
        self.assertNotIn("reference_trace", popup)

    def test_diagnostics_page_is_reached_and_dismissed_inside_the_modal(self) -> None:
        self.assertIn('"DEVELOPER DIAGNOSTICS"', SOURCE)
        self.assertIn("show_modern_runtime_diagnostics = true", SOURCE)
        modal = SOURCE.index("if (show_modern_graphics_settings) {")
        modal_block = SOURCE[modal:SOURCE.index("if (event.type == SDL_EVENT_KEY_DOWN && event.key.key == SDLK_ESCAPE)", modal)]
        self.assertIn("if (show_modern_runtime_diagnostics)", modal_block)
        self.assertIn("event.key.key == SDLK_ESCAPE", modal_block)
        self.assertIn("SDL_GAMEPAD_BUTTON_BACK", modal_block)
        self.assertIn("event.type == SDL_EVENT_FINGER_DOWN", modal_block)

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
        self.assertIn("tr(modern_graphics_preset_names.at", popup)
        self.assertIn("tr(display_aspect_names.at(settings.aspect_ratio_index))", popup)
        self.assertIn('tr(settings.pixel_reconstruction ? "SCALE2X (MEMORY ONLY)"', popup)
        self.assertIn('tr(settings.smooth_scaling ? "ON" : "OFF")', popup)
        self.assertIn('tr(settings.scanlines ? "ON" : "OFF")', popup)
        self.assertIn('tr(settings.frame ? "ON" : "OFF")', popup)
        self.assertIn('tr(modern_pack_selected ? "ON" : "CHOOSE…")', popup)

    def test_custom_can_explicitly_choose_a_modern_pack_without_autodiscovery(self) -> None:
        """A native dialog submits one candidate; existing loaders still validate it."""
        self.assertIn("SDL_ShowOpenFileDialog", SOURCE)
        self.assertIn('"Modern asset pack", "eonmodern"', SOURCE)
        self.assertIn("receive_modern_pack_dialog_selection", SOURCE)
        self.assertIn("selected_modern_pack_manifest", SOURCE)
        self.assertIn("request.modern_pack_manifest", SOURCE)
        self.assertIn("filters, 1, nullptr, false", SOURCE)
        self.assertIn("screen != Screen::menu || launcher_page != LauncherPage::profiles", SOURCE)
        self.assertIn("focused_profile_card != 2 || custom_profile_ready", SOURCE)
        self.assertIn("filelist && filelist[0] && !filelist[1]", SOURCE)
        self.assertIn("load_deuteros_amiga_held_opening_modern_sequence", SOURCE)
        self.assertIn("load_millennium_dos_title_modern_surface", SOURCE)

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

    def test_named_presets_are_renderer_only_and_manual_controls_become_custom(self) -> None:
        """Modern profiles must not become a hidden simulation or media mode."""
        for preset in ("clean", "crt", "cinematic", "high_contrast", "custom"):
            with self.subTest(preset=preset):
                self.assertIn(f"ModernGraphicsPreset::{preset}", SOURCE)
        self.assertIn('"GRAPHICS PRESET"', SOURCE)
        self.assertIn("apply_modern_graphics_preset", SOURCE)
        self.assertIn("mark_modern_graphics_custom", SOURCE)
        self.assertIn("cycle_modern_graphics_preset", SOURCE)
        preset_block = SOURCE[SOURCE.index("void apply_modern_graphics_preset"):
                              SOURCE.index("void mark_modern_graphics_custom")]
        for prohibited in ("LaunchRequest", "save", "input", "media"):
            with self.subTest(prohibited=prohibited):
                self.assertNotIn(prohibited, preset_block)

    def test_cinematic_and_high_contrast_overlays_are_modern_only(self) -> None:
        overlay_start = SOURCE.index("void draw_modern_preset_overlay")
        overlay = SOURCE[overlay_start:SOURCE.index("// This report is intentionally shared", overlay_start)]
        self.assertIn("ModernGraphicsPreset::cinematic", overlay)
        self.assertIn("ModernGraphicsPreset::high_contrast", overlay)
        self.assertIn("original texture bytes", overlay)
        self.assertGreaterEqual(SOURCE.count(
            "if (modern) draw_modern_preset_overlay(renderer, preview_bounds,"), 2)

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

    def test_deuteros_external_opening_pack_is_modern_only_and_tick_bound(self) -> None:
        loader = SOURCE.index("const auto load_deuteros_external_modern_sequence")
        renderer = SOURCE.index("SDL_Texture* texture = preview_texture;", loader)
        loader_block = SOURCE[loader:renderer]
        self.assertIn("request.presentation != eon::Presentation::modern", loader_block)
        self.assertIn("active_platform != eon::Platform::amiga", loader_block)
        self.assertIn("candidate.language == *active_release_language", loader_block)
        self.assertIn("load_deuteros_amiga_held_opening_modern_sequence", loader_block)
        refresh = SOURCE.index("const auto refresh_deuteros_external_modern_texture", loader)
        refresh_block = SOURCE[refresh:renderer]
        self.assertIn("source_tick > eon::deuteros_amiga_held_opening_frame_count", refresh_block)
        self.assertIn("source_tick == eon::deuteros_amiga_held_opening_frame_count && !title_handed_off", refresh_block)
        self.assertIn("load_deuteros_amiga_held_opening_modern_frame", refresh_block)
        self.assertIn("deuteros_external_modern_sequence.reset()", refresh_block)
        render_block = SOURCE[renderer:SOURCE.index("SDL_SetTextureScaleMode(texture", renderer)]
        self.assertIn("if (modern)", render_block)
        self.assertIn("refresh_deuteros_external_modern_texture(source_tick", render_block)
        self.assertLess(render_block.index("refresh_deuteros_external_modern_texture"),
                        render_block.index("reconstruct_rgba_scale2x(*frame"))

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
