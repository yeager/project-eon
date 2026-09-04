#include "engine/runtime_host.hpp"

namespace eon {

RuntimeCandidateLaunchResult RuntimeHost::launch_direct(const LaunchRequest& candidate,
    const std::vector<ReleaseArchive>& releases) {
    return NativeSessionController::launch_direct(candidate, releases);
}

RuntimeCandidateLaunchResult RuntimeHost::launch_menu(const LauncherSessionState& session,
    const LaunchRequest& base, const std::vector<ReleaseArchive>& releases) {
    return NativeSessionController::launch_menu(session, base, releases);
}

NativeSessionState RuntimeHost::state() const {
    return NativeSessionController::state();
}

bool RuntimeHost::is_menu() const {
    return NativeSessionController::is_menu();
}

bool RuntimeHost::requires_revocation_for(const LauncherSourceIdentity& source) const {
    return NativeSessionController::requires_revocation_for(source);
}

std::optional<ResolvedLaunchRequest> RuntimeHost::active() const {
    if (revoking()) return std::nullopt;
    return NativeSessionController::active();
}

ReleaseRuntimeAdmission RuntimeHost::admission() const {
    return NativeSessionController::admission();
}

ReleaseRuntimeRejection RuntimeHost::rejection() const {
    return NativeSessionController::rejection();
}

std::optional<RuntimeSessionSnapshot> RuntimeHost::session_snapshot() const {
    if (revoking()) return std::nullopt;
    return NativeSessionController::session_snapshot();
}

std::optional<MillenniumDosPresentationSnapshot> RuntimeHost::millennium_dos_presentation() const {
    if (revoking()) return std::nullopt;
    return NativeSessionController::millennium_dos_presentation();
}

std::optional<MillenniumDosStartupInputSnapshot> RuntimeHost::millennium_dos_startup_input() const {
    if (revoking()) return std::nullopt;
    return NativeSessionController::millennium_dos_startup_input();
}

std::optional<MillenniumDosStaticDispatchDiagnostics>
RuntimeHost::millennium_dos_static_dispatch_diagnostics() const {
    if (revoking()) return std::nullopt;
    return NativeSessionController::millennium_dos_static_dispatch_diagnostics();
}

std::optional<MillenniumDosNativeProcessCheckpoint>
RuntimeHost::millennium_dos_native_process_checkpoint() const {
    if (revoking()) return std::nullopt;
    return NativeSessionController::millennium_dos_native_process_checkpoint();
}

MillenniumDosGxActiveTraceAdmission
RuntimeHost::admit_active_millennium_dos_gx_startup_reference_trace(
    const ReferenceTrace& trace) {
    if (revoking()) return {false, "GX startup trace rejected during source revocation"};
    return NativeSessionController::admit_active_millennium_dos_gx_startup_reference_trace(trace);
}

std::optional<MillenniumDosGxStartupCheckpoint>
RuntimeHost::millennium_dos_gx_startup_checkpoint() const {
    if (revoking()) return std::nullopt;
    return NativeSessionController::millennium_dos_gx_startup_checkpoint();
}

MillenniumDosPostOverlayObservationResult
RuntimeHost::observe_millennium_dos_post_overlay_private_interrupt_return(
    const MillenniumDosPostOverlayPrivateInterruptReturnObservation observation) {
    if (revoking()) return {false, "Post-overlay observation rejected during source revocation"};
    return NativeSessionController::observe_millennium_dos_post_overlay_private_interrupt_return(
        observation);
}

MillenniumDosPostOverlayObservationResult
RuntimeHost::observe_millennium_dos_post_overlay_call_return(
    const MillenniumDosPostOverlayCallReturnObservation observation) {
    if (revoking()) return {false, "Post-overlay observation rejected during source revocation"};
    return NativeSessionController::observe_millennium_dos_post_overlay_call_return(observation);
}

MillenniumDosPostOverlayObservationResult RuntimeHost::observe_millennium_dos_post_overlay_al(
    const MillenniumDosPostOverlayAlObservation observation) {
    if (revoking()) return {false, "Post-overlay observation rejected during source revocation"};
    return NativeSessionController::observe_millennium_dos_post_overlay_al(observation);
}

MillenniumDosPostOverlayObservationResult
RuntimeHost::observe_millennium_dos_post_overlay_runtime_byte(
    const MillenniumDosPostOverlayRuntimeByteObservation observation) {
    if (revoking()) return {false, "Post-overlay observation rejected during source revocation"};
    return NativeSessionController::observe_millennium_dos_post_overlay_runtime_byte(observation);
}

std::optional<MillenniumDosPostOverlayLoopCheckpoint>
RuntimeHost::millennium_dos_post_overlay_loop_checkpoint() const {
    if (revoking()) return std::nullopt;
    return NativeSessionController::millennium_dos_post_overlay_loop_checkpoint();
}

#define EON_HOST_TENTH_PROXY(name, type) \
MillenniumDosTenthFunctionObservationResult RuntimeHost::name(const type observation) { \
    if (revoking()) return {false, "Tenth-function observation rejected during source revocation"}; \
    return NativeSessionController::name(observation); \
}
EON_HOST_TENTH_PROXY(observe_millennium_dos_tenth_function_dispatch, MillenniumDosTenthFunctionDispatchObservation)
EON_HOST_TENTH_PROXY(observe_millennium_dos_tenth_function_word, MillenniumDosTenthFunctionWordObservation)
EON_HOST_TENTH_PROXY(observe_millennium_dos_tenth_function_byte, MillenniumDosTenthFunctionByteObservation)
EON_HOST_TENTH_PROXY(observe_millennium_dos_tenth_function_call_return, MillenniumDosTenthFunctionCallReturnObservation)
EON_HOST_TENTH_PROXY(observe_millennium_dos_tenth_function_zero_flag, MillenniumDosTenthFunctionZeroFlagObservation)
EON_HOST_TENTH_PROXY(observe_millennium_dos_tenth_function_bl, MillenniumDosTenthFunctionBlObservation)
#undef EON_HOST_TENTH_PROXY

std::optional<MillenniumDosTenthFunctionCheckpoint>
RuntimeHost::millennium_dos_tenth_function_checkpoint() const {
    if (revoking()) return std::nullopt;
    return NativeSessionController::millennium_dos_tenth_function_checkpoint();
}

#define EON_HOST_SEVENTH_PROXY(name, type) \
MillenniumDosSeventhFunctionObservationResult RuntimeHost::name(const type observation) { \
    if (revoking()) return {false, "Seventh-function observation rejected during source revocation"}; \
    return NativeSessionController::name(observation); \
}
EON_HOST_SEVENTH_PROXY(observe_millennium_dos_seventh_function_dispatch, MillenniumDosSeventhFunctionDispatchObservation)
EON_HOST_SEVENTH_PROXY(observe_millennium_dos_seventh_function_word, MillenniumDosSeventhFunctionWordObservation)
EON_HOST_SEVENTH_PROXY(observe_millennium_dos_seventh_function_byte, MillenniumDosSeventhFunctionByteObservation)
EON_HOST_SEVENTH_PROXY(observe_millennium_dos_seventh_function_call_return, MillenniumDosSeventhFunctionCallReturnObservation)
EON_HOST_SEVENTH_PROXY(observe_millennium_dos_seventh_function_returned_bx, MillenniumDosSeventhFunctionReturnedBxObservation)
#undef EON_HOST_SEVENTH_PROXY

std::optional<MillenniumDosSeventhFunctionCheckpoint>
RuntimeHost::millennium_dos_seventh_function_checkpoint() const {
    if (revoking()) return std::nullopt;
    return NativeSessionController::millennium_dos_seventh_function_checkpoint();
}

#define EON_HOST_SIXTH_PROXY(name, type) \
MillenniumDosSixthFunctionObservationResult RuntimeHost::name(const type observation) { \
    if (revoking()) return {false, "Sixth-function observation rejected during source revocation"}; \
    return NativeSessionController::name(observation); \
}
EON_HOST_SIXTH_PROXY(observe_millennium_dos_sixth_function_dispatch, MillenniumDosSixthFunctionDispatchObservation)
EON_HOST_SIXTH_PROXY(observe_millennium_dos_sixth_function_word, MillenniumDosSixthFunctionWordObservation)
EON_HOST_SIXTH_PROXY(observe_millennium_dos_sixth_function_byte, MillenniumDosSixthFunctionByteObservation)
EON_HOST_SIXTH_PROXY(observe_millennium_dos_sixth_function_call_return, MillenniumDosSixthFunctionCallReturnObservation)
EON_HOST_SIXTH_PROXY(observe_millennium_dos_sixth_function_bl, MillenniumDosSixthFunctionBlObservation)
#undef EON_HOST_SIXTH_PROXY

std::optional<MillenniumDosSixthFunctionCheckpoint>
RuntimeHost::millennium_dos_sixth_function_checkpoint() const {
    if (revoking()) return std::nullopt;
    return NativeSessionController::millennium_dos_sixth_function_checkpoint();
}

#define EON_HOST_EIGHTH_PROXY(name, type) \
MillenniumDosEighthFunctionObservationResult RuntimeHost::name(const type observation) { \
    if (revoking()) return {false, "Eighth-function observation rejected during source revocation"}; \
    return NativeSessionController::name(observation); \
}
EON_HOST_EIGHTH_PROXY(observe_millennium_dos_eighth_function_dispatch,
    MillenniumDosEighthFunctionDispatchObservation)
EON_HOST_EIGHTH_PROXY(observe_millennium_dos_eighth_function_call_return,
    MillenniumDosEighthFunctionCallReturnObservation)
EON_HOST_EIGHTH_PROXY(observe_millennium_dos_eighth_function_bl,
    MillenniumDosEighthFunctionBlObservation)
#undef EON_HOST_EIGHTH_PROXY

std::optional<MillenniumDosEighthFunctionCheckpoint>
RuntimeHost::millennium_dos_eighth_function_checkpoint() const {
    if (revoking()) return std::nullopt;
    return NativeSessionController::millennium_dos_eighth_function_checkpoint();
}
#define EON_HOST_NINTH(name,type) MillenniumDosNinthFunctionObservationResult RuntimeHost::name(const type o) { if(revoking()) return {false,"Ninth-function observation rejected during source revocation"}; return NativeSessionController::name(o); }
EON_HOST_NINTH(observe_millennium_dos_ninth_function_dispatch,MillenniumDosNinthFunctionDispatchObservation)
EON_HOST_NINTH(observe_millennium_dos_ninth_function_word,MillenniumDosNinthFunctionWordObservation)
EON_HOST_NINTH(observe_millennium_dos_ninth_function_byte,MillenniumDosNinthFunctionByteObservation)
EON_HOST_NINTH(observe_millennium_dos_ninth_function_call_return,MillenniumDosNinthFunctionCallReturnObservation)
#undef EON_HOST_NINTH
std::optional<MillenniumDosNinthFunctionCheckpoint> RuntimeHost::millennium_dos_ninth_function_checkpoint() const { if(revoking()) return std::nullopt; return NativeSessionController::millennium_dos_ninth_function_checkpoint(); }
#define EON_HOST_FOURTH(n,t) MillenniumDosFourthFunctionObservationResult RuntimeHost::n(t o){if(revoking())return{false,"Fourth-function observation rejected during source revocation"};return NativeSessionController::n(o);}
EON_HOST_FOURTH(observe_millennium_dos_fourth_function_dispatch,MillenniumDosFourthFunctionDispatchObservation)
EON_HOST_FOURTH(observe_millennium_dos_fourth_function_word,MillenniumDosFourthFunctionWordObservation)
EON_HOST_FOURTH(observe_millennium_dos_fourth_function_call_return,MillenniumDosFourthFunctionCallReturnObservation)
#undef EON_HOST_FOURTH
std::optional<MillenniumDosFourthFunctionCheckpoint> RuntimeHost::millennium_dos_fourth_function_checkpoint()const{if(revoking())return std::nullopt;return NativeSessionController::millennium_dos_fourth_function_checkpoint();}
std::optional<MillenniumDosOwnedFunctionDiagnostics>
RuntimeHost::millennium_dos_owned_function_diagnostics() const {
    if (revoking()) return std::nullopt;
    return NativeSessionController::millennium_dos_owned_function_diagnostics();
}

std::optional<std::vector<float>> RuntimeHost::render_deuteros_amiga_opening_audio(
    const std::size_t frames) {
    if (revoking()) return std::nullopt;
    return NativeSessionController::render_deuteros_amiga_opening_audio(frames);
}

std::optional<DeuterosAmigaOpeningPresentationSnapshot>
RuntimeHost::deuteros_amiga_opening_presentation() const {
    if (revoking()) return std::nullopt;
    return NativeSessionController::deuteros_amiga_opening_presentation();
}

std::optional<DeuterosAmigaTitleStageBoundarySnapshot>
RuntimeHost::deuteros_amiga_title_stage_boundary() const {
    if (revoking()) return std::nullopt;
    return NativeSessionController::deuteros_amiga_title_stage_boundary();
}

DeuterosAmigaTitleDisplayTraceAdmission
RuntimeHost::admit_active_deuteros_amiga_title_display_trace(const ReferenceTrace& trace) {
    if (revoking()) {
        return {{}, "Title-display trace rejected during source revocation"};
    }
    return NativeSessionController::admit_active_deuteros_amiga_title_display_trace(trace);
}

std::optional<DeuterosAmigaTitleDisplayTraceCheckpoint>
RuntimeHost::deuteros_amiga_title_display_trace_checkpoint() const {
    if (revoking()) return std::nullopt;
    return NativeSessionController::deuteros_amiga_title_display_trace_checkpoint();
}

std::optional<DeuterosAtariBootstrapCheckpoint>
RuntimeHost::deuteros_atari_bootstrap_checkpoint() const {
    if (revoking()) return std::nullopt;
    return NativeSessionController::deuteros_atari_bootstrap_checkpoint();
}

std::optional<DeuterosAtariBootstrapPresentationSnapshot>
RuntimeHost::deuteros_atari_bootstrap_presentation() const {
    if (revoking()) return std::nullopt;
    return NativeSessionController::deuteros_atari_bootstrap_presentation();
}

std::optional<MillenniumAmigaBootstrapPresentationSnapshot>
RuntimeHost::millennium_amiga_bootstrap_presentation() const {
    if (revoking()) return std::nullopt;
    return NativeSessionController::millennium_amiga_bootstrap_presentation();
}

std::optional<MillenniumAtariBootstrapPresentationSnapshot>
RuntimeHost::millennium_atari_bootstrap_presentation() const {
    if (revoking()) return std::nullopt;
    return NativeSessionController::millennium_atari_bootstrap_presentation();
}

void RuntimeHost::begin_source_revocation() {
    if (state() == NativeSessionState::returning_to_menu) return;
    ++generation_;
    begin_return_to_menu();
}

void RuntimeHost::finish_source_revocation() {
    if (!revoking()) return;
    // A modal belongs to the outgoing front-end generation.  Retaining its
    // input gate after the coordinator has discarded that source would make a
    // fresh, independently admitted session silently reject its first real
    // observation. This is lifecycle cleanup only; the outgoing source has
    // already been made inaccessible by begin_source_revocation().
    input_suppressed_ = false;
    finish_return_to_menu();
}

RuntimeHostAdvance RuntimeHost::advance(const std::uint64_t monotonic_tick) {
    RuntimeHostAdvance result;
    if (state() != NativeSessionState::deuteros_amiga_opening) return result;
    if (!deuteros_amiga_opening_scheduler_active()) {
        result.opening_started = start_deuteros_amiga_opening_scheduler(monotonic_tick);
    }
    if (deuteros_amiga_opening_scheduler_active()) {
        result.opening = advance_deuteros_amiga_opening_scheduler(monotonic_tick);
    }
    result.opening_active = deuteros_amiga_opening_scheduler_active();
    return result;
}

RuntimeHostSnapshot RuntimeHost::snapshot() const {
    RuntimeHostSnapshot result;
    result.generation = generation_;
    result.revoking = revoking();
    result.input_suppressed = input_suppressed_;
    result.admission = admission();
    result.rejection = rejection();
    result.state = state();
    // A revocation interval is specifically the point at which SDL releases
    // its previous-generation borrows. Do not offer an old session/value to a
    // newly scheduled UI task during that interval.
    if (result.revoking) return result;
    result.session = session_snapshot();
    if (const auto presentation = presentation_snapshot()) {
        result.presentation = {presentation->kind, presentation->boundary,
            presentation->capabilities, presentation->input_contract};
    }
    return result;
}

void RuntimeHost::set_input_suppressed(const bool suppressed) {
    if (suppressed == input_suppressed_) return;
    if (suppressed) {
        // This is a host lifecycle cancellation, not a recovered input poll.
        // It must reach the coordinator before the gate closes so a prior
        // held value cannot affect a later native opening tick.
        static_cast<void>(NativeSessionController::observe_input(
            RuntimeInputObservation::opening_input_held(false)));
    }
    input_suppressed_ = suppressed;
}

RuntimeInputDisposition RuntimeHost::observe_input(const RuntimeInputObservation& observation) {
    if (input_suppressed_) return RuntimeInputDisposition::rejected;
    return NativeSessionController::observe_input(observation);
}

bool RuntimeHost::revoking() const {
    return state() == NativeSessionState::returning_to_menu;
}

} // namespace eon
