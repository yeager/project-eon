#pragma once

#include "platform/game_data.hpp"

#include <span>
#include <string_view>

namespace eon {

// A recovery-map entry is a declaration about already admitted original
// bytes. It is not a hook, patch, emulator dispatch table, or instruction for
// the runtime to execute code from media. `parser_profile_id` resolves through
// the hash-bound release manifest before this map is reported.
struct RecoveryMapEntry {
    std::string_view id;
    std::string_view release_sha256;
    std::string_view parser_profile_id;
    Game game;
    Platform platform;
    std::string_view language;
    std::string_view cpu;
    std::string_view source_address;
    std::string_view evidence_level;
    std::string_view runtime_status;
    std::string_view documentation_anchor;
};

[[nodiscard]] std::span<const RecoveryMapEntry> recovery_map();

// The recovery map is a complete one-to-one companion of the parser-profile
// manifest.  Validate its release tuple, profile binding, and diagnostics-only
// fields before diagnostics reports it as preservation provenance.
[[nodiscard]] bool recovery_map_manifest_is_valid();

// A map row can only be reported for its exact recognised outer archive and
// the exact bounded profile declared in the compiled release manifest.
[[nodiscard]] bool release_has_recovery_map_entry(
    std::string_view release_sha256, std::string_view entry_id);

[[nodiscard]] std::span<const RecoveryMapEntry> recovery_map_for_release(
    std::string_view release_sha256);

} // namespace eon
