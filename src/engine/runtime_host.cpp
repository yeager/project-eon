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

NativeCodeImageRegistryDiagnostics RuntimeHost::native_code_image_registry_diagnostics() const {
    // Registry counts remain safe during revocation, but the active binding
    // must disappear before any coordinator-owned media can be released.
    return eon::native_code_image_registry_diagnostics(
        revoking() ? std::nullopt : NativeSessionController::session_snapshot());
}

std::optional<MillenniumDosPresentationSnapshot> RuntimeHost::millennium_dos_presentation() const {
    if (revoking()) return std::nullopt;
    return NativeSessionController::millennium_dos_presentation();
}

std::optional<MillenniumDosStartupInputSnapshot> RuntimeHost::millennium_dos_startup_input() const {
    if (revoking()) return std::nullopt;
    return NativeSessionController::millennium_dos_startup_input();
}
MillenniumDosSoundDriverLoadObservationResult RuntimeHost::observe_millennium_dos_sound_driver_load(MillenniumDosSoundDriverLoadObservation o){if(revoking())return {false,"Sound-driver load rejected during revocation"};return NativeSessionController::observe_millennium_dos_sound_driver_load(std::move(o));}
std::optional<MillenniumDosSoundDriverLoadCheckpoint> RuntimeHost::millennium_dos_sound_driver_load_checkpoint()const{if(revoking())return std::nullopt;return NativeSessionController::millennium_dos_sound_driver_load_checkpoint();}
std::optional<MillenniumDosCompatibilityRunnerCheckpoint> RuntimeHost::tick_millennium_dos_compatibility_runner(){if(revoking())return std::nullopt;return NativeSessionController::tick_millennium_dos_compatibility_runner();}
MillenniumDosTitleExecEntryObservationResult RuntimeHost::observe_millennium_dos_title_child_process_entry(MillenniumDosTitleExecProcessEntry o){if(revoking())return {false,"Title child entry rejected during revocation"};return NativeSessionController::observe_millennium_dos_title_child_process_entry(o);}
MillenniumDosTitleExecEntryObservationResult RuntimeHost::advance_millennium_dos_title_entry_prefix(MillenniumDosTitleExecPrefixObservation o){if(revoking())return {false,"Title entry prefix rejected during revocation"};return NativeSessionController::advance_millennium_dos_title_entry_prefix(o);}
std::optional<MillenniumDosTitleExecEntryRuntimeCheckpoint> RuntimeHost::millennium_dos_title_exec_entry_checkpoint()const{if(revoking())return std::nullopt;return NativeSessionController::millennium_dos_title_exec_entry_checkpoint();}
MillenniumDosTitleInitializationObservationResult RuntimeHost::observe_millennium_dos_title_private_interrupt_result(MillenniumDosTitlePrivateInterruptResultObservation o){if(revoking())return {false,"Title private-interrupt result rejected during revocation"};return NativeSessionController::observe_millennium_dos_title_private_interrupt_result(o);}
MillenniumDosTitleInitializationObservationResult RuntimeHost::observe_millennium_dos_title_selected_callee_result(MillenniumDosTitleSelectedCalleeResultObservation o){if(revoking())return {false,"Selected title-callee result rejected during revocation"};return NativeSessionController::observe_millennium_dos_title_selected_callee_result(o);}
MillenniumDosTitleInitializationObservationResult RuntimeHost::observe_millennium_dos_title_bios_result(MillenniumDosTitleBiosResultObservation o){if(revoking())return {false,"Title BIOS result rejected during revocation"};return NativeSessionController::observe_millennium_dos_title_bios_result(o);}
MillenniumDosTitleInitializationObservationResult RuntimeHost::observe_millennium_dos_title_dos_memory_result(MillenniumDosTitleDosResultObservation o){if(revoking())return {false,"Title DOS-memory result rejected during revocation"};return NativeSessionController::observe_millennium_dos_title_dos_memory_result(o);}
MillenniumDosTitleInitializationObservationResult RuntimeHost::observe_millennium_dos_title_dos_file_result(MillenniumDosTitleDosFileResultObservation o){if(revoking())return {false,"Title DOS-file result rejected during revocation"};return NativeSessionController::observe_millennium_dos_title_dos_file_result(o);}
MillenniumDosTitleInitializationObservationResult RuntimeHost::observe_millennium_dos_title_dos_vector_result(MillenniumDosTitleDosVectorResultObservation o){if(revoking())return {false,"Title DOS-vector result rejected during revocation"};return NativeSessionController::observe_millennium_dos_title_dos_vector_result(o);}
MillenniumDosTitleInitializationObservationResult RuntimeHost::observe_millennium_dos_title_setup_bios_result(MillenniumDosTitleSetupBiosResultObservation o){if(revoking())return {false,"Title setup BIOS result rejected during revocation"};return NativeSessionController::observe_millennium_dos_title_setup_bios_result(o);}
MillenniumDosTitleInitializationObservationResult RuntimeHost::observe_millennium_dos_title_far_words(MillenniumDosTitleFarWordsObservation o){if(revoking())return {false,"Title far words rejected during revocation"};return NativeSessionController::observe_millennium_dos_title_far_words(o);}
MillenniumDosTitleInitializationObservationResult RuntimeHost::observe_millennium_dos_title_far_word(MillenniumDosTitleFarWordObservation o){if(revoking())return {false,"Title far word rejected during revocation"};return NativeSessionController::observe_millennium_dos_title_far_word(o);}
MillenniumDosTitleInitializationObservationResult RuntimeHost::observe_millennium_dos_title_far_byte(MillenniumDosTitleFarByteObservation o){if(revoking())return {false,"Title far byte rejected during revocation"};return NativeSessionController::observe_millennium_dos_title_far_byte(o);}

