#include "data/function_map.hpp"

#include "data/release_manifest.hpp"

#include <algorithm>
#include <array>

namespace eon {
namespace {

// Keep this table in exact source order with docs/function-map.json.  Every
// source hash names an existing, separately hash-checked original leaf or
// stage.  The descriptions deliberately retain unknown ABI/state boundaries.
constexpr std::array<FunctionMapEntry, 3> entries{{
    {"millennium-dos-en-action-poll", "e6e7044b25877fdf8b10d16d2f395886d9957953144ae15ca630cda9cab2a123",
     "millennium-dos-game-flow", Game::millennium, Platform::dos, "en", "i8086",
     "427574e5f780b2a7b5c4207d167116dc44aea3fb67096fbf12a46c4f544a0a57",
     "2200AD.EXE+0xd2db", "$d3db", "verified-static",
     "raw action return; input producer and handler semantics remain unproven",
     "diagnostics only", "PRESERVATION.md#main-loop-action-dispatch"},
    {"millennium-dos-en-f8-prefix", "e6e7044b25877fdf8b10d16d2f395886d9957953144ae15ca630cda9cab2a123",
     "millennium-dos-game-flow", Game::millennium, Platform::dos, "en", "i8086",
     "427574e5f780b2a7b5c4207d167116dc44aea3fb67096fbf12a46c4f544a0a57",
     "2200AD.EXE+0x7206", "$7306", "verified-static",
     "only the pre-call private-overlay write is proven; later native calls are opaque",
     "isolated transient overlay only", "PRESERVATION.md#main-loop-action-dispatch"},
    {"deuteros-amiga-en-title-exec-boundary", "f4dc8dd1c27c5d389837783becd9b95ab09b78baf40e94e39e2b7e590e470e04",
     "deuteros-amiga-clean-title-handoff", Game::deuteros, Platform::amiga, "en", "m68000",
     "48d65260e9b5f5cbf8d8b3675a178c81b8764810b61a6a2539a56dcb40a8de03",
     "title-stage+0x0450", "$40450", "verified-static",
     "Exec base/vector result, callback ABI, input and title state remain unobserved",
     "diagnostics only", "PRESERVATION.md#deuteros-amiga-title-input-and-bootstrap-handoff"},
}};

} // namespace

std::span<const FunctionMapEntry> function_map() { return entries; }

std::span<const FunctionMapEntry> function_map_for_release(const std::string_view release_sha256) {
    const auto first = std::find_if(entries.begin(), entries.end(), [release_sha256](const auto& entry) {
        return entry.release_sha256 == release_sha256;
    });
    const auto last = std::find_if(first, entries.end(), [release_sha256](const auto& entry) {
        return entry.release_sha256 != release_sha256;
    });
    return {first, last};
}

bool release_has_function_map_entry(const std::string_view release_sha256,
    const std::string_view entry_id) {
    const auto entry = std::find_if(entries.begin(), entries.end(), [&](const auto& candidate) {
        return candidate.release_sha256 == release_sha256 && candidate.id == entry_id;
    });
    return entry != entries.end()
        && release_has_parser_profile(release_sha256, entry->parser_profile_id);
}

} // namespace eon
