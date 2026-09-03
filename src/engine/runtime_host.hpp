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
class RuntimeHost : public NativeSessionController {
public:
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
