#include "platform/platform_coverage.hpp"
#include "engine/release_runtime_capability.hpp"

namespace eon {

PlatformCoverage platform_coverage(const Game game, const Platform platform) {
    switch (game) {
    case Game::millennium:
        switch (platform) {
        case Platform::dos: return PlatformCoverage::recovered_startup;
        case Platform::amiga: return PlatformCoverage::bootstrap_only;
        case Platform::atari_st: return PlatformCoverage::bootstrap_only;
        }
        break;
    case Game::deuteros:
        switch (platform) {
        // DOS is not a supported Deuteros platform. Keep the total function
        // conservative for diagnostic callers that hold an untrusted pair.
        case Platform::dos: return PlatformCoverage::bootstrap_only;
        case Platform::amiga: return PlatformCoverage::recovered_opening;
        case Platform::atari_st: return PlatformCoverage::bootstrap_only;
        }
        break;
    }
    return PlatformCoverage::bootstrap_only;
}

PlatformCoverage platform_coverage(const ReleaseArchive& release) {
    const auto capability = release_runtime_capability_for(release);
    return capability ? capability->coverage : PlatformCoverage::bootstrap_only;
}

std::string_view name(const PlatformCoverage coverage) {
    switch (coverage) {
    case PlatformCoverage::recovered_startup: return "RECOVERED STARTUP";
    case PlatformCoverage::recovered_opening: return "RECOVERED OPENING";
    case PlatformCoverage::bootstrap_only: return "BOOTSTRAP ONLY";
    }
    return "BOOTSTRAP ONLY";
}

} // namespace eon
