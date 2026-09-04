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

std::optional<MillenniumDosNativeProcessCheckpoint>
LauncherRuntimeController::millennium_dos_native_process_checkpoint() const {
    return coordinator_.millennium_dos_native_process_checkpoint();
}

MillenniumDosGxActiveTraceAdmission
LauncherRuntimeController::admit_active_millennium_dos_gx_startup_reference_trace(
    const ReferenceTrace& trace) {
    return coordinator_.admit_active_millennium_dos_gx_startup_reference_trace(trace);
}

std::optional<MillenniumDosGxStartupCheckpoint>
LauncherRuntimeController::millennium_dos_gx_startup_checkpoint() const {
    return coordinator_.millennium_dos_gx_startup_checkpoint();
}

MillenniumDosPostOverlayObservationResult
LauncherRuntimeController::observe_millennium_dos_post_overlay_private_interrupt_return(
    const MillenniumDosPostOverlayPrivateInterruptReturnObservation observation) {
    return coordinator_.observe_millennium_dos_post_overlay_private_interrupt_return(observation);
}

MillenniumDosPostOverlayObservationResult
LauncherRuntimeController::observe_millennium_dos_post_overlay_call_return(
    const MillenniumDosPostOverlayCallReturnObservation observation) {
    return coordinator_.observe_millennium_dos_post_overlay_call_return(observation);
}

MillenniumDosPostOverlayObservationResult
LauncherRuntimeController::observe_millennium_dos_post_overlay_al(
    const MillenniumDosPostOverlayAlObservation observation) {
    return coordinator_.observe_millennium_dos_post_overlay_al(observation);
}

MillenniumDosPostOverlayObservationResult
LauncherRuntimeController::observe_millennium_dos_post_overlay_runtime_byte(
    const MillenniumDosPostOverlayRuntimeByteObservation observation) {
    return coordinator_.observe_millennium_dos_post_overlay_runtime_byte(observation);
}

std::optional<MillenniumDosPostOverlayLoopCheckpoint>
LauncherRuntimeController::millennium_dos_post_overlay_loop_checkpoint() const {
    return coordinator_.millennium_dos_post_overlay_loop_checkpoint();
}

#define EON_LAUNCHER_TENTH_PROXY(name, type) \
MillenniumDosTenthFunctionObservationResult LauncherRuntimeController::name( \
    const type observation) { return coordinator_.name(observation); }
EON_LAUNCHER_TENTH_PROXY(observe_millennium_dos_tenth_function_dispatch, MillenniumDosTenthFunctionDispatchObservation)
EON_LAUNCHER_TENTH_PROXY(observe_millennium_dos_tenth_function_word, MillenniumDosTenthFunctionWordObservation)
EON_LAUNCHER_TENTH_PROXY(observe_millennium_dos_tenth_function_byte, MillenniumDosTenthFunctionByteObservation)
EON_LAUNCHER_TENTH_PROXY(observe_millennium_dos_tenth_function_call_return, MillenniumDosTenthFunctionCallReturnObservation)
EON_LAUNCHER_TENTH_PROXY(observe_millennium_dos_tenth_function_zero_flag, MillenniumDosTenthFunctionZeroFlagObservation)
EON_LAUNCHER_TENTH_PROXY(observe_millennium_dos_tenth_function_bl, MillenniumDosTenthFunctionBlObservation)
#undef EON_LAUNCHER_TENTH_PROXY

std::optional<MillenniumDosTenthFunctionCheckpoint>
LauncherRuntimeController::millennium_dos_tenth_function_checkpoint() const {
    return coordinator_.millennium_dos_tenth_function_checkpoint();
}

#define EON_LAUNCHER_SEVENTH_PROXY(name, type) \
MillenniumDosSeventhFunctionObservationResult LauncherRuntimeController::name( \
    const type observation) { return coordinator_.name(observation); }
EON_LAUNCHER_SEVENTH_PROXY(observe_millennium_dos_seventh_function_dispatch, MillenniumDosSeventhFunctionDispatchObservation)
EON_LAUNCHER_SEVENTH_PROXY(observe_millennium_dos_seventh_function_word, MillenniumDosSeventhFunctionWordObservation)
EON_LAUNCHER_SEVENTH_PROXY(observe_millennium_dos_seventh_function_byte, MillenniumDosSeventhFunctionByteObservation)
EON_LAUNCHER_SEVENTH_PROXY(observe_millennium_dos_seventh_function_call_return, MillenniumDosSeventhFunctionCallReturnObservation)
EON_LAUNCHER_SEVENTH_PROXY(observe_millennium_dos_seventh_function_returned_bx, MillenniumDosSeventhFunctionReturnedBxObservation)
#undef EON_LAUNCHER_SEVENTH_PROXY

std::optional<MillenniumDosSeventhFunctionCheckpoint>
LauncherRuntimeController::millennium_dos_seventh_function_checkpoint() const {
    return coordinator_.millennium_dos_seventh_function_checkpoint();
}

