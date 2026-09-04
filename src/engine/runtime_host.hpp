#pragma once

#include "engine/native_session_controller.hpp"

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
    [[nodiscard]] MillenniumDosBdfObservationResult observe_millennium_dos_bdf_byte(MillenniumDosBdfByteObservation);[[nodiscard]] MillenniumDosBdfObservationResult observe_millennium_dos_bdf_word(MillenniumDosBdfWordObservation);[[nodiscard]] MillenniumDosBdfObservationResult observe_millennium_dos_bdf_far_byte(MillenniumDosBdfFarByteObservation);[[nodiscard]] MillenniumDosBdfObservationResult observe_millennium_dos_bdf_poll_return(MillenniumDosBdfPollReturnObservation);[[nodiscard]] MillenniumDosBdfObservationResult observe_millennium_dos_bdf_mapping_return(MillenniumDosBdfMappingReturnObservation);[[nodiscard]]std::optional<MillenniumDosBdfCheckpoint>millennium_dos_bdf_checkpoint()const;
    [[nodiscard]] MillenniumDosBdfObservationResult observe_millennium_dos_bdf_external_return(MillenniumDosBdfExternalReturnObservation);
    [[nodiscard]] MillenniumDosBdfObservationResult observe_millennium_dos_bdf_terminal_jump(MillenniumDosBdfTerminalJumpObservation);
    [[nodiscard]] MillenniumDosBdfObservationResult observe_millennium_dos_bdf_mode_two_byte(MillenniumDosBdfByteObservation);[[nodiscard]] MillenniumDosBdfObservationResult observe_millennium_dos_bdf_mode_two_word(MillenniumDosBdfWordObservation);
    [[nodiscard]] MillenniumDosBdfObservationResult observe_millennium_dos_bdf_mode_two_far_word(MillenniumDosBdfModeTwoFarWordObservation);
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
