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
