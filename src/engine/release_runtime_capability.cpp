#include "engine/release_runtime_capability.hpp"

#include "data/release_manifest.hpp"

#include <algorithm>
#include <array>

namespace eon {
namespace {
constexpr std::array<ReleaseRuntimeCapability, 8> capabilities{{
    {"e6e7044b25877fdf8b10d16d2f395886d9957953144ae15ca630cda9cab2a123", Game::millennium, Platform::dos, "en", ReleaseRuntimeAdapter::millennium_dos, PlatformCoverage::recovered_startup, RuntimeSessionKind::millennium_dos_title, RuntimeSessionBoundary::recovered_presentation_boundary, {true, false, true}},
    {"b40cc2f2c39cdb476b4a82bda7bffed1c80decdfb7fe41b1a38bf54343e0c0a4", Game::millennium, Platform::dos, "es", ReleaseRuntimeAdapter::millennium_dos, PlatformCoverage::bootstrap_only, RuntimeSessionKind::millennium_dos_title, RuntimeSessionBoundary::recovered_presentation_boundary, {true, false, true}},
    {"2e27d7aeb8b8b7f2a75eda45b456ab42775a706aa85516c85e61ce94ec9eb400", Game::millennium, Platform::amiga, "en", ReleaseRuntimeAdapter::millennium_amiga, PlatformCoverage::bootstrap_only, RuntimeSessionKind::millennium_amiga_bootstrap, RuntimeSessionBoundary::bootstrap_boundary, {}},
    {"ec0424445d494809d2661492e289af71b056a429dde13b053a472ccc8347d4dd", Game::millennium, Platform::amiga, "en", ReleaseRuntimeAdapter::millennium_amiga, PlatformCoverage::bootstrap_only, RuntimeSessionKind::millennium_amiga_bootstrap, RuntimeSessionBoundary::bootstrap_boundary, {}},
    {"0056e9fe1bae35ba61660a4b563772e4037e8a6390d1f579ec160044e80a1d69", Game::millennium, Platform::atari_st, "en", ReleaseRuntimeAdapter::millennium_atari, PlatformCoverage::bootstrap_only, RuntimeSessionKind::millennium_atari_bootstrap, RuntimeSessionBoundary::bootstrap_boundary, {}},
    {"ba1174123a0531abeab5788f4ac87a3c2500696bf1c87a7efd209441b3ebdf01", Game::millennium, Platform::atari_st, "en", ReleaseRuntimeAdapter::millennium_atari, PlatformCoverage::bootstrap_only, RuntimeSessionKind::millennium_atari_bootstrap, RuntimeSessionBoundary::bootstrap_boundary, {}},
    {"f4dc8dd1c27c5d389837783becd9b95ab09b78baf40e94e39e2b7e590e470e04", Game::deuteros, Platform::amiga, "en", ReleaseRuntimeAdapter::deuteros_amiga, PlatformCoverage::recovered_opening, RuntimeSessionKind::deuteros_amiga_opening, RuntimeSessionBoundary::recovered_presentation_boundary, {true, true, true}},
    {"c6856d0a7ccda925289c60f0675e7aaed616f8a0289c74698e87e1ee11e6c653", Game::deuteros, Platform::atari_st, "en", ReleaseRuntimeAdapter::deuteros_atari, PlatformCoverage::bootstrap_only, RuntimeSessionKind::deuteros_atari_bootstrap, RuntimeSessionBoundary::bootstrap_boundary, {}},
}};
}

const std::vector<ReleaseRuntimeCapability>& release_runtime_capabilities() {
    static const std::vector<ReleaseRuntimeCapability> values(capabilities.begin(), capabilities.end());
    return values;
}

bool release_runtime_capability_manifest_is_valid() {
    const auto manifest = release_manifest();
    if (capabilities.size() != manifest.size()) return false;
    for (const auto& release : manifest) {
        const auto matches = std::count_if(capabilities.begin(), capabilities.end(),
            [&release](const auto& capability) {
                return capability.release_sha256 == release.sha256
                    && capability.game == release.game && capability.platform == release.platform
                    && capability.language == release.language;
            });
        if (matches != 1) return false;
    }
    for (const auto& capability : capabilities) {
        if (!runtime_session_declaration_is_valid(capability.initial_kind,
                capability.initial_boundary, capability.initial_capabilities)) return false;
        const auto matches = std::count_if(manifest.begin(), manifest.end(),
            [&capability](const auto& release) {
                return release.sha256 == capability.release_sha256
                    && release.game == capability.game && release.platform == capability.platform
                    && release.language == capability.language;
            });
        if (matches != 1) return false;
    }
    return true;
}

std::optional<ReleaseRuntimeCapability> release_runtime_capability_for(const ReleaseArchive& release) {
    for (const auto& capability : capabilities) {
        if (capability.release_sha256 == release.sha256 && capability.game == release.game
            && capability.platform == release.platform && capability.language == release.language) return capability;
    }
    return std::nullopt;
}
} // namespace eon