MillenniumDosTitleToGameObservationResult RuntimeHost::observe_millennium_dos_title_to_game_call_return(MillenniumDosTitleToGameCallReturnObservation o){if(revoking())return {false,"Title-to-game observation rejected during revocation"};return NativeSessionController::observe_millennium_dos_title_to_game_call_return(o);}
MillenniumDosTitleToGameObservationResult RuntimeHost::observe_millennium_dos_title_to_game_stack_word(MillenniumDosTitleToGameStackWordObservation o){if(revoking())return {false,"Title-to-game observation rejected during revocation"};return NativeSessionController::observe_millennium_dos_title_to_game_stack_word(o);}
MillenniumDosTitleToGameObservationResult RuntimeHost::observe_millennium_dos_title_to_game_title_termination(MillenniumDosTitleToGameInterruptObservation o){if(revoking())return {false,"Title-to-game observation rejected during revocation"};return NativeSessionController::observe_millennium_dos_title_to_game_title_termination(o);}
MillenniumDosTitleToGameObservationResult RuntimeHost::observe_millennium_dos_title_to_game_parent_exec_return(MillenniumDosTitleToGameInterruptObservation o){if(revoking())return {false,"Title-to-game observation rejected during revocation"};return NativeSessionController::observe_millennium_dos_title_to_game_parent_exec_return(o);}
MillenniumDosTitleToGameObservationResult RuntimeHost::observe_millennium_dos_title_to_game_child_status(MillenniumDosTitleToGameInterruptObservation o){if(revoking())return {false,"Title-to-game observation rejected during revocation"};return NativeSessionController::observe_millennium_dos_title_to_game_child_status(o);}
std::optional<MillenniumDosTitleToGameCheckpoint> RuntimeHost::millennium_dos_title_to_game_checkpoint()const{if(revoking())return std::nullopt;return NativeSessionController::millennium_dos_title_to_game_checkpoint();}

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
MillenniumDosPostOverlayObservationResult RuntimeHost::complete_millennium_dos_handler(const MillenniumDosHandlerCompletionObservation o){if(revoking())return{false,"Handler completion rejected during source revocation"};return NativeSessionController::complete_millennium_dos_handler(o);}
std::optional<MillenniumDosHandlerCompletionCheckpoint> RuntimeHost::millennium_dos_handler_completion_checkpoint()const{if(revoking())return std::nullopt;return NativeSessionController::millennium_dos_handler_completion_checkpoint();}

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
#define EON_HOST_HELPER(name,type) MillenniumDosSharedHelperObservationResult RuntimeHost::name(const type o){if(revoking())return{false,"Shared helper observation rejected during source revocation"};return NativeSessionController::name(o);}
EON_HOST_HELPER(observe_millennium_dos_shared_helper_entry,MillenniumDosSharedHelperEntryObservation) EON_HOST_HELPER(observe_millennium_dos_shared_helper_word,MillenniumDosSharedHelperWordObservation) EON_HOST_HELPER(observe_millennium_dos_shared_helper_far_word,MillenniumDosSharedHelperFarWordObservation) EON_HOST_HELPER(observe_millennium_dos_shared_helper_call_return,MillenniumDosSharedHelperCallReturnObservation) EON_HOST_HELPER(observe_millennium_dos_shared_helper_external_return,MillenniumDosSharedHelperExternalReturnObservation)
#undef EON_HOST_HELPER
std::optional<MillenniumDosSharedHelperCheckpoint>RuntimeHost::millennium_dos_shared_helper_checkpoint()const{if(revoking())return std::nullopt;return NativeSessionController::millennium_dos_shared_helper_checkpoint();}
MillenniumDosSharedHelperObservationResult RuntimeHost::observe_millennium_dos_special_action_helper_entry(const MillenniumDosSpecialActionHelperEntryObservation o){if(revoking())return{false,"Special action rejected during source revocation"};return NativeSessionController::observe_millennium_dos_special_action_helper_entry(o);} MillenniumDosSharedHelperObservationResult RuntimeHost::observe_millennium_dos_special_action_external_return(const MillenniumDosSharedHelperExternalReturnObservation o){if(revoking())return{false,"Special action rejected during source revocation"};return NativeSessionController::observe_millennium_dos_special_action_external_return(o);}
#define EON_HOST_GX_ADAPTER(name,type) MillenniumDosGxAdapterObservationResult RuntimeHost::name(const type o){if(revoking())return{false,"GX adapter rejected during source revocation"};return NativeSessionController::name(o);}
EON_HOST_GX_ADAPTER(observe_millennium_dos_second_special_action_adapter_entry,MillenniumDosGxAdapterEntryObservation) EON_HOST_GX_ADAPTER(observe_millennium_dos_gx_adapter_segment,MillenniumDosGxAdapterWordObservation) EON_HOST_GX_ADAPTER(observe_millennium_dos_gx_adapter_transfer,MillenniumDosGxAdapterTransferObservation) EON_HOST_GX_ADAPTER(observe_millennium_dos_gx_adapter_overlay_return,MillenniumDosGxAdapterReturnObservation) EON_HOST_GX_ADAPTER(observe_millennium_dos_gx_adapter_return,MillenniumDosGxAdapterReturnObservation) EON_HOST_GX_ADAPTER(observe_millennium_dos_second_special_action_return,MillenniumDosGxAdapterReturnObservation)
#undef EON_HOST_GX_ADAPTER
std::optional<MillenniumDosGxAdapterCheckpoint> RuntimeHost::millennium_dos_gx_adapter_checkpoint()const{if(revoking())return std::nullopt;return NativeSessionController::millennium_dos_gx_adapter_checkpoint();}

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
#define EON_HOST_F9H(name,type) MillenniumDosNinthHandoffObservationResult RuntimeHost::name(type o){if(revoking())return{false,"F9 continuation rejected during source revocation"};return NativeSessionController::name(o);}
EON_HOST_F9H(observe_millennium_dos_ninth_handoff_entry,MillenniumDosNinthHandoffEntryObservation) EON_HOST_F9H(observe_millennium_dos_ninth_handoff_byte,MillenniumDosNinthHandoffByteObservation) EON_HOST_F9H(observe_millennium_dos_ninth_handoff_word,MillenniumDosNinthHandoffWordObservation) EON_HOST_F9H(observe_millennium_dos_ninth_handoff_call_return,MillenniumDosNinthHandoffCallReturnObservation) EON_HOST_F9H(observe_millennium_dos_ninth_handoff_zero_flag,MillenniumDosNinthHandoffZeroFlagObservation) EON_HOST_F9H(observe_millennium_dos_ninth_handoff_bl,MillenniumDosNinthHandoffBlObservation)
#undef EON_HOST_F9H
std::optional<MillenniumDosNinthHandoffCheckpoint> RuntimeHost::millennium_dos_ninth_handoff_checkpoint()const{if(revoking())return std::nullopt;return NativeSessionController::millennium_dos_ninth_handoff_checkpoint();}
#define EON_HOST_FOURTH(n,t) MillenniumDosFourthFunctionObservationResult RuntimeHost::n(t o){if(revoking())return{false,"Fourth-function observation rejected during source revocation"};return NativeSessionController::n(o);}
EON_HOST_FOURTH(observe_millennium_dos_fourth_function_dispatch,MillenniumDosFourthFunctionDispatchObservation)
EON_HOST_FOURTH(observe_millennium_dos_fourth_function_word,MillenniumDosFourthFunctionWordObservation)
EON_HOST_FOURTH(observe_millennium_dos_fourth_function_call_return,MillenniumDosFourthFunctionCallReturnObservation)
#undef EON_HOST_FOURTH
std::optional<MillenniumDosFourthFunctionCheckpoint> RuntimeHost::millennium_dos_fourth_function_checkpoint()const{if(revoking())return std::nullopt;return NativeSessionController::millennium_dos_fourth_function_checkpoint();}
MillenniumDosFifthFunctionObservationResult RuntimeHost::observe_millennium_dos_fifth_function_dispatch(MillenniumDosFifthFunctionDispatchObservation o){if(revoking())return{false,"Fifth-function observation rejected during source revocation"};return NativeSessionController::observe_millennium_dos_fifth_function_dispatch(o);} MillenniumDosFifthFunctionObservationResult RuntimeHost::observe_millennium_dos_fifth_function_call_return(MillenniumDosFifthFunctionCallReturnObservation o){if(revoking())return{false,"Fifth-function observation rejected during source revocation"};return NativeSessionController::observe_millennium_dos_fifth_function_call_return(o);} std::optional<MillenniumDosFifthFunctionCheckpoint> RuntimeHost::millennium_dos_fifth_function_checkpoint()const{if(revoking())return std::nullopt;return NativeSessionController::millennium_dos_fifth_function_checkpoint();}
#define EON_HOST_THIRD(name,type) MillenniumDosThirdFunctionObservationResult RuntimeHost::name(type o){if(revoking())return{false,"Third-function observation rejected during source revocation"};return NativeSessionController::name(o);}
EON_HOST_THIRD(observe_millennium_dos_third_function_dispatch,MillenniumDosThirdFunctionDispatchObservation)
EON_HOST_THIRD(observe_millennium_dos_third_function_word,MillenniumDosThirdFunctionWordObservation)
EON_HOST_THIRD(observe_millennium_dos_third_function_call_return,MillenniumDosThirdFunctionCallReturnObservation)
EON_HOST_THIRD(observe_millennium_dos_third_function_bl,MillenniumDosThirdFunctionBlObservation)
#undef EON_HOST_THIRD
std::optional<MillenniumDosThirdFunctionCheckpoint> RuntimeHost::millennium_dos_third_function_checkpoint()const{if(revoking())return std::nullopt;return NativeSessionController::millennium_dos_third_function_checkpoint();}
#define EON_HOST_FIRST(name,type) MillenniumDosFirstFunctionObservationResult RuntimeHost::name(type o){if(revoking())return{false,"First-function observation rejected during source revocation"};return NativeSessionController::name(o);}
EON_HOST_FIRST(observe_millennium_dos_first_function_dispatch,MillenniumDosFirstFunctionDispatchObservation)
EON_HOST_FIRST(observe_millennium_dos_first_function_call_return,MillenniumDosFirstFunctionCallReturnObservation)
EON_HOST_FIRST(observe_millennium_dos_first_function_bl,MillenniumDosFirstFunctionBlObservation)
#undef EON_HOST_FIRST
std::optional<MillenniumDosFirstFunctionCheckpoint> RuntimeHost::millennium_dos_first_function_checkpoint()const{if(revoking())return std::nullopt;return NativeSessionController::millennium_dos_first_function_checkpoint();}
#define EON_HOST_SECOND(name,type) MillenniumDosSecondFunctionObservationResult RuntimeHost::name(type o){if(revoking())return{false,"Second-function observation rejected during source revocation"};return NativeSessionController::name(o);}
EON_HOST_SECOND(observe_millennium_dos_second_function_dispatch,MillenniumDosSecondFunctionDispatchObservation)
EON_HOST_SECOND(observe_millennium_dos_second_function_runtime_byte,MillenniumDosSecondFunctionRuntimeByteObservation)
EON_HOST_SECOND(observe_millennium_dos_second_function_call_return,MillenniumDosSecondFunctionCallReturnObservation)
EON_HOST_SECOND(observe_millennium_dos_second_function_bl,MillenniumDosSecondFunctionBlObservation)
#undef EON_HOST_SECOND
std::optional<MillenniumDosSecondFunctionCheckpoint> RuntimeHost::millennium_dos_second_function_checkpoint()const{if(revoking())return std::nullopt;return NativeSessionController::millennium_dos_second_function_checkpoint();}
#define EON_HOST_F2_CALLBACK(name,type) MillenniumDosSecondFunctionCallbackObservationResult RuntimeHost::name(type o){if(revoking())return{false,"F2 callback observation rejected during source revocation"};return NativeSessionController::name(o);}
EON_HOST_F2_CALLBACK(observe_millennium_dos_second_function_callback_entry,MillenniumDosSecondFunctionCallbackEntryObservation)
EON_HOST_F2_CALLBACK(observe_millennium_dos_second_function_callback_runtime_byte,MillenniumDosSecondFunctionCallbackRuntimeByteObservation)
EON_HOST_F2_CALLBACK(observe_millennium_dos_second_function_callback_runtime_word,MillenniumDosSecondFunctionCallbackRuntimeWordObservation)
EON_HOST_F2_CALLBACK(observe_millennium_dos_second_function_callback_call_return,MillenniumDosSecondFunctionCallbackCallReturnObservation)
EON_HOST_F2_CALLBACK(observe_millennium_dos_second_function_callback_bl,MillenniumDosSecondFunctionCallbackBlObservation)
EON_HOST_F2_CALLBACK(observe_millennium_dos_second_function_callback_jump_entry,MillenniumDosSecondFunctionCallbackJumpEntryObservation)
EON_HOST_F2_CALLBACK(observe_millennium_dos_second_function_callback_external_return,MillenniumDosSecondFunctionCallbackExternalReturnObservation)
#undef EON_HOST_F2_CALLBACK
std::optional<MillenniumDosSecondFunctionCallbackCheckpoint> RuntimeHost::millennium_dos_second_function_callback_checkpoint()const{if(revoking())return std::nullopt;return NativeSessionController::millennium_dos_second_function_callback_checkpoint();}
std::optional<NativeRuntimeMemoryCheckpoint> RuntimeHost::native_runtime_memory_checkpoint()const{if(revoking())return std::nullopt;return NativeSessionController::native_runtime_memory_checkpoint();}
std::optional<DeuterosAmigaTitlePlanarPatchSnapshot> RuntimeHost::deuteros_amiga_title_planar_patch()const{if(revoking())return std::nullopt;return NativeSessionController::deuteros_amiga_title_planar_patch();}
std::optional<DeuterosAmigaTitlePlanarSurfaceSnapshot> RuntimeHost::deuteros_amiga_title_planar_surface()const{if(revoking())return std::nullopt;return NativeSessionController::deuteros_amiga_title_planar_surface();}
std::optional<NativeRuntimeMemoryDiagnostics> RuntimeHost::native_runtime_memory_diagnostics()const{if(revoking())return std::nullopt;return NativeSessionController::native_runtime_memory_diagnostics();}
#define EON_HOST_BDF(name,type) MillenniumDosBdfObservationResult RuntimeHost::name(type o){if(revoking())return{false,"$0bdf observation rejected during source revocation"};return NativeSessionController::name(o);}
EON_HOST_BDF(observe_millennium_dos_bdf_byte,MillenniumDosBdfByteObservation) EON_HOST_BDF(observe_millennium_dos_bdf_word,MillenniumDosBdfWordObservation) EON_HOST_BDF(observe_millennium_dos_bdf_poll_return,MillenniumDosBdfPollReturnObservation)
EON_HOST_BDF(observe_millennium_dos_bdf_mapping_return,MillenniumDosBdfMappingReturnObservation)
EON_HOST_BDF(observe_millennium_dos_bdf_far_byte,MillenniumDosBdfFarByteObservation)
EON_HOST_BDF(observe_millennium_dos_bdf_external_return,MillenniumDosBdfExternalReturnObservation)
EON_HOST_BDF(observe_millennium_dos_bdf_terminal_jump,MillenniumDosBdfTerminalJumpObservation)
EON_HOST_BDF(observe_millennium_dos_bdf_mode_two_byte,MillenniumDosBdfByteObservation)
EON_HOST_BDF(observe_millennium_dos_bdf_mode_two_word,MillenniumDosBdfWordObservation)
EON_HOST_BDF(observe_millennium_dos_bdf_mode_two_far_word,MillenniumDosBdfModeTwoFarWordObservation)
EON_HOST_BDF(observe_millennium_dos_bdf_mode_two_far_byte,MillenniumDosBdfModeTwoFarByteObservation)
EON_HOST_BDF(observe_millennium_dos_bdf_mode_two_external_return,MillenniumDosBdfExternalReturnObservation)
EON_HOST_BDF(observe_millennium_dos_bdf_other_mode_byte,MillenniumDosBdfByteObservation)
EON_HOST_BDF(observe_millennium_dos_bdf_other_mode_word,MillenniumDosBdfWordObservation)
EON_HOST_BDF(observe_millennium_dos_bdf_other_mode_external_return,MillenniumDosBdfExternalReturnObservation)
EON_HOST_BDF(observe_millennium_dos_bdf_other_mode_far_word,MillenniumDosBdfModeTwoFarWordObservation)
EON_HOST_BDF(observe_millennium_dos_bdf_other_mode_far_byte,MillenniumDosBdfModeTwoFarByteObservation)
#undef EON_HOST_BDF
std::optional<MillenniumDosBdfCheckpoint>RuntimeHost::millennium_dos_bdf_checkpoint()const{if(revoking())return std::nullopt;return NativeSessionController::millennium_dos_bdf_checkpoint();}
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

