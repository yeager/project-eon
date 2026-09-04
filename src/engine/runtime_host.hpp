#pragma once

#include "engine/native_session_controller.hpp"
#include "engine/native_code_image_diagnostics.hpp"

#include <cstdint>

namespace eon {

struct RuntimeHostAdvance {
    DeuterosAmigaOpeningAdvance opening;
    bool opening_started = false;
    bool opening_active = false;
};

struct RuntimeHostPresentationSnapshot {
    RuntimePresentationKind kind = RuntimePresentationKind::millennium_dos_title;
    RuntimeSessionBoundary boundary = RuntimeSessionBoundary::bootstrap_boundary;
    RuntimeSessionCapabilities capabilities;
    RuntimeInputContract input_contract = RuntimeInputContract::none;
    constexpr bool operator==(const RuntimeHostPresentationSnapshot&) const = default;
};

// A copy-only UI/CLI boundary. In particular, it cannot retain a release
// coordinator, original-media span, adapter pointer or launch DTO reference.
struct RuntimeHostSnapshot {
    std::uint64_t generation = 0;
    bool revoking = false;
    bool input_suppressed = false;
    ReleaseRuntimeAdmission admission = ReleaseRuntimeAdmission::unselected;
    ReleaseRuntimeRejection rejection = ReleaseRuntimeRejection::none;
    NativeSessionState state = NativeSessionState::menu;
    std::optional<RuntimeSessionSnapshot> session;
    std::optional<RuntimeHostPresentationSnapshot> presentation;
};

// SDL owns windows, textures, queued device audio and text-input activation.
// RuntimeHost owns only the corresponding native lifecycle ordering.  It
// gives every platform front end one explicit revocation interval in which it
// must discard source-derived host objects before the native coordinator can
// release the read-only media adapters.  This is not a game-state abstraction
// and does not broaden the recovered input contract.
class RuntimeHost : private NativeSessionController {
public:
    // This is the intentionally narrow native-engine surface used by SDL and
    // the CLI. Private inheritance prevents a front end from bypassing the
    // host's modal gate, scheduler ownership or revocation interval through
    // an implicit NativeSessionController conversion.
    [[nodiscard]] RuntimeCandidateLaunchResult launch_direct(const LaunchRequest& candidate,
        const std::vector<ReleaseArchive>& releases);
    [[nodiscard]] RuntimeCandidateLaunchResult launch_menu(const LauncherSessionState& session,
        const LaunchRequest& base, const std::vector<ReleaseArchive>& releases);
    [[nodiscard]] NativeSessionState state() const;
    [[nodiscard]] bool is_menu() const;
    [[nodiscard]] bool requires_revocation_for(const LauncherSourceIdentity& source) const;
    // Do not publish a reference into the release coordinator. A copy makes
    // the active identity safe for diagnostics and impossible to retain
    // through a source-revocation interval.
    [[nodiscard]] std::optional<ResolvedLaunchRequest> active() const;
    [[nodiscard]] ReleaseRuntimeAdmission admission() const;
    [[nodiscard]] ReleaseRuntimeRejection rejection() const;
    [[nodiscard]] std::optional<RuntimeSessionSnapshot> session_snapshot() const;
    [[nodiscard]] NativeCodeImageRegistryDiagnostics native_code_image_registry_diagnostics() const;

