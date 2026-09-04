"""Guard Project Eon's renderer-only Modern graphics settings overlay."""

from pathlib import Path
import unittest


ROOT = Path(__file__).resolve().parents[1]
SOURCE = (ROOT / "src" / "main.cpp").read_text(encoding="utf-8")
OPENING_RUNNER_SOURCE = (ROOT / "src" / "engine" / "deuteros_amiga_opening_runner.cpp").read_text(
    encoding="utf-8")
OPENING_RUNNER_HEADER = (ROOT / "src" / "engine" / "deuteros_amiga_opening_runner.hpp").read_text(
    encoding="utf-8")


class ModernGraphicsPopupTests(unittest.TestCase):
    def test_f10_opens_renderer_only_settings(self) -> None:
        self.assertIn("SDLK_F10", SOURCE)
        self.assertIn("show_modern_graphics_settings", SOURCE)
        self.assertIn("MODERN GRAPHICS SETTINGS", SOURCE)
        self.assertIn("ORIGINAL DISPLAY SETTINGS", SOURCE)
        self.assertIn("original_display_option_count", SOURCE)
        self.assertIn("SETTINGS APPLY TO SDL RENDERING ONLY.", SOURCE)

    def test_original_f10_is_limited_to_display_only_options(self) -> None:
        popup_start = SOURCE.index("void draw_modern_graphics_popup")
        popup = SOURCE[popup_start:SOURCE.index("bool inside(", popup_start)]
        self.assertIn("modern ? modern_graphics_option_count : original_display_option_count", popup)
        self.assertIn("Original permits only output viewport choices", popup)
        f10 = SOURCE.index("event.key.key == SDLK_F10")
        modal = SOURCE.index("if (show_modern_graphics_settings) {", f10)
        f10_block = SOURCE[f10:modal]
        self.assertIn("display-only controls", f10_block)
        self.assertNotIn("request.presentation != eon::Presentation::modern) continue", f10_block)

    def test_diagnostics_page_is_a_modern_readout_not_a_guest_debugger(self) -> None:
        """F10 diagnostics must remain outside Original media and simulation."""
        start = SOURCE.index("struct ModernRuntimeDiagnostics")
        end = SOURCE.index("std::size_t output_resolution_index_for", start)
        diagnostics = SOURCE[start:end]
        self.assertIn("release_identity", diagnostics)
        self.assertIn("recovery_coverage", diagnostics)
        self.assertIn("startup_boundary", diagnostics)
        self.assertIn("recovery_boundary_count", diagnostics)
        self.assertIn("trace_admission", diagnostics)
        self.assertIn("modern_pack", diagnostics)
        self.assertIn("modern_pack_targets", diagnostics)
        self.assertNotIn("sdl_vsync", diagnostics)
        self.assertIn("truncated_identity_hash", diagnostics)
        self.assertNotIn("save", diagnostics)

    def test_diagnostics_reject_forged_release_metadata_before_map_composition(self) -> None:
        source = (ROOT / "src" / "data" / "runtime_diagnostics.cpp").read_text(encoding="utf-8")
        self.assertIn("is_recognised_release_identity(release)", source)
        self.assertIn("exact recognised manifest identity", source)
        self.assertIn("do not reopen user media", source)

        popup_start = SOURCE.index("void draw_modern_runtime_diagnostics_popup")
        popup = SOURCE[popup_start:SOURCE.index("bool inside(", popup_start)]
        for label in (
            "MODERN RUNTIME DIAGNOSTICS", "RELEASE IDENTITY",
            "RUNTIME ADMISSION", "LIFECYCLE STATE", "SESSION ADAPTER", "SESSION BOUNDARY", "SESSION CAPABILITIES",
            "RECOVERY COVERAGE", "STARTUP BOUNDARY", "RECOVERY MAP BOUNDARIES", "TRACE ADMISSION", "MODERN PACK",
            "PACK RENDER TARGETS", "RENDERER SETTINGS",
            "FRAME PACING", "DIAGNOSTICS ARE READ-ONLY; ORIGINAL DATA IS NOT MODIFIED.",
        ):
            with self.subTest(label=label):
                self.assertIn(label, popup)
        self.assertIn("tr(rows[index].first)", popup)
        self.assertIn("render_pacing_names", popup)
        self.assertIn("RECOVERY FUNCTION MAP", SOURCE)
        self.assertIn("runtime_diagnostics_for_release(*release)", SOURCE)
        self.assertIn("runtime.millennium_dos_owned_function_diagnostics()", SOURCE)
        self.assertIn("millennium_dos_owned_function", SOURCE)
        self.assertIn("diagnostics.millennium_dos_owned_function", popup)
        self.assertIn("runtime.deuteros_amiga_title_dependency_chain_checkpoint()", SOURCE)
        self.assertIn("deuteros_amiga_title_dependency_chain", SOURCE)
        self.assertIn("diagnostics.deuteros_amiga_title_dependency_chain", popup)
        self.assertIn("const auto runtime_view = runtime.snapshot();", SOURCE)
        self.assertIn("native_session_state_label(runtime_view.state)", SOURCE)
        self.assertIn("tr(diagnostics.lifecycle_state)", popup)
        self.assertIn("tr(diagnostics.session_adapter)", popup)
        self.assertIn("tr(diagnostics.session_boundary)", popup)
        self.assertIn("runtime_session_kind_label(session->kind)", SOURCE)
        self.assertIn("runtime_session_boundary_label(session->boundary)", SOURCE)
        self.assertIn('"MODE="', SOURCE)
        self.assertIn('" / DECODED_PRESENTATION="', SOURCE)
        self.assertIn('request.presentation == eon::Presentation::original', SOURCE)
        self.assertIn('" / INPUT="', SOURCE)
        self.assertIn("selected_modern_pack_preflight", SOURCE)
        self.assertIn("modern_pack_admission", SOURCE)
        self.assertIn("modern_pack_renderer_targets_summary", SOURCE)
        self.assertIn("truncated_diagnostic_value", SOURCE)
        self.assertIn("runtime_diagnostics_for_release(release)", SOURCE)
        self.assertIn("report_startup_boundary", SOURCE)
        self.assertIn("DECLARATIVE DIAGNOSTICS ONLY; THIS DOES NOT EXECUTE ORIGINAL CODE.", SOURCE)
        self.assertNotIn("manifest_path", popup)
        self.assertNotIn("preflight.error", popup)
        self.assertNotIn("load_millennium", popup)

    def test_function_map_is_paged_read_only_provenance_not_a_hook_table(self) -> None:
        popup_start = SOURCE.index("void draw_recovery_function_map_popup")
        popup = SOURCE[popup_start:SOURCE.index("bool inside(", popup_start)]
        for field in ("id", "profile", "cpu", "source_asset_sha256", "source_span_sha256", "source_offset",
                      "runtime_address", "evidence_level", "uncertainty", "runtime_status"):
            with self.subTest(field=field):
                self.assertIn(f"entry.{field}", popup)
        self.assertIn("rows_per_page = 3", popup)
        self.assertIn("UP/DOWN: PAGE", popup)
        self.assertIn("truncated_diagnostic_value(entry.runtime_status + \"; \" + entry.uncertainty, 92U)", popup)
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
        self.assertIn("modal_pointer_position(event)", modal_block)

    def test_modal_pointer_route_supports_mouse_and_touch_without_game_input_leakage(self) -> None:
        """Custom's card click must remain usable after its F10 modal opens."""
        start = SOURCE.index("const auto modal_pointer_position")
        end = SOURCE.index("std::optional<std::uint64_t> last_capped_present_ns", start)
        modal_pointer = SOURCE[start:end]
        self.assertIn("SDL_EVENT_MOUSE_BUTTON_DOWN", modal_pointer)
        self.assertIn("event.button.which != SDL_TOUCH_MOUSEID", modal_pointer)
        self.assertIn("SDL_EVENT_FINGER_DOWN", modal_pointer)
        self.assertIn("SDL_RenderCoordinatesFromWindow", modal_pointer)
        self.assertIn("handle_modal_pointer_down", modal_pointer)
        self.assertIn("visible_graphics_option_count()", modal_pointer)
        self.assertIn("show_recovery_function_map = true", modal_pointer)
        self.assertIn("recovery_function_map_page_count", modal_pointer)
        self.assertIn("close_modern_graphics_settings()", modal_pointer)
        self.assertIn("no\n        // route to runtime input, original media, or session state", modal_pointer)

    def test_every_visible_popup_label_is_explicitly_catalogued_before_rendering(self) -> None:
        """The F10 dialog is launcher chrome, not original in-game prose."""
        start = SOURCE.index("void draw_modern_graphics_popup")
        end = SOURCE.index("bool inside(", start)
        popup = SOURCE[start:end]
        self.assertIn("const eon::Translator& translator", popup)
        self.assertIn("translator.translate(message)", popup)
        for message in (
            "MODERN GRAPHICS SETTINGS",
            "ORIGINAL DISPLAY SETTINGS",
            "UP/DOWN: SELECT   LEFT/RIGHT: CHANGE   F10: CLOSE",
            "TOUCH: TAP ROW TO CHANGE   TAP OUTSIDE TO CLOSE",
            "SETTINGS APPLY TO SDL RENDERING ONLY.",
        ):
            with self.subTest(message=message):
                self.assertIn(f'tr("{message}")', popup)
        self.assertIn("tr(modern ? names[index] : original_names[index])", popup)
        self.assertIn("tr(modern_graphics_preset_names.at", popup)
        self.assertIn('tr("CODE IMAGES")', SOURCE)
        self.assertIn('tr("EXCLUDED")', SOURCE)
        self.assertIn('tr("ACTIVE")', SOURCE)
        self.assertIn("tr(display_aspect_names.at(settings.aspect_ratio_index))", popup)
        self.assertIn("tr(render_pacing_names.at", popup)
        self.assertIn('PixelReconstruction::scale4x ? "SCALE4X (MEMORY ONLY)"', popup)
        self.assertIn('tr(settings.smooth_scaling ? "ON" : "OFF")', popup)
        self.assertIn('tr(settings.scanlines ? "ON" : "OFF")', popup)
        self.assertIn('tr(settings.frame ? "ON" : "OFF")', popup)
        self.assertIn('ModernPackAdmission::ready ? "READY"', popup)
        self.assertIn('ModernPackAdmission::rejected ? "REJECTED" : "CHOOSE…"', popup)

    def test_custom_can_explicitly_choose_a_modern_pack_without_autodiscovery(self) -> None:
        """A native dialog preflights one candidate before loaders revalidate it."""
        self.assertIn("SDL_ShowOpenFileDialog", SOURCE)
        self.assertIn('mailbox.filter_label = tr("MODERN ASSET PACK")', SOURCE)
        self.assertIn('mailbox.filter = {mailbox.filter_label.c_str(), "eonmodern"}', SOURCE)
        self.assertIn("&mailbox.filter, 1, nullptr, false", SOURCE)
        self.assertNotIn('"Modern asset pack", "eonmodern"', SOURCE)
        self.assertIn("receive_modern_pack_dialog_selection", SOURCE)
        self.assertIn("selected_modern_pack_manifest", SOURCE)
        self.assertIn("admit_modern_pack_for_release", SOURCE)
        self.assertIn("preflight_modern_asset_pack", SOURCE)
        self.assertIn("ModernPackAdmission::rejected", SOURCE)
        self.assertIn("request.modern_pack_manifest", SOURCE)
        self.assertIn("screen != Screen::menu || launcher_page != LauncherPage::profiles", SOURCE)
        self.assertIn("focused_profile_card != 2 || custom_profile_ready", SOURCE)
        self.assertIn("filelist && filelist[0] && !filelist[1]", SOURCE)
        self.assertIn("ModernAssetPackPresentationResolver::create", SOURCE)
        self.assertIn("ModernAssetPackPresentationTarget::deuteros_amiga_held_opening", SOURCE)
        self.assertIn("ModernAssetPackPresentationTarget::millennium_dos_title", SOURCE)

    def test_popup_controls_only_renderer_options(self) -> None:
        for option in ("output_resolution_index", "aspect_ratio_index", "render_pacing", "smooth_scaling", "scanlines", "frame"):
            self.assertIn(option, SOURCE)
        self.assertIn("SDL_SetWindowSize(window, resolution.width, resolution.height)", SOURCE)
        self.assertIn("aspect_viewport", SOURCE)
        self.assertIn("display_aspect_ratios", SOURCE)
        self.assertIn("fit_display_aspect_viewport", SOURCE)
        self.assertIn("draw_scanlines", SOURCE)
        self.assertIn("draw_modern_surface_frame", SOURCE)
        self.assertIn("SDL_SetRenderVSync", SOURCE)
        self.assertIn("SDL_DelayPrecise", SOURCE)
        self.assertIn("presentation_period_ns", SOURCE)
        limiter = SOURCE[SOURCE.index("constexpr std::uint64_t presentation_period_ns"):
                         SOURCE.index("SDL_RenderPresent(renderer);", SOURCE.index("constexpr std::uint64_t presentation_period_ns"))]
        self.assertIn("SDL_DelayPrecise", limiter)
        self.assertNotIn("deuteros_opening->tick", limiter)

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

    def test_modern_reconstruction_cache_is_keyed_by_release_source_tick_and_mode(self) -> None:
        # The host may present several times between 20 ms opening-VM ticks.
        # Reusing a texture is safe only when the full renderer source key
        # still matches; F10 must therefore invalidate Scale4x -> Clean even
        # when the original frame/tick has not changed.
        header = (ROOT / "src" / "data" / "modern_pixel_reconstruction.hpp").read_text(encoding="utf-8")
        self.assertIn("struct ModernReconstructionCacheKey", header)
        for field in ("release_sha256", "source_id", "source_tick", "reconstruction"):
            with self.subTest(field=field):
                self.assertIn(field, header)
        self.assertNotIn("filesystem", header)
        self.assertNotIn("SDL_", header)
        self.assertIn("deuteros_preview_source_tick", SOURCE)
        pipeline_header = (ROOT / "src" / "presentation" / "modern_presentation_pipeline.hpp").read_text(encoding="utf-8")
        pipeline_source = (ROOT / "src" / "presentation" / "modern_presentation_pipeline.cpp").read_text(encoding="utf-8")
        self.assertIn("ModernPresentationPipeline deuteros_modern_pipeline", SOURCE)
        self.assertIn("ModernPresentationPipeline millennium_modern_pipeline", SOURCE)
        self.assertIn("class ModernPresentationPipeline", pipeline_header)
        self.assertIn("ModernReconstructedSurface", pipeline_header)
        self.assertIn("reconstruct_rgba_scale4x", pipeline_source)
        self.assertNotIn("SDL_", pipeline_header + pipeline_source)
        self.assertIn('"millennium.dos.title"', SOURCE)
        self.assertIn('"deuteros.amiga.opening"', SOURCE)
        self.assertIn("millennium_modern_pipeline.matches(requested_key)", SOURCE)
        self.assertIn("deuteros_modern_pipeline.matches(requested_key)", SOURCE)
        self.assertIn("opening->checkpoint.tick", SOURCE)
        self.assertIn("opening->rgba_frame", SOURCE)
        self.assertIn("runtime.deuteros_amiga_opening_presentation()", SOURCE)
        self.assertIn("deuteros_modern_pipeline.resolve(requested_key, *frame", SOURCE)
        self.assertIn("SDL_DestroyTexture(modern_preview_texture)", SOURCE)

    def test_deuteros_external_opening_pack_is_modern_only_and_tick_bound(self) -> None:
        loader = SOURCE.index("const auto load_deuteros_external_modern_sequence")
        renderer = SOURCE.index("SDL_Texture* texture = title_surface", loader)
        loader_block = SOURCE[loader:renderer]
        self.assertIn("request.presentation != eon::Presentation::modern", loader_block)
        self.assertIn("active_platform != eon::Platform::amiga", loader_block)
        self.assertIn("resolve_active_release(eon::Game::deuteros)", loader_block)
        self.assertIn("ModernAssetPackPresentationResolver::create", loader_block)
        refresh = SOURCE.index("const auto refresh_deuteros_external_modern_texture", loader)
        refresh_block = SOURCE[refresh:renderer]
        self.assertIn("source_tick > eon::deuteros_amiga_held_opening_frame_count", refresh_block)
        self.assertIn("source_tick == eon::deuteros_amiga_held_opening_frame_count && !title_handed_off", refresh_block)
        self.assertIn("deuteros_external_modern_resolver->resolve(source_tick, title_handed_off)", refresh_block)
        self.assertIn("deuteros_external_modern_resolver.reset()", refresh_block)
        render_block = SOURCE[renderer:SOURCE.index("SDL_SetTextureScaleMode(texture", renderer)]
        self.assertIn("if (modern && !title_surface)", render_block)
        self.assertIn("refresh_deuteros_external_modern_texture(source_tick", render_block)
        self.assertLess(render_block.index("refresh_deuteros_external_modern_texture"),
                        render_block.index("deuteros_modern_pipeline.resolve(requested_key, *frame"))

    def test_sparse_deuteros_title_surface_never_fabricates_missing_pixels(self) -> None:
        query = SOURCE.index("runtime.deuteros_amiga_title_planar_surface()")
        renderer = SOURCE.index("SDL_Texture* texture = title_surface", query)
        block = SOURCE[query:SOURCE.index("draw_text(renderer, 64, 580", renderer)]
        self.assertIn("SDL_BLENDMODE_BLEND", block)
        self.assertIn("title_surface->rgba.data()", block)
        self.assertIn("title_surface->decoded_pixel_count", block)
        self.assertIn("? deuteros_title_planar_texture : preview_texture", block)
        self.assertIn("if (modern && !title_surface)", block)
        reset = SOURCE.index("const auto reset_deuteros_runtime")
        reset_block = SOURCE[reset:SOURCE.index("const auto reset_active_runtime", reset)]
        self.assertIn("SDL_DestroyTexture(deuteros_title_planar_texture)", reset_block)
        self.assertIn("deuteros_title_planar_generation.reset()", reset_block)
        self.assertIn("deuteros_title_planar_memory_checksum.reset()", reset_block)

    def test_deuteros_title_handoff_stops_host_vm_scheduling(self) -> None:
        """The retained frame is presentation evidence, never a fake title VM."""
        # Timing belongs to the native session's SDL-free runner; main only
        # consumes its release-bound event stream and clears preview audio at
        # handoff.
        self.assertIn("scheduler_period_ms = 20", OPENING_RUNNER_HEADER)
        self.assertIn("maximum_catch_up_ticks", OPENING_RUNNER_HEADER)
        self.assertIn("tick_source_ ? tick_source_()", OPENING_RUNNER_SOURCE)
        self.assertIn("result.title_handoff", OPENING_RUNNER_SOURCE)
        scheduler = SOURCE.rindex("runtime.advance(SDL_GetTicks())")
        renderer = SOURCE.index("const bool modern", scheduler)
        scheduler_block = SOURCE[scheduler:renderer]
        self.assertIn("if (events.title_handoff)", scheduler_block)
        self.assertIn("SDL_ClearAudioStream(deuteros_audio_stream)", scheduler_block)
        self.assertIn("TITLE-STAGE EXECUTION IS NOT YET RECOVERED", SOURCE)

    def test_deuteros_title_panel_shows_only_session_provenance_before_exec(self) -> None:
        panel = SOURCE.index("const auto title_stage = runtime.deuteros_amiga_title_stage_boundary();")
        palette = SOURCE.index("graphics_setup_palette", panel)
        handoff_panel = SOURCE[panel:palette]
        self.assertIn("title_stage->entry_prefix_state", handoff_panel)
        self.assertIn("title_stage->exec_prelude", handoff_panel)
        self.assertIn('prefix_provenance << "0x"', handoff_panel)
        self.assertIn("unresolved Exec read", handoff_panel)
        self.assertNotIn("title_stage->tick", handoff_panel)
        self.assertNotIn("deuteros_opening->", handoff_panel)

    def test_deuteros_opening_panel_uses_lifecycle_gated_vm_snapshot(self) -> None:
        opening_panel = SOURCE.index('tr("AUTHENTIC AMIGA OPENING - ORIGINAL CHANNEL PROGRAM + PALETTE")')
        handoff = SOURCE.index("if (deuteros_title_resource)", opening_panel)
        panel = SOURCE[opening_panel:handoff]
        for observable in (
            "opening->checkpoint.tick",
            "opening->checkpoint.vblank_counter",
            "opening->palette_index",
            "opening->active_channel_count",
            "opening->checkpoint.input_gate",
        ):
            with self.subTest(observable=observable):
                self.assertIn(observable, panel)
        self.assertIn("Machine-state telemetry", panel)
        self.assertNotIn("deuteros_opening->", panel)

    def test_popup_is_modal_for_gamepad_navigation(self) -> None:
        self.assertIn("SDL_GAMEPAD_BUTTON_DPAD_UP", SOURCE)
        self.assertIn("SDL_GAMEPAD_BUTTON_DPAD_DOWN", SOURCE)
        modal = SOURCE.index("if (show_modern_graphics_settings) {")
        self.assertIn("SDL_GAMEPAD_BUTTON_BACK", SOURCE[modal:])
        self.assertIn("                continue;", SOURCE[modal:])
        self.assertLess(modal, SOURCE.index("RuntimeInputObservation::available_character()"))

    def test_popup_makes_custom_settings_usable_with_touch_without_game_input(self) -> None:
        """A Custom card opened by touch must be dismissible on an iPad."""
        pointer = SOURCE.index("const auto modal_pointer_position")
        handler = SOURCE[pointer:SOURCE.index("std::optional<std::uint64_t> last_capped_present_ns", pointer)]
        self.assertIn("SDL_EVENT_FINGER_DOWN", handler)
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
        runtime_host_source = (ROOT / "src" / "engine" / "runtime_host.cpp").read_text(encoding="utf-8")
        self.assertIn("RuntimeInputObservation::opening_input_held(false)", runtime_host_source)
        self.assertIn("runtime.set_input_suppressed(true);", SOURCE)
        self.assertIn("runtime.advance(SDL_GetTicks())", SOURCE)
        self.assertIn("tick_source_ ? tick_source_()", OPENING_RUNNER_SOURCE)
        self.assertLess(f10_guard, modal)

    def test_f10_does_not_signal_the_unrecovered_title_handoff(self) -> None:
        title_poll = SOURCE.index("RuntimeInputObservation::available_character()")
        f10_guard = SOURCE.rfind("event.key.key == SDLK_F10", 0, title_poll)
        text_input = SOURCE.rfind("event.type == SDL_EVENT_TEXT_INPUT", 0, title_poll)
        self.assertGreaterEqual(f10_guard, 0)
        self.assertGreaterEqual(text_input, 0)
        self.assertLess(f10_guard, text_input)


if __name__ == "__main__":
    unittest.main()
