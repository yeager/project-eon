#pragma once

#include "engine/menu_runtime_launch.hpp"
#include "engine/deuteros_amiga_opening_runner.hpp"
#include "engine/runtime_presentation.hpp"

#include <optional>
#include <string_view>
#include <vector>

namespace eon {

// The native engine lifecycle is deliberately separate from SDL's menu,
// renderer and modal state. Each non-menu value is a recovery-backed session
// or an explicit preservation boundary; none means a generic playable state.
enum class NativeSessionState {
    menu,
    admission_rejected,
    millennium_dos_title,
    millennium_dos_sound_driver_boundary,
    millennium_dos_title_handoff_boundary,
    millennium_amiga_bootstrap,
    millennium_atari_bootstrap,
    deuteros_amiga_opening,
    deuteros_amiga_title_stage_boundary,
    deuteros_atari_bootstrap,
    returning_to_menu,
};

[[nodiscard]] std::string_view native_session_state_label(NativeSessionState state);
[[nodiscard]] NativeSessionState native_session_state_for(
    const std::optional<RuntimeSessionSnapshot>& snapshot,
    ReleaseRuntimeAdmission admission);

// Owns the native release coordinator and publishes its lifecycle as one
// finite state machine. It neither creates SDL resources nor adds a common
// input model: inputs and ticks are passed to the existing evidence-limited
// coordinator, then the resulting recovered state is synchronized.
class NativeSessionController {
public:
    [[nodiscard]] RuntimeCandidateLaunchResult launch_direct(const LaunchRequest& candidate,
        const std::vector<ReleaseArchive>& releases);
    [[nodiscard]] RuntimeCandidateLaunchResult launch_menu(const LauncherSessionState& session,
        const LaunchRequest& base, const std::vector<ReleaseArchive>& releases);
    [[nodiscard]] RuntimeInputDisposition observe_input(const RuntimeInputObservation& observation);
    [[nodiscard]] std::optional<MillenniumDosPresentationSnapshot>
    millennium_dos_presentation() const;
    [[nodiscard]] std::optional<MillenniumDosStartupInputSnapshot>
    millennium_dos_startup_input() const;
    [[nodiscard]] std::optional<DeuterosAmigaVmEvents> tick_deuteros_amiga_opening();
    // The 50 Hz opening scheduler is native-session lifecycle state. SDL
    // supplies a monotonic timestamp, receives only immutable VM events, and
    // cannot retain a runner that outlives a source switch or teardown.
    [[nodiscard]] bool start_deuteros_amiga_opening_scheduler(std::uint64_t initial_tick);
    [[nodiscard]] DeuterosAmigaOpeningAdvance
    advance_deuteros_amiga_opening_scheduler(std::uint64_t now);
    [[nodiscard]] bool deuteros_amiga_opening_scheduler_active() const;
    [[nodiscard]] std::optional<std::vector<float>>
    render_deuteros_amiga_opening_audio(std::size_t frames);
    [[nodiscard]] std::optional<DeuterosAmigaOpeningCheckpoint>
    deuteros_amiga_opening_checkpoint() const;
    [[nodiscard]] std::optional<DeuterosAmigaOpeningPresentationSnapshot>
    deuteros_amiga_opening_presentation() const;
    [[nodiscard]] std::optional<DeuterosAmigaTitleStageBoundarySnapshot>
    deuteros_amiga_title_stage_boundary() const;
    [[nodiscard]] std::optional<DeuterosAtariBootstrapCheckpoint>
    deuteros_atari_bootstrap_checkpoint() const;
    [[nodiscard]] std::optional<DeuterosAtariBootstrapPresentationSnapshot>
    deuteros_atari_bootstrap_presentation() const;
    [[nodiscard]] std::optional<MillenniumAmigaBootstrapPresentationSnapshot>
    millennium_amiga_bootstrap_presentation() const;
    [[nodiscard]] std::optional<MillenniumAtariBootstrapPresentationSnapshot>
    millennium_atari_bootstrap_presentation() const;

    // SDL must revoke borrowed textures, audio and text input before calling
    // this method. The explicit intermediate state makes that ordering
    // visible to diagnostics and prevents a stale active snapshot.
    void begin_return_to_menu();
    void finish_return_to_menu();
    void reset();
    void synchronize();

    [[nodiscard]] NativeSessionState state() const { return state_; }
    [[nodiscard]] bool is_menu() const { return state_ == NativeSessionState::menu; }
    [[nodiscard]] bool is_live() const;
    [[nodiscard]] bool requires_revocation_for(const LauncherSourceIdentity& source) const;
    [[nodiscard]] const std::optional<ResolvedLaunchRequest>& active() const { return runtime_.active(); }
    [[nodiscard]] ReleaseRuntimeAdmission admission() const { return runtime_.admission(); }
    [[nodiscard]] ReleaseRuntimeRejection rejection() const { return runtime_.rejection(); }
    // SDL diagnostics receive a copy of the value-only session declaration,
    // never the mutable coordinator that owns platform adapters and media.
    [[nodiscard]] std::optional<RuntimeSessionSnapshot> session_snapshot() const;
    // A value-only SDL boundary. This cannot expose original media borrows or
    // allow the UI to manufacture a runtime state.
    [[nodiscard]] std::optional<RuntimePresentationSnapshot> presentation_snapshot() const;

private:
    void synchronize_after_runtime_change();

    LauncherRuntimeController runtime_;
    std::optional<DeuterosAmigaOpeningRunner> deuteros_amiga_opening_runner_;
    NativeSessionState state_ = NativeSessionState::menu;
};

} // namespace eon
