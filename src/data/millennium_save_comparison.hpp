#pragma once

#include <cstddef>
#include <cstdint>
#include <span>
#include <string>
#include <string_view>
#include <vector>

namespace eon {

// These identities are deliberately release- and pathname-specific.  A
// matching byte sequence from an unrelated dump must not be presented as one
// of the supplied Millennium save artifacts.
enum class MillenniumSavePlatform { dos, atari_st };

// An authenticated, in-memory copy of a supplied original save artifact.
// It deliberately exposes no mutation or serialization API.  `slot_name` is
// the original filename, not a claim about a game-visible save-slot meaning.
struct MillenniumAuthenticatedSave {
    MillenniumSavePlatform platform = MillenniumSavePlatform::dos;
    std::string slot_name;
    std::string sha256;
    std::vector<std::uint8_t> bytes;
};

// Positional byte facts about two independently authenticated originals.
// The counts cover only the shared prefix of their physical byte streams;
// `left_only_bytes` and `right_only_bytes` retain the remaining size fact.
// No difference is assigned a game-state or file-format meaning here.
struct MillenniumSaveByteComparison {
    std::string left_sha256;
    std::string right_sha256;
    std::size_t shared_bytes = 0;
    std::size_t equal_positions = 0;
    std::size_t different_positions = 0;
    std::size_t common_prefix_bytes = 0;
    std::size_t common_suffix_bytes = 0;
    std::size_t left_only_bytes = 0;
    std::size_t right_only_bytes = 0;
};

// Admits exactly the original DOS `2200SAVE.I` or the four original Equinox
// Atari ST `2200SAVE.*` payloads documented in PRESERVATION.md.  The input is
// copied only to retain a stable read-only in-memory view; it is never written
// back to the archive, floppy image, or any save path.
[[nodiscard]] MillenniumAuthenticatedSave authenticate_millennium_save(
    MillenniumSavePlatform platform, std::string_view slot_name,
    std::span<const std::uint8_t> bytes);

// Compares byte positions after both sources passed the identity gate above.
// This is intentionally a format-analysis observation rather than a save
// compatibility claim.
[[nodiscard]] MillenniumSaveByteComparison compare_millennium_saves(
    const MillenniumAuthenticatedSave& left, const MillenniumAuthenticatedSave& right);

} // namespace eon