    [[nodiscard]] std::optional<MillenniumDosPresentationSnapshot>
    millennium_dos_presentation() const;
    [[nodiscard]] std::optional<MillenniumDosStartupInputSnapshot>
    millennium_dos_startup_input() const;
    [[nodiscard]] MillenniumDosSoundDriverLoadObservationResult observe_millennium_dos_sound_driver_load(MillenniumDosSoundDriverLoadObservation);
    [[nodiscard]] std::optional<MillenniumDosSoundDriverLoadCheckpoint> millennium_dos_sound_driver_load_checkpoint() const;
    [[nodiscard]] std::optional<MillenniumDosCompatibilityRunnerCheckpoint> tick_millennium_dos_compatibility_runner();
    [[nodiscard]] MillenniumDosTitleExecEntryObservationResult observe_millennium_dos_title_child_process_entry(MillenniumDosTitleExecProcessEntry);
    [[nodiscard]] MillenniumDosTitleExecEntryObservationResult advance_millennium_dos_title_entry_prefix(MillenniumDosTitleExecPrefixObservation);
    [[nodiscard]] std::optional<MillenniumDosTitleExecEntryRuntimeCheckpoint> millennium_dos_title_exec_entry_checkpoint() const;
    [[nodiscard]] MillenniumDosTitleInitializationObservationResult observe_millennium_dos_title_private_interrupt_result(MillenniumDosTitlePrivateInterruptResultObservation);
    [[nodiscard]] MillenniumDosTitleInitializationObservationResult observe_millennium_dos_title_selected_callee_result(MillenniumDosTitleSelectedCalleeResultObservation);
    [[nodiscard]] MillenniumDosTitleInitializationObservationResult observe_millennium_dos_title_bios_result(MillenniumDosTitleBiosResultObservation);
    [[nodiscard]] MillenniumDosTitleInitializationObservationResult observe_millennium_dos_title_dos_memory_result(MillenniumDosTitleDosResultObservation);
    [[nodiscard]] MillenniumDosTitleInitializationObservationResult observe_millennium_dos_title_dos_file_result(MillenniumDosTitleDosFileResultObservation);
    [[nodiscard]] MillenniumDosTitleInitializationObservationResult observe_millennium_dos_title_dos_vector_result(MillenniumDosTitleDosVectorResultObservation);
    [[nodiscard]] MillenniumDosTitleInitializationObservationResult observe_millennium_dos_title_setup_bios_result(MillenniumDosTitleSetupBiosResultObservation);
    [[nodiscard]] MillenniumDosTitleInitializationObservationResult observe_millennium_dos_title_far_words(MillenniumDosTitleFarWordsObservation);
    [[nodiscard]] MillenniumDosTitleInitializationObservationResult observe_millennium_dos_title_far_word(MillenniumDosTitleFarWordObservation);
    [[nodiscard]] MillenniumDosTitleInitializationObservationResult observe_millennium_dos_title_far_byte(MillenniumDosTitleFarByteObservation);
    [[nodiscard]] MillenniumDosTitleToGameObservationResult observe_millennium_dos_title_to_game_call_return(MillenniumDosTitleToGameCallReturnObservation);
    [[nodiscard]] MillenniumDosTitleToGameObservationResult observe_millennium_dos_title_to_game_stack_word(MillenniumDosTitleToGameStackWordObservation);
    [[nodiscard]] MillenniumDosTitleToGameObservationResult observe_millennium_dos_title_to_game_title_termination(MillenniumDosTitleToGameInterruptObservation);
    [[nodiscard]] MillenniumDosTitleToGameObservationResult observe_millennium_dos_title_to_game_parent_exec_return(MillenniumDosTitleToGameInterruptObservation);
    [[nodiscard]] MillenniumDosTitleToGameObservationResult observe_millennium_dos_title_to_game_child_status(MillenniumDosTitleToGameInterruptObservation);
    [[nodiscard]] std::optional<MillenniumDosTitleToGameCheckpoint> millennium_dos_title_to_game_checkpoint() const;
    [[nodiscard]] std::optional<MillenniumDosStaticDispatchDiagnostics>
    millennium_dos_static_dispatch_diagnostics() const;
    [[nodiscard]] std::optional<MillenniumDosNativeProcessCheckpoint>
    millennium_dos_native_process_checkpoint() const;
    [[nodiscard]] MillenniumDosGxActiveTraceAdmission
    admit_active_millennium_dos_gx_startup_reference_trace(const ReferenceTrace& trace);
    [[nodiscard]] std::optional<MillenniumDosGxStartupCheckpoint>
    millennium_dos_gx_startup_checkpoint() const;
    [[nodiscard]] MillenniumDosPostOverlayObservationResult
    observe_millennium_dos_post_overlay_private_interrupt_return(
        MillenniumDosPostOverlayPrivateInterruptReturnObservation observation);
    [[nodiscard]] MillenniumDosPostOverlayObservationResult
    observe_millennium_dos_post_overlay_call_return(
        MillenniumDosPostOverlayCallReturnObservation observation);
    [[nodiscard]] MillenniumDosPostOverlayObservationResult
    observe_millennium_dos_post_overlay_al(MillenniumDosPostOverlayAlObservation observation);
    [[nodiscard]] MillenniumDosPostOverlayObservationResult
    observe_millennium_dos_post_overlay_runtime_byte(
        MillenniumDosPostOverlayRuntimeByteObservation observation);
    [[nodiscard]] std::optional<MillenniumDosPostOverlayLoopCheckpoint>
    millennium_dos_post_overlay_loop_checkpoint() const;
    [[nodiscard]] MillenniumDosPostOverlayObservationResult complete_millennium_dos_handler(MillenniumDosHandlerCompletionObservation);
    [[nodiscard]] std::optional<MillenniumDosHandlerCompletionCheckpoint> millennium_dos_handler_completion_checkpoint() const;
    [[nodiscard]] MillenniumDosTenthFunctionObservationResult observe_millennium_dos_tenth_function_dispatch(MillenniumDosTenthFunctionDispatchObservation observation);
    [[nodiscard]] MillenniumDosTenthFunctionObservationResult observe_millennium_dos_tenth_function_word(MillenniumDosTenthFunctionWordObservation observation);
    [[nodiscard]] MillenniumDosTenthFunctionObservationResult observe_millennium_dos_tenth_function_byte(MillenniumDosTenthFunctionByteObservation observation);
    [[nodiscard]] MillenniumDosTenthFunctionObservationResult observe_millennium_dos_tenth_function_call_return(MillenniumDosTenthFunctionCallReturnObservation observation);
    [[nodiscard]] MillenniumDosTenthFunctionObservationResult observe_millennium_dos_tenth_function_zero_flag(MillenniumDosTenthFunctionZeroFlagObservation observation);
    [[nodiscard]] MillenniumDosTenthFunctionObservationResult observe_millennium_dos_tenth_function_bl(MillenniumDosTenthFunctionBlObservation observation);
    [[nodiscard]] std::optional<MillenniumDosTenthFunctionCheckpoint> millennium_dos_tenth_function_checkpoint() const;
    [[nodiscard]] MillenniumDosSeventhFunctionObservationResult observe_millennium_dos_seventh_function_dispatch(MillenniumDosSeventhFunctionDispatchObservation observation);
    [[nodiscard]] MillenniumDosSeventhFunctionObservationResult observe_millennium_dos_seventh_function_word(MillenniumDosSeventhFunctionWordObservation observation);
    [[nodiscard]] MillenniumDosSeventhFunctionObservationResult observe_millennium_dos_seventh_function_byte(MillenniumDosSeventhFunctionByteObservation observation);
    [[nodiscard]] MillenniumDosSeventhFunctionObservationResult observe_millennium_dos_seventh_function_call_return(MillenniumDosSeventhFunctionCallReturnObservation observation);
    [[nodiscard]] MillenniumDosSeventhFunctionObservationResult observe_millennium_dos_seventh_function_returned_bx(MillenniumDosSeventhFunctionReturnedBxObservation observation);
    [[nodiscard]] std::optional<MillenniumDosSeventhFunctionCheckpoint> millennium_dos_seventh_function_checkpoint() const;
    [[nodiscard]] MillenniumDosSharedHelperObservationResult observe_millennium_dos_shared_helper_entry(MillenniumDosSharedHelperEntryObservation);[[nodiscard]] MillenniumDosSharedHelperObservationResult observe_millennium_dos_shared_helper_word(MillenniumDosSharedHelperWordObservation);[[nodiscard]] MillenniumDosSharedHelperObservationResult observe_millennium_dos_shared_helper_far_word(MillenniumDosSharedHelperFarWordObservation);[[nodiscard]] MillenniumDosSharedHelperObservationResult observe_millennium_dos_shared_helper_call_return(MillenniumDosSharedHelperCallReturnObservation);[[nodiscard]] MillenniumDosSharedHelperObservationResult observe_millennium_dos_shared_helper_external_return(MillenniumDosSharedHelperExternalReturnObservation);[[nodiscard]] std::optional<MillenniumDosSharedHelperCheckpoint> millennium_dos_shared_helper_checkpoint()const;[[nodiscard]] MillenniumDosSharedHelperObservationResult observe_millennium_dos_special_action_helper_entry(MillenniumDosSpecialActionHelperEntryObservation);[[nodiscard]] MillenniumDosSharedHelperObservationResult observe_millennium_dos_special_action_external_return(MillenniumDosSharedHelperExternalReturnObservation);
    [[nodiscard]] MillenniumDosGxAdapterObservationResult observe_millennium_dos_second_special_action_adapter_entry(MillenniumDosGxAdapterEntryObservation);[[nodiscard]] MillenniumDosGxAdapterObservationResult observe_millennium_dos_gx_adapter_segment(MillenniumDosGxAdapterWordObservation);[[nodiscard]] MillenniumDosGxAdapterObservationResult observe_millennium_dos_gx_adapter_transfer(MillenniumDosGxAdapterTransferObservation);[[nodiscard]] MillenniumDosGxAdapterObservationResult observe_millennium_dos_gx_adapter_overlay_return(MillenniumDosGxAdapterReturnObservation);[[nodiscard]] MillenniumDosGxAdapterObservationResult observe_millennium_dos_gx_adapter_return(MillenniumDosGxAdapterReturnObservation);[[nodiscard]] MillenniumDosGxAdapterObservationResult observe_millennium_dos_second_special_action_return(MillenniumDosGxAdapterReturnObservation);[[nodiscard]] std::optional<MillenniumDosGxAdapterCheckpoint> millennium_dos_gx_adapter_checkpoint()const;
    [[nodiscard]] MillenniumDosSixthFunctionObservationResult observe_millennium_dos_sixth_function_dispatch(MillenniumDosSixthFunctionDispatchObservation observation);
    [[nodiscard]] MillenniumDosSixthFunctionObservationResult observe_millennium_dos_sixth_function_word(MillenniumDosSixthFunctionWordObservation observation);
    [[nodiscard]] MillenniumDosSixthFunctionObservationResult observe_millennium_dos_sixth_function_byte(MillenniumDosSixthFunctionByteObservation observation);
    [[nodiscard]] MillenniumDosSixthFunctionObservationResult observe_millennium_dos_sixth_function_call_return(MillenniumDosSixthFunctionCallReturnObservation observation);
    [[nodiscard]] MillenniumDosSixthFunctionObservationResult observe_millennium_dos_sixth_function_bl(MillenniumDosSixthFunctionBlObservation observation);
    [[nodiscard]] std::optional<MillenniumDosSixthFunctionCheckpoint> millennium_dos_sixth_function_checkpoint() const;
    [[nodiscard]] MillenniumDosEighthFunctionObservationResult observe_millennium_dos_eighth_function_dispatch(MillenniumDosEighthFunctionDispatchObservation observation);
    [[nodiscard]] MillenniumDosEighthFunctionObservationResult observe_millennium_dos_eighth_function_call_return(MillenniumDosEighthFunctionCallReturnObservation observation);
    [[nodiscard]] MillenniumDosEighthFunctionObservationResult observe_millennium_dos_eighth_function_bl(MillenniumDosEighthFunctionBlObservation observation);
    [[nodiscard]] std::optional<MillenniumDosEighthFunctionCheckpoint> millennium_dos_eighth_function_checkpoint() const;
    [[nodiscard]] MillenniumDosNinthFunctionObservationResult observe_millennium_dos_ninth_function_dispatch(MillenniumDosNinthFunctionDispatchObservation);
    [[nodiscard]] MillenniumDosNinthFunctionObservationResult observe_millennium_dos_ninth_function_word(MillenniumDosNinthFunctionWordObservation);
    [[nodiscard]] MillenniumDosNinthFunctionObservationResult observe_millennium_dos_ninth_function_byte(MillenniumDosNinthFunctionByteObservation);
    [[nodiscard]] MillenniumDosNinthFunctionObservationResult observe_millennium_dos_ninth_function_call_return(MillenniumDosNinthFunctionCallReturnObservation);
    [[nodiscard]] std::optional<MillenniumDosNinthFunctionCheckpoint> millennium_dos_ninth_function_checkpoint() const;
    [[nodiscard]] MillenniumDosNinthHandoffObservationResult observe_millennium_dos_ninth_handoff_entry(MillenniumDosNinthHandoffEntryObservation);[[nodiscard]] MillenniumDosNinthHandoffObservationResult observe_millennium_dos_ninth_handoff_byte(MillenniumDosNinthHandoffByteObservation);[[nodiscard]] MillenniumDosNinthHandoffObservationResult observe_millennium_dos_ninth_handoff_word(MillenniumDosNinthHandoffWordObservation);[[nodiscard]] MillenniumDosNinthHandoffObservationResult observe_millennium_dos_ninth_handoff_call_return(MillenniumDosNinthHandoffCallReturnObservation);[[nodiscard]] MillenniumDosNinthHandoffObservationResult observe_millennium_dos_ninth_handoff_zero_flag(MillenniumDosNinthHandoffZeroFlagObservation);[[nodiscard]] MillenniumDosNinthHandoffObservationResult observe_millennium_dos_ninth_handoff_bl(MillenniumDosNinthHandoffBlObservation);[[nodiscard]] std::optional<MillenniumDosNinthHandoffCheckpoint> millennium_dos_ninth_handoff_checkpoint()const;
    [[nodiscard]] MillenniumDosFourthFunctionObservationResult observe_millennium_dos_fourth_function_dispatch(MillenniumDosFourthFunctionDispatchObservation);
    [[nodiscard]] MillenniumDosFourthFunctionObservationResult observe_millennium_dos_fourth_function_word(MillenniumDosFourthFunctionWordObservation);
    [[nodiscard]] MillenniumDosFourthFunctionObservationResult observe_millennium_dos_fourth_function_call_return(MillenniumDosFourthFunctionCallReturnObservation);
    [[nodiscard]] std::optional<MillenniumDosFourthFunctionCheckpoint> millennium_dos_fourth_function_checkpoint() const;
    [[nodiscard]] MillenniumDosFifthFunctionObservationResult observe_millennium_dos_fifth_function_dispatch(MillenniumDosFifthFunctionDispatchObservation); [[nodiscard]] MillenniumDosFifthFunctionObservationResult observe_millennium_dos_fifth_function_call_return(MillenniumDosFifthFunctionCallReturnObservation); [[nodiscard]] std::optional<MillenniumDosFifthFunctionCheckpoint> millennium_dos_fifth_function_checkpoint()const;
    [[nodiscard]] MillenniumDosThirdFunctionObservationResult observe_millennium_dos_third_function_dispatch(MillenniumDosThirdFunctionDispatchObservation);
    [[nodiscard]] MillenniumDosThirdFunctionObservationResult observe_millennium_dos_third_function_word(MillenniumDosThirdFunctionWordObservation);
    [[nodiscard]] MillenniumDosThirdFunctionObservationResult observe_millennium_dos_third_function_call_return(MillenniumDosThirdFunctionCallReturnObservation);
    [[nodiscard]] MillenniumDosThirdFunctionObservationResult observe_millennium_dos_third_function_bl(MillenniumDosThirdFunctionBlObservation);
    [[nodiscard]] std::optional<MillenniumDosThirdFunctionCheckpoint> millennium_dos_third_function_checkpoint() const;
    [[nodiscard]] MillenniumDosFirstFunctionObservationResult observe_millennium_dos_first_function_dispatch(MillenniumDosFirstFunctionDispatchObservation);
    [[nodiscard]] MillenniumDosFirstFunctionObservationResult observe_millennium_dos_first_function_call_return(MillenniumDosFirstFunctionCallReturnObservation);
    [[nodiscard]] MillenniumDosFirstFunctionObservationResult observe_millennium_dos_first_function_bl(MillenniumDosFirstFunctionBlObservation);
    [[nodiscard]] std::optional<MillenniumDosFirstFunctionCheckpoint> millennium_dos_first_function_checkpoint() const;
    [[nodiscard]] MillenniumDosSecondFunctionObservationResult observe_millennium_dos_second_function_dispatch(MillenniumDosSecondFunctionDispatchObservation);
    [[nodiscard]] MillenniumDosSecondFunctionObservationResult observe_millennium_dos_second_function_runtime_byte(MillenniumDosSecondFunctionRuntimeByteObservation);
    [[nodiscard]] MillenniumDosSecondFunctionObservationResult observe_millennium_dos_second_function_call_return(MillenniumDosSecondFunctionCallReturnObservation);
    [[nodiscard]] MillenniumDosSecondFunctionObservationResult observe_millennium_dos_second_function_bl(MillenniumDosSecondFunctionBlObservation);
    [[nodiscard]] std::optional<MillenniumDosSecondFunctionCheckpoint> millennium_dos_second_function_checkpoint() const;
    [[nodiscard]] MillenniumDosSecondFunctionCallbackObservationResult observe_millennium_dos_second_function_callback_entry(MillenniumDosSecondFunctionCallbackEntryObservation);
    [[nodiscard]] MillenniumDosSecondFunctionCallbackObservationResult observe_millennium_dos_second_function_callback_runtime_byte(MillenniumDosSecondFunctionCallbackRuntimeByteObservation);
    [[nodiscard]] MillenniumDosSecondFunctionCallbackObservationResult observe_millennium_dos_second_function_callback_runtime_word(MillenniumDosSecondFunctionCallbackRuntimeWordObservation);
    [[nodiscard]] MillenniumDosSecondFunctionCallbackObservationResult observe_millennium_dos_second_function_callback_call_return(MillenniumDosSecondFunctionCallbackCallReturnObservation);
    [[nodiscard]] MillenniumDosSecondFunctionCallbackObservationResult observe_millennium_dos_second_function_callback_bl(MillenniumDosSecondFunctionCallbackBlObservation);
    [[nodiscard]] MillenniumDosSecondFunctionCallbackObservationResult observe_millennium_dos_second_function_callback_jump_entry(MillenniumDosSecondFunctionCallbackJumpEntryObservation);
    [[nodiscard]] MillenniumDosSecondFunctionCallbackObservationResult observe_millennium_dos_second_function_callback_external_return(MillenniumDosSecondFunctionCallbackExternalReturnObservation);
    [[nodiscard]] std::optional<MillenniumDosSecondFunctionCallbackCheckpoint> millennium_dos_second_function_callback_checkpoint() const;
    [[nodiscard]] std::optional<NativeRuntimeMemoryCheckpoint> native_runtime_memory_checkpoint() const;
    [[nodiscard]] std::optional<NativeRuntimeMemoryDiagnostics> native_runtime_memory_diagnostics() const;
    [[nodiscard]] MillenniumDosBdfObservationResult observe_millennium_dos_bdf_byte(MillenniumDosBdfByteObservation);[[nodiscard]] MillenniumDosBdfObservationResult observe_millennium_dos_bdf_word(MillenniumDosBdfWordObservation);[[nodiscard]] MillenniumDosBdfObservationResult observe_millennium_dos_bdf_far_byte(MillenniumDosBdfFarByteObservation);[[nodiscard]] MillenniumDosBdfObservationResult observe_millennium_dos_bdf_poll_return(MillenniumDosBdfPollReturnObservation);[[nodiscard]] MillenniumDosBdfObservationResult observe_millennium_dos_bdf_mapping_return(MillenniumDosBdfMappingReturnObservation);[[nodiscard]]std::optional<MillenniumDosBdfCheckpoint>millennium_dos_bdf_checkpoint()const;
    [[nodiscard]] MillenniumDosBdfObservationResult observe_millennium_dos_bdf_external_return(MillenniumDosBdfExternalReturnObservation);
    [[nodiscard]] MillenniumDosBdfObservationResult observe_millennium_dos_bdf_terminal_jump(MillenniumDosBdfTerminalJumpObservation);
    [[nodiscard]] MillenniumDosBdfObservationResult observe_millennium_dos_bdf_mode_two_byte(MillenniumDosBdfByteObservation);[[nodiscard]] MillenniumDosBdfObservationResult observe_millennium_dos_bdf_mode_two_word(MillenniumDosBdfWordObservation);
    [[nodiscard]] MillenniumDosBdfObservationResult observe_millennium_dos_bdf_mode_two_far_word(MillenniumDosBdfModeTwoFarWordObservation);
    [[nodiscard]] MillenniumDosBdfObservationResult observe_millennium_dos_bdf_mode_two_far_byte(MillenniumDosBdfModeTwoFarByteObservation);
    [[nodiscard]] MillenniumDosBdfObservationResult observe_millennium_dos_bdf_mode_two_external_return(MillenniumDosBdfExternalReturnObservation);
    [[nodiscard]] MillenniumDosBdfObservationResult observe_millennium_dos_bdf_other_mode_byte(MillenniumDosBdfByteObservation);[[nodiscard]] MillenniumDosBdfObservationResult observe_millennium_dos_bdf_other_mode_word(MillenniumDosBdfWordObservation);[[nodiscard]] MillenniumDosBdfObservationResult observe_millennium_dos_bdf_other_mode_external_return(MillenniumDosBdfExternalReturnObservation);[[nodiscard]] MillenniumDosBdfObservationResult observe_millennium_dos_bdf_other_mode_far_word(MillenniumDosBdfModeTwoFarWordObservation);[[nodiscard]] MillenniumDosBdfObservationResult observe_millennium_dos_bdf_other_mode_far_byte(MillenniumDosBdfModeTwoFarByteObservation);
    [[nodiscard]] std::optional<MillenniumDosOwnedFunctionDiagnostics> millennium_dos_owned_function_diagnostics() const;
    [[nodiscard]] std::optional<std::vector<float>>
    render_deuteros_amiga_opening_audio(std::size_t frames);
    [[nodiscard]] std::optional<DeuterosAmigaOpeningPresentationSnapshot>
    deuteros_amiga_opening_presentation() const;
    [[nodiscard]] std::optional<DeuterosAmigaTitleStageBoundarySnapshot>
    deuteros_amiga_title_stage_boundary() const;
    [[nodiscard]] std::optional<DeuterosAmigaTitleDependencyChainCheckpoint> deuteros_amiga_title_dependency_chain_checkpoint() const;
    [[nodiscard]] DeuterosAmigaTitleDependencyObservationResult advance_deuteros_amiga_title_local_prefix();
    [[nodiscard]] DeuterosAmigaTitleDependencyObservationResult observe_deuteros_amiga_title_exec_return(DeuterosAmigaObservedExecReturn);
    [[nodiscard]] DeuterosAmigaTitleDependencyObservationResult observe_deuteros_amiga_title_open_library_return(DeuterosAmigaObservedOpenLibraryReturn);
    [[nodiscard]] DeuterosAmigaTitleDependencyObservationResult advance_deuteros_amiga_title_post_open_library_local_path();
    [[nodiscard]] DeuterosAmigaTitleDependencyObservationResult observe_deuteros_amiga_title_display_base(DeuterosAmigaObservedDisplayBaseRead);
    [[nodiscard]] DeuterosAmigaTitleDependencyObservationResult observe_deuteros_amiga_title_custom_chip_write(DeuterosAmigaObservedCustomChipWrite);
    [[nodiscard]] DeuterosAmigaTitleDependencyObservationResult observe_deuteros_amiga_title_callback_exec_return(DeuterosAmigaObservedCallbackExecReturn);
    [[nodiscard]] DeuterosAmigaTitleDependencyObservationResult observe_deuteros_amiga_title_service_setup_exec_return(DeuterosAmigaObservedServiceSetupExecReturn);
    [[nodiscard]] DeuterosAmigaTitleDependencyObservationResult observe_deuteros_amiga_title_second_service_exec_return(DeuterosAmigaObservedServiceSetupExecReturn);
    [[nodiscard]] DeuterosAmigaTitleDependencyObservationResult observe_deuteros_amiga_title_third_service_exec_return(DeuterosAmigaObservedServiceSetupExecReturn);
    [[nodiscard]] DeuterosAmigaTitleDependencyObservationResult observe_deuteros_amiga_title_fourth_service_exec_return(DeuterosAmigaObservedServiceSetupExecReturn);
    [[nodiscard]] DeuterosAmigaTitleDependencyObservationResult observe_deuteros_amiga_title_fifth_service_exec_return(DeuterosAmigaObservedServiceSetupExecReturn);
    [[nodiscard]] DeuterosAmigaTitleDependencyObservationResult advance_deuteros_amiga_title_controller_pointer_seed();
    [[nodiscard]] DeuterosAmigaTitleDependencyObservationResult observe_deuteros_amiga_title_service_batch_graphics_return(DeuterosAmigaObservedGraphicsVectorReturn);
    [[nodiscard]] DeuterosAmigaTitleDependencyObservationResult observe_deuteros_amiga_title_service_batch_runtime_word(DeuterosAmigaObservedServiceWordRead);
    [[nodiscard]] DeuterosAmigaTitleDependencyObservationResult observe_deuteros_amiga_title_graphics_service_first_return(DeuterosAmigaObservedGraphicsVectorReturn);
    [[nodiscard]] DeuterosAmigaTitleDependencyObservationResult observe_deuteros_amiga_title_graphics_service_second_return(DeuterosAmigaObservedGraphicsVectorReturn);
    [[nodiscard]] DeuterosAmigaTitleDependencyObservationResult observe_deuteros_amiga_title_graphics_service_third_return(DeuterosAmigaObservedGraphicsVectorReturn);
    [[nodiscard]] DeuterosAmigaTitleDependencyObservationResult observe_deuteros_amiga_title_tail_first_graphics_return(DeuterosAmigaObservedGraphicsVectorReturn);
    [[nodiscard]] DeuterosAmigaTitleDependencyObservationResult observe_deuteros_amiga_title_tail_copy_words(DeuterosAmigaObservedTailCopyWords);
    [[nodiscard]] DeuterosAmigaTitleDependencyObservationResult observe_deuteros_amiga_title_tail_selection_words(DeuterosAmigaObservedTailSelectionWords);
    [[nodiscard]] DeuterosAmigaTitleDependencyObservationResult observe_deuteros_amiga_title_tail_second_graphics_return(DeuterosAmigaObservedGraphicsVectorReturn);
    [[nodiscard]] DeuterosAmigaTitleDependencyObservationResult observe_deuteros_amiga_title_tail_repeated_selection_words(DeuterosAmigaObservedTailSelectionWords);
    [[nodiscard]] DeuterosAmigaTitleDependencyObservationResult observe_deuteros_amiga_title_tail_repeated_graphics_return(DeuterosAmigaObservedGraphicsVectorReturn);
    [[nodiscard]] DeuterosAmigaTitleDependencyObservationResult observe_deuteros_amiga_title_tail_repeated_wrapper_graphics_return(DeuterosAmigaObservedGraphicsVectorReturn);
    [[nodiscard]] DeuterosAmigaTitleDependencyObservationResult observe_deuteros_amiga_title_tail_source_table(DeuterosAmigaObservedTailSourceTable);
    [[nodiscard]] DeuterosAmigaTitleDependencyObservationResult observe_deuteros_amiga_title_tail_exec_return(DeuterosAmigaObservedTailExecReturn);
    [[nodiscard]] DeuterosAmigaTitleDependencyObservationResult observe_deuteros_amiga_title_load_service_return(DeuterosAmigaObservedLocalCallReturn);
    [[nodiscard]] DeuterosAmigaTitleDependencyObservationResult observe_deuteros_amiga_title_load_selector(DeuterosAmigaObservedLoadSelector);
    [[nodiscard]] DeuterosAmigaTitleDependencyObservationResult observe_deuteros_amiga_title_load_copy_chunk(DeuterosAmigaObservedLoadCopyChunk);
    [[nodiscard]] DeuterosAmigaTitleDependencyObservationResult observe_deuteros_amiga_title_load_dispatch_table_base(DeuterosAmigaObservedLoadDispatchTableBase);
    [[nodiscard]] DeuterosAmigaTitleDependencyObservationResult observe_deuteros_amiga_title_load_dispatch_table_word(DeuterosAmigaObservedLoadDispatchTableWord);
    [[nodiscard]] DeuterosAmigaTitleDependencyObservationResult observe_deuteros_amiga_title_command_opcode(DeuterosAmigaObservedTitleCommandOpcode);
    [[nodiscard]] DeuterosAmigaTitleDependencyObservationResult observe_deuteros_amiga_title_command_operand_byte(DeuterosAmigaObservedTitleCommandOperandByte);
    [[nodiscard]] DeuterosAmigaTitleDependencyObservationResult observe_deuteros_amiga_title_command_pointer_long(DeuterosAmigaObservedTitleCommandPointerLong);
    [[nodiscard]] DeuterosAmigaTitleDependencyObservationResult observe_deuteros_amiga_title_command_eight_pointer(DeuterosAmigaObservedTitleCommandEightPointer);
    [[nodiscard]] DeuterosAmigaTitleDependencyObservationResult observe_deuteros_amiga_title_command_eight_mode(DeuterosAmigaObservedTitleCommandEightMode);
    [[nodiscard]] DeuterosAmigaTitleDependencyObservationResult observe_deuteros_amiga_title_command_eight_scale(DeuterosAmigaObservedTitleCommandEightScale);
    [[nodiscard]] DeuterosAmigaTitleDependencyObservationResult observe_deuteros_amiga_title_command_call_return(DeuterosAmigaObservedTitleCommandCallReturn);
    [[nodiscard]] DeuterosAmigaTitleDependencyObservationResult observe_deuteros_amiga_title_command_planar_write(DeuterosAmigaObservedTitleCommandPlanarWrite);
    [[nodiscard]] DeuterosAmigaTitleDependencyObservationResult observe_deuteros_amiga_title_command_planar_variant_write(DeuterosAmigaObservedTitleCommandPlanarVariantWrite);
    [[nodiscard]] DeuterosAmigaTitleDependencyObservationResult observe_deuteros_amiga_title_command_negative_service(DeuterosAmigaObservedTitleCommandNegativeService);
    [[nodiscard]] DeuterosAmigaTitleDependencyObservationResult observe_deuteros_amiga_title_post_command_pointer_route(DeuterosAmigaObservedTitlePostCommandPointerRoute);
    [[nodiscard]] DeuterosAmigaTitleDependencyObservationResult observe_deuteros_amiga_title_post_command_graphics_return(DeuterosAmigaObservedGraphicsVectorReturn);
    [[nodiscard]] DeuterosAmigaTitleDependencyObservationResult advance_deuteros_amiga_title_post_command_first_dispatch();
    [[nodiscard]] DeuterosAmigaTitleDependencyObservationResult observe_deuteros_amiga_title_post_command_first_dispatch_header(DeuterosAmigaObservedTitleFirstDispatchHeader);
    [[nodiscard]] DeuterosAmigaTitleDependencyObservationResult advance_deuteros_amiga_title_post_command_first_dispatch_packet();
    [[nodiscard]] DeuterosAmigaTitleDependencyObservationResult advance_deuteros_amiga_title_post_command_first_dispatch_decode();
    [[nodiscard]] DeuterosAmigaTitleDependencyObservationResult advance_deuteros_amiga_title_post_command_first_dispatch_caller_tail();
    [[nodiscard]] DeuterosAmigaTitleDependencyObservationResult observe_deuteros_amiga_title_post_command_first_dispatch_destination_words(DeuterosAmigaObservedTitleFirstDispatchDestinationWords);
    [[nodiscard]] DeuterosAmigaTitleDependencyObservationResult advance_deuteros_amiga_title_post_command_second_dispatch();
    [[nodiscard]] DeuterosAmigaTitleDependencyObservationResult advance_deuteros_amiga_title_post_command_second_dispatch_decode();
    [[nodiscard]] DeuterosAmigaTitleDependencyObservationResult observe_deuteros_amiga_title_post_command_second_dispatch_destination_words(DeuterosAmigaObservedTitleSecondDispatchDestinationWords);
    [[nodiscard]] DeuterosAmigaTitleDependencyObservationResult advance_deuteros_amiga_title_post_command_service_route_prefix();
    [[nodiscard]] DeuterosAmigaTitleDependencyObservationResult observe_deuteros_amiga_title_post_command_service_first_return(DeuterosAmigaObservedLocalCallReturn);
    [[nodiscard]] DeuterosAmigaTitleDependencyObservationResult observe_deuteros_amiga_title_post_command_service_second_return(DeuterosAmigaObservedLocalCallReturn);
    [[nodiscard]] DeuterosAmigaTitleDependencyObservationResult observe_deuteros_amiga_title_post_command_service_third_return(DeuterosAmigaObservedLocalCallReturn);
    [[nodiscard]] DeuterosAmigaTitleDependencyObservationResult observe_deuteros_amiga_title_post_command_nested_words(DeuterosAmigaObservedTitlePostCommandNestedWords);
    [[nodiscard]] DeuterosAmigaTitleDependencyObservationResult observe_deuteros_amiga_title_post_command_nested_call_return(DeuterosAmigaObservedLocalCallReturn);
    [[nodiscard]] DeuterosAmigaTitleDependencyObservationResult advance_deuteros_amiga_title_post_command_nested_loop();
    [[nodiscard]] DeuterosAmigaTitleDependencyObservationResult observe_deuteros_amiga_title_post_command_continuation_return(DeuterosAmigaObservedLocalCallReturn);
    [[nodiscard]] DeuterosAmigaTitleDependencyObservationResult observe_deuteros_amiga_title_post_command_pointer_chain(DeuterosAmigaObservedTitlePostCommandPointerChain);
    [[nodiscard]] DeuterosAmigaTitleDependencyObservationResult observe_deuteros_amiga_title_post_command_dispatch_destination(DeuterosAmigaObservedTitlePostCommandDispatchDestination);
    [[nodiscard]] DeuterosAmigaTitleDependencyObservationResult advance_deuteros_amiga_title_post_command_selected_stream();
    [[nodiscard]] DeuterosAmigaTitleDependencyObservationResult observe_deuteros_amiga_title_post_command_descriptor_call_return(DeuterosAmigaObservedLocalCallReturn);
    [[nodiscard]] DeuterosAmigaTitleDependencyObservationResult advance_deuteros_amiga_title_post_command_descriptor_loop();
    [[nodiscard]] DeuterosAmigaTitleDependencyObservationResult observe_deuteros_amiga_title_post_command_descriptor_byte(DeuterosAmigaObservedTitlePostCommandDescriptorByte);
    [[nodiscard]] DeuterosAmigaTitleDependencyObservationResult observe_deuteros_amiga_title_post_command_adjusted_dispatch_destination(DeuterosAmigaObservedTitlePostCommandAdjustedDispatchDestination);
    [[nodiscard]] DeuterosAmigaTitleDependencyObservationResult observe_deuteros_amiga_title_post_adjusted_caller_pointer(DeuterosAmigaObservedTitlePostAdjustedCallerPointer);
    [[nodiscard]] DeuterosAmigaTitleDependencyObservationResult observe_deuteros_amiga_title_post_adjusted_object_gate(DeuterosAmigaObservedTitlePostAdjustedObjectGate);
    [[nodiscard]] DeuterosAmigaTitleDependencyObservationResult observe_deuteros_amiga_title_post_adjusted_first_helper_return(DeuterosAmigaObservedLocalCallReturn);
    [[nodiscard]] DeuterosAmigaTitleDependencyObservationResult observe_deuteros_amiga_title_post_adjusted_second_helper_return(DeuterosAmigaObservedLocalCallReturn);
    [[nodiscard]] DeuterosAmigaTitleDependencyObservationResult observe_deuteros_amiga_title_post_adjusted_rts_frame(DeuterosAmigaObservedTitlePostAdjustedRtsFrame);
    [[nodiscard]] DeuterosAmigaTitleDependencyObservationResult observe_deuteros_amiga_title_post_adjusted_repeated_nested_words(DeuterosAmigaObservedTitlePostCommandNestedWords);
    [[nodiscard]] DeuterosAmigaTitleDependencyObservationResult observe_deuteros_amiga_title_post_adjusted_repeated_nested_call_return(DeuterosAmigaObservedLocalCallReturn);
    [[nodiscard]] DeuterosAmigaTitleDependencyObservationResult advance_deuteros_amiga_title_post_adjusted_repeated_nested_loop();
    [[nodiscard]] DeuterosAmigaTitleDependencyObservationResult advance_deuteros_amiga_title_post_adjusted_caller_indirect();
    [[nodiscard]] DeuterosAmigaTitleDependencyObservationResult observe_deuteros_amiga_title_post_adjusted_caller_indirect_return(DeuterosAmigaObservedTitlePostAdjustedIndirectReturn);
    [[nodiscard]] DeuterosAmigaTitleDependencyObservationResult observe_deuteros_amiga_title_post_adjusted_caller_37180_return(DeuterosAmigaObservedTitlePostAdjusted37180Return);
    [[nodiscard]] DeuterosAmigaTitleDependencyObservationResult observe_deuteros_amiga_title_post_adjusted_mode_return(DeuterosAmigaObservedLocalCallReturn);


