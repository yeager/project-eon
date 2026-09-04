#include "game_text_localization.hpp"
#include "data/sha256.hpp"

#include <cassert>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <map>
#include <set>
#include <stdexcept>
#include <string>
#include <vector>

namespace {
constexpr std::string_view mill_com_sha256 =
    "4edc491db60d18ba74cda380c7ce99705b262801298829b63b09932f23f8667e";
}

int main() {
    // Original DOS/Amiga/ST strings are byte sequences, not necessarily
    // seven-bit ASCII. Provenance comparison must preserve high-bit source
    // bytes regardless of whether plain `char` is signed on the host.
    const std::vector<std::uint8_t> legacy_encoded_text{0x80U, 0xe5U};
    const std::string legacy_original{
        static_cast<char>(0x80U), static_cast<char>(0xe5U)};
    const std::string legacy_sha = eon::to_hex(eon::sha256(legacy_encoded_text));
    const eon::GameTextDefinition legacy_definition{
        eon::Game::deuteros, eon::Platform::amiga, "test.legacy", "TEST.BIN",
        legacy_sha, 0, legacy_encoded_text.size(), legacy_original,
        "Legacy text", "und"};
    assert(eon::verify_game_text_source(legacy_definition, legacy_encoded_text));

    const auto definitions = eon::game_text_definitions();
    assert(!definitions.empty());
    std::set<std::string> keys;
    for (const auto& definition : definitions) {
        assert(!definition.id.empty());
        assert(definition.source_leaf == "MILL.COM"
            || definition.source_leaf == "2200AD4.BIN");
        assert(definition.source_sha256.size() == 64);
        assert(definition.source_size == definition.original_text.size());
        assert(!definition.original_text.empty());
        assert(definition.source_language == "en" || definition.source_language == "es");
        assert(!definition.canonical_english.empty());
        assert(keys.emplace(std::string(definition.id) + ":"
            + std::string(definition.source_sha256)).second);
    }

#ifdef EON_DIRECT_DATA_DIR
    std::map<std::string, std::vector<std::uint8_t>> source_leaves;
    for (const auto& definition : definitions) {
        // This configured installed directory is the exact English DOS set.
        // Spanish source ranges are exercised when the archive corpus is
        // supplied to the broader native tests; never substitute this leaf.
        if (definition.source_language != "en") continue;
        auto& source_bytes = source_leaves[std::string(definition.source_leaf)];
        if (source_bytes.empty()) {
            const auto source_path = std::filesystem::path(EON_DIRECT_DATA_DIR)
                / definition.source_leaf;
            std::ifstream source_file(source_path, std::ios::binary);
            assert(source_file);
            source_bytes.assign(std::istreambuf_iterator<char>(source_file),
                std::istreambuf_iterator<char>());
        }
        assert(eon::verify_game_text_source(definition, source_bytes));
    }
    auto altered = source_leaves.at("MILL.COM");
    altered[definitions.front().source_offset] ^= 1U;
    assert(!eon::verify_game_text_source(definitions.front(), altered));

    const eon::Translator source_english;
    const auto source_welcome = eon::localize_game_text_at_source(
        eon::Game::millennium, eon::Platform::dos, "MILL.COM",
        source_leaves.at("MILL.COM"), definitions.front().source_offset,
        definitions.front().source_size, "en", source_english);
    assert(source_welcome.id == definitions.front().id);
    assert(source_welcome.displayed_text == "Welcome to Millennium.");

    const auto all_launcher_text = eon::localize_all_game_text_from_source(
        eon::Game::millennium, eon::Platform::dos, "MILL.COM",
        source_leaves.at("MILL.COM"), "en", source_english);
    assert(all_launcher_text.size() == 10);
    for (std::size_t index = 1; index < all_launcher_text.size(); ++index) {
        assert(all_launcher_text[index - 1].source_offset
            < all_launcher_text[index].source_offset);
    }

    const auto all_celestial_text = eon::localize_all_game_text_from_source(
        eon::Game::millennium, eon::Platform::dos, "2200AD4.BIN",
        source_leaves.at("2200AD4.BIN"), "sv",
        eon::Translator::from_language("sv"));
    assert(all_celestial_text.size() == 41);
    assert(all_celestial_text.front().language == "sv");
    assert(all_celestial_text.front().catalog_translation_used);

    bool source_rejected = false;
    const auto admitted_celestial_text = eon::admit_all_game_text_from_source(
        eon::Game::millennium, eon::Platform::dos, "2200AD4.BIN",
        source_leaves.at("2200AD4.BIN"));
    assert(admitted_celestial_text.size() == 41);
    const auto admitted_swedish = eon::localize_admitted_game_text(
        eon::Game::millennium, eon::Platform::dos,
        admitted_celestial_text.front(), "sv", eon::Translator::from_language("sv"));
    assert(admitted_swedish.displayed_text == all_celestial_text.front().displayed_text);
    auto forged_token = admitted_celestial_text.front();
    forged_token.source_offset += 1;
    source_rejected = false;
    try {
        static_cast<void>(eon::localize_admitted_game_text(
            eon::Game::millennium, eon::Platform::dos, forged_token,
            "en", source_english));
    } catch (const std::runtime_error&) {
        source_rejected = true;
    }
    assert(source_rejected);

    source_rejected = false;
    try {
        static_cast<void>(eon::localize_game_text_at_source(
            eon::Game::millennium, eon::Platform::dos, "MILL.COM", altered,
            definitions.front().source_offset, definitions.front().source_size,
            "en", source_english));
    } catch (const std::runtime_error&) {
        source_rejected = true;
    }
    assert(source_rejected);

    source_rejected = false;
    try {
        static_cast<void>(eon::localize_all_game_text_from_source(
            eon::Game::millennium, eon::Platform::dos, "WRONG.BIN",
            source_leaves.at("MILL.COM"), "en", source_english));
    } catch (const std::runtime_error&) {
        source_rejected = true;
    }
    assert(source_rejected);
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
                definition.original_text, language, translator);
            assert(localized.original_text == definition.original_text);
            assert(localized.source_language == definition.source_language);
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
