#include "data/startup_boundary.hpp"

#include "data/release_manifest.hpp"

#include <array>

namespace eon {
namespace {
constexpr std::array boundaries{
    StartupBoundary{"e6e7044b25877fdf8b10d16d2f395886d9957953144ae15ca630cda9cab2a123", "millennium-dos-launcher", "MILL.COM+0x0", "DOS, driver, child-process and private-interrupt results"},
    StartupBoundary{"b40cc2f2c39cdb476b4a82bda7bffed1c80decdfb7fe41b1a38bf54343e0c0a4", "millennium-dos-spanish-startup", "disk+0x0", "FAT12 executable-chain and DOS ABI observations"},
    StartupBoundary{"2e27d7aeb8b8b7f2a75eda45b456ab42775a706aa85516c85e61ce94ec9eb400", "millennium-amiga-defjam-bootstrap", "ADF+0x400", "raw-stage invocation, relocation and Amiga ABI"},
    StartupBoundary{"ec0424445d494809d2661492e289af71b056a429dde13b053a472ccc8347d4dd", "millennium-amiga-defjam-direct-bootstrap", "ADF+0x400", "raw-stage invocation, relocation and Amiga ABI"},
    StartupBoundary{"0056e9fe1bae35ba61660a4b563772e4037e8a6390d1f579ec160044e80a1d69", "millennium-atari-equinox-direct-bootstrap", "disk+0x0", "TOS executable-chain and runtime load map"},
    StartupBoundary{"ba1174123a0531abeab5788f4ac87a3c2500696bf1c87a7efd209441b3ebdf01", "millennium-atari-equinox-bootstrap", "disk+0x0", "TOS executable-chain and runtime load map"},
    StartupBoundary{"f4dc8dd1c27c5d389837783becd9b95ab09b78baf40e94e39e2b7e590e470e04", "deuteros-amiga-clean-main-stage", "ADF+0x5800 -> $20000", "Exec/graphics/callback returns and title input"},
    StartupBoundary{"c6856d0a7ccda925289c60f0675e7aaed616f8a0289c74698e87e1ee11e6c653", "deuteros-atari-replicants-first-stage", "disk+0x4ec00 -> $1200", "XBIOS results, RAM vectors and callback dispatch"},
};
}
std::optional<StartupBoundary> startup_boundary_for_release(const std::string_view hash) {
    for (const auto& boundary : boundaries) if (boundary.release_sha256 == hash
        && release_has_parser_profile(hash, boundary.parser_profile_id)) return boundary;
    return std::nullopt;
}
} // namespace eon