std::optional<DeuterosAmigaTitleDependencyChainCheckpoint>
RuntimeHost::deuteros_amiga_title_dependency_chain_checkpoint() const {
    if (revoking()) return std::nullopt;
    return NativeSessionController::deuteros_amiga_title_dependency_chain_checkpoint();
}
#define EON_HOST_DEUTEROS_TITLE(name,signature,arg) DeuterosAmigaTitleDependencyObservationResult RuntimeHost::name signature { if(revoking()) return {false,"Deuteros title observation rejected during source revocation"}; return NativeSessionController::name arg; }
EON_HOST_DEUTEROS_TITLE(advance_deuteros_amiga_title_local_prefix,(),())
EON_HOST_DEUTEROS_TITLE(observe_deuteros_amiga_title_exec_return,(const DeuterosAmigaObservedExecReturn o),(o))
EON_HOST_DEUTEROS_TITLE(observe_deuteros_amiga_title_open_library_return,(const DeuterosAmigaObservedOpenLibraryReturn o),(o))
EON_HOST_DEUTEROS_TITLE(advance_deuteros_amiga_title_post_open_library_local_path,(),())
EON_HOST_DEUTEROS_TITLE(observe_deuteros_amiga_title_display_base,(const DeuterosAmigaObservedDisplayBaseRead o),(o))
EON_HOST_DEUTEROS_TITLE(observe_deuteros_amiga_title_custom_chip_write,(const DeuterosAmigaObservedCustomChipWrite o),(o))
EON_HOST_DEUTEROS_TITLE(observe_deuteros_amiga_title_callback_exec_return,(const DeuterosAmigaObservedCallbackExecReturn o),(o))
EON_HOST_DEUTEROS_TITLE(observe_deuteros_amiga_title_service_setup_exec_return,(const DeuterosAmigaObservedServiceSetupExecReturn o),(o))
EON_HOST_DEUTEROS_TITLE(observe_deuteros_amiga_title_second_service_exec_return,(const DeuterosAmigaObservedServiceSetupExecReturn o),(o))
EON_HOST_DEUTEROS_TITLE(observe_deuteros_amiga_title_third_service_exec_return,(const DeuterosAmigaObservedServiceSetupExecReturn o),(o))
EON_HOST_DEUTEROS_TITLE(observe_deuteros_amiga_title_fourth_service_exec_return,(const DeuterosAmigaObservedServiceSetupExecReturn o),(o))
EON_HOST_DEUTEROS_TITLE(observe_deuteros_amiga_title_fifth_service_exec_return,(const DeuterosAmigaObservedServiceSetupExecReturn o),(o))
EON_HOST_DEUTEROS_TITLE(advance_deuteros_amiga_title_controller_pointer_seed,(),())
EON_HOST_DEUTEROS_TITLE(observe_deuteros_amiga_title_service_batch_graphics_return,(const DeuterosAmigaObservedGraphicsVectorReturn o),(o))
EON_HOST_DEUTEROS_TITLE(observe_deuteros_amiga_title_service_batch_runtime_word,(const DeuterosAmigaObservedServiceWordRead o),(o))
EON_HOST_DEUTEROS_TITLE(observe_deuteros_amiga_title_graphics_service_first_return,(const DeuterosAmigaObservedGraphicsVectorReturn o),(o))
EON_HOST_DEUTEROS_TITLE(observe_deuteros_amiga_title_graphics_service_second_return,(const DeuterosAmigaObservedGraphicsVectorReturn o),(o))
EON_HOST_DEUTEROS_TITLE(observe_deuteros_amiga_title_graphics_service_third_return,(const DeuterosAmigaObservedGraphicsVectorReturn o),(o))
EON_HOST_DEUTEROS_TITLE(observe_deuteros_amiga_title_tail_first_graphics_return,(const DeuterosAmigaObservedGraphicsVectorReturn o),(o))
EON_HOST_DEUTEROS_TITLE(observe_deuteros_amiga_title_tail_copy_words,(const DeuterosAmigaObservedTailCopyWords o),(o))
EON_HOST_DEUTEROS_TITLE(observe_deuteros_amiga_title_tail_selection_words,(const DeuterosAmigaObservedTailSelectionWords o),(o))
EON_HOST_DEUTEROS_TITLE(observe_deuteros_amiga_title_tail_second_graphics_return,(const DeuterosAmigaObservedGraphicsVectorReturn o),(o))
EON_HOST_DEUTEROS_TITLE(observe_deuteros_amiga_title_tail_repeated_selection_words,(const DeuterosAmigaObservedTailSelectionWords o),(o))
EON_HOST_DEUTEROS_TITLE(observe_deuteros_amiga_title_tail_repeated_graphics_return,(const DeuterosAmigaObservedGraphicsVectorReturn o),(o))
EON_HOST_DEUTEROS_TITLE(observe_deuteros_amiga_title_tail_repeated_wrapper_graphics_return,(const DeuterosAmigaObservedGraphicsVectorReturn o),(o))
EON_HOST_DEUTEROS_TITLE(observe_deuteros_amiga_title_tail_source_table,(const DeuterosAmigaObservedTailSourceTable o),(o))
EON_HOST_DEUTEROS_TITLE(observe_deuteros_amiga_title_tail_exec_return,(const DeuterosAmigaObservedTailExecReturn o),(o))
EON_HOST_DEUTEROS_TITLE(observe_deuteros_amiga_title_load_service_return,(const DeuterosAmigaObservedLocalCallReturn o),(o))
EON_HOST_DEUTEROS_TITLE(observe_deuteros_amiga_title_load_selector,(const DeuterosAmigaObservedLoadSelector o),(o))
EON_HOST_DEUTEROS_TITLE(observe_deuteros_amiga_title_load_copy_chunk,(const DeuterosAmigaObservedLoadCopyChunk o),(o))
EON_HOST_DEUTEROS_TITLE(observe_deuteros_amiga_title_load_dispatch_table_base,(const DeuterosAmigaObservedLoadDispatchTableBase o),(o))
EON_HOST_DEUTEROS_TITLE(observe_deuteros_amiga_title_load_dispatch_table_word,(const DeuterosAmigaObservedLoadDispatchTableWord o),(o))
EON_HOST_DEUTEROS_TITLE(observe_deuteros_amiga_title_command_opcode,(const DeuterosAmigaObservedTitleCommandOpcode o),(o))
EON_HOST_DEUTEROS_TITLE(observe_deuteros_amiga_title_command_operand_byte,(const DeuterosAmigaObservedTitleCommandOperandByte o),(o))
EON_HOST_DEUTEROS_TITLE(observe_deuteros_amiga_title_command_pointer_long,(const DeuterosAmigaObservedTitleCommandPointerLong o),(o))
EON_HOST_DEUTEROS_TITLE(observe_deuteros_amiga_title_command_eight_pointer,(const DeuterosAmigaObservedTitleCommandEightPointer o),(o))
EON_HOST_DEUTEROS_TITLE(observe_deuteros_amiga_title_command_eight_mode,(const DeuterosAmigaObservedTitleCommandEightMode o),(o))
EON_HOST_DEUTEROS_TITLE(observe_deuteros_amiga_title_command_eight_scale,(const DeuterosAmigaObservedTitleCommandEightScale o),(o))
EON_HOST_DEUTEROS_TITLE(observe_deuteros_amiga_title_command_call_return,(const DeuterosAmigaObservedTitleCommandCallReturn o),(o))
EON_HOST_DEUTEROS_TITLE(observe_deuteros_amiga_title_command_planar_write,(const DeuterosAmigaObservedTitleCommandPlanarWrite o),(o))
EON_HOST_DEUTEROS_TITLE(observe_deuteros_amiga_title_command_planar_variant_write,(const DeuterosAmigaObservedTitleCommandPlanarVariantWrite o),(o))
EON_HOST_DEUTEROS_TITLE(observe_deuteros_amiga_title_command_negative_service,(const DeuterosAmigaObservedTitleCommandNegativeService o),(o))
EON_HOST_DEUTEROS_TITLE(observe_deuteros_amiga_title_post_command_pointer_route,(const DeuterosAmigaObservedTitlePostCommandPointerRoute o),(o))
EON_HOST_DEUTEROS_TITLE(observe_deuteros_amiga_title_post_command_graphics_return,(const DeuterosAmigaObservedGraphicsVectorReturn o),(o))
EON_HOST_DEUTEROS_TITLE(advance_deuteros_amiga_title_post_command_first_dispatch,(),())
EON_HOST_DEUTEROS_TITLE(observe_deuteros_amiga_title_post_command_first_dispatch_header,(const DeuterosAmigaObservedTitleFirstDispatchHeader o),(o))
EON_HOST_DEUTEROS_TITLE(advance_deuteros_amiga_title_post_command_first_dispatch_packet,(),())
EON_HOST_DEUTEROS_TITLE(advance_deuteros_amiga_title_post_command_first_dispatch_decode,(),())
EON_HOST_DEUTEROS_TITLE(advance_deuteros_amiga_title_post_command_first_dispatch_caller_tail,(),())
EON_HOST_DEUTEROS_TITLE(observe_deuteros_amiga_title_post_command_first_dispatch_destination_words,(const DeuterosAmigaObservedTitleFirstDispatchDestinationWords o),(o))
EON_HOST_DEUTEROS_TITLE(advance_deuteros_amiga_title_post_command_second_dispatch,(),())
EON_HOST_DEUTEROS_TITLE(advance_deuteros_amiga_title_post_command_second_dispatch_decode,(),())
EON_HOST_DEUTEROS_TITLE(observe_deuteros_amiga_title_post_command_second_dispatch_destination_words,(const DeuterosAmigaObservedTitleSecondDispatchDestinationWords o),(o))
EON_HOST_DEUTEROS_TITLE(advance_deuteros_amiga_title_post_command_service_route_prefix,(),())
EON_HOST_DEUTEROS_TITLE(observe_deuteros_amiga_title_post_command_service_first_return,(const DeuterosAmigaObservedLocalCallReturn o),(o))
EON_HOST_DEUTEROS_TITLE(observe_deuteros_amiga_title_post_command_service_second_return,(const DeuterosAmigaObservedLocalCallReturn o),(o))
EON_HOST_DEUTEROS_TITLE(observe_deuteros_amiga_title_post_command_service_third_return,(const DeuterosAmigaObservedLocalCallReturn o),(o))
EON_HOST_DEUTEROS_TITLE(observe_deuteros_amiga_title_post_command_nested_words,(const DeuterosAmigaObservedTitlePostCommandNestedWords o),(o))
EON_HOST_DEUTEROS_TITLE(observe_deuteros_amiga_title_post_command_nested_call_return,(const DeuterosAmigaObservedLocalCallReturn o),(o))
EON_HOST_DEUTEROS_TITLE(advance_deuteros_amiga_title_post_command_nested_loop,(),())
EON_HOST_DEUTEROS_TITLE(observe_deuteros_amiga_title_post_command_continuation_return,(const DeuterosAmigaObservedLocalCallReturn o),(o))
EON_HOST_DEUTEROS_TITLE(observe_deuteros_amiga_title_post_command_pointer_chain,(const DeuterosAmigaObservedTitlePostCommandPointerChain o),(o))
EON_HOST_DEUTEROS_TITLE(observe_deuteros_amiga_title_post_command_dispatch_destination,(const DeuterosAmigaObservedTitlePostCommandDispatchDestination o),(o))
EON_HOST_DEUTEROS_TITLE(advance_deuteros_amiga_title_post_command_selected_stream,(),())
EON_HOST_DEUTEROS_TITLE(observe_deuteros_amiga_title_post_command_descriptor_call_return,(const DeuterosAmigaObservedLocalCallReturn o),(o))
EON_HOST_DEUTEROS_TITLE(advance_deuteros_amiga_title_post_command_descriptor_loop,(),())
EON_HOST_DEUTEROS_TITLE(observe_deuteros_amiga_title_post_command_descriptor_byte,(const DeuterosAmigaObservedTitlePostCommandDescriptorByte o),(o))
EON_HOST_DEUTEROS_TITLE(observe_deuteros_amiga_title_post_command_adjusted_dispatch_destination,(const DeuterosAmigaObservedTitlePostCommandAdjustedDispatchDestination o),(o))
EON_HOST_DEUTEROS_TITLE(observe_deuteros_amiga_title_post_adjusted_caller_pointer,(const DeuterosAmigaObservedTitlePostAdjustedCallerPointer o),(o))
EON_HOST_DEUTEROS_TITLE(observe_deuteros_amiga_title_post_adjusted_object_gate,(const DeuterosAmigaObservedTitlePostAdjustedObjectGate o),(o))
EON_HOST_DEUTEROS_TITLE(observe_deuteros_amiga_title_post_adjusted_first_helper_return,(const DeuterosAmigaObservedLocalCallReturn o),(o))
EON_HOST_DEUTEROS_TITLE(observe_deuteros_amiga_title_post_adjusted_second_helper_return,(const DeuterosAmigaObservedLocalCallReturn o),(o))
EON_HOST_DEUTEROS_TITLE(observe_deuteros_amiga_title_post_adjusted_rts_frame,(const DeuterosAmigaObservedTitlePostAdjustedRtsFrame o),(o))
EON_HOST_DEUTEROS_TITLE(observe_deuteros_amiga_title_post_adjusted_repeated_nested_words,(const DeuterosAmigaObservedTitlePostCommandNestedWords o),(o))
EON_HOST_DEUTEROS_TITLE(observe_deuteros_amiga_title_post_adjusted_repeated_nested_call_return,(const DeuterosAmigaObservedLocalCallReturn o),(o))
EON_HOST_DEUTEROS_TITLE(advance_deuteros_amiga_title_post_adjusted_repeated_nested_loop,(),())
EON_HOST_DEUTEROS_TITLE(advance_deuteros_amiga_title_post_adjusted_caller_indirect,(),())
EON_HOST_DEUTEROS_TITLE(observe_deuteros_amiga_title_post_adjusted_caller_indirect_return,(const DeuterosAmigaObservedTitlePostAdjustedIndirectReturn o),(o))
EON_HOST_DEUTEROS_TITLE(observe_deuteros_amiga_title_post_adjusted_caller_37180_return,(const DeuterosAmigaObservedTitlePostAdjusted37180Return o),(o))
EON_HOST_DEUTEROS_TITLE(observe_deuteros_amiga_title_post_adjusted_mode_return,(const DeuterosAmigaObservedLocalCallReturn o),(o))
EON_HOST_DEUTEROS_TITLE(observe_deuteros_amiga_title_post_adjusted_222c0_return,(const DeuterosAmigaObservedLocalCallReturn o),(o))
EON_HOST_DEUTEROS_TITLE(observe_deuteros_amiga_title_post_adjusted_timer_state,(const DeuterosAmigaObservedTitlePostAdjustedTimerState o),(o))
EON_HOST_DEUTEROS_TITLE(observe_deuteros_amiga_title_post_adjusted_4069a_return,(const DeuterosAmigaObservedLocalCallReturn o),(o))
EON_HOST_DEUTEROS_TITLE(observe_deuteros_amiga_title_post_adjusted_join_byte,(const DeuterosAmigaObservedTitlePostAdjustedJoinByte o),(o))
#undef EON_HOST_DEUTEROS_TITLE

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
MillenniumAmigaBootstrapRelocatorObservationResult RuntimeHost::observe_millennium_amiga_bootstrap_relocator_overread(const MillenniumAmigaBootstrapRelocatorObservation o){if(revoking())return{false,"Relocator observation rejected during source revocation"};return NativeSessionController::observe_millennium_amiga_bootstrap_relocator_overread(o);}
MillenniumAmigaBootstrapRelocatorObservationResult RuntimeHost::observe_millennium_amiga_bootstrap_relocator_terminal_jump(const MillenniumAmigaBootstrapRelocatorObservation o){if(revoking())return{false,"Relocator observation rejected during source revocation"};return NativeSessionController::observe_millennium_amiga_bootstrap_relocator_terminal_jump(o);}
std::optional<MillenniumAmigaBootstrapRelocatorCheckpoint> RuntimeHost::millennium_amiga_bootstrap_relocator_checkpoint()const{if(revoking())return std::nullopt;return NativeSessionController::millennium_amiga_bootstrap_relocator_checkpoint();}

