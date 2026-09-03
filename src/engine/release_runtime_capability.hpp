#pragma once

#include "engine/runtime_session.hpp"
#include "platform/platform_coverage.hpp"

#include <optional>
#include <string_view>
#include <vector>

namespace eon {

// A capability is a hash-bound statement of what Eon may construct for one
// original release. It is not an emulator configuration or parity claim.
enum class ReleaseRuntimeAdapter { millennium_dos, millennium_amiga, millennium_atari,
    deuteros_amiga, deuteros_atari };

struct ReleaseRuntimeCapability {
    std::string_view release_sha256;
    Game game;
    Platform platform;
    std::string_view language;
    ReleaseRuntimeAdapter adapter;
    PlatformCoverage coverage;
    RuntimeSessionKind initial_kind;
    RuntimeSessionBoundary initial_boundary;
    RuntimeSessionCapabilities initial_capabilities;
};

[[nodiscard]] const std::vector<ReleaseRuntimeCapability>& release_runtime_capabilities();
// The compiled native-adapter map must be a one-to-one companion of the
// release manifest. This detects an accidental duplicate, omission, or tuple
// mismatch before a coordinator can select the first matching row.
[[nodiscard]] bool release_runtime_capability_manifest_is_valid();
[[nodiscard]] std::optional<ReleaseRuntimeCapability> release_runtime_capability_for(
    const ReleaseArchive& release);

} // namespace eon
