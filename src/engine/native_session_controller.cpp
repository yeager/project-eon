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
    case NativeSessionState::millennium_dos_ninth_function_handoff:return "MILLENNIUM DOS NINTH-FUNCTION CONTINUATION";
    case NativeSessionState::millennium_dos_fourth_function:return "MILLENNIUM DOS FOURTH-FUNCTION HANDLER";
    case NativeSessionState::millennium_dos_fifth_function:return "MILLENNIUM DOS FIFTH-FUNCTION HANDLER";
    case NativeSessionState::millennium_dos_third_function:return "MILLENNIUM DOS THIRD-FUNCTION HANDLER";
    case NativeSessionState::millennium_dos_first_function:return "MILLENNIUM DOS FIRST-FUNCTION HANDLER";
    case NativeSessionState::millennium_dos_second_function:return "MILLENNIUM DOS SECOND-FUNCTION HANDLER";
    case NativeSessionState::millennium_dos_second_function_callback:return "MILLENNIUM DOS SECOND-FUNCTION CALLBACK";
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
    case RuntimeSessionKind::millennium_dos_ninth_function_handoff:return NativeSessionState::millennium_dos_ninth_function_handoff;
    case RuntimeSessionKind::millennium_dos_fourth_function:return NativeSessionState::millennium_dos_fourth_function;
    case RuntimeSessionKind::millennium_dos_fifth_function:return NativeSessionState::millennium_dos_fifth_function;
    case RuntimeSessionKind::millennium_dos_third_function:return NativeSessionState::millennium_dos_third_function;
    case RuntimeSessionKind::millennium_dos_first_function:return NativeSessionState::millennium_dos_first_function;
    case RuntimeSessionKind::millennium_dos_second_function:return NativeSessionState::millennium_dos_second_function;
    case RuntimeSessionKind::millennium_dos_second_function_callback:return NativeSessionState::millennium_dos_second_function_callback;
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
MillenniumDosSoundDriverLoadObservationResult NativeSessionController::observe_millennium_dos_sound_driver_load(MillenniumDosSoundDriverLoadObservation o){if(state_!=NativeSessionState::millennium_dos_sound_driver_boundary)return {false,"Sound-driver load requires the active selected-driver boundary"};return runtime_.observe_millennium_dos_sound_driver_load(std::move(o));}
std::optional<MillenniumDosSoundDriverLoadCheckpoint> NativeSessionController::millennium_dos_sound_driver_load_checkpoint()const{if(state_!=NativeSessionState::millennium_dos_sound_driver_boundary)return std::nullopt;return runtime_.millennium_dos_sound_driver_load_checkpoint();}
std::optional<MillenniumDosCompatibilityRunnerCheckpoint> NativeSessionController::tick_millennium_dos_compatibility_runner(){if(state_!=NativeSessionState::millennium_dos_sound_driver_boundary)return std::nullopt;auto result=runtime_.tick_millennium_dos_compatibility_runner();synchronize_after_runtime_change();return result;}
MillenniumDosTitleExecEntryObservationResult NativeSessionController::observe_millennium_dos_title_child_process_entry(MillenniumDosTitleExecProcessEntry o){if(state_!=NativeSessionState::millennium_dos_sound_driver_boundary)return {false,"Title child entry requires the sound-driver boundary"};return runtime_.observe_millennium_dos_title_child_process_entry(o);}
MillenniumDosTitleExecEntryObservationResult NativeSessionController::advance_millennium_dos_title_entry_prefix(MillenniumDosTitleExecPrefixObservation o){if(state_!=NativeSessionState::millennium_dos_sound_driver_boundary)return {false,"Title entry prefix requires the child-process boundary"};auto result=runtime_.advance_millennium_dos_title_entry_prefix(o);if(result.accepted)synchronize_after_runtime_change();return result;}
std::optional<MillenniumDosTitleExecEntryRuntimeCheckpoint> NativeSessionController::millennium_dos_title_exec_entry_checkpoint()const{if(state_!=NativeSessionState::millennium_dos_sound_driver_boundary&&state_!=NativeSessionState::millennium_dos_title)return std::nullopt;return runtime_.millennium_dos_title_exec_entry_checkpoint();}
MillenniumDosTitleInitializationObservationResult NativeSessionController::observe_millennium_dos_title_private_interrupt_result(MillenniumDosTitlePrivateInterruptResultObservation o){if(state_!=NativeSessionState::millennium_dos_title)return {false,"Title private-interrupt result requires the active title boundary"};return runtime_.observe_millennium_dos_title_private_interrupt_result(o);}
MillenniumDosTitleInitializationObservationResult NativeSessionController::observe_millennium_dos_title_selected_callee_result(MillenniumDosTitleSelectedCalleeResultObservation o){if(state_!=NativeSessionState::millennium_dos_title)return {false,"Selected title-callee result requires the active title boundary"};return runtime_.observe_millennium_dos_title_selected_callee_result(o);}
MillenniumDosTitleInitializationObservationResult NativeSessionController::observe_millennium_dos_title_bios_result(MillenniumDosTitleBiosResultObservation o){if(state_!=NativeSessionState::millennium_dos_title)return {false,"Title BIOS result requires the active title boundary"};return runtime_.observe_millennium_dos_title_bios_result(o);}
MillenniumDosTitleInitializationObservationResult NativeSessionController::observe_millennium_dos_title_dos_memory_result(MillenniumDosTitleDosResultObservation o){if(state_!=NativeSessionState::millennium_dos_title)return {false,"Title DOS-memory result requires the active title boundary"};return runtime_.observe_millennium_dos_title_dos_memory_result(o);}
MillenniumDosTitleInitializationObservationResult NativeSessionController::observe_millennium_dos_title_dos_file_result(MillenniumDosTitleDosFileResultObservation o){if(state_!=NativeSessionState::millennium_dos_title)return {false,"Title DOS-file result requires the active title boundary"};return runtime_.observe_millennium_dos_title_dos_file_result(o);}
MillenniumDosTitleInitializationObservationResult NativeSessionController::observe_millennium_dos_title_dos_vector_result(MillenniumDosTitleDosVectorResultObservation o){if(state_!=NativeSessionState::millennium_dos_title)return {false,"Title DOS-vector result requires the active title boundary"};return runtime_.observe_millennium_dos_title_dos_vector_result(o);}
MillenniumDosTitleInitializationObservationResult NativeSessionController::observe_millennium_dos_title_setup_bios_result(MillenniumDosTitleSetupBiosResultObservation o){if(state_!=NativeSessionState::millennium_dos_title)return {false,"Title setup BIOS result requires the active title boundary"};return runtime_.observe_millennium_dos_title_setup_bios_result(o);}
MillenniumDosTitleInitializationObservationResult NativeSessionController::observe_millennium_dos_title_far_words(MillenniumDosTitleFarWordsObservation o){if(state_!=NativeSessionState::millennium_dos_title)return {false,"Title far words require the active title boundary"};return runtime_.observe_millennium_dos_title_far_words(o);}
MillenniumDosTitleInitializationObservationResult NativeSessionController::observe_millennium_dos_title_far_word(MillenniumDosTitleFarWordObservation o){if(state_!=NativeSessionState::millennium_dos_title)return {false,"Title far word requires the active title boundary"};return runtime_.observe_millennium_dos_title_far_word(o);}
MillenniumDosTitleInitializationObservationResult NativeSessionController::observe_millennium_dos_title_far_byte(MillenniumDosTitleFarByteObservation o){if(state_!=NativeSessionState::millennium_dos_title)return {false,"Title far byte requires the active title boundary"};return runtime_.observe_millennium_dos_title_far_byte(o);}

