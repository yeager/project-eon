#include "data/recovery_map.hpp"

#include "data/release_manifest.hpp"

#include <algorithm>
#include <array>

namespace eon {
namespace {

// Keep this compact, declarative table synchronized with
// docs/recovery-map.json. Each row is a preservation index over existing
// parsers. It deliberately has no replacement address, patch bytes, host
// callback, or executable action.
constexpr std::array<RecoveryMapEntry, 9> entries{{
    {"deuteros-amiga-main-stage", "f4dc8dd1c27c5d389837783becd9b95ab09b78baf40e94e39e2b7e590e470e04", "deuteros-amiga-clean-main-stage", Game::deuteros, Platform::amiga, "en", "m68000", "$13000", "verified-static", "read-only parser and diagnostics", "PRESERVATION.md#deuteros-amiga"},
    {"deuteros-amiga-title-handoff", "f4dc8dd1c27c5d389837783becd9b95ab09b78baf40e94e39e2b7e590e470e04", "deuteros-amiga-clean-title-handoff", Game::deuteros, Platform::amiga, "en", "m68000", "$40000", "verified-static", "read-only parser and diagnostics", "PRESERVATION.md#deuteros-amiga"},
    {"deuteros-atari-protected-boot", "c6856d0a7ccda925289c60f0675e7aaed616f8a0289c74698e87e1ee11e6c653", "deuteros-atari-killer-boot", Game::deuteros, Platform::atari_st, "en", "m68000", "$1000", "verified-static", "read-only parser and diagnostics", "PRESERVATION.md#deuteros-atari-st"},
    {"deuteros-atari-first-stage", "c6856d0a7ccda925289c60f0675e7aaed616f8a0289c74698e87e1ee11e6c653", "deuteros-atari-replicants-first-stage", Game::deuteros, Platform::atari_st, "en", "m68000", "$2000", "verified-static", "read-only parser and diagnostics", "PRESERVATION.md#deuteros-atari-st"},
    {"millennium-amiga-defjam-bootstrap", "2e27d7aeb8b8b7f2a75eda45b456ab42775a706aa85516c85e61ce94ec9eb400", "millennium-amiga-defjam-bootstrap", Game::millennium, Platform::amiga, "en", "m68000", "$41000", "verified-static", "read-only parser and diagnostics", "PRESERVATION.md#millennium-amiga"},
    {"millennium-amiga-shared-resident", "2e27d7aeb8b8b7f2a75eda45b456ab42775a706aa85516c85e61ce94ec9eb400", "millennium-amiga-shared-resident", Game::millennium, Platform::amiga, "en", "m68000", "$68000", "verified-static", "read-only parser and diagnostics", "PRESERVATION.md#millennium-amiga"},
    {"millennium-atari-equinox-bootstrap", "ba1174123a0531abeab5788f4ac87a3c2500696bf1c87a7efd209441b3ebdf01", "millennium-atari-equinox-bootstrap", Game::millennium, Platform::atari_st, "en", "m68000", "$77000", "verified-static", "read-only parser and diagnostics", "PRESERVATION.md#millennium-atari-st"},
    {"millennium-dos-title-flow", "e6e7044b25877fdf8b10d16d2f395886d9957953144ae15ca630cda9cab2a123", "millennium-dos-title-flow", Game::millennium, Platform::dos, "en", "i8086", "TITLE.LIB+0x0", "verified-static", "read-only parser and diagnostics", "PRESERVATION.md#millennium-dos"},
    {"millennium-dos-spanish-startup", "b40cc2f2c39cdb476b4a82bda7bffed1c80decdfb7fe41b1a38bf54343e0c0a4", "millennium-dos-spanish-startup", Game::millennium, Platform::dos, "es", "i8086", "boot+0x0", "verified-static", "read-only parser and diagnostics", "PRESERVATION.md#millennium-dos"},
}};

} // namespace

std::span<const RecoveryMapEntry> recovery_map() { return entries; }

bool release_has_recovery_map_entry(const std::string_view release_sha256,
    const std::string_view entry_id) {
    for (const auto& entry : entries) {
        if (entry.release_sha256 == release_sha256 && entry.id == entry_id) {
            return release_has_parser_profile(release_sha256, entry.parser_profile_id);
        }
    }
    return false;
}

std::span<const RecoveryMapEntry> recovery_map_for_release(const std::string_view release_sha256) {
    const auto first = std::find_if(entries.begin(), entries.end(), [release_sha256](const auto& entry) {
        return entry.release_sha256 == release_sha256;
    });
    if (first == entries.end()) return {};
    const auto last = std::find_if(first, entries.end(), [release_sha256](const auto& entry) {
        return entry.release_sha256 != release_sha256;
    });
    return {first, last};
}

} // namespace eon