#define EON_LAUNCHER_SIXTH_PROXY(name, type) \
MillenniumDosSixthFunctionObservationResult LauncherRuntimeController::name( \
    const type observation) { return coordinator_.name(observation); }
EON_LAUNCHER_SIXTH_PROXY(observe_millennium_dos_sixth_function_dispatch, MillenniumDosSixthFunctionDispatchObservation)
EON_LAUNCHER_SIXTH_PROXY(observe_millennium_dos_sixth_function_word, MillenniumDosSixthFunctionWordObservation)
EON_LAUNCHER_SIXTH_PROXY(observe_millennium_dos_sixth_function_byte, MillenniumDosSixthFunctionByteObservation)
EON_LAUNCHER_SIXTH_PROXY(observe_millennium_dos_sixth_function_call_return, MillenniumDosSixthFunctionCallReturnObservation)
EON_LAUNCHER_SIXTH_PROXY(observe_millennium_dos_sixth_function_bl, MillenniumDosSixthFunctionBlObservation)
#undef EON_LAUNCHER_SIXTH_PROXY

std::optional<MillenniumDosSixthFunctionCheckpoint>
LauncherRuntimeController::millennium_dos_sixth_function_checkpoint() const {
    return coordinator_.millennium_dos_sixth_function_checkpoint();
}

#define EON_LAUNCHER_EIGHTH_PROXY(name, type) \
MillenniumDosEighthFunctionObservationResult LauncherRuntimeController::name( \
    const type observation) { return coordinator_.name(observation); }
EON_LAUNCHER_EIGHTH_PROXY(observe_millennium_dos_eighth_function_dispatch,
    MillenniumDosEighthFunctionDispatchObservation)
EON_LAUNCHER_EIGHTH_PROXY(observe_millennium_dos_eighth_function_call_return,
    MillenniumDosEighthFunctionCallReturnObservation)
EON_LAUNCHER_EIGHTH_PROXY(observe_millennium_dos_eighth_function_bl,
    MillenniumDosEighthFunctionBlObservation)
#undef EON_LAUNCHER_EIGHTH_PROXY

std::optional<MillenniumDosEighthFunctionCheckpoint>
LauncherRuntimeController::millennium_dos_eighth_function_checkpoint() const {
    return coordinator_.millennium_dos_eighth_function_checkpoint();
}
#define EON_LAUNCHER_NINTH(name,type) MillenniumDosNinthFunctionObservationResult LauncherRuntimeController::name(const type o) { return coordinator_.name(o); }
EON_LAUNCHER_NINTH(observe_millennium_dos_ninth_function_dispatch,MillenniumDosNinthFunctionDispatchObservation)
EON_LAUNCHER_NINTH(observe_millennium_dos_ninth_function_word,MillenniumDosNinthFunctionWordObservation)
EON_LAUNCHER_NINTH(observe_millennium_dos_ninth_function_byte,MillenniumDosNinthFunctionByteObservation)
EON_LAUNCHER_NINTH(observe_millennium_dos_ninth_function_call_return,MillenniumDosNinthFunctionCallReturnObservation)
#undef EON_LAUNCHER_NINTH
std::optional<MillenniumDosNinthFunctionCheckpoint> LauncherRuntimeController::millennium_dos_ninth_function_checkpoint() const { return coordinator_.millennium_dos_ninth_function_checkpoint(); }
#define EON_LAUNCHER_FOURTH(n,t) MillenniumDosFourthFunctionObservationResult LauncherRuntimeController::n(t o){return coordinator_.n(o);}
EON_LAUNCHER_FOURTH(observe_millennium_dos_fourth_function_dispatch,MillenniumDosFourthFunctionDispatchObservation)
EON_LAUNCHER_FOURTH(observe_millennium_dos_fourth_function_word,MillenniumDosFourthFunctionWordObservation)
EON_LAUNCHER_FOURTH(observe_millennium_dos_fourth_function_call_return,MillenniumDosFourthFunctionCallReturnObservation)
#undef EON_LAUNCHER_FOURTH
std::optional<MillenniumDosFourthFunctionCheckpoint> LauncherRuntimeController::millennium_dos_fourth_function_checkpoint()const{return coordinator_.millennium_dos_fourth_function_checkpoint();}
std::optional<MillenniumDosOwnedFunctionDiagnostics>
LauncherRuntimeController::millennium_dos_owned_function_diagnostics() const {
    return coordinator_.millennium_dos_owned_function_diagnostics();
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

DeuterosAmigaTitleDisplayTraceAdmission
LauncherRuntimeController::admit_active_deuteros_amiga_title_display_trace(
    const ReferenceTrace& trace) {
    return coordinator_.admit_active_deuteros_amiga_title_display_trace(trace);
}

std::optional<DeuterosAmigaTitleDisplayTraceCheckpoint>
LauncherRuntimeController::deuteros_amiga_title_display_trace_checkpoint() const {
    return coordinator_.deuteros_amiga_title_display_trace_checkpoint();
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
