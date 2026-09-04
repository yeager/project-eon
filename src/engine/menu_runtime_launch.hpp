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
    [[nodiscard]] std::optional<DeuterosAmigaVmEvents> tick_deuteros_amiga_opening();
    [[nodiscard]] std::optional<std::vector<float>>
    render_deuteros_amiga_opening_audio(std::size_t frames);
    [[nodiscard]] std::optional<DeuterosAmigaOpeningCheckpoint>
    deuteros_amiga_opening_checkpoint() const;
    [[nodiscard]] std::optional<DeuterosAmigaOpeningPresentationSnapshot>
    deuteros_amiga_opening_presentation() const;
    [[nodiscard]] std::optional<DeuterosAmigaTitleStageBoundarySnapshot>
    deuteros_amiga_title_stage_boundary() const;
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
