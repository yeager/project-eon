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
constexpr std::array<RecoveryMapEntry, 42> entries{{
    {"millennium-atari-equinox-direct-bootstrap", "0056e9fe1bae35ba61660a4b563772e4037e8a6390d1f579ec160044e80a1d69", "millennium-atari-equinox-direct-bootstrap", Game::millennium, Platform::atari_st, "en", "m68000", "$77000", "verified-static", "read-only parser and diagnostics", "PRESERVATION.md#direct-container-recognition"},
    {"millennium-atari-equinox-direct-root-inventory", "0056e9fe1bae35ba61660a4b563772e4037e8a6390d1f579ec160044e80a1d69", "millennium-atari-equinox-direct-root-inventory", Game::millennium, Platform::atari_st, "en", "m68000", "disk+0x00000", "verified-static", "read-only parser and diagnostics", "PRESERVATION.md#direct-container-recognition"},
    {"millennium-atari-equinox-direct-prg-chain", "0056e9fe1bae35ba61660a4b563772e4037e8a6390d1f579ec160044e80a1d69", "millennium-atari-equinox-direct-prg-chain", Game::millennium, Platform::atari_st, "en", "m68000", "MILENIUM.TOS+0x0", "verified-static", "read-only parser and diagnostics", "PRESERVATION.md#direct-container-recognition"},
    {"millennium-atari-equinox-direct-config-chain", "0056e9fe1bae35ba61660a4b563772e4037e8a6390d1f579ec160044e80a1d69", "millennium-atari-equinox-direct-config-chain", Game::millennium, Platform::atari_st, "en", "m68000", "MILL22A.INF+0x0", "verified-static", "read-only parser and diagnostics", "PRESERVATION.md#direct-container-recognition"},
    {"millennium-atari-equinox-direct-auxiliary-resource", "0056e9fe1bae35ba61660a4b563772e4037e8a6390d1f579ec160044e80a1d69", "millennium-atari-equinox-direct-auxiliary-resource", Game::millennium, Platform::atari_st, "en", "m68000", "MILL22B.INF+0x11600", "verified-static", "read-only parser and diagnostics", "PRESERVATION.md#direct-container-recognition"},
    {"deuteros-amiga-main-stage", "f4dc8dd1c27c5d389837783becd9b95ab09b78baf40e94e39e2b7e590e470e04", "deuteros-amiga-clean-main-stage", Game::deuteros, Platform::amiga, "en", "m68000", "$13000", "verified-static", "read-only parser and diagnostics", "PRESERVATION.md#deuteros-amiga-execution-chain"},
    {"deuteros-amiga-title-handoff", "f4dc8dd1c27c5d389837783becd9b95ab09b78baf40e94e39e2b7e590e470e04", "deuteros-amiga-clean-title-handoff", Game::deuteros, Platform::amiga, "en", "m68000", "$40000", "verified-static", "read-only parser and diagnostics", "PRESERVATION.md#deuteros-amiga-title-input-and-bootstrap-handoff"},
    {"deuteros-amiga-bundle-0", "f4dc8dd1c27c5d389837783becd9b95ab09b78baf40e94e39e2b7e590e470e04", "deuteros-amiga-bundle-0", Game::deuteros, Platform::amiga, "en", "m68000", "disk+0x1b800", "verified-static", "read-only parser and diagnostics", "PRESERVATION.md#deuteros-amiga-execution-chain"},
    {"deuteros-atari-protected-boot", "c6856d0a7ccda925289c60f0675e7aaed616f8a0289c74698e87e1ee11e6c653", "deuteros-atari-killer-boot", Game::deuteros, Platform::atari_st, "en", "m68000", "$1000", "verified-static", "read-only parser and diagnostics", "PRESERVATION.md#deuteros-atari-st-protected-media-boot-chain"},
    {"deuteros-atari-first-stage", "c6856d0a7ccda925289c60f0675e7aaed616f8a0289c74698e87e1ee11e6c653", "deuteros-atari-replicants-first-stage", Game::deuteros, Platform::atari_st, "en", "m68000", "$2000", "verified-static", "read-only parser and diagnostics", "PRESERVATION.md#deuteros-atari-st-protected-media-boot-chain"},
    {"deuteros-atari-second-stage", "c6856d0a7ccda925289c60f0675e7aaed616f8a0289c74698e87e1ee11e6c653", "deuteros-atari-replicants-second-stage", Game::deuteros, Platform::atari_st, "en", "m68000", "disk+0x04800 -> $70000", "verified-static", "read-only parser and diagnostics", "PRESERVATION.md#deuteros-atari-st-protected-media-boot-chain"},
    {"millennium-amiga-defjam-bootstrap", "2e27d7aeb8b8b7f2a75eda45b456ab42775a706aa85516c85e61ce94ec9eb400", "millennium-amiga-defjam-bootstrap", Game::millennium, Platform::amiga, "en", "m68000", "$41000", "verified-static", "read-only parser and diagnostics", "PRESERVATION.md#millennium-amiga-raw-loader-evidence"},
    {"millennium-amiga-defjam-first-stage-entry", "2e27d7aeb8b8b7f2a75eda45b456ab42775a706aa85516c85e61ce94ec9eb400", "millennium-amiga-defjam-first-stage-entry", Game::millennium, Platform::amiga, "en", "m68000", "$41000", "verified-static", "native first ILLEGAL handler through $410fc", "PRESERVATION.md#millennium-amiga-raw-loader-evidence"},
    {"millennium-atari-equinox-bootstrap", "ba1174123a0531abeab5788f4ac87a3c2500696bf1c87a7efd209441b3ebdf01", "millennium-atari-equinox-bootstrap", Game::millennium, Platform::atari_st, "en", "m68000", "$77000", "verified-static", "read-only parser and diagnostics", "PRESERVATION.md#millennium-atari-st-relocation-evidence"},
    {"millennium-atari-equinox-root-inventory", "ba1174123a0531abeab5788f4ac87a3c2500696bf1c87a7efd209441b3ebdf01", "millennium-atari-equinox-root-inventory", Game::millennium, Platform::atari_st, "en", "m68000", "disk+0x00000", "verified-static", "read-only parser and diagnostics", "PRESERVATION.md#amiga-and-atari-st-corpus-boundary-census"},
    {"millennium-atari-equinox-prg-chain", "ba1174123a0531abeab5788f4ac87a3c2500696bf1c87a7efd209441b3ebdf01", "millennium-atari-equinox-prg-chain", Game::millennium, Platform::atari_st, "en", "m68000", "MILENIUM.TOS+0x0", "verified-static", "read-only parser and diagnostics", "PRESERVATION.md#amiga-and-atari-st-corpus-boundary-census"},
    {"millennium-atari-equinox-config-chain", "ba1174123a0531abeab5788f4ac87a3c2500696bf1c87a7efd209441b3ebdf01", "millennium-atari-equinox-config-chain", Game::millennium, Platform::atari_st, "en", "m68000", "MILL22A.INF+0x0", "verified-static", "read-only parser and diagnostics", "PRESERVATION.md#millennium-atari-st-relocation-evidence"},
    {"millennium-atari-equinox-auxiliary-resource", "ba1174123a0531abeab5788f4ac87a3c2500696bf1c87a7efd209441b3ebdf01", "millennium-atari-equinox-auxiliary-resource", Game::millennium, Platform::atari_st, "en", "m68000", "MILL22B.INF+0x11600", "verified-static", "read-only parser and diagnostics", "PRESERVATION.md#millennium-atari-st-relocation-evidence"},
    {"millennium-atari-physical-control-text", "ba1174123a0531abeab5788f4ac87a3c2500696bf1c87a7efd209441b3ebdf01", "millennium-atari-physical-control-text", Game::millennium, Platform::atari_st, "en", "m68000", "disk+0x12420", "verified-static", "read-only parser and diagnostics", "PRESERVATION.md#amiga-and-atari-st-corpus-boundary-census"},
    {"millennium-dos-title-flow", "e6e7044b25877fdf8b10d16d2f395886d9957953144ae15ca630cda9cab2a123", "millennium-dos-title-flow", Game::millennium, Platform::dos, "en", "i8086", "TITLES.EXE+$1b80", "verified-static", "read-only parser and diagnostics", "PRESERVATION.md#millennium-dos-execution-model"},
    {"millennium-dos-save-layout", "e6e7044b25877fdf8b10d16d2f395886d9957953144ae15ca630cda9cab2a123", "millennium-dos-save-layout", Game::millennium, Platform::dos, "en", "i8086", "2200SAVE.I+0x0", "verified-static", "read-only parser and diagnostics", "PRESERVATION.md#millennium-dos-gx-canvas"},
    {"millennium-dos-gx-canvas", "e6e7044b25877fdf8b10d16d2f395886d9957953144ae15ca630cda9cab2a123", "millennium-dos-gx-canvas", Game::millennium, Platform::dos, "en", "i8086", "GX.LIB+0x6", "verified-static", "read-only parser and diagnostics", "PRESERVATION.md#millennium-dos-gx-canvas"},
    {"millennium-dos-game-flow", "e6e7044b25877fdf8b10d16d2f395886d9957953144ae15ca630cda9cab2a123", "millennium-dos-game-flow", Game::millennium, Platform::dos, "en", "i8086", "2200AD.EXE+0x0", "verified-static", "read-only parser and diagnostics", "PRESERVATION.md#millennium-dos-execution-model"},
    {"millennium-dos-english-startup-callees", "e6e7044b25877fdf8b10d16d2f395886d9957953144ae15ca630cda9cab2a123", "millennium-dos-english-startup-callees", Game::millennium, Platform::dos, "en", "i8086", "2200AD.EXE+$d1a1", "verified-static", "read-only parser and diagnostics", "PRESERVATION.md#millennium-dos-execution-model"},
    {"millennium-dos-english-startup-followups", "e6e7044b25877fdf8b10d16d2f395886d9957953144ae15ca630cda9cab2a123", "millennium-dos-english-startup-followups", Game::millennium, Platform::dos, "en", "i8086", "2200AD.EXE+$0356", "verified-static", "read-only parser and diagnostics", "PRESERVATION.md#millennium-dos-execution-model"},
    {"millennium-dos-static-data", "e6e7044b25877fdf8b10d16d2f395886d9957953144ae15ca630cda9cab2a123", "millennium-dos-static-data", Game::millennium, Platform::dos, "en", "i8086", "2200AD4.BIN+0x0", "verified-static", "read-only parser and diagnostics", "PRESERVATION.md#millennium-dos-gx-canvas"},
    {"millennium-dos-gx-overlay", "e6e7044b25877fdf8b10d16d2f395886d9957953144ae15ca630cda9cab2a123", "millennium-dos-gx-overlay", Game::millennium, Platform::dos, "en", "i8086", "2200GX.EXE+0x0", "verified-static", "trace-gated sparse GX startup session", "PRESERVATION.md#millennium-dos-gx-startup-record-boundary"},
    {"millennium-dos-ega-video", "e6e7044b25877fdf8b10d16d2f395886d9957953144ae15ca630cda9cab2a123", "millennium-dos-ega-video", Game::millennium, Platform::dos, "en", "i8086", "EGA640.BIN+0x0", "verified-static", "read-only parser and diagnostics", "PRESERVATION.md#millennium-dos-execution-model"},
    {"millennium-dos-mcga-video", "e6e7044b25877fdf8b10d16d2f395886d9957953144ae15ca630cda9cab2a123", "millennium-dos-mcga-video", Game::millennium, Platform::dos, "en", "i8086", "MCGA.BIN+0x0", "verified-static", "read-only parser and diagnostics", "PRESERVATION.md#millennium-dos-execution-model"},
    {"millennium-dos-last-screen", "e6e7044b25877fdf8b10d16d2f395886d9957953144ae15ca630cda9cab2a123", "millennium-dos-last-screen", Game::millennium, Platform::dos, "en", "i8086", "LAST.LIB+0x6", "verified-static", "read-only parser and diagnostics", "PRESERVATION.md#millennium-dos-lastlib-screen-evidence"},
    {"millennium-dos-title-library", "e6e7044b25877fdf8b10d16d2f395886d9957953144ae15ca630cda9cab2a123", "millennium-dos-title-library", Game::millennium, Platform::dos, "en", "i8086", "TITLE.LIB+0x0", "verified-static", "read-only parser and diagnostics", "PRESERVATION.md#millennium-dos-execution-model"},
    {"millennium-dos-launcher", "e6e7044b25877fdf8b10d16d2f395886d9957953144ae15ca630cda9cab2a123", "millennium-dos-launcher", Game::millennium, Platform::dos, "en", "i8086", "MILL.COM+0x0", "verified-static", "read-only parser and diagnostics", "PRESERVATION.md#millennium-dos-execution-model"},
    {"millennium-dos-sound-selection", "e6e7044b25877fdf8b10d16d2f395886d9957953144ae15ca630cda9cab2a123", "millennium-dos-sound-selection", Game::millennium, Platform::dos, "en", "i8086", "$0511", "verified-static", "read-only parser and diagnostics", "PRESERVATION.md#millennium-dos-execution-model"},
    {"millennium-dos-sound-blaster-driver", "e6e7044b25877fdf8b10d16d2f395886d9957953144ae15ca630cda9cab2a123", "millennium-dos-sound-blaster-driver", Game::millennium, Platform::dos, "en", "i8086", "SSBL.DRV+0x0", "verified-static", "read-only parser and diagnostics", "PRESERVATION.md#millennium-dos-execution-model"},
    {"millennium-dos-covox-driver", "e6e7044b25877fdf8b10d16d2f395886d9957953144ae15ca630cda9cab2a123", "millennium-dos-covox-driver", Game::millennium, Platform::dos, "en", "i8086", "SCVX.DRV+0x0", "verified-static", "read-only parser and diagnostics", "PRESERVATION.md#millennium-dos-execution-model"},
    {"millennium-dos-sfx1-voice", "e6e7044b25877fdf8b10d16d2f395886d9957953144ae15ca630cda9cab2a123", "millennium-dos-sfx1-voice", Game::millennium, Platform::dos, "en", "i8086", "SFX1.VOC+0x0", "verified-static", "read-only parser and diagnostics", "PRESERVATION.md#millennium-dos-execution-model"},
    {"millennium-dos-spanish-startup", "b40cc2f2c39cdb476b4a82bda7bffed1c80decdfb7fe41b1a38bf54343e0c0a4", "millennium-dos-spanish-startup", Game::millennium, Platform::dos, "es", "i8086", "boot+0x0", "verified-static", "read-only parser and diagnostics", "PRESERVATION.md#millennium-spanish-dos-floppy-evidence"},
    {"millennium-dos-spanish-title-boundary", "b40cc2f2c39cdb476b4a82bda7bffed1c80decdfb7fe41b1a38bf54343e0c0a4", "millennium-dos-spanish-title-boundary", Game::millennium, Platform::dos, "es", "i8086", "TITLE.LIB+0x6", "verified-static", "read-only parser and diagnostics", "PRESERVATION.md#millennium-spanish-dos-floppy-evidence"},
    {"millennium-dos-spanish-static-text", "b40cc2f2c39cdb476b4a82bda7bffed1c80decdfb7fe41b1a38bf54343e0c0a4", "millennium-dos-spanish-static-text", Game::millennium, Platform::dos, "es", "i8086", "2200AD4.BIN+$03db", "verified-static", "read-only parser and diagnostics", "PRESERVATION.md#millennium-spanish-dos-floppy-evidence"},
    {"millennium-dos-spanish-launch-manual", "b40cc2f2c39cdb476b4a82bda7bffed1c80decdfb7fe41b1a38bf54343e0c0a4", "millennium-dos-spanish-launch-manual", Game::millennium, Platform::dos, "es", "i8086", "MILL.BAT+0x0", "verified-static", "read-only parser and diagnostics", "PRESERVATION.md#millennium-spanish-dos-floppy-evidence"},
    {"millennium-amiga-defjam-direct-bootstrap", "ec0424445d494809d2661492e289af71b056a429dde13b053a472ccc8347d4dd", "millennium-amiga-defjam-direct-bootstrap", Game::millennium, Platform::amiga, "en", "m68000", "$41000", "verified-static", "read-only parser and diagnostics", "PRESERVATION.md#direct-container-recognition"},
    {"millennium-amiga-defjam-direct-first-stage-entry", "ec0424445d494809d2661492e289af71b056a429dde13b053a472ccc8347d4dd", "millennium-amiga-defjam-direct-first-stage-entry", Game::millennium, Platform::amiga, "en", "m68000", "$41000", "verified-static", "native first ILLEGAL handler through $410fc", "PRESERVATION.md#millennium-amiga-raw-loader-evidence"},
}};

} // namespace

