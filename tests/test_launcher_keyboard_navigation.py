"""Guard the launcher keyboard-focus and modal-input accessibility contract."""

from pathlib import Path
import unittest


ROOT = Path(__file__).resolve().parents[1]
SOURCE = (ROOT / "src" / "main.cpp").read_text(encoding="utf-8")
README = (ROOT / "README.md").read_text(encoding="utf-8")


class LauncherKeyboardNavigationTests(unittest.TestCase):
    def test_keyboard_focus_has_cards_platform_and_start_controls(self) -> None:
        self.assertIn("enum class MenuFocus { cards, platform, start }", SOURCE)
        self.assertIn("SDLK_TAB", SOURCE)
        self.assertIn("SDL_KMOD_SHIFT", SOURCE)
        self.assertIn("SDLK_HOME", SOURCE)
        self.assertIn("SDLK_END", SOURCE)
        self.assertIn("advance_menu_focus", SOURCE)

    def test_focus_is_visible_without_new_untranslated_launcher_prose(self) -> None:
        self.assertIn("card_has_keyboard_focus", SOURCE)
        self.assertIn("start_focus_bounds", SOURCE)
        self.assertIn("platform_focus_bounds", SOURCE)
        self.assertIn("SDL_RenderRect(renderer, &platform_focus_bounds)", SOURCE)

    def test_modern_popup_consumes_events_before_game_or_menu_input(self) -> None:
        modal = SOURCE.index("if (show_modern_graphics_settings) {")
        modal_continue = SOURCE.index("                continue;", modal)
        title_input = SOURCE.index("millennium_title_session->poll_console(true)")
        menu_input = SOURCE.index("advance_menu_focus", modal_continue)
        self.assertLess(modal, modal_continue)
        self.assertLess(modal_continue, title_input)
        self.assertLess(modal_continue, menu_input)

    def test_readme_documents_keyboard_focus_and_modal_behavior(self) -> None:
        self.assertIn("Tab and Shift+Tab", README)
        self.assertIn("Home/End", README)
        self.assertIn("input-modal", README)


if __name__ == "__main__":
    unittest.main()
