#include "game_text_localization.hpp"

#include <cassert>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <set>
#include <stdexcept>
#include <string>
#include <vector>

namespace {
constexpr std::string_view mill_com_sha256 =
    "4edc491db60d18ba74cda380c7ce99705b262801298829b63b09932f23f8667e";
}

int main() {
    const auto definitions = eon::game_text_definitions();
    assert(!definitions.empty());
    std::set<std::string> keys;
    for (const auto& definition : definitions) {
        assert(!definition.id.empty());
        assert(definition.source_leaf == "MILL.COM");
        assert(definition.source_sha256.size() == 64);
        assert(definition.source_size == definition.original_english.size());
        assert(!definition.original_english.empty());
        assert(!definition.canonical_english.empty());
        assert(keys.emplace(definition.id).second);
    }

#ifdef EON_DIRECT_DATA_DIR
    const auto source_path = std::filesystem::path(EON_DIRECT_DATA_DIR) / "MILL.COM";
    std::ifstream source_file(source_path, std::ios::binary);
    assert(source_file);
    const std::vector<std::uint8_t> source_bytes{
        std::istreambuf_iterator<char>(source_file), std::istreambuf_iterator<char>()};
    for (const auto& definition : definitions) {
        assert(eon::verify_game_text_source(definition, source_bytes));
    }
    auto altered = source_bytes;
    altered[definitions.front().source_offset] ^= 1U;
    assert(!eon::verify_game_text_source(definitions.front(), altered));
#endif

    const eon::Translator english;
    const auto original = eon::localize_game_text(eon::Game::millennium,
        eon::Platform::dos, mill_com_sha256, "Welcome To MILLENIUM.", "en", english);
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
                definition.platform, definition.source_sha256,
                definition.original_english, language, translator);
            assert(localized.original_text == definition.original_english);
            assert(!localized.displayed_text.empty());
            assert(localized.original_bytes_preserved);
            assert(localized.catalog_translation_used);
        }
    }

    bool rejected = false;
    try {
        static_cast<void>(eon::localize_game_text(eon::Game::millennium,
            eon::Platform::dos, mill_com_sha256, "unknown media text", "en", english));
    } catch (const std::runtime_error&) {
        rejected = true;
    }
    assert(rejected);

    rejected = false;
    try {
        static_cast<void>(eon::localize_game_text(eon::Game::millennium,
            eon::Platform::dos, mill_com_sha256,
            "Welcome To MILLENIUM.", "sv", english));
    } catch (const std::runtime_error&) {
        rejected = true;
    }
    assert(rejected);

    rejected = false;
    try {
        static_cast<void>(eon::localize_game_text(eon::Game::millennium,
            eon::Platform::dos, std::string(64, '0'),
            "Welcome To MILLENIUM.", "en", english));
    } catch (const std::runtime_error&) {
        rejected = true;
    }
    assert(rejected);
}
