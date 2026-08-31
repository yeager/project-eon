#pragma once

#include "engine/release_runtime.hpp"

namespace eon {

// The CLI and card menu share this final candidate boundary.  It accepts only
// a normalized request DTO and scanner-produced identities, then publishes a
// successful result solely from the coordinator's rehashed active snapshot.
// It has no SDL, renderer, save, Modern-pack, or game-media decoding surface.
struct RuntimeCandidateLaunchResult {
    ReleaseRuntimeAdmission admission = ReleaseRuntimeAdmission::unselected;
    std::optional<ResolvedLaunchRequest> active_launch;

    [[nodiscard]] bool accepted() const {
        return admission == ReleaseRuntimeAdmission::active && active_launch.has_value();
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
    [[nodiscard]] ReleaseRuntimeCoordinator& coordinator() { return coordinator_; }
    [[nodiscard]] const std::optional<ResolvedLaunchRequest>& active() const { return coordinator_.active(); }
    [[nodiscard]] ReleaseRuntimeAdmission admission() const { return coordinator_.admission(); }

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
