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
    [[nodiscard]] std::optional<std::vector<float>>
    render_deuteros_amiga_opening_audio(std::size_t frames);
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
