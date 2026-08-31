#include "engine/menu_runtime_launch.hpp"

namespace eon {

MenuRuntimeLaunchResult launch_menu_runtime(const LauncherSessionState& session,
    const LaunchRequest& base, const std::vector<ReleaseArchive>& releases,
    ReleaseRuntimeCoordinator& coordinator) {
    // `launch_request` is the sole menu conversion: it carries the exact
    // game/platform/language/hash route and the selected presentation, but
    // has neither a source path nor an adapter.  The common gate below then
    // resolves that identity against this scanner snapshot and rehashes its
    // outer archive before publishing anything to SDL.
    const auto candidate = session.launch_request(base);
    const auto admission = admit_runtime_launch(coordinator, candidate, releases);
    if (!admission.accepted() || !coordinator.active()) return {admission, std::nullopt};
    return {admission, *coordinator.active()};
}

} // namespace eon
