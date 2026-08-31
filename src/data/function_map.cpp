#include "data/function_map.hpp"

#include "data/release_manifest.hpp"

#include <algorithm>
#include <array>
#include <cctype>

namespace eon {
namespace {

// Keep this table in exact source order with docs/function-map.json.  Every
// source hash names an existing, separately hash-checked original leaf or
// stage.  The descriptions deliberately retain unknown ABI/state boundaries.
constexpr std::array<FunctionMapEntry, 13> entries{{
    {"millennium-atari-en-prg-entry", "ba1174123a0531abeab5788f4ac87a3c2500696bf1c87a7efd209441b3ebdf01",
     "millennium-atari-equinox-prg-chain", Game::millennium, Platform::atari_st, "en", "m68000",
     "4584ddc459e3bf03e642f3156fbedb74aa33a847db4937beb5635eb492e93686",
     "MILENIUM.TOS+0x001c", "+0x0000", "verified-static",
     "GEMDOS relocation, runtime load base, TOS/XBIOS results, and execution remain unproven",
     "diagnostics only", "PRESERVATION.md#millennium-atari-st-relocation-evidence",
     "image-relative-unrelocated"},
    {"millennium-amiga-en-resident-independent-entry", "2e27d7aeb8b8b7f2a75eda45b456ab42775a706aa85516c85e61ce94ec9eb400",
     "millennium-amiga-shared-resident", Game::millennium, Platform::amiga, "en", "m68000",
     "d144abc05f891710dc99b30d87f020bd6e2ff7796ef86a847f07b8d97d55d18e",
     "ADF+0x16908", "$68508", "verified-static",
     "D3, the tested runtime byte, both branch outcomes, and external targets remain unproven",
     "diagnostics only", "PRESERVATION.md#millennium-amiga-raw-loader-evidence"},
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
    {"millennium-dos-es-title-entry", "b40cc2f2c39cdb476b4a82bda7bffed1c80decdfb7fe41b1a38bf54343e0c0a4",
     "millennium-dos-spanish-title-boundary", Game::millennium, Platform::dos, "es", "i8086",
     "02082c35e18cee330f7d1b88098f502e68011f7e47a3a649961f6f03d1d14fe7",
     "TITLES.EXE+0x1a80", "$1b80", "verified-static",
     "private-driver results, DOS character semantics, child status, frames, and game state remain unproven",
     "diagnostics only", "PRESERVATION.md#millennium-spanish-dos-floppy-evidence"},
    {"deuteros-amiga-en-main-entry", "f4dc8dd1c27c5d389837783becd9b95ab09b78baf40e94e39e2b7e590e470e04",
     "deuteros-amiga-clean-main-stage", Game::deuteros, Platform::amiga, "en", "m68000",
     "6ea0cc68d3af37203a885032eddf7c28e839e6abb59d8c9cd3792f1308bdec38",
     "ADF+0x06f34", "$21734", "verified-static",
     "decoded disk-read results, Exec/graphics ABI, input, timing, and game state remain unproven",
     "diagnostics only", "PRESERVATION.md#deuteros-amiga-execution-chain"},
    {"deuteros-atari-en-copied-dispatcher", "c6856d0a7ccda925289c60f0675e7aaed616f8a0289c74698e87e1ee11e6c653",
     "deuteros-atari-replicants-first-stage", Game::deuteros, Platform::atari_st, "en", "m68000",
     "2489256511e857a4a1b20d413b4f869edaae1f4df7f62ce869e324cad40e81d7",
     "track-2+0x00c4", "$1ec4", "verified-static",
     "the preceding raw-read result, dispatcher state word, vector choice, callback ABI, and XBIOS effects remain unproven",
     "diagnostics only", "PRESERVATION.md#deuteros-atari-st-protected-media-boot-chain"},
    {"deuteros-amiga-en-title-exec-boundary", "f4dc8dd1c27c5d389837783becd9b95ab09b78baf40e94e39e2b7e590e470e04",
     "deuteros-amiga-clean-title-handoff", Game::deuteros, Platform::amiga, "en", "m68000",
     "48d65260e9b5f5cbf8d8b3675a178c81b8764810b61a6a2539a56dcb40a8de03",
     "title-stage+0x0450", "$40450", "verified-static",
     "Exec base/vector result, callback ABI, input and title state remain unobserved",
     "diagnostics only", "PRESERVATION.md#deuteros-amiga-title-input-and-bootstrap-handoff"},
}};

bool is_lower_hex(const std::string_view value) {
    return !value.empty() && std::all_of(value.begin(), value.end(), [](const unsigned char character) {
        return (character >= '0' && character <= '9') || (character >= 'a' && character <= 'f');
    });
}

} // namespace

std::span<const FunctionMapEntry> function_map() { return entries; }

std::vector<FunctionMapEntry> function_map_for_release(const std::string_view release_sha256) {
    std::vector<FunctionMapEntry> result;
    result.reserve(entries.size());
    for (const auto& entry : entries) {
        if (entry.release_sha256 == release_sha256) result.push_back(entry);
    }
    return result;
}

bool function_map_entry_is_well_formed(const FunctionMapEntry& entry) {
    if (entry.id.empty() || entry.parser_profile_id.empty() || entry.release_sha256.size() != 64U
        || !is_lower_hex(entry.release_sha256) || entry.source_asset_sha256.size() != 64U
        || !is_lower_hex(entry.source_asset_sha256) || entry.source_offset.empty()
        || entry.uncertainty.empty() || entry.runtime_status.empty()
        || !entry.documentation_anchor.starts_with("PRESERVATION.md#")
        || entry.evidence_level != "verified-static") return false;
    if (entry.cpu != "i8086" && entry.cpu != "m68000") return false;
    if (entry.address_space == "runtime") {
        return entry.runtime_address.size() > 1U && entry.runtime_address.front() == '$';
    }
    return entry.address_space == "image-relative-unrelocated"
        && entry.runtime_address.size() > 3U && entry.runtime_address.starts_with("+0x")
        && is_lower_hex(entry.runtime_address.substr(3));
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
