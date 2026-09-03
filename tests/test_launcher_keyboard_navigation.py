"""Guard the staged card-launch and modal-input accessibility contracts."""

from pathlib import Path
import unittest


ROOT = Path(__file__).resolve().parents[1]
SOURCE = (ROOT / "src" / "main.cpp").read_text(encoding="utf-8")
ROUTE_HEADER = (ROOT / "src" / "launcher.hpp").read_text(encoding="utf-8")
ROUTE_SOURCE = (ROOT / "src" / "launcher.cpp").read_text(encoding="utf-8")
RUNTIME_SOURCE = (ROOT / "src" / "engine" / "release_runtime.cpp").read_text(encoding="utf-8")
RUNTIME_HEADER = (ROOT / "src" / "engine" / "release_runtime.hpp").read_text(encoding="utf-8")
MENU_RUNTIME_HEADER = (ROOT / "src" / "engine" / "menu_runtime_launch.hpp").read_text(encoding="utf-8")
MENU_RUNTIME_SOURCE = (ROOT / "src" / "engine" / "menu_runtime_launch.cpp").read_text(encoding="utf-8")
OPENING_RUNNER_HEADER = (ROOT / "src" / "engine" / "deuteros_amiga_opening_runner.hpp").read_text(encoding="utf-8")
OPENING_RUNNER_SOURCE = (ROOT / "src" / "engine" / "deuteros_amiga_opening_runner.cpp").read_text(encoding="utf-8")
class LauncherKeyboardNavigationTests(unittest.TestCase):
    def test_menu_has_explicit_game_platform_release_and_profile_pages(self) -> None:
        self.assertIn("enum class LauncherPage { games, platforms, releases, profiles }", ROUTE_HEADER)
        self.assertIn("struct LauncherRouteState", ROUTE_HEADER)
        self.assertIn("platform_cards_for_game", SOURCE)
        self.assertIn("eon::supported_platforms(game)", SOURCE)
        self.assertIn("std::array<ProfileCard, 3>", SOURCE)
        self.assertIn("LauncherPage::platforms", SOURCE)
        self.assertIn("LauncherPage::releases", SOURCE)
        self.assertIn("LauncherPage::profiles", SOURCE)

    def test_english_defaults_only_for_a_unique_outer_release(self) -> None:
        self.assertIn("available_release_identities", SOURCE)
        self.assertIn("select_available_release_sha256", ROUTE_SOURCE)
        self.assertIn("if (english.size() == 1)", ROUTE_SOURCE)
        self.assertIn("page = release_sha256 ? LauncherPage::profiles : LauncherPage::releases", ROUTE_SOURCE)
        self.assertIn("LauncherInteractionController::activate", ROUTE_SOURCE)

    def test_platform_cards_are_hash_verified_and_disabled_when_missing(self) -> None:
        self.assertIn("eon::platform_card_status(releases, game, card.platform)", SOURCE)
        self.assertIn("eon::platform_card_selectable(status)", SOURCE)
        self.assertIn("UNAVAILABLE PLATFORM CARDS CANNOT START A GAME", SOURCE)
        self.assertIn("if (!platform_card_selectable(platform_card_status", ROUTE_SOURCE)
        self.assertIn("RELEASE SELECTION REQUIRED", SOURCE)
        self.assertIn("platform_coverage(game, card.platform)", SOURCE)
        self.assertIn("tr(eon::name(eon::platform_coverage(game, card.platform)))", SOURCE)
        self.assertIn("VERIFIED ORIGINAL DATA", SOURCE)
        self.assertIn("session.choose_platform(releases, platforms[focus.platform])", ROUTE_SOURCE)

    def test_card_focus_is_bounded_in_the_shared_launcher_core(self) -> None:
        self.assertIn("struct LauncherCardFocus", ROUTE_HEADER)
        self.assertIn("void move(LauncherPage page, std::size_t count, int direction)", ROUTE_HEADER)
        self.assertIn("if (count == 0 || direction == 0) return;", ROUTE_SOURCE)
        self.assertIn("reset_after_game_change", ROUTE_SOURCE)
        self.assertIn("struct LauncherInteractionController", ROUTE_HEADER)
        self.assertIn("focus.move(page, card_count_for(session, releases), direction)", ROUTE_SOURCE)
        self.assertIn("move_launcher_cards", SOURCE)

    def test_platform_cards_are_game_specific_before_media_availability_is_shown(self) -> None:
        # Deuteros supports Amiga and Atari ST. A missing archive must render
        # as unavailable on one of those cards, not create a fictitious DOS
        # target labelled as missing data.
        cards = SOURCE[SOURCE.index("const auto platform_cards_for_game"):
                       SOURCE.index("const auto focus_active_platform_card")]
        self.assertIn("eon::supported_platforms(game)", cards)
        self.assertIn("platform_card_templates", cards)
        self.assertIn("unsupported platform is never misrepresented as absent user media", SOURCE)

    def test_atari_media_scope_never_implies_a_physical_dump_fallback(self) -> None:
        self.assertIn("absent from this verified outer release", SOURCE)
        self.assertIn("no physical-media fallback or substitution", SOURCE)

    def test_deuteros_atari_bootstrap_panel_uses_only_admitted_static_evidence(self) -> None:
        """The Atari launch panel must stop at the verified boot boundary."""
        panel_start = SOURCE.index('tr("VERIFIED DEUTEROS ATARI ST MEDIA - PROTECTED BOOT CHAIN ONLY")')
        panel_end = SOURCE.index('} else if (selected == eon::Game::deuteros && preview_texture', panel_start)
        panel = SOURCE[panel_start:panel_end]
        self.assertIn("runtime.deuteros_atari_bootstrap_presentation()", panel)
        self.assertIn("if (atari_bootstrap)", panel)
        self.assertIn("atari_bootstrap->first_stage_disk_offset", panel)
        self.assertIn("atari_bootstrap->copy_execution", panel)
        self.assertIn("atari_bootstrap->entry_execution", panel)
        self.assertIn("atari_bootstrap->checkpoint.first_stage_sha256", panel)
        self.assertIn("atari_bootstrap->checkpoint.second_stage_sha256", panel)
        self.assertIn('tr("STATIC BOOT EVIDENCE ONLY — NO XBIOS, RAW READ, STATE SELECTION, TITLE, OR GAMEPLAY.")', panel)
        self.assertNotIn("state_selection_layout()", panel)
        self.assertNotIn("raw_reader_call_layout()", panel)

    def test_ambiguous_or_missing_platform_cards_cannot_start_a_game(self) -> None:
        self.assertIn("release_is_selected()", ROUTE_SOURCE)
        self.assertIn("resolve_launch_request_identity(*candidate, releases)", ROUTE_SOURCE)
        self.assertIn("admit_runtime_launch", RUNTIME_SOURCE)
        self.assertIn("no scan-order fallback was selected", SOURCE)

    def test_release_cards_carry_exact_outer_identity(self) -> None:
        cards = SOURCE[SOURCE.index("struct ReleaseLanguageCard"):
                       SOURCE.index("enum class ProfileChoice")]
        self.assertIn("std::size_t identity_index", cards)
        self.assertIn("std::string sha256", cards)
        self.assertIn("available_release_identities", SOURCE)
        self.assertIn("session.choose_release(releases, identities[focus.release].sha256)", ROUTE_SOURCE)
        self.assertIn("release_sha256 = release->sha256", ROUTE_SOURCE)
        self.assertIn("truncated_identity_hash(card.sha256)", SOURCE)
        self.assertIn("Borrowed from the selected generated platform card", SOURCE)
        self.assertIn("SDL_RenderTexture(renderer, card.texture", SOURCE)

    def test_release_card_pages_preserve_global_identity_indices(self) -> None:
        # A future manifest can contain more than the two current Millennium
        # DOS identities. Page-local mouse/touch positions must never select
        # a different outer archive after the focused card crosses a page.
        release_cards = SOURCE[SOURCE.index("const auto release_language_cards"):
                               SOURCE.index("const auto focus_menu_card")]
        self.assertIn("release_card_page_for_focus", ROUTE_HEADER)
        self.assertIn("constexpr std::size_t cards_per_page = 4", ROUTE_SOURCE)
        self.assertIn("const auto page = eon::release_card_page_for_focus", release_cards)
        self.assertIn("const auto identity_index = page.first_identity + visible_index", release_cards)
        self.assertIn("cards_for_platform.push_back({identity_index", release_cards)
        self.assertIn("two-by-two grid", release_cards)
        pointer_route = SOURCE[SOURCE.index("const auto handle_menu_pointer_down"):
                               SOURCE.index("if (screen == Screen::launching")]
        self.assertIn("activate_launcher_card(card.identity_index)", pointer_route)
        self.assertIn("card.identity_index == static_cast<std::size_t>(focused_release_card)", SOURCE)

    def test_touch_and_mouse_have_visible_back_and_release_page_controls(self) -> None:
        self.assertIn("bool page_releases(const std::vector<ReleaseArchive>& releases, int direction)", ROUTE_HEADER)
        self.assertIn("LauncherInteractionController::page_releases", ROUTE_SOURCE)
        self.assertIn("next_page * 4U", ROUTE_SOURCE)
        self.assertIn("launcher_back_bounds", SOURCE)
        self.assertIn("release_page_previous_bounds", SOURCE)
        self.assertIn("release_page_next_bounds", SOURCE)
        pointer_route = SOURCE[SOURCE.index("const auto handle_menu_pointer_down"):
                               SOURCE.index("if (screen == Screen::launching")]
        self.assertLess(pointer_route.index("inside(launcher_back_bounds"),
                        pointer_route.index("activate_launcher_card(card.identity_index)"))
        self.assertIn("page_release_cards(-1)", pointer_route)
        self.assertIn("page_release_cards(1)", pointer_route)
        self.assertIn('"<<"', SOURCE)

    def test_menu_launcher_locale_is_separate_from_original_release_language(self) -> None:
        self.assertIn("supported_launcher_languages", (ROOT / "src" / "i18n.hpp").read_text(encoding="utf-8"))
        self.assertIn("cycle_launcher_language", SOURCE)
        self.assertIn("event.key.key == SDLK_L", SOURCE)
        self.assertIn("launcher_language_bounds", SOURCE)
        self.assertIn('tr("LANGUAGE")', SOURCE)
        self.assertIn("launcher_language_autonym(request.language)", SOURCE)
        self.assertIn("SDL_GAMEPAD_BUTTON_LEFT_SHOULDER", SOURCE)
        self.assertIn("SDL_GAMEPAD_BUTTON_RIGHT_SHOULDER", SOURCE)
        self.assertIn("translator = eon::Translator::from_language(request.language", SOURCE)
        locale = SOURCE[SOURCE.index("const auto cycle_launcher_language"):
                        SOURCE.index("const auto handle_menu_pointer_down")]
        self.assertNotIn("launcher_route.release_language =", locale)
        self.assertNotIn("reset_active_runtime", locale)

    def test_incremental_scan_revokes_newly_ambiguous_automatic_release(self) -> None:
        self.assertIn("bool release_explicit = false", ROUTE_HEADER)
        self.assertIn("bool LauncherRouteState::reconcile_releases", ROUTE_SOURCE)
        self.assertIn("!release_explicit && identities.size() > 1", ROUTE_SOURCE)
        self.assertIn("page = LauncherPage::releases", ROUTE_SOURCE)
        self.assertIn("session.reconcile_releases(releases)", ROUTE_SOURCE)
        scanner_update = SOURCE.index("const auto source_before_scan")
        scanner_body = SOURCE[scanner_update:SOURCE.index("if (screen == Screen::launching", scanner_update)]
        self.assertIn("launcher_interaction.synchronize(releases);", scanner_body)
        self.assertIn("apply_launcher_navigation(source_before_scan);", scanner_body)

    def test_runtime_loaders_consume_the_resolved_outer_identity(self) -> None:
        # The launcher must resolve a media identity once before it enters a
        # session. A loader searching the mutable scanner list again could
        # accidentally make an exact release card into a scan-order decision.
        self.assertIn("const auto resolve_active_release", SOURCE)
        for loader in (
            "load_millennium_dos_runtime",
            "load_deuteros_amiga_runtime",
            "load_millennium_atari_runtime",
            "load_millennium_amiga_runtime",
            "load_deuteros_atari_runtime",
        ):
            with self.subTest(loader=loader):
                declaration = RUNTIME_HEADER[RUNTIME_HEADER.index(loader):]
                self.assertIn("const ReleaseArchive& release", declaration)
                self.assertIn("const VerifiedReleaseMedia& media", declaration)
        acquire = RUNTIME_SOURCE[RUNTIME_SOURCE.index("bool ReleaseRuntimeCoordinator::acquire"):
                                 RUNTIME_SOURCE.index("std::optional<DeuterosAmigaVmEvents>")]
        self.assertIn("VerifiedReleaseMedia::open(launch.release)", acquire)
        self.assertNotIn("verify_release_archive(launch.release)", acquire)
        self.assertIn("load_millennium_dos_runtime(*media)", acquire)
        self.assertNotIn("eon::load_deuteros_amiga_runtime(", SOURCE)
        self.assertNotIn("eon::load_millennium_dos_runtime(", SOURCE)
        self.assertNotIn("runtime_coordinator.deuteros_amiga()", SOURCE)
        self.assertIn("runtime.deuteros_amiga_opening_presentation()", SOURCE)
        self.assertIn("runtime.render_deuteros_amiga_opening_audio", SOURCE)
        self.assertIn("runtime.millennium_dos_presentation()", SOURCE)
        self.assertIn("runtime.millennium_dos_startup_input()", SOURCE)
        self.assertNotIn("runtime_coordinator.millennium_dos()", SOURCE)
        self.assertNotIn("runtime_coordinator.millennium_dos_title()", SOURCE)
        self.assertNotIn("runtime_coordinator.millennium_dos_sound_selection()", SOURCE)

    def test_deuteros_opening_handoff_publishes_a_fail_closed_title_stage_session(self) -> None:
        session_header = (ROOT / "src" / "engine" / "runtime_session.hpp").read_text(encoding="utf-8")
        self.assertIn("deuteros_amiga_title_stage", session_header)
        self.assertIn("DEUTEROS AMIGA TITLE STAGE", (ROOT / "src" / "engine" / "runtime_session.cpp").read_text(encoding="utf-8"))
        tick = RUNTIME_SOURCE[RUNTIME_SOURCE.index("tick_deuteros_amiga_opening"):
                              RUNTIME_SOURCE.index("RuntimeLaunchAdmission admit_runtime_launch")]
        self.assertIn("events.title_handoff", tick)
        self.assertIn("RuntimeSessionKind::deuteros_amiga_title_stage", tick)
        self.assertIn("deuteros_amiga_opening_input_held_ = false", tick)

    def test_deuteros_opening_scheduler_is_owned_by_the_native_session(self) -> None:
        self.assertIn("class DeuterosAmigaOpeningRunner", OPENING_RUNNER_HEADER)
        self.assertIn("scheduler_period_ms = 20", OPENING_RUNNER_HEADER)
        self.assertIn("maximum_catch_up_ticks = 4", OPENING_RUNNER_HEADER)
        self.assertIn("using TickSource", OPENING_RUNNER_HEADER)
        self.assertIn("tick_source_ ? tick_source_()", OPENING_RUNNER_SOURCE)
        self.assertNotIn("ReleaseRuntimeCoordinator", OPENING_RUNNER_SOURCE)
        self.assertIn("result.resynchronized = true", OPENING_RUNNER_SOURCE)
        native_header = (ROOT / "src" / "engine" / "native_session_controller.hpp").read_text(encoding="utf-8")
        native_source = (ROOT / "src" / "engine" / "native_session_controller.cpp").read_text(encoding="utf-8")
        self.assertIn("start_deuteros_amiga_opening_scheduler", native_header)
        self.assertIn("advance_deuteros_amiga_opening_scheduler", native_header)
        self.assertIn("deuteros_amiga_opening_runner_", native_header)
        self.assertIn("[this] { return tick_deuteros_amiga_opening(); }", native_source)
        self.assertIn("runtime.advance(SDL_GetTicks())", SOURCE)
        self.assertNotIn("deuteros_opening_runner->advance", SOURCE)

    def test_millennium_terminal_startup_observations_close_input_routing(self) -> None:
        session_header = (ROOT / "src" / "engine" / "runtime_session.hpp").read_text(encoding="utf-8")
        self.assertIn("millennium_dos_sound_driver_boundary", session_header)
        self.assertIn("millennium_dos_title_handoff_boundary", session_header)
        observe = RUNTIME_SOURCE[RUNTIME_SOURCE.index("RuntimeInputDisposition ReleaseRuntimeCoordinator::observe_input"):
                                 RUNTIME_SOURCE.index("tick_deuteros_amiga_opening")]
        self.assertIn("RuntimeSessionKind::millennium_dos_sound_driver_boundary", observe)
        self.assertIn("RuntimeSessionKind::millennium_dos_title_handoff_boundary", observe)
        self.assertIn("if (!active_) return RuntimeInputDisposition::rejected", observe)

    def test_cli_and_card_routes_share_the_final_runtime_admission_gate(self) -> None:
        self.assertIn("struct RuntimeLaunchAdmission", RUNTIME_HEADER)
        self.assertIn("admit_runtime_launch", RUNTIME_SOURCE)
        self.assertIn("struct MenuRuntimeLaunchResult", MENU_RUNTIME_HEADER)
        self.assertIn("struct RuntimeCandidateLaunchResult", MENU_RUNTIME_HEADER)
        self.assertIn("launch_runtime_candidate", MENU_RUNTIME_HEADER)
        self.assertIn("launch_menu_runtime", MENU_RUNTIME_HEADER)
        self.assertIn("const auto candidate = session.launch_request(base)", MENU_RUNTIME_SOURCE)
        self.assertIn("launch_runtime_candidate(candidate, releases, coordinator)", MENU_RUNTIME_SOURCE)
        self.assertIn("coordinator.active()", MENU_RUNTIME_SOURCE)
        self.assertIn("admit_runtime_launch(coordinator, candidate, releases)", MENU_RUNTIME_SOURCE)
        self.assertIn("runtime.launch_direct(launch_candidate, releases)", SOURCE)
        self.assertIn("Native runtime admission rejected the selected verified release", SOURCE)
        self.assertIn("ReleaseRuntimeRejection::launch_identity", SOURCE)
        self.assertIn("runtime.launch_menu(launcher_session, request, releases)", SOURCE)
        launch = SOURCE.index("const auto launch_menu_selection")
        admission = SOURCE.index("runtime.launch_menu", launch)
        self.assertIn("reset_active_runtime();", SOURCE[launch:admission])

    def test_profile_launch_rejections_are_visible_without_media_details(self) -> None:
        self.assertIn("std::string launcher_runtime_admission", SOURCE)
        self.assertIn("launcher_runtime_admission = std::string(", SOURCE)
        self.assertIn("release_runtime_admission_label(runtime.admission())", SOURCE)
        self.assertIn('tr("RUNTIME ADMISSION")', SOURCE)
        self.assertNotIn("launcher_runtime_admission = resolved->release.path", SOURCE)

    def test_returning_to_launcher_resets_the_active_release_runtime(self) -> None:
        # Escape and gamepad Back must not retain an active archive, texture,
        # audio stream, or recovered VM behind a newly visible launcher.
        reset = SOURCE.index("const auto reset_active_runtime")
        self.assertIn("runtime.begin_source_revocation();", SOURCE[reset:reset + 1100])
        self.assertIn("runtime.finish_source_revocation();", SOURCE[reset:reset + 1100])
        self.assertIn("reset_deuteros_runtime();", SOURCE[reset:reset + 1100])
        escape = SOURCE.rfind("if (event.type == SDL_EVENT_KEY_DOWN && event.key.key == SDLK_ESCAPE)")
        self.assertIn("reset_active_runtime();", SOURCE[escape:escape + 450])
        gamepad_back = SOURCE.rfind("event.gbutton.button == SDL_GAMEPAD_BUTTON_BACK")
        self.assertIn("reset_active_runtime();", SOURCE[gamepad_back:gamepad_back + 500])

    def test_runtime_lifecycle_controller_keeps_sdl_borrows_outside_the_engine(self) -> None:
        self.assertIn("class LauncherRuntimeController", MENU_RUNTIME_HEADER)
        self.assertIn("requires_revocation_for", MENU_RUNTIME_HEADER)
        self.assertIn("LauncherRuntimeController(LauncherRuntimeController&&) = delete", MENU_RUNTIME_HEADER)
        self.assertNotIn("SDL_", MENU_RUNTIME_HEADER + MENU_RUNTIME_SOURCE)
        self.assertNotIn("filesystem", MENU_RUNTIME_HEADER + MENU_RUNTIME_SOURCE)
        self.assertNotIn("runtime.coordinator()", SOURCE)
        reset = SOURCE.index("const auto reset_active_runtime")
        reset_body = SOURCE[reset:SOURCE.index("const auto start_millennium_title", reset)]
        self.assertLess(reset_body.index("stop_millennium_title();"), reset_body.index("runtime.begin_source_revocation();"))
        self.assertLess(reset_body.index("discard_millennium_assets();"), reset_body.index("runtime.begin_source_revocation();"))
        self.assertLess(reset_body.index("reset_deuteros_runtime();"), reset_body.index("runtime.begin_source_revocation();"))
        navigation = SOURCE[SOURCE.index("const auto apply_launcher_navigation"):
                            SOURCE.index("const auto activate_launcher_card")]
        self.assertIn("runtime.requires_revocation_for(launcher_interaction.source_identity())", navigation)

    def test_native_session_controller_cannot_borrow_the_release_coordinator(self) -> None:
        controller_header = (ROOT / "src" / "engine" / "native_session_controller.hpp").read_text(encoding="utf-8")
        controller_source = (ROOT / "src" / "engine" / "native_session_controller.cpp").read_text(encoding="utf-8")
        self.assertNotIn("coordinator()", controller_header + controller_source)
        self.assertNotIn("coordinator()", MENU_RUNTIME_HEADER)
        self.assertIn("RuntimeInputDisposition observe_input", MENU_RUNTIME_HEADER)
        self.assertIn("std::optional<RuntimeSessionSnapshot> session_snapshot() const", MENU_RUNTIME_HEADER)

    def test_automatic_verified_platform_also_updates_keyboard_card_focus(self) -> None:
        # If a game has only Amiga/Atari media, selecting its game card must
        # not leave Enter/South-A focused on the disabled DOS card.
        sync = SOURCE.index("const auto focus_active_platform_card")
        game_focus = SOURCE.index("const auto focus_menu_card")
        self.assertLess(sync, game_focus)
        self.assertIn("focus_active_platform_card();", SOURCE[game_focus:game_focus + 900])
        self.assertIn("std::distance(platform_cards.begin(), card)", SOURCE)

    def test_all_launcher_card_labels_and_dynamic_selection_names_are_translated(self) -> None:
        # The launcher owns its card labels and selection panel. Original
        # in-game strings are deliberately out of scope and remain media data.
        self.assertIn('tr(card.title)', SOURCE)
        self.assertIn('tr(card.subtitle)', SOURCE)
        self.assertIn('tr(launcher_game_label(selected))', SOURCE)
        self.assertIn('tr(launcher_platform_label(*active_platform))', SOURCE)
        self.assertNotIn('tr("Game: ") + eon::name(selected)', SOURCE)
        self.assertNotIn('card.bounds.h - 46, card.title);', SOURCE)

    def test_menu_can_choose_an_immutable_original_data_folder_or_archive(self) -> None:
        # The graphical launcher may begin before the default data directory
        # exists, then accepts one explicit folder or one exact original archive
        # through SDL's native pickers. The callback transfers a typed path only;
        # the main thread replaces its bounded read-only scanner and never copies,
        # creates, or unpacks media.
        self.assertIn("SDL_ShowOpenFolderDialog", SOURCE)
        self.assertIn("SDL_ShowOpenFileDialog", SOURCE)
        self.assertIn("receive_original_data_source_dialog_selection", SOURCE)
        self.assertIn('tr("CHOOSE ORIGINAL DATA FOLDER (O)")', SOURCE)
        self.assertIn('tr("CHOOSE ORIGINAL ARCHIVE (A)")', SOURCE)
        self.assertIn("event.key.key == SDLK_O", SOURCE)
        self.assertIn("event.key.key == SDLK_A", SOURCE)

    def test_runtime_coordinator_publishes_only_a_sanitized_session_snapshot(self) -> None:
        runtime_session = (ROOT / "src" / "engine" / "runtime_session.hpp").read_text(encoding="utf-8")
        runtime_source = (ROOT / "src" / "engine" / "release_runtime.cpp").read_text(encoding="utf-8")
        self.assertIn("struct RuntimeSessionSnapshot", runtime_session)
        self.assertIn("RuntimeSessionCapabilities", runtime_session)
        self.assertIn("bool admitted_input = false", runtime_session)
        self.assertNotIn("filesystem", runtime_session)
        self.assertNotIn("SDL_", runtime_session)
        self.assertIn("session_snapshot_ = std::move(session_snapshot)", runtime_source)
        self.assertIn("session_snapshot_.reset()", runtime_source)
        self.assertIn("make_runtime_session_snapshot", runtime_source)

    def test_dos_text_observations_cross_the_coordinator_not_a_local_session(self) -> None:
        runtime_source = (ROOT / "src" / "engine" / "release_runtime.cpp").read_text(encoding="utf-8")
        runtime_header = (ROOT / "src" / "engine" / "runtime_session.hpp").read_text(encoding="utf-8")
        self.assertIn("RuntimeInputDisposition ReleaseRuntimeCoordinator::observe_input", runtime_source)
        self.assertIn("RuntimeInputObservationKind::ascii_character", runtime_source)
        self.assertIn("RuntimeInputObservationKind::character_available", runtime_header)
        self.assertIn("RuntimeInputObservationKind::opening_input_held", runtime_header)
        self.assertIn("RuntimeInputDisposition::rejected", runtime_source)
        self.assertIn("RuntimeInputObservation::ascii(event.text.text[0])", SOURCE)
        self.assertIn("RuntimeInputObservation::available_character()", SOURCE)
        self.assertNotIn("millennium_sound_selection_session->accept_ascii_character", SOURCE)
        self.assertNotIn("millennium_title_session->poll_console(true)", SOURCE)
        self.assertIn("runtime.advance(SDL_GetTicks())", SOURCE)
        self.assertIn("tick_source_ ? tick_source_()", OPENING_RUNNER_SOURCE)
        self.assertIn("RuntimeInputObservation::opening_input_held", SOURCE)
        self.assertIn("OriginalDataSourceDialogKind::directory", SOURCE)
        self.assertIn("OriginalDataSourceDialogKind::archive", SOURCE)
        self.assertIn("eon::classify_original_data_source", SOURCE)
        self.assertIn("eon::OriginalDataSourceKind::directory", SOURCE)
        self.assertIn("eon::OriginalDataSourceKind::archive", SOURCE)
        self.assertIn("request.data_directory_is_default = false", SOURCE)
        self.assertIn("scanner = std::make_unique<eon::ReleaseScanner>", SOURCE)
        self.assertIn("ReleaseScanner advances later in", SOURCE)

    def test_scanner_overlay_uses_the_shared_aggregate_only_snapshot(self) -> None:
        overlay = SOURCE.index("if (show_scanner) {")
        body = SOURCE[overlay:SOURCE.index("        } else {", overlay)]
        self.assertIn("const auto snapshot = scanner->snapshot();", body)
        self.assertIn("scanner_source_text(snapshot.source_kind)", body)
        self.assertIn("scanner_rejections_text(snapshot)", body)
        self.assertIn("scanner_admission_text(snapshot)", body)
        self.assertIn('tr("DATA SOURCE: DIRECTORY")', SOURCE)
        self.assertIn('tr("DATA SOURCE: ARCHIVE")', SOURCE)
        self.assertIn('tr("DATA SOURCE: MISSING")', SOURCE)
        self.assertIn('tr("DATA SOURCE: UNSUPPORTED")', SOURCE)
        self.assertIn('tr("REJECTIONS: SIZE {size}; HASH {hash}; UNREADABLE {unreadable}; LINKS {links}")', SOURCE)
        self.assertIn('tr("VERIFIED RELEASES: {unique}; DUPLICATES: {duplicates}")', SOURCE)

    def test_replacing_data_source_invalidates_every_release_bound_runtime(self) -> None:
        # A new scanner cannot inherit a resolved archive, decoded frames, VM
        # state, queued audio, or a Modern sequence from its predecessor.
        reset = SOURCE.index("const auto reset_active_runtime")
        source_change = SOURCE.index("if (selected_original_data_source && screen == Screen::menu)")
        self.assertLess(reset, source_change)
        reset_body = SOURCE[reset:SOURCE.index("const auto start_millennium_title", reset)]
        for clearing in (
            "runtime.begin_source_revocation();",
            "runtime.finish_source_revocation();",
            "stop_millennium_title();",
            "millennium_game_session.reset();",
            "discard_millennium_assets();",
            "reset_deuteros_runtime();",
        ):
            with self.subTest(clearing=clearing):
                self.assertIn(clearing, reset_body)
        deuteros_reset = SOURCE.index("const auto reset_deuteros_runtime")
        deuteros_body = SOURCE[deuteros_reset:reset]
        for clearing in (
            "SDL_ClearAudioStream(deuteros_audio_stream)",
            "deuteros_title_resource.reset();",
            "discard_deuteros_external_modern_sequence();",
        ):
            with self.subTest(clearing=clearing):
                self.assertIn(clearing, deuteros_body)
        self.assertIn("runtime.begin_source_revocation();", reset_body)
        self.assertIn("runtime.finish_source_revocation();", reset_body)
        self.assertNotIn("deuteros_opening =", deuteros_body)
        switch_body = SOURCE[source_change:SOURCE.index("} else {", source_change)]
        self.assertIn("reset_active_runtime();", switch_body)

    def test_profiles_have_two_runtime_modes_and_a_custom_tuning_route(self) -> None:
        self.assertIn("enum class ProfileChoice { original, modern, custom }", SOURCE)
        self.assertIn("Custom is a deliberate renderer-only configuration route", SOURCE)
        self.assertIn("session.begin_custom();", ROUTE_SOURCE)
        self.assertIn("session.choose_original();", ROUTE_SOURCE)
        self.assertIn("session.choose_modern();", ROUTE_SOURCE)
        self.assertIn("struct LauncherSessionState", (ROOT / "src" / "launcher.hpp").read_text(encoding="utf-8"))
        self.assertIn("custom_profile_ready", SOURCE)
        self.assertIn("CUSTOM SETTINGS READY", SOURCE)

    def test_runtime_shortcuts_do_not_promote_original_to_modern(self) -> None:
        # Original/Modern is decided by the profile cards before launch. F10
        # may expose Original's display-only controls, but must never turn an
        # Original session into a different presentation behind the user's
        # back; F1 is intentionally inert for the same reason.
        f10 = SOURCE.index("event.key.key == SDLK_F10")
        f1 = SOURCE.index("event.key.key == SDLK_F1 && !event.key.repeat")
        f10_block = SOURCE[f10:f1]
        self.assertIn("Original exposes only its two", f10_block)
        self.assertIn("never switches into Modern", f10_block)
        self.assertNotIn("request.presentation = eon::Presentation::modern", f10_block)
        self.assertIn("Presentation is chosen by the profile card before launch.", SOURCE[f1:f1 + 500])

    def test_back_navigation_consumes_one_event_and_moves_one_card_page(self) -> None:
        # A single Escape/Back moves exactly one page and never falls through
        # to the menu handler. Multilingual profiles return to the release
        # card first; a single-language platform returns to platforms.
        self.assertIn("Escape is a single navigation action", SOURCE)
        self.assertIn("Keep Back equivalent to Escape", SOURCE)
        self.assertIn("const auto back_launcher_cards", SOURCE)
        self.assertGreaterEqual(SOURCE.count("back_launcher_cards();"), 2)
        back = SOURCE[SOURCE.index("const auto back_launcher_cards"):
                      SOURCE.index("const auto open_original_data_source_dialog")]
        self.assertIn("launcher_interaction.source_identity();", back)
        self.assertIn("apply_launcher_navigation(before);", back)
        self.assertIn("available_release_identities(releases, game, *platform).size() > 1", ROUTE_SOURCE)

    def test_modern_popup_consumes_events_before_game_or_menu_input(self) -> None:
        modal = SOURCE.index("if (show_modern_graphics_settings) {")
        modal_continue = SOURCE.index("                continue;", modal)
        title_input = SOURCE.index("RuntimeInputObservation::available_character()")
        menu_input = SOURCE.index("LauncherPage::games", modal_continue)
        self.assertLess(modal, modal_continue)
        self.assertLess(modal_continue, title_input)
        self.assertLess(modal_continue, menu_input)

    def test_dos_title_handoff_uses_text_availability_not_raw_keydown(self) -> None:
        # INT 21h/AH=06h branches only on a nonzero console character result.
        # SDL text input is the narrow host availability analogue; a physical
        # key event must not be treated as a made-up DOS character.
        title_poll = SOURCE.index("RuntimeInputObservation::available_character()")
        self.assertIn("SDL_StartTextInput(window)", SOURCE)
        self.assertIn("SDL_StopTextInput(window)", SOURCE)
        text_event = SOURCE.rfind("event.type == SDL_EVENT_TEXT_INPUT", 0, title_poll)
        self.assertGreaterEqual(text_event, 0)
        self.assertIn("event.text.text && event.text.text[0] != '\\0'", SOURCE[text_event:title_poll])
        self.assertNotIn("event.type == SDL_EVENT_KEY_DOWN && !event.key.repeat",
            SOURCE[text_event:title_poll])

    def test_back_or_restart_ends_the_host_title_input_session(self) -> None:
        # A launcher visit is a distinct one-shot DOS title boundary. Going
        # back must dismiss host text input (including a mobile virtual
        # keyboard), and a later start must create a fresh session instead of
        # retaining the former hand-off state.
        stop = SOURCE.index("const auto stop_millennium_title")
        start = SOURCE.index("const auto start_millennium_title")
        self.assertLess(stop, start)
        self.assertIn("SDL_StopTextInput(window)", SOURCE[stop:start])
        self.assertIn("millennium_startup_input.reset();", SOURCE[stop:start])
        self.assertIn("runtime.begin_source_revocation();", SOURCE)
        self.assertIn("runtime.finish_source_revocation();", SOURCE)
        self.assertIn("reset_active_runtime();\n                    screen = Screen::menu;", SOURCE)
        self.assertIn("stop_millennium_title();", SOURCE[SOURCE.index("const auto start_deuteros"):])

    def test_touch_cards_share_the_verified_mouse_admission_route(self) -> None:
        # iPad touch must activate the same game/platform/release/profile
        # cards as a pointer click, without accepting SDL's compatibility
        # touch-mouse event a second time.
        handler = SOURCE.index("const auto handle_menu_pointer_down")
        modal_pointer = SOURCE.index("const auto modal_pointer_position", handler)
        mouse = SOURCE.index("event.type == SDL_EVENT_MOUSE_BUTTON_DOWN", modal_pointer)
        finger = SOURCE.index("event.type == SDL_EVENT_FINGER_DOWN", mouse)
        modal_handler = SOURCE[modal_pointer:SOURCE.index("std::optional<std::uint64_t> last_capped_present_ns", modal_pointer)]
        self.assertIn("SDL_TOUCH_MOUSEID", SOURCE[mouse:finger])
        self.assertIn("SDL_GetWindowSize(window", SOURCE[finger:finger + 700])
        self.assertIn("SDL_RenderCoordinatesFromWindow", SOURCE[finger:finger + 700])
        self.assertIn("handle_modal_pointer_down", modal_handler)
        # The menu's own route stays separate and is reached after the F10
        # modal consumes pointer events.
        menu_mouse = SOURCE.index("if (screen == Screen::menu && event.type == SDL_EVENT_MOUSE_BUTTON_DOWN", finger)
        self.assertIn("handle_menu_pointer_down(x, y);", SOURCE[menu_mouse:menu_mouse + 1100])

    def test_save_inspection_reports_recovered_original_columns(self) -> None:
        # --inspect-save must be useful to preservation work: expose the
        # executable-recovered positional records, while retaining its
        # read-only, non-runtime contract.
        inspection = SOURCE.index("int report_millennium_dos_save_inspection")
        inspection_end = SOURCE.index("enum class Screen", inspection)
        body = SOURCE[inspection:inspection_end]
        self.assertIn("save.state_record(index)", body)
        self.assertIn("] +00=0x", body)
        self.assertIn(" +04=0x", body)
        self.assertIn(" +06=0x", body)
        self.assertIn(" +08=0x", body)
        self.assertIn("never imported into runtime", body)
        self.assertIn("verified English Millennium DOS archive", body)
        self.assertIn("extract_verified_release_asset", body)

    def test_spanish_startup_diagnostics_print_byte_operands_numerically(self) -> None:
        # A uint8_t streams as a character in C++. Preservation output must
        # retain the observed comparison byte (0x02 here), not emit a control
        # character into logs intended for trace comparison.
        marker = SOURCE.index("game_startup_callees.other_compare_value")
        self.assertIn("static_cast<unsigned>", SOURCE[marker - 120:marker + 160])


if __name__ == "__main__":
    unittest.main()
