#pragma once

#include "launcher.hpp"
#include "engine/deuteros_amiga_opening.hpp"
#include "engine/deuteros_amiga_paula.hpp"
#include "engine/deuteros_amiga_title_display_trace_session.hpp"
#include "engine/runtime_session.hpp"
#include "engine/deuteros_atari_bootstrap_session.hpp"
#include "engine/millennium_amiga_bootstrap_session.hpp"
#include "engine/millennium_atari_bootstrap_session.hpp"
#include "engine/millennium_dos_save_session.hpp"
#include "engine/millennium_dos_sound_selection_session.hpp"
#include "engine/millennium_dos_gx_startup_trace_admission.hpp"
#include "engine/millennium_dos_native_process_admission.hpp"
#include "engine/millennium_dos_owned_function_diagnostics.hpp"
#include "engine/millennium_dos_title_session.hpp"
#include "data/millennium_dos_game_flow.hpp"
#include "data/millennium_dos_sound_driver.hpp"
#include "data/millennium_dos_title_flow.hpp"
#include "data/millennium_dos_video_driver.hpp"
#include "data/millennium_dos_voice_bank.hpp"

#include <cstdint>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace eon {

struct ReferenceTrace;

enum class ReleaseRuntimeAdmission {
    unselected, active, identity_rejected, archive_rejected, adapter_rejected,
};

// Stable, media-safe diagnostic reason for a rejected native launch. It never
// includes paths, archive-member names, original bytes, or parser exceptions.
enum class ReleaseRuntimeRejection {
    none, launch_identity, original_media, runtime_capability,
    adapter_construction, input_contract, child_session, lifecycle_transition,
};

// This is intentionally a small, media-safe diagnostic vocabulary. It
// describes only which preservation boundary declined admission; it never
// surfaces archive paths, member names, original bytes, or parser exceptions.
[[nodiscard]] std::string_view release_runtime_admission_label(
    ReleaseRuntimeAdmission admission);
[[nodiscard]] std::string_view release_runtime_rejection_label(
    ReleaseRuntimeRejection rejection);

// Immutable decoded pixels remain derived from the caller's already verified
// original media. This DTO deliberately contains no SDL objects, and its
// bytes never escape into user media or a save file.
struct MillenniumDosPreviewAnimation {
    int width = 0;
    int height = 0;
    std::vector<std::vector<std::uint8_t>> rgba_frames;
};

// Presentation facts copied out of the coordinator-owned Deuteros opening
// adapter.  SDL may retain this value for its current draw only, but cannot
// retain a mutable VM, ADF, title-stage session, or an adapter borrow past a
// lifecycle transition.  `rgba_frame` is decoded from the admitted media and
// exists only while the recovered opening session remains active.
struct DeuterosAmigaOpeningPresentationSnapshot {
    DeuterosAmigaOpeningCheckpoint checkpoint;
    std::uint16_t palette_index = 0;
    std::size_t active_channel_count = 0;
    bool frame_composed_on_last_tick = false;
    std::optional<std::vector<std::uint8_t>> rgba_frame;
};

// The narrow, immutable facts that are safe to show after the opening has
// handed control to its hash-validated title stage.  This is deliberately a
// preservation boundary, not a title-stage executor or a display surface.
struct DeuterosAmigaTitleStageBoundarySnapshot {
    AmigaLoadStage stage;
    std::string original_sha256;
    DeuterosAmigaTitleEntryPrefixState entry_prefix_state;
    DeuterosAmigaTitleExecPrelude exec_prelude;
    bool local_prefix_executed = false;
    std::array<RgbColor, 20> graphics_setup_palette{};
    std::optional<DeuterosAmigaAlternateRendererTrace> alternate_renderer_trace;
};

// Copy-only diagnostics for the currently owned title dependency chain. It
// reports only boundaries already reached by the title-stage session.
struct DeuterosAmigaTitleDependencyChainCheckpoint {
    std::string title_stage_sha256;
    DeuterosAmigaTitleExecBoundaryCheckpoint exec;
    std::optional<DeuterosAmigaTitleOpenLibraryBoundaryCheckpoint> open_library;
    bool custom_chip_boundary_present = false;
    std::size_t observed_custom_chip_write_count = 0;
    bool custom_chip_complete = false;
    bool callback_exec_return_observed = false;
    bool service_setup_boundary_armed = false;
    std::optional<DeuterosAmigaTitleServiceSetupLocalPlan> service_setup_local_plan;
    std::optional<DeuterosAmigaTitleSecondServiceLocalPlan> second_service_local_plan;
    std::optional<DeuterosAmigaTitleThirdServiceLocalPlan> third_service_local_plan;
    std::uint32_t stop_before_address = 0;
};
struct DeuterosAmigaTitleDependencyObservationResult { bool accepted=false; std::string error; };