std::optional<MillenniumAtariBootstrapPresentationSnapshot>
RuntimeHost::millennium_atari_bootstrap_presentation() const {
    if (revoking()) return std::nullopt;
    return NativeSessionController::millennium_atari_bootstrap_presentation();
}
MillenniumAtariConfigConsumerResult RuntimeHost::observe_millennium_atari_status_register(
    const MillenniumAtariStatusRegisterObservation observation) {
    if (revoking()) return {false, "Atari SR observation rejected during source revocation"};
    return NativeSessionController::observe_millennium_atari_status_register(observation);
}
MillenniumAtariConfigConsumerResult RuntimeHost::observe_millennium_atari_xbios_selector_two(
    const MillenniumAtariXbiosSelectorTwoObservation observation) {
    if (revoking()) return {false, "XBIOS selector-2 result rejected during source revocation"};
    return NativeSessionController::observe_millennium_atari_xbios_selector_two(observation);
}
MillenniumAtariConfigConsumerResult RuntimeHost::observe_millennium_atari_xbios_selector_three(
    const MillenniumAtariXbiosSelectorThreeObservation observation) {
    if (revoking()) return {false, "XBIOS selector-3 result rejected during source revocation"};
    return NativeSessionController::observe_millennium_atari_xbios_selector_three(observation);
}
MillenniumAtariConfigConsumerResult RuntimeHost::observe_millennium_atari_xbios_selector_four(
    const MillenniumAtariXbiosSelectorFourObservation observation) {
    if (revoking()) return {false, "XBIOS selector-4 result rejected during source revocation"};
    return NativeSessionController::observe_millennium_atari_xbios_selector_four(observation);
}
MillenniumAtariConfigConsumerResult RuntimeHost::observe_millennium_atari_line_a(
    const MillenniumAtariLineAObservation observation) {
    if (revoking()) return {false, "Line-A result rejected during source revocation"};
    return NativeSessionController::observe_millennium_atari_line_a(observation);
}

