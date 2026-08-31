#include "platform/platform_coverage.hpp"

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
    // Spanish DOS has title presentation evidence but no recovered executable
    // hand-off. It must not inherit the English DOS startup claim.
    if (release.game == Game::millennium && release.platform == Platform::dos
        && release.language == "es"
        && release.sha256 == "b40cc2f2c39cdb476b4a82bda7bffed1c80decdfb7fe41b1a38bf54343e0c0a4") {
        return PlatformCoverage::bootstrap_only;
    }
    return platform_coverage(release.game, release.platform);
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