// Media-safe facts for the exact Deuteros Atari ST bootstrap boundary.  The
// retained prefixes are only local copy/entry results; this DTO cannot select
// a protected state, issue Floprd, or cross the unrecovered XBIOS boundary.
struct DeuterosAtariBootstrapPresentationSnapshot {
    DeuterosAtariBootstrapCheckpoint checkpoint;
    std::size_t first_stage_disk_offset = 0;
    std::size_t first_stage_length = 0;
    DeuterosAtariFirstStageCopyExecutionPrefix copy_execution;
    DeuterosAtariSecondStageEntryExecutionPrefix entry_execution;
};

// Immutable, static Millennium Amiga bootstrap provenance. It contains the
// exact read-only load ranges and caller-side handoff facts, never a decoded
// first stage, an Amiga OS result, input, or runnable execution state.
struct MillenniumAmigaBootstrapPresentationSnapshot {
    MillenniumAmigaLoadPlan plan;
    MillenniumAmigaBootstrapOpaqueInvocationBoundary opaque_invocation_boundary;
    MillenniumAmigaResidentEvidenceSnapshot resident_evidence;
};

// Immutable Millennium Atari ST bootstrap provenance.  It reports only the
// locally recovered copy, Fopen and Fread-prefix facts.  The snapshot cannot
// issue GEMDOS/XBIOS calls, materialize a guest Fread result, or cross the
// configuration-transfer boundary.
struct MillenniumAtariBootstrapPresentationSnapshot {
    MillenniumAtariBootstrap bootstrap;
    MillenniumAtariBssEntry bss_entry;
    MillenniumAtariBssSource bss_source;
    MillenniumAtariMaterializedTarget target;
    MillenniumAtariBootstrapExecution execution;
    MillenniumAtariTrapEntry fopen_boundary;
    MillenniumAtariFopenResultGateExecution fopen_result_gate;
    MillenniumAtariFopenFallthrough fopen_fallthrough;
    MillenniumAtariFreadFramePrefixExecution fread_frame_prefix;
    MillenniumAtariFreadConfigTransferBoundary fread_config_transfer;
    MillenniumAtariRootInventory root_inventory;
    MillenniumAtariConfigEvidence config;
    MillenniumAtariConfigEntry config_entry;
    MillenniumAtariFreadConfigLoadAddressBoundary fread_config_load_address_boundary;
    MillenniumAtariFreadMappedConfigPrelude fread_mapped_config_prelude;
};

// The recovered DOS title/runtime evidence for one exact archive identity.
// Spanish is title-only until its executable handoff is evidenced; English
// retains the additional parser outputs below. No field is a fallback for a
// different release, platform, or language.
struct MillenniumDosRuntimeAssets {
    MillenniumDosPreviewAnimation title;
    std::string language;
    std::optional<MillenniumDosPreviewAnimation> gx_canvas;
    // Immutable original labels and pointer-table topology. These establish
    // source data for a later recovered game core, not a host-generated UI
    // or a claim that any label has reached a live simulation state.
    std::optional<MillenniumDosGameData> static_game_data;
    std::optional<MillenniumDosStaticDataEvidence> static_data_evidence;
    // Exact original VOC catalogue metadata. Its presence does not permit
    // playback: event mapping, driver ABI and timing remain unrecovered.
    std::optional<MillenniumDosVoiceBankEvidence> voice_bank;
    std::optional<MillenniumDosTitleFlow> title_flow;
    std::optional<MillenniumDosSoundSelectionEvidence> sound_selection;
    std::optional<std::string> sound_selection_prompt;
    // These are identity-only admissions for the two supplied selectable
    // driver leaves. The driver bytes are discarded after validation: no
    // driver is executed, emulated, cached, or written by the runtime.
    std::optional<MillenniumDosSoundDriverLeaf> sound_blaster_driver;
    std::optional<MillenniumDosSoundDriverLeaf> covox_driver;
    std::optional<MillenniumDosSpanishTitleBoundary> spanish_title_boundary;
    std::optional<MillenniumDosGameFlow> game_flow;
    std::optional<MillenniumDosVideoDriverProfile> ega_video_driver;
    std::optional<MillenniumDosVideoDriverProfile> mcga_video_driver;
    std::optional<MillenniumDosSaveSession> initial_save;
};

// A value-only view of one admitted Millennium DOS session.  It deliberately
// copies decoded, media-derived values so SDL cannot retain the coordinator's
// adapter or any of its input/session objects across reset or a new launch.
struct MillenniumDosPresentationSnapshot {
    MillenniumDosRuntimeAssets assets;
};

// The only DOS startup-input facts the UI needs.  The raw title and sound
// selection sessions remain coordinator-owned; callers can request a fresh
// snapshot after forwarding an observation but cannot poll or mutate either
// session themselves.
struct MillenniumDosStartupInputSnapshot {
    bool sound_selection_active = false;
    bool sound_selection_awaiting_choice = false;
    std::optional<std::string> selected_original_filename;
    bool selected_driver_is_admitted = false;
    bool title_active = false;
    bool title_handed_off = false;
};