MillenniumAtariConfigConsumerResult RuntimeHost::observe_millennium_atari_xbios_selector_21(
    const MillenniumAtariXbiosSelector21Observation observation) {
    if (revoking()) return {false, "XBIOS selector-21 result rejected during source revocation"};
    return NativeSessionController::observe_millennium_atari_xbios_selector_21(observation);
}

MillenniumAtariConfigConsumerResult RuntimeHost::observe_millennium_atari_xbios_selector_6(
    const MillenniumAtariXbiosSelector6Observation observation) {
    if (revoking()) return {false, "XBIOS selector-6 result rejected during source revocation"};
    return NativeSessionController::observe_millennium_atari_xbios_selector_6(observation);
}

MillenniumAtariConfigConsumerResult RuntimeHost::observe_millennium_atari_bchg_2b55a(
    const MillenniumAtariBchgObservation observation) {
    if (revoking()) return {false, "BCHG rejected during source revocation"};
    return NativeSessionController::observe_millennium_atari_bchg_2b55a(observation);
}

MillenniumAtariConfigConsumerResult RuntimeHost::execute_millennium_atari_jsr_2b55a() {
    if (revoking()) return {false, "JSR $2b55a rejected during source revocation"};
    return NativeSessionController::execute_millennium_atari_jsr_2b55a();
}

