#pragma once

#include "engine/release_runtime.hpp"

namespace eon {

// The card menu has exactly one transition into a live release adapter.  This
// SDL-free gate deliberately receives session state, scanner identities and
// the runtime coordinator rather than card indexes, paths or adapters.  It
// makes a successful result observable only through the coordinator's final,
// rehashed identity.
struct MenuRuntimeLaunchResult {
    RuntimeLaunchAdmission admission;
    std::optional<ResolvedLaunchRequest> active_launch;

    [[nodiscard]] bool accepted() const {
        return admission.accepted() && active_launch.has_value();
    }
};

[[nodiscard]] MenuRuntimeLaunchResult launch_menu_runtime(
    const LauncherSessionState& session, const LaunchRequest& base,
    const std::vector<ReleaseArchive>& releases, ReleaseRuntimeCoordinator& coordinator);

} // namespace eon
