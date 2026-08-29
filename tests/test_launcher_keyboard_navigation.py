"""Guard the staged card-launch and modal-input accessibility contracts."""

from pathlib import Path
import unittest


ROOT = Path(__file__).resolve().parents[1]
SOURCE = (ROOT / "src" / "main.cpp").read_text(encoding="utf-8")
class LauncherKeyboardNavigationTests(unittest.TestCase):
    def test_menu_has_three_explicit_card_pages(self) -> None:
        self.assertIn("enum class LauncherPage { games, platforms, profiles }", SOURCE)
        self.assertIn("std::array<PlatformCard, 3>", SOURCE)
        self.assertIn("std::array<ProfileCard, 3>", SOURCE)
        self.assertIn("LauncherPage::platforms", SOURCE)
        self.assertIn("LauncherPage::profiles", SOURCE)

    def test_platform_cards_are_hash_verified_and_disabled_when_missing(self) -> None:
        self.assertIn("eon::release_available(releases, game, card.platform)", SOURCE)
        self.assertIn("UNAVAILABLE PLATFORM CARDS CANNOT START A GAME", SOURCE)
        self.assertIn("if (!eon::release_available(releases, game, platform)) return false;", SOURCE)
        self.assertIn("&& choose_platform_card(static_cast<int>(index))", SOURCE)

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
        # A single Escape/Back from profiles must land on platforms, rather
        # than falling through to the menu handler and skipping to games.
        self.assertIn("Escape is a single navigation action", SOURCE)
        self.assertIn("Keep Back equivalent to Escape", SOURCE)
        self.assertGreaterEqual(SOURCE.count("launcher_page == LauncherPage::profiles\n                        ? LauncherPage::platforms : LauncherPage::games"), 2)

    def test_modern_popup_consumes_events_before_game_or_menu_input(self) -> None:
        modal = SOURCE.index("if (show_modern_graphics_settings) {")
        modal_continue = SOURCE.index("                continue;", modal)
        title_input = SOURCE.index("millennium_title_session->poll_console(true)")
        menu_input = SOURCE.index("LauncherPage::games", modal_continue)
        self.assertLess(modal, modal_continue)
        self.assertLess(modal_continue, title_input)
        self.assertLess(modal_continue, menu_input)


if __name__ == "__main__":
    unittest.main()