MillenniumDosTitleToGameObservationResult NativeSessionController::observe_millennium_dos_title_to_game_call_return(MillenniumDosTitleToGameCallReturnObservation o){if(state_!=NativeSessionState::millennium_dos_title_handoff_boundary)return {false,"Title-to-game observation requires the title-handoff boundary"};return runtime_.observe_millennium_dos_title_to_game_call_return(o);}
MillenniumDosTitleToGameObservationResult NativeSessionController::observe_millennium_dos_title_to_game_stack_word(MillenniumDosTitleToGameStackWordObservation o){if(state_!=NativeSessionState::millennium_dos_title_handoff_boundary)return {false,"Title-to-game observation requires the title-handoff boundary"};return runtime_.observe_millennium_dos_title_to_game_stack_word(o);}
MillenniumDosTitleToGameObservationResult NativeSessionController::observe_millennium_dos_title_to_game_title_termination(MillenniumDosTitleToGameInterruptObservation o){if(state_!=NativeSessionState::millennium_dos_title_handoff_boundary)return {false,"Title-to-game observation requires the title-handoff boundary"};return runtime_.observe_millennium_dos_title_to_game_title_termination(o);}
MillenniumDosTitleToGameObservationResult NativeSessionController::observe_millennium_dos_title_to_game_parent_exec_return(MillenniumDosTitleToGameInterruptObservation o){if(state_!=NativeSessionState::millennium_dos_title_handoff_boundary)return {false,"Title-to-game observation requires the title-handoff boundary"};return runtime_.observe_millennium_dos_title_to_game_parent_exec_return(o);}
MillenniumDosTitleToGameObservationResult NativeSessionController::observe_millennium_dos_title_to_game_child_status(MillenniumDosTitleToGameInterruptObservation o){if(state_!=NativeSessionState::millennium_dos_title_handoff_boundary)return {false,"Title-to-game observation requires the title-handoff boundary"};return runtime_.observe_millennium_dos_title_to_game_child_status(o);}
std::optional<MillenniumDosTitleToGameCheckpoint> NativeSessionController::millennium_dos_title_to_game_checkpoint()const{if(state_!=NativeSessionState::millennium_dos_title_handoff_boundary)return std::nullopt;return runtime_.millennium_dos_title_to_game_checkpoint();}

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
MillenniumDosPostOverlayObservationResult NativeSessionController::complete_millennium_dos_handler(const MillenniumDosHandlerCompletionObservation o){auto r=runtime_.complete_millennium_dos_handler(o);synchronize_after_runtime_change();return r;}
std::optional<MillenniumDosHandlerCompletionCheckpoint> NativeSessionController::millennium_dos_handler_completion_checkpoint()const{if(state_!=NativeSessionState::millennium_dos_post_overlay_loop)return std::nullopt;return runtime_.millennium_dos_handler_completion_checkpoint();}

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
#define EON_NATIVE_HELPER(name,type) MillenniumDosSharedHelperObservationResult NativeSessionController::name(const type o){if(state_!=NativeSessionState::millennium_dos_seventh_function&&state_!=NativeSessionState::millennium_dos_post_overlay_loop)return{false,"Shared helper requires an owned parent"};return runtime_.name(o);}
EON_NATIVE_HELPER(observe_millennium_dos_shared_helper_entry,MillenniumDosSharedHelperEntryObservation) EON_NATIVE_HELPER(observe_millennium_dos_shared_helper_word,MillenniumDosSharedHelperWordObservation) EON_NATIVE_HELPER(observe_millennium_dos_shared_helper_far_word,MillenniumDosSharedHelperFarWordObservation) EON_NATIVE_HELPER(observe_millennium_dos_shared_helper_call_return,MillenniumDosSharedHelperCallReturnObservation) EON_NATIVE_HELPER(observe_millennium_dos_shared_helper_external_return,MillenniumDosSharedHelperExternalReturnObservation)
#undef EON_NATIVE_HELPER
std::optional<MillenniumDosSharedHelperCheckpoint>NativeSessionController::millennium_dos_shared_helper_checkpoint()const{if(state_!=NativeSessionState::millennium_dos_seventh_function&&state_!=NativeSessionState::millennium_dos_post_overlay_loop)return std::nullopt;return runtime_.millennium_dos_shared_helper_checkpoint();}
MillenniumDosSharedHelperObservationResult NativeSessionController::observe_millennium_dos_special_action_helper_entry(const MillenniumDosSpecialActionHelperEntryObservation o){if(state_!=NativeSessionState::millennium_dos_post_overlay_loop)return{false,"Special action requires post-overlay loop"};return runtime_.observe_millennium_dos_special_action_helper_entry(o);} MillenniumDosSharedHelperObservationResult NativeSessionController::observe_millennium_dos_special_action_external_return(const MillenniumDosSharedHelperExternalReturnObservation o){if(state_!=NativeSessionState::millennium_dos_post_overlay_loop)return{false,"Special action requires post-overlay loop"};return runtime_.observe_millennium_dos_special_action_external_return(o);}
#define EON_NATIVE_GX_ADAPTER(name,type) MillenniumDosGxAdapterObservationResult NativeSessionController::name(const type o){if(state_!=NativeSessionState::millennium_dos_post_overlay_loop)return{false,"GX adapter requires post-overlay loop"};return runtime_.name(o);}
EON_NATIVE_GX_ADAPTER(observe_millennium_dos_second_special_action_adapter_entry,MillenniumDosGxAdapterEntryObservation) EON_NATIVE_GX_ADAPTER(observe_millennium_dos_gx_adapter_segment,MillenniumDosGxAdapterWordObservation) EON_NATIVE_GX_ADAPTER(observe_millennium_dos_gx_adapter_transfer,MillenniumDosGxAdapterTransferObservation) EON_NATIVE_GX_ADAPTER(observe_millennium_dos_gx_adapter_overlay_return,MillenniumDosGxAdapterReturnObservation) EON_NATIVE_GX_ADAPTER(observe_millennium_dos_gx_adapter_return,MillenniumDosGxAdapterReturnObservation) EON_NATIVE_GX_ADAPTER(observe_millennium_dos_second_special_action_return,MillenniumDosGxAdapterReturnObservation)
#undef EON_NATIVE_GX_ADAPTER
std::optional<MillenniumDosGxAdapterCheckpoint> NativeSessionController::millennium_dos_gx_adapter_checkpoint()const{if(state_!=NativeSessionState::millennium_dos_post_overlay_loop)return std::nullopt;return runtime_.millennium_dos_gx_adapter_checkpoint();}

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
MillenniumDosNinthHandoffObservationResult NativeSessionController::observe_millennium_dos_ninth_handoff_entry(MillenniumDosNinthHandoffEntryObservation o){if(state_!=NativeSessionState::millennium_dos_ninth_function)return{false,"F9 handoff entry requires ninth-function session"};auto r=runtime_.observe_millennium_dos_ninth_handoff_entry(o);synchronize_after_runtime_change();return r;}
#define EON_NATIVE_F9H(name,type) MillenniumDosNinthHandoffObservationResult NativeSessionController::name(type o){if(state_!=NativeSessionState::millennium_dos_ninth_function_handoff)return{false,"Observation requires F9 continuation"};return runtime_.name(o);}
EON_NATIVE_F9H(observe_millennium_dos_ninth_handoff_byte,MillenniumDosNinthHandoffByteObservation) EON_NATIVE_F9H(observe_millennium_dos_ninth_handoff_word,MillenniumDosNinthHandoffWordObservation) EON_NATIVE_F9H(observe_millennium_dos_ninth_handoff_call_return,MillenniumDosNinthHandoffCallReturnObservation) EON_NATIVE_F9H(observe_millennium_dos_ninth_handoff_zero_flag,MillenniumDosNinthHandoffZeroFlagObservation) EON_NATIVE_F9H(observe_millennium_dos_ninth_handoff_bl,MillenniumDosNinthHandoffBlObservation)
#undef EON_NATIVE_F9H
std::optional<MillenniumDosNinthHandoffCheckpoint> NativeSessionController::millennium_dos_ninth_handoff_checkpoint()const{if(state_!=NativeSessionState::millennium_dos_ninth_function_handoff)return std::nullopt;return runtime_.millennium_dos_ninth_handoff_checkpoint();}
MillenniumDosFourthFunctionObservationResult NativeSessionController::observe_millennium_dos_fourth_function_dispatch(MillenniumDosFourthFunctionDispatchObservation o){if(state_!=NativeSessionState::millennium_dos_post_overlay_loop)return{false,"Fourth-function dispatch requires post-overlay loop"};auto r=runtime_.observe_millennium_dos_fourth_function_dispatch(o);synchronize_after_runtime_change();return r;}
#define EON_NATIVE_FOURTH(n,t) MillenniumDosFourthFunctionObservationResult NativeSessionController::n(t o){if(state_!=NativeSessionState::millennium_dos_fourth_function)return{false,"Observation requires fourth-function session"};return runtime_.n(o);}
EON_NATIVE_FOURTH(observe_millennium_dos_fourth_function_word,MillenniumDosFourthFunctionWordObservation)
EON_NATIVE_FOURTH(observe_millennium_dos_fourth_function_call_return,MillenniumDosFourthFunctionCallReturnObservation)
#undef EON_NATIVE_FOURTH
std::optional<MillenniumDosFourthFunctionCheckpoint> NativeSessionController::millennium_dos_fourth_function_checkpoint()const{if(state_!=NativeSessionState::millennium_dos_fourth_function)return std::nullopt;return runtime_.millennium_dos_fourth_function_checkpoint();}
MillenniumDosFifthFunctionObservationResult NativeSessionController::observe_millennium_dos_fifth_function_dispatch(MillenniumDosFifthFunctionDispatchObservation o){if(state_!=NativeSessionState::millennium_dos_post_overlay_loop)return{false,"Fifth-function dispatch requires post-overlay loop"};auto r=runtime_.observe_millennium_dos_fifth_function_dispatch(o);synchronize_after_runtime_change();return r;} MillenniumDosFifthFunctionObservationResult NativeSessionController::observe_millennium_dos_fifth_function_call_return(MillenniumDosFifthFunctionCallReturnObservation o){if(state_!=NativeSessionState::millennium_dos_fifth_function)return{false,"Observation requires fifth-function session"};return runtime_.observe_millennium_dos_fifth_function_call_return(o);} std::optional<MillenniumDosFifthFunctionCheckpoint> NativeSessionController::millennium_dos_fifth_function_checkpoint()const{if(state_!=NativeSessionState::millennium_dos_fifth_function)return std::nullopt;return runtime_.millennium_dos_fifth_function_checkpoint();}
#define EON_NATIVE_THIRD(name, type) \
MillenniumDosThirdFunctionObservationResult NativeSessionController::name(type o) { \
    if (state_ != NativeSessionState::millennium_dos_third_function) \
        return {false, "Observation requires third-function session"}; \
    return runtime_.name(o); \
}
MillenniumDosThirdFunctionObservationResult NativeSessionController::observe_millennium_dos_third_function_dispatch(MillenniumDosThirdFunctionDispatchObservation o){if(state_!=NativeSessionState::millennium_dos_post_overlay_loop)return{false,"Third-function dispatch requires post-overlay loop"};auto r=runtime_.observe_millennium_dos_third_function_dispatch(o);synchronize_after_runtime_change();return r;}
EON_NATIVE_THIRD(observe_millennium_dos_third_function_word, MillenniumDosThirdFunctionWordObservation)
EON_NATIVE_THIRD(observe_millennium_dos_third_function_call_return, MillenniumDosThirdFunctionCallReturnObservation)
EON_NATIVE_THIRD(observe_millennium_dos_third_function_bl, MillenniumDosThirdFunctionBlObservation)
#undef EON_NATIVE_THIRD
std::optional<MillenniumDosThirdFunctionCheckpoint> NativeSessionController::millennium_dos_third_function_checkpoint()const{if(state_!=NativeSessionState::millennium_dos_third_function)return std::nullopt;return runtime_.millennium_dos_third_function_checkpoint();}
MillenniumDosFirstFunctionObservationResult NativeSessionController::observe_millennium_dos_first_function_dispatch(MillenniumDosFirstFunctionDispatchObservation o){if(state_!=NativeSessionState::millennium_dos_post_overlay_loop)return{false,"First-function dispatch requires post-overlay loop"};auto r=runtime_.observe_millennium_dos_first_function_dispatch(o);synchronize_after_runtime_change();return r;}
#define EON_NATIVE_FIRST(name,type) MillenniumDosFirstFunctionObservationResult NativeSessionController::name(type o){if(state_!=NativeSessionState::millennium_dos_first_function)return{false,"Observation requires first-function session"};return runtime_.name(o);}
EON_NATIVE_FIRST(observe_millennium_dos_first_function_call_return,MillenniumDosFirstFunctionCallReturnObservation)
EON_NATIVE_FIRST(observe_millennium_dos_first_function_bl,MillenniumDosFirstFunctionBlObservation)
#undef EON_NATIVE_FIRST
std::optional<MillenniumDosFirstFunctionCheckpoint> NativeSessionController::millennium_dos_first_function_checkpoint()const{if(state_!=NativeSessionState::millennium_dos_first_function)return std::nullopt;return runtime_.millennium_dos_first_function_checkpoint();}
MillenniumDosSecondFunctionObservationResult NativeSessionController::observe_millennium_dos_second_function_dispatch(MillenniumDosSecondFunctionDispatchObservation o){if(state_!=NativeSessionState::millennium_dos_post_overlay_loop)return{false,"Second-function dispatch requires post-overlay loop"};auto r=runtime_.observe_millennium_dos_second_function_dispatch(o);synchronize_after_runtime_change();return r;}
#define EON_NATIVE_SECOND(name,type) MillenniumDosSecondFunctionObservationResult NativeSessionController::name(type o){if(state_!=NativeSessionState::millennium_dos_second_function)return{false,"Observation requires second-function session"};return runtime_.name(o);}
EON_NATIVE_SECOND(observe_millennium_dos_second_function_runtime_byte,MillenniumDosSecondFunctionRuntimeByteObservation)
EON_NATIVE_SECOND(observe_millennium_dos_second_function_call_return,MillenniumDosSecondFunctionCallReturnObservation)
EON_NATIVE_SECOND(observe_millennium_dos_second_function_bl,MillenniumDosSecondFunctionBlObservation)
#undef EON_NATIVE_SECOND
std::optional<MillenniumDosSecondFunctionCheckpoint> NativeSessionController::millennium_dos_second_function_checkpoint()const{if(state_!=NativeSessionState::millennium_dos_second_function)return std::nullopt;return runtime_.millennium_dos_second_function_checkpoint();}
MillenniumDosSecondFunctionCallbackObservationResult NativeSessionController::observe_millennium_dos_second_function_callback_entry(MillenniumDosSecondFunctionCallbackEntryObservation o){if(state_!=NativeSessionState::millennium_dos_post_overlay_loop)return{false,"F2 callback entry requires post-overlay loop"};auto r=runtime_.observe_millennium_dos_second_function_callback_entry(o);synchronize_after_runtime_change();return r;}
#define EON_NATIVE_F2_CALLBACK(name,type) MillenniumDosSecondFunctionCallbackObservationResult NativeSessionController::name(type o){if(state_!=NativeSessionState::millennium_dos_second_function_callback)return{false,"Observation requires F2 callback session"};return runtime_.name(o);}
EON_NATIVE_F2_CALLBACK(observe_millennium_dos_second_function_callback_runtime_byte,MillenniumDosSecondFunctionCallbackRuntimeByteObservation)
EON_NATIVE_F2_CALLBACK(observe_millennium_dos_second_function_callback_runtime_word,MillenniumDosSecondFunctionCallbackRuntimeWordObservation)
EON_NATIVE_F2_CALLBACK(observe_millennium_dos_second_function_callback_call_return,MillenniumDosSecondFunctionCallbackCallReturnObservation)
EON_NATIVE_F2_CALLBACK(observe_millennium_dos_second_function_callback_bl,MillenniumDosSecondFunctionCallbackBlObservation)
EON_NATIVE_F2_CALLBACK(observe_millennium_dos_second_function_callback_jump_entry,MillenniumDosSecondFunctionCallbackJumpEntryObservation)
EON_NATIVE_F2_CALLBACK(observe_millennium_dos_second_function_callback_external_return,MillenniumDosSecondFunctionCallbackExternalReturnObservation)
#undef EON_NATIVE_F2_CALLBACK
std::optional<MillenniumDosSecondFunctionCallbackCheckpoint> NativeSessionController::millennium_dos_second_function_callback_checkpoint()const{if(state_!=NativeSessionState::millennium_dos_second_function_callback)return std::nullopt;return runtime_.millennium_dos_second_function_callback_checkpoint();}
std::optional<NativeRuntimeMemoryCheckpoint> NativeSessionController::native_runtime_memory_checkpoint()const{if(state_==NativeSessionState::menu)return std::nullopt;return runtime_.native_runtime_memory_checkpoint();}
std::optional<NativeRuntimeMemoryDiagnostics> NativeSessionController::native_runtime_memory_diagnostics()const{if(state_==NativeSessionState::menu)return std::nullopt;return runtime_.native_runtime_memory_diagnostics();}
#define EON_NATIVE_BDF(name,type) MillenniumDosBdfObservationResult NativeSessionController::name(type o){if(state_!=NativeSessionState::millennium_dos_second_function_callback)return{false,"Observation requires $0bdf continuation"};return runtime_.name(o);}
EON_NATIVE_BDF(observe_millennium_dos_bdf_byte,MillenniumDosBdfByteObservation) EON_NATIVE_BDF(observe_millennium_dos_bdf_word,MillenniumDosBdfWordObservation) EON_NATIVE_BDF(observe_millennium_dos_bdf_poll_return,MillenniumDosBdfPollReturnObservation)
EON_NATIVE_BDF(observe_millennium_dos_bdf_mapping_return,MillenniumDosBdfMappingReturnObservation)
EON_NATIVE_BDF(observe_millennium_dos_bdf_far_byte,MillenniumDosBdfFarByteObservation)
EON_NATIVE_BDF(observe_millennium_dos_bdf_external_return,MillenniumDosBdfExternalReturnObservation)
EON_NATIVE_BDF(observe_millennium_dos_bdf_terminal_jump,MillenniumDosBdfTerminalJumpObservation)
EON_NATIVE_BDF(observe_millennium_dos_bdf_mode_two_byte,MillenniumDosBdfByteObservation)
EON_NATIVE_BDF(observe_millennium_dos_bdf_mode_two_word,MillenniumDosBdfWordObservation)
EON_NATIVE_BDF(observe_millennium_dos_bdf_mode_two_far_word,MillenniumDosBdfModeTwoFarWordObservation)
EON_NATIVE_BDF(observe_millennium_dos_bdf_mode_two_far_byte,MillenniumDosBdfModeTwoFarByteObservation)
EON_NATIVE_BDF(observe_millennium_dos_bdf_mode_two_external_return,MillenniumDosBdfExternalReturnObservation)
EON_NATIVE_BDF(observe_millennium_dos_bdf_other_mode_byte,MillenniumDosBdfByteObservation)
EON_NATIVE_BDF(observe_millennium_dos_bdf_other_mode_word,MillenniumDosBdfWordObservation)
EON_NATIVE_BDF(observe_millennium_dos_bdf_other_mode_external_return,MillenniumDosBdfExternalReturnObservation)
EON_NATIVE_BDF(observe_millennium_dos_bdf_other_mode_far_word,MillenniumDosBdfModeTwoFarWordObservation)
EON_NATIVE_BDF(observe_millennium_dos_bdf_other_mode_far_byte,MillenniumDosBdfModeTwoFarByteObservation)
#undef EON_NATIVE_BDF
std::optional<MillenniumDosBdfCheckpoint>NativeSessionController::millennium_dos_bdf_checkpoint()const{if(state_!=NativeSessionState::millennium_dos_second_function_callback)return std::nullopt;return runtime_.millennium_dos_bdf_checkpoint();}
std::optional<MillenniumDosOwnedFunctionDiagnostics>
NativeSessionController::millennium_dos_owned_function_diagnostics() const {
    switch (state_) {
    case NativeSessionState::millennium_dos_fourth_function:
    case NativeSessionState::millennium_dos_fifth_function:
    case NativeSessionState::millennium_dos_third_function:
    case NativeSessionState::millennium_dos_first_function:
    case NativeSessionState::millennium_dos_second_function:
    case NativeSessionState::millennium_dos_second_function_callback:
    case NativeSessionState::millennium_dos_sixth_function:
    case NativeSessionState::millennium_dos_seventh_function:
    case NativeSessionState::millennium_dos_eighth_function:
    case NativeSessionState::millennium_dos_ninth_function:
    case NativeSessionState::millennium_dos_ninth_function_handoff:
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
#define EON_NATIVE_DEUTEROS_TITLE(name,signature,arg) DeuterosAmigaTitleDependencyObservationResult NativeSessionController::name signature { if(state_!=NativeSessionState::deuteros_amiga_title_stage_boundary) return {false,"Deuteros title observation requires the active title stage"}; return runtime_.name arg; }
EON_NATIVE_DEUTEROS_TITLE(advance_deuteros_amiga_title_local_prefix,(),())
EON_NATIVE_DEUTEROS_TITLE(observe_deuteros_amiga_title_exec_return,(const DeuterosAmigaObservedExecReturn o),(o))
EON_NATIVE_DEUTEROS_TITLE(observe_deuteros_amiga_title_open_library_return,(const DeuterosAmigaObservedOpenLibraryReturn o),(o))
EON_NATIVE_DEUTEROS_TITLE(advance_deuteros_amiga_title_post_open_library_local_path,(),())
EON_NATIVE_DEUTEROS_TITLE(observe_deuteros_amiga_title_display_base,(const DeuterosAmigaObservedDisplayBaseRead o),(o))
EON_NATIVE_DEUTEROS_TITLE(observe_deuteros_amiga_title_custom_chip_write,(const DeuterosAmigaObservedCustomChipWrite o),(o))
EON_NATIVE_DEUTEROS_TITLE(observe_deuteros_amiga_title_callback_exec_return,(const DeuterosAmigaObservedCallbackExecReturn o),(o))
EON_NATIVE_DEUTEROS_TITLE(observe_deuteros_amiga_title_service_setup_exec_return,(const DeuterosAmigaObservedServiceSetupExecReturn o),(o))
EON_NATIVE_DEUTEROS_TITLE(observe_deuteros_amiga_title_second_service_exec_return,(const DeuterosAmigaObservedServiceSetupExecReturn o),(o))
EON_NATIVE_DEUTEROS_TITLE(observe_deuteros_amiga_title_third_service_exec_return,(const DeuterosAmigaObservedServiceSetupExecReturn o),(o))
EON_NATIVE_DEUTEROS_TITLE(observe_deuteros_amiga_title_fourth_service_exec_return,(const DeuterosAmigaObservedServiceSetupExecReturn o),(o))
EON_NATIVE_DEUTEROS_TITLE(observe_deuteros_amiga_title_fifth_service_exec_return,(const DeuterosAmigaObservedServiceSetupExecReturn o),(o))
EON_NATIVE_DEUTEROS_TITLE(advance_deuteros_amiga_title_controller_pointer_seed,(),())
EON_NATIVE_DEUTEROS_TITLE(observe_deuteros_amiga_title_service_batch_graphics_return,(const DeuterosAmigaObservedGraphicsVectorReturn o),(o))
EON_NATIVE_DEUTEROS_TITLE(observe_deuteros_amiga_title_service_batch_runtime_word,(const DeuterosAmigaObservedServiceWordRead o),(o))
EON_NATIVE_DEUTEROS_TITLE(observe_deuteros_amiga_title_graphics_service_first_return,(const DeuterosAmigaObservedGraphicsVectorReturn o),(o))
EON_NATIVE_DEUTEROS_TITLE(observe_deuteros_amiga_title_graphics_service_second_return,(const DeuterosAmigaObservedGraphicsVectorReturn o),(o))
EON_NATIVE_DEUTEROS_TITLE(observe_deuteros_amiga_title_graphics_service_third_return,(const DeuterosAmigaObservedGraphicsVectorReturn o),(o))
EON_NATIVE_DEUTEROS_TITLE(observe_deuteros_amiga_title_tail_first_graphics_return,(const DeuterosAmigaObservedGraphicsVectorReturn o),(o))
EON_NATIVE_DEUTEROS_TITLE(observe_deuteros_amiga_title_tail_copy_words,(const DeuterosAmigaObservedTailCopyWords o),(o))
EON_NATIVE_DEUTEROS_TITLE(observe_deuteros_amiga_title_tail_selection_words,(const DeuterosAmigaObservedTailSelectionWords o),(o))
EON_NATIVE_DEUTEROS_TITLE(observe_deuteros_amiga_title_tail_second_graphics_return,(const DeuterosAmigaObservedGraphicsVectorReturn o),(o))
EON_NATIVE_DEUTEROS_TITLE(observe_deuteros_amiga_title_tail_repeated_selection_words,(const DeuterosAmigaObservedTailSelectionWords o),(o))
EON_NATIVE_DEUTEROS_TITLE(observe_deuteros_amiga_title_tail_repeated_graphics_return,(const DeuterosAmigaObservedGraphicsVectorReturn o),(o))
EON_NATIVE_DEUTEROS_TITLE(observe_deuteros_amiga_title_tail_repeated_wrapper_graphics_return,(const DeuterosAmigaObservedGraphicsVectorReturn o),(o))
EON_NATIVE_DEUTEROS_TITLE(observe_deuteros_amiga_title_tail_source_table,(const DeuterosAmigaObservedTailSourceTable o),(o))
EON_NATIVE_DEUTEROS_TITLE(observe_deuteros_amiga_title_tail_exec_return,(const DeuterosAmigaObservedTailExecReturn o),(o))
EON_NATIVE_DEUTEROS_TITLE(observe_deuteros_amiga_title_load_service_return,(const DeuterosAmigaObservedLocalCallReturn o),(o))
EON_NATIVE_DEUTEROS_TITLE(observe_deuteros_amiga_title_load_selector,(const DeuterosAmigaObservedLoadSelector o),(o))
EON_NATIVE_DEUTEROS_TITLE(observe_deuteros_amiga_title_load_copy_chunk,(const DeuterosAmigaObservedLoadCopyChunk o),(o))
EON_NATIVE_DEUTEROS_TITLE(observe_deuteros_amiga_title_load_dispatch_table_base,(const DeuterosAmigaObservedLoadDispatchTableBase o),(o))
EON_NATIVE_DEUTEROS_TITLE(observe_deuteros_amiga_title_load_dispatch_table_word,(const DeuterosAmigaObservedLoadDispatchTableWord o),(o))
EON_NATIVE_DEUTEROS_TITLE(observe_deuteros_amiga_title_command_opcode,(const DeuterosAmigaObservedTitleCommandOpcode o),(o))
EON_NATIVE_DEUTEROS_TITLE(observe_deuteros_amiga_title_command_operand_byte,(const DeuterosAmigaObservedTitleCommandOperandByte o),(o))
EON_NATIVE_DEUTEROS_TITLE(observe_deuteros_amiga_title_command_pointer_long,(const DeuterosAmigaObservedTitleCommandPointerLong o),(o))
EON_NATIVE_DEUTEROS_TITLE(observe_deuteros_amiga_title_command_eight_pointer,(const DeuterosAmigaObservedTitleCommandEightPointer o),(o))
EON_NATIVE_DEUTEROS_TITLE(observe_deuteros_amiga_title_command_eight_mode,(const DeuterosAmigaObservedTitleCommandEightMode o),(o))
EON_NATIVE_DEUTEROS_TITLE(observe_deuteros_amiga_title_command_eight_scale,(const DeuterosAmigaObservedTitleCommandEightScale o),(o))
EON_NATIVE_DEUTEROS_TITLE(observe_deuteros_amiga_title_command_call_return,(const DeuterosAmigaObservedTitleCommandCallReturn o),(o))
EON_NATIVE_DEUTEROS_TITLE(observe_deuteros_amiga_title_command_planar_write,(const DeuterosAmigaObservedTitleCommandPlanarWrite o),(o))
EON_NATIVE_DEUTEROS_TITLE(observe_deuteros_amiga_title_command_planar_variant_write,(const DeuterosAmigaObservedTitleCommandPlanarVariantWrite o),(o))
EON_NATIVE_DEUTEROS_TITLE(observe_deuteros_amiga_title_command_negative_service,(const DeuterosAmigaObservedTitleCommandNegativeService o),(o))
EON_NATIVE_DEUTEROS_TITLE(observe_deuteros_amiga_title_post_command_pointer_route,(const DeuterosAmigaObservedTitlePostCommandPointerRoute o),(o))
EON_NATIVE_DEUTEROS_TITLE(observe_deuteros_amiga_title_post_command_graphics_return,(const DeuterosAmigaObservedGraphicsVectorReturn o),(o))
EON_NATIVE_DEUTEROS_TITLE(advance_deuteros_amiga_title_post_command_first_dispatch,(),())
EON_NATIVE_DEUTEROS_TITLE(observe_deuteros_amiga_title_post_command_first_dispatch_header,(const DeuterosAmigaObservedTitleFirstDispatchHeader o),(o))
EON_NATIVE_DEUTEROS_TITLE(advance_deuteros_amiga_title_post_command_first_dispatch_packet,(),())
EON_NATIVE_DEUTEROS_TITLE(advance_deuteros_amiga_title_post_command_first_dispatch_decode,(),())
EON_NATIVE_DEUTEROS_TITLE(advance_deuteros_amiga_title_post_command_first_dispatch_caller_tail,(),())
EON_NATIVE_DEUTEROS_TITLE(observe_deuteros_amiga_title_post_command_first_dispatch_destination_words,(const DeuterosAmigaObservedTitleFirstDispatchDestinationWords o),(o))
EON_NATIVE_DEUTEROS_TITLE(advance_deuteros_amiga_title_post_command_second_dispatch,(),())
EON_NATIVE_DEUTEROS_TITLE(advance_deuteros_amiga_title_post_command_second_dispatch_decode,(),())
EON_NATIVE_DEUTEROS_TITLE(observe_deuteros_amiga_title_post_command_second_dispatch_destination_words,(const DeuterosAmigaObservedTitleSecondDispatchDestinationWords o),(o))
EON_NATIVE_DEUTEROS_TITLE(advance_deuteros_amiga_title_post_command_service_route_prefix,(),())
EON_NATIVE_DEUTEROS_TITLE(observe_deuteros_amiga_title_post_command_service_first_return,(const DeuterosAmigaObservedLocalCallReturn o),(o))
EON_NATIVE_DEUTEROS_TITLE(observe_deuteros_amiga_title_post_command_service_second_return,(const DeuterosAmigaObservedLocalCallReturn o),(o))
EON_NATIVE_DEUTEROS_TITLE(observe_deuteros_amiga_title_post_command_service_third_return,(const DeuterosAmigaObservedLocalCallReturn o),(o))
EON_NATIVE_DEUTEROS_TITLE(observe_deuteros_amiga_title_post_command_nested_words,(const DeuterosAmigaObservedTitlePostCommandNestedWords o),(o))
EON_NATIVE_DEUTEROS_TITLE(observe_deuteros_amiga_title_post_command_nested_call_return,(const DeuterosAmigaObservedLocalCallReturn o),(o))
EON_NATIVE_DEUTEROS_TITLE(advance_deuteros_amiga_title_post_command_nested_loop,(),())
EON_NATIVE_DEUTEROS_TITLE(observe_deuteros_amiga_title_post_command_continuation_return,(const DeuterosAmigaObservedLocalCallReturn o),(o))
EON_NATIVE_DEUTEROS_TITLE(observe_deuteros_amiga_title_post_command_pointer_chain,(const DeuterosAmigaObservedTitlePostCommandPointerChain o),(o))
EON_NATIVE_DEUTEROS_TITLE(observe_deuteros_amiga_title_post_command_dispatch_destination,(const DeuterosAmigaObservedTitlePostCommandDispatchDestination o),(o))
EON_NATIVE_DEUTEROS_TITLE(advance_deuteros_amiga_title_post_command_selected_stream,(),())
EON_NATIVE_DEUTEROS_TITLE(observe_deuteros_amiga_title_post_command_descriptor_call_return,(const DeuterosAmigaObservedLocalCallReturn o),(o))
EON_NATIVE_DEUTEROS_TITLE(advance_deuteros_amiga_title_post_command_descriptor_loop,(),())
EON_NATIVE_DEUTEROS_TITLE(observe_deuteros_amiga_title_post_command_descriptor_byte,(const DeuterosAmigaObservedTitlePostCommandDescriptorByte o),(o))
EON_NATIVE_DEUTEROS_TITLE(observe_deuteros_amiga_title_post_command_adjusted_dispatch_destination,(const DeuterosAmigaObservedTitlePostCommandAdjustedDispatchDestination o),(o))
EON_NATIVE_DEUTEROS_TITLE(observe_deuteros_amiga_title_post_adjusted_caller_pointer,(const DeuterosAmigaObservedTitlePostAdjustedCallerPointer o),(o))
EON_NATIVE_DEUTEROS_TITLE(observe_deuteros_amiga_title_post_adjusted_object_gate,(const DeuterosAmigaObservedTitlePostAdjustedObjectGate o),(o))
EON_NATIVE_DEUTEROS_TITLE(observe_deuteros_amiga_title_post_adjusted_first_helper_return,(const DeuterosAmigaObservedLocalCallReturn o),(o))
EON_NATIVE_DEUTEROS_TITLE(observe_deuteros_amiga_title_post_adjusted_second_helper_return,(const DeuterosAmigaObservedLocalCallReturn o),(o))
EON_NATIVE_DEUTEROS_TITLE(observe_deuteros_amiga_title_post_adjusted_rts_frame,(const DeuterosAmigaObservedTitlePostAdjustedRtsFrame o),(o))
EON_NATIVE_DEUTEROS_TITLE(observe_deuteros_amiga_title_post_adjusted_repeated_nested_words,(const DeuterosAmigaObservedTitlePostCommandNestedWords o),(o))
EON_NATIVE_DEUTEROS_TITLE(observe_deuteros_amiga_title_post_adjusted_repeated_nested_call_return,(const DeuterosAmigaObservedLocalCallReturn o),(o))
EON_NATIVE_DEUTEROS_TITLE(advance_deuteros_amiga_title_post_adjusted_repeated_nested_loop,(),())
EON_NATIVE_DEUTEROS_TITLE(advance_deuteros_amiga_title_post_adjusted_caller_indirect,(),())
EON_NATIVE_DEUTEROS_TITLE(observe_deuteros_amiga_title_post_adjusted_caller_indirect_return,(const DeuterosAmigaObservedTitlePostAdjustedIndirectReturn o),(o))
EON_NATIVE_DEUTEROS_TITLE(observe_deuteros_amiga_title_post_adjusted_caller_37180_return,(const DeuterosAmigaObservedTitlePostAdjusted37180Return o),(o))
EON_NATIVE_DEUTEROS_TITLE(observe_deuteros_amiga_title_post_adjusted_mode_return,(const DeuterosAmigaObservedLocalCallReturn o),(o))
EON_NATIVE_DEUTEROS_TITLE(observe_deuteros_amiga_title_post_adjusted_222c0_return,(const DeuterosAmigaObservedLocalCallReturn o),(o))
EON_NATIVE_DEUTEROS_TITLE(observe_deuteros_amiga_title_post_adjusted_timer_state,(const DeuterosAmigaObservedTitlePostAdjustedTimerState o),(o))
EON_NATIVE_DEUTEROS_TITLE(observe_deuteros_amiga_title_post_adjusted_4069a_return,(const DeuterosAmigaObservedLocalCallReturn o),(o))
EON_NATIVE_DEUTEROS_TITLE(observe_deuteros_amiga_title_post_adjusted_join_byte,(const DeuterosAmigaObservedTitlePostAdjustedJoinByte o),(o))
EON_NATIVE_DEUTEROS_TITLE(observe_deuteros_amiga_title_post_adjusted_1f9a4_return,(const DeuterosAmigaObservedLocalCallReturn o),(o))
EON_NATIVE_DEUTEROS_TITLE(observe_deuteros_amiga_title_post_adjusted_1fe88_return,(const DeuterosAmigaObservedTitlePostAdjusted1fe88Return o),(o))
EON_NATIVE_DEUTEROS_TITLE(observe_deuteros_amiga_title_post_adjusted_final_gate,(const DeuterosAmigaObservedTitlePostAdjustedFinalGate o),(o))
EON_NATIVE_DEUTEROS_TITLE(observe_deuteros_amiga_title_post_adjusted_input_return,(const DeuterosAmigaObservedTitlePostAdjustedInputReturn o),(o))
EON_NATIVE_DEUTEROS_TITLE(observe_deuteros_amiga_title_post_adjusted_repeated_input_return,(const DeuterosAmigaObservedLocalCallReturn o),(o))
#undef EON_NATIVE_DEUTEROS_TITLE

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

std::optional<DeuterosAmigaTitlePlanarPatchSnapshot>
NativeSessionController::deuteros_amiga_title_planar_patch() const {
    if (state_ != NativeSessionState::deuteros_amiga_title_display_trace_boundary) {
        return std::nullopt;
    }
    return runtime_.deuteros_amiga_title_planar_patch();
}

std::optional<DeuterosAmigaTitlePlanarSurfaceSnapshot>
NativeSessionController::deuteros_amiga_title_planar_surface() const {
    if (state_ != NativeSessionState::deuteros_amiga_title_display_trace_boundary) {
        return std::nullopt;
    }
    return runtime_.deuteros_amiga_title_planar_surface();
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
MillenniumAmigaBootstrapRelocatorObservationResult NativeSessionController::observe_millennium_amiga_bootstrap_relocator_overread(const MillenniumAmigaBootstrapRelocatorObservation o){if(state_!=NativeSessionState::millennium_amiga_bootstrap)return{false,"Relocator observation requires Millennium Amiga bootstrap"};return runtime_.observe_millennium_amiga_bootstrap_relocator_overread(o);}
MillenniumAmigaBootstrapRelocatorObservationResult NativeSessionController::observe_millennium_amiga_bootstrap_relocator_terminal_jump(const MillenniumAmigaBootstrapRelocatorObservation o){if(state_!=NativeSessionState::millennium_amiga_bootstrap)return{false,"Relocator observation requires Millennium Amiga bootstrap"};return runtime_.observe_millennium_amiga_bootstrap_relocator_terminal_jump(o);}
std::optional<MillenniumAmigaBootstrapRelocatorCheckpoint> NativeSessionController::millennium_amiga_bootstrap_relocator_checkpoint()const{if(state_!=NativeSessionState::millennium_amiga_bootstrap)return std::nullopt;return runtime_.millennium_amiga_bootstrap_relocator_checkpoint();}

std::optional<MillenniumAtariBootstrapPresentationSnapshot>
NativeSessionController::millennium_atari_bootstrap_presentation() const {
    if (state_ != NativeSessionState::millennium_atari_bootstrap) return std::nullopt;
    return runtime_.millennium_atari_bootstrap_presentation();
}
MillenniumAtariConfigConsumerResult
NativeSessionController::observe_millennium_atari_status_register(
    const MillenniumAtariStatusRegisterObservation observation) {
    if (state_ != NativeSessionState::millennium_atari_bootstrap) {
        return {false, "Atari SR observation requires Millennium Atari bootstrap"};
    }
    return runtime_.observe_millennium_atari_status_register(observation);
}
MillenniumAtariConfigConsumerResult
NativeSessionController::observe_millennium_atari_xbios_selector_two(
    const MillenniumAtariXbiosSelectorTwoObservation observation) {
    if (state_ != NativeSessionState::millennium_atari_bootstrap) {
        return {false, "XBIOS selector-2 result requires Millennium Atari bootstrap"};
    }
    return runtime_.observe_millennium_atari_xbios_selector_two(observation);
}

MillenniumAtariConfigConsumerResult
NativeSessionController::observe_millennium_atari_xbios_selector_three(
    const MillenniumAtariXbiosSelectorThreeObservation observation) {
    if (state_ != NativeSessionState::millennium_atari_bootstrap) {
        return {false, "XBIOS selector-3 result requires Millennium Atari bootstrap"};
    }
    return runtime_.observe_millennium_atari_xbios_selector_three(observation);
}
MillenniumAtariConfigConsumerResult
NativeSessionController::observe_millennium_atari_xbios_selector_four(
    const MillenniumAtariXbiosSelectorFourObservation observation) {
    if (state_ != NativeSessionState::millennium_atari_bootstrap) {
        return {false, "XBIOS selector-4 result requires Millennium Atari bootstrap"};
    }
    return runtime_.observe_millennium_atari_xbios_selector_four(observation);
}
MillenniumAtariConfigConsumerResult NativeSessionController::observe_millennium_atari_line_a(
    const MillenniumAtariLineAObservation observation) {
    if (state_ != NativeSessionState::millennium_atari_bootstrap) {
        return {false, "Line-A result requires Millennium Atari bootstrap"};
    }
    return runtime_.observe_millennium_atari_line_a(observation);
}

MillenniumAtariConfigConsumerResult NativeSessionController::observe_millennium_atari_xbios_selector_21(
    const MillenniumAtariXbiosSelector21Observation observation) {
    if (state_ != NativeSessionState::millennium_atari_bootstrap) return {false, "XBIOS selector-21 result requires Millennium Atari bootstrap"};
    return runtime_.observe_millennium_atari_xbios_selector_21(observation);
}

MillenniumAtariConfigConsumerResult NativeSessionController::observe_millennium_atari_xbios_selector_6(
    const MillenniumAtariXbiosSelector6Observation observation) {
    if (state_ != NativeSessionState::millennium_atari_bootstrap) return {false, "XBIOS selector-6 result requires Millennium Atari bootstrap"};
    return runtime_.observe_millennium_atari_xbios_selector_6(observation);
}

MillenniumAtariConfigConsumerResult NativeSessionController::observe_millennium_atari_bchg_2b55a(
    const MillenniumAtariBchgObservation observation) {
    if (state_ != NativeSessionState::millennium_atari_bootstrap) return {false, "BCHG requires Millennium Atari bootstrap"};
    return runtime_.observe_millennium_atari_bchg_2b55a(observation);
}

MillenniumAtariConfigConsumerResult NativeSessionController::execute_millennium_atari_jsr_2b55a() {
    if (state_ != NativeSessionState::millennium_atari_bootstrap) return {false, "JSR $2b55a requires Millennium Atari bootstrap"};
    return runtime_.execute_millennium_atari_jsr_2b55a();
}

MillenniumAtariConfigConsumerResult NativeSessionController::execute_millennium_atari_bsr_2b59a() {
    if (state_ != NativeSessionState::millennium_atari_bootstrap) return {false, "BSR $2b59a requires Millennium Atari bootstrap"};
    return runtime_.execute_millennium_atari_bsr_2b59a();
}

MillenniumAtariConfigConsumerResult NativeSessionController::observe_millennium_atari_d0_indexed_byte(
    const MillenniumAtariD0IndexedByteObservation observation) {
    if (state_ != NativeSessionState::millennium_atari_bootstrap) return {false, "D0-indexed byte requires Millennium Atari bootstrap"};
    return runtime_.observe_millennium_atari_d0_indexed_byte(observation);
}

MillenniumAtariConfigConsumerResult NativeSessionController::execute_millennium_atari_a1_setup(){if(state_!=NativeSessionState::millennium_atari_bootstrap)return{false,"A1 setup requires Millennium Atari bootstrap"};return runtime_.execute_millennium_atari_a1_setup();}

MillenniumAtariConfigConsumerResult NativeSessionController::observe_millennium_atari_d0_indexed_word(const MillenniumAtariD0IndexedWordObservation o){if(state_!=NativeSessionState::millennium_atari_bootstrap)return{false,"Indexed word requires Millennium Atari bootstrap"};return runtime_.observe_millennium_atari_d0_indexed_word(o);}

MillenniumAtariConfigConsumerResult NativeSessionController::observe_millennium_atari_a0_indexed_word(const MillenniumAtariA0IndexedWordObservation o){if(state_!=NativeSessionState::millennium_atari_bootstrap)return{false,"A0-indexed word requires Millennium Atari bootstrap"};return runtime_.observe_millennium_atari_a0_indexed_word(o);}

MillenniumAtariConfigConsumerResult NativeSessionController::execute_millennium_atari_loop_iteration_setup(){if(state_!=NativeSessionState::millennium_atari_bootstrap)return{false,"Loop setup requires Millennium Atari bootstrap"};return runtime_.execute_millennium_atari_loop_iteration_setup();}

MillenniumAtariConfigConsumerResult NativeSessionController::execute_millennium_atari_loop_epilogue(){if(state_!=NativeSessionState::millennium_atari_bootstrap)return{false,"Loop epilogue requires Millennium Atari bootstrap"};return runtime_.execute_millennium_atari_loop_epilogue();}

MillenniumAtariConfigConsumerResult NativeSessionController::observe_millennium_atari_movem_frame(const MillenniumAtariMovemFrameObservation o){if(state_!=NativeSessionState::millennium_atari_bootstrap)return{false,"MOVEM frame requires Millennium Atari bootstrap"};return runtime_.observe_millennium_atari_movem_frame(o);}

MillenniumAtariConfigConsumerResult NativeSessionController::execute_millennium_atari_jsr_2aa68(){if(state_!=NativeSessionState::millennium_atari_bootstrap)return{false,"JSR $2aa68 requires Millennium Atari bootstrap"};return runtime_.execute_millennium_atari_jsr_2aa68();}

MillenniumAtariConfigConsumerResult NativeSessionController::observe_millennium_atari_xbios_selector_38(const MillenniumAtariXbiosSelector38Observation o){if(state_!=NativeSessionState::millennium_atari_bootstrap)return{false,"Selector 38 requires Millennium Atari bootstrap"};return runtime_.observe_millennium_atari_xbios_selector_38(o);}

MillenniumAtariConfigConsumerResult NativeSessionController::execute_millennium_atari_jsr_2aa0c(){if(state_!=NativeSessionState::millennium_atari_bootstrap)return{false,"JSR $2aa0c requires Millennium Atari bootstrap"};return runtime_.execute_millennium_atari_jsr_2aa0c();}
MillenniumAtariConfigConsumerResult NativeSessionController::observe_millennium_atari_gemdos_selector_61(const MillenniumAtariGemdosSelector61Observation o){if(state_!=NativeSessionState::millennium_atari_bootstrap)return{false,"GEMDOS selector 61 requires Millennium Atari bootstrap"};return runtime_.observe_millennium_atari_gemdos_selector_61(o);}
MillenniumAtariConfigConsumerResult NativeSessionController::execute_millennium_atari_jsr_2a5c2(){if(state_!=NativeSessionState::millennium_atari_bootstrap)return{false,"JSR $2a5c2 requires Millennium Atari bootstrap"};return runtime_.execute_millennium_atari_jsr_2a5c2();}
MillenniumAtariConfigConsumerResult NativeSessionController::observe_millennium_atari_gemdos_selector_63(const MillenniumAtariGemdosSelector63Observation o){if(state_!=NativeSessionState::millennium_atari_bootstrap)return{false,"GEMDOS selector 63 requires Millennium Atari bootstrap"};return runtime_.observe_millennium_atari_gemdos_selector_63(o);}
MillenniumAtariConfigConsumerResult NativeSessionController::observe_millennium_atari_gemdos_selector_62(const MillenniumAtariGemdosSelector62Observation o){if(state_!=NativeSessionState::millennium_atari_bootstrap)return{false,"GEMDOS selector 62 requires Millennium Atari bootstrap"};return runtime_.observe_millennium_atari_gemdos_selector_62(o);}
MillenniumAtariConfigConsumerResult NativeSessionController::observe_millennium_atari_fread_prefix(const MillenniumAtariFreadPrefixObservation o){if(state_!=NativeSessionState::millennium_atari_bootstrap)return{false,"Fread prefix requires Millennium Atari bootstrap"};return runtime_.observe_millennium_atari_fread_prefix(o);}
MillenniumAtariConfigConsumerResult NativeSessionController::execute_millennium_atari_jsr_2b2be(){if(state_!=NativeSessionState::millennium_atari_bootstrap)return{false,"JSR $2b2be requires Millennium Atari bootstrap"};return runtime_.execute_millennium_atari_jsr_2b2be();}
MillenniumAtariConfigConsumerResult NativeSessionController::observe_millennium_atari_game_init_source_byte(const MillenniumAtariGameInitSourceByteObservation o){if(state_!=NativeSessionState::millennium_atari_bootstrap)return{false,"Game-init source byte requires Millennium Atari bootstrap"};return runtime_.observe_millennium_atari_game_init_source_byte(o);}
MillenniumAtariConfigConsumerResult NativeSessionController::observe_millennium_atari_game_init_zero_pair(const MillenniumAtariGameInitZeroPairObservation o){if(state_!=NativeSessionState::millennium_atari_bootstrap)return{false,"Game-init zero pair requires Millennium Atari bootstrap"};return runtime_.observe_millennium_atari_game_init_zero_pair(o);}
MillenniumAtariConfigConsumerResult NativeSessionController::execute_millennium_atari_game_init_zero_counter_branch(){if(state_!=NativeSessionState::millennium_atari_bootstrap)return{false,"Game-init zero counter branch requires Millennium Atari bootstrap"};return runtime_.execute_millennium_atari_game_init_zero_counter_branch();}
MillenniumAtariConfigConsumerResult NativeSessionController::observe_millennium_atari_game_init_replicated_byte(const MillenniumAtariGameInitReplicatedByteObservation o){if(state_!=NativeSessionState::millennium_atari_bootstrap)return{false,"Replicated-byte run requires Millennium Atari bootstrap"};return runtime_.observe_millennium_atari_game_init_replicated_byte(o);}
MillenniumAtariConfigConsumerResult NativeSessionController::observe_millennium_atari_game_init_swapped_pair(const MillenniumAtariGameInitSwappedPairObservation o){if(state_!=NativeSessionState::millennium_atari_bootstrap)return{false,"Swapped-pair run requires Millennium Atari bootstrap"};return runtime_.observe_millennium_atari_game_init_swapped_pair(o);}
MillenniumAtariConfigConsumerResult NativeSessionController::observe_millennium_atari_game_init_extended_run(const MillenniumAtariGameInitExtendedRunObservation o){if(state_!=NativeSessionState::millennium_atari_bootstrap)return{false,"Extended run requires Millennium Atari bootstrap"};return runtime_.observe_millennium_atari_game_init_extended_run(o);}
MillenniumAtariConfigConsumerResult NativeSessionController::execute_millennium_atari_game_init_return(){if(state_!=NativeSessionState::millennium_atari_bootstrap)return{false,"Game-init return requires Millennium Atari bootstrap"};return runtime_.execute_millennium_atari_game_init_return();}
MillenniumAtariConfigConsumerResult NativeSessionController::execute_millennium_atari_game_init_palette_copy_prefix(){if(state_!=NativeSessionState::millennium_atari_bootstrap)return{false,"Palette-copy prefix requires Millennium Atari bootstrap"};return runtime_.execute_millennium_atari_game_init_palette_copy_prefix();}
MillenniumAtariConfigConsumerResult NativeSessionController::observe_millennium_atari_game_init_palette_words(const MillenniumAtariGameInitPaletteWordsObservation o){if(state_!=NativeSessionState::millennium_atari_bootstrap)return{false,"Palette arithmetic requires Millennium Atari bootstrap"};return runtime_.observe_millennium_atari_game_init_palette_words(o);}
MillenniumAtariConfigConsumerResult NativeSessionController::observe_millennium_atari_game_init_palette_xbios_selector_6(const MillenniumAtariGameInitPaletteXbios6Observation o){if(state_!=NativeSessionState::millennium_atari_bootstrap)return{false,"Palette XBIOS selector-6 result requires Millennium Atari bootstrap"};return runtime_.observe_millennium_atari_game_init_palette_xbios_selector_6(o);}
MillenniumAtariConfigConsumerResult NativeSessionController::observe_millennium_atari_game_init_palette_recurrence(const MillenniumAtariGameInitPaletteRecurrenceObservation o){if(state_!=NativeSessionState::millennium_atari_bootstrap)return{false,"Recurrent palette pass requires Millennium Atari bootstrap"};return runtime_.observe_millennium_atari_game_init_palette_recurrence(o);}
MillenniumAtariConfigConsumerResult NativeSessionController::observe_millennium_atari_game_init_palette_rts(const MillenniumAtariGameInitPaletteRtsObservation o){if(state_!=NativeSessionState::millennium_atari_bootstrap)return{false,"Palette RTS requires Millennium Atari bootstrap"};return runtime_.observe_millennium_atari_game_init_palette_rts(o);}
MillenniumAtariConfigConsumerResult NativeSessionController::observe_millennium_atari_game_init_second_config_fopen(const MillenniumAtariGemdosSelector61Observation o){if(state_!=NativeSessionState::millennium_atari_bootstrap)return{false,"Second config Fopen requires Millennium Atari bootstrap"};return runtime_.observe_millennium_atari_game_init_second_config_fopen(o);}
MillenniumAtariConfigConsumerResult NativeSessionController::observe_millennium_atari_game_init_second_config_fread(const MillenniumAtariGemdosSelector63Observation o){if(state_!=NativeSessionState::millennium_atari_bootstrap)return{false,"Second config Fread requires Millennium Atari bootstrap"};return runtime_.observe_millennium_atari_game_init_second_config_fread(o);}
MillenniumAtariConfigConsumerResult NativeSessionController::observe_millennium_atari_game_init_second_config_fclose(const MillenniumAtariGemdosSelector62Observation o){if(state_!=NativeSessionState::millennium_atari_bootstrap)return{false,"Second config Fclose requires Millennium Atari bootstrap"};return runtime_.observe_millennium_atari_game_init_second_config_fclose(o);}
MillenniumAtariConfigConsumerResult NativeSessionController::observe_millennium_atari_game_init_second_config_rts(const MillenniumAtariGameInitSecondConfigRtsObservation o){if(state_!=NativeSessionState::millennium_atari_bootstrap)return{false,"Second config RTS requires Millennium Atari bootstrap"};return runtime_.observe_millennium_atari_game_init_second_config_rts(o);}
MillenniumAtariConfigConsumerResult NativeSessionController::observe_millennium_atari_game_init_second_config_xbios_38(const MillenniumAtariXbiosSelector38Observation o){if(state_!=NativeSessionState::millennium_atari_bootstrap)return{false,"Second config XBIOS selector-38 requires Millennium Atari bootstrap"};return runtime_.observe_millennium_atari_game_init_second_config_xbios_38(o);}
MillenniumAtariConfigConsumerResult NativeSessionController::observe_millennium_atari_game_init_config_final_rts(const MillenniumAtariGameInitSecondConfigRtsObservation o){if(state_!=NativeSessionState::millennium_atari_bootstrap)return{false,"Final config RTS requires Millennium Atari bootstrap"};return runtime_.observe_millennium_atari_game_init_config_final_rts(o);}
MillenniumAtariConfigConsumerResult NativeSessionController::observe_millennium_atari_game_init_post_config_fopen(const MillenniumAtariGemdosSelector61Observation o){if(state_!=NativeSessionState::millennium_atari_bootstrap)return{false,"Post-config Fopen requires Millennium Atari bootstrap"};return runtime_.observe_millennium_atari_game_init_post_config_fopen(o);}
MillenniumAtariConfigConsumerResult NativeSessionController::observe_millennium_atari_game_init_post_config_fread(const MillenniumAtariGemdosSelector63Observation o){if(state_!=NativeSessionState::millennium_atari_bootstrap)return{false,"Post-config Fread requires Millennium Atari bootstrap"};return runtime_.observe_millennium_atari_game_init_post_config_fread(o);}
MillenniumAtariConfigConsumerResult NativeSessionController::observe_millennium_atari_game_init_post_config_fclose(const MillenniumAtariGemdosSelector62Observation o){if(state_!=NativeSessionState::millennium_atari_bootstrap)return{false,"Post-config Fclose requires Millennium Atari bootstrap"};return runtime_.observe_millennium_atari_game_init_post_config_fclose(o);}
MillenniumAtariConfigConsumerResult NativeSessionController::observe_millennium_atari_game_init_post_config_rts(const MillenniumAtariGameInitSecondConfigRtsObservation o){if(state_!=NativeSessionState::millennium_atari_bootstrap)return{false,"Post-config RTS requires Millennium Atari bootstrap"};return runtime_.observe_millennium_atari_game_init_post_config_rts(o);}

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
