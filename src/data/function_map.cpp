#include "data/function_map.hpp"

#include "data/release_manifest.hpp"

#include <algorithm>
#include <array>

namespace eon {
namespace {

// Keep this table in exact source order with docs/function-map.json.  Every
// source hash names an existing, separately hash-checked original leaf or
// stage.  The descriptions deliberately retain unknown ABI/state boundaries.
constexpr std::array<FunctionMapEntry, 8> entries{{
    {"millennium-dos-en-launcher-driver-request", "e6e7044b25877fdf8b10d16d2f395886d9957953144ae15ca630cda9cab2a123",
     "millennium-dos-launcher", Game::millennium, Platform::dos, "en", "i8086",
     "4edc491db60d18ba74cda380c7ce99705b262801298829b63b09932f23f8667e",
     "MILL.COM+0x01cf", "$02cf", "verified-static",
     "the requested video driver, private interrupt result, and subsequent branch remain unproven",
     "diagnostics only", "PRESERVATION.md#english-millennium-dos-reference-trace-adapter"},
    {"millennium-dos-en-title-entry", "e6e7044b25877fdf8b10d16d2f395886d9957953144ae15ca630cda9cab2a123",
     "millennium-dos-title-flow", Game::millennium, Platform::dos, "en", "i8086",
     "3cc57f2b12a0da44dd43220f44f06a05b9e3f009bcf008b7bb87622a5988cbe6",
     "TITLES.EXE+0x1a80", "$1b80", "verified-static",
     "entry conditions, resource routine results, input, and title presentation remain unproven",
     "diagnostics only", "PRESERVATION.md#title-to-game-hand-off"},
    {"millennium-dos-en-title-private-wrapper", "e6e7044b25877fdf8b10d16d2f395886d9957953144ae15ca630cda9cab2a123",
     "millennium-dos-title-flow", Game::millennium, Platform::dos, "en", "i8086",
     "3cc57f2b12a0da44dd43220f44f06a05b9e3f009bcf008b7bb87622a5988cbe6",
     "TITLES.EXE+0x0127", "$0227", "verified-static",
     "the private INT 91h ABI and observed raw returns are not runtime inputs",
     "diagnostics only", "PRESERVATION.md#english-millennium-dos-reference-trace-adapter"},
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
    {"millennium-dos-en-game-entry", "e6e7044b25877fdf8b10d16d2f395886d9957953144ae15ca630cda9cab2a123",
     "millennium-dos-game-flow", Game::millennium, Platform::dos, "en", "i8086",
     "427574e5f780b2a7b5c4207d167116dc44aea3fb67096fbf12a46c4f544a0a57",
     "2200AD.EXE+0xd1b0", "$d2b0", "verified-static",
     "the first private wrapper result, BIOS results, game state, and action loop remain unproven",
     "diagnostics only", "PRESERVATION.md#english-millennium-dos-startup-prefix"},
    {"millennium-dos-en-gx-dispatcher", "e6e7044b25877fdf8b10d16d2f395886d9957953144ae15ca630cda9cab2a123",
     "millennium-dos-gx-overlay", Game::millennium, Platform::dos, "en", "i8086",
     "093f8416de6d23837d2faf82360ef79777c2c2bf146619aafad87626c61ab6fb",
     "2200GX.EXE+0x0000", "$0100", "verified-static",
     "selector policy, overlay segment, handler results, resource order, and display effects remain unproven",
     "trace-gated sparse GX startup session", "PRESERVATION.md#millennium-dos-execution-model"},
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