    [[nodiscard]] DeuterosAmigaTitleDisplayTraceAdmission
    admit_active_deuteros_amiga_title_display_trace(const ReferenceTrace& trace);
    [[nodiscard]] std::optional<DeuterosAmigaTitleDisplayTraceCheckpoint>
    deuteros_amiga_title_display_trace_checkpoint() const;
    [[nodiscard]] std::optional<DeuterosAmigaTitlePlanarPatchSnapshot>
    deuteros_amiga_title_planar_patch() const;
    [[nodiscard]] std::optional<DeuterosAmigaTitlePlanarSurfaceSnapshot>
    deuteros_amiga_title_planar_surface() const;
    [[nodiscard]] std::optional<DeuterosAtariBootstrapCheckpoint>
    deuteros_atari_bootstrap_checkpoint() const;
    [[nodiscard]] std::optional<DeuterosAtariBootstrapPresentationSnapshot>
    deuteros_atari_bootstrap_presentation() const;
    [[nodiscard]] std::optional<MillenniumAmigaBootstrapPresentationSnapshot>
    millennium_amiga_bootstrap_presentation() const;
    [[nodiscard]] MillenniumAmigaBootstrapRelocatorObservationResult observe_millennium_amiga_bootstrap_relocator_overread(MillenniumAmigaBootstrapRelocatorObservation);
    [[nodiscard]] MillenniumAmigaBootstrapRelocatorObservationResult observe_millennium_amiga_bootstrap_relocator_terminal_jump(MillenniumAmigaBootstrapRelocatorObservation);
    [[nodiscard]] std::optional<MillenniumAmigaBootstrapRelocatorCheckpoint> millennium_amiga_bootstrap_relocator_checkpoint() const;

