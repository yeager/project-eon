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
