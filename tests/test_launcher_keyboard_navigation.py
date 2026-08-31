"""Guard the staged card-launch and modal-input accessibility contracts."""

from pathlib import Path
import unittest


ROOT = Path(__file__).resolve().parents[1]
SOURCE = (ROOT / "src" / "main.cpp").read_text(encoding="utf-8")
ROUTE_HEADER = (ROOT / "src" / "launcher.hpp").read_text(encoding="utf-8")
ROUTE_SOURCE = (ROOT / "src" / "launcher.cpp").read_text(encoding="utf-8")
RUNTIME_SOURCE = (ROOT / "src" / "engine" / "release_runtime.cpp").read_text(encoding="utf-8")
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
        self.assertIn("ATARI BOOTSTRAP ONLY", SOURCE)
        self.assertIn("card.platform == eon::Platform::atari_st", SOURCE)
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

    def test_ambiguous_or_missing_platform_cards_cannot_start_a_game(self) -> None:
        self.assertIn("release_is_selected()", ROUTE_SOURCE)
        self.assertIn("resolve_launch_request_identity(candidate, releases)", ROUTE_SOURCE)
        self.assertIn("no scan-order fallback was selected", SOURCE)

    def test_release_cards_carry_exact_outer_identity(self) -> None:
        cards = SOURCE[SOURCE.index("struct ReleaseLanguageCard"):
                       SOURCE.index("enum class ProfileChoice")]
        self.assertIn("std::string sha256", cards)
        self.assertIn("available_release_identities", SOURCE)
        self.assertIn("session.choose_release(releases, identities[focus.release].sha256)", ROUTE_SOURCE)
        self.assertIn("release_sha256 = release->sha256", ROUTE_SOURCE)
        self.assertIn("truncated_identity_hash(card.sha256)", SOURCE)

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
                start = RUNTIME_SOURCE.index(f"{loader}(")
                signature = RUNTIME_SOURCE[start:RUNTIME_SOURCE.index(") {", start) + 3]
                self.assertIn("const ReleaseArchive& release", signature)
        self.assertIn("eon::load_deuteros_amiga_runtime(*release)", SOURCE)
        self.assertIn("eon::load_millennium_dos_runtime(*release)", SOURCE)

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

    def test_menu_can_choose_an_immutable_original_data_folder(self) -> None:
        # The graphical launcher may begin before the default data directory
        # exists, then accepts one explicit folder through SDL's native picker.
        # The callback transfers a path only; the main thread replaces its
        # bounded read-only scanner and never copies, creates, or unpacks media.
        self.assertIn("SDL_ShowOpenFolderDialog", SOURCE)
        self.assertIn("receive_data_directory_dialog_selection", SOURCE)
        self.assertIn('tr("CHOOSE ORIGINAL DATA FOLDER (O)")', SOURCE)
        self.assertIn("event.key.key == SDLK_O", SOURCE)
        self.assertIn("request.data_directory_is_default = false", SOURCE)
        self.assertIn("scanner = std::make_unique<eon::ReleaseScanner>", SOURCE)
        self.assertIn("ReleaseScanner advances later in", SOURCE)

    def test_replacing_data_source_invalidates_every_release_bound_runtime(self) -> None:
        # A new scanner cannot inherit a resolved archive, decoded frames, VM
        # state, queued audio, or a Modern sequence from its predecessor.
        reset = SOURCE.index("const auto reset_runtime_for_data")
        source_change = SOURCE.index("if (selected_data_directory && screen == Screen::menu)")
        self.assertLess(reset, source_change)
        reset_body = SOURCE[reset:SOURCE.index("const auto start_millennium_title", reset)]
        for clearing in (
            "runtime_coordinator.reset();",
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
            "deuteros_opening.reset();",
            "deuteros_atari_session.reset();",
            "deuteros_title_resource.reset();",
            "discard_deuteros_external_modern_sequence();",
        ):
            with self.subTest(clearing=clearing):
                self.assertIn(clearing, deuteros_body)
        switch_body = SOURCE[source_change:SOURCE.index("} else {", source_change)]
        self.assertIn("reset_runtime_for_data();", switch_body)

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
                      SOURCE.index("const auto open_data_directory_dialog")]
        self.assertIn("clear_modern_pack_admission();", back)
        self.assertIn("discard_millennium_assets();", back)
        self.assertIn("available_release_identities(releases, game, *platform).size() > 1", ROUTE_SOURCE)

    def test_modern_popup_consumes_events_before_game_or_menu_input(self) -> None:
        modal = SOURCE.index("if (show_modern_graphics_settings) {")
        modal_continue = SOURCE.index("                continue;", modal)
        title_input = SOURCE.index("millennium_title_session->poll_console(true)")
        menu_input = SOURCE.index("LauncherPage::games", modal_continue)
        self.assertLess(modal, modal_continue)
        self.assertLess(modal_continue, title_input)
        self.assertLess(modal_continue, menu_input)

    def test_dos_title_handoff_uses_text_availability_not_raw_keydown(self) -> None:
        # INT 21h/AH=06h branches only on a nonzero console character result.
        # SDL text input is the narrow host availability analogue; a physical
        # key event must not be treated as a made-up DOS character.
        title_poll = SOURCE.index("millennium_title_session->poll_console(true)")
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
        self.assertIn("millennium_title_session.reset();", SOURCE[stop:start])
        self.assertIn("stop_millennium_title();\n                    screen = Screen::menu;", SOURCE)
        self.assertIn("stop_millennium_title();", SOURCE[SOURCE.index("const auto start_deuteros"):])

    def test_touch_cards_share_the_verified_mouse_admission_route(self) -> None:
        # iPad touch must activate the same game/platform/release/profile
        # cards as a pointer click, without accepting SDL's compatibility
        # touch-mouse event a second time.
        handler = SOURCE.index("const auto handle_menu_pointer_down")
        mouse = SOURCE.index("event.type == SDL_EVENT_MOUSE_BUTTON_DOWN", handler)
        finger = SOURCE.index("event.type == SDL_EVENT_FINGER_DOWN", mouse)
        self.assertIn("SDL_TOUCH_MOUSEID", SOURCE[mouse:finger])
        self.assertIn("handle_menu_pointer_down(x, y);", SOURCE[mouse:finger])
        self.assertIn("SDL_GetWindowSize(window", SOURCE[finger:finger + 700])
        self.assertIn("SDL_RenderCoordinatesFromWindow", SOURCE[finger:finger + 700])
        self.assertIn("handle_menu_pointer_down(x, y);", SOURCE[finger:finger + 700])

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
