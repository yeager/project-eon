#pragma once

#include "engine/release_runtime.hpp"

namespace eon {

// The CLI and card menu share this final candidate boundary.  It accepts only
// a normalized request DTO and scanner-produced identities, then publishes a
// successful result solely from the coordinator's rehashed active snapshot.
// It has no SDL, renderer, save, Modern-pack, or game-media decoding surface.
struct RuntimeCandidateLaunchResult {
    ReleaseRuntimeAdmission admission = ReleaseRuntimeAdmission::unselected;
    ReleaseRuntimeRejection rejection = ReleaseRuntimeRejection::none;
    std::optional<ResolvedLaunchRequest> active_launch;

    [[nodiscard]] bool accepted() const {
        return admission == ReleaseRuntimeAdmission::active
            && rejection == ReleaseRuntimeRejection::none && active_launch.has_value();
    }
};

[[nodiscard]] RuntimeCandidateLaunchResult launch_runtime_candidate(
    const std::optional<LaunchRequest>& candidate,
    const std::vector<ReleaseArchive>& releases, ReleaseRuntimeCoordinator& coordinator);

// Owns the live adapter identity for the launcher. SDL remains responsible
// for destroying textures/audio/input borrows when this controller reports a
// revocation; this class owns only the coordinator and its safe provenance.
class LauncherRuntimeController {
public:
    LauncherRuntimeController() = default;
    LauncherRuntimeController(const LauncherRuntimeController&) = delete;
    LauncherRuntimeController& operator=(const LauncherRuntimeController&) = delete;
    LauncherRuntimeController(LauncherRuntimeController&&) = delete;
    LauncherRuntimeController& operator=(LauncherRuntimeController&&) = delete;
    [[nodiscard]] RuntimeCandidateLaunchResult launch_direct(const LaunchRequest& candidate,
        const std::vector<ReleaseArchive>& releases);
    [[nodiscard]] RuntimeCandidateLaunchResult launch_menu(const LauncherSessionState& session,
        const LaunchRequest& base, const std::vector<ReleaseArchive>& releases);
    // Pure query: main must discard SDL-side borrows before reset() invalidates
    // the coordinator-owned adapters that supplied them.
    [[nodiscard]] bool requires_revocation_for(const LauncherSourceIdentity& source) const;
    void reset();
    // Typed native operations are deliberately forwarded here instead of
    // exposing the mutable release coordinator to the session/UI layer.
    // Every result is a value copy or a transient buffer; SDL cannot acquire
    // an adapter, media view, or mutable input session through this facade.
    [[nodiscard]] RuntimeInputDisposition observe_input(const RuntimeInputObservation& observation);
    [[nodiscard]] std::optional<MillenniumDosPresentationSnapshot>
    millennium_dos_presentation() const;
    [[nodiscard]] std::optional<MillenniumDosStartupInputSnapshot>
    millennium_dos_startup_input() const;
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
    [[nodiscard]] std::optional<DeuterosAmigaVmEvents> tick_deuteros_amiga_opening();
    [[nodiscard]] std::optional<std::vector<float>>
    render_deuteros_amiga_opening_audio(std::size_t frames);
    [[nodiscard]] std::optional<DeuterosAmigaOpeningCheckpoint>
    deuteros_amiga_opening_checkpoint() const;
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


    [[nodiscard]] DeuterosAmigaTitleDisplayTraceAdmission
    admit_active_deuteros_amiga_title_display_trace(const ReferenceTrace& trace);
    [[nodiscard]] std::optional<DeuterosAmigaTitleDisplayTraceCheckpoint>
    deuteros_amiga_title_display_trace_checkpoint() const;
    [[nodiscard]] std::optional<DeuterosAtariBootstrapCheckpoint>
    deuteros_atari_bootstrap_checkpoint() const;
    [[nodiscard]] std::optional<DeuterosAtariBootstrapPresentationSnapshot>
    deuteros_atari_bootstrap_presentation() const;
    [[nodiscard]] std::optional<MillenniumAmigaBootstrapPresentationSnapshot>
    millennium_amiga_bootstrap_presentation() const;
    [[nodiscard]] std::optional<MillenniumAtariBootstrapPresentationSnapshot>
    millennium_atari_bootstrap_presentation() const;
    [[nodiscard]] const std::optional<ResolvedLaunchRequest>& active() const { return coordinator_.active(); }
    [[nodiscard]] ReleaseRuntimeAdmission admission() const { return coordinator_.admission(); }
    [[nodiscard]] ReleaseRuntimeRejection rejection() const { return coordinator_.rejection(); }
    [[nodiscard]] std::optional<RuntimeSessionSnapshot> session_snapshot() const;

private:
    ReleaseRuntimeCoordinator coordinator_;
};

// The card menu has exactly one transition into a live release adapter.  This
// SDL-free gate deliberately receives session state, scanner identities and
// the runtime coordinator rather than card indexes, paths or adapters.  It
// makes a successful result observable only through the coordinator's final,
// rehashed identity.
struct MenuRuntimeLaunchResult {
    RuntimeLaunchAdmission admission;
    std::optional<ResolvedLaunchRequest> active_launch;

    [[nodiscard]] bool accepted() const {
        return admission.accepted() && active_launch.has_value();
    }
};

[[nodiscard]] MenuRuntimeLaunchResult launch_menu_runtime(
    const LauncherSessionState& session, const LaunchRequest& base,
    const std::vector<ReleaseArchive>& releases, ReleaseRuntimeCoordinator& coordinator);

} // namespace eon