// Hash-gated, static dispatch provenance for 2200AD.EXE.  This deliberately
// describes the table encoded in the admitted executable, not a playable
// game session: no action has been observed, no handler has executed, and
// no native runtime cells are exposed or reconstructed here.
struct MillenniumDosStaticDispatchEntry {
    std::string function_id;
    std::uint8_t action = 0;
    std::uint16_t handler_address = 0;
};

struct MillenniumDosStaticDispatchDiagnostics {
    std::uint32_t action_poll_address = 0;
    std::uint8_t first_action = 0;
    std::size_t action_count = 0;
    std::uint16_t table_address = 0;
    std::size_t table_stride = 0;
    std::uint32_t dispatch_address = 0;
    std::array<std::uint16_t, 10> handler_addresses{};
    std::array<MillenniumDosStaticDispatchEntry, 10> handlers{};
};

// Value-only terminal checkpoint for the one admitted GX startup suffix.
// The write records are reconstructed from the exact original instructions;
// they neither expose original bytes nor name complete game-state semantics.
struct MillenniumDosGxStartupCheckpoint {
    MillenniumDosGxStartupSessionState state =
        MillenniumDosGxStartupSessionState::awaiting_private_return;
    std::size_t observed_post_overlay_call_return_count = 0;
    std::uint16_t overlay_return_boundary = 0;
    std::uint16_t private_interrupt_boundary = 0;
    std::vector<MillenniumDosGxOverlayStartupWrite> overlay_writes;
};

struct MillenniumDosGxActiveTraceAdmission {
    bool accepted = false;
    std::string error;
};

struct MillenniumDosPostOverlayPrivateInterruptReturnObservation {
    std::uint16_t interrupt_address = 0;
    std::uint16_t ax = 0;
};

struct MillenniumDosPostOverlayCallReturnObservation {
    std::uint16_t call_address = 0;
    std::uint16_t return_address = 0;
};

struct MillenniumDosPostOverlayAlObservation {
    std::uint16_t test_address = 0;
    std::uint8_t value = 0;
};

struct MillenniumDosPostOverlayRuntimeByteObservation {
    std::uint16_t load_address = 0;
    std::uint16_t runtime_address = 0;
    std::uint8_t value = 0;
};

struct MillenniumDosPostOverlayObservationResult {
    bool accepted = false;
    std::string error;
};

struct MillenniumDosPostOverlayLoopCheckpoint {
    MillenniumDosPostOverlayLoopState state =
        MillenniumDosPostOverlayLoopState::awaiting_private_interrupt_return;
    MillenniumDosPostOverlayLoopBoundary boundary;
    std::size_t completed_call_return_count = 0;
    std::size_t action_poll_count = 0;
    std::optional<std::uint16_t> observed_private_interrupt_ax;
    std::optional<std::uint8_t> observed_action;
    std::optional<std::size_t> function_key_index;
    std::vector<MillenniumDosPostOverlayRuntimeByteEffect> runtime_effects;
};

// The post-overlay loop proves only the scaled dispatcher call and its index.
// A transition into a concrete handler additionally requires this independent,
// address-bound observation of the original dispatcher resolution.
struct MillenniumDosTenthFunctionDispatchObservation {
    std::uint16_t scaled_call_address = 0;
    std::uint16_t dispatcher_address = 0;
    std::size_t function_key_index = 0;
    std::uint16_t handler_address = 0;
};

struct MillenniumDosTenthFunctionWordObservation {
    std::uint16_t instruction_address = 0;
    std::uint16_t runtime_address = 0;
    std::uint16_t value = 0;
};
struct MillenniumDosTenthFunctionByteObservation {
    std::uint16_t instruction_address = 0;
    std::uint16_t runtime_address = 0;
    std::uint8_t value = 0;
};
struct MillenniumDosTenthFunctionCallReturnObservation {
    std::uint16_t call_address = 0;
    std::uint16_t return_address = 0;
};
struct MillenniumDosTenthFunctionZeroFlagObservation {
    std::uint16_t branch_address = 0;
    bool set = false;
};
struct MillenniumDosTenthFunctionBlObservation {
    std::uint16_t shift_address = 0;
    std::uint8_t value = 0;
};
struct MillenniumDosTenthFunctionObservationResult {
    bool accepted = false;
    std::string error;
};
struct MillenniumDosTenthFunctionCheckpoint {
    MillenniumDosTenthFunctionState state =
        MillenniumDosTenthFunctionState::awaiting_initialization_guard;
    MillenniumDosTenthFunctionBoundary boundary;
    std::size_t limit_loop_count = 0;
    std::size_t wait_loop_count = 0;
    std::vector<MillenniumDosTenthFunctionByteEffect> runtime_effects;
};