MillenniumAtariConfigConsumerResult RuntimeHost::execute_millennium_atari_bsr_2b59a() {
    if (revoking()) return {false, "BSR $2b59a rejected during source revocation"};
    return NativeSessionController::execute_millennium_atari_bsr_2b59a();
}

MillenniumAtariConfigConsumerResult RuntimeHost::observe_millennium_atari_d0_indexed_byte(
    const MillenniumAtariD0IndexedByteObservation observation) {
    if (revoking()) return {false, "D0-indexed byte rejected during source revocation"};
    return NativeSessionController::observe_millennium_atari_d0_indexed_byte(observation);
}

MillenniumAtariConfigConsumerResult RuntimeHost::execute_millennium_atari_a1_setup(){if(revoking())return{false,"A1 setup rejected during source revocation"};return NativeSessionController::execute_millennium_atari_a1_setup();}

MillenniumAtariConfigConsumerResult RuntimeHost::observe_millennium_atari_d0_indexed_word(const MillenniumAtariD0IndexedWordObservation o){if(revoking())return{false,"Indexed word rejected during revocation"};return NativeSessionController::observe_millennium_atari_d0_indexed_word(o);}

MillenniumAtariConfigConsumerResult RuntimeHost::observe_millennium_atari_a0_indexed_word(const MillenniumAtariA0IndexedWordObservation o){if(revoking())return{false,"A0-indexed word rejected during revocation"};return NativeSessionController::observe_millennium_atari_a0_indexed_word(o);}

