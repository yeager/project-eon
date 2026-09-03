#pragma once

#include <optional>
#include <string_view>

namespace eon {

struct StartupBoundary {
    std::string_view release_sha256;
    std::string_view parser_profile_id;
    std::string_view source_address;
    std::string_view unresolved;
};

// Presentation provenance only: this is not a loader, dispatcher, or media
// reader. Unknown releases intentionally have no active start boundary.
[[nodiscard]] std::optional<StartupBoundary> startup_boundary_for_release(std::string_view release_sha256);
// Startup provenance must name each recognised release exactly once and bind
// that row to its own parser profile. It remains presentation metadata only.
[[nodiscard]] bool startup_boundary_manifest_is_valid();

} // namespace eon