struct MillenniumDosSeventhFunctionDispatchObservation {
    std::uint16_t scaled_call_address = 0;
    std::uint16_t dispatcher_address = 0;
    std::size_t function_key_index = 0;
    std::uint16_t handler_address = 0;
};
struct MillenniumDosSeventhFunctionWordObservation {
    std::uint16_t instruction_address = 0;
    std::uint16_t runtime_address = 0;
    std::uint16_t value = 0;
};
struct MillenniumDosSeventhFunctionByteObservation {
    std::uint16_t instruction_address = 0;
    std::uint16_t runtime_address = 0;
    std::uint8_t value = 0;
};
struct MillenniumDosSeventhFunctionCallReturnObservation {
    std::uint16_t call_address = 0;
    std::uint16_t return_address = 0;
};
struct MillenniumDosSeventhFunctionReturnedBxObservation {
    std::uint16_t store_address = 0;
    std::uint16_t value = 0;
};
struct MillenniumDosSeventhFunctionObservationResult {
    bool accepted = false;
    std::string error;
};
struct MillenniumDosSeventhFunctionCheckpoint {
    MillenniumDosSeventhFunctionBoundary boundary;
    bool returned = false;
    bool returned_by_guard = false;
    std::vector<MillenniumDosSeventhFunctionWordEffect> runtime_effects;
};

struct MillenniumDosSixthFunctionDispatchObservation {
    std::uint16_t scaled_call_address = 0;
    std::uint16_t dispatcher_address = 0;
    std::size_t function_key_index = 0;
    std::uint16_t handler_address = 0;
};
struct MillenniumDosSixthFunctionWordObservation {
    std::uint16_t instruction_address = 0;
    std::uint16_t runtime_address = 0;
    std::uint16_t value = 0;
};
struct MillenniumDosSixthFunctionByteObservation {
    std::uint16_t instruction_address = 0;
    std::uint16_t runtime_address = 0;
    std::uint8_t value = 0;
};
struct MillenniumDosSixthFunctionCallReturnObservation {
    std::uint16_t call_address = 0;
    std::uint16_t return_address = 0;
};
struct MillenniumDosSixthFunctionBlObservation {
    std::uint16_t shift_address = 0;
    std::uint8_t value = 0;
};
struct MillenniumDosSixthFunctionObservationResult {
    bool accepted = false;
    std::string error;
};
struct MillenniumDosSixthFunctionCheckpoint {
    MillenniumDosSixthFunctionState state =
        MillenniumDosSixthFunctionState::awaiting_initialization_guard;
    MillenniumDosSixthFunctionBoundary boundary;
    std::vector<MillenniumDosSixthFunctionEffect> effects;
    std::vector<std::uint8_t> shifted_bl_values;
};
struct MillenniumDosEighthFunctionDispatchObservation {
    std::uint16_t scaled_call_address = 0;
    std::uint16_t dispatcher_address = 0;
    std::size_t function_key_index = 0;
    std::uint16_t handler_address = 0;
};
struct MillenniumDosEighthFunctionCallReturnObservation {
    std::uint16_t call_address = 0;
    std::uint16_t return_address = 0;
};
struct MillenniumDosEighthFunctionBlObservation {
    std::uint16_t shift_address = 0;
    std::uint8_t value = 0;
};
struct MillenniumDosEighthFunctionObservationResult {
    bool accepted = false;
    std::string error;
};
struct MillenniumDosEighthFunctionCheckpoint {
    MillenniumDosEighthFunctionState state =
        MillenniumDosEighthFunctionState::awaiting_preflight_call_return;
    MillenniumDosEighthFunctionBoundary boundary;
    std::vector<MillenniumDosEighthFunctionByteEffect> effects;
    std::vector<std::uint8_t> shifted_bl_values;
};
struct MillenniumDosNinthFunctionDispatchObservation { std::uint16_t scaled_call_address=0; std::uint16_t dispatcher_address=0; std::size_t function_key_index=0; std::uint16_t handler_address=0; };
struct MillenniumDosNinthFunctionWordObservation { std::uint16_t instruction_address=0; std::uint16_t runtime_address=0; std::uint16_t value=0; };
struct MillenniumDosNinthFunctionByteObservation { std::uint16_t instruction_address=0; std::uint16_t runtime_address=0; std::uint8_t value=0; };
struct MillenniumDosNinthFunctionCallReturnObservation { std::uint16_t call_address=0; std::uint16_t return_address=0; };
struct MillenniumDosNinthFunctionObservationResult { bool accepted=false; std::string error; };
struct MillenniumDosNinthFunctionCheckpoint { MillenniumDosNinthFunctionState state=MillenniumDosNinthFunctionState::awaiting_guard; MillenniumDosNinthFunctionBoundary boundary; std::size_t loop_count=0; std::vector<MillenniumDosNinthFunctionByteEffect> effects; };
struct MillenniumDosFourthFunctionDispatchObservation { std::uint16_t scaled_call_address=0; std::uint16_t dispatcher_address=0; std::size_t function_key_index=0; std::uint16_t handler_address=0; };
struct MillenniumDosFourthFunctionWordObservation { std::uint16_t instruction_address=0; std::uint16_t runtime_address=0; std::uint16_t value=0; };
struct MillenniumDosFourthFunctionCallReturnObservation { std::uint16_t call_address=0; std::uint16_t return_address=0; };
struct MillenniumDosFourthFunctionObservationResult { bool accepted=false; std::string error; };
struct MillenniumDosFourthFunctionCheckpoint { MillenniumDosFourthFunctionState state=MillenniumDosFourthFunctionState::awaiting_guard; MillenniumDosFourthFunctionBoundary boundary; std::vector<MillenniumDosFourthFunctionByteEffect> effects; };
struct MillenniumDosFifthFunctionDispatchObservation{std::uint16_t scaled_call_address=0,dispatcher_address=0;std::size_t function_key_index=0;std::uint16_t handler_address=0;}; struct MillenniumDosFifthFunctionCallReturnObservation{std::uint16_t call_address=0,return_address=0;}; struct MillenniumDosFifthFunctionObservationResult{bool accepted=false;std::string error;}; struct MillenniumDosFifthFunctionCheckpoint{MillenniumDosFifthFunctionState state=MillenniumDosFifthFunctionState::first_call;MillenniumDosFifthFunctionBoundary boundary;};
struct MillenniumDosThirdFunctionDispatchObservation {
    std::uint16_t scaled_call_address = 0;
    std::uint16_t dispatcher_address = 0;
    std::size_t function_key_index = 0;
    std::uint16_t handler_address = 0;
};
struct MillenniumDosThirdFunctionWordObservation {
    std::uint16_t instruction_address = 0;
    std::uint16_t runtime_address = 0;
    std::uint16_t value = 0;
};
struct MillenniumDosThirdFunctionCallReturnObservation {
    std::uint16_t call_address = 0;
    std::uint16_t return_address = 0;
};
struct MillenniumDosThirdFunctionBlObservation {
    std::uint16_t instruction_address = 0;
    std::uint8_t value = 0;
};
struct MillenniumDosThirdFunctionObservationResult { bool accepted = false; std::string error; };
struct MillenniumDosThirdFunctionCheckpoint {
    MillenniumDosThirdFunctionState state =
        MillenniumDosThirdFunctionState::awaiting_initialization_guard;
    MillenniumDosThirdFunctionBoundary boundary;
    std::vector<MillenniumDosThirdFunctionEffect> effects;
};
struct MillenniumDosFirstFunctionDispatchObservation {
    std::uint16_t scaled_call_address = 0;
    std::uint16_t dispatcher_address = 0;
    std::size_t function_key_index = 0;
    std::uint16_t handler_address = 0;
};
struct MillenniumDosFirstFunctionCallReturnObservation {
    std::uint16_t call_address = 0;
    std::uint16_t return_address = 0;
};
struct MillenniumDosFirstFunctionBlObservation {
    std::uint16_t instruction_address = 0;
    std::uint8_t value = 0;
};
struct MillenniumDosFirstFunctionObservationResult { bool accepted = false; std::string error; };
struct MillenniumDosFirstFunctionCheckpoint {
    MillenniumDosFirstFunctionState state = MillenniumDosFirstFunctionState::awaiting_display_return;
    MillenniumDosFirstFunctionBoundary boundary;
    std::vector<MillenniumDosFirstFunctionEffect> effects;
};
struct MillenniumDosSecondFunctionDispatchObservation { std::uint16_t scaled_call_address=0; std::uint16_t dispatcher_address=0; std::size_t function_key_index=0; std::uint16_t handler_address=0; };
struct MillenniumDosSecondFunctionRuntimeByteObservation { std::uint16_t instruction_address=0; std::uint16_t runtime_address=0; std::uint8_t value=0; };
struct MillenniumDosSecondFunctionCallReturnObservation { std::uint16_t call_address=0; std::uint16_t return_address=0; };
struct MillenniumDosSecondFunctionBlObservation { std::uint16_t instruction_address=0; std::uint8_t value=0; };
struct MillenniumDosSecondFunctionObservationResult { bool accepted=false; std::string error; };
struct MillenniumDosSecondFunctionCheckpoint { MillenniumDosSecondFunctionState state=MillenniumDosSecondFunctionState::awaiting_availability; MillenniumDosSecondFunctionBoundary boundary; std::vector<MillenniumDosSecondFunctionEffect> effects; };

