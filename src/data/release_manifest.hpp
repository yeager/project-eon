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

// A complete, installed media set is distinct from the archive whose content
// identity it proves.  The set digest is calculated over the documented
// lexical `name<TAB>size<TAB>sha256<LF>` member records, never over a
// reconstructed archive.
struct DirectMediaSetMember {
    std::string_view name;
    std::uint64_t size;
    std::string_view sha256;
};

struct DirectMediaSetManifestEntry {
    std::string_view set_sha256;
    std::string_view content_release_sha256;
    Game game;
    Platform platform;
    std::string_view language;
    std::span<const DirectMediaSetMember> members;
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
[[nodiscard]] std::span<const DirectMediaSetManifestEntry> direct_media_set_manifest();
[[nodiscard]] std::span<const ParserProfileManifestEntry> parser_profile_manifest();

// This is deliberately a strict identity check.  It does not infer that a
// matching filename, a related crack, or another platform has the profile.
[[nodiscard]] bool release_has_parser_profile(std::string_view release_sha256,
                                               std::string_view profile_id);

} // namespace eon
