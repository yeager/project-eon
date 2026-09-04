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

// Copy-only capability created while the complete source leaf is still
// available for hashing. Runtime snapshots may retain this provenance token,
// but never the commercial source leaf itself.
struct AdmittedGameText {
    std::string id;
    std::string original_text;
    std::string canonical_english;
    std::string source_leaf;
    std::string source_sha256;
    std::size_t source_offset = 0;
    std::size_t source_size = 0;
    std::string source_language;
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

[[nodiscard]] std::vector<AdmittedGameText> admit_all_game_text_from_source(
    Game game, Platform platform, std::string_view source_leaf,
    std::span<const std::uint8_t> source_bytes);

// Revalidates every token field against the compiled source map before using
// it. A forged or stale runtime snapshot therefore cannot acquire a semantic
// key merely by presenting plausible original text.
[[nodiscard]] LocalizedGameText localize_admitted_game_text(
    Game game, Platform platform, const AdmittedGameText& admitted,
    std::string_view selected_language, const Translator& translator);

// Runtime presentation normally holds a table of copy-only admission tokens,
// not the source leaf. Resolve either a stable semantic ID or the exact
// recovered source spelling while still revalidating the selected token
// against the compiled map. Zero or multiple matches fail closed.
[[nodiscard]] LocalizedGameText localize_admitted_game_text_by_id(
    Game game, Platform platform, std::span<const AdmittedGameText> admitted,
    std::string_view id, std::string_view selected_language,
    const Translator& translator);

[[nodiscard]] LocalizedGameText localize_admitted_game_text_by_original(
    Game game, Platform platform, std::span<const AdmittedGameText> admitted,
    std::string_view original_text, std::string_view selected_language,
    const Translator& translator);

// Localizes a complete admitted runtime table in its preserved source order.
// This is the mode-independent presentation path for object/item names,
// messages, help, labels, and menus after media admission. It validates the
// entire table before returning any text and rejects duplicate, reordered, or
// mixed-source tokens. Original and Modern must call this same API with the
// same selected language; presentation mode is deliberately not an input.
[[nodiscard]] std::vector<LocalizedGameText> localize_admitted_game_text_table(
    Game game, Platform platform, std::span<const AdmittedGameText> admitted,
    std::string_view selected_language, const Translator& translator);

} // namespace eon