// Owns the one immutable original-media identity that a runtime is permitted
// to consume. SDL textures, audio devices, and recovered game objects remain
// outside this class; this is the common source boundary for every platform
// adapter. Acquiring a launch re-hashes the outer archive before retaining it.
class ReleaseRuntimeCoordinator {
public:
    [[nodiscard]] bool acquire(const ResolvedLaunchRequest& launch);
    void reset();
    [[nodiscard]] const std::optional<ResolvedLaunchRequest>& active() const { return active_; }
    [[nodiscard]] ReleaseRuntimeAdmission admission() const { return admission_; }
    [[nodiscard]] ReleaseRuntimeRejection rejection() const { return rejection_; }
    // A populated snapshot is proof that one exact adapter was constructed
    // after rehashing. It contains no source path or original bytes and is
    // cleared together with the adapter on every reset/rejection.
    [[nodiscard]] const std::optional<RuntimeSessionSnapshot>& session_snapshot() const {
        return session_snapshot_;
    }
    [[nodiscard]] std::optional<MillenniumDosPresentationSnapshot>
    millennium_dos_presentation() const;
    [[nodiscard]] std::optional<MillenniumDosStartupInputSnapshot>
    millennium_dos_startup_input() const;
    // A value-only diagnostics view. It is available only for the live,
    // exact Millennium DOS title adapter and is revoked with that adapter.
    [[nodiscard]] std::optional<MillenniumDosStaticDispatchDiagnostics>
    millennium_dos_static_dispatch_diagnostics() const;
    // Prepared static recovery entry for the exact active English DOS media.
    // This never advances the live session or accepts boundary observations.
    [[nodiscard]] std::optional<MillenniumDosNativeProcessCheckpoint>
    millennium_dos_native_process_checkpoint() const;
    [[nodiscard]] RuntimeInputDisposition observe_input(const RuntimeInputObservation& observation);
    // Advances exactly one recovered Deuteros Amiga opening tick using the
    // coordinator-owned held observation. All non-opening sessions return no
    // result, so SDL cannot accidentally tick a different platform adapter.
    [[nodiscard]] std::optional<DeuterosAmigaVmEvents> tick_deuteros_amiga_opening();
    // Audio is mixed within the same owner as the recovered VM and is
    // therefore revoked at title handoff/reset. SDL receives only a transient
    // float buffer; it never borrows the opening sound bank or its PCM bytes.
    [[nodiscard]] std::optional<std::vector<float>>
    render_deuteros_amiga_opening_audio(std::size_t frames);
    // Query only: checkpoints never tick or retain a frame outside the active
    // recovered opening session.
    [[nodiscard]] std::optional<DeuterosAmigaOpeningCheckpoint>
    deuteros_amiga_opening_checkpoint() const;
    // These safe copies are the only presentation/title-stage observations
    // SDL should consume. Both are revoked automatically when the session
    // kind changes, admission fails, or the coordinator resets.
    [[nodiscard]] std::optional<DeuterosAmigaOpeningPresentationSnapshot>
    deuteros_amiga_opening_presentation() const;
    [[nodiscard]] std::optional<DeuterosAmigaTitleStageBoundarySnapshot>
    deuteros_amiga_title_stage_boundary() const;
    [[nodiscard]] std::optional<DeuterosAmigaTitleDependencyChainCheckpoint>
    deuteros_amiga_title_dependency_chain_checkpoint() const;
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
    // Active-session transition for a complete, already validated v4/v5
    // trace. The owned result is metadata only and grants no host capability.
    [[nodiscard]] DeuterosAmigaTitleDisplayTraceAdmission
    admit_active_deuteros_amiga_title_display_trace(const ReferenceTrace& trace);
    [[nodiscard]] std::optional<DeuterosAmigaTitleDisplayTraceCheckpoint>
    deuteros_amiga_title_display_trace_checkpoint() const;
    // Query only: this reports static, hash-gated Atari bootstrap facts. It
    // neither selects a runtime state nor invokes an Atari service.
    [[nodiscard]] std::optional<DeuterosAtariBootstrapCheckpoint>
    deuteros_atari_bootstrap_checkpoint() const;
    [[nodiscard]] std::optional<DeuterosAtariBootstrapPresentationSnapshot>
    deuteros_atari_bootstrap_presentation() const;
    [[nodiscard]] std::optional<MillenniumAmigaBootstrapPresentationSnapshot>
    millennium_amiga_bootstrap_presentation() const;
    [[nodiscard]] std::optional<MillenniumAtariBootstrapPresentationSnapshot>
    millennium_atari_bootstrap_presentation() const;
    // This is a transient, trace-gated exception for the proven GX suffix.
    // It does not acquire or publish a game runtime. Its result privately
    // owns the exact transient parser bytes required by its span-based
    // session; no bytes cross its public API. Every other trace remains
    // diagnostics-only.
    [[nodiscard]] MillenniumDosGxStartupTraceAdmission
    admit_millennium_dos_gx_startup_reference_trace(const ReferenceTrace& trace) const;
    // Active-session transition: unlike the diagnostics-only helper above,
    // this requires the exact English DOS adapter to have independently
    // reached its title-handoff boundary before it owns the admitted suffix.
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
    [[nodiscard]] MillenniumDosTenthFunctionObservationResult
    observe_millennium_dos_tenth_function_dispatch(
        MillenniumDosTenthFunctionDispatchObservation observation);
    [[nodiscard]] MillenniumDosTenthFunctionObservationResult
    observe_millennium_dos_tenth_function_word(MillenniumDosTenthFunctionWordObservation observation);
    [[nodiscard]] MillenniumDosTenthFunctionObservationResult
    observe_millennium_dos_tenth_function_byte(MillenniumDosTenthFunctionByteObservation observation);
    [[nodiscard]] MillenniumDosTenthFunctionObservationResult
    observe_millennium_dos_tenth_function_call_return(
        MillenniumDosTenthFunctionCallReturnObservation observation);
    [[nodiscard]] MillenniumDosTenthFunctionObservationResult
    observe_millennium_dos_tenth_function_zero_flag(
        MillenniumDosTenthFunctionZeroFlagObservation observation);
    [[nodiscard]] MillenniumDosTenthFunctionObservationResult
    observe_millennium_dos_tenth_function_bl(MillenniumDosTenthFunctionBlObservation observation);
    [[nodiscard]] std::optional<MillenniumDosTenthFunctionCheckpoint>
    millennium_dos_tenth_function_checkpoint() const;
    [[nodiscard]] MillenniumDosSeventhFunctionObservationResult
    observe_millennium_dos_seventh_function_dispatch(MillenniumDosSeventhFunctionDispatchObservation observation);
    [[nodiscard]] MillenniumDosSeventhFunctionObservationResult
    observe_millennium_dos_seventh_function_word(MillenniumDosSeventhFunctionWordObservation observation);
    [[nodiscard]] MillenniumDosSeventhFunctionObservationResult
    observe_millennium_dos_seventh_function_byte(MillenniumDosSeventhFunctionByteObservation observation);
    [[nodiscard]] MillenniumDosSeventhFunctionObservationResult
    observe_millennium_dos_seventh_function_call_return(MillenniumDosSeventhFunctionCallReturnObservation observation);
    [[nodiscard]] MillenniumDosSeventhFunctionObservationResult
    observe_millennium_dos_seventh_function_returned_bx(MillenniumDosSeventhFunctionReturnedBxObservation observation);
    [[nodiscard]] std::optional<MillenniumDosSeventhFunctionCheckpoint>
    millennium_dos_seventh_function_checkpoint() const;
    [[nodiscard]] MillenniumDosSixthFunctionObservationResult
    observe_millennium_dos_sixth_function_dispatch(MillenniumDosSixthFunctionDispatchObservation observation);
    [[nodiscard]] MillenniumDosSixthFunctionObservationResult
    observe_millennium_dos_sixth_function_word(MillenniumDosSixthFunctionWordObservation observation);
    [[nodiscard]] MillenniumDosSixthFunctionObservationResult
    observe_millennium_dos_sixth_function_byte(MillenniumDosSixthFunctionByteObservation observation);
    [[nodiscard]] MillenniumDosSixthFunctionObservationResult
    observe_millennium_dos_sixth_function_call_return(MillenniumDosSixthFunctionCallReturnObservation observation);
    [[nodiscard]] MillenniumDosSixthFunctionObservationResult
    observe_millennium_dos_sixth_function_bl(MillenniumDosSixthFunctionBlObservation observation);
    [[nodiscard]] std::optional<MillenniumDosSixthFunctionCheckpoint>
    millennium_dos_sixth_function_checkpoint() const;
    [[nodiscard]] MillenniumDosEighthFunctionObservationResult
    observe_millennium_dos_eighth_function_dispatch(MillenniumDosEighthFunctionDispatchObservation observation);
    [[nodiscard]] MillenniumDosEighthFunctionObservationResult
    observe_millennium_dos_eighth_function_call_return(MillenniumDosEighthFunctionCallReturnObservation observation);
    [[nodiscard]] MillenniumDosEighthFunctionObservationResult
    observe_millennium_dos_eighth_function_bl(MillenniumDosEighthFunctionBlObservation observation);
    [[nodiscard]] std::optional<MillenniumDosEighthFunctionCheckpoint>
    millennium_dos_eighth_function_checkpoint() const;
    [[nodiscard]] MillenniumDosNinthFunctionObservationResult observe_millennium_dos_ninth_function_dispatch(MillenniumDosNinthFunctionDispatchObservation);
    [[nodiscard]] MillenniumDosNinthFunctionObservationResult observe_millennium_dos_ninth_function_word(MillenniumDosNinthFunctionWordObservation);
    [[nodiscard]] MillenniumDosNinthFunctionObservationResult observe_millennium_dos_ninth_function_byte(MillenniumDosNinthFunctionByteObservation);
    [[nodiscard]] MillenniumDosNinthFunctionObservationResult observe_millennium_dos_ninth_function_call_return(MillenniumDosNinthFunctionCallReturnObservation);
    [[nodiscard]] std::optional<MillenniumDosNinthFunctionCheckpoint> millennium_dos_ninth_function_checkpoint() const;
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
    [[nodiscard]] std::optional<MillenniumDosOwnedFunctionDiagnostics>
    millennium_dos_owned_function_diagnostics() const;

private:
    std::optional<ResolvedLaunchRequest> active_;
    std::optional<MillenniumDosRuntimeAssets> millennium_dos_;
    std::unique_ptr<MillenniumDosSoundSelectionSession> millennium_dos_sound_selection_;
    std::unique_ptr<MillenniumDosTitleSession> millennium_dos_title_;
    std::optional<MillenniumDosGxStartupTraceAdmission> millennium_dos_gx_startup_;
    std::optional<MillenniumDosNativeProcessAdmission> millennium_dos_native_process_;
    // This span-based session is destroyed before its preceding admission,
    // whose exact verified game buffer is its sole backing owner.
    std::optional<MillenniumDosPostOverlayLoopSession> millennium_dos_post_overlay_loop_;
    std::optional<MillenniumDosSeventhFunctionSession> millennium_dos_seventh_function_;
    std::optional<MillenniumDosSixthFunctionSession> millennium_dos_sixth_function_;
    std::optional<MillenniumDosEighthFunctionSession> millennium_dos_eighth_function_;
    std::optional<MillenniumDosNinthFunctionSession> millennium_dos_ninth_function_;
    std::optional<MillenniumDosFourthFunctionSession> millennium_dos_fourth_function_;
    std::optional<MillenniumDosFifthFunctionSession> millennium_dos_fifth_function_;
    std::optional<MillenniumDosThirdFunctionSession> millennium_dos_third_function_;
    std::optional<MillenniumDosFirstFunctionSession> millennium_dos_first_function_;
    std::optional<MillenniumDosSecondFunctionSession> millennium_dos_second_function_;
    std::optional<MillenniumDosTenthFunctionSession> millennium_dos_tenth_function_;
    std::unique_ptr<MillenniumAmigaBootstrapSession> millennium_amiga_;
    std::unique_ptr<MillenniumAtariBootstrapSession> millennium_atari_;
    std::unique_ptr<DeuterosAmigaOpening> deuteros_amiga_;
    std::optional<DeuterosAmigaTitleServiceSetupLocalPlan> deuteros_amiga_title_service_setup_plan_;
    std::optional<DeuterosAmigaTitleSecondServiceLocalPlan> deuteros_amiga_title_second_service_plan_;
    std::optional<DeuterosAmigaTitleThirdServiceLocalPlan> deuteros_amiga_title_third_service_plan_;
    std::unique_ptr<DeuterosAmigaPaulaMixer> deuteros_amiga_paula_;
    std::optional<DeuterosAmigaTitleDisplayTraceSession>
        deuteros_amiga_title_display_trace_;
    bool deuteros_amiga_opening_input_held_ = false;
    std::unique_ptr<DeuterosAtariBootstrapSession> deuteros_atari_;
    std::optional<RuntimeSessionSnapshot> session_snapshot_;
    ReleaseRuntimeAdmission admission_ = ReleaseRuntimeAdmission::unselected;
    ReleaseRuntimeRejection rejection_ = ReleaseRuntimeRejection::none;
};