std::span<const RecoveryMapEntry> recovery_map() { return entries; }

bool recovery_map_manifest_is_valid() {
    const auto profiles = parser_profile_manifest();
    if (entries.size() != profiles.size()) return false;
    for (const auto& profile : profiles) {
        const auto matches = std::count_if(entries.begin(), entries.end(), [&profile](const auto& entry) {
            return entry.release_sha256 == profile.release_sha256
                && entry.parser_profile_id == profile.id;
        });
        if (matches != 1) return false;
    }
    const auto releases = release_manifest();
    for (const auto& entry : entries) {
        if (entry.id.empty() || entry.parser_profile_id.empty() || entry.cpu.empty()
            || entry.source_address.empty() || entry.evidence_level != "verified-static"
            || entry.runtime_status.empty()
            || !entry.documentation_anchor.starts_with("PRESERVATION.md#")) return false;
        if ((entry.platform == Platform::dos && entry.cpu != "i8086")
            || ((entry.platform == Platform::amiga || entry.platform == Platform::atari_st)
                && entry.cpu != "m68000")) return false;
        const auto release_matches = std::count_if(releases.begin(), releases.end(), [&entry](const auto& release) {
            return release.sha256 == entry.release_sha256 && release.game == entry.game
                && release.platform == entry.platform && release.language == entry.language;
        });
        const auto entry_matches = std::count_if(entries.begin(), entries.end(), [&entry](const auto& candidate) {
            return candidate.id == entry.id;
        });
        const auto profile_matches = std::count_if(profiles.begin(), profiles.end(), [&entry](const auto& profile) {
            return profile.release_sha256 == entry.release_sha256
                && profile.id == entry.parser_profile_id;
        });
        if (release_matches != 1 || entry_matches != 1 || profile_matches != 1) return false;
    }
    return true;
}

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
