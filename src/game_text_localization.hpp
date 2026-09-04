#pragma once

#include "data/release_manifest.hpp"
#include "i18n.hpp"

#include <span>
#include <string>
#include <string_view>

namespace eon {

// A stable presentation key for text recovered from original media. Source
// bytes stay in the owning parser/runtime object; this table supplies only the
// canonical English message used by Eon's independent PO catalogs.
struct GameTextDefinition {
    Game game = Game::millennium;
    Platform platform = Platform::dos;
    std::string_view id;
    std::string_view original_english;
    std::string_view canonical_english;
};

struct LocalizedGameText {
    std::string id;
    std::string original_text;
    std::string displayed_text;
    std::string language;
    bool original_bytes_preserved = true;
    bool catalog_translation_used = false;
};

[[nodiscard]] std::span<const GameTextDefinition> game_text_definitions();

// Fail closed when a rendered source string lacks a declared stable key or a
// selected non-English catalog lacks its translation. This makes localization
// part of both Original and Modern presentation contracts rather than an
// optional filter. English presentation uses the canonical English message;
// it does not require a redundant en.po catalog.
[[nodiscard]] LocalizedGameText localize_game_text(
    Game game, Platform platform, std::string_view original_text,
    std::string_view selected_language, const Translator& translator);

} // namespace eon