    [[nodiscard]] std::optional<MillenniumAtariBootstrapPresentationSnapshot>
    millennium_atari_bootstrap_presentation() const;
    [[nodiscard]] MillenniumAtariConfigConsumerResult
    observe_millennium_atari_status_register(MillenniumAtariStatusRegisterObservation);
    [[nodiscard]] MillenniumAtariConfigConsumerResult
    observe_millennium_atari_xbios_selector_two(MillenniumAtariXbiosSelectorTwoObservation);
    [[nodiscard]] MillenniumAtariConfigConsumerResult observe_millennium_atari_xbios_selector_three(MillenniumAtariXbiosSelectorThreeObservation);
    [[nodiscard]] MillenniumAtariConfigConsumerResult observe_millennium_atari_xbios_selector_four(MillenniumAtariXbiosSelectorFourObservation);
    [[nodiscard]] MillenniumAtariConfigConsumerResult observe_millennium_atari_line_a(MillenniumAtariLineAObservation);
    [[nodiscard]] MillenniumAtariConfigConsumerResult observe_millennium_atari_xbios_selector_21(MillenniumAtariXbiosSelector21Observation);
    [[nodiscard]] MillenniumAtariConfigConsumerResult observe_millennium_atari_xbios_selector_6(MillenniumAtariXbiosSelector6Observation);
    [[nodiscard]] MillenniumAtariConfigConsumerResult observe_millennium_atari_bchg_2b55a(MillenniumAtariBchgObservation);
    [[nodiscard]] MillenniumAtariConfigConsumerResult execute_millennium_atari_jsr_2b55a();
    [[nodiscard]] MillenniumAtariConfigConsumerResult execute_millennium_atari_bsr_2b59a();
    [[nodiscard]] MillenniumAtariConfigConsumerResult observe_millennium_atari_d0_indexed_byte(MillenniumAtariD0IndexedByteObservation);
    [[nodiscard]] MillenniumAtariConfigConsumerResult execute_millennium_atari_a1_setup();
    [[nodiscard]] MillenniumAtariConfigConsumerResult observe_millennium_atari_d0_indexed_word(MillenniumAtariD0IndexedWordObservation);
    [[nodiscard]] MillenniumAtariConfigConsumerResult observe_millennium_atari_a0_indexed_word(MillenniumAtariA0IndexedWordObservation);
    [[nodiscard]] MillenniumAtariConfigConsumerResult execute_millennium_atari_loop_iteration_setup();
    [[nodiscard]] MillenniumAtariConfigConsumerResult execute_millennium_atari_loop_epilogue();
    [[nodiscard]] MillenniumAtariConfigConsumerResult observe_millennium_atari_movem_frame(MillenniumAtariMovemFrameObservation);
    [[nodiscard]] MillenniumAtariConfigConsumerResult execute_millennium_atari_jsr_2aa68();
    [[nodiscard]] MillenniumAtariConfigConsumerResult observe_millennium_atari_xbios_selector_38(MillenniumAtariXbiosSelector38Observation);
    [[nodiscard]] MillenniumAtariConfigConsumerResult execute_millennium_atari_jsr_2aa0c();
    [[nodiscard]] MillenniumAtariConfigConsumerResult observe_millennium_atari_gemdos_selector_61(MillenniumAtariGemdosSelector61Observation);
    [[nodiscard]] MillenniumAtariConfigConsumerResult execute_millennium_atari_jsr_2a5c2();
    [[nodiscard]] MillenniumAtariConfigConsumerResult observe_millennium_atari_gemdos_selector_63(MillenniumAtariGemdosSelector63Observation);
    [[nodiscard]] MillenniumAtariConfigConsumerResult observe_millennium_atari_gemdos_selector_62(MillenniumAtariGemdosSelector62Observation);
    [[nodiscard]] MillenniumAtariConfigConsumerResult observe_millennium_atari_fread_prefix(MillenniumAtariFreadPrefixObservation);
    [[nodiscard]] MillenniumAtariConfigConsumerResult execute_millennium_atari_jsr_2b2be();
    [[nodiscard]] MillenniumAtariConfigConsumerResult observe_millennium_atari_game_init_source_byte(MillenniumAtariGameInitSourceByteObservation);
    [[nodiscard]] MillenniumAtariConfigConsumerResult observe_millennium_atari_game_init_zero_pair(MillenniumAtariGameInitZeroPairObservation);
    [[nodiscard]] MillenniumAtariConfigConsumerResult execute_millennium_atari_game_init_zero_counter_branch();
    [[nodiscard]] MillenniumAtariConfigConsumerResult observe_millennium_atari_game_init_replicated_byte(MillenniumAtariGameInitReplicatedByteObservation);
    [[nodiscard]] MillenniumAtariConfigConsumerResult observe_millennium_atari_game_init_swapped_pair(MillenniumAtariGameInitSwappedPairObservation);
    [[nodiscard]] MillenniumAtariConfigConsumerResult observe_millennium_atari_game_init_extended_run(MillenniumAtariGameInitExtendedRunObservation);
    [[nodiscard]] MillenniumAtariConfigConsumerResult execute_millennium_atari_game_init_return();
    [[nodiscard]] MillenniumAtariConfigConsumerResult execute_millennium_atari_game_init_palette_copy_prefix();
    [[nodiscard]] MillenniumAtariConfigConsumerResult observe_millennium_atari_game_init_palette_words(MillenniumAtariGameInitPaletteWordsObservation);
    [[nodiscard]] MillenniumAtariConfigConsumerResult observe_millennium_atari_game_init_palette_xbios_selector_6(MillenniumAtariGameInitPaletteXbios6Observation);
    [[nodiscard]] MillenniumAtariConfigConsumerResult observe_millennium_atari_game_init_palette_recurrence(MillenniumAtariGameInitPaletteRecurrenceObservation);
    [[nodiscard]] MillenniumAtariConfigConsumerResult observe_millennium_atari_game_init_palette_rts(MillenniumAtariGameInitPaletteRtsObservation);
    [[nodiscard]] MillenniumAtariConfigConsumerResult observe_millennium_atari_game_init_second_config_fopen(MillenniumAtariGemdosSelector61Observation);
    [[nodiscard]] MillenniumAtariConfigConsumerResult observe_millennium_atari_game_init_second_config_fread(MillenniumAtariGemdosSelector63Observation);
    [[nodiscard]] MillenniumAtariConfigConsumerResult observe_millennium_atari_game_init_second_config_fclose(MillenniumAtariGemdosSelector62Observation);
    [[nodiscard]] MillenniumAtariConfigConsumerResult observe_millennium_atari_game_init_second_config_rts(MillenniumAtariGameInitSecondConfigRtsObservation);