MillenniumAtariConfigConsumerResult RuntimeHost::execute_millennium_atari_loop_iteration_setup(){if(revoking())return{false,"Loop setup rejected during revocation"};return NativeSessionController::execute_millennium_atari_loop_iteration_setup();}

MillenniumAtariConfigConsumerResult RuntimeHost::execute_millennium_atari_loop_epilogue(){if(revoking())return{false,"Loop epilogue rejected during revocation"};return NativeSessionController::execute_millennium_atari_loop_epilogue();}

MillenniumAtariConfigConsumerResult RuntimeHost::observe_millennium_atari_movem_frame(const MillenniumAtariMovemFrameObservation o){if(revoking())return{false,"MOVEM frame rejected during revocation"};return NativeSessionController::observe_millennium_atari_movem_frame(o);}

MillenniumAtariConfigConsumerResult RuntimeHost::execute_millennium_atari_jsr_2aa68(){if(revoking())return{false,"JSR $2aa68 rejected during revocation"};return NativeSessionController::execute_millennium_atari_jsr_2aa68();}

MillenniumAtariConfigConsumerResult RuntimeHost::observe_millennium_atari_xbios_selector_38(const MillenniumAtariXbiosSelector38Observation o){if(revoking())return{false,"Selector 38 rejected during revocation"};return NativeSessionController::observe_millennium_atari_xbios_selector_38(o);}

