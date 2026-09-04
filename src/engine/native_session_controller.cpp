#include "engine/native_session_controller.hpp"
#include "engine/runtime_presentation.hpp"

namespace eon {

std::string_view native_session_state_label(const NativeSessionState state) {
    switch (state) {
    case NativeSessionState::menu: return "MENU";
    case NativeSessionState::admission_rejected: return "ADMISSION REJECTED";
    case NativeSessionState::millennium_dos_title: return "MILLENNIUM DOS TITLE";
    case NativeSessionState::millennium_dos_sound_driver_boundary:
        return "MILLENNIUM DOS SOUND DRIVER BOUNDARY";
    case NativeSessionState::millennium_dos_title_handoff_boundary:
        return "MILLENNIUM DOS TITLE HANDOFF BOUNDARY";
    case NativeSessionState::millennium_dos_gx_startup_boundary:
        return "MILLENNIUM DOS GX STARTUP BOUNDARY";
    case NativeSessionState::millennium_dos_post_overlay_loop:
        return "MILLENNIUM DOS POST-OVERLAY LOOP";
    case NativeSessionState::millennium_dos_seventh_function:
        return "MILLENNIUM DOS SEVENTH-FUNCTION HANDLER";
    case NativeSessionState::millennium_dos_sixth_function:
        return "MILLENNIUM DOS SIXTH-FUNCTION HANDLER";
    case NativeSessionState::millennium_dos_eighth_function:
        return "MILLENNIUM DOS EIGHTH-FUNCTION HANDLER";
    case NativeSessionState::millennium_dos_ninth_function:
        return "MILLENNIUM DOS NINTH-FUNCTION HANDLER";
    case NativeSessionState::millennium_dos_fourth_function:return "MILLENNIUM DOS FOURTH-FUNCTION HANDLER";
    case NativeSessionState::millennium_dos_fifth_function:return "MILLENNIUM DOS FIFTH-FUNCTION HANDLER";
    case NativeSessionState::millennium_dos_tenth_function:
        return "MILLENNIUM DOS TENTH-FUNCTION HANDLER";
    case NativeSessionState::millennium_amiga_bootstrap: return "MILLENNIUM AMIGA BOOTSTRAP";
    case NativeSessionState::millennium_atari_bootstrap: return "MILLENNIUM ATARI ST BOOTSTRAP";
    case NativeSessionState::deuteros_amiga_opening: return "DEUTEROS AMIGA OPENING";
    case NativeSessionState::deuteros_amiga_title_stage_boundary:
        return "DEUTEROS AMIGA TITLE STAGE BOUNDARY";
    case NativeSessionState::deuteros_amiga_title_display_trace_boundary:
        return "DEUTEROS AMIGA TITLE DISPLAY TRACE BOUNDARY";
    case NativeSessionState::deuteros_atari_bootstrap: return "DEUTEROS ATARI ST BOOTSTRAP";
    case NativeSessionState::returning_to_menu: return "RETURNING TO MENU";
    }
    return "ADMISSION REJECTED";
}

NativeSessionState native_session_state_for(const std::optional<RuntimeSessionSnapshot>& snapshot,
    const ReleaseRuntimeAdmission admission) {
    if (!snapshot) {
        return admission == ReleaseRuntimeAdmission::unselected
            ? NativeSessionState::menu : NativeSessionState::admission_rejected;
    }
    if (admission != ReleaseRuntimeAdmission::active) return NativeSessionState::admission_rejected;
    switch (snapshot->kind) {
    case RuntimeSessionKind::millennium_dos_title: return NativeSessionState::millennium_dos_title;
    case RuntimeSessionKind::millennium_dos_sound_driver_boundary:
        return NativeSessionState::millennium_dos_sound_driver_boundary;
    case RuntimeSessionKind::millennium_dos_title_handoff_boundary:
        return NativeSessionState::millennium_dos_title_handoff_boundary;
    case RuntimeSessionKind::millennium_dos_gx_startup_boundary:
        return NativeSessionState::millennium_dos_gx_startup_boundary;
    case RuntimeSessionKind::millennium_dos_post_overlay_loop:
        return NativeSessionState::millennium_dos_post_overlay_loop;
    case RuntimeSessionKind::millennium_dos_seventh_function:
        return NativeSessionState::millennium_dos_seventh_function;
    case RuntimeSessionKind::millennium_dos_sixth_function:
        return NativeSessionState::millennium_dos_sixth_function;
    case RuntimeSessionKind::millennium_dos_eighth_function:
        return NativeSessionState::millennium_dos_eighth_function;
    case RuntimeSessionKind::millennium_dos_ninth_function:
        return NativeSessionState::millennium_dos_ninth_function;
    case RuntimeSessionKind::millennium_dos_fourth_function:return NativeSessionState::millennium_dos_fourth_function;
    case RuntimeSessionKind::millennium_dos_fifth_function:return NativeSessionState::millennium_dos_fifth_function;
    case RuntimeSessionKind::millennium_dos_tenth_function:
        return NativeSessionState::millennium_dos_tenth_function;
    case RuntimeSessionKind::millennium_amiga_bootstrap: return NativeSessionState::millennium_amiga_bootstrap;
    case RuntimeSessionKind::millennium_atari_bootstrap: return NativeSessionState::millennium_atari_bootstrap;
    case RuntimeSessionKind::deuteros_amiga_opening: return NativeSessionState::deuteros_amiga_opening;
    case RuntimeSessionKind::deuteros_amiga_title_stage:
        return NativeSessionState::deuteros_amiga_title_stage_boundary;
    case RuntimeSessionKind::deuteros_amiga_title_display_trace_boundary:
        return NativeSessionState::deuteros_amiga_title_display_trace_boundary;
    case RuntimeSessionKind::deuteros_atari_bootstrap: return NativeSessionState::deuteros_atari_bootstrap;
    }
    return NativeSessionState::admission_rejected;
}

RuntimeCandidateLaunchResult NativeSessionController::launch_direct(const LaunchRequest& candidate,
    const std::vector<ReleaseArchive>& releases) {
    if (state_ == NativeSessionState::returning_to_menu) {
        // SDL may still be releasing source-derived borrows. Do not expose
        // the preceding active admission as an attempted new launch, and do
        // not reset the coordinator before that teardown is complete.
        return {ReleaseRuntimeAdmission::identity_rejected,
            ReleaseRuntimeRejection::lifecycle_transition, std::nullopt};
    }
    deuteros_amiga_opening_runner_.reset();
    const auto result = runtime_.launch_direct(candidate, releases);
    synchronize_after_runtime_change();
    return result;
}

RuntimeCandidateLaunchResult NativeSessionController::launch_menu(const LauncherSessionState& session,
    const LaunchRequest& base, const std::vector<ReleaseArchive>& releases) {
    if (state_ == NativeSessionState::returning_to_menu) {
        return {ReleaseRuntimeAdmission::identity_rejected,
            ReleaseRuntimeRejection::lifecycle_transition, std::nullopt};
    }
    deuteros_amiga_opening_runner_.reset();
    const auto result = runtime_.launch_menu(session, base, releases);
    synchronize_after_runtime_change();
    return result;
}

RuntimeInputDisposition NativeSessionController::observe_input(const RuntimeInputObservation& observation) {
    if (state_ == NativeSessionState::returning_to_menu) {
        return RuntimeInputDisposition::rejected;
    }
    const auto result = runtime_.observe_input(observation);
    synchronize_after_runtime_change();
    return result;
}

std::optional<MillenniumDosPresentationSnapshot>
NativeSessionController::millennium_dos_presentation() const {
    if (state_ != NativeSessionState::millennium_dos_title) return std::nullopt;
    return runtime_.millennium_dos_presentation();
}

std::optional<MillenniumDosStartupInputSnapshot>
NativeSessionController::millennium_dos_startup_input() const {
    if (state_ != NativeSessionState::millennium_dos_title
        && state_ != NativeSessionState::millennium_dos_sound_driver_boundary) return std::nullopt;
    return runtime_.millennium_dos_startup_input();
}

std::optional<MillenniumDosStaticDispatchDiagnostics>
NativeSessionController::millennium_dos_static_dispatch_diagnostics() const {
    if (state_ != NativeSessionState::millennium_dos_title) return std::nullopt;
    return runtime_.millennium_dos_static_dispatch_diagnostics();
}

std::optional<MillenniumDosNativeProcessCheckpoint>
NativeSessionController::millennium_dos_native_process_checkpoint() const {
    if (state_ == NativeSessionState::menu || state_ == NativeSessionState::admission_rejected
        || state_ == NativeSessionState::returning_to_menu) return std::nullopt;
    return runtime_.millennium_dos_native_process_checkpoint();
}

MillenniumDosGxActiveTraceAdmission
NativeSessionController::admit_active_millennium_dos_gx_startup_reference_trace(
    const ReferenceTrace& trace) {
    if (state_ != NativeSessionState::millennium_dos_title_handoff_boundary) {
        return {false, "GX startup trace requires the active title-handoff boundary"};
    }
    const auto result = runtime_.admit_active_millennium_dos_gx_startup_reference_trace(trace);
    synchronize_after_runtime_change();
    return result;
}

std::optional<MillenniumDosGxStartupCheckpoint>
NativeSessionController::millennium_dos_gx_startup_checkpoint() const {
    if (state_ != NativeSessionState::millennium_dos_gx_startup_boundary) return std::nullopt;
    return runtime_.millennium_dos_gx_startup_checkpoint();
}

MillenniumDosPostOverlayObservationResult
NativeSessionController::observe_millennium_dos_post_overlay_private_interrupt_return(
    const MillenniumDosPostOverlayPrivateInterruptReturnObservation observation) {
    if (state_ != NativeSessionState::millennium_dos_gx_startup_boundary) {
        return {false, "Post-overlay INT 91h return requires the GX startup boundary"};
    }
    const auto result = runtime_.observe_millennium_dos_post_overlay_private_interrupt_return(
        observation);
    synchronize_after_runtime_change();
    return result;
}

MillenniumDosPostOverlayObservationResult
NativeSessionController::observe_millennium_dos_post_overlay_call_return(
    const MillenniumDosPostOverlayCallReturnObservation observation) {
    if (state_ != NativeSessionState::millennium_dos_post_overlay_loop) {
        return {false, "Call return requires the post-overlay loop"};
    }
    return runtime_.observe_millennium_dos_post_overlay_call_return(observation);
}

MillenniumDosPostOverlayObservationResult
NativeSessionController::observe_millennium_dos_post_overlay_al(
    const MillenniumDosPostOverlayAlObservation observation) {
    if (state_ != NativeSessionState::millennium_dos_post_overlay_loop) {
        return {false, "AL observation requires the post-overlay loop"};
    }
    return runtime_.observe_millennium_dos_post_overlay_al(observation);
}

MillenniumDosPostOverlayObservationResult
NativeSessionController::observe_millennium_dos_post_overlay_runtime_byte(
    const MillenniumDosPostOverlayRuntimeByteObservation observation) {
    if (state_ != NativeSessionState::millennium_dos_post_overlay_loop) {
        return {false, "Runtime-byte observation requires the post-overlay loop"};
    }
    return runtime_.observe_millennium_dos_post_overlay_runtime_byte(observation);
}

std::optional<MillenniumDosPostOverlayLoopCheckpoint>
NativeSessionController::millennium_dos_post_overlay_loop_checkpoint() const {
    if (state_ != NativeSessionState::millennium_dos_post_overlay_loop) return std::nullopt;
    return runtime_.millennium_dos_post_overlay_loop_checkpoint();
}

MillenniumDosTenthFunctionObservationResult
NativeSessionController::observe_millennium_dos_tenth_function_dispatch(
    const MillenniumDosTenthFunctionDispatchObservation observation) {
    if (state_ != NativeSessionState::millennium_dos_post_overlay_loop)
        return {false, "Tenth-function dispatch requires the post-overlay loop"};
    const auto result = runtime_.observe_millennium_dos_tenth_function_dispatch(observation);
    synchronize_after_runtime_change();
    return result;
}

#define EON_NATIVE_TENTH_PROXY(name, type) \
MillenniumDosTenthFunctionObservationResult NativeSessionController::name( \
    const type observation) { \
    if (state_ != NativeSessionState::millennium_dos_tenth_function) \
        return {false, "Observation requires the tenth-function session"}; \
    return runtime_.name(observation); \
}
EON_NATIVE_TENTH_PROXY(observe_millennium_dos_tenth_function_word, MillenniumDosTenthFunctionWordObservation)
EON_NATIVE_TENTH_PROXY(observe_millennium_dos_tenth_function_byte, MillenniumDosTenthFunctionByteObservation)
EON_NATIVE_TENTH_PROXY(observe_millennium_dos_tenth_function_call_return, MillenniumDosTenthFunctionCallReturnObservation)
EON_NATIVE_TENTH_PROXY(observe_millennium_dos_tenth_function_zero_flag, MillenniumDosTenthFunctionZeroFlagObservation)
EON_NATIVE_TENTH_PROXY(observe_millennium_dos_tenth_function_bl, MillenniumDosTenthFunctionBlObservation)
#undef EON_NATIVE_TENTH_PROXY

std::optional<MillenniumDosTenthFunctionCheckpoint>
NativeSessionController::millennium_dos_tenth_function_checkpoint() const {
    if (state_ != NativeSessionState::millennium_dos_tenth_function) return std::nullopt;
    return runtime_.millennium_dos_tenth_function_checkpoint();
}

MillenniumDosSeventhFunctionObservationResult
NativeSessionController::observe_millennium_dos_seventh_function_dispatch(
    const MillenniumDosSeventhFunctionDispatchObservation observation) {
    if (state_ != NativeSessionState::millennium_dos_post_overlay_loop)
        return {false, "Seventh-function dispatch requires the post-overlay loop"};
    const auto result = runtime_.observe_millennium_dos_seventh_function_dispatch(observation);
    synchronize_after_runtime_change();
    return result;
}
#define EON_NATIVE_SEVENTH_PROXY(name, type) \
MillenniumDosSeventhFunctionObservationResult NativeSessionController::name( \
    const type observation) { \
    if (state_ != NativeSessionState::millennium_dos_seventh_function) \
        return {false, "Observation requires the seventh-function session"}; \
    return runtime_.name(observation); \
}
EON_NATIVE_SEVENTH_PROXY(observe_millennium_dos_seventh_function_word, MillenniumDosSeventhFunctionWordObservation)
EON_NATIVE_SEVENTH_PROXY(observe_millennium_dos_seventh_function_byte, MillenniumDosSeventhFunctionByteObservation)
EON_NATIVE_SEVENTH_PROXY(observe_millennium_dos_seventh_function_call_return, MillenniumDosSeventhFunctionCallReturnObservation)
EON_NATIVE_SEVENTH_PROXY(observe_millennium_dos_seventh_function_returned_bx, MillenniumDosSeventhFunctionReturnedBxObservation)
#undef EON_NATIVE_SEVENTH_PROXY

std::optional<MillenniumDosSeventhFunctionCheckpoint>
NativeSessionController::millennium_dos_seventh_function_checkpoint() const {
    if (state_ != NativeSessionState::millennium_dos_seventh_function) return std::nullopt;
    return runtime_.millennium_dos_seventh_function_checkpoint();
}

MillenniumDosSixthFunctionObservationResult
NativeSessionController::observe_millennium_dos_sixth_function_dispatch(
    const MillenniumDosSixthFunctionDispatchObservation observation) {
    if (state_ != NativeSessionState::millennium_dos_post_overlay_loop)
        return {false, "Sixth-function dispatch requires the post-overlay loop"};
    const auto result = runtime_.observe_millennium_dos_sixth_function_dispatch(observation);
    synchronize_after_runtime_change();
    return result;
}
#define EON_NATIVE_SIXTH_PROXY(name, type) \
MillenniumDosSixthFunctionObservationResult NativeSessionController::name( \
    const type observation) { \
    if (state_ != NativeSessionState::millennium_dos_sixth_function) \
        return {false, "Observation requires the sixth-function session"}; \
    return runtime_.name(observation); \
}
EON_NATIVE_SIXTH_PROXY(observe_millennium_dos_sixth_function_word, MillenniumDosSixthFunctionWordObservation)
EON_NATIVE_SIXTH_PROXY(observe_millennium_dos_sixth_function_byte, MillenniumDosSixthFunctionByteObservation)
EON_NATIVE_SIXTH_PROXY(observe_millennium_dos_sixth_function_call_return, MillenniumDosSixthFunctionCallReturnObservation)
EON_NATIVE_SIXTH_PROXY(observe_millennium_dos_sixth_function_bl, MillenniumDosSixthFunctionBlObservation)
#undef EON_NATIVE_SIXTH_PROXY

std::optional<MillenniumDosSixthFunctionCheckpoint>
NativeSessionController::millennium_dos_sixth_function_checkpoint() const {
    if (state_ != NativeSessionState::millennium_dos_sixth_function) return std::nullopt;
    return runtime_.millennium_dos_sixth_function_checkpoint();
}

MillenniumDosEighthFunctionObservationResult
NativeSessionController::observe_millennium_dos_eighth_function_dispatch(
    const MillenniumDosEighthFunctionDispatchObservation observation) {
    if (state_ != NativeSessionState::millennium_dos_post_overlay_loop)
        return {false, "Eighth-function dispatch requires the post-overlay loop"};
    const auto result = runtime_.observe_millennium_dos_eighth_function_dispatch(observation);
    synchronize_after_runtime_change();
    return result;
}
#define EON_NATIVE_EIGHTH_PROXY(name, type) \
MillenniumDosEighthFunctionObservationResult NativeSessionController::name( \
    const type observation) { \
    if (state_ != NativeSessionState::millennium_dos_eighth_function) \
        return {false, "Observation requires the eighth-function session"}; \
    return runtime_.name(observation); \
}
EON_NATIVE_EIGHTH_PROXY(observe_millennium_dos_eighth_function_call_return,
    MillenniumDosEighthFunctionCallReturnObservation)
EON_NATIVE_EIGHTH_PROXY(observe_millennium_dos_eighth_function_bl,
    MillenniumDosEighthFunctionBlObservation)
#undef EON_NATIVE_EIGHTH_PROXY

std::optional<MillenniumDosEighthFunctionCheckpoint>
NativeSessionController::millennium_dos_eighth_function_checkpoint() const {
    if (state_ != NativeSessionState::millennium_dos_eighth_function) return std::nullopt;
    return runtime_.millennium_dos_eighth_function_checkpoint();
}
MillenniumDosNinthFunctionObservationResult NativeSessionController::observe_millennium_dos_ninth_function_dispatch(const MillenniumDosNinthFunctionDispatchObservation o) { if(state_!=NativeSessionState::millennium_dos_post_overlay_loop) return {false,"Ninth-function dispatch requires the post-overlay loop"}; auto r=runtime_.observe_millennium_dos_ninth_function_dispatch(o); synchronize_after_runtime_change(); return r; }
#define EON_NATIVE_NINTH(name,type) MillenniumDosNinthFunctionObservationResult NativeSessionController::name(const type o) { if(state_!=NativeSessionState::millennium_dos_ninth_function) return {false,"Observation requires the ninth-function session"}; return runtime_.name(o); }
EON_NATIVE_NINTH(observe_millennium_dos_ninth_function_word,MillenniumDosNinthFunctionWordObservation)
EON_NATIVE_NINTH(observe_millennium_dos_ninth_function_byte,MillenniumDosNinthFunctionByteObservation)
EON_NATIVE_NINTH(observe_millennium_dos_ninth_function_call_return,MillenniumDosNinthFunctionCallReturnObservation)
#undef EON_NATIVE_NINTH
std::optional<MillenniumDosNinthFunctionCheckpoint> NativeSessionController::millennium_dos_ninth_function_checkpoint() const { if(state_!=NativeSessionState::millennium_dos_ninth_function) return std::nullopt; return runtime_.millennium_dos_ninth_function_checkpoint(); }
MillenniumDosFourthFunctionObservationResult NativeSessionController::observe_millennium_dos_fourth_function_dispatch(MillenniumDosFourthFunctionDispatchObservation o){if(state_!=NativeSessionState::millennium_dos_post_overlay_loop)return{false,"Fourth-function dispatch requires post-overlay loop"};auto r=runtime_.observe_millennium_dos_fourth_function_dispatch(o);synchronize_after_runtime_change();return r;}
#define EON_NATIVE_FOURTH(n,t) MillenniumDosFourthFunctionObservationResult NativeSessionController::n(t o){if(state_!=NativeSessionState::millennium_dos_fourth_function)return{false,"Observation requires fourth-function session"};return runtime_.n(o);}
EON_NATIVE_FOURTH(observe_millennium_dos_fourth_function_word,MillenniumDosFourthFunctionWordObservation)
EON_NATIVE_FOURTH(observe_millennium_dos_fourth_function_call_return,MillenniumDosFourthFunctionCallReturnObservation)
#undef EON_NATIVE_FOURTH
std::optional<MillenniumDosFourthFunctionCheckpoint> NativeSessionController::millennium_dos_fourth_function_checkpoint()const{if(state_!=NativeSessionState::millennium_dos_fourth_function)return std::nullopt;return runtime_.millennium_dos_fourth_function_checkpoint();}
MillenniumDosFifthFunctionObservationResult NativeSessionController::observe_millennium_dos_fifth_function_dispatch(MillenniumDosFifthFunctionDispatchObservation o){if(state_!=NativeSessionState::millennium_dos_post_overlay_loop)return{false,"Fifth-function dispatch requires post-overlay loop"};auto r=runtime_.observe_millennium_dos_fifth_function_dispatch(o);synchronize_after_runtime_change();return r;} MillenniumDosFifthFunctionObservationResult NativeSessionController::observe_millennium_dos_fifth_function_call_return(MillenniumDosFifthFunctionCallReturnObservation o){if(state_!=NativeSessionState::millennium_dos_fifth_function)return{false,"Observation requires fifth-function session"};return runtime_.observe_millennium_dos_fifth_function_call_return(o);} std::optional<MillenniumDosFifthFunctionCheckpoint> NativeSessionController::millennium_dos_fifth_function_checkpoint()const{if(state_!=NativeSessionState::millennium_dos_fifth_function)return std::nullopt;return runtime_.millennium_dos_fifth_function_checkpoint();}
std::optional<MillenniumDosOwnedFunctionDiagnostics>
NativeSessionController::millennium_dos_owned_function_diagnostics() const {
    switch (state_) {
    case NativeSessionState::millennium_dos_fourth_function:
    case NativeSessionState::millennium_dos_fifth_function:
    case NativeSessionState::millennium_dos_sixth_function:
    case NativeSessionState::millennium_dos_seventh_function:
    case NativeSessionState::millennium_dos_eighth_function:
    case NativeSessionState::millennium_dos_ninth_function:
    case NativeSessionState::millennium_dos_tenth_function:
        return runtime_.millennium_dos_owned_function_diagnostics();
    default: return std::nullopt;
    }
}

std::optional<DeuterosAmigaVmEvents> NativeSessionController::tick_deuteros_amiga_opening() {
    if (state_ == NativeSessionState::returning_to_menu) return std::nullopt;
    const auto events = runtime_.tick_deuteros_amiga_opening();
    synchronize_after_runtime_change();
    return events;
}

bool NativeSessionController::start_deuteros_amiga_opening_scheduler(
    const std::uint64_t initial_tick) {
    if (state_ != NativeSessionState::deuteros_amiga_opening) return false;
    deuteros_amiga_opening_runner_.emplace(
        [this] { return tick_deuteros_amiga_opening(); }, initial_tick);
    return true;
}

DeuterosAmigaOpeningAdvance
NativeSessionController::advance_deuteros_amiga_opening_scheduler(const std::uint64_t now) {
    if (!deuteros_amiga_opening_runner_) return {};
    return deuteros_amiga_opening_runner_->advance(now);
}

bool NativeSessionController::deuteros_amiga_opening_scheduler_active() const {
    return state_ == NativeSessionState::deuteros_amiga_opening
        && deuteros_amiga_opening_runner_ && !deuteros_amiga_opening_runner_->stopped();
}

std::optional<std::vector<float>>
NativeSessionController::render_deuteros_amiga_opening_audio(const std::size_t frames) {
    if (state_ != NativeSessionState::deuteros_amiga_opening) return std::nullopt;
    return runtime_.render_deuteros_amiga_opening_audio(frames);
}

std::optional<DeuterosAmigaOpeningCheckpoint>
NativeSessionController::deuteros_amiga_opening_checkpoint() const {
    if (state_ != NativeSessionState::deuteros_amiga_opening) return std::nullopt;
    return runtime_.deuteros_amiga_opening_checkpoint();
}

std::optional<DeuterosAmigaOpeningPresentationSnapshot>
NativeSessionController::deuteros_amiga_opening_presentation() const {
    if (state_ != NativeSessionState::deuteros_amiga_opening) return std::nullopt;
    return runtime_.deuteros_amiga_opening_presentation();
}

std::optional<DeuterosAmigaTitleStageBoundarySnapshot>
NativeSessionController::deuteros_amiga_title_stage_boundary() const {
    if (state_ != NativeSessionState::deuteros_amiga_title_stage_boundary) return std::nullopt;
    return runtime_.deuteros_amiga_title_stage_boundary();
}

std::optional<DeuterosAmigaTitleDependencyChainCheckpoint>
NativeSessionController::deuteros_amiga_title_dependency_chain_checkpoint() const {
    if (state_ != NativeSessionState::deuteros_amiga_title_stage_boundary) return std::nullopt;
    return runtime_.deuteros_amiga_title_dependency_chain_checkpoint();
}

DeuterosAmigaTitleDisplayTraceAdmission
NativeSessionController::admit_active_deuteros_amiga_title_display_trace(
    const ReferenceTrace& trace) {
    if (state_ != NativeSessionState::deuteros_amiga_title_stage_boundary) {
        return {{}, "Title-display trace requires the active Deuteros Amiga title-stage boundary"};
    }
    auto result = runtime_.admit_active_deuteros_amiga_title_display_trace(trace);
    synchronize_after_runtime_change();
    return result;
}

std::optional<DeuterosAmigaTitleDisplayTraceCheckpoint>
NativeSessionController::deuteros_amiga_title_display_trace_checkpoint() const {
    if (state_ != NativeSessionState::deuteros_amiga_title_display_trace_boundary) {
        return std::nullopt;
    }
    return runtime_.deuteros_amiga_title_display_trace_checkpoint();
}

std::optional<DeuterosAtariBootstrapCheckpoint>
NativeSessionController::deuteros_atari_bootstrap_checkpoint() const {
    if (state_ != NativeSessionState::deuteros_atari_bootstrap) return std::nullopt;
    return runtime_.deuteros_atari_bootstrap_checkpoint();
}

std::optional<DeuterosAtariBootstrapPresentationSnapshot>
NativeSessionController::deuteros_atari_bootstrap_presentation() const {
    if (state_ != NativeSessionState::deuteros_atari_bootstrap) return std::nullopt;
    return runtime_.deuteros_atari_bootstrap_presentation();
}

std::optional<MillenniumAmigaBootstrapPresentationSnapshot>
NativeSessionController::millennium_amiga_bootstrap_presentation() const {
    if (state_ != NativeSessionState::millennium_amiga_bootstrap) return std::nullopt;
    return runtime_.millennium_amiga_bootstrap_presentation();
}

std::optional<MillenniumAtariBootstrapPresentationSnapshot>
NativeSessionController::millennium_atari_bootstrap_presentation() const {
    if (state_ != NativeSessionState::millennium_atari_bootstrap) return std::nullopt;
    return runtime_.millennium_atari_bootstrap_presentation();
}

void NativeSessionController::begin_return_to_menu() {
    deuteros_amiga_opening_runner_.reset();
    state_ = NativeSessionState::returning_to_menu;
}

void NativeSessionController::finish_return_to_menu() {
    if (state_ != NativeSessionState::returning_to_menu) return;
    runtime_.reset();
    state_ = NativeSessionState::menu;
}

void NativeSessionController::reset() {
    begin_return_to_menu();
    finish_return_to_menu();
}

void NativeSessionController::synchronize() {
    synchronize_after_runtime_change();
}

bool NativeSessionController::is_live() const {
    return state_ != NativeSessionState::menu && state_ != NativeSessionState::admission_rejected
        && state_ != NativeSessionState::returning_to_menu;
}

bool NativeSessionController::requires_revocation_for(const LauncherSourceIdentity& source) const {
    return runtime_.requires_revocation_for(source);
}

std::optional<RuntimeSessionSnapshot> NativeSessionController::session_snapshot() const {
    return runtime_.session_snapshot();
}

std::optional<RuntimePresentationSnapshot> NativeSessionController::presentation_snapshot() const {
    return runtime_presentation_for(state_, runtime_.admission(), runtime_.session_snapshot());
}

void NativeSessionController::synchronize_after_runtime_change() {
    if (state_ == NativeSessionState::returning_to_menu) return;
    state_ = native_session_state_for(runtime_.session_snapshot(), runtime_.admission());
}

} // namespace eon
