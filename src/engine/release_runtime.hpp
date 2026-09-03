#pragma once

#include "launcher.hpp"
#include "engine/deuteros_amiga_opening.hpp"
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

// The recovered DOS title/runtime evidence for one exact archive identity.
// Spanish is title-only until its executable handoff is evidenced; English
// retains the additional parser outputs below. No field is a fallback for a
// different release, platform, or language.
struct MillenniumDosRuntimeAssets {
    MillenniumDosPreviewAnimation title;
    std::string language;
    std::optional<MillenniumDosPreviewAnimation> gx_canvas;
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
    [[nodiscard]] const MillenniumDosRuntimeAssets* millennium_dos() const {
        return millennium_dos_ ? &*millennium_dos_ : nullptr;
    }
    [[nodiscard]] MillenniumDosSoundSelectionSession* millennium_dos_sound_selection() const {
        return millennium_dos_sound_selection_.get();
    }
    [[nodiscard]] MillenniumDosTitleSession* millennium_dos_title() const {
        return millennium_dos_title_.get();
    }
    [[nodiscard]] RuntimeInputDisposition observe_input(const RuntimeInputObservation& observation);
    // Advances exactly one recovered Deuteros Amiga opening tick using the
    // coordinator-owned held observation. All non-opening sessions return no
    // result, so SDL cannot accidentally tick a different platform adapter.
    [[nodiscard]] std::optional<DeuterosAmigaVmEvents> tick_deuteros_amiga_opening();
    // Query only: checkpoints never tick or retain a frame outside the active
    // recovered opening session.
    [[nodiscard]] std::optional<DeuterosAmigaOpeningCheckpoint>
    deuteros_amiga_opening_checkpoint() const;
    // Query only: this reports static, hash-gated Atari bootstrap facts. It
    // neither selects a runtime state nor invokes an Atari service.
    [[nodiscard]] std::optional<DeuterosAtariBootstrapCheckpoint>
    deuteros_atari_bootstrap_checkpoint() const;
    [[nodiscard]] MillenniumAmigaBootstrapSession* millennium_amiga() const {
        return millennium_amiga_.get();
    }
    [[nodiscard]] MillenniumAtariBootstrapSession* millennium_atari() const {
        return millennium_atari_.get();
    }
    [[nodiscard]] DeuterosAmigaOpening* deuteros_amiga() const {
        return deuteros_amiga_.get();
    }
    [[nodiscard]] DeuterosAtariBootstrapSession* deuteros_atari() const {
        return deuteros_atari_.get();
    }
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
