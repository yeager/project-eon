#pragma once

#include "platform/game_data.hpp"

#include <cstddef>
#include <cstdint>
#include <span>
#include <string_view>

namespace eon {

// The outer archive identifies a supplied release.  A parser profile is only
// admitted when its exact leaf and byte range are present in that archive.
// These records are evidence, not a substitute for the original media.
struct ReleaseManifestEntry {
    std::string_view sha256;
    Game game;
    Platform platform;
    std::string_view language;
    std::uintmax_t size;
};

struct ParserProfileManifestEntry {
    std::string_view id;
    std::string_view release_sha256;
    std::string_view leaf_sha256;
    std::uint64_t leaf_size;
    std::uint64_t offset;
    std::uint64_t length;
};

[[nodiscard]] std::span<const ReleaseManifestEntry> release_manifest();
[[nodiscard]] std::span<const ParserProfileManifestEntry> parser_profile_manifest();

// This is deliberately a strict identity check.  It does not infer that a
// matching filename, a related crack, or another platform has the profile.
[[nodiscard]] bool release_has_parser_profile(std::string_view release_sha256,
                                               std::string_view profile_id);

} // namespace eon
