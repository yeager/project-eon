"""Guard the staged card-launch and modal-input accessibility contracts."""

from pathlib import Path
import unittest


ROOT = Path(__file__).resolve().parents[1]
SOURCE = (ROOT / "src" / "main.cpp").read_text(encoding="utf-8")
class LauncherKeyboardNavigationTests(unittest.TestCase):
    def test_menu_has_explicit_game_platform_release_and_profile_pages(self) -> None:
        self.assertIn("enum class LauncherPage { games, platforms, releases, profiles }", SOURCE)
        self.assertIn("std::array<PlatformCard, 3>", SOURCE)
        self.assertIn("std::array<ProfileCard, 3>", SOURCE)
        self.assertIn("LauncherPage::platforms", SOURCE)
        self.assertIn("LauncherPage::releases", SOURCE)
        self.assertIn("LauncherPage::profiles", SOURCE)

    def test_english_is_default_for_multilingual_platforms(self) -> None:
        self.assertIn("available_release_languages", SOURCE)
        self.assertIn("select_available_release_language", SOURCE)
        self.assertIn('std::find(languages.begin(), languages.end(), "en")', SOURCE)
        self.assertIn("English is selected automatically when it exists.", SOURCE)
        self.assertIn("advance_after_platform_selection", SOURCE)
        advance = SOURCE[SOURCE.index("advance_after_platform_selection"):]
        self.assertIn("active_release_language ? LauncherPage::profiles", advance)

    def test_platform_cards_are_hash_verified_and_disabled_when_missing(self) -> None:
        self.assertIn("eon::platform_card_status(releases, game, card.platform)", SOURCE)
        self.assertIn("eon::platform_card_selectable(status)", SOURCE)
        self.assertIn("UNAVAILABLE PLATFORM CARDS CANNOT START A GAME", SOURCE)
        self.assertIn("if (!eon::platform_card_selectable(status)) return false;", SOURCE)
        self.assertIn("RELEASE SELECTION REQUIRED", SOURCE)
        self.assertIn("ATARI BOOTSTRAP ONLY", SOURCE)
        self.assertIn("card.platform == eon::Platform::atari_st", SOURCE)
        self.assertIn("&& choose_platform_card(static_cast<int>(index))", SOURCE)

    def test_atari_media_scope_never_implies_a_physical_dump_fallback(self) -> None:
        self.assertIn("absent from this verified outer release", SOURCE)
        self.assertIn("no physical-media fallback or substitution", SOURCE)

    def test_ambiguous_or_missing_platform_cards_cannot_start_a_game(self) -> None:
        self.assertIn("eon::platform_card_startable", SOURCE)
        self.assertIn("!active_platform || !active_release_language", SOURCE)

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

    def test_profiles_have_two_runtime_modes_and_a_custom_tuning_route(self) -> None:
        self.assertIn("enum class ProfileChoice { original, modern, custom }", SOURCE)
        self.assertIn("Custom is not a third runtime mode", SOURCE)
        self.assertIn("request.presentation = eon::Presentation::modern", SOURCE)
        self.assertIn("custom_profile_ready", SOURCE)
        self.assertIn("CUSTOM SETTINGS READY", SOURCE)

    def test_runtime_shortcuts_do_not_promote_original_to_modern(self) -> None:
        # Original/Modern is decided by the profile cards before launch. F10
        # remains the settings route for Modern/Custom, but must never turn an
        # Original session into a different presentation behind the user's
        # back; F1 is intentionally inert for the same reason.
        f10 = SOURCE.index("event.key.key == SDLK_F10")
        f10_guard = SOURCE.index(
            "if (request.presentation != eon::Presentation::modern) continue;", f10)
        f1 = SOURCE.index("event.key.key == SDLK_F1 && !event.key.repeat")
        self.assertLess(f10, f10_guard)
        self.assertIn("Presentation is chosen by the profile card before launch.", SOURCE[f1:f1 + 500])

    def test_back_navigation_consumes_one_event_and_moves_one_card_page(self) -> None:
        # A single Escape/Back moves exactly one page and never falls through
        # to the menu handler. Multilingual profiles return to the release
        # card first; a single-language platform returns to platforms.
        self.assertIn("Escape is a single navigation action", SOURCE)
        self.assertIn("Keep Back equivalent to Escape", SOURCE)
        self.assertGreaterEqual(SOURCE.count("release_language_cards().size() > 1 ? LauncherPage::releases : LauncherPage::platforms"), 2)

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
