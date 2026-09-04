#pragma once

#include "data/release_manifest.hpp"
#include "i18n.hpp"

#include <span>
#include <cstddef>
#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

namespace eon {

// A stable presentation key for text recovered from original media. Source
// bytes stay in the owning parser/runtime object; this table supplies only the
// canonical English message used by Eon's independent PO catalogs.
struct GameTextDefinition {
    Game game = Game::millennium;
    Platform platform = Platform::dos;
    std::string_view id;
    std::string_view source_leaf;
    std::string_view source_sha256;
    std::size_t source_offset = 0;
    std::size_t source_size = 0;
    std::string_view original_text;
    std::string_view canonical_english;
    std::string_view source_language = "en";
};

struct LocalizedGameText {
    std::string id;
    std::string original_text;
    std::string displayed_text;
    std::string language;
    std::string source_sha256;
    std::size_t source_offset = 0;
    std::size_t source_size = 0;
    std::string source_language;
    bool original_bytes_preserved = true;
    bool catalog_translation_used = false;
};

[[nodiscard]] std::span<const GameTextDefinition> game_text_definitions();

// Rehashes the complete original leaf and checks the exact source range. This
// is intended for media admission/tests; localization never retains the leaf.
[[nodiscard]] bool verify_game_text_source(const GameTextDefinition& definition,
    std::span<const std::uint8_t> source_bytes);

// Fail closed when a rendered source string lacks a declared stable key or a
// selected non-English catalog lacks its translation. This makes localization
// part of both Original and Modern presentation contracts rather than an
// optional filter. English presentation uses the canonical English message;
// it does not require a redundant en.po catalog.
[[nodiscard]] LocalizedGameText localize_game_text(
    Game game, Platform platform, std::string_view source_sha256,
    std::string_view original_text,
    std::string_view selected_language, const Translator& translator);

// Resolves an exact range from an immutable original leaf. The whole leaf is
// rehashed before lookup, so callers cannot present bytes from another release
// under a trusted hash. This is the preferred API for newly recovered item
// names, messages, status text, and every other player-visible text table.
[[nodiscard]] LocalizedGameText localize_game_text_at_source(
    Game game, Platform platform, std::string_view source_leaf,
    std::span<const std::uint8_t> source_bytes, std::size_t source_offset,
    std::size_t source_size, std::string_view selected_language,
    const Translator& translator);

// Admits and localizes every declared range belonging to one exact source
// leaf in source order. It fails atomically if the leaf identity, any range,
// or any selected catalog entry is invalid; partial player-facing text must
// never escape into either Original or Modern presentation.
[[nodiscard]] std::vector<LocalizedGameText> localize_all_game_text_from_source(
    Game game, Platform platform, std::string_view source_leaf,
    std::span<const std::uint8_t> source_bytes,
    std::string_view selected_language, const Translator& translator);

} // namespace eon