MillenniumAtariConfigConsumerResult RuntimeHost::execute_millennium_atari_jsr_2aa0c(){if(revoking())return{false,"JSR $2aa0c rejected during revocation"};return NativeSessionController::execute_millennium_atari_jsr_2aa0c();}
MillenniumAtariConfigConsumerResult RuntimeHost::observe_millennium_atari_gemdos_selector_61(const MillenniumAtariGemdosSelector61Observation o){if(revoking())return{false,"GEMDOS selector 61 rejected during revocation"};return NativeSessionController::observe_millennium_atari_gemdos_selector_61(o);}
MillenniumAtariConfigConsumerResult RuntimeHost::execute_millennium_atari_jsr_2a5c2(){if(revoking())return{false,"JSR $2a5c2 rejected during revocation"};return NativeSessionController::execute_millennium_atari_jsr_2a5c2();}
MillenniumAtariConfigConsumerResult RuntimeHost::observe_millennium_atari_gemdos_selector_63(const MillenniumAtariGemdosSelector63Observation o){if(revoking())return{false,"GEMDOS selector 63 rejected during revocation"};return NativeSessionController::observe_millennium_atari_gemdos_selector_63(o);}
MillenniumAtariConfigConsumerResult RuntimeHost::observe_millennium_atari_gemdos_selector_62(const MillenniumAtariGemdosSelector62Observation o){if(revoking())return{false,"GEMDOS selector 62 rejected during revocation"};return NativeSessionController::observe_millennium_atari_gemdos_selector_62(o);}
MillenniumAtariConfigConsumerResult RuntimeHost::observe_millennium_atari_fread_prefix(const MillenniumAtariFreadPrefixObservation o){if(revoking())return{false,"Fread prefix rejected during revocation"};return NativeSessionController::observe_millennium_atari_fread_prefix(o);}
MillenniumAtariConfigConsumerResult RuntimeHost::execute_millennium_atari_jsr_2b2be(){if(revoking())return{false,"JSR $2b2be rejected during revocation"};return NativeSessionController::execute_millennium_atari_jsr_2b2be();}
MillenniumAtariConfigConsumerResult RuntimeHost::observe_millennium_atari_game_init_source_byte(const MillenniumAtariGameInitSourceByteObservation o){if(revoking())return{false,"Game-init source byte rejected during revocation"};return NativeSessionController::observe_millennium_atari_game_init_source_byte(o);}
MillenniumAtariConfigConsumerResult RuntimeHost::observe_millennium_atari_game_init_zero_pair(const MillenniumAtariGameInitZeroPairObservation o){if(revoking())return{false,"Game-init zero pair rejected during revocation"};return NativeSessionController::observe_millennium_atari_game_init_zero_pair(o);}
MillenniumAtariConfigConsumerResult RuntimeHost::execute_millennium_atari_game_init_zero_counter_branch(){if(revoking())return{false,"Game-init zero counter branch rejected during revocation"};return NativeSessionController::execute_millennium_atari_game_init_zero_counter_branch();}
MillenniumAtariConfigConsumerResult RuntimeHost::observe_millennium_atari_game_init_replicated_byte(const MillenniumAtariGameInitReplicatedByteObservation o){if(revoking())return{false,"Replicated-byte run rejected during revocation"};return NativeSessionController::observe_millennium_atari_game_init_replicated_byte(o);}
MillenniumAtariConfigConsumerResult RuntimeHost::observe_millennium_atari_game_init_swapped_pair(const MillenniumAtariGameInitSwappedPairObservation o){if(revoking())return{false,"Swapped-pair run rejected during revocation"};return NativeSessionController::observe_millennium_atari_game_init_swapped_pair(o);}
MillenniumAtariConfigConsumerResult RuntimeHost::observe_millennium_atari_game_init_extended_run(const MillenniumAtariGameInitExtendedRunObservation o){if(revoking())return{false,"Extended run rejected during revocation"};return NativeSessionController::observe_millennium_atari_game_init_extended_run(o);}
MillenniumAtariConfigConsumerResult RuntimeHost::execute_millennium_atari_game_init_return(){if(revoking())return{false,"Game-init return rejected during revocation"};return NativeSessionController::execute_millennium_atari_game_init_return();}
MillenniumAtariConfigConsumerResult RuntimeHost::execute_millennium_atari_game_init_palette_copy_prefix(){if(revoking())return{false,"Palette-copy prefix rejected during revocation"};return NativeSessionController::execute_millennium_atari_game_init_palette_copy_prefix();}
MillenniumAtariConfigConsumerResult RuntimeHost::observe_millennium_atari_game_init_palette_words(const MillenniumAtariGameInitPaletteWordsObservation o){if(revoking())return{false,"Palette arithmetic rejected during revocation"};return NativeSessionController::observe_millennium_atari_game_init_palette_words(o);}
MillenniumAtariConfigConsumerResult RuntimeHost::observe_millennium_atari_game_init_palette_xbios_selector_6(const MillenniumAtariGameInitPaletteXbios6Observation o){if(revoking())return{false,"Palette XBIOS selector-6 result rejected during revocation"};return NativeSessionController::observe_millennium_atari_game_init_palette_xbios_selector_6(o);}
MillenniumAtariConfigConsumerResult RuntimeHost::observe_millennium_atari_game_init_palette_recurrence(const MillenniumAtariGameInitPaletteRecurrenceObservation o){if(revoking())return{false,"Recurrent palette pass rejected during revocation"};return NativeSessionController::observe_millennium_atari_game_init_palette_recurrence(o);}
MillenniumAtariConfigConsumerResult RuntimeHost::observe_millennium_atari_game_init_palette_rts(const MillenniumAtariGameInitPaletteRtsObservation o){if(revoking())return{false,"Palette RTS rejected during revocation"};return NativeSessionController::observe_millennium_atari_game_init_palette_rts(o);}
MillenniumAtariConfigConsumerResult RuntimeHost::observe_millennium_atari_game_init_second_config_fopen(const MillenniumAtariGemdosSelector61Observation o){if(revoking())return{false,"Second config Fopen rejected during revocation"};return NativeSessionController::observe_millennium_atari_game_init_second_config_fopen(o);}
MillenniumAtariConfigConsumerResult RuntimeHost::observe_millennium_atari_game_init_second_config_fread(const MillenniumAtariGemdosSelector63Observation o){if(revoking())return{false,"Second config Fread rejected during revocation"};return NativeSessionController::observe_millennium_atari_game_init_second_config_fread(o);}
MillenniumAtariConfigConsumerResult RuntimeHost::observe_millennium_atari_game_init_second_config_fclose(const MillenniumAtariGemdosSelector62Observation o){if(revoking())return{false,"Second config Fclose rejected during revocation"};return NativeSessionController::observe_millennium_atari_game_init_second_config_fclose(o);}
MillenniumAtariConfigConsumerResult RuntimeHost::observe_millennium_atari_game_init_second_config_rts(const MillenniumAtariGameInitSecondConfigRtsObservation o){if(revoking())return{false,"Second config RTS rejected during revocation"};return NativeSessionController::observe_millennium_atari_game_init_second_config_rts(o);}
MillenniumAtariConfigConsumerResult RuntimeHost::observe_millennium_atari_game_init_second_config_xbios_38(const MillenniumAtariXbiosSelector38Observation o){if(revoking())return{false,"Second config XBIOS selector-38 rejected during revocation"};return NativeSessionController::observe_millennium_atari_game_init_second_config_xbios_38(o);}
MillenniumAtariConfigConsumerResult RuntimeHost::observe_millennium_atari_game_init_config_final_rts(const MillenniumAtariGameInitSecondConfigRtsObservation o){if(revoking())return{false,"Final config RTS rejected during revocation"};return NativeSessionController::observe_millennium_atari_game_init_config_final_rts(o);}
MillenniumAtariConfigConsumerResult RuntimeHost::observe_millennium_atari_game_init_post_config_fopen(const MillenniumAtariGemdosSelector61Observation o){if(revoking())return{false,"Post-config Fopen rejected during revocation"};return NativeSessionController::observe_millennium_atari_game_init_post_config_fopen(o);}

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
