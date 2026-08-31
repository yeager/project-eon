#include "engine/menu_runtime_launch.hpp"

namespace eon {

RuntimeCandidateLaunchResult launch_runtime_candidate(const std::optional<LaunchRequest>& candidate,
    const std::vector<ReleaseArchive>& releases, ReleaseRuntimeCoordinator& coordinator) {
    const auto gate = admit_runtime_launch(coordinator, candidate, releases);
    if (!gate.accepted() || !coordinator.active()) return {gate.admission, std::nullopt};
    return {gate.admission, *coordinator.active()};
}

MenuRuntimeLaunchResult launch_menu_runtime(const LauncherSessionState& session,
    const LaunchRequest& base, const std::vector<ReleaseArchive>& releases,
    ReleaseRuntimeCoordinator& coordinator) {
    // `launch_request` is the sole menu conversion: it carries the exact
    // game/platform/language/hash route and the selected presentation, but
    // has neither a source path nor an adapter.  The common gate below then
    // resolves that identity against this scanner snapshot and rehashes its
    // outer archive before publishing anything to SDL.
    const auto candidate = session.launch_request(base);
    const auto result = launch_runtime_candidate(candidate, releases, coordinator);
    return {{result.admission}, result.active_launch};
}

} // namespace eon
