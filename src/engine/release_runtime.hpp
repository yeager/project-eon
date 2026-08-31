#pragma once

#include "launcher.hpp"
#include "engine/deuteros_amiga_opening.hpp"
#include "engine/deuteros_atari_bootstrap_session.hpp"
#include "engine/millennium_amiga_bootstrap_session.hpp"
#include "engine/millennium_atari_bootstrap_session.hpp"
#include "engine/millennium_dos_save_session.hpp"
#include "data/millennium_dos_game_flow.hpp"
#include "data/millennium_dos_sound_driver.hpp"
#include "data/millennium_dos_title_flow.hpp"
#include "data/millennium_dos_video_driver.hpp"

#include <cstdint>
#include <memory>
#include <optional>
#include <string>
#include <vector>

namespace eon {

enum class ReleaseRuntimeAdmission { unselected, active, identity_rejected, archive_rejected };

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

private:
    std::optional<ResolvedLaunchRequest> active_;
    ReleaseRuntimeAdmission admission_ = ReleaseRuntimeAdmission::unselected;
};

[[nodiscard]] std::unique_ptr<DeuterosAmigaOpening> load_deuteros_amiga_runtime(
    const ReleaseArchive& release);
[[nodiscard]] std::unique_ptr<DeuterosAtariBootstrapSession> load_deuteros_atari_runtime(
    const ReleaseArchive& release);
[[nodiscard]] std::unique_ptr<MillenniumAmigaBootstrapSession> load_millennium_amiga_runtime(
    const ReleaseArchive& release);
[[nodiscard]] std::unique_ptr<MillenniumAtariBootstrapSession> load_millennium_atari_runtime(
    const ReleaseArchive& release);
[[nodiscard]] std::optional<MillenniumDosRuntimeAssets> load_millennium_dos_runtime(
    const ReleaseArchive& release);

} // namespace eon
