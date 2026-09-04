#include "game_text_localization.hpp"

#include <cassert>
#include <set>
#include <stdexcept>
#include <string>

int main() {
    const auto definitions = eon::game_text_definitions();
    assert(!definitions.empty());
    std::set<std::string> keys;
    for (const auto& definition : definitions) {
        assert(!definition.id.empty());
        assert(!definition.original_english.empty());
        assert(!definition.canonical_english.empty());
        assert(keys.emplace(definition.id).second);
    }

    const eon::Translator english;
    const auto original = eon::localize_game_text(eon::Game::millennium,
        eon::Platform::dos, "Welcome To MILLENIUM.", "en", english);
    assert(original.id == "millennium.dos.sound.welcome");
    assert(original.original_text == "Welcome To MILLENIUM.");
    assert(original.displayed_text == "Welcome to Millennium.");
    assert(original.original_bytes_preserved);
    assert(!original.catalog_translation_used);

    for (const auto language : eon::supported_launcher_languages()) {
        if (language == "en") continue;
        const auto translator = eon::Translator::from_language(language);
        assert(!translator.empty());
        for (const auto& definition : definitions) {
            const auto localized = eon::localize_game_text(definition.game,
                definition.platform, definition.original_english, language, translator);
            assert(localized.original_text == definition.original_english);
            assert(!localized.displayed_text.empty());
            assert(localized.original_bytes_preserved);
            assert(localized.catalog_translation_used);
        }
    }

    bool rejected = false;
    try {
        static_cast<void>(eon::localize_game_text(eon::Game::millennium,
            eon::Platform::dos, "unknown media text", "en", english));
    } catch (const std::runtime_error&) {
        rejected = true;
    }
    assert(rejected);

    rejected = false;
    try {
        static_cast<void>(eon::localize_game_text(eon::Game::millennium,
            eon::Platform::dos, "Welcome To MILLENIUM.", "sv", english));
    } catch (const std::runtime_error&) {
        rejected = true;
    }
    assert(rejected);
}
