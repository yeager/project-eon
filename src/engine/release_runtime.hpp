#pragma once

#include "launcher.hpp"
#include "engine/deuteros_amiga_opening.hpp"
#include "engine/deuteros_amiga_paula.hpp"
#include "engine/runtime_session.hpp"
#include "engine/deuteros_atari_bootstrap_session.hpp"
#include "engine/millennium_amiga_bootstrap_session.hpp"
#include "engine/millennium_atari_bootstrap_session.hpp"
#include "engine/millennium_dos_save_session.hpp"
#include "engine/millennium_dos_sound_selection_session.hpp"
#include "engine/millennium_dos_gx_startup_trace_admission.hpp"
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
    // It does not acquire or publish a game runtime and retains neither trace
    // nor media bytes. Every other trace remains diagnostics-only.
    [[nodiscard]] MillenniumDosGxStartupTraceAdmission
    admit_millennium_dos_gx_startup_reference_trace(const ReferenceTrace& trace) const;

private:
    std::optional<ResolvedLaunchRequest> active_;
    std::optional<MillenniumDosRuntimeAssets> millennium_dos_;
    std::unique_ptr<MillenniumDosSoundSelectionSession> millennium_dos_sound_selection_;
    std::unique_ptr<MillenniumDosTitleSession> millennium_dos_title_;
    std::unique_ptr<MillenniumAmigaBootstrapSession> millennium_amiga_;
    std::unique_ptr<MillenniumAtariBootstrapSession> millennium_atari_;
    std::unique_ptr<DeuterosAmigaOpening> deuteros_amiga_;
    std::unique_ptr<DeuterosAmigaPaulaMixer> deuteros_amiga_paula_;
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
