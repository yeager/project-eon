#include "engine/menu_runtime_launch.hpp"

namespace eon {

RuntimeCandidateLaunchResult launch_runtime_candidate(const std::optional<LaunchRequest>& candidate,
    const std::vector<ReleaseArchive>& releases, ReleaseRuntimeCoordinator& coordinator) {
    const auto gate = admit_runtime_launch(coordinator, candidate, releases);
    if (!gate.accepted() || !coordinator.active()) return {gate.admission, gate.rejection, std::nullopt};
    return {gate.admission, gate.rejection, *coordinator.active()};
}

RuntimeCandidateLaunchResult LauncherRuntimeController::launch_direct(
    const LaunchRequest& candidate, const std::vector<ReleaseArchive>& releases) {
    return launch_runtime_candidate(candidate, releases, coordinator_);
}

RuntimeCandidateLaunchResult LauncherRuntimeController::launch_menu(
    const LauncherSessionState& session, const LaunchRequest& base,
    const std::vector<ReleaseArchive>& releases) {
    return launch_runtime_candidate(session.launch_request(base), releases, coordinator_);
}

bool LauncherRuntimeController::requires_revocation_for(const LauncherSourceIdentity& source) const {
    if (!coordinator_.active()) return false;
    const auto& launch = *coordinator_.active();
    return LauncherSourceIdentity{launch.release.game, launch.release.platform, launch.release.language,
        launch.release.sha256} != source;
}

void LauncherRuntimeController::reset() {
    coordinator_.reset();
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
    return {{result.admission, result.rejection}, result.active_launch};
}

} // namespace eon