    // Begin before SDL destroys any source-derived object.  A monotonically
    // increasing generation lets a front end reject stale render/audio work
    // it scheduled for the preceding exact release.
    void begin_source_revocation();
    // Finish only after the front end has released all source-derived borrows.
    // Calling this outside a revocation interval is deliberately inert.
    void finish_source_revocation();

    // SDL supplies a monotonic time value; the host decides whether the one
    // recovered 50 Hz session may run.  No SDL clock, renderer, device audio
    // or generic game tick crosses this boundary.
    [[nodiscard]] RuntimeHostAdvance advance(std::uint64_t monotonic_tick);
    [[nodiscard]] RuntimeHostSnapshot snapshot() const;

    // A front-end modal owns physical input until it closes. Suppression is
    // checked before the release-bound coordinator sees an observation; it is
    // not an alternate input mapping. Enabling it also drops a held opening
    // signal so it cannot survive behind a modal.
    void set_input_suppressed(bool suppressed);
    [[nodiscard]] bool input_suppressed() const { return input_suppressed_; }
    [[nodiscard]] RuntimeInputDisposition observe_input(const RuntimeInputObservation& observation);

    [[nodiscard]] bool revoking() const;
    [[nodiscard]] std::uint64_t generation() const { return generation_; }

private:
    std::uint64_t generation_ = 0;
    bool input_suppressed_ = false;
};

} // namespace eon