// The one common final launch gate for CLI and card-menu candidates. It
// resolves a menu/CLI DTO through scanner-produced identities and only then
// acquires the platform adapter. An absent or stale candidate clears any
// prior runtime just like a rejected archive/parser boundary.
struct RuntimeLaunchAdmission {
    ReleaseRuntimeAdmission admission = ReleaseRuntimeAdmission::unselected;
    // This result describes the attempted candidate even when the coordinator
    // was reset before acquisition. It lets CLI and card UI report the same
    // safe cause without retaining a stale runtime or any media detail.
    ReleaseRuntimeRejection rejection = ReleaseRuntimeRejection::none;
    [[nodiscard]] bool accepted() const {
        return admission == ReleaseRuntimeAdmission::active
            && rejection == ReleaseRuntimeRejection::none;
    }
};

[[nodiscard]] RuntimeLaunchAdmission admit_runtime_launch(
    ReleaseRuntimeCoordinator& coordinator, const std::optional<LaunchRequest>& candidate,
    const std::vector<ReleaseArchive>& releases);

[[nodiscard]] std::unique_ptr<DeuterosAmigaOpening> load_deuteros_amiga_runtime(
    const ReleaseArchive& release);
[[nodiscard]] std::unique_ptr<DeuterosAmigaOpening> load_deuteros_amiga_runtime(
    const VerifiedReleaseMedia& media);
