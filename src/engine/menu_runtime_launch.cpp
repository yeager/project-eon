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

RuntimeInputDisposition LauncherRuntimeController::observe_input(
    const RuntimeInputObservation& observation) {
    return coordinator_.observe_input(observation);
}

std::optional<MillenniumDosPresentationSnapshot>
LauncherRuntimeController::millennium_dos_presentation() const {
    return coordinator_.millennium_dos_presentation();
}

std::optional<MillenniumDosStartupInputSnapshot>
LauncherRuntimeController::millennium_dos_startup_input() const {
    return coordinator_.millennium_dos_startup_input();
}

std::optional<MillenniumDosStaticDispatchDiagnostics>
LauncherRuntimeController::millennium_dos_static_dispatch_diagnostics() const {
    return coordinator_.millennium_dos_static_dispatch_diagnostics();
}

std::optional<DeuterosAmigaVmEvents> LauncherRuntimeController::tick_deuteros_amiga_opening() {
    return coordinator_.tick_deuteros_amiga_opening();
}

std::optional<std::vector<float>>
LauncherRuntimeController::render_deuteros_amiga_opening_audio(const std::size_t frames) {
    return coordinator_.render_deuteros_amiga_opening_audio(frames);
}

std::optional<DeuterosAmigaOpeningCheckpoint>
LauncherRuntimeController::deuteros_amiga_opening_checkpoint() const {
    return coordinator_.deuteros_amiga_opening_checkpoint();
}

std::optional<DeuterosAmigaOpeningPresentationSnapshot>
LauncherRuntimeController::deuteros_amiga_opening_presentation() const {
    return coordinator_.deuteros_amiga_opening_presentation();
}

std::optional<DeuterosAmigaTitleStageBoundarySnapshot>
LauncherRuntimeController::deuteros_amiga_title_stage_boundary() const {
    return coordinator_.deuteros_amiga_title_stage_boundary();
}

std::optional<DeuterosAtariBootstrapCheckpoint>
LauncherRuntimeController::deuteros_atari_bootstrap_checkpoint() const {
    return coordinator_.deuteros_atari_bootstrap_checkpoint();
}

std::optional<DeuterosAtariBootstrapPresentationSnapshot>
LauncherRuntimeController::deuteros_atari_bootstrap_presentation() const {
    return coordinator_.deuteros_atari_bootstrap_presentation();
}

std::optional<MillenniumAmigaBootstrapPresentationSnapshot>
LauncherRuntimeController::millennium_amiga_bootstrap_presentation() const {
    return coordinator_.millennium_amiga_bootstrap_presentation();
}

std::optional<MillenniumAtariBootstrapPresentationSnapshot>
LauncherRuntimeController::millennium_atari_bootstrap_presentation() const {
    return coordinator_.millennium_atari_bootstrap_presentation();
}

std::optional<RuntimeSessionSnapshot> LauncherRuntimeController::session_snapshot() const {
    return coordinator_.session_snapshot();
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
