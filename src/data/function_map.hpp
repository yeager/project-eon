#pragma once

#include "platform/game_data.hpp"

#include <span>
#include <string_view>

namespace eon {

// A named function-level preservation record.  Unlike a recovery-map parser
// boundary, this names one byte-verified routine or dispatch site and retains
// both its immutable source identity and its observed/load-time address.
// It is not executable metadata: no entry contains replacement bytes, a host
// callback, a hook address, or a result to emulate.
struct FunctionMapEntry {
    std::string_view id;
    std::string_view release_sha256;
    std::string_view parser_profile_id;
    Game game;
    Platform platform;
    std::string_view language;
    std::string_view cpu;
    std::string_view source_asset_sha256;
    std::string_view source_offset;
    std::string_view runtime_address;
    std::string_view evidence_level;
    std::string_view uncertainty;
    std::string_view runtime_status;
    std::string_view documentation_anchor;
};

[[nodiscard]] std::span<const FunctionMapEntry> function_map();
[[nodiscard]] std::span<const FunctionMapEntry> function_map_for_release(
    std::string_view release_sha256);
[[nodiscard]] bool release_has_function_map_entry(
    std::string_view release_sha256, std::string_view entry_id);

} // namespace eon