[[nodiscard]] std::unique_ptr<DeuterosAtariBootstrapSession> load_deuteros_atari_runtime(
    const ReleaseArchive& release);
[[nodiscard]] std::unique_ptr<DeuterosAtariBootstrapSession> load_deuteros_atari_runtime(
    const VerifiedReleaseMedia& media);
[[nodiscard]] std::unique_ptr<MillenniumAmigaBootstrapSession> load_millennium_amiga_runtime(
    const ReleaseArchive& release);
[[nodiscard]] std::unique_ptr<MillenniumAmigaBootstrapSession> load_millennium_amiga_runtime(
    const VerifiedReleaseMedia& media);
[[nodiscard]] std::unique_ptr<MillenniumAtariBootstrapSession> load_millennium_atari_runtime(
    const ReleaseArchive& release);
[[nodiscard]] std::unique_ptr<MillenniumAtariBootstrapSession> load_millennium_atari_runtime(
    const VerifiedReleaseMedia& media);
[[nodiscard]] std::optional<MillenniumDosRuntimeAssets> load_millennium_dos_runtime(
    const ReleaseArchive& release);
[[nodiscard]] std::optional<MillenniumDosRuntimeAssets> load_millennium_dos_runtime(
    const VerifiedReleaseMedia& media);

} // namespace eon
